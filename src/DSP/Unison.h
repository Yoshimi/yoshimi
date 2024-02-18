/*
    ZynAddSubFX - a software synthesizer

    Unison.h - Unison effect (multivoice chorus)
    Original author: Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License (version 2 or later) for more details.

    You should have received a copy of the GNU General Public License (version 2)
    along with this program; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

    This file is a derivative of a ZynAddSubFX original, modified January 2011
*/

#ifndef UNISON_H
#define UNISON_H

#include "Misc/SynthEngine.h"

#include <memory>

#define UNISON_FREQ_SPAN 2.0f // how much the unison frequencies varies (always >= 1.0)

class SynthEngine;

class Unison
{
    public:
        Unison(int update_period_samples_, float max_delay_sec_, SynthEngine *_synth);
       ~Unison() = default;

        void setSize(int new_size);
        void setBaseFrequency(float freq);
        void setBandwidth(float bandwidth_cents);

        void process(int bufsize, float *inbuf, float *outbuf = NULL);

    private:
        void updateParameters(void);
        void updateUnisonData(void);

        struct UnisonVoice {
            float step;     // base LFO
            float position;
            float realpos1; // the position regarding samples
            float realpos2;
            float relative_amplitude;
            UnisonVoice()
                : step{0.0f}
                , position{}
                , realpos1{}
                , realpos2{}
                , relative_amplitude{1.0f}
            { }

            void setPosition(float newPos)
            {
                position = newPos;
            }
        };
        int   unison_size;
        float base_freq;
        int   max_delay, delay_k;
        bool  first_time;

        std::unique_ptr<UnisonVoice[]> voice;
        std::unique_ptr<float[]> delay_buffer;

        int           update_period_samples;
        int           update_period_sample_k;
        float         unison_amplitude_samples;
        float         unison_bandwidth_cents;

        SynthEngine *synth;
};

#endif

