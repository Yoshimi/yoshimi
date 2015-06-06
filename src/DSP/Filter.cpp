/*
    Filter.cpp - Filters, uses analog,formant,etc. filters

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

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

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "DSP/Filter.h"

Filter::Filter(FilterParams *pars)
{
    unsigned char Ftype = pars->Ptype;
    unsigned char Fstages = pars->Pstages;

    category = pars->Pcategory;

    switch (category)
    {
        case 1:
            filter = new FormantFilter(pars);
            break;
        case 2:
            filter = new SVFilter(Ftype, 1000.0, pars->getQ(), Fstages);
            filter->outgain = dB2rap(pars->getGain());
            if (filter->outgain > 1.0)
                filter->outgain = sqrtf(filter->outgain);
            break;
        default:
            filter = new AnalogFilter(Ftype, 1000.0, pars->getQ(), Fstages);
            if (Ftype >= 6 && Ftype <= 8)
                filter->setGain(pars->getGain());
            else
                filter->outgain = dB2rap(pars->getGain());
            break;
    }
}

Filter::~Filter()
{
    delete (filter);
}

void Filter::filterOut(float *smp)
{
    filter->filterOut(smp);
}

void Filter::setFreq(float frequency)
{
    filter->setFreq(frequency);
}

void Filter::setFreq_and_Q(float frequency, float q_)
{
    filter->setFreq_and_Q(frequency, q_);
}

void Filter::setQ(float q_)
{
    filter->setQ(q_);
}

float Filter::getRealFreq(float freqpitch)
{
    if (category == 0 || category == 2)
        return powf(2.0, freqpitch + 9.96578428); // log2(1000)=9.95748
    else
        return freqpitch;
}
