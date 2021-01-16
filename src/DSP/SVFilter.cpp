/*
    SVFilter.cpp - Several state-variable filters

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2020-2021 Kristian Amlie, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code
*/

#include <cstring>

#include "DSP/FFTwrapper.h"
#include "Misc/SynthEngine.h"
#include "DSP/SVFilter.h"
#include "Misc/NumericFuncs.h"

using func::dB2rap;


SVFilter::SVFilter(unsigned char Ftype, float Ffreq, float Fq,
                   unsigned char Fstages, SynthEngine *_synth) :
    type(Ftype),
    stages(Fstages),
    freq(Ffreq),
    q(Fq),
    needsinterpolation(0),
    firsttime(1),
    synth(_synth)
{
    if (stages >= MAX_FILTER_STAGES)
        stages = MAX_FILTER_STAGES;
    outgain = 1.0f;
    tmpismp = (float*)fftwf_malloc(synth->bufferbytes);
    cleanup();
    setfreq_and_q(Ffreq, Fq);
}


SVFilter::SVFilter(const SVFilter &orig) :
    par(orig.par),
    ipar(orig.ipar),
    type(orig.type),
    stages(orig.stages),
    freq(orig.freq),
    q(orig.q),
    abovenq(orig.abovenq),
    oldabovenq(orig.oldabovenq),
    needsinterpolation(orig.needsinterpolation),
    firsttime(orig.firsttime),
    synth(orig.synth)
{
    outgain = orig.outgain;

    memcpy(st, orig.st, sizeof(st));

    tmpismp = (float*)fftwf_malloc(synth->bufferbytes);
}


SVFilter::~SVFilter()
{
    if (tmpismp)
        fftwf_free(tmpismp);
}


void SVFilter::cleanup()
{
    for (int i = 0; i < MAX_FILTER_STAGES + 1; ++i)
        st[i].low = st[i].high = st[i].band = st[i].notch = 0.0f;
    oldabovenq = 0;
    abovenq = 0;
}


void SVFilter::computefiltercoefs(void)
{
    par.f = freq / synth->samplerate_f * 4.0f;
    if (par.f > 0.99999f)
        par.f = 0.99999f;
    par.q = 1.0f - atanf(sqrtf(q)) * 2.0f / PI;
    par.q = powf(par.q, 1.0f / (stages + 1));
    par.q_sqrt = sqrtf(par.q);
}


void SVFilter::setfreq(float frequency)
{
    if (frequency < 0.1f)
        frequency = 0.1f;
    float rap = freq / frequency;
    if (rap < 1.0f)
        rap = 1.0f / rap;

    oldabovenq = abovenq;
    abovenq = frequency > (synth->halfsamplerate_f - 500.0f);

    int nyquistthresh = (abovenq ^ oldabovenq);


    if (rap > 3.0f || nyquistthresh != 0)
    {   //if the frequency is changed fast, it needs interpolation (now, filter and coeficients backup)
        if (firsttime == 0)
            needsinterpolation = 1;
        ipar = par;
    }
    freq = frequency;
    computefiltercoefs();
    firsttime = 0;
}


void SVFilter::setfreq_and_q(float frequency, float q_)
{
    q = q_;
    setfreq(frequency);
}


void SVFilter::setq(float q_)
{
    q = q_;
    computefiltercoefs();
}


void SVFilter::settype(int type_)
{
    type = type_;
    computefiltercoefs();
}


void SVFilter::setstages(int stages_)
{
    if (stages_ >= MAX_FILTER_STAGES)
        stages_ = MAX_FILTER_STAGES - 1;
    stages = stages_;
    cleanup();
    computefiltercoefs();
}


void SVFilter::singlefilterout(float *smp, fstage &x, parameters &par)
{
    float *out = NULL;
    switch (type)
    {
        case 0:
            out = &x.low;
            break;
        case 1:
            out = &x.high;
            break;
        case 2:
            out = &x.band;
            break;
        case 3:
            out = &x.notch;
            break;
    }

    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
        x.low = x.low + par.f * x.band;
        x.high = par.q_sqrt * smp[i] - x.low - par.q * x.band;
        x.band = par.f * x.high + x.band;
        x.notch = x.high + x.low;
        smp[i] = (*out);
    }
}


void SVFilter::filterout(float *smp)
{
    if (needsinterpolation)
    {
        memcpy(tmpismp, smp, synth->sent_bufferbytes);
        for (int i = 0; i < stages + 1; ++i)
            singlefilterout(tmpismp, st[i],ipar);
    }

    for (int i = 0; i < stages + 1; ++i)
        singlefilterout(smp, st[i],par);

    if (needsinterpolation)
    {
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float x = (float)i / synth->sent_buffersize_f;
            smp[i] = tmpismp[i] * (1.0f - x) + smp[i] * x;
        }
        needsinterpolation = 0;
    }
    for (int i = 0; i < synth->sent_buffersize; ++i)
        smp[i] *= outgain;
}
