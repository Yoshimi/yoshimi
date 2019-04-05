/*
    PADnote.cpp - The "pad" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010 Alan Calvert

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

    This file is a derivative of the ZynAddSubFX original, modified January 2010
*/
#include <cmath>

using namespace std;

#include "Misc/Config.h"
#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Synth/PADnote.h"

PADnote::PADnote(PADnoteParameters *parameters, Controller *ctl_, float freq,
    float velocity, int portamento_, int midinote, bool besilent) :
    ready(false),
    finished_(false),
    pars(parameters),
    firsttime(true),
    released(false),
    nsample(0),
    portamento(portamento_),
    ctl(ctl_)

{
    samplerate = pars->getSamplerate();
    buffersize = pars->getBuffersize();
    oscilsize = pars->getOscilsize();

    // Initialise some legato-specific vars
    Legato.msg = LM_Norm;
    Legato.fade.length = (int)(samplerate * 0.005); // 0.005 seems ok.
    if (Legato.fade.length < 1)
        Legato.fade.length = 1; // (if something's fishy)
    Legato.fade.step = (1.0 / Legato.fade.length);
    Legato.decounter = -10;
    Legato.param.freq = freq;
    Legato.param.vel = velocity;
    Legato.param.portamento = portamento_;
    Legato.param.midinote = midinote;
    Legato.silent = besilent;

    this->velocity = velocity;

    if (pars->Pfixedfreq == 0)
        basefreq = freq;
    else
    {
        basefreq = 440.0;
        int fixedfreqET = pars->PfixedfreqET;
        if (fixedfreqET != 0)
        {   // if the frequency varies according the keyboard note
            float tmp = (midinote - 69.0) / 12.0
                              * (powf(2.0f, (fixedfreqET - 1) / 63.0) - 1.0);
            if (fixedfreqET <= 64)
                basefreq *= powf(2.0f, tmp);
            else
                basefreq *= powf(3.0f, tmp);
        }
    }

    realfreq = basefreq;
    NoteGlobalPar.Detune = getdetune(pars->PDetuneType, pars->PCoarseDetune,
                                     pars->PDetune);

    // find out the closest note
    float logfreq = logf(basefreq * powf(2.0f, NoteGlobalPar.Detune / 1200.0));
    float mindist = fabsf(logfreq - logf(pars->sample[0].basefreq + 0.0001f));
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

    int size = pars->sample[nsample].size;
    if (size == 0)
        size = 1;

    poshi_l = (int)(zynMaster->numRandom() * (size - 1));
    if (pars->PStereo != 0)
        poshi_r = (poshi_l + size / 2) % size;
    else
        poshi_r = poshi_l;
    poslo = 0.0;

    tmpwave = new float [buffersize];

    if (pars->PPanning == 0)
        NoteGlobalPar.Panning = zynMaster->numRandom();
    else
        NoteGlobalPar.Panning = pars->PPanning / 128.0;

    NoteGlobalPar.FilterCenterPitch =
        pars->GlobalFilter->getfreq() + // center freq
            pars->PFilterVelocityScale / 127.0 * 6.0
            * (VelF(velocity, pars->PFilterVelocityScaleFunction) - 1); // velocity sensing

    if (pars->PPunchStrength != 0)
    {
        NoteGlobalPar.Punch.Enabled = 1;
        NoteGlobalPar.Punch.t = 1.0; // start from 1.0 and to 0.0
        NoteGlobalPar.Punch.initialvalue =
            (powf(10.0f, 1.5 * pars->PPunchStrength / 127.0) - 1.0)
                    * VelF(velocity, pars->PPunchVelocitySensing);
        float time = powf(10.0f, 3.0 * pars->PPunchTime / 127.0) / 10000.0; // 0.1 .. 100 ms
        float stretch = powf(440.0f / freq, pars->PPunchStretch / 64.0);
        NoteGlobalPar.Punch.dt = 1.0 / (time * samplerate * stretch);
    } else
        NoteGlobalPar.Punch.Enabled = 0;

    NoteGlobalPar.FreqEnvelope = new Envelope(pars->FreqEnvelope, basefreq);
    NoteGlobalPar.FreqLfo = new LFO(pars->FreqLfo, basefreq);

    NoteGlobalPar.AmpEnvelope = new Envelope(pars->AmpEnvelope, basefreq);
    NoteGlobalPar.AmpLfo = new LFO(pars->AmpLfo, basefreq);

    NoteGlobalPar.Volume =
        4.0 * powf(0.1f, 3.0 * (1.0 - pars->PVolume / 96.0)) //-60 dB .. 0 dB
        * VelF(velocity, pars->PAmpVelocityScaleFunction); // velocity sensing

    NoteGlobalPar.AmpEnvelope->envout_dB(); // discard the first envelope output
    globaloldamplitude =
        globalnewamplitude = NoteGlobalPar.Volume
        * NoteGlobalPar.AmpEnvelope->envout_dB()
        * NoteGlobalPar.AmpLfo->amplfoout();

    NoteGlobalPar.GlobalFilterL =
        new Filter(pars->GlobalFilter);
    NoteGlobalPar.GlobalFilterR =
        new Filter(pars->GlobalFilter);

    NoteGlobalPar.FilterEnvelope = new Envelope(pars->FilterEnvelope, basefreq);
    NoteGlobalPar.FilterLfo = new LFO(pars->FilterLfo, basefreq);
    NoteGlobalPar.FilterQ = pars->GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking=pars->GlobalFilter->getfreqtracking(basefreq);

    ready = true; ///sa il pun pe asta doar cand e chiar gata

    if (parameters->sample[nsample].smp == NULL)
    {
        finished_ = true;
        return;
    }
}


