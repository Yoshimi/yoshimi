/*
    FormantFilter.cpp - formant filters

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009, James Morris
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

#include "DSP/FFTwrapper.h"
#include "DSP/FormantFilter.h"
#include "Misc/SynthEngine.h"
#include "Misc/SynthHelper.h"
#include "Misc/NumericFuncs.h"

using synth::aboveAmplitudeThreshold;
using synth::interpolateAmplitude;
using func::dB2rap;


FormantFilter::FormantFilter(FilterParams *pars_, SynthEngine *_synth):
    pars(pars_),
    parsUpdate(pars_),
    synth(_synth)
{
    numformants = pars->Pnumformants;
    for (int i = 0; i < numformants; ++i)
        formant[i] = new AnalogFilter(4/*BPF*/, 1000.0f, 10.0f, pars->Pstages, synth);
    cleanup();
    inbuffer = (float*)fftwf_malloc(synth->bufferbytes);
    tmpbuf = (float*)fftwf_malloc(synth->bufferbytes);

    for (int i = 0; i < FF_MAX_FORMANTS; ++i)
        oldformantamp[i] = 1.0f;
    for (int i = 0; i < numformants; ++i)
    {
        currentformants[i].freq = 1000.0f;
        currentformants[i].amp = 1.0f;
        currentformants[i].q = 2.0f;
    }

    sequencesize = pars->Psequencesize;
    if (sequencesize == 0)
        sequencesize = 1;
    for (int k = 0; k < sequencesize; ++k)
        sequence[k].nvowel = pars->Psequence[k].nvowel;

    oldinput = -1.0f;
    oldQfactor = Qfactor;
    firsttime = 1;

    // Prevent parameter update happening next time.
    parsUpdate.checkUpdated();
    // And do it now.
    updateCurrentParameters();
}


FormantFilter::FormantFilter(const FormantFilter &orig) :
    pars(orig.pars),
    parsUpdate(orig.parsUpdate),
    sequencesize(orig.sequencesize),
    numformants(orig.numformants),
    firsttime(orig.firsttime),
    oldinput(orig.oldinput),
    slowinput(orig.slowinput),
    Qfactor(orig.Qfactor),
    formantslowness(orig.formantslowness),
    oldQfactor(orig.oldQfactor),
    vowelclearness(orig.vowelclearness),
    sequencestretch(orig.sequencestretch),
    synth(orig.synth)
{
    outgain = orig.outgain;

    memcpy(formantpar, orig.formantpar, sizeof(formantpar));
    memcpy(currentformants, orig.currentformants, sizeof(currentformants));
    memcpy(sequence, orig.sequence, sizeof(sequence));
    memcpy(oldformantamp, orig.oldformantamp, sizeof(oldformantamp));

    for (int i = 0; i < numformants; ++i)
        formant[i] = new AnalogFilter(*orig.formant[i]);

    // These don't hold persistent state and don't need a memcpy
    inbuffer = (float*)fftwf_malloc(synth->bufferbytes);
    tmpbuf = (float*)fftwf_malloc(synth->bufferbytes);
}


FormantFilter::~FormantFilter()
{
    for (int i = 0; i < numformants; ++i)
        delete(formant[i]);
    fftwf_free(inbuffer);
    fftwf_free(tmpbuf);
}


void FormantFilter::cleanup()
{
    for (int i = 0; i < numformants; ++i)
        formant[i]->cleanup();
}


