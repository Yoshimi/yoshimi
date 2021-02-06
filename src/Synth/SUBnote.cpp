/*

    SUBnote.cpp - The "subtractive" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others
    Copyright 2020 Kristian Amlie & others

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
#include <iostream>

#include "DSP/FFTwrapper.h"
#include "Params/SUBnoteParameters.h"
#include "Params/Controller.h"
#include "Synth/Envelope.h"
#include "DSP/Filter.h"
#include "Misc/SynthEngine.h"
#include "Misc/SynthHelper.h"
#include "Synth/SUBnote.h"
#include "Misc/NumericFuncs.h"

using synth::velF;
using synth::getDetune;
using synth::interpolateAmplitude;
using synth::aboveAmplitudeThreshold;

using func::setRandomPan;

// These have little reason to exist, as GCC actually performs constant folding
// on logf even on -O0 and Clang (currently, as of 9.0.1) constant-folds these
// on -O1 and above. These used to be members of SUBnote initialized on note
// construction. Thankfully constant folding would still occur, but it wasn't
// ideal. Older compilers might generate library calls, so there might still be
// some justification for this.
const float LOG_0_01 = logf(0.01f);
const float LOG_0_001 = logf(0.001f);
const float LOG_0_0001 = logf(0.0001f);
const float LOG_0_00001 = logf(0.00001f);


SUBnote::SUBnote(SUBnoteParameters *parameters, Controller *ctl_, float basefreq_,
                 float velocity_, int portamento_, int midinote_, SynthEngine *_synth) :
    pars(parameters),
    velocity(velocity_ > 1.0f ? 1.0f : velocity_),
    portamento(portamento_),
    midinote(midinote_),
    GlobalFilterL(NULL),
    GlobalFilterR(NULL),
    GlobalFilterEnvelope(NULL),
    ctl(ctl_),
    subNoteChange(parameters),
    synth(_synth),
    filterStep(0)
{
    // Initialise some legato-specific vars
    legatoFade = 1.0f; // Full volume
    legatoFadeStep = 0.0f; // Legato disabled

    NoteStatus = NOTE_ENABLED;

    numstages = pars->Pnumstages;
    stereo = pars->Pstereo;
    start = pars->Pstart;
    firsttick = 1;

    setRandomPan(synth->numRandom(), randpanL, randpanR, synth->getRuntime().panLaw, pars->PPanning, pars->PWidth);

    numharmonics = 0;
    lfilter = NULL;
    rfilter = NULL;

    basefreq = basefreq_;
    computeNoteFreq();

    oldpitchwheel = 0;
    oldbandwidth = 64;

    if (pars->Pfixedfreq == 0)
        initparameters(notefreq);
    else
        initparameters(notefreq / 440.0f * basefreq);

    computeNoteParameters();
    computecurrentparameters();

    oldamplitude = newamplitude;
}


// Copy constructor, currently only exists for legato
SUBnote::SUBnote(const SUBnote &orig) :
    pars(orig.pars),
    stereo(orig.stereo),
    numstages(orig.numstages),
    numharmonics(orig.numharmonics),
    start(orig.start),
    basefreq(orig.basefreq),
    notefreq(orig.notefreq),
    velocity(orig.velocity),
    portamento(orig.portamento),
    midinote(orig.midinote),
    BendAdjust(orig.BendAdjust),
    OffsetHz(orig.OffsetHz),
    randpanL(orig.randpanL),
    randpanR(orig.randpanR),
    FreqEnvelope(NULL),
    BandWidthEnvelope(NULL),
    GlobalFilterL(NULL),
    GlobalFilterR(NULL),
    GlobalFilterEnvelope(NULL),
    // For legato. Move this somewhere else if copying
    // notes gets used for another purpose
    NoteStatus(NOTE_KEEPALIVE),
    firsttick(orig.firsttick),
    volume(orig.volume),
    oldamplitude(orig.oldamplitude),
    newamplitude(orig.newamplitude),
    lfilter(NULL),
    rfilter(NULL),
    ctl(orig.ctl),
    oldpitchwheel(orig.oldpitchwheel),
    oldbandwidth(orig.oldbandwidth),
    legatoFade(0.0f), // Silent by default
    legatoFadeStep(0.0f), // Legato disabled
    subNoteChange(pars),
    synth(orig.synth),
    filterStep(orig.filterStep)
{
    memcpy(pos, orig.pos, MAX_SUB_HARMONICS * sizeof(int));
    memcpy(overtone_rolloff, orig.overtone_rolloff,
        numharmonics * sizeof(float));
    memcpy(overtone_freq, orig.overtone_freq,
        numharmonics * sizeof(float));

    AmpEnvelope = new Envelope(*orig.AmpEnvelope);

    if (orig.FreqEnvelope != NULL)
        FreqEnvelope = new Envelope(*orig.FreqEnvelope);
    if (orig.BandWidthEnvelope != NULL)
        BandWidthEnvelope = new Envelope(*orig.BandWidthEnvelope);
    if (pars->PGlobalFilterEnabled != 0)
    {
        GlobalFilterL = new Filter(*orig.GlobalFilterL);
        GlobalFilterR = new Filter(*orig.GlobalFilterR);
        GlobalFilterEnvelope = new Envelope(*orig.GlobalFilterEnvelope);
    }

    if (orig.lfilter != NULL)
    {
        lfilter = new bpfilter[numstages * numharmonics];
        memcpy(lfilter, orig.lfilter,
            numstages * numharmonics * sizeof(bpfilter));
    }
    if (orig.rfilter != NULL)
    {
        rfilter = new bpfilter[numstages * numharmonics];
        memcpy(rfilter, orig.rfilter,
            numstages * numharmonics * sizeof(bpfilter));
    }
}


void SUBnote::legatoFadeIn(float basefreq_, float velocity_, int portamento_, int midinote_)
{
    velocity = velocity_ > 1.0f ? 1.0f : velocity_;
    portamento = portamento_;
    midinote = midinote_;

    basefreq = basefreq_;
    computeNoteFreq();

    if (!portamento) // Do not crossfade portamento
    {
        legatoFade = 0.0f; // Start silent
        legatoFadeStep = synth->fadeStepShort; // Positive steps

        // I'm not sure if these are necessary or even beneficial
        oldpitchwheel = 0;
        oldbandwidth = 64;
        oldamplitude = newamplitude;

    }

    computeNoteParameters();
}


void SUBnote::legatoFadeOut(const SUBnote &orig)
{
    velocity = orig.velocity;
    portamento = orig.portamento;
    midinote = orig.midinote;

    firsttick = orig.firsttick;
    volume = orig.volume;

    basefreq = orig.basefreq;
    notefreq = orig.notefreq;

    // Not sure if this is necessary
    oldamplitude = orig.oldamplitude;
    newamplitude = orig.newamplitude;

    // AmpEnvelope should never be null
    *AmpEnvelope = *orig.AmpEnvelope;

    if (orig.FreqEnvelope != NULL)
        *FreqEnvelope = *orig.FreqEnvelope;
    if (orig.BandWidthEnvelope != NULL)
        *BandWidthEnvelope = *orig.BandWidthEnvelope;
    if (pars->PGlobalFilterEnabled)
    {
        *GlobalFilterEnvelope = *orig.GlobalFilterEnvelope;

        // Supporting virtual copy assignment would be hairy
        // so we have to use the copy constructor here
        delete GlobalFilterL;
        GlobalFilterL = new Filter(*orig.GlobalFilterL);
        delete GlobalFilterR;
        GlobalFilterR = new Filter(*orig.GlobalFilterR);
    }

    // This assumes that numstages and numharmonics don't change
    // while notes exist, or if they do change, they change for
    // all notes equally and simultaneously. If this is ever not
    // the case, this code needs to be changed.
    if (orig.lfilter != NULL)
    {
        memcpy(lfilter, orig.lfilter,
            numstages * numharmonics * sizeof(bpfilter));
    }
    if (orig.rfilter != NULL)
    {
        memcpy(rfilter, orig.rfilter,
            numstages * numharmonics * sizeof(bpfilter));
    }

    memcpy(overtone_rolloff, orig.overtone_rolloff,
        numharmonics * sizeof(float));
    memcpy(overtone_freq, orig.overtone_freq,
        numharmonics * sizeof(float));

    legatoFade = 1.0f; // Start at full volume
    legatoFadeStep = -synth->fadeStepShort; // Negative steps
}


SUBnote::~SUBnote()
{
    KillNote();
}


// Kill the note
void SUBnote::KillNote(void)
{
    if (NoteStatus != NOTE_DISABLED)
    {
        delete [] lfilter;
        lfilter = NULL;
        if (stereo)
            delete [] rfilter;
        rfilter = NULL;
        delete AmpEnvelope;
        if (FreqEnvelope != NULL)
            delete FreqEnvelope;
        if (BandWidthEnvelope != NULL)
            delete BandWidthEnvelope;
        NoteStatus = NOTE_DISABLED;
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
        if (pars->Phmag[n] == 0 || alreadyEnabled[n])
            continue;
        if (n * notefreq > synth->halfsamplerate_f)
            break; // remove the freqs above the Nyquist freq
        pos[numharmonics++] = n;
        alreadyEnabled[n] = true;
    }

    if (numharmonics == origNumHarmonics)
        return 0;

    bpfilter *newFilter = new bpfilter[numstages * numharmonics];
    if (lfilter != NULL)
    {
        memcpy(newFilter, lfilter, numstages * origNumHarmonics * sizeof(bpfilter));
        delete [] lfilter;
    }
    lfilter = newFilter;
    if (stereo != 0)
    {
        newFilter = new bpfilter[numstages * numharmonics];
        if (rfilter != NULL)
        {
            memcpy(newFilter, rfilter, numstages * origNumHarmonics * sizeof(bpfilter));
            delete [] rfilter;
        }
        rfilter = newFilter;
    }

    return numharmonics - origNumHarmonics;
}

void SUBnote::computeNoteFreq()
{
    if (pars->Pfixedfreq == 0)
        notefreq = basefreq;
    else
    {
        notefreq = 440.0f;
        int fixedfreqET = pars->PfixedfreqET;
        if (fixedfreqET)
        {   // if the frequency varies according the keyboard note
            float tmp =
                (midinote - 69.0f) / 12.0f * powf(2.0f, (((fixedfreqET - 1) / 63.0f) - 1.0f));
            if (fixedfreqET <= 64)
                notefreq *= powf(2.0f, tmp);
            else
                notefreq *= powf(3.0f, tmp);
        }
    }

    float detune = getDetune(pars->PDetuneType, pars->PCoarseDetune, pars->PDetune);
    notefreq *= powf(2.0f, detune / 1200.0f); // detune
//    notefreq*=ctl->pitchwheel.relfreq;//pitch wheel
}

void SUBnote::computeNoteParameters()
{
    volume = powf(0.1f, 3.0f * (1.0f - pars->PVolume / 96.0f)); // -60 dB .. 0 dB
    volume *= velF(velocity, pars->PAmpVelocityScaleFunction);

    int BendAdj = pars->PBendAdjust - 64;
    if (BendAdj % 24 == 0)
        BendAdjust = BendAdj / 24;
    else
        BendAdjust = BendAdj / 24.0f;
    float offset_val = (pars->POffsetHz - 64)/64.0f;
    OffsetHz = 15.0f*(offset_val * sqrtf(fabsf(offset_val)));

    updatefilterbank();
}

// Compute the filters coefficients
void SUBnote::computefiltercoefs(bpfilter &filter, float freq, float bw, float gain)
{
    if (freq > synth->halfsamplerate_f - 200.0f)
    {
        freq = synth->halfsamplerate_f - 200.0f;
    }

    float omega = TWOPI * freq / synth->samplerate_f;
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
        float p = synth->numRandom() * TWOPI;
        if (start == 1)
            a *= synth->numRandom();
        filter.yn1 = a * cosf(p);
        filter.yn2 = a * cosf(p + filter.freq * TWOPI / synth->samplerate_f);

        // correct the error of computation the start amplitude
        // at very high frequencies
        if (filter.freq > synth->samplerate_f * 0.96f)
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
    if (synth->getIsLV2Plugin()){
        filterVarRun(filter, smps);
        return;
    }

    int remainder = synth->sent_buffersize % 8;
    int blocksize = synth->sent_buffersize - remainder;
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
    int runLength = synth->sent_buffersize;
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

    for (; i < synth->sent_buffersize; ++i){
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
    AmpEnvelope = new Envelope(pars->AmpEnvelope, freq, synth);
    if (pars->PFreqEnvelopeEnabled != 0)
        FreqEnvelope = new Envelope(pars->FreqEnvelope, freq, synth);
    else
        FreqEnvelope = NULL;
    if (pars->PBandWidthEnvelopeEnabled != 0)
        BandWidthEnvelope = new Envelope(pars->BandWidthEnvelope, freq, synth);
    else
        BandWidthEnvelope = NULL;
    if (pars->PGlobalFilterEnabled != 0)
    {
        GlobalFilterL = new Filter(pars->GlobalFilter, synth);
        if (stereo != 0)
            GlobalFilterR = new Filter(pars->GlobalFilter, synth);
        GlobalFilterEnvelope = new Envelope(pars->GlobalFilterEnvelope, freq, synth);
    }
}
//end of port


// Compute how much to reduce amplitude near nyquist or subaudible frequencies.
float SUBnote::computerolloff(float freq)
{
    const float lower_limit = 10.0f;
    const float lower_width = 10.0f;
    const float upper_width = 200.0f;
    float upper_limit = synth->samplerate / 2.0f;

    if (freq > lower_limit + lower_width &&
            freq < upper_limit - upper_width)
        return 1.0f;
    if (freq <= lower_limit || freq >= upper_limit)
        return 0.0f;
    if (freq <= lower_limit + lower_width)
        return (1.0f - cosf(M_PI * (freq - lower_limit) / lower_width)) / 2.0f;
    return (1.0f - cosf(M_PI * (freq - upper_limit) / upper_width)) / 2.0f;
}

void SUBnote::computeallfiltercoefs()
{
    float envfreq = 1.0f;
    float envbw = 1.0f;
    float gain = 1.0f;

    if (FreqEnvelope != NULL)
    {
        envfreq = FreqEnvelope->envout() / 1200;
        envfreq = powf(2.0f, envfreq);
    }

    envfreq *= powf(ctl->pitchwheel.relfreq, BendAdjust); // pitch wheel

    if (portamento != 0)
    {   // portamento is used
        envfreq *= ctl->portamento.freqrap;
        if (ctl->portamento.used == 0)
        {   // the portamento has finished
            portamento = 0; // this note is no longer "portamented"
        }
    }

    if (BandWidthEnvelope != NULL)
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
    oldbandwidth = ctl->bandwidth.data;
    oldpitchwheel = ctl->pitchwheel.data;
}

// Compute Parameters of SUBnote for each tick
void SUBnote::computecurrentparameters(void)
{
    // disabled till we know what we are doing!
    /*for (int n = 0; n < MAX_SUB_HARMONICS; ++n)
    {
        int changed = pars->PfilterChanged[n];
        if (changed)
        {
            if (changed == 6) // magnitude
                ;
            else if (changed == 7) // bandwidth
                ;
            cout << "Filter changed " << changed << endl;
            pars->PfilterChanged[n] = 0;
        }
    }*/
    if (FreqEnvelope != NULL
        || BandWidthEnvelope != NULL
        || oldpitchwheel != ctl->pitchwheel.data
        || oldbandwidth != ctl->bandwidth.data
        || portamento != 0)
        computeallfiltercoefs();
    newamplitude = volume * AmpEnvelope->envout_dB() * 2.0f;

    // Filter
    if (GlobalFilterL != NULL)
    {
        float filterCenterPitch =
            pars->GlobalFilter->getfreq()
            + // center freq
            (pars->PGlobalFilterVelocityScale / 127.0f * 6.0f)
            * // velocity sensing
            (velF(velocity, pars->PGlobalFilterVelocityScaleFunction) - 1);
        float filtercenterq = pars->GlobalFilter->getq();
        float filterFreqTracking = pars->GlobalFilter->getfreqtracking(basefreq);
        float globalfilterpitch = filterCenterPitch + GlobalFilterEnvelope->envout();
        float filterfreq = globalfilterpitch + ctl->filtercutoff.relfreq + filterFreqTracking;
        filterfreq = GlobalFilterL->getrealfreq(filterfreq);

        GlobalFilterL->setfreq_and_q(filterfreq, filtercenterq * ctl->filterq.relq);
        if (GlobalFilterR != NULL)
            GlobalFilterR->setfreq_and_q(filterfreq, filtercenterq * ctl->filterq.relq);
    }
}


