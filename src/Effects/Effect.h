/*
    Effect.h - inherited by the all effects

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
    Modified march 2018
*/

#ifndef EFFECT_H
#define EFFECT_H

#include "Params/FilterParams.h"
#include "Misc/SynthHelper.h"

class Effect
{
    public:
        Effect(bool insertion_, float *efxoutl_, float *efxoutr_,
               FilterParams *filterpars_, unsigned char Ppreset_,
               SynthEngine *synth_);
        virtual ~Effect() { };

        virtual void setpreset(unsigned char npreset) = 0;
        virtual void changepar(int npar, unsigned char value) = 0;
        virtual unsigned char getpar(int npar) = 0;
        virtual void out(float *smpsl, float *smpsr) = 0;
        virtual void cleanup();
        virtual float getfreqresponse(float /* freq */) { return (0); };

        unsigned char Ppreset; // Current preset
        float *const efxoutl;
        float *const efxoutr;
        synth::InterpolatedValue<float> outvolume;
        synth::InterpolatedValue<float> volume;
        FilterParams *filterpars;

    protected:
        void setpanning(char Ppanning_);
        void setlrcross(char Plrcross_);

        bool  insertion;
        char  Ppanning;
        synth::InterpolatedValue<float> pangainL;
        synth::InterpolatedValue<float> pangainR;
        char  Plrcross; // L/R mix
        synth::InterpolatedValue<float> lrcross;

        SynthEngine *synth;
};

#endif

