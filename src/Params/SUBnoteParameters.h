/*
    SUBnoteParameters.h - Parameters for SUBnote (SUBsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert
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

    This file is derivative of ZynAddSubFX original code.

*/

#ifndef SUB_NOTE_PARAMETERS_H
#define SUB_NOTE_PARAMETERS_H

#include "Misc/XMLwrapper.h"
#include "Params/EnvelopeParams.h"
#include "Params/FilterParams.h"
#include "Params/Presets.h"

class SynthEngine;

class SUBnoteParameters : public Presets
{
    public:
        SUBnoteParameters(SynthEngine *_synth);
        ~SUBnoteParameters();
        void setPan(char pan, unsigned char panLaw);
        //bool randomPan(void) { return !PPanning; }
        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);
        float getLimits(CommandBlock *getData);
        void updateFrequencyMultipliers(void);

        // Amplitude Parametrers
        bool Pstereo; // true = stereo, false = mono
        unsigned char PVolume;
        unsigned char PPanning;
        bool PRandom;
        unsigned char PWidth;
        float pangainL;         // derived from PPanning
        float pangainR;         // ^^
        unsigned char PAmpVelocityScaleFunction;
        EnvelopeParams *AmpEnvelope;

        // Frequency Parameters
        unsigned short int PDetune;
        unsigned short int PCoarseDetune;
        unsigned char PDetuneType;
        unsigned char PFreqEnvelopeEnabled;
        EnvelopeParams *FreqEnvelope;
        unsigned char PBandWidthEnvelopeEnabled;
        EnvelopeParams *BandWidthEnvelope;

        unsigned char PBendAdjust; // Pitch Bend
        unsigned char POffsetHz;

        // Filter Parameters (Global)
        unsigned char PGlobalFilterEnabled;
        FilterParams *GlobalFilter;
        unsigned char PGlobalFilterVelocityScale;
        unsigned char PGlobalFilterVelocityScaleFunction;
        EnvelopeParams *GlobalFilterEnvelope;

        // Other Parameters
        unsigned char Pfixedfreq;   // If the base frequency is fixed to 440 Hz

        unsigned char PfixedfreqET; // Equal temperate (this is used only if the
                                    // Pfixedfreq is enabled)
                                    // If this parameter is 0, the frequency is
                                    // fixed (to 440 Hz)
                                    // if this parameter is 64,
                                    // 1 MIDI halftone -> 1 frequency halftone

        // Overtone spread parameters
        struct {
            unsigned char type;
            unsigned char par1;
            unsigned char par2;
            unsigned char par3;
        } POvertoneSpread;
        float POvertoneFreqMult[MAX_SUB_HARMONICS];

        unsigned char Pnumstages;   // how many times the filters are applied
        unsigned char Pbandwidth;

        unsigned char Phmagtype;    // how the magnitudes are computed
                                    // 0 = linear, 1 = -60dB, 2 = -60dB

        unsigned char PfilterChanged[MAX_SUB_HARMONICS]; // 0 = no, 6 = magnitude, 7 = bandwidth

        unsigned char Phmag[MAX_SUB_HARMONICS];   // Magnitudes

        unsigned char Phrelbw[MAX_SUB_HARMONICS]; // Relative BandWidth ("64"=1.0)

        unsigned char Pbwscale; // how much the bandwidth is increased according
                                // to lower/higher frequency; 64-default

        unsigned char Pstart;   // how the harmonics start, "0" = 0, "1" = random, "2" = 1
};

#endif



