/*
    AnalogFilter.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2021, Will Godfrey

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
*/

#include <cstring>

#include "DSP/AnalogFilter.h"
#include "Misc/SynthEngine.h"
#include "Misc/NumericFuncs.h"

using func::decibel;


AnalogFilter::AnalogFilter(unsigned char Ftype, float Ffreq, float Fq, unsigned char Fstages, SynthEngine *_synth) :
    type(Ftype),
    stages(Fstages),
    freq(Ffreq),
    q(Fq),
    gain(1.0),
    abovenq(0),
    oldabovenq(0),
    tmpismp(_synth->buffersize),
    synth(_synth)
{

    for (int i = 0; i < 3; ++i)
        oldc[i] = oldd[i] = c[i] = d[i] = 0.0f;
    if (stages >= MAX_FILTER_STAGES)
        stages = MAX_FILTER_STAGES;
    cleanup();
    firsttime = false;
    setfreq_and_q(Ffreq, Fq);
    firsttime = true;
    d[0] = 0; // this is not used
    outgain = 1.0f;
}


AnalogFilter::AnalogFilter(const AnalogFilter &orig) :
    type(orig.type),
    stages(orig.stages),
    freq(orig.freq),
    q(orig.q),
    gain(orig.gain),
    order(orig.order),
    needsinterpolation(orig.needsinterpolation),
    firsttime(orig.firsttime),
    abovenq(orig.abovenq),
    oldabovenq(orig.oldabovenq),
    tmpismp(orig.synth->buffersize),   // No need to copy sample data, as this is filled from input data
    synth(orig.synth)
{
    outgain = orig.outgain;

    memcpy(x, orig.x, sizeof(x));
    memcpy(y, orig.y, sizeof(y));
    memcpy(oldx, orig.oldx, sizeof(oldx));
    memcpy(oldy, orig.oldy, sizeof(oldy));
    memcpy(c, orig.c, sizeof(c));
    memcpy(d, orig.d, sizeof(d));
    memcpy(oldc, orig.oldc, sizeof(oldc));
    memcpy(oldd, orig.oldd, sizeof(oldd));
    memcpy(xd, orig.xd, sizeof(xd));
    memcpy(yd, orig.yd, sizeof(yd));
}



void AnalogFilter::cleanup()
{
    for (int i = 0; i < MAX_FILTER_STAGES + 1; ++i)
    {
        x[i].c1 = x[i].c2 = y[i].c1 = y[i].c2 = 0.0f;
        oldx[i] = x[i];
        oldy[i] = y[i];
    }
    needsinterpolation = false;
}


