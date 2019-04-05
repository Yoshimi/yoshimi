/*
    AnalogFilter.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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
#include "DSP/AnalogFilter.h"

AnalogFilter::AnalogFilter(unsigned char Ftype, float Ffreq,
                           float Fq, unsigned char Fstages) :
    type(Ftype),
    stages(Fstages),
    freq(Ffreq),
    q(Fq),
    gain(1.0),
    abovenq(0),
    oldabovenq(0),
    samplerate(zynMaster->getSamplerate()),
    buffersize(zynMaster->getBuffersize())
{

    for (int i = 0; i < 3; ++i)
        oldc[i] = oldd[i] = c[i] = d[i] = 0.0;
    if (stages >= MAX_FILTER_STAGES)
        stages = MAX_FILTER_STAGES;
    cleanup();
    firsttime = 0;
    setfreq_and_q(Ffreq, Fq);
    firsttime = 1;
    d[0] = 0; // this is not used
    outgain = 1.0;
}

AnalogFilter::~AnalogFilter() { }

void AnalogFilter::cleanup()
{
    for (int i = 0; i < MAX_FILTER_STAGES + 1; ++i)
    {
        x[i].c1 = x[i].c2 = y[i].c1 = y[i].c2 = 0.0;
        oldx[i] = x[i];
        oldy[i] = y[i];
    }
    needsinterpolation = 0;
}

void AnalogFilter::computefiltercoefs(void)
{
    float tmp;
    float omega, sn, cs, alpha, beta;
    int zerocoefs = 0; // this is used if the freq is too high

    // do not allow frequencies bigger than samplerate/2
    float freq = this->freq;
    if (freq > (samplerate / 2 - 500.0))
    {
        freq = samplerate / 2 - 500.0;
        zerocoefs = 1;
    }
    if (freq < 0.1)
        freq = 0.1;
    // do not allow bogus Q
    if (q < 0.0)
        q = 0.0;
    float tmpq;
    float tmpgain;
    if (stages == 0)
    {
        tmpq = q;
        tmpgain = gain;
    }
    else
    {
        // tmpq = (q > 1.0) ? pow(q, (float)1.0 / (stages + 1)) : q;
        // tmpgain = pow(gain, (float)1.0 / (stages + 1));
        tmpq = (q > 1.0) ? powf(q, 1.0 / (stages + 1)) : q;
        tmpgain = powf(gain, 1.0 / (stages + 1));
    }

    // most of theese are implementations of
    // the "Cookbook formulae for audio EQ" by Robert Bristow-Johnson
    // The original location of the Cookbook is:
    // http://www.harmony-central.com/Computer/Programming/Audio-EQ-Cookbook.txt
    switch (type)
    {
        case 0: // LPF 1 pole
            if (zerocoefs == 0)
                tmp = expf(-2.0 * PI * freq / samplerate);
            else
                tmp = 0.0;
            c[0] = 1.0 -tmp;
            c[1] = 0.0;
            c[2] = 0.0;
            d[1] = tmp;
            d[2] = 0.0;
            order = 1;
            break;
        case 1: // HPF 1 pole
            if (zerocoefs == 0)
                tmp = expf(-2.0 * PI * freq / samplerate);
            else
                tmp = 0.0;
            c[0] = (1.0 + tmp) / 2.0;
            c[1] = -(1.0 + tmp) / 2.0;
            c[2] = 0.0;
            d[1] = tmp;
            d[2] = 0.0;
            order = 1;
            break;
        case 2:// LPF 2 poles
            if (zerocoefs == 0)
            {
                omega = 2 * PI * freq / samplerate;
                sn = sinf(omega);
                cs = cosf(omega);
                alpha = sn / (2 * tmpq);
                tmp = 1 + alpha;
                c[0] = (1.0 - cs) / 2.0 / tmp;
                c[1] = (1.0 - cs) / tmp;
                c[2] = (1.0 - cs) / 2.0 / tmp;
                d[1] = -2 * cs / tmp * -1;
                d[2] = (1 - alpha) / tmp * -1;
            }
            else
            {
                c[0] = 1.0;
                c[1] = 0.0;
                c[2] = 0.0;
                d[1] = 0.0;
                d[2] = 0.0;
            }
            order = 2;
            break;
        case 3: // HPF 2 poles
            if (zerocoefs == 0)
            {
                omega = 2 * PI * freq / samplerate;
                sn = sinf(omega);
                cs = cosf(omega);
                alpha = sn / (2 * tmpq);
                tmp = 1 + alpha;
                c[0] = (1.0 + cs) / 2.0 / tmp;
                c[1] = -(1.0 + cs) / tmp;
                c[2] = (1.0 + cs) / 2.0 / tmp;
                d[1] = -2 * cs / tmp * -1;
                d[2] = (1 - alpha) / tmp * -1;
            }
            else
            {
                c[0] = 0.0;
                c[1] = 0.0;
                c[2] = 0.0;
                d[1] = 0.0;
                d[2] = 0.0;
            }
            order = 2;
            break;
        case 4: // BPF 2 poles
            if (zerocoefs == 0)
            {
                omega = 2 * PI * freq / samplerate;
                sn = sinf(omega);
                cs = cosf(omega);
                alpha = sn / (2 * tmpq);
                tmp = 1 + alpha;
                c[0] = alpha / tmp * sqrtf(tmpq + 1);
                c[1] = 0;
                c[2] = -alpha / tmp * sqrtf(tmpq + 1);
                d[1] = -2 * cs / tmp * -1;
                d[2] = (1 - alpha) / tmp * -1;
            }
            else
            {
                c[0] = 0.0;
                c[1] = 0.0;
                c[2] = 0.0;
                d[1] = 0.0;
                d[2] = 0.0;
            }
            order = 2;
            break;
        case 5: // NOTCH 2 poles
            if (zerocoefs == 0)
            {
                omega = 2 * PI * freq / samplerate;
                sn = sinf(omega);
                cs = cosf(omega);
                alpha = sn / (2 * sqrtf(tmpq));
                tmp = 1 + alpha;
                c[0] = 1 / tmp;
                c[1] = -2 * cs / tmp;
                c[2] = 1 / tmp;
                d[1] = -2 * cs / tmp * -1;
                d[2] = (1 - alpha) / tmp * -1;
            }
            else
            {
                c[0] = 1.0;
                c[1] = 0.0;
                c[2] = 0.0;
                d[1] = 0.0;
                d[2] = 0.0;
            }
            order = 2;
            break;
        case 6: // PEAK (2 poles)
            if (zerocoefs == 0)
            {
                omega = 2 * PI * freq / samplerate;
                sn = sinf(omega);
                cs = cosf(omega);
                tmpq *= 3.0;
                alpha = sn / (2 * tmpq);
                tmp = 1 + alpha / tmpgain;
                c[0] = (1.0 + alpha * tmpgain) / tmp;
                c[1] = (-2.0 * cs) / tmp;
                c[2] = (1.0 - alpha * tmpgain) / tmp;
                d[1] = -2 * cs / tmp * -1;
                d[2] = (1 - alpha / tmpgain) / tmp * -1;
            }
            else
            {
                c[0] = 1.0;
                c[1] = 0.0;
                c[2] = 0.0;
                d[1] = 0.0;
                d[2] = 0.0;
            }
            order = 2;
            break;
        case 7: // Low Shelf - 2 poles
            if (zerocoefs == 0)
            {
                omega = 2 * PI * freq / samplerate;
                sn = sinf(omega);
                cs = cosf(omega);
                tmpq = sqrtf(tmpq);
                alpha = sn / (2 * tmpq);
                beta = sqrtf(tmpgain) / tmpq;
                tmp = (tmpgain + 1.0) + (tmpgain - 1.0) * cs + beta * sn;

                c[0] = tmpgain * ((tmpgain + 1.0) - (tmpgain - 1.0) * cs + beta * sn) / tmp;
                c[1] = 2.0 * tmpgain * ((tmpgain - 1.0) - (tmpgain + 1.0) * cs) / tmp;
                c[2] = tmpgain * ((tmpgain + 1.0) - (tmpgain - 1.0) * cs - beta * sn) / tmp;
                d[1] = -2.0 * ((tmpgain - 1.0) + (tmpgain + 1.0) * cs) / tmp * -1;
                d[2] = ((tmpgain + 1.0) + (tmpgain - 1.0) * cs - beta * sn) / tmp * -1;
            }
            else
            {
                c[0] = tmpgain;
                c[1] = 0.0;
                c[2] = 0.0;
                d[1] = 0.0;
                d[2] = 0.0;
            }
            order = 2;
            break;
        case 8: // High Shelf - 2 poles
            if (zerocoefs == 0)
            {
                omega = 2 * PI * freq / samplerate;
                sn = sinf(omega);
                cs = cosf(omega);
                tmpq = sqrtf(tmpq);
                alpha = sn / (2 * tmpq);
                beta = sqrtf(tmpgain) / tmpq;
                tmp = (tmpgain + 1.0) - (tmpgain - 1.0) * cs + beta * sn;

                c[0]=tmpgain * ((tmpgain + 1.0) + (tmpgain - 1.0) * cs + beta * sn) / tmp;
                c[1] = -2.0 * tmpgain * ((tmpgain - 1.0) + (tmpgain + 1.0) * cs) / tmp;
                c[2] = tmpgain * ((tmpgain + 1.0) + (tmpgain - 1.0) * cs - beta * sn) / tmp;
                d[1] = 2.0 * ((tmpgain - 1.0) - (tmpgain + 1.0) * cs) / tmp * -1;
                d[2] = ((tmpgain + 1.0) - (tmpgain - 1.0) * cs - beta * sn) / tmp * -1;
            }
            else
            {
                c[0] = 1.0;
                c[1] = 0.0;
                c[2] = 0.0;
                d[1] = 0.0;
                d[2] = 0.0;
            }
            order = 2;
            break;
        default: // wrong type
            type = 0;
            computefiltercoefs();
            break;
    }
}


void AnalogFilter::setfreq(float frequency)
{
    if (frequency < 0.1)
        frequency = 0.1;
    float rap = freq / frequency;
    if (rap < 1.0)
        rap = 1.0 / rap;

    oldabovenq = abovenq;
    abovenq = frequency > (samplerate / 2 - 500.0);

    int nyquistthresh = (abovenq ^ oldabovenq);

    if (rap > 3.0 || nyquistthresh != 0)
    {   // if the frequency is changed fast, it needs interpolation (now, filter and coeficients backup)
        for (int i = 0; i < 3; ++i)
        {
            oldc[i] = c[i];
            oldd[i] = d[i];
        }
        for (int i = 0; i < MAX_FILTER_STAGES + 1; ++i)
        {
            oldx[i] = x[i];
            oldy[i] = y[i];
        }
        if (firsttime == 0)
            needsinterpolation = 1;
    }
    freq = frequency;
    computefiltercoefs();
    firsttime = 0;
}

void AnalogFilter::setfreq_and_q(float frequency, float q_)
{
    q = q_;
    setfreq(frequency);
}

void AnalogFilter::setq(float q_)
{
    q = q_;
    computefiltercoefs();
}

void AnalogFilter::settype(int type_)
{
    type = type_;
    computefiltercoefs();
}

void AnalogFilter::setgain(float dBgain)
{
    gain = dB2rap(dBgain);
    computefiltercoefs();
}

void AnalogFilter::setstages(int stages_)
{
    if (stages_ >= MAX_FILTER_STAGES)
        stages_ = MAX_FILTER_STAGES - 1;
    stages = stages_;
    cleanup();
    computefiltercoefs();
}

void AnalogFilter::singlefilterout(float *smp, fstage &x, fstage &y,
                                   float *c, float *d)
{
    //unsigned int u;
    float y0;
    if (order == 1)
    {   // First order filter
        for (int i = 0; i < buffersize; ++i) {
            y0 = smp[i] * c[0] + x.c1 * c[1] + y.c1 * d[1];
            y.c1 = y0;
            x.c1 = smp[i];
            // output
            smp[i] = y0;
        }
    }
    if (order==2) {// Second order filter
        for (int i = 0; i < buffersize; ++i) {
            y0 = smp[i] * c[0] + x.c1 * c[1] + x.c2 * c[2] + y.c1 * d[1] + y.c2 * d[2];
            y.c2 = y.c1;
            y.c1 = y0;
            x.c2 = x.c1;
            x.c1 = smp[i];
            //output
            smp[i] = y0;
        }
    }
}
void AnalogFilter::filterout(float *smp)
{
    float *ismp = NULL;      // used if it needs interpolation
    if (needsinterpolation != 0)
    {
        ismp = new float[buffersize];
        //for (int i = 0; i < buffersize; ++i)
        //    ismp[i] = smp[i];
        memcpy(ismp, smp, buffersize * sizeof(float));
        for (int i = 0; i < stages + 1; ++i)
            singlefilterout(ismp, oldx[i], oldy[i], oldc, oldd);
    }

    for (int i = 0; i < stages + 1; ++i)
        singlefilterout(smp, x[i], y[i], c, d);

    if (needsinterpolation != 0)
    {
        for (int i = 0; i < buffersize; ++i)
        {
            float x = (float)i / (float)buffersize;
            smp[i] = ismp[i] * (1.0 - x) + smp[i] * x;
        }
        delete [] ismp;
        needsinterpolation = 0;
    }

    for (int i = 0; i < buffersize; ++i)
        smp[i] *= outgain;
}

float AnalogFilter::H(float freq)
{
    float fr = freq / samplerate * PI * 2.0;
    float x = c[0], y = 0.0;
    for (int n = 1; n < 3; ++n)
    {
        x += cosf(n * fr) * c[n];
        y -= sinf(n * fr) * c[n];
    }
    float h = x * x + y * y;
    x = 1.0;
    y = 0.0;
    for (int n = 1; n < 3; ++n)
    {
        x -= cosf(n * fr) * d[n];
        y += sinf(n * fr) * d[n];
    }
    h = h / (x * x + y * y);
    return powf(h, (stages + 1.0) / 2.0);
}