void FormantFilter::setpos(float input)
{
    int p1, p2;
    bool needsUpdate = parsUpdate.checkUpdated();

    if (needsUpdate)
        updateCurrentParameters();

    if (firsttime)
        slowinput = input;
    else
        slowinput = slowinput * (1.0f - formantslowness) + input * formantslowness;

    if (!needsUpdate && (fabsf(oldinput-input) < 0.001f) &&
            (fabsf(slowinput - input) < 0.001f) && (fabsf(Qfactor - oldQfactor) < 0.001f))
    {
        //	oldinput=input; daca setez asta, o sa faca probleme la schimbari foarte lente
        firsttime = 0;
        return;
    } else
        oldinput = input;

    float pos = input * sequencestretch;
    pos -= floorf(pos);

    p2 = (int)(pos * sequencesize);
    p1 = p2 - 1;
    if (p1 < 0)
        p1 += sequencesize;

    pos = pos * sequencesize;
    pos -= floorf(pos);

    pos = (atanf((pos * 2.0f - 1.0f) * vowelclearness) / atanf(vowelclearness) + 1.0f) * 0.5f;

    p1 = sequence[p1].nvowel;
    p2 = sequence[p2].nvowel;

    if (firsttime)
    {
        for (int i = 0; i < numformants; ++i)
        {
            currentformants[i].freq =
                formantpar[p1][i].freq * (1.0f - pos) + formantpar[p2][i].freq * pos;
            currentformants[i].amp =
                formantpar[p1][i].amp * (1.0f - pos) + formantpar[p2][i].amp * pos;
            currentformants[i].q =
                formantpar[p1][i].q * (1.0f - pos) + formantpar[p2][i].q * pos;
            formant[i]->setfreq_and_q(currentformants[i].freq,
                                      currentformants[i].q * Qfactor);
            oldformantamp[i] = currentformants[i].amp;
        }
        firsttime = 0;
    } else {
        for (int i = 0; i < numformants; ++i)
        {
            currentformants[i].freq =
                currentformants[i].freq * (1.0f - formantslowness)
                + (formantpar[p1][i].freq
                    * (1.0f - pos) + formantpar[p2][i].freq * pos)
                * formantslowness;

            currentformants[i].amp =
                currentformants[i].amp * (1.0f - formantslowness)
                + (formantpar[p1][i].amp * (1.0f - pos)
                   + formantpar[p2][i].amp * pos) * formantslowness;

            currentformants[i].q =
                currentformants[i].q * (1.0f - formantslowness)
                    + (formantpar[p1][i].q * (1.0f - pos)
                        + formantpar[p2][i].q * pos) * formantslowness;

            formant[i]->setfreq_and_q(currentformants[i].freq,
                                      currentformants[i].q * Qfactor);
        }
    }
    oldQfactor = Qfactor;
}

void FormantFilter::updateCurrentParameters()
{
    for (int j = 0; j < FF_MAX_VOWELS; ++j)
        for (int i = 0; i < numformants; ++i)
        {
            formantpar[j][i].freq = pars->getformantfreq(pars->Pvowels[j].formants[i].freq);
            formantpar[j][i].amp = pars->getformantamp(pars->Pvowels[j].formants[i].amp);
            formantpar[j][i].q = pars->getformantq(pars->Pvowels[j].formants[i].q);
        }

    formantslowness = powf(1.0f - (pars->Pformantslowness / 128.0f), 3.0f);

    vowelclearness = powf(10.0f, (pars->Pvowelclearness - 32.0f) / 48.0f);

    sequencestretch = powf(0.1f, (pars->Psequencestretch - 32.0f) / 48.0f);
    if (pars->Psequencereversed)
        sequencestretch *= -1.0f;

    outgain = dB2rap(pars->getgain());

    Qfactor = pars->getq();
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
    memcpy(inbuffer, smp, synth->sent_bufferbytes);
    memset(smp, 0, synth->sent_bufferbytes);

    for (int j = 0; j < numformants; ++j)
    {
        for (int k = 0; k < synth->sent_buffersize; ++k)
            tmpbuf[k] = inbuffer[k] * outgain;
        formant[j]->filterout(tmpbuf);

        if (aboveAmplitudeThreshold(oldformantamp[j], currentformants[j].amp))
            for (int i = 0; i < synth->sent_buffersize; ++i)
                smp[i] += tmpbuf[i]
                          * interpolateAmplitude(oldformantamp[j],
                                                  currentformants[j].amp, i,
                                                  synth->sent_buffersize);
        else
            for (int i = 0; i < synth->sent_buffersize; ++i)
                smp[i] += tmpbuf[i] * currentformants[j].amp;
        oldformantamp[j] = currentformants[j].amp;
    }
}
