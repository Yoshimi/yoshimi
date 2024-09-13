/*
    Analog Filter.h - Several analog filters (lowpass, highpass...)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is a derivative of a ZynAddSubFX original, modified April 2011
*/

#ifndef ANALOG_FILTER_H
#define ANALOG_FILTER_H

#include "DSP/Filter_.h"
#include "Misc/Alloc.h"
#include "globals.h"

#include <array>


class SynthEngine;

class AnalogFilter : public Filter_
{
    public:
       ~AnalogFilter() = default;
        AnalogFilter(SynthEngine&, uchar _type, float _freq, float _q, uchar _stages, float dBgain =1.0);
        Filter_* clone() override { return new AnalogFilter(*this); };

        // can be cloned and moved, but not assigned
        AnalogFilter(AnalogFilter const&);
        AnalogFilter(AnalogFilter&&)                 = default;
        AnalogFilter& operator=(AnalogFilter&&)      = delete;
        AnalogFilter& operator=(AnalogFilter const&) = delete;


        void filterout(float* smp);
        void setfreq(float);
        float getFreq();
        void setfreq_and_q(float frequency, float q_);
        void setq(float);

        void settype(int type_);
        void setgain(float dBgain);
        void setstages(int stages_);
        // Request that the next buffer be interpolated. Should be called before
        // changing parameters so that coefficients can be saved.
        void interpolatenextbuffer();
        void cleanup();

        float calcFilterResponse(float freq) const;

        static constexpr uint MAX_TYPES = 1 + TOPLEVEL::filter::HighShelf2;  // NOTE: change this if adding new filter types

    private:
        struct FStage {
            float c1{0}, c2{0};
        };
        using FStages = std::array<FStage,MAX_FILTER_STAGES + 1>;
        FStages x,y,oldx,oldy;

        uint type;          // The type of the filter (LPF1,HPF1,LPF2,HPF2...)
        uint stages;        // how many times the filter is applied (0->1,1->2,etc.)
        float freq;         // Frequency given in Hz
        float q;            // Q factor (resonance or Q factor)
        float gain;         // the gain of the filter (if are shelf/peak) filters

        uint order;         // the order of the filter (number of poles)

        using Coeffs = std::array<float,3>;
        Coeffs c, d;        // coefficients
        Coeffs oldc, oldd;  // old coefficients (used to interpolate on fast changes)

        bool needsinterpolation, firsttime;
        bool abovenq;       // if frequency is above the nyquist
        bool oldabovenq;    // (last state to determine if it needs interpolation)

        Samples tmpismp;    // used if it needs interpolation in filterout()
        SynthEngine& synth;

        void singlefilterout(float* smp, FStage& x, FStage& y, Coeffs const& c, Coeffs const& d);
        void computefiltercoefs();
};

#endif /*ANALOG_FILTER_H*/
