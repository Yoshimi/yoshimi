/*
    PADnoteParameters.h - Parameters for PADnote (PADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2018, Will Godfrey
    Copyright 2020 Kristian Amlie & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
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

class XMLwrapper;
class FFTwrapper;
class OscilGen;
class OscilParameters;
class Resonance;
class EnvelopeParams;
class LFOParams;
class FilterParams;

class SynthEngine;

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
        struct {
            struct {    //base function
                unsigned char type;
                unsigned char par1;
            } base;
            unsigned char freqmult; // frequency multiplier of the distribution
            struct {                // the modulator of the distribution
                unsigned char par1;
                unsigned char freq;
            } modulator;

            unsigned char width; // the width of the resulting function after the modulation

            struct { // the amplitude multiplier of the harmonic profile
                unsigned char mode;
                unsigned char type;
                unsigned char par1;
                unsigned char par2;
            } amp;
            bool autoscale;        //  if the scale of the harmonic profile is
                                   // computed automatically
            unsigned char onehalf; // what part of the base function is used to
                                   // make the distribution
        } Php;


        unsigned int Pbandwidth; // the values are from 0 to 1000
        unsigned char Pbwscale;  // how the bandwidth is increased according to
                                 // the harmonic's frequency

        struct { // where are positioned the harmonics (on integer multiplier or different places)
            unsigned char type;
            unsigned char par1, par2, par3; // 0..255
        } Phrpos;

        struct { // quality of the samples (how many samples, the length of them,etc.)
            unsigned char samplesize;
            unsigned char basenote, oct, smpoct;
        } Pquality;

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
        void applyparameters(void);
        bool export2wav(std::string basefilename);

        OscilParameters *POscil;
        OscilGen *oscilgen;
        Resonance *resonance;

        struct {
            int size;
            float basefreq;
            float *smp;
        } sample[PAD_MAX_SAMPLES], newsample;

    private:
        void generatespectrum_bandwidthMode(float *spectrum, int size,
                                            float basefreq,
                                            float *profile,
                                            int profilesize,
                                            float bwadjust);
        void generatespectrum_otherModes(float *spectrum, int size,
                                         float basefreq);
        void deletesamples(void);
        void deletesample(int n);

        FFTwrapper *fft;
};

#endif
