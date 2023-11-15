/*
    PADnoteParameters.h - Parameters for PADnote (PADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2018, Will Godfrey
    Copyright 2020 Kristian Amlie & others
    Copyright 2022 Ichthyostega

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

#include "Params/ParamCheck.h"
#include "Misc/RandomGen.h"
#include "Misc/BuildScheduler.h"
#include "Params/RandomWalk.h"
#include "Synth/XFadeManager.h"
#include "Synth/OscilGen.h"
#include "DSP/FFTwrapper.h"

#include <memory>
#include <utility>
#include <cassert>
#include <vector>
#include <string>

using std::unique_ptr;
using std::vector;

class XMLwrapper;
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
    const size_t numTables;
    const size_t tableSize;

    unique_ptr<float[]> basefreq;

private:
    vector<fft::Waveform> samples;

public: // can be moved and swapped, but not copied...
   ~PADTables()                            = default;
    PADTables(PADTables&&)                 = default;
    PADTables(PADTables const&)            = delete;
    PADTables& operator=(PADTables&&)      = delete;
    PADTables& operator=(PADTables const&) = delete;

    PADTables(PADQuality const& quality)
        : numTables{calcNumTables(quality)}
        , tableSize{calcTableSize(quality)}
        , basefreq{new float[numTables]}
        , samples{}
    {
        assert(numTables > 0);
        assert(tableSize > 0);
        samples.reserve(numTables);
        for (size_t tab=0; tab < numTables; ++tab)
        {
            samples.emplace_back(tableSize); // cause allocation and zero-init of wavetable(s)
            basefreq[tab] = 440.0f; // fallback base frequency; makes even empty wavetable usable
        }
    }

    void reset() // fill existing wavetables with silence
    {
        for (size_t tab=0; tab < numTables; ++tab)
            samples[tab].reset();
    }

    // Subscript: access n-th wavetable
    fft::Waveform& operator[](size_t tableNo)
    {
        assert(tableNo < numTables);
        assert(samples.size() == numTables);
        return samples[tableNo];
    }

    fft::Waveform const& operator[](size_t tableNo)  const
    {   return const_cast<PADTables*>(this)->operator[](tableNo); }


    void cloneDataFrom(PADTables const& org)
    {
        const_cast<size_t&>(numTables) = org.numTables;
        const_cast<size_t&>(tableSize) = org.tableSize;
        samples.clear(); // discard existing allocations (size may differ)
        basefreq.reset(new float[numTables]);
        for (size_t tab=0; tab < numTables; ++tab)
        {
            samples.emplace_back(tableSize);
            samples[tab]  = org[tab];   // clone sample data
            basefreq[tab] = org.basefreq[tab];
        }
    }

    // deliberately allow to swap two PADTables,
    // even while not being move assignable due to the const fields
    friend void swap(PADTables& p1, PADTables& p2)
    {
        using std::swap;
        swap(p1.samples, p2.samples);
        swap(p1.basefreq,p2.basefreq);
        swap(const_cast<size_t&>(p1.numTables), const_cast<size_t&>(p2.numTables));
        swap(const_cast<size_t&>(p1.tableSize), const_cast<size_t&>(p2.tableSize));
    }

private:
    static size_t calcNumTables(PADQuality const&);
    static size_t calcTableSize(PADQuality const&);
};




class PADnoteParameters : public ParamBase
{
        static constexpr size_t SIZE_HARMONIC_PROFILE = 512;
        static constexpr size_t PROFILE_OVERSAMPLING = 16;
    public:
        static constexpr size_t XFADE_UPDATE_MAX   = 20000; // milliseconds
        static constexpr size_t XFADE_UPDATE_DEFAULT = 200;
        static constexpr size_t REBUILDTRIGGER_MAX = 60000; // milliseconds

    public:
        PADnoteParameters(uchar pID, uchar kID, SynthEngine *_synth);
       ~PADnoteParameters()  = default;

        // shall not be copied or moved or assigned
        PADnoteParameters(PADnoteParameters&&)                 = delete;
        PADnoteParameters(PADnoteParameters const&)            = delete;
        PADnoteParameters& operator=(PADnoteParameters&&)      = delete;
        PADnoteParameters& operator=(PADnoteParameters const&) = delete;


        void defaults(void);
        void reseed(int value);
        void setPan(char pan, unsigned char panLaw);

        void add2XML(XMLwrapper *xml);
        void getfromXML(XMLwrapper *xml);
        float getLimits(CommandBlock *getData);
        float getBandwithInCent(); // convert Pbandwith setting into cents

        // (re)Building the Wavetable
        void buildNewWavetable(bool blocking =false);
        Optional<PADTables> render_wavetable();
        void activate_wavetable();
        bool export2wav(std::string basefilename);

        vector<float> buildProfile(size_t size);
        float calcProfileBandwith(vector<float> const& profile);
        float calcHarmonicPositionFactor(float n); // position of partials, possibly non-harmonic.


        // Harmonic profile settings
        // (controls the frequency distribution of a single harmonic)
        struct HarmonicProfile {
            struct BaseFunction {
                unsigned char type;
                unsigned char pwidth;
            };
            struct Modulator{
                unsigned char pstretch;
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
            void defaults();
        };

        // Positioning of partials
        // on integer multiples (type=0 -> regular harmonics)
        // or shifted away for distorted spectrum
        // see calcHarmonicPositionFactor(partial)
        struct HarmonicPos {
            unsigned char type = 0;  // harmonic,ushift,lshift,upower,lpower,sine,power,shift
            unsigned char par1 = 64; // strength of the shift
            unsigned char par2 = 64; // depending on type, defines threshold, exponent or frequency
            unsigned char par3 = 0;  // forceH : increasingly shift towards next harmonic position
                                     // these params are 0..255
            void defaults();
        };


        //----PADSynth parameters--------------

        //the mode: 0 - bandwidth, 1 - discrete (bandwidth=0), 2 - continuous
        //the harmonic profile is used only on mode 0
        unsigned char Pmode;

        PADQuality Pquality;     // Quality settings; controls number and size of wavetables

        HarmonicProfile PProfile;

        unsigned int Pbandwidth; // the values are from 0 to 1000
        unsigned char Pbwscale;  // how the bandwidth is increased according to
                                 // the harmonic's frequency
        HarmonicPos Phrpos;      // Positioning of partials (harmonic / distorted)

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

        fft::Calc fft; // private instance used by OscilGen

        unique_ptr<OscilParameters> POscil;
        unique_ptr<Resonance> resonance;
        unique_ptr<OscilGen> oscilgen;

        unique_ptr<EnvelopeParams> FreqEnvelope; // Frequency Envelope
        unique_ptr<LFOParams> FreqLfo;           // Frequency LFO

        // Amplitude parameters
        unsigned char PStereo;
        unsigned char PPanning;  // 1 left, 64 center, 127 right
        bool  PRandom;
        char  PWidth;
        float         pangainL;  // derived from PPanning
        float         pangainR;  // ^^
        unsigned char PVolume;
        unsigned char PAmpVelocityScaleFunction;

        unique_ptr<EnvelopeParams> AmpEnvelope;
        unique_ptr<LFOParams> AmpLfo;

        // Adjustment factor for anti-pop fadein
        unsigned char Fadein_adjustment;

        unsigned char PPunchStrength, PPunchTime, PPunchStretch, PPunchVelocitySensing;

        // Filter Parameters
        unique_ptr<FilterParams> GlobalFilter;
        unsigned char PFilterVelocityScale; // filter velocity sensing
        unsigned char PFilterVelocityScaleFunction; // filter velocity sensing

        unique_ptr<EnvelopeParams> FilterEnvelope;
        unique_ptr<LFOParams> FilterLfo;

        // re-Trigger Wavetable build with random walk
        uint PrebuildTrigger;
        uchar PrandWalkDetune;
        uchar PrandWalkBandwidth;
        uchar PrandWalkFilterFreq;
        uchar PrandWalkProfileWidth;
        uchar PrandWalkProfileStretch;

        RandomWalk randWalkDetune;
        RandomWalk randWalkBandwidth;
        RandomWalk randWalkFilterFreq;
        RandomWalk randWalkProfileWidth;
        RandomWalk randWalkProfileStretch;

        // manage secondary PADTables during a wavetable X-Fade
        XFadeManager<PADTables> xFade;
        uint PxFadeUpdate;    // in milliseconds, XFADE_UPDATE_MAX = 20000

        // current wavetable
        PADTables waveTable;

        // control for rebuilding wavetable (background action)
        FutureBuild<PADTables> futureBuild;

        const uchar partID;
        const uchar kitID;

    private:
        size_t sampleTime;
        RandomGen wavetablePhasePrng;

        vector<float> generateSpectrum_bandwidthMode(float basefreq, size_t spectrumSize, vector<float> const& profile);
        vector<float> generateSpectrum_otherModes(float basefreq, size_t spectrumSize);

        void maybeRetrigger();
        void mute_and_rebuild_synchronous();

        // type abbreviations
        using FutureVal = std::future<PADTables>;
        using ResultVal = Optional<PADTables>;
        using BuildOperation = std::function<ResultVal()>;
        using ScheduleAction = std::function<FutureVal()>;
        using SchedulerSetup = std::function<ScheduleAction(BuildOperation)>;
};

#endif /*PAD_NOTE_PARAMETERS_H*/
