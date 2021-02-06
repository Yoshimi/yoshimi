/*
    PADnote.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011 Alan Calvert
    Copyright 2017-2019 Will Godfrey & others
    Copyright 2020 Kristian Amlie

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
#include <cmath>

#include "Misc/Config.h"
#include "Params/PADnoteParameters.h"
#include "Params/Controller.h"
#include "Synth/Envelope.h"
#include "Synth/LFO.h"
#include "DSP/Filter.h"
#include "Params/Controller.h"
#include "Misc/SynthEngine.h"
#include "Misc/SynthHelper.h"
#include "Synth/PADnote.h"
#include "Misc/NumericFuncs.h"

using synth::velF;
using synth::getDetune;
using synth::interpolateAmplitude;
using synth::aboveAmplitudeThreshold;
using func::setRandomPan;


PADnote::PADnote(PADnoteParameters *parameters, Controller *ctl_, float freq,
    float velocity, int portamento_, int midinote_, SynthEngine *_synth) :
    NoteStatus(NOTE_ENABLED),
    pars(parameters),
    firsttime(true),
    released(false),
    nsample(0),
    portamento(portamento_),
    midinote(midinote_),
    ctl(ctl_),
    legatoFade(1.0f), // Full volume
    legatoFadeStep(0.0f), // Legato disabled
    padSynthUpdate(parameters),
    synth(_synth)

{
    this->velocity = velocity;

    setBaseFreq(freq);

    realfreq = basefreq;

    setRandomPan(synth->numRandom(), randpanL, randpanR, synth->getRuntime().panLaw, pars->PPanning, pars->PWidth);

    NoteGlobalPar.Fadein_adjustment =
            pars->Fadein_adjustment / (float)FADEIN_ADJUSTMENT_SCALE;
    NoteGlobalPar.Fadein_adjustment *= NoteGlobalPar.Fadein_adjustment;
    if (pars->PPunchStrength != 0)
    {
        NoteGlobalPar.Punch.Enabled = 1;
        NoteGlobalPar.Punch.t = 1.0f; // start from 1.0 and to 0.0
        NoteGlobalPar.Punch.initialvalue =
            (powf(10.0f, 1.5f * pars->PPunchStrength / 127.0f) - 1.0f)
                    * velF(velocity, pars->PPunchVelocitySensing);
        float time = powf(10.0f, 3.0f * pars->PPunchTime / 127.0f) / 10000.0f; // 0.1 .. 100 ms
        float stretch = powf(440.0f / freq, pars->PPunchStretch / 64.0f);
        NoteGlobalPar.Punch.dt = 1.0f / (time * synth->samplerate_f * stretch);
    }
    else
        NoteGlobalPar.Punch.Enabled = 0;

    NoteGlobalPar.FreqEnvelope = new Envelope(pars->FreqEnvelope, basefreq, synth);
    NoteGlobalPar.FreqLfo = new LFO(pars->FreqLfo, basefreq, synth);

    NoteGlobalPar.AmpEnvelope = new Envelope(pars->AmpEnvelope, basefreq, synth);
    NoteGlobalPar.AmpLfo = new LFO(pars->AmpLfo, basefreq, synth);

    NoteGlobalPar.AmpEnvelope->envout_dB(); // discard the first envelope output

    NoteGlobalPar.GlobalFilterL =
        new Filter(pars->GlobalFilter, synth);
    NoteGlobalPar.GlobalFilterR =
        new Filter(pars->GlobalFilter, synth);

    NoteGlobalPar.FilterEnvelope = new Envelope(pars->FilterEnvelope, basefreq, synth);
    NoteGlobalPar.FilterLfo = new LFO(pars->FilterLfo, basefreq, synth);

    computeNoteParameters();

    globaloldamplitude =
        globalnewamplitude = NoteGlobalPar.Volume
        * NoteGlobalPar.AmpEnvelope->envout_dB()
        * NoteGlobalPar.AmpLfo->amplfoout();

    int size = pars->sample[nsample].size;
    if (size == 0)
        size = 1;

    poshi_l = int(synth->numRandom() * (size - 1));
    if (pars->PStereo != 0)
        poshi_r = (poshi_l + size / 2) % size;
    else
        poshi_r = poshi_l;
    poslo = 0.0f;

    if (parameters->sample[nsample].smp == NULL)
    {
        NoteStatus = NOTE_DISABLED;
        return;
    }
}


// Copy constructor, currently only used for legato
PADnote::PADnote(const PADnote &orig) :
    // For legato. Move this somewhere else if copying
    // notes gets used for another purpose
    NoteStatus(NOTE_KEEPALIVE),
    pars(orig.pars),
    poshi_l(orig.poshi_l),
    poshi_r(orig.poshi_r),
    poslo(orig.poslo),
    basefreq(orig.basefreq),
    BendAdjust(orig.BendAdjust),
    OffsetHz(orig.OffsetHz),
    firsttime(orig.firsttime),
    released(orig.released),
    nsample(orig.nsample),
    portamento(orig.portamento),
    ctl(orig.ctl),
    globaloldamplitude(orig.globaloldamplitude),
    globalnewamplitude(orig.globalnewamplitude),
    velocity(orig.velocity),
    realfreq(orig.realfreq),
    randpanL(orig.randpanL),
    randpanR(orig.randpanR),
    legatoFade(0.0f), // Silent by default
    legatoFadeStep(0.0f), // Legato disabled
    padSynthUpdate(pars),
    synth(orig.synth)
{
    auto &gpar = NoteGlobalPar;
    auto &oldgpar = orig.NoteGlobalPar;

    gpar.Detune = oldgpar.Detune;
    gpar.Volume = oldgpar.Volume;
    gpar.Panning = oldgpar.Panning;

    gpar.Fadein_adjustment = oldgpar.Fadein_adjustment;
    gpar.Punch = oldgpar.Punch;

    // These are never null
    gpar.FreqEnvelope = new Envelope(*oldgpar.FreqEnvelope);
    gpar.FreqLfo = new LFO(*oldgpar.FreqLfo);
    gpar.AmpEnvelope = new Envelope(*oldgpar.AmpEnvelope);
    gpar.AmpLfo = new LFO(*oldgpar.AmpLfo);

    gpar.GlobalFilterL = new Filter(*oldgpar.GlobalFilterL);
    gpar.GlobalFilterR = new Filter(*oldgpar.GlobalFilterR);

    gpar.FilterEnvelope = new Envelope(*oldgpar.FilterEnvelope);
    gpar.FilterLfo = new LFO(*oldgpar.FilterLfo);
}


void PADnote::legatoFadeIn(float freq_, float velocity_, int portamento_, int midinote_)
{
    if (pars->sample[nsample].smp == NULL)
    {
        NoteStatus = NOTE_DISABLED;
        return;
    }

    velocity = velocity_;
    portamento = portamento_;
    midinote = midinote_;

    setBaseFreq(freq_);

    globalnewamplitude = NoteGlobalPar.Volume
        * NoteGlobalPar.AmpEnvelope->envout_dB()
        * NoteGlobalPar.AmpLfo->amplfoout();
    globaloldamplitude = globalnewamplitude;

    if (!portamento) // Do not crossfade portamento
    {
        legatoFade = 0.0f; // Start silent
        legatoFadeStep = synth->fadeStepShort; // Positive steps

        computeNoteParameters();
    }
}


void PADnote::legatoFadeOut(const PADnote &orig)
{
    velocity = orig.velocity;
    portamento = orig.portamento;
    midinote = orig.midinote;

    poshi_l = orig.poshi_l;
    poshi_r = orig.poshi_r;
    poslo = orig.poslo;
    basefreq = orig.basefreq;
    BendAdjust = orig.BendAdjust;
    OffsetHz = orig.OffsetHz;
    firsttime = orig.firsttime;
    released = orig.released;
    nsample = orig.nsample;
    portamento = orig.portamento;
    globaloldamplitude = orig.globaloldamplitude;
    globalnewamplitude = orig.globalnewamplitude;
    realfreq = orig.realfreq;
    randpanL = orig.randpanL;
    randpanR = orig.randpanR;

    auto &gpar = NoteGlobalPar;
    auto &oldgpar = orig.NoteGlobalPar;

    gpar.Detune = oldgpar.Detune;
    gpar.Volume = oldgpar.Volume;
    gpar.Panning = oldgpar.Panning;

    gpar.Fadein_adjustment = oldgpar.Fadein_adjustment;
    gpar.Punch = oldgpar.Punch;

    *gpar.FreqEnvelope = *oldgpar.FreqEnvelope;
    *gpar.FreqLfo = *oldgpar.FreqLfo;
    *gpar.AmpEnvelope = *oldgpar.AmpEnvelope;
    *gpar.AmpLfo = *oldgpar.AmpLfo;

    *gpar.FilterEnvelope = *oldgpar.FilterEnvelope;
    *gpar.FilterLfo = *oldgpar.FilterLfo;

    // Supporting virtual copy assignment would be hairy
    // so we have to use the copy constructor here
    delete gpar.GlobalFilterL;
    gpar.GlobalFilterL = new Filter(*oldgpar.GlobalFilterL);
    delete gpar.GlobalFilterR;
    gpar.GlobalFilterR = new Filter(*oldgpar.GlobalFilterR);

    legatoFade = 1.0f; // Start at full volume
    legatoFadeStep = -synth->fadeStepShort; // Negative steps
}


PADnote::~PADnote()
{
    delete NoteGlobalPar.FreqEnvelope;
    delete NoteGlobalPar.FreqLfo;
    delete NoteGlobalPar.AmpEnvelope;
    delete NoteGlobalPar.AmpLfo;
    delete NoteGlobalPar.GlobalFilterL;
    delete NoteGlobalPar.GlobalFilterR;
    delete NoteGlobalPar.FilterEnvelope;
    delete NoteGlobalPar.FilterLfo;
}


void PADnote::setBaseFreq(float basefreq_)
{
    if (pars->Pfixedfreq == 0)
        basefreq = basefreq_;
    else
    {
        basefreq = 440.0f;
        int fixedfreqET = pars->PfixedfreqET;
        if (fixedfreqET != 0)
        {   // if the frequency varies according the keyboard note
            float tmp = (midinote - 69.0f) / 12.0f
                              * (powf(2.0f, (fixedfreqET - 1) / 63.0f) - 1.0f);
            if (fixedfreqET <= 64)
                basefreq *= powf(2.0f, tmp);
            else
                basefreq *= powf(3.0f, tmp);
        }
    }
}

inline void PADnote::fadein(float *smps)
{
    int zerocrossings = 0;
    for (int i = 1; i < synth->sent_buffersize; ++i)
        if (smps[i - 1] < 0.0 && smps[i] > 0.0)
            zerocrossings++; // this is only the positive crossings

    float tmp = (synth->sent_buffersize_f - 1.0) / (zerocrossings + 1) / 3.0;
    if (tmp < 8.0)
        tmp = 8.0;
    tmp *= NoteGlobalPar.Fadein_adjustment;

    int n = int(tmp); // how many samples is the fade-in
    if (n > synth->sent_buffersize)
        n = synth->sent_buffersize;
    for (int i = 0; i < n; ++i)
    {   // fade-in
        float tmp = 0.5 - cosf((float)i / (float) n * PI) * 0.5f;
        smps[i] *= tmp;
    }
}


void PADnote::computeNoteParameters()
{
    setBaseFreq(basefreq);

    int BendAdj = pars->PBendAdjust - 64;
    if (BendAdj % 24 == 0)
        BendAdjust = BendAdj / 24;
    else
        BendAdjust = BendAdj / 24.0f;
    float offset_val = (pars->POffsetHz - 64)/64.0f;
    OffsetHz = 15.0f*(offset_val * sqrtf(fabsf(offset_val)));

    NoteGlobalPar.Detune = getDetune(pars->PDetuneType, pars->PCoarseDetune, pars->PDetune);

    // find out the closest note
    float logfreq = logf(basefreq * powf(2.0f, NoteGlobalPar.Detune / 1200.0f));
    float mindist = fabsf(logfreq - logf(pars->sample[0].basefreq + 0.0001f));
    nsample = 0;
    for (int i = 1; i < PAD_MAX_SAMPLES; ++i)
    {
        if (pars->sample[i].smp == NULL)
            break;
        float dist = fabsf(logfreq - logf(pars->sample[i].basefreq + 0.0001f));
//	printf("(mindist=%g) %i %g                  %g\n",mindist,i,dist,pars->sample[i].basefreq);

        if (dist < mindist)
        {
            nsample = i;
            mindist = dist;
        }
    }

    NoteGlobalPar.Volume =
        4.0f * powf(0.1f, 3.0f * (1.0f - pars->PVolume / 96.0f)) //-60 dB .. 0 dB
        * velF(velocity, pars->PAmpVelocityScaleFunction); // velocity sensing
}


void PADnote::computecurrentparameters()
{
    float globalpitch,globalfilterpitch;
    globalpitch =
        0.01 * (NoteGlobalPar.FreqEnvelope->envout()
        + NoteGlobalPar.FreqLfo->lfoout() * ctl->modwheel.relmod + NoteGlobalPar.Detune);
    globaloldamplitude = globalnewamplitude;
    globalnewamplitude =
        NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB()
        * NoteGlobalPar.AmpLfo->amplfoout();

    float filterCenterPitch =
        pars->GlobalFilter->getfreq() + // center freq
            pars->PFilterVelocityScale / 127.0 * 6.0
            * (velF(velocity, pars->PFilterVelocityScaleFunction) - 1); // velocity sensing

    float filterQ = pars->GlobalFilter->getq();
    float filterFreqTracking = pars->GlobalFilter->getfreqtracking(basefreq);

    globalfilterpitch =
        NoteGlobalPar.FilterEnvelope->envout() + NoteGlobalPar.FilterLfo->lfoout()
        + filterCenterPitch;

    float tmpfilterfreq =
        globalfilterpitch+ctl->filtercutoff.relfreq + filterFreqTracking;

    tmpfilterfreq =
        NoteGlobalPar.GlobalFilterL->getrealfreq(tmpfilterfreq);

    float globalfilterq = filterQ * ctl->filterq.relq;
    NoteGlobalPar.GlobalFilterL->setfreq_and_q(tmpfilterfreq,globalfilterq);
    NoteGlobalPar.GlobalFilterR->setfreq_and_q(tmpfilterfreq,globalfilterq);

    // compute the portamento, if it is used by this note
    float portamentofreqrap = 1.0;
    if (portamento != 0)
    {   // this voice use portamento
        portamentofreqrap = ctl->portamento.freqrap;
        if (ctl->portamento.used == 0)
        {   // the portamento has finished
            portamento = 0; // this note is no longer "portamented"
        }
    }

    realfreq = basefreq * portamentofreqrap * powf(2.0f, globalpitch / 12.0)
               * powf(ctl->pitchwheel.relfreq, BendAdjust) + OffsetHz;
}


int PADnote::Compute_Linear(float *outl, float *outr, int freqhi, float freqlo)
{
    float *smps = pars->sample[nsample].smp;
    if (smps == NULL)
    {
        NoteStatus = NOTE_DISABLED;
        return 1;
    }
    int size = pars->sample[nsample].size;
    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
        poshi_l += freqhi;
        poshi_r += freqhi;
        poslo += freqlo;
        if (poslo >= 1.0)
        {
            poshi_l += 1;
            poshi_r += 1;
            poslo -= 1.0;
        }
        if (poshi_l >= size)
            poshi_l %= size;
        if (poshi_r >= size)
            poshi_r %= size;

        outl[i] = smps[poshi_l] * (1.0 - poslo) + smps[poshi_l + 1] * poslo;
        outr[i] = smps[poshi_r] * (1.0 - poslo) + smps[poshi_r + 1] * poslo;
    }
    return 1;
}

int PADnote::Compute_Cubic(float *outl, float *outr, int freqhi, float freqlo)
{
    float *smps = pars->sample[nsample].smp;
    if (smps == NULL)
    {
        NoteStatus = NOTE_DISABLED;
        return 1;
    }
    int size = pars->sample[nsample].size;
    float xm1, x0, x1, x2, a, b, c;
    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
        poshi_l += freqhi;
        poshi_r += freqhi;
        poslo += freqlo;
        if (poslo >= 1.0)
        {
            poshi_l += 1;
            poshi_r += 1;
            poslo -= 1.0;
        }
        if (poshi_l >= size)
            poshi_l %= size;
        if (poshi_r >= size)
            poshi_r %= size;

        // left
        xm1 = smps[poshi_l];
        x0 = smps[poshi_l + 1];
        x1 = smps[poshi_l + 2];
        x2 = smps[poshi_l + 3];
        a = (3.0 * (x0 - x1) - xm1 + x2) * 0.5;
        b = 2.0 * x1 + xm1 - (5.0 * x0 + x2) * 0.5;
        c = (x1 - xm1) * 0.5;
        outl[i] = (((a * poslo) + b) * poslo + c) * poslo + x0;
        // right
        xm1 = smps[poshi_r];
        x0 = smps[poshi_r + 1];
        x1 = smps[poshi_r + 2];
        x2 = smps[poshi_r + 3];
        a = (3.0 * (x0 - x1) - xm1 + x2) * 0.5;
        b = 2.0 * x1 + xm1 - (5.0 * x0 + x2) * 0.5;
        c = (x1 - xm1) * 0.5;
        outr[i] = (((a * poslo) + b) * poslo + c) * poslo + x0;
    }
    return 1;
}


int PADnote::noteout(float *outl,float *outr)
{
    if (padSynthUpdate.checkUpdated())
        computeNoteParameters();

    computecurrentparameters();
    float *smps = pars->sample[nsample].smp;
    if (smps == NULL)
    {
        memset(outl, 0, synth->sent_buffersize * sizeof(float));
        memset(outr, 0, synth->sent_buffersize * sizeof(float));
        return 1;
    }
    float smpfreq = pars->sample[nsample].basefreq;

    float freqrap = realfreq / smpfreq;
    int freqhi = (int) (floorf(freqrap));
    float freqlo = freqrap - floorf(freqrap);

    if (synth->getRuntime().Interpolation)
        Compute_Cubic(outl, outr, freqhi, freqlo);
    else
        Compute_Linear(outl, outr, freqhi, freqlo);

    if (firsttime)
    {
        fadein(outl);
        fadein(outr);
        firsttime = false;
    }

    NoteGlobalPar.GlobalFilterL->filterout(outl);
    NoteGlobalPar.GlobalFilterR->filterout(outr);

    // Apply the punch
    if (NoteGlobalPar.Punch.Enabled != 0)
    {
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float punchamp =
                NoteGlobalPar.Punch.initialvalue * NoteGlobalPar.Punch.t + 1.0;
            outl[i] *= punchamp;
            outr[i] *= punchamp;
            NoteGlobalPar.Punch.t -= NoteGlobalPar.Punch.dt;
            if (NoteGlobalPar.Punch.t < 0.0)
            {
                NoteGlobalPar.Punch.Enabled = 0;
                break;
            }
        }
    }

    float pangainL = pars->pangainL; // assume non random pan
    float pangainR = pars->pangainR;
    if (pars->PRandom)
    {
        pangainL = randpanL;
        pangainR = randpanR;
    }

    if (aboveAmplitudeThreshold(globaloldamplitude,globalnewamplitude))
    {
        // Amplitude Interpolation
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float tmpvol = interpolateAmplitude(globaloldamplitude,
                                                globalnewamplitude, i,
                                                synth->sent_buffersize);
            outl[i] *= tmpvol * pangainL;
            outr[i] *= tmpvol * pangainR;
        }
    }
    else
    {
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            outl[i] *= globalnewamplitude * pangainL;
            outr[i] *= globalnewamplitude * pangainR;
        }
    }

    // Apply legato fading if any
    if (legatoFadeStep != 0.0f)
    {
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            legatoFade += legatoFadeStep;
            if (legatoFade <= 0.0f)
            {
                legatoFade = 0.0f;
                legatoFadeStep = 0.0f;
                memset(outl + i, 0, (synth->sent_buffersize - i) * sizeof(float));
                memset(outr + i, 0, (synth->sent_buffersize - i) * sizeof(float));
                break;
            }
            else if (legatoFade >= 1.0f)
            {
                legatoFade = 1.0f;
                legatoFadeStep = 0.0f;
                break;
            }
            outl[i] *= legatoFade;
            outr[i] *= legatoFade;
        }
    }

    // Check if the global amplitude is finished.
    // If it does, disable the note
    if (NoteGlobalPar.AmpEnvelope->finished() != 0)
    {
        for (int i = 0 ; i < synth->sent_buffersize; ++i)
        {   // fade-out
            float tmp = 1.0f - (float)i / synth->sent_buffersize_f;
            outl[i] *= tmp;
            outr[i] *= tmp;
        }
        NoteStatus = NOTE_DISABLED;
    }
    return 1;
}


void PADnote::releasekey()
{
    NoteGlobalPar.FreqEnvelope->releasekey();
    NoteGlobalPar.FilterEnvelope->releasekey();
    NoteGlobalPar.AmpEnvelope->releasekey();
    if (NoteStatus == NOTE_KEEPALIVE)
        NoteStatus = NOTE_ENABLED;
}
