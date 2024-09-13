/*
    Filter.h - Filters, uses analog,formant,etc. filters

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
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

#ifndef FILTER_H
#define FILTER_H

#include "DSP/Filter_.h"
#include "DSP/AnalogFilter.h"
#include "DSP/FormantFilter.h"
#include "DSP/SVFilter.h"
#include "Params/FilterParams.h"

#include <memory>

class SynthEngine;

class Filter
{
    public:
       ~Filter() = default;

        Filter(FilterParams& p, SynthEngine& synth)
            : category{p.Pcategory}
            , params{p}
            , parsUpdate{p}
            , filterImpl{buildImpl(synth)}
            {
                updateCurrentParameters();
            }

        // can be cloned
        Filter(Filter const& o)
            : category{o.category}
            , params{o.params}
            , parsUpdate{o.parsUpdate}
            , filterImpl{o.filterImpl->clone()}
            { };
        // can be moved, but not assigned
        Filter(Filter&&)                 = default;
        Filter& operator=(Filter&&)      = delete;
        Filter& operator=(Filter const&) = delete;

        void filterout(float *smp);
        void setfreq(float frequency);
        void setfreq_and_q(float frequency, float q_);
        void setq(float q_);
        float getrealfreq(float freqpitch);

    private:
        Filter_* buildImpl(SynthEngine&);
        void updateCurrentParameters();

        uchar category;
        FilterParams& params;
        ParamBase::ParamsUpdate parsUpdate;
        std::unique_ptr<Filter_> filterImpl;
};

#endif
