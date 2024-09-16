/*
    Filter.cpp - Filters, uses analog,formant,etc. filters

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2020 Kristian Amlie

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

    This file is derivative of ZynAddSubFX original code
*/

#include "DSP/Filter.h"
#include "Misc/NumericFuncs.h"

using func::decibel;
using func::power;


Filter_* Filter::buildImpl(SynthEngine& synth)
{
    uchar type = params.Ptype;
    uchar stages = params.Pstages;
    switch (category)
    {
        case 1 : return new FormantFilter(&synth, &params);
        case 2 : return new SVFilter(&synth, type, 1000.0f, params.getq(), stages);
        default: return new AnalogFilter(synth, type, 1000.0f, params.getq(), stages);
    }
}


void Filter::updateCurrentParameters()
{
    switch (category)
    {
        case 1:
            // Handled inside filter.
            break;

        case 2:
            filterImpl->outgain = decibel(params.getgain());
            if (filterImpl->outgain > 1.0f)
                filterImpl->outgain = sqrtf(filterImpl->outgain);
            break;

        default:
            uchar Ftype = params.Ptype;
            if (Ftype >= 6 and Ftype <= 8)
                filterImpl->setgain(params.getgain());
            else
                filterImpl->outgain = decibel(params.getgain());
            break;
    }
}

void Filter::filterout(float *smp)
{
    if (parsUpdate.checkUpdated())
        updateCurrentParameters();

    filterImpl->filterout(smp);
}


void Filter::setfreq(float frequency)
{
    filterImpl->setfreq(frequency);
}


void Filter::setfreq_and_q(float frequency, float q_)
{
    filterImpl->setfreq_and_q(frequency, q_);
}


void Filter::setq(float q_)
{
    filterImpl->setq(q_);
}


float Filter::getrealfreq(float freqpitch)
{
    if (category == 0 || category == 2)
        return power<2>(freqpitch + 9.96578428f); // log2(1000)=9.95748
    else
        return freqpitch;
}
