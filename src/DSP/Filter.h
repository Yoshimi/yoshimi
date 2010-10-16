/*
    Filter.h - Filters, uses analog,formant,etc. filters

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

    This file is a derivative of a ZynAddSubFX original, modified October 2010
*/

#ifndef FILTER_H
#define FILTER_H

#include "Misc/MiscFuncs.h"
#include "DSP/Filter_.h"
#include "DSP/AnalogFilter.h"
#include "DSP/FormantFilter.h"
#include "DSP/SVFilter.h"
#include "Params/FilterParams.h"

class Filter : private MiscFuncs
{
    public:
        Filter(FilterParams *pars);
        ~Filter();
        void filterout(float *smp);
        void setfreq(float frequency);
        void setfreq_and_q(float frequency, float q_);
        void setq(float q_);
        float getrealfreq(float freqpitch);

    private:
        Filter_ *filter;
        unsigned char category;
};

#endif
