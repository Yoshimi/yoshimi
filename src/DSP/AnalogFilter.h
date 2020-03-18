/*
    Analog Filter.h - Several analog filters (lowpass, highpass...)

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

    This file is a derivative of a ZynAddSubFX original, modified April 2011
*/

#ifndef ANALOG_FILTER_H
#define ANALOG_FILTER_H

#include "DSP/Filter_.h"
#include "globals.h"

class SynthEngine;

class AnalogFilter : public Filter_
{
    public:
        AnalogFilter(unsigned char Ftype, float Ffreq, float Fq,
                     unsigned char Fstages, SynthEngine *_synth);
        AnalogFilter(const AnalogFilter &orig);
        ~AnalogFilter();
        Filter_* clone() { return new AnalogFilter(*this); };
        void filterout(float *smp);
        void setfreq(float frequency);
        void setfreq_and_q(float frequency, float q_);
        void setq(float q_);

        void settype(int type_);
        void setgain(float dBgain);
        void setstages(int stages_);
        // Request that the next buffer be interpolated. Should be called before
        // changing parameters so that coefficients can be saved.
        void interpolatenextbuffer();
        void cleanup();

        float H(float freq); // Obtains the response for a given frequency

    private:
        struct fstage {
            float c1, c2;
        } x[MAX_FILTER_STAGES + 1],
          y[MAX_FILTER_STAGES + 1],
          oldx[MAX_FILTER_STAGES + 1],
          oldy[MAX_FILTER_STAGES + 1];

        void singlefilterout(float *smp, fstage &x, fstage &y, float *c,
                             float *d);
        void computefiltercoefs(void);
        int type;   // The type of the filter (LPF1,HPF1,LPF2,HPF2...)
        int stages; // how many times the filter is applied (0->1,1->2,etc.)
        float freq; // Frequency given in Hz
        float q;    // Q factor (resonance or Q factor)
        float gain; // the gain of the filter (if are shelf/peak) filters

        int order; // the order of the filter (number of poles)

        float c[3], d[3]; // coefficients

        float oldc[3], oldd[3]; // old coefficients(used only if some filter parameters changes very fast, and it needs interpolation)

        float xd[3], yd[3]; // used if the filter is applied more times
        bool needsinterpolation, firsttime;
        int abovenq;    // this is 1 if the frequency is above the nyquist
        int oldabovenq; // if the last time was above nyquist (used to see if it needs interpolation)

        float *tmpismp; // used if it needs interpolation in filterout()
        SynthEngine *synth;
};

#endif