void AnalogFilter::computefiltercoefs(void)
{
    double tmp;
    double omega, sn, cs, alpha, beta;
    bool zerocoefs = false; // this is used if the freq is too high

    // do not allow frequencies bigger than samplerate/2
    float freq = this->freq;
    if (freq > (synth->halfsamplerate_f - 500.0f))
    {
        freq = synth->halfsamplerate_f - 500.0f;
        zerocoefs = true;
    }
    if (freq < 0.1f)
        freq = 0.1;
    // do not allow bogus Q
    if (q < 0.0f)
        q = 0.0f;
    double tmpq;
    double tmpgain;
    if (stages == 0)
    {
        tmpq = q;
        tmpgain = gain;
    }
    else
    {
        tmpq = (q > 1.0f) ? pow(q, 1.0 / (stages + 1)) : q;
        tmpgain = pow(gain, 1.0 / (stages + 1));
    }

    // most of these are implementations of
    // the "Cookbook formulae for audio EQ" by Robert Bristow-Johnson
    // The original location of the Cookbook is:
    // http://www.harmony-central.com/Computer/Programming/Audio-EQ-Cookbook.txt

    // (Remark 2021: URL went offline in 2005; content can still be found on archive.org):
    // https://web.archive.org/web/20050404051659/http://www.harmony-central.com/Computer/Programming/Audio-EQ-Cookbook.txt

    // Note: filter coefficients are computed using doubles,
    //       to get reproducible numbers under optimisation on different compilers/platforms
    switch (type)
    {
        case TOPLEVEL::filter::Low1: // LPF 1 pole
            if (not zerocoefs)
                tmp = exp(-TWOPI * freq / synth->samplerate_f);
            else
                tmp = 0.0;
            c[0] = 1.0 - tmp;
            c[1] = 0.0f;
            c[2] = 0.0f;
            d[1] = tmp;
            d[2] = 0.0f;
            order = 1;
            break;

        case TOPLEVEL::filter::High1: // HPF 1 pole
            if (not zerocoefs)
                tmp = exp(-TWOPI * freq / synth->samplerate_f);
            else
                tmp = 0.0f;
            c[0] = (1.0 + tmp) / 2.0;
            c[1] = -(1.0 + tmp) / 2.0;
            c[2] = 0.0f;
            d[1] = tmp;
            d[2] = 0.0f;
            order = 1;
            break;

        case TOPLEVEL::filter::Low2:// LPF 2 poles
            if (not zerocoefs)
            {
                omega = TWOPI * freq / synth->samplerate_f;
                sn = sin(omega);
                cs = cos(omega);
                alpha = sn / (2.0 * tmpq);
                tmp = 1 + alpha;
                c[1] = (1.0 - cs) / tmp;
                c[0] = c[2] = c[1] / 2.0;
                d[1] = -2.0 * cs / tmp * -1.0;
                d[2] = (1.0 - alpha) / tmp * -1.0;
            }
            else
            {
                c[0] = 1.0f;
                c[1] = c[2] = d[1] = d[2] = 0.0f;
            }
            order = 2;
            break;

        case TOPLEVEL::filter::High2: // HPF 2 poles
            if (not zerocoefs)
            {
                omega = TWOPI * freq / synth->samplerate_f;
                sn = sin(omega);
                cs = cos(omega);
                alpha = sn / (2.0 * tmpq);
                tmp = 1 + alpha;
                c[0] = (1.0 + cs) / 2.0 / tmp;
                c[1] = -(1.0 + cs) / tmp;
                c[2] = (1.0 + cs) / 2.0 / tmp;
                d[1] = -2.0 * cs / tmp * -1.0;
                d[2] = (1.0 - alpha) / tmp * -1.0;
            }
            else
                c[0] = c[1] = c[2] = d[1] = d[2] = 0.0f;
            order = 2;
            break;

        case TOPLEVEL::filter::Band2: // BPF 2 poles
            if (not zerocoefs)
            {
                omega = TWOPI * freq / synth->samplerate_f;
                sn = sin(omega);
                cs = cos(omega);
                alpha = sn / (2.0 * tmpq);
                tmp = 1.0 + alpha;
                c[0] = alpha / tmp * sqrt(tmpq + 1.0);
                c[1] = 0.0f;
                c[2] = -alpha / tmp * sqrt(tmpq + 1.0);
                d[1] = -2.0 * cs / tmp * -1.0;
                d[2] = (1.0 - alpha) / tmp * -1.0;
            }
            else
                c[0] = c[1] = c[2] = d[1] = d[2] = 0.0f;
            order = 2;
            break;

        case TOPLEVEL::filter::Notch2: // NOTCH 2 poles
            if (not zerocoefs)
            {
                omega = TWOPI * freq / synth->samplerate_f;
                sn = sin(omega);
                cs = cos(omega);
                alpha = sn / (2.0 * sqrt(tmpq));
                tmp = 1.0 + alpha;
                c[0] = 1.0 / tmp;
                c[1] = -2.0 * cs / tmp;
                c[2] = 1.0 / tmp;
                d[1] = -2.0 * cs / tmp * -1.0;
                d[2] = (1.0 - alpha) / tmp * -1.0;
            }
            else
            {
                c[0] = 1.0f;
                c[1] = c[2] = d[1] = d[2] = 0.0f;
            }
            order = 2;
            break;

        case TOPLEVEL::filter::Peak2: // PEAK (2 poles)
            if (not zerocoefs)
            {
                omega = TWOPI * freq / synth->samplerate_f;
                sn = sin(omega);
                cs = cos(omega);
                tmpq *= 3.0;
                alpha = sn / (2.0 * tmpq);
                tmp = 1.0 + alpha / tmpgain;
                c[0] = (1.0 + alpha * tmpgain) / tmp;
                c[1] = (-2.0 * cs) / tmp;
                c[2] = (1.0 - alpha * tmpgain) / tmp;
                d[1] = -2.0 * cs / tmp * -1.0;
                d[2] = (1.0 - alpha / tmpgain) / tmp * -1.0;
            }
            else
            {
                c[0] = 1.0f;
                c[1] = c[2] = d[1] = d[2] = 0.0f;
            }
            order = 2;
            break;

        case TOPLEVEL::filter::LowShelf2: // Low Shelf - 2 poles
            if (not zerocoefs)
            {
                omega = TWOPI * freq / synth->samplerate_f;
                sn = sin(omega);
                cs = cos(omega);
                tmpq = sqrt(tmpq);
                alpha = sn / (2.0 * tmpq);
                beta = sqrt(tmpgain) / tmpq;
                tmp = (tmpgain + 1.0) + (tmpgain - 1.0) * cs + beta * sn;

                c[0] = tmpgain * ((tmpgain + 1.0) - (tmpgain - 1.0) * cs + beta * sn) / tmp;
                c[1] = 2.0 * tmpgain * ((tmpgain - 1.0) - (tmpgain + 1.0) * cs) / tmp;
                c[2] = tmpgain * ((tmpgain + 1.0) - (tmpgain - 1.0) * cs - beta * sn) / tmp;
                d[1] = -2.0 * ((tmpgain - 1.0) + (tmpgain + 1.0) * cs) / tmp * -1.0;
                d[2] = ((tmpgain + 1.0) + (tmpgain - 1.0) * cs - beta * sn) / tmp * -1.0;
            }
            else
            {
                c[0] = tmpgain;
                c[1] = c[2] = d[1] = d[2] = 0.0f;
            }
            order = 2;
            break;

        case TOPLEVEL::filter::HighShelf2: // High Shelf - 2 poles
            if (not zerocoefs)
            {
                omega = TWOPI * freq / synth->samplerate_f;
                sn = sin(omega);
                cs = cos(omega);
                tmpq = sqrt(tmpq);
                alpha = sn / (2.0 * tmpq);
                beta = sqrt(tmpgain) / tmpq;
                tmp = (tmpgain + 1.0) - (tmpgain - 1.0) * cs + beta * sn;

                c[0] = tmpgain * ((tmpgain + 1.0) + (tmpgain - 1.0) * cs + beta * sn) / tmp;
                c[1] = -2.0 * tmpgain * ((tmpgain - 1.0) + (tmpgain + 1.0) * cs) / tmp;
                c[2] = tmpgain * ((tmpgain + 1.0) + (tmpgain - 1.0) * cs - beta * sn) / tmp;
                d[1] = 2.0 * ((tmpgain - 1.0) - (tmpgain + 1.0) * cs) / tmp * -1.0;
                d[2] = ((tmpgain + 1.0) - (tmpgain - 1.0) * cs - beta * sn) / tmp * -1.0;
            }
            else
            {
                c[0] = 1.0f;
                c[1] = c[2] = d[1] = d[2] = 0.0f;
            }
            order = 2;
            break;

        default: // wrong type so give it the simplest
            type = TOPLEVEL::filter::Low1;
            computefiltercoefs();
            break;
    }
}


