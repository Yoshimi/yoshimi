/*
    EffectLFO.h - Stereo LFO used by some effects

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

#ifndef EFFECT_LFO_H
#define EFFECT_LFO_H

#include "globals.h"

class EffectLFO
{
    public:
        EffectLFO();
        ~EffectLFO();
        void effectlfoout(float *outl, float *outr);
        void updateparams(void);
        unsigned char Pfreq;
        unsigned char Prandomness;
        unsigned char PLFOtype;
        unsigned char Pstereo; // "64"=0
    private:
        float getlfoshape(float x);

        float xl,xr;
        float incx;
        float ampl1, ampl2, ampr1, ampr2; // necessary for "randomness"
        float lfointensity;
        float lfornd;
        char lfotype;
};

#endif
