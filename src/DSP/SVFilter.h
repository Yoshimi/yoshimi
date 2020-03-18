/*
    SVFilter.h - Several state-variable filters

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2020 Kristian Amlie

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

    This file is a derivative of a ZynAddSubFX original
*/

#ifndef SV_FILTER_H
#define SV_FILTER_H

#include "DSP/Filter_.h"

class SynthEngine;

class SVFilter : public Filter_
{
    public:
        SVFilter(unsigned char Ftype, float Ffreq, float Fq, unsigned char Fstages, SynthEngine *_synth);
        SVFilter(const SVFilter &orig);
        ~SVFilter();
        Filter_* clone() { return new SVFilter(*this); };
        void filterout(float *smp);
        void setfreq(float frequency);
        void setfreq_and_q(float frequency, float q_);
        void setq(float q_);

        void settype(int type_);
        void setstages(int stages_);
        void cleanup();

    private:
        struct fstage {
            float low, high, band, notch;
        } st[MAX_FILTER_STAGES + 1];

        struct parameters {
            float f, q, q_sqrt;
        } par, ipar;

        void singlefilterout(float *smp, fstage &x, parameters &par);
        void computefiltercoefs(void);
        int type;      // The type of the filter (LPF1,HPF1,LPF2,HPF2...)
        int stages;    // how many times the filter is applied (0->1,1->2,etc.)
        float freq; // Frequency given in Hz
        float q;    // Q factor (resonance or Q factor)

        int abovenq;   // this is 1 if the frequency is above the nyquist
        int oldabovenq;
        int needsinterpolation, firsttime;
        float *tmpismp; // used if it needs interpolation in filterout()

        SynthEngine *synth;
};

#endif