// PADlegatonote: This function is (mostly) a copy of PADnote(...)
// with some lines removed so that it only alter the already playing
// note (to perform legato). It is possible I left stuff that is not
// required for this.
void PADnote::PADlegatonote(float freq, float velocity,
                            int portamento_, int midinote, bool externcall)
{
    PADnoteParameters *parameters = pars;
    // Controller *ctl_=ctl; (an original comment)

    // Manage legato stuff
    if (externcall)
        Legato.msg = LM_Norm;
    if (Legato.msg != LM_CatchUp)
    {
        Legato.lastfreq = Legato.param.freq;
        Legato.param.freq = freq;
        Legato.param.vel = velocity;
        Legato.param.portamento = portamento_;
        Legato.param.midinote = midinote;
        if (Legato.msg == LM_Norm)
        {
            if (Legato.silent)
            {
                Legato.fade.m = 0.0;
                Legato.msg = LM_FadeIn;
            } else {
                Legato.fade.m = 1.0;
                Legato.msg = LM_FadeOut;
                return;
            }
        }
        if (Legato.msg == LM_ToNorm)
            Legato.msg = LM_Norm;
    }

    portamento = portamento_;
    this->velocity = velocity;
    finished_ = false;

    if (pars->Pfixedfreq == 0)
        basefreq = freq;
    else {
        basefreq = 440.0;
        int fixedfreqET = pars->PfixedfreqET;
        if (fixedfreqET != 0)
        {   // if the frequency varies according the keyboard note
            float tmp = (midinote - 69.0) / 12.0
                               * (powf(2.0f, (fixedfreqET - 1) / 63.0) - 1.0);
            if (fixedfreqET <= 64)
                basefreq *= powf(2.0f, tmp);
            else
                basefreq *= powf(3.0f, tmp);
        }
    }

    released = false;
    realfreq = basefreq;

    getdetune(pars->PDetuneType, pars->PCoarseDetune, pars->PDetune);

    // find out the closest note
    float logfreq = logf(basefreq * powf(2.0f, NoteGlobalPar.Detune / 1200.0));
    float mindist = fabsf(logfreq - logf(pars->sample[0].basefreq + 0.0001));
    nsample = 0;
    for (int i = 1; i < PAD_MAX_SAMPLES; ++i)
    {
        if (pars->sample[i].smp == NULL)
            break;
        float dist = fabsf(logfreq - logf(pars->sample[i].basefreq + 0.0001));

        if (dist < mindist)
        {
            nsample = i;
            mindist = dist;
        }
    }

    int size = pars->sample[nsample].size;
    if (size == 0)
        size = 1;

    if (pars->PPanning == 0)
        NoteGlobalPar.Panning = zynMaster->numRandom();
    else
        NoteGlobalPar.Panning = pars->PPanning / 128.0;

    NoteGlobalPar.FilterCenterPitch =
        pars->GlobalFilter->getfreq() // center freq
        + pars->PFilterVelocityScale / 127.0 * 6.0 // velocity sensing
        * (VelF(velocity, pars->PFilterVelocityScaleFunction) - 1);

    NoteGlobalPar.Volume =
        4.0 * powf(0.1f, 3.0 * (1.0 - pars->PVolume / 96.0)) // -60 dB .. 0 dB
        * VelF(velocity, pars->PAmpVelocityScaleFunction); // velocity sensing

    NoteGlobalPar.AmpEnvelope->envout_dB(); // discard the first envelope output
    globaloldamplitude =
        globalnewamplitude =
            NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB()
                * NoteGlobalPar.AmpLfo->amplfoout();

    NoteGlobalPar.FilterQ = pars->GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking = pars->GlobalFilter->getfreqtracking(basefreq);

    if (parameters->sample[nsample].smp == NULL)
    {
        finished_ = true;
        return;
    }
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
    delete [] tmpwave;
}


