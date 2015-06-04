/*
    LFO.h - LFO implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LFO_H
#define LFO_H

#include "globals.h"
#include "Params/LFOParams.h"

class LFO
{
    public:
        LFO(LFOParams *lfopars, float basefreq);
        ~LFO() { };
        float lfoout(void);
        float amplfoout(void);
    private:
        void computenextincrnd(void);
        float x;
        float incx, incrnd, nextincrnd;
        float amp1, amp2; // used for randomness
        float lfointensity;
        float lfornd, lfofreqrnd;
        float lfodelay;
        char lfotype;
        int freqrndenabled;
};

#endif

