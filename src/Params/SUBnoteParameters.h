/*
    SUBnoteParameters.h - Parameters for SUBnote (SUBsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert
    Copyright 2017-2018, Will Godfrey
    Copyright 2020-2022 Kristian Amlie & others

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

    This file is derivative of ZynAddSubFX original code.

*/

#ifndef SUB_NOTE_PARAMETERS_H
#define SUB_NOTE_PARAMETERS_H

#include "Params/EnvelopeParams.h"
#include "Params/FilterParams.h"
#include "Params/ParamCheck.h"

class SynthEngine;
class XMLtree;


class SUBnoteParameters : public ParamBase
{
    public:
        SUBnoteParameters(SynthEngine&);
       ~SUBnoteParameters()  override;

        void defaults()  override;

        void  setPan(char pan, uchar panLaw);
        void  add2XML(XMLtree&);
        void  getfromXML(XMLtree&);
        float getLimits(CommandBlock* getData);
        void  updateFrequencyMultipliers();

        // Amplitude Parametrers
        bool  Pstereo; // true = stereo, false = mono
        uchar PVolume;
        uchar PPanning;
        bool  PRandom;
        uchar PWidth;
        float pangainL;         // derived from PPanning
        float pangainR;         // ^^
        uchar PAmpVelocityScaleFunction;
        EnvelopeParams *AmpEnvelope;

        // Frequency Parameters
        ushort PDetune;
        ushort PCoarseDetune;
        uchar  PDetuneType;

        bool            PFreqEnvelopeEnabled;
        EnvelopeParams* FreqEnvelope;
        bool            PBandWidthEnvelopeEnabled;
        EnvelopeParams* BandWidthEnvelope;

        uchar PBendAdjust; // Pitch Bend
        uchar POffsetHz;

        // Filter Parameters (Global)
        bool  PGlobalFilterEnabled;
        FilterParams* GlobalFilter;
        uchar PGlobalFilterVelocityScale;
        uchar PGlobalFilterVelocityScaleFunction;
        EnvelopeParams* GlobalFilterEnvelope;

        // Other Parameters
        uchar Pfixedfreq;   // If the base frequency is fixed to 440 Hz

        uchar PfixedfreqET; // Equal temperate (this is used only if the
                            // Pfixedfreq is enabled)
                            // If this parameter is 0, the frequency is
                            // fixed (to 440 Hz)
                            // if this parameter is 64,
                            // 1 MIDI halftone -> 1 frequency halftone

        // Overtone spread parameters
        struct {
            uchar type;
            uchar par1;
            uchar par2;
            uchar par3;
        } POvertoneSpread;
        float POvertoneFreqMult[MAX_SUB_HARMONICS];

        uchar Pnumstages;   // how many times the filters are applied
        uchar Pbandwidth;

        uchar Phmagtype;    // how the magnitudes are computed
                            // 0 = linear, 1 = -60dB, 2 = -60dB

        uchar Phmag[MAX_SUB_HARMONICS];   // Magnitudes

        uchar Phrelbw[MAX_SUB_HARMONICS]; // Relative BandWidth ("64"=1.0)

        uchar Pbwscale; // how much the bandwidth is increased according
                        // to lower/higher frequency; 64-default
        uchar Pstart;   // how the harmonics start, "0" = 0, "1" = random, "2" = 1
};

#endif /*SUB_NOTE_PARAMETERS_H*/

