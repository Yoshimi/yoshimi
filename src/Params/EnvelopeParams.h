/*
    EnvelopeParams.h - Parameters for Envelope

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018, Will Godfrey

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

    Modified August 2018
*/

#ifndef ENVELOPE_PARAMS_H
#define ENVELOPE_PARAMS_H

#include "Params/Presets.h"

class XMLwrapper;

class SynthEngine;

class EnvelopeParams : public Presets
{
    public:
        EnvelopeParams(unsigned char Penvstretch_, unsigned char Pforcedrelease_, SynthEngine *_synth);
        ~EnvelopeParams() { }
        void ADSRinit(float A_dt, float D_dt, float S_val, float R_dt);
        void ADSRinit_dB(float A_dt, float D_dt, float S_val, float R_dt);
        void ASRinit(float A_val, float A_dt, float R_val, float R_dt);
        void ADSRinit_filter(float A_val, float A_dt, float D_val, float D_dt,
                             float R_dt, float R_val);
        void ASRinit_bw(float A_val, float A_dt, float R_val, float R_dt);
        void converttofree(void);

        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);

        float getdt(size_t i);

        // MIDI Parameters
        unsigned char Pfreemode;       // 1 if it is in free mode or 0 if it is in ADSR or ASR mode (comment from original author)
        size_t Penvpoints;             // stays < MAX_ENVELOPE_POINTS
        size_t Penvsustain;            // 0 means disabled  -- see Envelope::envout()
        float Penvdt[MAX_ENVELOPE_POINTS];
        float Penvval[MAX_ENVELOPE_POINTS];
        unsigned char Penvstretch;     // 64=normal stretch (piano-like), 0=no stretch
        unsigned char Pforcedrelease;  // 0 - OFF, 1 - ON
        unsigned char Plinearenvelope; // if the amplitude envelope is linear

        float PA_dt, PD_dt, PR_dt,
                      PA_val, PD_val, PS_val, PR_val;

        int Envmode; // 1 for ADSR parameters (linear amplitude)
                     // 2 for ADSR_dB parameters (dB amplitude)
                     // 3 for ASR parameters (frequency LFO)
                     // 4 for ADSR_filter parameters (filter parameters)
                     // 5 for ASR_bw parameters (bandwidth parameters)

    private:
        void store2defaults(void);

        // Default parameters
        unsigned char Denvstretch;
        unsigned char Dforcedrelease;
        unsigned char Dlinearenvelope;
        float DA_dt, DD_dt, DR_dt,
                      DA_val, DD_val, DS_val, DR_val;
};

class envelopeLimit
{
    public:
        float getEnvelopeLimits(CommandBlock *getData);
};

struct ENVminmax{
    float min;
    float max;
    float def;
    bool learn;
    bool integer;
};
namespace ENVDEF{
    const ENVminmax ampAttackTime {0,127,0 ,true,false};
        const ENVminmax modAmpAttackTime {0,127,80 ,true,false};
    const ENVminmax ampDecayTime {0,127,40 ,true,false};
        const ENVminmax voiceAmpDecayTime {0,127,100,true,false};
        const ENVminmax modAmpDecayTime {0,127,90,true,false};
    const ENVminmax ampSustainValue {0,127,127,true,false};
    const ENVminmax ampReleaseTime {0,127,25,true,false};
        const ENVminmax voiceAmpReleaseTime {0,127,100,true,false};
        const ENVminmax modAmpReleaseTime {0,127,100,true,false};
    const ENVminmax ampStretch {0,127,64 ,true,true};

    const ENVminmax freqAttackValue {0,127,64,true,false};
        const ENVminmax voiceFreqAtValue {0,127,30,true,false};
        const ENVminmax modFreqAtValue {0,127,20,true,false};
        const ENVminmax subFreqAtValue {0,127,30,true,false};
    const ENVminmax freqAttackTime {0,127,50,true,false};
        const ENVminmax voiceFreqAtTime {0,127,40,true,false};
        const ENVminmax modFreqAtTime {0,127,90,true,false};
    const ENVminmax freqReleaseTime {0,127,60,true,false};
        const ENVminmax modFreqReleaseTime {0,127,80,true,false};
    const ENVminmax freqReleaseValue {0,127,64,true,false};
        const ENVminmax modFreqReleaseValue {0,127,40,true,false};
    const ENVminmax freqStretch {0,127,0,true,true};
        const ENVminmax subFreqStretch {0,127,64,true,true};

    const ENVminmax subBandAttackValue {0,127,100,true,false};
    const ENVminmax subBandAttackTime {0,127,70,true,false};
    const ENVminmax subBandReleaseTime {0,127,60,true,false};
    const ENVminmax subBandReleaseValue {0,127,64,true,false};
    const ENVminmax subBandStretch {0,127,64,true,false};

    const ENVminmax filtAttackValue {0,127,64,true,false};
        const ENVminmax voiceFiltAtValue {0,127,90,true,false};
    const ENVminmax filtAttackTime {0,127,40,true,false};
        const ENVminmax voiceFiltAtTime {0,127,70,true,false};
    const ENVminmax filtDecayValue {0,127,64,true,false};
        const ENVminmax voiceFiltDeValue {0,127,40,true,false};
    const ENVminmax filtDecayTime {0,127,70,true,false};
    const ENVminmax filtReleaseTime {0,127,60,true,false};
        const ENVminmax voiceFiltRelTime {0,127,10,true,false};
    const ENVminmax filtReleaseValue {0,127,64,true,false};
        const ENVminmax voiceFiltRelValue {0,127,40,true,false};
    const ENVminmax filtStretch {0,127,0,true,true};
    const ENVminmax point {0,MAX_ENVELOPE_POINTS-1,2,false,true};
        const ENVminmax freqPoint {0,MAX_ENVELOPE_POINTS-1,1,false,true};
        const ENVminmax bandPoint {0,MAX_ENVELOPE_POINTS-1,1,false,true};
    const ENVminmax count {0,MAX_ENVELOPE_POINTS-1,4,false,true};
        const ENVminmax freqCount {0,MAX_ENVELOPE_POINTS-1,3,false,true};
        const ENVminmax bandCount {0,MAX_ENVELOPE_POINTS-1,3,false,true};
}
namespace ENVSWITCH{
    const bool defLinear = false;
    const bool defForce = true;
        const bool defForceVoiceFilt = false;
        const bool defForceFreq = false;
        const bool defForceBand = false;
    const bool defFreeMode = false;
}
#endif
