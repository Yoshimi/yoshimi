/*
    DynamicFilter.h - "WahWah" effect and others

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018-2019, Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original.

    Modified March 2019
*/

#ifndef DYNAMICFILTER_H
#define DYNAMICFILTER_H

#include "DSP/Filter.h"
#include "Effects/EffectLFO.h"
#include "Effects/Effect.h"

class SynthEngine;

class DynamicFilter : public Effect
{
    public:
        DynamicFilter(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth);
        ~DynamicFilter();
        void out(float *smpsl, float *smpsr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar);
        void cleanup(void);
    //	void setdryonly();

    private:
        // Parametrii DynamicFilter
        bool Pchanged;
        EffectLFO lfo; // lfo-ul DynamicFilter
        unsigned char Pvolume;
        unsigned char Pdepth;
        unsigned char Pampsns;
        unsigned char Pampsnsinv; // if the filter freq is lowered if the input amplitude rises
        unsigned char Pampsmooth; // how smooth the input amplitude changes the filter

        // Parameter Control
        void setvolume(unsigned char Pvolume_);
        void setdepth(unsigned char Pdepth_);
        void setampsns(unsigned char Pampsns_);
        void reinitfilter(void);

        // Internal Values
        float depth, ampsns, ampsmooth;

        Filter *filterl, *filterr;
        float ms1, ms2, ms3, ms4; // mean squares

        SynthEngine *synth;
};

class Dynamlimit
{
    public:
        float getlimits(CommandBlock *getData);
};

#endif