inline void PADnote::fadein(float *smps)
{
    int zerocrossings = 0;
    //unsigned int buffersize = zynMaster->getBuffersize();
    for (int i = 1; i < buffersize; ++i)
        if (smps[i - 1] < 0.0 && smps[i] > 0.0)
            zerocrossings++; // this is only the possitive crossings

    float tmp = (buffersize - 1.0) / (zerocrossings + 1) / 3.0;
    if (tmp < 8.0)
        tmp = 8.0;

    int n;
    F2I(tmp, n); // how many samples is the fade-in
    if (n > (int)buffersize)
        n = buffersize;
    for (int i = 0; i < n; ++i)
    {   // fade-in
        float tmp = 0.5 - cosf((float)i / (float) n * PI) * 0.5;
        smps[i] *= tmp;
    }
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

    globalfilterpitch =
        NoteGlobalPar.FilterEnvelope->envout() + NoteGlobalPar.FilterLfo->lfoout()
        + NoteGlobalPar.FilterCenterPitch;

    float tmpfilterfreq =
        globalfilterpitch+ctl->filtercutoff.relfreq + NoteGlobalPar.FilterFreqTracking;

    tmpfilterfreq =
        NoteGlobalPar.GlobalFilterL->getrealfreq(tmpfilterfreq);

    float globalfilterq = NoteGlobalPar.FilterQ * ctl->filterq.relq;
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
               * ctl->pitchwheel.relfreq;
}


