/*
    PADnote.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011 Alan Calvert
    Copyright 2017-2019 Will Godfrey & others
    Copyright 2020 Kristian Amlie
    Copyright 2022 Ichthyostega

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
#include "Params/PADStatus.h"
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


PADnote::~PADnote() { }


PADnote::PADnote(PADnoteParameters& parameters, Controller& ctl_, Note note_, bool portamento_)
    : synth{*parameters.getSynthEngine()}
    , pars{parameters}
    , padSynthUpdate{parameters}
    , ctl{ctl_}
    , noteStatus{NOTE_ENABLED}
    , waveInterpolator{}   // will be installed in computeNoteParameters()
    , note{note_}
    , realfreq{note.freq}
    , BendAdjust{1}
    , OffsetHz{0}
    , firsttime{true}
    , released{false}
    , portamento{portamento_}
    , globaloldamplitude{0}
    , globalnewamplitude{0}
    , randpanL{0.7}
    , randpanR{0.7}
    , legatoFade{1.0f}      // Full volume
    , legatoFadeStep{0.0f}  // Legato disabled
{
    setupBaseFreq();
    setRandomPan(synth.numRandom(),
                 randpanL, randpanR,
                 synth.getRuntime().panLaw,
                 pars.PPanning, pars.PWidth);

    noteGlobal.fadeinAdjustment = pars.Fadein_adjustment / (float)FADEIN_ADJUSTMENT_SCALE;
    noteGlobal.fadeinAdjustment *= noteGlobal.fadeinAdjustment;

    if (pars.PPunchStrength != 0)
    {
        noteGlobal.punch.enabled = true;
        noteGlobal.punch.t = 1.0f; // start from 1.0 and to 0.0
        noteGlobal.punch.initialvalue =
            (power<10>(1.5f * pars.PPunchStrength / 127.0f) - 1.0f)
                    * velF(note.vel, pars.PPunchVelocitySensing);
        float time = power<10>(3.0f * pars.PPunchTime / 127.0f) / 10000.0f; // 0.1 .. 100 ms
        float stretch = powf(440.0f / note.freq, pars.PPunchStretch / 64.0f);
        noteGlobal.punch.dt = 1.0f / (time * synth.samplerate_f * stretch);
    }
    else
        noteGlobal.punch.enabled = false;

    noteGlobal.freqEnvelope.reset(new Envelope{pars.FreqEnvelope.get(), note.freq, &synth});
    noteGlobal.freqLFO     .reset(new LFO{pars.FreqLfo.get(), note.freq, &synth});

    noteGlobal.ampEnvelope .reset(new Envelope{pars.AmpEnvelope.get(), note.freq, &synth});
    noteGlobal.ampLFO      .reset(new LFO{pars.AmpLfo.get(), note.freq, &synth});

    noteGlobal.ampEnvelope->envout_dB(); // discard the first envelope output

    noteGlobal.filterL.reset(new Filter{pars.GlobalFilter.get(), &synth});
    noteGlobal.filterR.reset(new Filter{pars.GlobalFilter.get(), &synth});

    noteGlobal.filterEnvelope.reset(new Envelope{pars.FilterEnvelope.get(), note.freq, &synth});
    noteGlobal.filterLFO     .reset(new LFO{pars.FilterLfo.get(), note.freq, &synth});

    // cause invocation of computeNoteParameter() with next noteout() in Synth-thread (to avoid races)
    padSynthUpdate.forceUpdate();
}


// Copy constructor, used only used for legato (as of 4/2022)
PADnote::PADnote(const PADnote &orig)
    : synth{orig.synth}
    , pars{orig.pars}
    , padSynthUpdate{pars}
    , ctl{orig.ctl}
    , noteStatus{orig.noteStatus}
    , waveInterpolator{WaveInterpolator::clone(orig.waveInterpolator)}  // use wavetable and reading position from orig
    , note{orig.note}
    , realfreq{orig.realfreq}
    , BendAdjust{orig.BendAdjust}
    , OffsetHz{orig.OffsetHz}
    , firsttime{orig.firsttime}
    , released{orig.released}
    , portamento{orig.portamento}
    , globaloldamplitude{orig.globaloldamplitude}
    , globalnewamplitude{orig.globalnewamplitude}
    , randpanL{orig.randpanL}
    , randpanR{orig.randpanR}
    , legatoFade{0.0f}     // initially silent..
    , legatoFadeStep{0.0f} // Legato disabled
{
    auto& gpar = noteGlobal;
    auto& opar = orig.noteGlobal;

    gpar.detune  = opar.detune;
    gpar.volume  = opar.volume;
    gpar.panning = opar.panning;

    gpar.fadeinAdjustment = opar.fadeinAdjustment;
    gpar.punch = opar.punch;

    // Clone all sub components owned by this note
    gpar.freqEnvelope.reset(new Envelope{*opar.freqEnvelope});
    gpar.freqLFO     .reset(new LFO{*opar.freqLFO});
    gpar.ampEnvelope .reset(new Envelope{*opar.ampEnvelope});
    gpar.ampLFO      .reset(new LFO{*opar.ampLFO});

    gpar.filterL.reset(new Filter{*opar.filterL});
    gpar.filterR.reset(new Filter{*opar.filterR});

    gpar.filterEnvelope.reset(new Envelope{*opar.filterEnvelope});
    gpar.filterLFO     .reset(new LFO{*opar.filterLFO});
}


void PADnote::legatoFadeIn(Note note_)
{
    portamento = false; // portamento-legato treated separately
    this->note = note_;
    setupBaseFreq();
    // cause invocation of computeNoteParameter() with next noteout()
    // in Synth-thread (deliberately not called directly, to avoid races)
    padSynthUpdate.forceUpdate();

    legatoFade = 0.0f; // Start crossfade up from volume zero
    legatoFadeStep = synth.fadeStepShort; // Positive steps
}


void PADnote::legatoFadeOut()
{
    legatoFade = 1.0f;     // crossfade down from full volume
    legatoFadeStep = -synth.fadeStepShort; // Negative steps

    // transitory state similar to a released Envelope
    noteStatus = NOTE_LEGATOFADEOUT;
}


void PADnote::performPortamento(Note note_)
{
    portamento = true;
    this->note = note_;
    setupBaseFreq();
    // carry on all other parameters unaltered
}



void PADnote::setupBaseFreq()
{
    if (pars.Pfixedfreq)
    {
        note.freq = 440.0f;
        int fixedfreqET = pars.PfixedfreqET;
        if (fixedfreqET != 0)
        {   // if the frequency varies according the keyboard note
            float exponent = (note.midi - 69.0f) / 12.0f
                              * (power<2>((fixedfreqET - 1) / 63.0f) - 1.0f);
            note.freq *= (fixedfreqET <= 64)? power<2>(exponent)
                                            : power<3>(exponent);
        }
    }
}

inline void PADnote::fadein(float *smps)
{
    int zerocrossings = 0;
    for (int i = 1; i < synth.sent_buffersize; ++i)
        if (smps[i - 1] < 0.0 && smps[i] > 0.0)
            zerocrossings++; // this is only the positive crossings

    float tmp = (synth.sent_buffersize_f - 1.0) / (zerocrossings + 1) / 3.0;
    if (tmp < 8.0)
        tmp = 8.0;
    tmp *= noteGlobal.fadeinAdjustment;

    int n = int(tmp); // how many samples is the fade-in
    if (n > synth.sent_buffersize)
        n = synth.sent_buffersize;
    for (int i = 0; i < n; ++i)
    {   // fade-in
        float tmp = 0.5 - cosf((float)i / (float) n * PI) * 0.5f;
        smps[i] *= tmp;
    }
}


bool PADnote::isWavetableChanged(size_t tableNr)
{
    return not(waveInterpolator
               and waveInterpolator->matches(pars.waveTable[tableNr]));
}


WaveInterpolator* PADnote::buildInterpolator(size_t tableNr)
{
    bool useCubicInterpolation = synth.getRuntime().Interpolation;
    float startPhase = waveInterpolator? waveInterpolator->getCurrentPhase()
                                       : synth.numRandom();

    return WaveInterpolator::create(useCubicInterpolation
                                   ,startPhase
                                   ,pars.PStereo
                                   ,pars.waveTable[tableNr]
                                   ,pars.waveTable.basefreq[tableNr]);
}


WaveInterpolator* PADnote::setupCrossFade(WaveInterpolator* newInterpolator)
{
    if (waveInterpolator and newInterpolator)
    {// typically called from the Synth-thread from an already playing note (=single-threaded)
        auto attachCrossFade = [&]()
        {// Warning: not thread-safe!
            pars.xFade.attachFader();
            PADStatus::mark(PADStatus::FADING, synth.interchange, pars.partID,pars.kitID);
        };
        auto detachCrossFade = [&]()
        {// Warning: not thread-safe!
            pars.xFade.detachFader();
            if (not pars.xFade)
                PADStatus::mark(PADStatus::CLEAN, synth.interchange, pars.partID,pars.kitID);
        };
        auto switchInterpolator = [&](WaveInterpolator* followUpInterpolator)
        {
            waveInterpolator.reset(followUpInterpolator);
        };
        static_assert(PADnoteParameters::XFADE_UPDATE_MAX/1000 * 96000  < std::numeric_limits<size_t>::max(),
                      "cross-fade sample count represented as size_t");
        size_t crossFadeLengthSmps = pars.PxFadeUpdate * synth.samplerate / 1000; // param given in ms
        return WaveInterpolator::createXFader(attachCrossFade
                                             ,detachCrossFade
                                             ,switchInterpolator
                                             ,unique_ptr<WaveInterpolator>{waveInterpolator.release()}
                                             ,unique_ptr<WaveInterpolator>{newInterpolator}
                                             ,crossFadeLengthSmps
                                             ,synth.buffersize);
    }
    else // fallback: no existing Interpolator ==> just install given new one
        return newInterpolator;    // relevant for NoteOn after wavetable rebuild (no waveInterpolator yet)
}


// Setup basic parameters and wavetable for this note instance.
// Warning: should only be called from Synth-thread (not concurrently)
//          to avoid races with wavetable rebuilding and crossfades.
void PADnote::computeNoteParameters()
{
    setupBaseFreq();

    int BendAdj = pars.PBendAdjust - 64;
    if (BendAdj % 24 == 0)
        BendAdjust = BendAdj / 24;
    else
        BendAdjust = BendAdj / 24.0f;
    float offset_val = (pars.POffsetHz - 64)/64.0f;
    OffsetHz = 15.0f*(offset_val * sqrtf(fabsf(offset_val)));

    noteGlobal.detune = getDetune(pars.PDetuneType, pars.PCoarseDetune, pars.PDetune);

    // find wavetable closest to current note frequency
    float logfreq = logf(note.freq * power<2>(noteGlobal.detune / 1200.0f));
    float mindist = fabsf(logfreq - logf(pars.waveTable.basefreq[0] + 0.0001f));
    size_t tableNr = 0;
    // Note: even when empty(silent), tableNr.0 has always a usable basefreq
    for (size_t tab = 1; tab < pars.waveTable.numTables; ++tab)
    {
        float dist = fabsf(logfreq - logf(pars.waveTable.basefreq[tab] + 0.0001f));
        if (dist < mindist)
        {
            tableNr = tab;
            mindist = dist;
        }
    }
    if (isWavetableChanged(tableNr))
    {
        if (pars.xFade and not isLegatoFading())
            waveInterpolator.reset(setupCrossFade(buildInterpolator(tableNr)));
        else
            waveInterpolator.reset(buildInterpolator(tableNr));
    }

    noteGlobal.volume =
        4.0f                                               // +12dB boost (similar on ADDnote, while SUBnote only boosts +6dB)
        * decibel<-60>(1.0f - pars.PVolume / 96.0f)       // -60 dB .. +19.375 dB
        * velF(note.vel, pars.PAmpVelocityScaleFunction); // velocity sensing
}


void PADnote::computecurrentparameters()
{
    float globalpitch =
        0.01 * (noteGlobal.freqEnvelope->envout()
        + noteGlobal.freqLFO->lfoout() * ctl.modwheel.relmod + noteGlobal.detune);
    globaloldamplitude = globalnewamplitude;
    globalnewamplitude = noteGlobal.volume
        * noteGlobal.ampEnvelope->envout_dB()
        * noteGlobal.ampLFO->amplfoout();

    float filterCenterPitch =
        pars.GlobalFilter->getfreq() + // center freq
            pars.PFilterVelocityScale / 127.0 * 6.0
            * (velF(note.vel, pars.PFilterVelocityScaleFunction) - 1); // velocity sensing

    float filterQ = pars.GlobalFilter->getq();
    float filterFreqTracking = pars.GlobalFilter->getfreqtracking(note.freq);

    float globalfilterpitch =
        noteGlobal.filterEnvelope->envout() + noteGlobal.filterLFO->lfoout()
        + filterCenterPitch;

    float tmpfilterfreq =
        globalfilterpitch + ctl.filtercutoff.relfreq + filterFreqTracking;

    tmpfilterfreq = noteGlobal.filterL->getrealfreq(tmpfilterfreq);

    float globalfilterq = filterQ * ctl.filterq.relq;
    globalfilterq *= pars.randWalkFilterFreq.getFactor();
    noteGlobal.filterL->setfreq_and_q(tmpfilterfreq,globalfilterq);
    noteGlobal.filterR->setfreq_and_q(tmpfilterfreq,globalfilterq);

    // compute the portamento, if it is used by this note
    float portamentofreqrap = 1.0;
    if (portamento)
    {   // this voice use portamento
        portamentofreqrap = ctl.portamento.freqrap;
        if (ctl.portamento.used == 0)
        {   // the portamento has finished
            portamento = false; // this note is no longer "portamented"
        }
    }

    realfreq = note.freq * portamentofreqrap * power<2>(globalpitch / 12.0)
               * powf(ctl.pitchwheel.relfreq, BendAdjust) + OffsetHz;
    realfreq *= pars.randWalkDetune.getFactor();
}



void PADnote::noteout(float *outl,float *outr)
{
    pars.activate_wavetable();
    if (padSynthUpdate.checkUpdated())
        computeNoteParameters();
    computecurrentparameters();
    if (not waveInterpolator
         or noteStatus == NOTE_DISABLED)
        return;

    waveInterpolator->caculateSamples(outl,outr, realfreq,
                                      synth.sent_buffersize);
    if (firsttime)
    {
        fadein(outl);
        fadein(outr);
        globaloldamplitude = globalnewamplitude;
        // avoid triggering amplitude interpolation at first buffer cycle
        firsttime = false;
    }

    noteGlobal.filterL->filterout(outl);
    noteGlobal.filterR->filterout(outr);

    // Apply the punch
    if (noteGlobal.punch.enabled)
    {
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            float punchamp =
                noteGlobal.punch.initialvalue * noteGlobal.punch.t + 1.0;
            outl[i] *= punchamp;
            outr[i] *= punchamp;
            noteGlobal.punch.t -= noteGlobal.punch.dt;
            if (noteGlobal.punch.t < 0.0)
            {
                noteGlobal.punch.enabled = false;
                break;
            }
        }
    }

    float pangainL = pars.pangainL; // assume non random pan
    float pangainR = pars.pangainR;
    if (pars.PRandom)
    {
        pangainL = randpanL;
        pangainR = randpanR;
    }

    if (aboveAmplitudeThreshold(globaloldamplitude,globalnewamplitude))
    {// interpolate amplitude change
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            float fade = interpolateAmplitude(globaloldamplitude,
                                              globalnewamplitude, i,
                                              synth.sent_buffersize);
            outl[i] *= fade * pangainL;
            outr[i] *= fade * pangainR;
        }
    }
    else
    {
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            outl[i] *= globalnewamplitude * pangainL;
            outr[i] *= globalnewamplitude * pangainR;
        }
    }

    if (isLegatoFading())
    {// apply legato fade to computed samples...
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            legatoFade += legatoFadeStep;
            if (legatoFade <= 0.0f)
            {
                legatoFade = 0.0f;
                legatoFadeStep = 0.0f;
                memset(outl + i, 0, (synth.sent_buffersize - i) * sizeof(float));
                memset(outr + i, 0, (synth.sent_buffersize - i) * sizeof(float));
                noteStatus = NOTE_DISABLED; // causes clean-up of this note instance
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

    // Check global envelope and discard this note when finished.
    if (noteGlobal.ampEnvelope->finished() != 0)
    {
        for (int i = 0 ; i < synth.sent_buffersize; ++i)
        {   // fade-out
            float tmp = 1.0f - (float)i / synth.sent_buffersize_f;
            outl[i] *= tmp;
            outr[i] *= tmp;
        }
        noteStatus = NOTE_DISABLED; // causes clean-up of this note instance
        return;
    }
}


void PADnote::releasekey()
{
    if (noteStatus == NOTE_LEGATOFADEOUT)
        return; // keep envelopes in sustained state (thereby blocking NoteOff)

    noteGlobal.freqEnvelope->releasekey();
    noteGlobal.filterEnvelope->releasekey();
    noteGlobal.ampEnvelope->releasekey();
}

