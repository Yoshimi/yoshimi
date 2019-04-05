/*
    FormantFilter.cpp - formant filters

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009, James Morris
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
#include "DSP/FormantFilter.h"

FormantFilter::FormantFilter(FilterParams *pars)
{
    numformants = pars->Pnumformants;
    for (int i = 0; i < numformants; ++i)
        formant[i] = new AnalogFilter(4/*BPF*/, 1000.0, 10.0, pars->Pstages);
    cleanup();
    inbuffer = new float [zynMaster->getBuffersize()];
    tmpbuf = new float [zynMaster->getBuffersize()];

    for (int j = 0; j < FF_MAX_VOWELS; ++j)
        for (int i = 0; i < numformants; ++i)
        {
            formantpar[j][i].freq = pars->getformantfreq(pars->Pvowels[j].formants[i].freq);
            formantpar[j][i].amp = pars->getformantamp(pars->Pvowels[j].formants[i].amp);
            formantpar[j][i].q = pars->getformantq(pars->Pvowels[j].formants[i].q);
        }
    for (int i = 0; i < FF_MAX_FORMANTS; ++i)
        oldformantamp[i] = 1.0;
    for (int i = 0; i < numformants; ++i)
    {
        currentformants[i].freq = 1000.0;
        currentformants[i].amp = 1.0;
        currentformants[i].q = 2.0;
    }

    formantslowness = powf(1.0 - (pars->Pformantslowness / 128.0), 3.0);

    sequencesize = pars->Psequencesize;
    if (sequencesize == 0)
        sequencesize = 1;
    for (int k = 0; k < sequencesize; ++k)
        sequence[k].nvowel = pars->Psequence[k].nvowel;

    vowelclearness = powf(10.0, (pars->Pvowelclearness - 32.0) / 48.0);

    sequencestretch = powf(0.1, (pars->Psequencestretch - 32.0) / 48.0);
    if (pars->Psequencereversed)
        sequencestretch *= -1.0;

    outgain = dB2rap(pars->getgain());

    oldinput = -1.0;
    Qfactor = 1.0;
    oldQfactor = Qfactor;
    firsttime = 1;
}

FormantFilter::~FormantFilter()
{
    for (int i = 0; i < numformants; ++i)
        delete(formant[i]);
    delete[] inbuffer;
    delete[] tmpbuf;
}


void FormantFilter::cleanup()
{
    for (int i = 0; i < numformants; ++i)
        formant[i]->cleanup();
}

void FormantFilter::setpos(float input)
{
    int p1, p2;

    if (firsttime != 0)
        slowinput = input;
    else
        slowinput = slowinput * (1.0 - formantslowness) + input * formantslowness;

    if ((fabsf(oldinput-input) < 0.001) && (fabsf(slowinput - input) < 0.001) &&
            (fabsf(Qfactor - oldQfactor) < 0.001))
    {
//	oldinput=input; daca setez asta, o sa faca probleme la schimbari foarte lente
        firsttime = 0;
        return;
    } else
        oldinput = input;

    float pos = fmodf(input * sequencestretch, (float)1.0);
    if (pos < 0.0)
        pos += 1.0;

    F2I(pos * sequencesize, p2);
    p1=p2-1;
    if (p1<0) p1+=sequencesize;

    pos = fmodf(pos*sequencesize, (float)1.0);
    if (pos < 0.0)
        pos = 0.0;
    else if (pos > 1.0)
        pos = 1.0;
    pos = (atanf((pos * 2.0 - 1.0) * vowelclearness) / atanf(vowelclearness) + 1.0) * 0.5;

    p1 = sequence[p1].nvowel;
    p2 = sequence[p2].nvowel;

    if (firsttime != 0)
    {
        for (int i = 0; i < numformants; ++i)
        {
            currentformants[i].freq =
                formantpar[p1][i].freq * (1.0 - pos) + formantpar[p2][i].freq * pos;
            currentformants[i].amp =
                formantpar[p1][i].amp * (1.0 - pos) + formantpar[p2][i].amp * pos;
            currentformants[i].q =
                formantpar[p1][i].q * (1.0 - pos) + formantpar[p2][i].q * pos;
            formant[i]->setfreq_and_q(currentformants[i].freq,
                                      currentformants[i].q * Qfactor);
            oldformantamp[i] = currentformants[i].amp;
        }
        firsttime = 0;
    } else {
        for (int i = 0; i < numformants; ++i)
        {
            currentformants[i].freq =
                currentformants[i].freq * (1.0 - formantslowness)
                + (formantpar[p1][i].freq
                    * (1.0 - pos) + formantpar[p2][i].freq * pos)
                * formantslowness;

            currentformants[i].amp =
                currentformants[i].amp * (1.0 - formantslowness)
                + (formantpar[p1][i].amp * (1.0 - pos)
                   + formantpar[p2][i].amp * pos) * formantslowness;

            currentformants[i].q =
                currentformants[i].q * (1.0 - formantslowness)
                    + (formantpar[p1][i].q * (1.0 - pos)
                        + formantpar[p2][i].q * pos) * formantslowness;

            formant[i]->setfreq_and_q(currentformants[i].freq,
                                      currentformants[i].q * Qfactor);
        }
    }
    oldQfactor = Qfactor;
}

void FormantFilter::setfreq(float frequency)
{
    setpos(frequency);
}

void FormantFilter::setq(float q_)
{
    Qfactor = q_;
    for (int i = 0; i <numformants; ++i)
        formant[i]->setq(Qfactor * currentformants[i].q);
}

void FormantFilter::setfreq_and_q(float frequency, float q_)
{
    Qfactor = q_;
    setpos(frequency);
}

void FormantFilter::filterout(float *smp)
{
    int buffersize = zynMaster->getBuffersize();
    memcpy(inbuffer, smp, buffersize * sizeof(float));
    memset(smp, 0, buffersize * sizeof(float));

    for (int j = 0; j < numformants; ++j)
    {
        for (int k = 0; k < buffersize; ++k)
            tmpbuf[k] = inbuffer[k] * outgain;
        formant[j]->filterout(tmpbuf);

        if (AboveAmplitudeThreshold(oldformantamp[j], currentformants[j].amp))
            for (int i = 0; i < buffersize; ++i)
                smp[i] += tmpbuf[i]
                          * InterpolateAmplitude(oldformantamp[j],
                                                  currentformants[j].amp, i,
                                                  buffersize);
        else
            for (int i = 0; i < buffersize; ++i)
                smp[i] += tmpbuf[i] * currentformants[j].amp;
        oldformantamp[j] = currentformants[j].amp;
    }
}
