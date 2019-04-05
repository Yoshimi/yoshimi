/*
    SVFilter.cpp - Several state-variable filters

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
#include "DSP/SVFilter.h"

SVFilter::SVFilter(unsigned char Ftype, float Ffreq, float Fq,
                   unsigned char Fstages) :
    type(Ftype),
    stages(Fstages),
    freq(Ffreq),
    q(Fq),
    gain(1.0),
    needsinterpolation(0),
    firsttime(1)
{
    if (stages >= MAX_FILTER_STAGES)
        stages = MAX_FILTER_STAGES;
    outgain = 1.0;
    cleanup();
    setfreq_and_q(Ffreq, Fq);
}

void SVFilter::cleanup()
{
    for (int i = 0; i < MAX_FILTER_STAGES + 1; ++i)
        st[i].low = st[i].high = st[i].band = st[i].notch = 0.0;
    oldabovenq = 0;
    abovenq = 0;
}

void SVFilter::computefiltercoefs(void)
{
    par.f = freq / zynMaster->getSamplerate() * 4.0;
    if (par.f > 0.99999)
        par.f = 0.99999;
    par.q = 1.0 - atanf(sqrtf(q)) * 2.0 / PI;
    par.q = powf(par.q, 1.0 / (stages + 1));
    par.q_sqrt = sqrtf(par.q);
}


void SVFilter::setfreq(float frequency)
{
    if (frequency < 0.1)
        frequency = 0.1;
    float rap = freq / frequency;
    if (rap < 1.0)
        rap = 1.0 / rap;

    oldabovenq = abovenq;
    abovenq = frequency > (zynMaster->getSamplerate() / 2 - 500.0);

    int nyquistthresh = (abovenq ^ oldabovenq);


    if (rap > 3.0 || nyquistthresh != 0)
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

void SVFilter::setgain(float dBgain)
{
    gain = dB2rap(dBgain);
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

    int buffersize = zynMaster->getBuffersize();
    for (int i = 0; i < buffersize; ++i)
    {
        x.low = x.low + par.f * x.band;
        x.high = par.q_sqrt * smp[i] - x.low - par.q * x.band;
        x.band = par.f * x.high + x.band;
        x.notch = x.high + x.low;
        smp[i] = *out;
    }
}

void SVFilter::filterout(float *smp)
{
    float *ismp = NULL;
    int buffersize = zynMaster->getBuffersize();

    if (needsinterpolation != 0)
    {
        ismp = new float[buffersize];

        //for (int i = 0; i < buffersize; ++i)
        //    ismp[i] = smp[i];
        memcpy(ismp, smp, buffersize * sizeof(float));

        for (int i = 0; i < stages + 1; ++i)
            singlefilterout(ismp, st[i],ipar);
    }

    for (int i = 0; i < stages + 1; ++i)
        singlefilterout(smp, st[i],par);

    if (needsinterpolation != 0)
    {
        for (int i = 0; i < buffersize; ++i)
        {
            float x = (float)i / (float)buffersize;
            smp[i] = ismp[i] * (1.0 - x) + smp[i] * x;
        }
        needsinterpolation = 0;
    }
    if (NULL != ismp)
        delete [] ismp;

    for (int i = 0; i < buffersize; ++i)
        smp[i] *= outgain;
}
