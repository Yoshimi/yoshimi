/*
    Effect.h - inherited by the all effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of original ZynAddSubFX code, modified November 2010
*/

#ifndef EFFECT_H
#define EFFECT_H

#include "Params/FilterParams.h"

typedef enum {
    indeterminate = 0,
    reverb = 1,
    echo = 2,
    chorus = 3,
    phaser = 4,
    alienwah = 5,
    distorsion = 6,
    eq = 7,
    dynamicfilter = 8
} fx_type;

class Effect
{
    public:
        Effect(bool insertion_, float *efxoutl_, float *efxoutr_,
               FilterParams *filterpars_, unsigned char Ppreset_);
        virtual ~Effect() { };

        virtual void setpreset(unsigned char npreset) = 0;
        virtual void changepar(int npar, unsigned char value) = 0;
        virtual unsigned char getpar(int npar) = 0;
        virtual void out(float *smpsl, float *smpsr) = 0;
        virtual void cleanup() { };
        virtual float getfreqresponse(float freq) { return (0.0f); }

        unsigned char Ppreset; // current preset
        float *const efxoutl;
        float *const efxoutr;
        float outvolume;
        float volume;
        FilterParams *filterpars;
        fx_type effect_type;

    protected:
        bool insertion;
};

#endif
