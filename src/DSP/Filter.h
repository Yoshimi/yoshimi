/*
    Filter.h - Filters, uses analog,formant,etc. filters

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

#ifndef FILTER_H
#define FILTER_H

#include "globals.h"

#include "DSP/Filter_.h"
#include "DSP/AnalogFilter.h"
#include "DSP/FormantFilter.h"
#include "DSP/SVFilter.h"
#include "Params/FilterParams.h"

class Filter
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
