/*

    SUBnote.cpp - The "subtractive" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others
    Copyright 2020 Kristian Amlie & others

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

    This file is derivative of ZynAddSubFX original code

*/

#include <cmath>
#include <iostream>

#include "Params/SUBnoteParameters.h"
#include "Params/Controller.h"
#include "Synth/SUBnote.h"
#include "Synth/Envelope.h"
#include "DSP/Filter.h"
#include "Misc/SynthEngine.h"
#include "Misc/SynthHelper.h"
#include "Misc/NumericFuncs.h"

using func::power;
using func::powFrac;
using func::decibel;
using synth::velF;
using synth::getDetune;
using synth::interpolateAmplitude;
using synth::aboveAmplitudeThreshold;

using func::setRandomPan;



SUBnote::SUBnote(SUBnoteParameters& parameters, Controller& ctl_, Note note_, bool portamento_)
    : synth{parameters.getSynthEngine()}
    , pars{parameters}
    , subNoteChange{parameters}
    , ctl{ctl_}
    , note{note_}
    , stereo{pars.Pstereo}
    , realfreq{computeRealFreq()}
    , portamento{portamento_}
    , numstages{pars.Pnumstages}
    , numharmonics{0}
    , start{pars.Pstart}
    , pos{0}
    , bendAdjust{0}
    , offsetHz{0}
    , ampEnvelope{}
    , freqEnvelope{}
    , bandWidthEnvelope{}
    , globalFilterEnvelope{}
    , globalFilterL{}
    , globalFilterR{}
    , noteStatus{NOTE_ENABLED}
    , firsttick{1}
    , lfilter{}
    , rfilter{}
    , tmpsmp{synth.getRuntime().genTmp1}
    , tmprnd{synth.getRuntime().genTmp2}
    , oldpitchwheel{0}
    , oldbandwidth{64}
    , legatoFade{1.0f}       // Full volume
    , legatoFadeStep{0.0f}   // Legato disabled
    , filterStep(0)
{
    // Initialise some legato-specific vars

    setRandomPan(synth.numRandom(), randpanL, randpanR, synth.getRuntime().panLaw, pars.PPanning, pars.PWidth);

    if (pars.Pfixedfreq == 0)
        initparameters(realfreq);
    else
        initparameters(realfreq / 440.0f * note.freq);

    computeNoteParameters();
    computecurrentparameters();

    oldamplitude = newamplitude;
}


// Copy constructor, used only used for legato (as of 4/2022)
SUBnote::SUBnote(SUBnote const& orig)
    : synth{orig.synth}
    , pars{orig.pars}
    , subNoteChange{pars}
    , ctl{orig.ctl}
    , note{orig.note}
    , stereo{orig.stereo}
    , realfreq{orig.realfreq}
    , portamento{orig.portamento}
    , numstages{orig.numstages}
    , numharmonics{orig.numharmonics}
    , start{orig.start}
    // pos
    , bendAdjust{orig.bendAdjust}
    , offsetHz{orig.offsetHz}
    , randpanL{orig.randpanL}
    , randpanR{orig.randpanR}
    , ampEnvelope{}
    , freqEnvelope{}
    , bandWidthEnvelope{}
    , globalFilterEnvelope{}
    , globalFilterL{}
    , globalFilterR{}
    , noteStatus{orig.noteStatus}
    , firsttick{orig.firsttick}
    , volume{orig.volume}
    , oldamplitude{orig.oldamplitude}
    , newamplitude{orig.newamplitude}
    , lfilter{}
    , rfilter{}
    , tmpsmp{orig.synth.getRuntime().genTmp1}
    , tmprnd{orig.synth.getRuntime().genTmp2}
    , oldpitchwheel{orig.oldpitchwheel}
    , oldbandwidth{orig.oldbandwidth}
    , legatoFade{0.0f}     // Silent by default
    , legatoFadeStep{0.0f} // Legato disabled
    , filterStep{orig.filterStep}
{
    memcpy(pos, orig.pos, MAX_SUB_HARMONICS * sizeof(int));
    memcpy(overtone_rolloff, orig.overtone_rolloff,
        numharmonics * sizeof(float));
    memcpy(overtone_freq, orig.overtone_freq,
        numharmonics * sizeof(float));

    ampEnvelope.reset(new Envelope{*orig.ampEnvelope});
    if (orig.freqEnvelope)
        freqEnvelope.reset(new Envelope{*orig.freqEnvelope});
    if (orig.bandWidthEnvelope)
        bandWidthEnvelope.reset(new Envelope{*orig.bandWidthEnvelope});
    if (pars.PGlobalFilterEnabled != 0)
    {
        globalFilterL.reset(new Filter{*orig.globalFilterL});
        globalFilterR.reset(new Filter{*orig.globalFilterR});
        globalFilterEnvelope.reset(new Envelope{*orig.globalFilterEnvelope});
    }

    if (orig.lfilter)
    {
        lfilter.reset(new bpfilter[numstages * numharmonics]);
        memcpy(lfilter.get(), orig.lfilter.get(),
            numstages * numharmonics * sizeof(bpfilter));
    }
    if (orig.rfilter)
    {
        rfilter.reset(new bpfilter[numstages * numharmonics]);
        memcpy(rfilter.get(), orig.rfilter.get(),
            numstages * numharmonics * sizeof(bpfilter));
    }
}



