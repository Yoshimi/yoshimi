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
        void ADSRinit(char A_dt, char D_dt, char S_val, char R_dt);
        void ADSRinit_dB(char A_dt, char D_dt, char S_val, char R_dt);
        void ASRinit(char A_val, char A_dt, char R_val, char R_dt);
        void ADSRinit_filter(char A_val, char A_dt, char D_val, char D_dt,
                             char R_dt, char R_val);
        void ASRinit_bw(char A_val, char A_dt, char R_val, char R_dt);
        void converttofree(void);

        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);

        float getdt(char i);

        // MIDI Parameters
        unsigned char Pfreemode; // 1 daca este in modul free sau 0 daca este in mod ADSR,ASR,...
        unsigned char Penvpoints;
        unsigned char Penvsustain; // 127 pentru dezactivat
        unsigned char Penvdt[MAX_ENVELOPE_POINTS];
        unsigned char Penvval[MAX_ENVELOPE_POINTS];
        unsigned char Penvstretch; // 64=normal stretch (piano-like), 0=no stretch
        unsigned char Pforcedrelease; // 0 - OFF, 1 - ON
        unsigned char Plinearenvelope; // if the amplitude envelope is linear

        unsigned char PA_dt, PD_dt, PR_dt,
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
        unsigned char DA_dt, DD_dt, DR_dt,
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
    const ENVminmax ampAttackTime {0,127,0 ,true,true};
        const ENVminmax modAmpAttackTime {0,127,80 ,true,true};
    const ENVminmax ampDecayTime {0,127,40 ,true,true};
        const ENVminmax voiceAmpDecayTime {0,127,100,true,true};
        const ENVminmax modAmpDecayTime {0,127,90,true,true};
    const ENVminmax ampSustainValue {0,127,127,true,true};
    const ENVminmax ampReleaseTime {0,127,25,true,true};
        const ENVminmax voiceAmpReleaseTime {0,127,100,true,true};
        const ENVminmax modAmpReleaseTime {0,127,100,true,true};
    const ENVminmax ampStretch {0,127,64 ,true,true};

    const ENVminmax freqAttackValue {0,127,64,true,true};
        const ENVminmax voiceFreqAtValue {0,127,30,true,true};
        const ENVminmax modFreqAtValue {0,127,20,true,true};
        const ENVminmax subFreqAtValue {0,127,30,true,true};
    const ENVminmax freqAttackTime {0,127,50,true,true};
        const ENVminmax voiceFreqAtTime {0,127,40,true,true};
        const ENVminmax modFreqAtTime {0,127,90,true,true};
    const ENVminmax freqReleaseTime {0,127,60,true,true};
        const ENVminmax modFreqReleaseTime {0,127,80,true,true};
    const ENVminmax freqReleaseValue {0,127,64,true,true};
        const ENVminmax modFreqReleaseValue {0,127,40,true,true};
    const ENVminmax freqStretch {0,127,0,true,true};
        const ENVminmax subFreqStretch {0,127,64,true,true};

    const ENVminmax subBandAttackValue {0,127,100,true,true};
    const ENVminmax subBandAttackTime {0,127,70,true,true};
    const ENVminmax subBandReleaseTime {0,127,60,true,true};
    const ENVminmax subBandReleaseValue {0,127,64,true,true};
    const ENVminmax subBandStretch {0,127,64,true,true};

    const ENVminmax filtAttackValue {0,127,64,true,true};
        const ENVminmax voiceFiltAtValue {0,127,90,true,true};
    const ENVminmax filtAttackTime {0,127,40,true,true};
        const ENVminmax voiceFiltAtTime {0,127,70,true,true};
    const ENVminmax filtDecayValue {0,127,64,true,true};
        const ENVminmax voiceFiltDeValue {0,127,40,true,true};
    const ENVminmax filtDecayTime {0,127,70,true,true};
    const ENVminmax filtReleaseTime {0,127,60,true,true};
        const ENVminmax voiceFiltRelTime {0,127,10,true,true};
    const ENVminmax filtReleaseValue {0,127,64,true,true};
        const ENVminmax voiceFiltRelValue {0,127,40,true,true};
    const ENVminmax filtStretch {0,127,0,true,true};
}
#endif
