/*

    SUBnote.cpp - The "subtractive" synthesizer
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Author: Nasca Octavian Paul
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

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Synth/SUBnote.h"

SUBnote::SUBnote(SUBnoteParameters *pars_, Controller *ctl_, float freq_,
                 float velocity_, int portamento_, int midinote_, bool besilent) :
    subnotepars(pars_),
    GlobalFilterL(NULL),
    GlobalFilterR(NULL),
    GlobalFilterEnvelope(NULL),
    portamento(portamento_),
    ctl(ctl_),
    log_0_01(logf(0.01)),
    log_0_001(logf(0.001)),
    log_0_0001(logf(0.0001)),
    log_0_00001(logf(0.00001)),
    samplerate(zynMaster->getSamplerate()),
    buffersize(zynMaster->getBuffersize())
{
    ready = 0;

    tmpsmp = (float*)subnotepars->buffPool->malloc();
    tmprnd = (float*)subnotepars->buffPool->malloc();

    // Initialise some legato-specific vars
    Legato.msg = LM_Norm;
    Legato.fade.length = (int)(samplerate * 0.005f); // 0.005 seems ok.
    if (Legato.fade.length < 1)
        Legato.fade.length = 1;// (if something's fishy)
    Legato.fade.step = (1.0f / Legato.fade.length);
    Legato.decounter = -10;
    Legato.param.freq = freq_;
    Legato.param.vel = velocity_;
    Legato.param.portamento = portamento_;
    Legato.param.midinote = midinote_;
    Legato.silent = besilent;

    NoteEnabled = true;
    volume = powf(0.1f, 3.0f * (1.0 - subnotepars->PVolume / 96.0f)); // -60 dB .. 0 dB
    volume *= VelF(velocity_, subnotepars->PAmpVelocityScaleFunction);
    panning = (subnotepars->PPanning) ? subnotepars->PPanning / 127.0f : zynMaster->numRandom();
    numstages = subnotepars->Pnumstages;
    stereo = subnotepars->Pstereo;
    start = subnotepars->Pstart;
    firsttick = 1;
    int pos[MAX_SUB_HARMONICS];

    if (subnotepars->Pfixedfreq == 0)
        basefreq = freq_;
    else
    {
        basefreq = 440.0f;
        int fixedfreqET = subnotepars->PfixedfreqET;
        if (fixedfreqET)
        {   // if the frequency varies according the keyboard note
            float tmp =
                (midinote_ - 69.0f) / 12.0f * powf(2.0f, (((fixedfreqET - 1) / 63.0f) - 1.0f));
            if (fixedfreqET <= 64)
                basefreq *= powf(2.0f, tmp);
            else
                basefreq *= powf(3.0f, tmp);
        }
    }
    float detune = getdetune(subnotepars->PDetuneType, subnotepars->PCoarseDetune, subnotepars->PDetune);
    basefreq *= powf(2.0f, detune / 1200.0f); // detune
//    basefreq*=ctl->pitchwheel.relfreq;//pitch wheel

    // global filter
    GlobalFilterCenterPitch =
        subnotepars->GlobalFilter->getfreq()
        + // center freq
          (subnotepars->PGlobalFilterVelocityScale / 127.0f * 6.0f)
        * // velocity sensing
          (VelF(velocity_, subnotepars->PGlobalFilterVelocityScaleFunction) - 1);

    // select only harmonics that desire to compute
    numharmonics = 0;
    for (int n = 0; n < MAX_SUB_HARMONICS; ++n)
    {
        if (!subnotepars->Phmag[n])
            continue;
        if (n * basefreq > samplerate / 2.0f)
            break; // remove the freqs above the Nyquist freq
        pos[numharmonics++] = n;
    }
    firstnumharmonics = numharmonics; // (gf)Useful in legato mode.

    if (!numharmonics)
    {
        NoteEnabled = false;
        return;
    }

    lfilter = (bpfilter*)subnotepars->bpfilterPool->malloc();
    if (stereo)
        rfilter = (bpfilter*)subnotepars->bpfilterPool->malloc();

    // how much the amplitude is normalised (because the harmonics)
    float reduceamp = 0.0f;

    for (int n = 0; n < numharmonics; ++n)
    {
        float freq = basefreq * (pos[n] + 1);

        // the bandwidth is not absolute(Hz); it is relative to frequency
        float bw = powf(10.0f, (subnotepars->Pbandwidth - 127.0f) / 127.0f * 4.0f) * numstages;

        // Bandwidth Scale
        bw *= powf(1000.0f / freq, (subnotepars->Pbwscale - 64.0f) / 64.0f * 3.0f);

        // Relative BandWidth
        bw *= powf(100.0f, (subnotepars->Phrelbw[pos[n]] - 64.0f) / 64.0f);

        if (bw > 25.0f)
            bw = 25.0f;

        // try to keep same amplitude on all freqs and bw. (empirically)
        float gain = sqrtf(1500.0f / (bw * freq));

        float hmagnew = 1.0f - subnotepars->Phmag[pos[n]] / 127.0f;
        float hgain;

        switch (subnotepars->Phmagtype)
        {
            case 1:
                hgain = expf(hmagnew * log_0_01);
                break;
            case 2:
                hgain = expf(hmagnew * log_0_001);
                break;
            case 3:
                hgain = expf(hmagnew * log_0_0001);
                break;
            case 4:
                hgain = expf(hmagnew * log_0_00001);
                break;
            default:
                hgain = 1.0f - hmagnew;
        }
        gain *= hgain;
        reduceamp += hgain;

        for (int nph = 0; nph < numstages; ++nph)
        {
            float amp = 1.0f;
            if (nph == 0)
                amp = gain;
            initfilter(lfilter[nph + n * numstages], freq, bw, amp, hgain);
            if (stereo)
                initfilter(rfilter[nph + n * numstages], freq, bw, amp, hgain);
        }
    }

    if (reduceamp < 0.001f)
        reduceamp = 1.0f;
    volume /= reduceamp;

    oldpitchwheel = 0;
    oldbandwidth = 64;
    if (subnotepars->Pfixedfreq == 0)
        initparameters(basefreq);
    else
        initparameters(basefreq / 440.0f * freq_);

    oldamplitude = newamplitude;
    ready = 1;
}


// SUBlegatonote: This function is (mostly) a copy of SUBnote(...) and
// initparameters(...) stuck together with some lines removed so that
// it only alter the already playing note (to perform legato). It is
// possible I left stuff that is not required for this.
void SUBnote::SUBlegatonote(float freq, float velocity,
                            int portamento_, int midinote, bool externcall)
{
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
            }
            else
            {
                Legato.fade.m = 1.0;
                Legato.msg = LM_FadeOut;
                return;
            }
        }
        if (Legato.msg == LM_ToNorm)
            Legato.msg = LM_Norm;
    }

    portamento = portamento_;

    volume = powf(0.1f, 3.0f * (1.0f - subnotepars->PVolume / 96.0f)); // -60 dB .. 0 dB
    volume *= VelF(velocity, subnotepars->PAmpVelocityScaleFunction);
    if (subnotepars->PPanning)
        panning = subnotepars->PPanning / 127.0f;
    else
        panning = zynMaster->numRandom();

    // start=subnotepars->Pstart;

    int pos[MAX_SUB_HARMONICS];

    if (subnotepars->Pfixedfreq == 0)
        basefreq = freq;
    else
    {
        basefreq = 440.0f;
        int fixedfreqET = subnotepars->PfixedfreqET;
        if (fixedfreqET != 0)
        {   //if the frequency varies according the keyboard note
            float tmp = (midinote - 69.0f) / 12.0f
                              * (powf(2.0f, (fixedfreqET - 1) / 63.0f) - 1.0f);
            if (fixedfreqET <= 64)
                basefreq *= powf(2.0f, tmp);
            else
                basefreq *= powf(3.0f, tmp);
        }
    }
    float detune = getdetune(subnotepars->PDetuneType, subnotepars->PCoarseDetune, subnotepars->PDetune);
    basefreq *= powf(2.0f, detune / 1200.0f); // detune

    // global filter
    GlobalFilterCenterPitch = subnotepars->GlobalFilter->getfreq() + // center freq
                              (subnotepars->PGlobalFilterVelocityScale / 127.0f * 6.0f)
                              // velocity sensing
                              * (VelF(velocity, subnotepars->PGlobalFilterVelocityScaleFunction) - 1);


    int legatonumharmonics = 0;
    for (int n = 0; n < MAX_SUB_HARMONICS; ++n)
    {
        if (!subnotepars->Phmag[n])
            continue;
        if (n * basefreq > samplerate / 2.0f)
            break; // remove the freqs above the Nyquist freq
        pos[legatonumharmonics++] = n;
    }
    if (legatonumharmonics > firstnumharmonics)
        numharmonics = firstnumharmonics;
    else
        numharmonics = legatonumharmonics;

    if (!numharmonics)
    {
        NoteEnabled = false;
        return;
    }

    // how much the amplitude is normalised (because the harmonics)
    float reduceamp = 0.0f;
    for (int n = 0; n < numharmonics; ++n)
    {
        float freq = basefreq * (pos[n] + 1);

        // the bandwidth is not absolute(Hz); it is relative to frequency
        float bw = powf(10.0f, (subnotepars->Pbandwidth - 127.0f) / 127.0f * 4.0f) * numstages;

        // Bandwidth Scale
        bw *= powf(1000.0f / freq, ((subnotepars->Pbwscale - 64.0f) / 64.0f * 3.0f));

        // Relative BandWidth
        bw *= powf(100.0f, (subnotepars->Phrelbw[pos[n]] - 64.0f) / 64.0f);

        if (bw > 25.0f)
            bw = 25.0f;

        // try to keep same amplitude on all freqs and bw. (empirically)
        float gain = sqrtf(1500.0f / (bw * freq));

        float hmagnew = 1.0f - subnotepars->Phmag[pos[n]] / 127.0f;
        float hgain;

        switch (subnotepars->Phmagtype)
        {
            case 1:
                hgain = expf(hmagnew * log_0_01);
                break;
            case 2:
                hgain = expf(hmagnew * log_0_001);
                break;
            case 3:
                hgain = expf(hmagnew * log_0_0001);
                break;
            case 4:
                hgain = expf(hmagnew * log_0_00001);
                break;
            default:
                hgain = 1.0f - hmagnew;
        }
        gain *= hgain;
        reduceamp += hgain;

        for (int nph = 0; nph < numstages; ++nph)
        {
            float amp = 1.0;
            if (nph == 0)
                amp = gain;
            initfilter(lfilter[nph + n * numstages], freq, bw, amp, hgain);
            if (stereo)
                initfilter(rfilter[nph + n * numstages], freq, bw, amp, hgain);
        }
    }

    if (reduceamp < 0.001f)
        reduceamp = 1.0f;
    volume /= reduceamp;

    oldpitchwheel = 0;
    oldbandwidth = 64;

    if (!subnotepars->Pfixedfreq)
        freq = basefreq;
    else
        freq *= basefreq / 440.0f;

    ///////////////
    // Altered initparameters(...) content:

    if (subnotepars->PGlobalFilterEnabled)
    {
        globalfiltercenterq = subnotepars->GlobalFilter->getq();
        GlobalFilterFreqTracking = subnotepars->GlobalFilter->getfreqtracking(basefreq);
    }

    // end of the altered initparameters function content.
    ///////////////

    oldamplitude = newamplitude;

    // End of the SUBlegatonote function.
}

SUBnote::~SUBnote()
{
    if (NoteEnabled)
        KillNote();
    subnotepars->buffPool->free(tmpsmp);
    subnotepars->buffPool->free(tmprnd);
}

// Kill the note
void SUBnote::KillNote(void)
{
    if (NoteEnabled)
    {
        if (lfilter)
        {
            subnotepars->bpfilterPool->free(lfilter);
            lfilter = NULL;
        }
        if (rfilter)
        {
            subnotepars->bpfilterPool->free(rfilter);
            rfilter = NULL;
        }
        subnotepars->envPool->destroy(AmpEnvelope);
        if (FreqEnvelope)
        {
            subnotepars->envPool->destroy(FreqEnvelope);
            FreqEnvelope = NULL;
        }
        if (BandWidthEnvelope)
        {
            subnotepars->envPool->destroy(BandWidthEnvelope);
            BandWidthEnvelope = NULL;
        }
        NoteEnabled = false;
    }
}

// Compute the filters coefficients
void SUBnote::computefiltercoefs(bpfilter &filter, float freq,
                                 float bw, float gain)
{
    if (freq > samplerate / 2.0f - 200.0f)
        freq = samplerate / 2.0f - 200.0f;
    float omega = 2.0f * PI * freq / samplerate;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn * sinhf(LOG_2 / 2.0f * bw * omega / sn);
    if (alpha > 1)
        alpha = 1;
    if (alpha > bw)
        alpha = bw;
    filter.b0 = alpha / (1.0f + alpha) * filter.amp * gain;
    filter.b2 = -alpha / (1.0f + alpha) * filter.amp * gain;
    filter.a1 = -2.0f * cs / (1.0f + alpha);
    filter.a2 = (1.0f - alpha) / (1.0f + alpha);
}

// Initialise the filters
void SUBnote::initfilter(bpfilter &filter, float freq, float bw,
                         float amp, float mag)
{
    filter.xn1 = 0.0f;
    filter.xn2 = 0.0f;
    if (!start)
    {
        filter.yn1 = 0.0f;
        filter.yn2 = 0.0f;
    }
    else
    {
        float a = 0.1f * mag; // empirically
        float p = zynMaster->numRandom() * 2.0f * PI;
        if (start == 1)
            a *= zynMaster->numRandom();
        filter.yn1 = a * cosf(p);
        filter.yn2 = a * cosf(p + freq * 2.0f * PI / samplerate);

        // correct the error of computation the start amplitude
        // at very high frequencies
        if (freq > samplerate * 0.96f)
        {
            filter.yn1 = 0.0f;
            filter.yn2 = 0.0f;
        }
    }

    filter.amp = amp;
    filter.freq = freq;
    filter.bw = bw;
    computefiltercoefs(filter, freq, bw, 1.0f);
}

// Do the filtering
void SUBnote::filter(bpfilter &filter, float *smps)
{
    float out;
    for (int i = 0; i < buffersize; ++i)
    {
        out = smps[i] * filter.b0 + filter.b2 * filter.xn2
              - filter.a1 * filter.yn1 - filter.a2 * filter.yn2;
        filter.xn2 = filter.xn1;
        filter.xn1 = smps[i];
        filter.yn2 = filter.yn1;
        filter.yn1 = out;
        smps[i] = out;
    }
}

// Init Parameters
void SUBnote::initparameters(float freq)
{
    AmpEnvelope = subnotepars->envPool->
        construct(Envelope(subnotepars->AmpEnvelope, freq));
    if (subnotepars->PFreqEnvelopeEnabled)
        FreqEnvelope = subnotepars->envPool->
            construct(Envelope(subnotepars->FreqEnvelope, freq));
    else
        FreqEnvelope = NULL;
    if (subnotepars->PBandWidthEnvelopeEnabled)
        BandWidthEnvelope = subnotepars->envPool->
            construct(Envelope(subnotepars->BandWidthEnvelope, freq));
    else
        BandWidthEnvelope = NULL;
    if (subnotepars->PGlobalFilterEnabled)
    {
        globalfiltercenterq = subnotepars->GlobalFilter->getq();
        GlobalFilterL = new Filter(subnotepars->GlobalFilter);
        if (stereo)
            GlobalFilterR = new Filter(subnotepars->GlobalFilter);
        GlobalFilterEnvelope = subnotepars->envPool->
            construct(Envelope(subnotepars->GlobalFilterEnvelope, freq));
        GlobalFilterFreqTracking = subnotepars->GlobalFilter->getfreqtracking(basefreq);
    }
    computecurrentparameters();
}

// Compute Parameters of SUBnote for each tick
void SUBnote::computecurrentparameters(void)
{
    if (FreqEnvelope
        || BandWidthEnvelope
        || oldpitchwheel != ctl->pitchwheel.data
        || oldbandwidth != ctl->bandwidth.data
        || portamento)
    {
        float envfreq = 1.0f;
        float envbw = 1.0f;
        float gain = 1.0f;

        if (FreqEnvelope)
        {
            envfreq = FreqEnvelope->envout() / 1200;
            envfreq = powf(2.0f, envfreq);
        }
        envfreq *= ctl->pitchwheel.relfreq; // pitch wheel
        if (portamento)
        {   // portamento is used
            envfreq *= ctl->portamento.freqrap;
            if (!ctl->portamento.used)
            {   // the portamento has finished
                portamento = 0; // this note is no longer "portamented"
            }
        }

        if (BandWidthEnvelope)
        {
            envbw = BandWidthEnvelope->envout();
            envbw = powf(2.0f, envbw);
        }
        envbw *= ctl->bandwidth.relbw; // bandwidth controller

        float tmpgain = 1.0f / sqrtf(envbw * envfreq);

        for (int n = 0; n < numharmonics; ++n)
        {
            for (int nph = 0; nph < numstages; ++nph)
            {
                if (!nph)
                    gain = tmpgain;
                else
                    gain = 1.0f;
                computefiltercoefs( lfilter[nph + n * numstages],
                                    lfilter[nph + n *numstages].freq * envfreq,
                                    lfilter[nph + n * numstages].bw * envbw, gain);
            }
        }
        if (stereo)
            for (int n = 0; n < numharmonics; ++n)
            {
                for (int nph = 0; nph < numstages; ++nph)
                {
                    if (!nph)
                        gain = tmpgain;
                    else
                        gain = 1.0f;
                    computefiltercoefs( rfilter[nph + n * numstages],
                                        rfilter[nph + n * numstages].freq * envfreq,
                                        rfilter[nph + n * numstages].bw * envbw, gain);
                }
            }
        oldbandwidth = ctl->bandwidth.data;
        oldpitchwheel = ctl->pitchwheel.data;
    }
    newamplitude = volume * AmpEnvelope->envout_dB() * 2.0f;

    // Filter
    if (GlobalFilterL)
    {
        float globalfilterpitch = GlobalFilterCenterPitch + GlobalFilterEnvelope->envout();
        float filterfreq = globalfilterpitch + ctl->filtercutoff.relfreq + GlobalFilterFreqTracking;
        filterfreq = GlobalFilterL->getrealfreq(filterfreq);

        GlobalFilterL->setfreq_and_q(filterfreq, globalfiltercenterq * ctl->filterq.relq);
        if (GlobalFilterR)
            GlobalFilterR->setfreq_and_q(filterfreq, globalfiltercenterq * ctl->filterq.relq);
    }
}

// Note Output
int SUBnote::noteout(float *outl, float *outr)
{
    memset(outl, 0, buffersize * sizeof(float));
    memset(outr, 0, buffersize * sizeof(float));
    if (!NoteEnabled)
        return 0;

    // left channel
    for (int i = 0; i < buffersize; ++i)
        tmprnd[i] = zynMaster->numRandom() * 2.0f - 1.0f;
    for (int n = 0; n < numharmonics; ++n)
    {
        //for (int i = 0; i < buffersize; ++i)
        //    tmpsmp[i] = tmprnd[i];
        memcpy(tmpsmp, tmprnd, buffersize * sizeof(float));
        for (int nph = 0; nph < numstages; ++nph)
            filter(lfilter[nph + n * numstages], tmpsmp);
        for (int i = 0; i < buffersize; ++i)
            outl[i] += tmpsmp[i];
    }

    if (GlobalFilterL)
        GlobalFilterL->filterout(&outl[0]);

    // right channel
    if (stereo)
    {
        for (int i = 0; i < buffersize; ++i)
            tmprnd[i] = zynMaster->numRandom() * 2.0f - 1.0f;
        for (int n = 0; n < numharmonics; ++n)
        {
            memcpy(tmpsmp, tmprnd, buffersize * sizeof(float));
            for (int nph = 0; nph < numstages; ++nph)
                filter(rfilter[nph + n * numstages], tmpsmp);
            for (int i = 0; i < buffersize; ++i)
                outr[i] += tmpsmp[i];
        }
        if (GlobalFilterR)
            GlobalFilterR->filterout(&outr[0]);
    }
    else
        memcpy(outr, outl, buffersize * sizeof(float));

    if (firsttick)
    {
        int n = 10;
        if (n > buffersize)
            n = buffersize;
        for (int i = 0; i < n; ++i)
        {
            float ampfadein = 0.5f - 0.5f * cosf((float)i / (float)n * PI);
            outl[i] *= ampfadein;
            outr[i] *= ampfadein;
        }
        firsttick = 0;
    }

    if (AboveAmplitudeThreshold(oldamplitude, newamplitude))
    {
        // Amplitude interpolation
        for (int i = 0; i < buffersize; ++i)
        {
            float tmpvol = InterpolateAmplitude(oldamplitude, newamplitude, i,
                                                buffersize);
            outl[i] *= tmpvol * (1.0f - panning);
            outr[i] *= tmpvol * panning;
        }
    }
    else
    {
        for (int i = 0; i < buffersize; ++i)
        {
            outl[i] *= newamplitude * (1.0f - panning);
            outr[i] *= newamplitude * panning;
        }
    }
    oldamplitude = newamplitude;
    computecurrentparameters();

    // Apply legato-specific sound signal modifications
    if (Legato.silent)
    {   // Silencer
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
            {   // Yea, could be done without the loop...
                Legato.decounter--;
                if (Legato.decounter < 1)
                {
                    // Catching-up done, we can finally set
                    // the note to the actual parameters.
                    Legato.decounter = -10;
                    Legato.msg = LM_ToNorm;
                    SUBlegatonote(Legato.param.freq, Legato.param.vel,
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
                        outl[j] = outr[j] = 0.0f;
                    Legato.decounter = -10;
                    Legato.silent = true;
                    // Fading-out done, now set the catch-up :
                    Legato.decounter = Legato.fade.length;
                    Legato.msg = LM_CatchUp;
                    // This freq should make this now silent note to catch-up
                    // (or should I say resync ?) with the heard note for the same
                    // length it stayed at the previous freq during the fadeout.
                    float catchupfreq =
                        Legato.param.freq * (Legato.param.freq / Legato.lastfreq);
                    SUBlegatonote(catchupfreq, Legato.param.vel,
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

    // Check if the note needs to be computed more
    if (AmpEnvelope->finished())
    {
        for (int i = 0; i < buffersize; ++i)
        {   // fade-out
            float tmp = 1.0f - (float)i / (float)buffersize;
            outl[i] *= tmp;
            outr[i] *= tmp;
        }
        KillNote();
    }
    return 1;
}

// Relase Key (Note Off)
void SUBnote::relasekey(void)
{
    AmpEnvelope->relasekey();
    if (FreqEnvelope)
        FreqEnvelope->relasekey();
    if (BandWidthEnvelope)
        BandWidthEnvelope->relasekey();
    if (GlobalFilterEnvelope)
        GlobalFilterEnvelope->relasekey();
}
