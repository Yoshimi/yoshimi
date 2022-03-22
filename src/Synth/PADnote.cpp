/*
    PADnote.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011 Alan Calvert
    Copyright 2017-2019 Will Godfrey & others
    Copyright 2020 Kristian Amlie

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

#include "Misc/Config.h"
#include "Params/PADnoteParameters.h"
#include "Params/Controller.h"
#include "Synth/WaveInterpolator.h"
#include "Synth/PADnote.h"
#include "Synth/Envelope.h"
#include "Synth/LFO.h"
#include "DSP/Filter.h"
#include "DSP/FFTwrapper.h"
#include "Params/Controller.h"
#include "Misc/SynthEngine.h"
#include "Misc/SynthHelper.h"
#include "Misc/NumericFuncs.h"

#include <limits>
#include <memory>
#include <cmath>

using func::decibel;
using func::power;
using synth::velF;
using synth::getDetune;
using synth::interpolateAmplitude;
using synth::aboveAmplitudeThreshold;
using func::setRandomPan;
using std::unique_ptr;


PADnote::PADnote(PADnoteParameters *parameters, Controller *ctl_, float freq,
    float velocity, int portamento_, int midinote_, SynthEngine *_synth) :
    NoteStatus(NOTE_ENABLED),
    pars(parameters),
    waveInterpolator{}, // will be installed in computeNoteParameters()
    firsttime(true),
    released(false),
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
            (power<10>(1.5f * pars->PPunchStrength / 127.0f) - 1.0f)
                    * velF(velocity, pars->PPunchVelocitySensing);
        float time = power<10>(3.0f * pars->PPunchTime / 127.0f) / 10000.0f; // 0.1 .. 100 ms
        float stretch = powf(440.0f / freq, pars->PPunchStretch / 64.0f);
        NoteGlobalPar.Punch.dt = 1.0f / (time * synth->samplerate_f * stretch);
    }
    else
        NoteGlobalPar.Punch.Enabled = 0;

    NoteGlobalPar.FreqEnvelope = new Envelope(pars->FreqEnvelope.get(), basefreq, synth);
    NoteGlobalPar.FreqLfo = new LFO(pars->FreqLfo.get(), basefreq, synth);

    NoteGlobalPar.AmpEnvelope = new Envelope(pars->AmpEnvelope.get(), basefreq, synth);
    NoteGlobalPar.AmpLfo = new LFO(pars->AmpLfo.get(), basefreq, synth);

    NoteGlobalPar.AmpEnvelope->envout_dB(); // discard the first envelope output

    NoteGlobalPar.GlobalFilterL =
        new Filter(pars->GlobalFilter.get(), synth);
    NoteGlobalPar.GlobalFilterR =
        new Filter(pars->GlobalFilter.get(), synth);

    NoteGlobalPar.FilterEnvelope = new Envelope(pars->FilterEnvelope.get(), basefreq, synth);
    NoteGlobalPar.FilterLfo = new LFO(pars->FilterLfo.get(), basefreq, synth);

    computeNoteParameters();

    globaloldamplitude =
        globalnewamplitude = NoteGlobalPar.Volume
        * NoteGlobalPar.AmpEnvelope->envout_dB()
        * NoteGlobalPar.AmpLfo->amplfoout();
}


// Copy constructor, currently only used for legato
PADnote::PADnote(const PADnote &orig) :
    // For legato. Move this somewhere else if copying
    // notes gets used for another purpose
    NoteStatus(NOTE_KEEPALIVE),
    pars(orig.pars),
    waveInterpolator{WaveInterpolator::clone(*orig.waveInterpolator)},  // use wavetable and reading position from orig
    basefreq(orig.basefreq),
    BendAdjust(orig.BendAdjust),
    OffsetHz(orig.OffsetHz),
    firsttime(orig.firsttime),
    released(orig.released),
    portamento(orig.portamento),
    midinote(orig.midinote),
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
    auto& gpar = NoteGlobalPar;
    auto& oldgpar = orig.NoteGlobalPar;

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

    waveInterpolator.reset(WaveInterpolator::clone(*orig.waveInterpolator));
    basefreq = orig.basefreq;
    BendAdjust = orig.BendAdjust;
    OffsetHz = orig.OffsetHz;
    firsttime = orig.firsttime;
    released = orig.released;
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
                              * (power<2>((fixedfreqET - 1) / 63.0f) - 1.0f);
            if (fixedfreqET <= 64)
                basefreq *= power<2>(tmp);
            else
                basefreq *= power<3>(tmp);
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


bool PADnote::isWavetableChanged(size_t tableNr)
{
    return not(waveInterpolator
               and waveInterpolator->matches(pars->waveTable[tableNr]));
}


WaveInterpolator* PADnote::buildInterpolator(size_t tableNr)
{
    bool useCubicInterpolation = synth->getRuntime().Interpolation;
    float startPhase = waveInterpolator? waveInterpolator->getCurrentPhase()
                                       : synth->numRandom();

    return WaveInterpolator::create(useCubicInterpolation
                                   ,startPhase
                                   ,pars->PStereo
                                   ,pars->waveTable[tableNr]
                                   ,pars->waveTable.basefreq[tableNr]);
}


WaveInterpolator* PADnote::setupCrossFade(WaveInterpolator* newInterpolator)
{
    if (waveInterpolator and newInterpolator)
    {
        auto attachCrossFade = [&]()
        {
            pars->xFade.attachFader();
            std::cout << "XFade-ATTACH.. Freq="<<basefreq<<" PADnote "<<this<<std::endl;        ////////////////TODO padthread debugging output
        };
        auto detachCrossFade = [&]()
        {
            std::cout << "XFade-DETACH.. Freq="<<basefreq<<" PADnote "<<this<<std::endl;        ////////////////TODO padthread debugging output
            pars->xFade.detachFader();
        };
        auto switchInterpolator = [&](WaveInterpolator* followUpInterpolator)
        {
            std::cout << "XFade-COMPLETE Freq="<<basefreq<<" PADnote "<<this<<std::endl;        ////////////////TODO padthread debugging output
            waveInterpolator.reset(followUpInterpolator);
        };
        static_assert(PADnoteParameters::XFADE_UPDATE_MAX/1000 * 96000  < std::numeric_limits<size_t>::max(),
                      "cross-fade sample count represented as size_t");
        size_t crossFadeLengthSmps = pars->PxFadeUpdate * synth->samplerate / 1000; // param given in ms
        WaveInterpolator* oldInterpolator = waveInterpolator.release();
        WaveInterpolator* xFader = WaveInterpolator::createXFader(attachCrossFade
                                                                 ,detachCrossFade
                                                                 ,switchInterpolator
                                                                 ,unique_ptr<WaveInterpolator>{oldInterpolator}
                                                                 ,unique_ptr<WaveInterpolator>{newInterpolator}
                                                                 ,crossFadeLengthSmps
                                                                 ,synth->buffersize);
        return xFader;
    }
    else // fallback: no existing Interpolator ==> just install given new one
        return newInterpolator;
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

    // find wavetable closest to current note frequency
    float logfreq = logf(basefreq * power<2>(NoteGlobalPar.Detune / 1200.0f));
    float mindist = fabsf(logfreq - logf(pars->waveTable.basefreq[0] + 0.0001f));
    size_t tableNr = 0;
    // Note: even when empty(silent), tableNr.0 has always a usable basefreq
    for (size_t tab = 1; tab < pars->waveTable.numTables; ++tab)
    {
        float dist = fabsf(logfreq - logf(pars->waveTable.basefreq[tab] + 0.0001f));
        if (dist < mindist)
        {
            tableNr = tab;
            mindist = dist;
        }
    }
    if (isWavetableChanged(tableNr))
    {
        if (pars->xFade)
            waveInterpolator.reset(setupCrossFade(buildInterpolator(tableNr)));
        else
            waveInterpolator.reset(buildInterpolator(tableNr));
    }

    NoteGlobalPar.Volume =
        4.0f                                               // +12dB boost (similar on ADDnote, while SUBnote only boosts +6dB)
        * decibel<-60>(1.0f - pars->PVolume / 96.0f)       // -60 dB .. +19.375 dB
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

    realfreq = basefreq * portamentofreqrap * power<2>(globalpitch / 12.0)
               * powf(ctl->pitchwheel.relfreq, BendAdjust) + OffsetHz;
}



void PADnote::noteout(float *outl,float *outr)
{
    pars->activate_wavetable();
    if (padSynthUpdate.checkUpdated())
        computeNoteParameters();
    computecurrentparameters();
    if (not waveInterpolator
         or NoteStatus == NOTE_DISABLED)
        return;

    waveInterpolator->caculateSamples(outl,outr, realfreq,
                                      synth->sent_buffersize);
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
}


void PADnote::releasekey()
{
    NoteGlobalPar.FreqEnvelope->releasekey();
    NoteGlobalPar.FilterEnvelope->releasekey();
    NoteGlobalPar.AmpEnvelope->releasekey();
    if (NoteStatus == NOTE_KEEPALIVE)
        NoteStatus = NOTE_ENABLED;
}