// Note Output
int SUBnote::noteout(float *outl, float *outr)
{
    tmpsmp = synth->getRuntime().genTmp1;
    tmprnd = synth->getRuntime().genTmp2;
    memset(outl, 0, synth->sent_bufferbytes);
    memset(outr, 0, synth->sent_bufferbytes);
    if (NoteStatus == NOTE_DISABLED)
        return 0;

    if (subNoteChange.checkUpdated())
    {
        computeNoteFreq();
        computeNoteParameters();
    }

    // left channel
    for (int i = 0; i < synth->sent_buffersize; ++i)
        tmprnd[i] = synth->numRandom() * 2.0f - 1.0f;
    for (int n = 0; n < numharmonics; ++n)
    {
        float rolloff = overtone_rolloff[n];
        memcpy(tmpsmp, tmprnd, synth->sent_bufferbytes);
        for (int nph = 0; nph < numstages; ++nph)
            filter(lfilter[nph + n * numstages], tmpsmp);
        for (int i = 0; i < synth->sent_buffersize; ++i)
            outl[i] += tmpsmp[i] * rolloff;
    }

    if (GlobalFilterL != NULL)
        GlobalFilterL->filterout(outl);

    // right channel
    if (stereo)
    {
        for (int i = 0; i < synth->sent_buffersize; ++i)
            tmprnd[i] = synth->numRandom() * 2.0f - 1.0f;
        for (int n = 0; n < numharmonics; ++n)
        {
            float rolloff = overtone_rolloff[n];
            memcpy(tmpsmp, tmprnd, synth->sent_bufferbytes);
            for (int nph = 0; nph < numstages; ++nph)
                filter(rfilter[nph + n * numstages], tmpsmp);
            for (int i = 0; i < synth->sent_buffersize; ++i)
                outr[i] += tmpsmp[i] * rolloff;
        }
        if (GlobalFilterR != NULL)
            GlobalFilterR->filterout(outr);
    }
    else
        memcpy(outr, outl, synth->sent_bufferbytes);

    if (firsttick)
    {
        int n = 10;
        if (n > synth->sent_buffersize)
            n = synth->sent_buffersize;
        for (int i = 0; i < n; ++i)
        {
            float ampfadein = 0.5f - 0.5f * cosf((float)i / (float)n * PI);
            outl[i] *= ampfadein;
            outr[i] *= ampfadein;
        }
        firsttick = 0;
    }


    float pangainL = pars->pangainL; // assume non random pan
    float pangainR = pars->pangainR;
    if (pars->PRandom)
    {
        pangainL = randpanL;
        pangainR = randpanR;
    }

    if (aboveAmplitudeThreshold(oldamplitude, newamplitude))
    {
        // Amplitude interpolation
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float tmpvol = interpolateAmplitude(oldamplitude, newamplitude, i,
                                                synth->sent_buffersize);
            outl[i] *= tmpvol * pangainL;
            outr[i] *= tmpvol * pangainR;
        }
    }
    else
    {
        for (int i = 0; i < synth->sent_buffersize; ++i)
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

    // Check if the note needs to be computed more
    if (AmpEnvelope->finished() != 0)
    {
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {   // fade-out
            float tmp = 1.0f - (float)i / synth->sent_buffersize_f;
            outl[i] *= tmp;
            outr[i] *= tmp;
        }
        KillNote();
    }
    return 1;
}


