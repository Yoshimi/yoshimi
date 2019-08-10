/*
    Filter.h - Filters, uses analog,formant,etc. filters

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

    This file is derivative of ZynAddSubFX original code, modified October 2011
*/

#ifndef FILTER_H
#define FILTER_H

#include "DSP/Filter_.h"
#include "DSP/AnalogFilter.h"
#include "DSP/FormantFilter.h"
#include "DSP/SVFilter.h"
#include "Params/FilterParams.h"

class SynthEngine;

class Filter
{
    public:
        Filter(FilterParams *pars, SynthEngine *_synth);
        ~Filter();
        void filterout(float *smp);
        void setfreq(float frequency);
        void setfreq_and_q(float frequency, float q_);
        void setq(float q_);
        float getrealfreq(float freqpitch);

    private:
        Filter_ *filter;
        unsigned char category;

        SynthEngine *synth;
};

#endif