void SUBnote::performPortamento(Note note_)
{
    portamento = true;
    this->note = note_;
    realfreq = computeRealFreq();
    // carry on all other parameters unaltered

    computeNoteParameters();
}


void SUBnote::legatoFadeIn(Note note_)
{
    portamento = false; // portamento-legato treated separately
    this->note = note_;
    realfreq = computeRealFreq();

    computeNoteParameters();

    legatoFade = 0.0f; // Start crossfade silent
    legatoFadeStep = synth.fadeStepShort; // Positive steps
}


void SUBnote::legatoFadeOut()
{
    legatoFade = 1.0f;     // crossfade down from full volume
    legatoFadeStep = -synth.fadeStepShort; // Negative steps

    // transitory state similar to a released Envelope
    noteStatus = NOTE_LEGATOFADEOUT;
}


SUBnote::~SUBnote()
{
    killNote();
}


// Kill the note
void SUBnote::killNote()
{
    if (noteStatus != NOTE_DISABLED)
    {
        lfilter.reset();
        rfilter.reset();
        ampEnvelope.reset();
        freqEnvelope.reset();
        bandWidthEnvelope.reset();
        globalFilterEnvelope.reset();
        noteStatus = NOTE_DISABLED;
    }
}

int SUBnote::createNewFilters()
{
    bool alreadyEnabled[MAX_SUB_HARMONICS];
    memset(alreadyEnabled, 0, sizeof(alreadyEnabled));
    for (int p = 0; p < numharmonics; ++p)
        alreadyEnabled[pos[p]] = true;

    // select only harmonics that desire to compute
    int origNumHarmonics = numharmonics;
    for (int n = 0; n < MAX_SUB_HARMONICS; ++n)
    {
        if (pars.Phmag[n] == 0 || alreadyEnabled[n])
            continue;
        if (n * realfreq > synth.halfsamplerate_f)
            break; // remove the freqs above the Nyquist freq
        pos[numharmonics++] = n;
        alreadyEnabled[n] = true;
    }

    if (numharmonics == origNumHarmonics)
        return 0;

    bpfilter *newFilter = new bpfilter[numstages * numharmonics];
    if (lfilter)
        memcpy(newFilter, lfilter.get(), numstages * origNumHarmonics * sizeof(bpfilter));
    lfilter.reset(newFilter);
    if (stereo)
    {
        newFilter = new bpfilter[numstages * numharmonics];
        if (rfilter)
            memcpy(newFilter, rfilter.get(), numstages * origNumHarmonics * sizeof(bpfilter));
        rfilter.reset(newFilter);
    }

    return numharmonics - origNumHarmonics;
}

float SUBnote::computeRealFreq()
{
    float freq = note.freq;
    if (pars.Pfixedfreq)
    {
        freq = 440.0f;
        int fixedfreqET = pars.PfixedfreqET;
        if (fixedfreqET)
        {// if the frequency varies according the keyboard note
            float exponent = (note.midi - 69.0f) / 12.0f * power<2>((((fixedfreqET - 1) / 63.0f) - 1.0f));
            freq *= (fixedfreqET <= 64)? power<2>(exponent)
                                       : power<3>(exponent);
        }
    }

    float detune = getDetune(pars.PDetuneType, pars.PCoarseDetune, pars.PDetune);
    freq *= power<2>(detune / 1200.0f); // detune
    return freq;
}

