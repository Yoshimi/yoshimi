/*
    Unison.cpp - Unison effect (multivoice chorus)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018-2019, Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original

    Modified January 2019
*/

#include <cmath>
#include <cstring>
#include <stdio.h>

#include "Misc/Config.h"
#include "Misc/NumericFuncs.h"
#include "DSP/Unison.h"

using func::power;


Unison::Unison(int update_period_samples_, float max_delay_sec_, SynthEngine* _synth)
    : unison_size{0}
    , base_freq{1.0f}
    , max_delay{std::max(10, int(_synth->samplerate_f * max_delay_sec_) + 1)}
    , delay_k{0}
    , first_time{false}
    , voice{}
    , delay_buffer{new float[max_delay]{0}}  // zero-init
    , update_period_samples{update_period_samples_}
    , update_period_sample_k{0}
    , unison_amplitude_samples{0.0f}
    , unison_bandwidth_cents{10.0f}
    , synth{_synth}
{
    setSize(1);
}



void Unison::setSize(int new_size)
{
    if (new_size < 1)
        new_size = 1;
    unison_size = new_size;
    voice.reset(new UnisonVoice[unison_size]);
    for (int i = 0; i < unison_size; ++i)
    {
        voice [i].setPosition(synth->numRandom() * 1.8f - 0.9f);
    }
    first_time = true;
    updateParameters();
}


void Unison::setBaseFrequency(float freq)
{
    base_freq = freq;
    updateParameters();
}


void Unison::setBandwidth(float bandwidth)
{
    if (bandwidth < 0)
        bandwidth = 0.0f;
    if (bandwidth > 1200.0f)
        bandwidth = 1200.0f;
    //#warning
    //    : todo: if bandwidth is too small the audio will be self cancelled (because of the sign change of the outputs)
    unison_bandwidth_cents = bandwidth;
    updateParameters();
}


void Unison::updateParameters()
{
    if (!voice)
        return;
    float increments_per_second = synth->samplerate_f / float(update_period_samples);
    for (int i = 0; i < unison_size; ++i)
    {
        float base = powf(UNISON_FREQ_SPAN, synth->numRandom() * 2.0f - 1.0f);
        voice[i].relative_amplitude = base;
        float period = base / base_freq;
        float m = 4.0f / (period * increments_per_second);
        if (synth->numRandom() < 0.5f)
            m = -m;
        voice[i].step = m;
    }

    float max_speed = power<2>(unison_bandwidth_cents / 1200.0f);
    unison_amplitude_samples = 0.125f * (max_speed - 1.0f) * synth->samplerate_f / base_freq;

    //#warning
    //    todo: test if unison_amplitude_samples is to big and reallocate bigger memory
    if (unison_amplitude_samples >= max_delay - 1)
        unison_amplitude_samples = max_delay - 2;
    updateUnisonData();
}


void Unison::process(int bufsize, float* inbuf, float* outbuf)
{
    if (!voice)
        return;
    if (!outbuf)
        outbuf = inbuf;

    float volume = 1.0f / sqrtf(unison_size);
    float xpos_step = 1.0f / update_period_samples;
    float xpos = float(update_period_sample_k) * xpos_step;
    for (int i = 0; i < bufsize; ++i)
    {
        if (update_period_sample_k++ >= update_period_samples)
        {
            updateUnisonData();
            update_period_sample_k = 0;
            xpos = 0.0f;
        }
        xpos += xpos_step;
        float in = inbuf[i];
        float out = 0.0f;
        float sign = 1.0f;
        for (int k = 0; k < unison_size; ++k)
        {
            float vpos = voice[k].realpos1 * (1.0f - xpos) + voice[k].realpos2 * xpos;
            float pos  = float(delay_k + max_delay) - vpos - 1.0f;
            int posi = int(pos);
            int posi_next = posi + 1;
            if (posi >= max_delay)
                posi -= max_delay;
            if (posi_next >= max_delay)
                posi_next -= max_delay;
            float posf = pos - floorf(pos);
            out += ((1.0f - posf) * delay_buffer[posi] + posf * delay_buffer[posi_next]) * sign;
            sign = -sign;
        }
        outbuf[i] = out * volume;
        delay_buffer[delay_k] = in;
        delay_k = (++delay_k < max_delay) ? delay_k : 0;
    }
}


void Unison::updateUnisonData()
{
    if (!voice)
        return;

    float newval;
    float pos;
    float step;
    float vibratoFactor;
    for (int k = 0; k < unison_size; ++k)
    {
        pos  = voice[k].position;
        step = voice[k].step;
        pos += step;
        if (pos <= -1.0f)
        {
            pos  = -1.0f;
            step = -step;
        }
        else if (pos >= 1.0f)
        {
            pos  = 1.0f;
            step = -step;
        }
        vibratoFactor = (pos - 1/3.0f * pos*pos*pos) * 1.5f; //make the vibrato LFO smoother

        // #warning
        // I will use relative amplitude, so the delay might be bigger than the whole buffer
        // #warning
        // I have to enlarge (reallocate) the buffer to make place for the whole delay

        newval = 1.0f + 0.5f * (vibratoFactor + 1.0f) * unison_amplitude_samples * voice[k].relative_amplitude;

        if (first_time)
            voice[k].realpos1 = voice[k].realpos2 = newval;
        else
        {
            voice[k].realpos1 = voice[k].realpos2;
            voice[k].realpos2 = newval;
        }
        voice[k].position = pos;
        voice[k].step     = step;
    }
    first_time = false;
}