float AnalogFilter::getFreq()
{
    return this->freq;
}

void AnalogFilter::setfreq(float frequency)
{
    if (frequency < 0.1f)
        frequency = 0.1f;
    float rap = freq / frequency;
    if (rap < 1.0f)
        rap = 1.0f / rap;

    oldabovenq = abovenq;
    abovenq = frequency > (synth->halfsamplerate_f - 500.0f);

    int nyquistthresh = (abovenq ^ oldabovenq);

    if (!firsttime && (rap > 3.0f || nyquistthresh != 0))
    {   // if the frequency is changed fast, it needs interpolation
        // (now, filter and coefficients backup)
        interpolatenextbuffer();
    }
    freq = frequency;
    computefiltercoefs();
    firsttime = false;
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
    gain = decibel(dBgain);
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

void AnalogFilter::interpolatenextbuffer()
{
    if (needsinterpolation)
        // Don't repeat the coefficient saving, because if interpolation was
        // requested due to multiple parameters, some values may already have
        // changed. The oldest coefficient are the correct ones, basically.
        return;

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
    needsinterpolation = true;
}


void AnalogFilter::singlefilterout(float *smp, fstage &x, fstage &y, float *c, float *d)
{
    float y0;
    if (order == 1)
    {   // First order filter
        for (int i = 0; i < synth->sent_buffersize; ++i)
        { // anti-denormal added in here
            y0 = (smp[i] + float(1e-20)) * c[0] + x.c1 * c[1] + y.c1 * d[1];
            y.c1 = y0;
            x.c1 = smp[i];
            smp[i] = y0; // out it goes
        }
    }
    if (order == 2)
    { // Second order filter
        for (int i = 0; i < synth->sent_buffersize; ++i)
        { // anti-denormal added in here
            y0 = (smp[i] + float(1e-20)) * c[0] + x.c1 * c[1] + x.c2 * c[2] + y.c1 * d[1] + y.c2 * d[2];
            y.c2 = y.c1;
            y.c1 = y0;
            x.c2 = x.c1;
            x.c1 = smp[i];
            smp[i] = y0; // out it goes
        }
    }
}


void AnalogFilter::filterout(float *smp)
{
     if (needsinterpolation)
    {
        memcpy(tmpismp.get(), smp, synth->sent_bufferbytes);
        for (int i = 0; i < stages + 1; ++i)
            singlefilterout(tmpismp.get(), oldx[i], oldy[i], oldc, oldd);
    }

    for (int i = 0; i < stages + 1; ++i)
        singlefilterout(smp, x[i], y[i], c, d);

    if (needsinterpolation)
    {
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float x = (float)i / synth->sent_buffersize_f;
            smp[i] = tmpismp[i] * (1.0f - x) + smp[i] * x;
        }
        needsinterpolation = false;
    }

    for (int i = 0; i < synth->sent_buffersize; ++i)
        smp[i] *= outgain;
}


float AnalogFilter::H(float freq)
{
    float fr = freq / synth->samplerate_f * PI * 2.0f;
    float x = c[0], y = 0.0f;
    for (int n = 1; n < 3; ++n)
    {
        x += cosf(n * fr) * c[n];
        y -= sinf(n * fr) * c[n];
    }
    float h = x * x + y * y;
    x = 1.0f;
    y = 0.0f;
    for (int n = 1; n < 3; ++n)
    {
        x -= cosf(n * fr) * d[n];
        y += sinf(n * fr) * d[n];
    }
    h = h / (x * x + y * y);
    return powf(h, (stages + 1.0f) / 2.0f);
}
