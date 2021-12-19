/*
    PADnoteParameters.h - Parameters for PADnote (PADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2018, Will Godfrey
    Copyright 2020 Kristian Amlie & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of a ZynAddSubFX original.

*/

#ifndef PAD_NOTE_PARAMETERS_H
#define PAD_NOTE_PARAMETERS_H

#include "Params/Presets.h"

#include <memory>
#include <utility>
#include <cassert>

class XMLwrapper;
class FFTwrapper;
class OscilGen;
class OscilParameters;
class Resonance;
class EnvelopeParams;
class LFOParams;
class FilterParams;

class SynthEngine;

// defines quality / resolution of PADSynth wavetables
struct PADQuality {
    unsigned char samplesize;
    unsigned char basenote, oct, smpoct;

    PADQuality() { resetToDefaults(); }

    void resetToDefaults()
    {
        samplesize = 3;
        basenote = 4;
        oct = 3;
        smpoct = 2;
    }
};


class PADTables
{
public:
    // size parameters
    static constexpr size_t INTERPOLATION_BUFFER = 5;
    const size_t numTables;
    const size_t tableSize;

    std::unique_ptr<float[]> samples;
    std::unique_ptr<float[]> basefreq;

public: // can be moved and swapped, but not copied...
   ~PADTables()                            = default;
    PADTables(PADTables&&)                 = default;
    PADTables(PADTables const&)            = delete;
    PADTables& operator=(PADTables&&)      = delete;
    PADTables& operator=(PADTables const&) = delete;

    PADTables(PADQuality const& quality)
        : numTables(calcNumTables(quality))
        , tableSize(calcTableSize(quality))
        , samples(new float[numTables * (tableSize + INTERPOLATION_BUFFER)]{0})
        , basefreq(new float[numTables])                                 // zero-init samples
    {
        assert(numTables > 0);
        assert(tableSize > 0);
        for (size_t tab=0; tab<numTables; ++tab)
            basefreq[tab] = 440.0f; // fallback base frequency; makes even empty wavetable usable
    }

    // Subscript: access n-th wavetable
    float* operator[](size_t tableNo)
    {
        assert(samples);
        assert(tableNo < numTables);
        return &samples[0] + tableNo * (tableSize + INTERPOLATION_BUFFER);
    }
private:
    static size_t calcNumTables(PADQuality const&);
    static size_t calcTableSize(PADQuality const&);
};

namespace std {
    // deliberately allow to swap two PADTables,
    // even while not being move assignable due to the const fields
    inline void swap(PADTables& p1, PADTables& p2)
    {
        swap(p1.samples, p2.samples);
        swap(p1.basefreq,p2.basefreq);
        swap(const_cast<size_t&>(p1.numTables), const_cast<size_t&>(p2.numTables));
        swap(const_cast<size_t&>(p1.tableSize), const_cast<size_t&>(p2.tableSize));
    }
}


class PADnoteParameters : public Presets
{
    public:
        PADnoteParameters(FFTwrapper *fft_, SynthEngine *_synth);
        ~PADnoteParameters();

        void defaults(void);
        void setPan(char pan, unsigned char panLaw);

        void add2XML(XMLwrapper *xml);
        void getfromXML(XMLwrapper *xml);
        float getLimits(CommandBlock *getData);

        //returns a value between 0.0-1.0 that represents the estimation
        // perceived bandwidth
        float getprofile(float *smp, int size);

        //parameters

        //the mode: 0 - bandwidth, 1 - discrete (bandwidth=0), 2 - continuous
        //the harmonic profile is used only on mode 0
        unsigned char Pmode;

        //Harmonic profile (the frequency distribution of a single harmonic)
        struct HarmonicProfile {
            struct BaseFunction {
                unsigned char type;
                unsigned char par1;
            };
            struct Modulator{
                unsigned char par1;
                unsigned char freq;
            };
            struct AmplitudeMultiplier {
                unsigned char mode;
                unsigned char type;
                unsigned char par1;
                unsigned char par2;
            };

            BaseFunction base;
            unsigned char freqmult;  // frequency multiplier of the distribution
            Modulator modulator;     // the modulator of the distribution
            unsigned char width;     // the width of the resulting function after the modulation
            AmplitudeMultiplier amp; // the amplitude multiplier of the harmonic profile

            bool autoscale;        //  if the scale of the harmonic profile is
                                   // computed automatically
            unsigned char onehalf; // what part of the base function is used to
                                   // make the distribution
        };

        struct HarmonicPos { // where harmonics are positioned (on integer multiples or shifted away)
            unsigned char type;
            unsigned char par1, par2, par3; // 0..255
        };


        HarmonicProfile Php;

        unsigned int Pbandwidth; // the values are from 0 to 1000
        unsigned char Pbwscale;  // how the bandwidth is increased according to
                                 // the harmonic's frequency
        HarmonicPos Phrpos;
        PADQuality Pquality;

        // Frequency parameters
        unsigned char Pfixedfreq; // If the base frequency is fixed to 440 Hz

        // Equal temperate (this is used only if the Pfixedfreq is enabled)
        // If this parameter is 0, the frequency is fixed (to 440 Hz);
        // if this parameter is 64, 1 MIDI halftone -> 1 frequency halftone
        unsigned char      PfixedfreqET;

        unsigned char PBendAdjust; // Pitch Bend
        unsigned char POffsetHz;

        unsigned short int PDetune;       // fine detune
        unsigned short int PCoarseDetune; // coarse detune+octave
        unsigned char      PDetuneType;   // detune type

        EnvelopeParams *FreqEnvelope; // Frequency Envelope
        LFOParams *FreqLfo;           // Frequency LFO

        // Amplitude parameters
        unsigned char PStereo;
        unsigned char PPanning;  // 1 left, 64 center, 127 right
        bool  PRandom;
        char  PWidth;
        float         pangainL;  // derived from PPanning
        float         pangainR;  // ^^
        unsigned char PVolume;
        unsigned char PAmpVelocityScaleFunction;

        EnvelopeParams *AmpEnvelope;
        LFOParams *AmpLfo;

        // Adjustment factor for anti-pop fadein
        unsigned char Fadein_adjustment;

        unsigned char PPunchStrength, PPunchTime, PPunchStretch, PPunchVelocitySensing;

        // Filter Parameters
        FilterParams *GlobalFilter;
        unsigned char PFilterVelocityScale; // filter velocity sensing
        unsigned char PFilterVelocityScaleFunction; // filter velocity sensing

        EnvelopeParams *FilterEnvelope;
        LFOParams *FilterLfo;

        float setPbandwidth(int Pbandwidth); // returns the BandWidth in cents
        float getNhr(int n); // gets the n-th overtone position relatively to N harmonic

        bool Papplied;
        bool Pbuilding;
        bool Pready;
        void setpadparams(bool force);
        void padparamsthread(bool force);
        void applyparameters(bool force);
        void activate_wavetable(void);
        bool export2wav(std::string basefilename);

        OscilParameters *POscil;
        OscilGen *oscilgen;
        Resonance *resonance;

        PADTables waveTable;

///////////////////////////////////////////TODO: obsolete, will be replaced by future
        std::unique_ptr<PADTables> newWaveTable;
///////////////////////////////////////////TODO: (End)obsolete, will be replaced by future


    private:
        void generatespectrum_bandwidthMode(float *spectrum, int size,
                                            float basefreq,
                                            float *profile,
                                            int profilesize,
                                            float bwadjust);
        void generatespectrum_otherModes(float *spectrum, int size,
                                         float basefreq);

        FFTwrapper *fft;
};

#endif
