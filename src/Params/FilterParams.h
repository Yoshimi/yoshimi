/*
    FilterParams.h - Parameters for filter

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018 - 2023, Will Godfrey

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

#ifndef FILTER_PARAMS_H
#define FILTER_PARAMS_H

#include "Params/ParamCheck.h"
#include "Misc/NumericFuncs.h"
#include "globals.h"

#include <cmath>

using func::power;
using func::decibel;

class XMLwrapper;
class SynthEngine;

class FilterParams : public ParamBase
{
    public:
        FilterParams(unsigned char Ptype_, float Pfreq, float Pq_, unsigned char Pfreqtrackoffset_, SynthEngine *_synth);
        ~FilterParams() { }

        void add2XML(XMLwrapper *xml);
        void add2XMLsection(XMLwrapper *xml, int n);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);
        void getfromXMLsection(XMLwrapper *xml, int n);


        void getfromFilterParams(FilterParams *pars);

        float getfreq(void);
        float getq(void);
        float getfreqtracking(float notefreq);
        float getgain(void);

        float getcenterfreq(void);
        float getoctavesfreq(void);
        float getfreqpos(float freq);
        float getfreqx(float x);

        void formantfilterH(int nvowel, int nfreqs, float *freqs); // used by UI

        float getformantfreq(float freq) // Transforms a parameter to
            { return getfreqx(freq / 127.0f); }  // the real value
        float getformantamp(float amp)
            { return decibel<-80>(1.0f - amp / 127.0f); }
        float getformantq(float q)
            { return power<25>((q - 32.0f) / 64.0f); }

        unsigned char Pcategory;  // Filter category (Analog/Formant/StVar)
        unsigned char Ptype;      // Filter type  (for analog lpf,hpf,bpf..)
        float Pfreq;              // Frequency (64-central frequency)
        float Pq;                 // Q parameters (resonance or bandwidth)
        unsigned char Pstages;    // filter stages+1
        float Pfreqtrack;         // how the filter frequency is changing
                                  // according the note frequency
        unsigned char Pfreqtrackoffset;  // Shift range for freq tracking
        float Pgain;      // filter's output gain

        // Formant filter parameters
        unsigned char Pnumformants;     // how many formants are used
        unsigned char Pformantslowness; // how slow varies the formants
        unsigned char Pvowelclearness;  // how vowels are kept clean (how much try
                                        // to avoid "mixed" vowels)
        unsigned char Pcenterfreq;      // the centre frequency of the res. func.
        unsigned char Poctavesfreq;     // the number of octaves
        struct {
            struct {
                float firstF, freq, amp, q; // frequency,amplitude,Q
            } formants[FF_MAX_FORMANTS];
        } Pvowels[FF_MAX_VOWELS];

        unsigned char Psequencesize;     // how many vowels are in the sequence
        float Psequencestretch;  // how the sequence is stretched (how
                                         // the input from filter envelopes/LFOs/etc.
                                         // is "stretched")
        unsigned char Psequencereversed; // if the input from filter envelopes/LFOs/etc.
                                         // is reversed(negated)
        struct {
            unsigned char nvowel; // the vowel from the position
        } Psequence[FF_MAX_SEQUENCE];

        bool changed;

    private:
        void defaults(int n);

        // stored default parameters
        unsigned char Dtype;
        float Dfreq;
        float Dq;
        unsigned char Dfreqtrackoffset;
};

class filterLimit
{
    public:
        float getFilterLimits(CommandBlock *getData);
};

struct FILTminmax{
    float min;
    float max;
    float def;
    bool learn;
    bool integer;
};
namespace FILTDEF{
    const FILTminmax addFreq {0,127,94,true,false};
    const FILTminmax voiceFreq {0,127,50,true,false};
    const FILTminmax subFreq {0,127,80,true,false};
    const FILTminmax padFreq {0,127,94,true,false};

        const FILTminmax dynFreq0 {0,127,45,true,false};
        const FILTminmax dynFreq1 {0,127,72,true,false};
        const FILTminmax dynFreq2 {0,127,64,true,false};
        const FILTminmax dynFreq3 {0,127,50,true,false};
        const FILTminmax dynFreq4 {0,127,64,true,false};

    const FILTminmax qVal {0,127,40,true,false};
        const FILTminmax voiceQval {0,127,60,true,false};

        const FILTminmax dynQval0 {0,127,64,true,false};
        const FILTminmax dynQval1 {0,127,64,true,false};
        const FILTminmax dynQval2 {0,127,64,true,false};
        const FILTminmax dynQval3 {0,127,70,true,false};
        const FILTminmax dynQval4 {0,127,70,true,false};

    const FILTminmax velSense {0,127,64,true,false};
        const FILTminmax voiceVelSense {0,127,0,true,false};
    const FILTminmax velFuncSense {0,127,64,true,true};
    const FILTminmax gain {0,127,64,true,false};
    const FILTminmax freqTrack {0,127,64,true,true};

    const FILTminmax formCount {1,FF_MAX_FORMANTS,3,false,true};
    const FILTminmax formSpeed {0,127,64,true,false};
    const FILTminmax formClear {0,127,64,true,false};
    const FILTminmax formFreq {0,127,-1,true,false}; // pseudo default value
    const FILTminmax formQ {0,127,64,true,false};
    const FILTminmax formAmp {0,127,127,true,false};
    const FILTminmax formStretch {0,127,40,true,false};
    const FILTminmax formCentre {0,127,64,true,true};
    const FILTminmax formOctave {0,127,64,true,true};
    const FILTminmax formVowel {1,FF_MAX_SEQUENCE,1,false,true};
    const FILTminmax sequenceSize{1,FF_MAX_SEQUENCE,3,false,true};

    const FILTminmax stages {0,MAX_FILTER_STAGES-1,0,false,true};
        const FILTminmax dynStages {0,MAX_FILTER_STAGES-1,1,false,true};
    const FILTminmax category {0,2,0,false,true};
    const FILTminmax analogType {0,8,2,false,true};
    const FILTminmax stVarfType {0,3,0,false,true};
}

struct DYNinsert{
    float freq;
    float amp;
    float q;
};

namespace DYNform{
    const DYNinsert Preset3V0F0 {34,127,64};
    const DYNinsert Preset3V0F1 {99,122,64};
    const DYNinsert Preset3V0F2 {108,112,64};
    const DYNinsert Preset3V1F0 {61,127,64};
    const DYNinsert Preset3V1F1 {71,121,64};
    const DYNinsert Preset3V1F2 {99,117,64};

    const DYNinsert Preset4V0F0 {70,127,64};
    const DYNinsert Preset4V0F1 {80,122,64};
    const DYNinsert Preset4V1F0 {20,127,64};
    const DYNinsert Preset4V1F1 {100,121,64};
}

namespace FILTSWITCH{
    const bool trackRange = false;
    const bool sequenceReverse = false;
}
#endif