void SUBnote::computeNoteParameters()
{
    volume = 2.0f                                         // +6dB boost (note ADDnote and PADnote apply a +12dB boost)
           * decibel<-60>(1.0f - pars.PVolume / 96.0f)   // -60 dB .. +19.375 dB
           * velF(note.vel, pars.PAmpVelocityScaleFunction);

    int BendAdj = pars.PBendAdjust - 64;
    if (BendAdj % 24 == 0)
        bendAdjust = BendAdj / 24;
    else
        bendAdjust = BendAdj / 24.0f;
    float offset_val = (pars.POffsetHz - 64)/64.0f;
    offsetHz = 15.0f*(offset_val * sqrtf(fabsf(offset_val)));

    updatefilterbank();
}

// Compute the filters coefficients
void SUBnote::computefiltercoefs(bpfilter &filter, float freq, float bw, float gain)
{
    if (freq > synth.halfsamplerate_f - 200.0f)
    {
        freq = synth.halfsamplerate_f - 200.0f;
    }

    float omega = TWOPI * freq / synth.samplerate_f;
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
void SUBnote::initfilters(int startIndex)
{
    for (int n = startIndex; n < numharmonics; ++n)
    {
        float hgain = getHgain(n);

        for (int nph = 0; nph < numstages; ++nph)
        {
            initfilter(lfilter[nph + n * numstages], hgain);
            if (stereo)
                initfilter(rfilter[nph + n * numstages], hgain);
        }
    }
}

void SUBnote::initfilter(bpfilter &filter, float mag)
{
    filter.xn1 = 0.0f;
    filter.xn2 = 0.0f;

    if (start == 0)
    {
        filter.yn1 = 0.0f;
        filter.yn2 = 0.0f;
    }
    else
    {
        float a = 0.1f * mag; // empirically
        float p = synth.numRandom() * TWOPI;
        if (start == 1)
            a *= synth.numRandom();
        filter.yn1 = a * cosf(p);
        filter.yn2 = a * cosf(p + filter.freq * TWOPI / synth.samplerate_f);

        // correct the error of computation the start amplitude
        // at very high frequencies
        if (filter.freq > synth.samplerate_f * 0.96f)
        {
            filter.yn1 = 0.0f;
            filter.yn2 = 0.0f;
        }
    }
}


// Do the filtering
inline void SubFilterA(const float coeff[4], float &src, float work[4])
{
    work[3] = src*coeff[0]+work[1]*coeff[1]+work[2]*coeff[2]+work[3]*coeff[3];
    work[1] = src;
    src     = work[3];
}


inline void SubFilterB(const float coeff[4], float &src, float work[4])
{
    work[2] = src*coeff[0]+work[0]*coeff[1]+work[3]*coeff[2]+work[2]*coeff[3];
    work[0] = src;
    src     = work[2];
}


// ported from zynaddsubfx V 2.4.4
//This dance is designed to minimize unneeded memory operations which can result
//in quite a bit of wasted time
void SUBnote::filter(bpfilter &filter, float *smps)
{
    if (synth.getRuntime().isLV2){
        filterVarRun(filter, smps);
        return;
    }

    int remainder = synth.sent_buffersize % 8;
    int blocksize = synth.sent_buffersize - remainder;
    float coeff[4] = {filter.b0, filter.b2,  -filter.a1, -filter.a2};
    float work[4]  = {filter.xn1, filter.xn2, filter.yn1, filter.yn2};

    for (int i = 0; i < blocksize; i += 8)
    {
        SubFilterA(coeff, smps[i + 0], work);
        SubFilterB(coeff, smps[i + 1], work);
        SubFilterA(coeff, smps[i + 2], work);
        SubFilterB(coeff, smps[i + 3], work);
        SubFilterA(coeff, smps[i + 4], work);
        SubFilterB(coeff, smps[i + 5], work);
        SubFilterA(coeff, smps[i + 6], work);
        SubFilterB(coeff, smps[i + 7], work);
    }
    if (remainder > 0)
    {
        for (int i = blocksize; i < blocksize + remainder ; i += 2)
        {
            SubFilterA(coeff, smps[i + 0], work);
            SubFilterB(coeff, smps[i + 1], work);
        }
    }
    filter.xn1 = work[0];
    filter.xn2 = work[1];
    filter.yn1 = work[2];
    filter.yn2 = work[3];
}


//Andrew Deryabin: support for variable-length runs
//currently only for lv2 plugin
void SUBnote::filterVarRun(SUBnote::bpfilter &filter, float *smps)
{
    float tmpout;
    int runLength = synth.sent_buffersize;
    int i = 0;
    if (runLength >= 8){
        float coeff[4] = {filter.b0, filter.b2,  -filter.a1, -filter.a2};
        float work[4]  = {filter.xn1, filter.xn2, filter.yn1, filter.yn2};
        while (runLength >= 8){
            SubFilterA(coeff, smps[i + 0], work);
            SubFilterB(coeff, smps[i + 1], work);
            SubFilterA(coeff, smps[i + 2], work);
            SubFilterB(coeff, smps[i + 3], work);
            SubFilterA(coeff, smps[i + 4], work);
            SubFilterB(coeff, smps[i + 5], work);
            SubFilterA(coeff, smps[i + 6], work);
            SubFilterB(coeff, smps[i + 7], work);
            i += 8;
            runLength -= 8;
        }
        filter.xn1 = work[0];
        filter.xn2 = work[1];
        filter.yn1 = work[2];
        filter.yn2 = work[3];
    }

    for (; i < synth.sent_buffersize; ++i){
        tmpout=smps[i] * filter.b0 + filter.b2 * filter.xn2
               -filter.a1 * filter.yn1 - filter.a2 * filter.yn2;
        filter.xn2=filter.xn1;
        filter.xn1=smps[i];
        filter.yn2=filter.yn1;
        filter.yn1=tmpout;
        smps[i]=tmpout;
    }

}


// Init Parameters
void SUBnote::initparameters(float freq)
{
    ampEnvelope.reset(new Envelope{pars.AmpEnvelope, freq, &synth});
    if (pars.PFreqEnvelopeEnabled != 0)
        freqEnvelope.reset(new Envelope{pars.FreqEnvelope, freq, &synth});
    if (pars.PBandWidthEnvelopeEnabled != 0)
        bandWidthEnvelope.reset(new Envelope{pars.BandWidthEnvelope, freq, &synth});
    if (pars.PGlobalFilterEnabled != 0)
    {
        globalFilterL.reset(new Filter{*pars.GlobalFilter, synth});
        /* TODO
         * Sort this properly it is a temporary fix to stop a segfault
         * with the following very specific settings:
         * Part Mode set to Legato
         * Subsynth enabled
         * Subsynth Filter enabled
         * Subsynth Stereo disabled
         */
        //if (stereo)
            globalFilterR.reset(new Filter{*pars.GlobalFilter, synth});
        globalFilterEnvelope.reset(new Envelope{pars.GlobalFilterEnvelope, freq, &synth});
    }
}



// Compute how much to reduce amplitude near nyquist or subaudible frequencies.
float SUBnote::computerolloff(float freq)
{
    const float lower_limit = 10.0f;
    const float lower_width = 10.0f;
    const float upper_width = 200.0f;
    float upper_limit = synth.samplerate / 2.0f;

    if (freq > lower_limit + lower_width &&
            freq < upper_limit - upper_width)
        return 1.0f;
    if (freq <= lower_limit || freq >= upper_limit)
        return 0.0f;
    if (freq <= lower_limit + lower_width)
        return (1.0f - cosf(PI * (freq - lower_limit) / lower_width)) / 2.0f;
    return (1.0f - cosf(PI * (freq - upper_limit) / upper_width)) / 2.0f;
}

void SUBnote::computeallfiltercoefs()
{
    float envfreq = 1.0f;
    float envbw = 1.0f;
    float gain = 1.0f;

    if (freqEnvelope != NULL)
    {
        envfreq = freqEnvelope->envout() / 1200;
        envfreq = power<2>(envfreq);
    }

    envfreq *= powf(ctl.pitchwheel.relfreq, bendAdjust); // pitch wheel

    if (portamento)
    {
        envfreq *= ctl.portamento.freqrap;
        if (ctl.portamento.used == 0)
        {   // the portamento has finished
            portamento = false; // this note is no longer "portamented"
        }
    }

    if (bandWidthEnvelope != NULL)
    {
        envbw = bandWidthEnvelope->envout();
        envbw = power<2>(envbw);
    }
    envbw *= ctl.bandwidth.relbw; // bandwidth controller

    float tmpgain = 1.0f / sqrtf(envbw * envfreq);

    for (int n = 0; n < numharmonics; ++n)
    {
        for (int nph = 0; nph < numstages; ++nph)
        {
            if (nph == 0)
                gain = tmpgain;
            else
                gain = 1.0f;
            computefiltercoefs(lfilter[nph + n * numstages],
                               lfilter[nph + n *numstages].freq * envfreq,
                               lfilter[nph + n * numstages].bw * envbw, gain);
        }
    }
    if (stereo)
        for (int n = 0; n < numharmonics; ++n)
        {
            for (int nph = 0; nph < numstages; ++nph)
            {
                if (nph == 0)
                    gain = tmpgain;
                else
                    gain = 1.0f;
                computefiltercoefs(rfilter[nph + n * numstages],
                                   rfilter[nph + n * numstages].freq * envfreq,
                                   rfilter[nph + n * numstages].bw * envbw, gain);
            }
        }
    oldbandwidth = ctl.bandwidth.data;
    oldpitchwheel = ctl.pitchwheel.data;
}

// Compute Parameters of SUBnote for each tick
void SUBnote::computecurrentparameters()
{
    if (freqEnvelope != NULL
        || bandWidthEnvelope != NULL
        || oldpitchwheel != ctl.pitchwheel.data
        || oldbandwidth != ctl.bandwidth.data
        || portamento)
        computeallfiltercoefs();

    // Envelope
    newamplitude = volume * ampEnvelope->envout_dB();

    // Filter
    if (globalFilterL != NULL)
    {
        float filterCenterPitch =
            pars.GlobalFilter->getfreq()
            + // center freq
            (pars.PGlobalFilterVelocityScale / 127.0f * 6.0f)
            * // velocity sensing
            (velF(note.vel, pars.PGlobalFilterVelocityScaleFunction) - 1);
        float filtercenterq = pars.GlobalFilter->getq();
        float filterFreqTracking = pars.GlobalFilter->getfreqtracking(note.freq);
        float globalfilterpitch = filterCenterPitch + globalFilterEnvelope->envout();
        float filterfreq = globalfilterpitch + ctl.filtercutoff.relfreq + filterFreqTracking;
        filterfreq = globalFilterL->getrealfreq(filterfreq);

        globalFilterL->setfreq_and_q(filterfreq, filtercenterq * ctl.filterq.relq);
        if (globalFilterR != NULL)
            globalFilterR->setfreq_and_q(filterfreq, filtercenterq * ctl.filterq.relq);
    }
}


// Note Output
void SUBnote::noteout(float *outl, float *outr)
{
    assert(tmpsmp.get() == synth.getRuntime().genTmp1.get());
    assert(tmprnd.get() == synth.getRuntime().genTmp2.get());
    memset(outl, 0, synth.sent_bufferbytes);
    memset(outr, 0, synth.sent_bufferbytes);
    if (noteStatus == NOTE_DISABLED) return;

    if (subNoteChange.checkUpdated())
    {
        realfreq = computeRealFreq();
        computeNoteParameters();
    }

    // left channel
    for (int i = 0; i < synth.sent_buffersize; ++i)
        tmprnd[i] = synth.numRandom() * 2.0f - 1.0f;
    for (int n = 0; n < numharmonics; ++n)
    {
        float rolloff = overtone_rolloff[n];
        memcpy(tmpsmp.get(), tmprnd.get(), synth.sent_bufferbytes);
        for (int nph = 0; nph < numstages; ++nph)
            filter(lfilter[nph + n * numstages], tmpsmp.get());
        for (int i = 0; i < synth.sent_buffersize; ++i)
            outl[i] += tmpsmp[i] * rolloff;
    }

    if (globalFilterL != NULL)
        globalFilterL->filterout(outl);

    // right channel
    if (stereo)
    {
        for (int i = 0; i < synth.sent_buffersize; ++i)
            tmprnd[i] = synth.numRandom() * 2.0f - 1.0f;
        for (int n = 0; n < numharmonics; ++n)
        {
            float rolloff = overtone_rolloff[n];
            memcpy(tmpsmp.get(), tmprnd.get(), synth.sent_bufferbytes);
            for (int nph = 0; nph < numstages; ++nph)
                filter(rfilter[nph + n * numstages], tmpsmp.get());
            for (int i = 0; i < synth.sent_buffersize; ++i)
                outr[i] += tmpsmp[i] * rolloff;
        }
        if (globalFilterR != NULL)
            globalFilterR->filterout(outr);
    }
    else
        memcpy(outr, outl, synth.sent_bufferbytes);

    if (firsttick)
    {
        int n = 10;
        if (n > synth.sent_buffersize)
            n = synth.sent_buffersize;
        for (int i = 0; i < n; ++i)
        {
            float ampfadein = 0.5f - 0.5f * cosf((float)i / (float)n * PI);
            outl[i] *= ampfadein;
            outr[i] *= ampfadein;
        }
        firsttick = 0;
    }


    float pangainL = pars.pangainL; // assume non random pan
    float pangainR = pars.pangainR;
    if (pars.PRandom)
    {
        pangainL = randpanL;
        pangainR = randpanR;
    }

    if (aboveAmplitudeThreshold(oldamplitude, newamplitude))
    {
        // Amplitude interpolation
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            float tmpvol = interpolateAmplitude(oldamplitude, newamplitude, i,
                                                synth.sent_buffersize);
            outl[i] *= tmpvol * pangainL;
            outr[i] *= tmpvol * pangainR;
        }
    }
    else
    {
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            outl[i] *= newamplitude * pangainL;
            outr[i] *= newamplitude * pangainR;
        }
    }
    oldamplitude = newamplitude;
    computecurrentparameters();

    // Apply legato fading if any
    if (legatoFadeStep != 0.0f)
    {
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            legatoFade += legatoFadeStep;
            if (legatoFade <= 0.0f)
            {
                legatoFade = 0.0f;
                legatoFadeStep = 0.0f;
                memset(outl + i, 0, (synth.sent_buffersize - i) * sizeof(float));
                memset(outr + i, 0, (synth.sent_buffersize - i) * sizeof(float));
                killNote(); // NOTE_DISABLED
                return;
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

    // Check if the note needs to be computed more
    if (ampEnvelope->finished() != 0)
    {
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {   // fade-out
            float tmp = 1.0f - (float)i / synth.sent_buffersize_f;
            outl[i] *= tmp;
            outr[i] *= tmp;
        }
        killNote();
        return;
    }
}


// Release Key (Note Off)
void SUBnote::releasekey()
{
    if (noteStatus == NOTE_LEGATOFADEOUT)
        return; // keep envelopes in sustained state (thereby blocking NoteOff)

    ampEnvelope->releasekey();
    if (freqEnvelope)
        freqEnvelope->releasekey();
    if (bandWidthEnvelope)
        bandWidthEnvelope->releasekey();
    if (globalFilterEnvelope)
        globalFilterEnvelope->releasekey();
}


float SUBnote::getHgain(int harmonic)
{
    if (pars.Phmag[pos[harmonic]] == 0)
        return 0.0f;

    float hmagnew = 1.0f - pars.Phmag[pos[harmonic]] / 127.0f;
    float hgain;

    switch (pars.Phmagtype)
    {
        case 1:
            hgain = powFrac<100>(hmagnew);
            break;

        case 2:
            hgain = powFrac<1000>(hmagnew);
            break;

        case 3:
            hgain = powFrac<10000>(hmagnew);
            break;

        case 4:
            hgain = powFrac<100000>(hmagnew);
            break;

        default:
            hgain = 1.0f - hmagnew;
            break;
    }

    return hgain;
}

void SUBnote::updatefilterbank()
{
    int createdFilters = createNewFilters();

    // moved from noteon
    // how much the amplitude is normalised (because the harmonics)
    float reduceamp = 0.0;

    for (int n = 0; n < numharmonics; ++n)
    {
        float freq =  realfreq * pars.POvertoneFreqMult[pos[n]];
        overtone_freq[n] = freq;
        overtone_rolloff[n] = computerolloff(freq);

        // the bandwidth is not absolute(Hz); it is relative to frequency
        float bw = power<10>((pars.Pbandwidth - 127.0f) / 127.0f * 4.0f) * numstages;

        // Bandwidth Scale
        bw *= powf(1000.0f / freq, (pars.Pbwscale - 64.0f) / 64.0f * 3.0f);

        // Relative BandWidth
        bw *= power<100>((pars.Phrelbw[pos[n]] - 64.0f) / 64.0f);

        if (bw > 25.0f)
            bw = 25.0f;

        // try to keep same amplitude on all freqs and bw. (empirically)
        float gain = sqrtf(1500.0f / (bw * freq));

        float hgain = getHgain(n);

        gain *= hgain;
        reduceamp += hgain;

        for (int nph = 0; nph < numstages; ++nph)
        {
            float amp = 1.0f;
            if (nph == 0)
                amp = gain;
            bpfilter *filter = &lfilter[nph + n * numstages];
            filter->amp = amp;
            filter->freq = freq + offsetHz;
            filter->bw = bw;
            if (stereo)
            {
                filter = &rfilter[nph + n * numstages];
                filter->amp = amp;
                filter->freq = freq + offsetHz;
                filter->bw = bw;
            }
        }
    }

    initfilters(numharmonics - createdFilters);
    computeallfiltercoefs();

    if (reduceamp < 0.001f)
        reduceamp = 1.0f;
    volume /= reduceamp;
}

