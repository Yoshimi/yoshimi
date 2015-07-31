/*
    LFO.h - LFO implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified January 2011
*/

#ifndef LFO_H
#define LFO_H

#include "Params/LFOParams.h"
#include "Params/ControllableByMIDI.h"

class SynthEngine;

class LFO: public ControllableByMIDI
{
    public:
        LFO(LFOParams *lfopars, float basefreq, SynthEngine *_synth);
        ~LFO();
        float lfoout(void);
        float amplfoout(void);
        void changepar(int npar, double value);
        unsigned char getparChar(int npar){ return 0;};
        float getparFloat(int npar);

        enum {
            c_Pfreq,
            c_Pintensity,
            c_Pstartphase,
            c_PLFOtype,
            c_Prandomness,
            c_Pfreqrand,
            c_Pdelay,
            c_Pcontinous,
            c_Pstretch
        };

    private:
        void computenextincrnd(void);
        float x;
        float incx, incrnd, nextincrnd;
        float amp1, amp2; // used for randomness
        float lfointensity;
        float lfornd;
        float lfofreqrnd;
        float lfodelay;
        char lfotype;
        int freqrndenabled;
        float basefreq;

        LFOParams *lfopars;
        SynthEngine *synth;
};

#endif