// Release Key (Note Off)
void SUBnote::releasekey(void)
{
    AmpEnvelope->releasekey();
    if (FreqEnvelope != NULL)
        FreqEnvelope->releasekey();
    if (BandWidthEnvelope != NULL)
        BandWidthEnvelope->releasekey();
    if (GlobalFilterEnvelope != NULL)
        GlobalFilterEnvelope->releasekey();
    if (NoteStatus == NOTE_KEEPALIVE)
        NoteStatus = NOTE_ENABLED;
}

float SUBnote::getHgain(int harmonic)
{
    if (pars->Phmag[pos[harmonic]] == 0)
        return 0.0f;

    float hmagnew = 1.0f - pars->Phmag[pos[harmonic]] / 127.0f;
    float hgain;

    switch (pars->Phmagtype)
    {
        case 1:
            hgain = expf(hmagnew * LOG_0_01);
            break;

        case 2:
            hgain = expf(hmagnew * LOG_0_001);
            break;

        case 3:
            hgain = expf(hmagnew * LOG_0_0001);
            break;

        case 4:
            hgain = expf(hmagnew * LOG_0_00001);
            break;

        default:
            hgain = 1.0f - hmagnew;
    }

    return hgain;
}

void SUBnote::updatefilterbank(void)
{
    int createdFilters = createNewFilters();

    // moved from noteon
    // how much the amplitude is normalised (because the harmonics)
    float reduceamp = 0.0;

    for (int n = 0; n < numharmonics; ++n)
    {
        float freq =  notefreq * pars->POvertoneFreqMult[pos[n]];
        overtone_freq[n] = freq;
        overtone_rolloff[n] = computerolloff(freq);

        // the bandwidth is not absolute(Hz); it is relative to frequency
        float bw = powf(10.0f, (pars->Pbandwidth - 127.0f) / 127.0f * 4.0f) * numstages;

        // Bandwidth Scale
        bw *= powf(1000.0f / freq, (pars->Pbwscale - 64.0f) / 64.0f * 3.0f);

        // Relative BandWidth
        bw *= powf(100.0f, (pars->Phrelbw[pos[n]] - 64.0f) / 64.0f);

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
            filter->freq = freq + OffsetHz;
            filter->bw = bw;
            if (stereo)
            {
                filter = &rfilter[nph + n * numstages];
                filter->amp = amp;
                filter->freq = freq + OffsetHz;
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

