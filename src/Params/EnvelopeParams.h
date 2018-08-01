/*
    EnvelopeParams.h - Parameters for Envelope

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018, Will Godfrey

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

#endif
