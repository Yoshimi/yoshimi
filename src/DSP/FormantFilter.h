/*
    FormantFilter.h - formant filter

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

#ifndef FORMANT_FILTER_H
#define FORMANT_FILTER_H

#include "globals.h"
#include "DSP/Filter_.h"
#include "DSP/AnalogFilter.h"
#include "Params/FilterParams.h"


class FormantFilter : public Filter_
{
    public:
        FormantFilter(FilterParams *pars);
        ~FormantFilter();
        void filterout(float *smp);
        void setfreq(float frequency);
        void setfreq_and_q(float frequency, float q_);
        void setq(float q_);

        void cleanup();
    private:
        void setpos(float input);


        AnalogFilter *formant[FF_MAX_FORMANTS];
        float *inbuffer, *tmpbuf;

        struct {
            float freq, amp, q; // frequency,amplitude,Q
        } formantpar[FF_MAX_VOWELS][FF_MAX_FORMANTS],
          currentformants[FF_MAX_FORMANTS];

        struct {
            unsigned char nvowel;
        } sequence [FF_MAX_SEQUENCE];

        float oldformantamp[FF_MAX_FORMANTS];

        int sequencesize, numformants, firsttime;
        float oldinput, slowinput;
        float Qfactor, formantslowness, oldQfactor;
        float vowelclearness, sequencestretch;
};


#endif

