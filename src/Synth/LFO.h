/*
    LFO.h - LFO implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017, Will Godfrey & others
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

    This file is derivative of ZynAddSubFX original code

*/

#ifndef LFO_H
#define LFO_H

#include "Params/LFOParams.h"
#include "Misc/NumericFuncs.h"

class SynthEngine;

class LFO
{
    public:
        LFO(LFOParams *_lfopars, float basefreq, SynthEngine *_synth);
        ~LFO() { };
        float lfoout(void);
        float amplfoout(void);
    private:
        std::pair<float, float> getBpmFrac() {
            return func::LFOfreqBPMFraction((float)lfopars->PfreqI / Fmul2I);
        }

        LFOParams *lfopars;
        Presets::PresetsUpdate lfoUpdate;
        void Recompute(void);
        void RecomputeFreq(void);
        void computenextincrnd(void);
        float x;
        float basefreq;
        float incx, incrnd, nextincrnd;
        float amp1, amp2; // used for randomness
        float lfointensity;
        float lfornd;
        float lfofreqrnd;
        float lfoelapsed;
        float startPhase;
        char lfotype;
        int freqrndenabled;

        float sampandholdvalue;
        int issampled;

        float prevMonotonicBeat;
        std::pair<float, float> prevBpmFrac;

        SynthEngine *synth;
};

#endif
