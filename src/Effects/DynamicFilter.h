/*
    DynamicFilter.h - "WahWah" effect and others

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018-2019, Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original.

    Modified March 2019
*/

#ifndef DYNAMICFILTER_H
#define DYNAMICFILTER_H

#include "DSP/Filter.h"
#include "Effects/EffectLFO.h"
#include "Effects/Effect.h"

static const int dynPRESET_SIZE = 10;
static const int dynNUM_PRESETS = 5;
static const char dynPresets[dynNUM_PRESETS][dynPRESET_SIZE] = {
        // WahWah
        { 110, 64, 80, 0, 0, 64, 0, 90, 0, 60 },
        // AutoWah
        {110, 64, 70, 0, 0, 80, 70, 0, 0, 60 },
        // Sweep
        {100, 64, 30, 0, 0, 50, 80, 0, 0, 60 },
        // VocalMorph1
        { 110, 64, 80, 0, 0, 64, 0, 64, 0, 60 },
        // VocalMorph1
        {127, 64, 50, 0, 0, 96, 64, 0, 0, 60 }
};

class SynthEngine;

class DynamicFilter : public Effect
{
    public:
        DynamicFilter(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine&);
       ~DynamicFilter();

        void out(float *smpsl, float *smpsr)  override;
        void setpreset(uchar npreset)         override;
        void changepar(int npar, uchar value) override;
        uchar getpar(int npar)          const override;
        void cleanup()                        override;


    private:
        // DynamicFilter parameters
        bool Pchanged;
        EffectLFO lfo; // lfo-ul DynamicFilter
        uchar Pvolume;
        uchar Pdepth;
        uchar Pampsns;
        uchar Pampsnsinv; // if the filter freq is lowered if the input amplitude rises
        uchar Pampsmooth; // how smooth the input amplitude changes the filter

        // Parameter Control
        void setvolume(uchar Pvolume_);
        void setdepth(uchar Pdepth_);
        void setampsns(uchar Pampsns_);
        void reinitfilter();

        // Internal Values
        float depth, ampsns, ampsmooth;

        Filter *filterl, *filterr;
        float ms1, ms2, ms3, ms4; // mean squares
};

class Dynamlimit
{
    public:
        float getlimits(CommandBlock *getData);
};

#endif