int PADnote::Compute_Linear(float *outl, float *outr, int freqhi, float freqlo)
{
    float *smps = pars->sample[nsample].smp;
    if (smps == NULL)
    {
        finished_ = true;
        return 1;
    }
    int size = pars->sample[nsample].size;
    for (int i = 0; i < buffersize; ++i)
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
        finished_ = true;
        return 1;
    }
    int size = pars->sample[nsample].size;
    float xm1, x0, x1, x2, a, b, c;
    for (int i = 0; i < buffersize; ++i)
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
    computecurrentparameters();
    float *smps = pars->sample[nsample].smp;
    if (smps == NULL)
    {
        memset(outl, 0, buffersize * sizeof(float));
        memset(outr, 0, buffersize * sizeof(float));
        return 1;
    }
    float smpfreq = pars->sample[nsample].basefreq;

    float freqrap = realfreq / smpfreq;
    int freqhi = (int) (floorf(freqrap));
    float freqlo = freqrap - floorf(freqrap);

    if (Runtime.Interpolation)
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
        for (int i = 0; i < buffersize; ++i)
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

    if (AboveAmplitudeThreshold(globaloldamplitude,globalnewamplitude))
    {
        // Amplitude Interpolation
        for (int i = 0; i < buffersize; ++i)
        {
            float tmpvol = InterpolateAmplitude(globaloldamplitude,
                                                globalnewamplitude, i,
                                                buffersize);
            outl[i] *= tmpvol * (1.0 - NoteGlobalPar.Panning);
            outr[i] *= tmpvol * NoteGlobalPar.Panning;
        }
    } else {
        for (int i = 0; i < buffersize; ++i)
        {
            outl[i] *= globalnewamplitude * (1.0 - NoteGlobalPar.Panning);
            outr[i] *= globalnewamplitude * NoteGlobalPar.Panning;
        }
    }

    // Apply legato-specific sound signal modifications
    if (Legato.silent) { // Silencer
        if (Legato.msg != LM_FadeIn)
        {
            memset(outl, 0, buffersize * sizeof(float));
            memset(outr, 0, buffersize * sizeof(float));
        }
    }
    switch (Legato.msg)
    {
        case LM_CatchUp : // Continue the catch-up...
            if (Legato.decounter == -10)
                Legato.decounter = Legato.fade.length;
            for (int i = 0; i < buffersize; ++i)
            {   //Yea, could be done without the loop...
                Legato.decounter--;
                if (Legato.decounter < 1)
                {
                    // Catching-up done, we can finally set
                    // the note to the actual parameters.
                    Legato.decounter = -10;
                    Legato.msg = LM_ToNorm;
                    PADlegatonote(Legato.param.freq, Legato.param.vel,
                                  Legato.param.portamento, Legato.param.midinote,
                                  false);
                    break;
                }
            }
            break;
        case LM_FadeIn : // Fade-in
            if (Legato.decounter == -10)
                Legato.decounter = Legato.fade.length;
            Legato.silent = false;
            for (int i = 0; i < buffersize; ++i)
            {
                Legato.decounter--;
                if (Legato.decounter < 1)
                {
                    Legato.decounter = -10;
                    Legato.msg = LM_Norm;
                    break;
                }
                Legato.fade.m += Legato.fade.step;
                Legato.fade.m = Legato.fade.m;
                outl[i] *= Legato.fade.m;
                outr[i] *= Legato.fade.m;
            }
            break;
        case LM_FadeOut : // Fade-out, then set the catch-up
            if (Legato.decounter == -10)
                Legato.decounter = Legato.fade.length;
            for (int i = 0; i < buffersize; ++i)
            {
                Legato.decounter--;
                if (Legato.decounter < 1)
                {
                    for (int j = i; j < buffersize; ++j)
                        outl[j] = outr[j] = 0.0;
                    Legato.decounter = -10;
                    Legato.silent = true;
                    // Fading-out done, now set the catch-up :
                    Legato.decounter = Legato.fade.length;
                    Legato.msg = LM_CatchUp;
                    float catchupfreq =
                        Legato.param.freq * (Legato.param.freq / Legato.lastfreq);
                        // This freq should make this now silent note to catch-up
                        // (or should I say resync ?) with the heard note for
                        // the same length it stayed at the previous freq during
                        // the fadeout.
                    PADlegatonote(catchupfreq, Legato.param.vel,
                                  Legato.param.portamento, Legato.param.midinote,
                                  false);
                    break;
                }
                Legato.fade.m -= Legato.fade.step;
                Legato.fade.m = Legato.fade.m;
                outl[i] *= Legato.fade.m;
                outr[i] *= Legato.fade.m;
            }
            break;
        default :
            break;
    }


    // Check if the global amplitude is finished.
    // If it does, disable the note
    if (NoteGlobalPar.AmpEnvelope->finished() != 0)
    {
        for (int i = 0 ; i < buffersize; ++i)
        {   // fade-out
            float tmp = 1.0 - (float)i / (float)buffersize;
            outl[i] *= tmp;
            outr[i] *= tmp;
        }
        finished_ = 1;
    }
    return 1;
}


void PADnote::relasekey()
{
    NoteGlobalPar.FreqEnvelope->relasekey();
    NoteGlobalPar.FilterEnvelope->relasekey();
    NoteGlobalPar.AmpEnvelope->relasekey();
}
