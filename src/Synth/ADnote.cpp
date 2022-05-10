/*
    ADnote.cpp - The "additive" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others
    Copyright 2020-2021 Kristian Amlie & Will Godfrey

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

    This file is derivative of original ZynAddSubFX code.

*/

#include <cmath>
#include <cassert>
#include <iostream>

#include "Synth/Envelope.h"
#include "Synth/ADnote.h"
#include "Synth/LFO.h"
#include "DSP/Filter.h"
#include "Params/ADnoteParameters.h"
#include "Params/Controller.h"
#include "Misc/SynthEngine.h"
#include "Misc/SynthHelper.h"
#include "Misc/NumericFuncs.h"

#include "globals.h"

using func::power;
using func::decibel;
using synth::velF;
using synth::getDetune;
using synth::interpolateAmplitude;
using synth::aboveAmplitudeThreshold;
using func::setRandomPan;

using std::isgreater;


ADnote::ADnoteGlobal::ADnoteGlobal()
    : detune{0.0f}
    , freqEnvelope{}
    , freqLFO{}
    , volume{0.0f}
    , randpanL{0.0f}
    , randpanR{0.0f}
    , fadeinAdjustment{0.0f}
    , ampEnvelope{}
    , ampLFO{}
    , punch{}
    , filterL{}
    , filterR{}
    , filterEnvelope{}
    , filterLFO{}
{ }

ADnote::ADnoteGlobal::ADnoteGlobal(ADnoteGlobal const& o)
    : detune{o.detune}
    , freqEnvelope{}
    , freqLFO{}
    , volume{o.volume}
    , randpanL{o.randpanL}
    , randpanR{o.randpanR}
    , fadeinAdjustment{o.fadeinAdjustment}
    , ampEnvelope{}
    , ampLFO{}
    , punch{o.punch}
    , filterL{}
    , filterR{}
    , filterEnvelope{}
    , filterLFO{}
{
    // Clone all sub components owned by this note
    freqEnvelope.reset(new Envelope{*o.freqEnvelope});
    freqLFO     .reset(new LFO{*o.freqLFO});
    ampEnvelope .reset(new Envelope{*o.ampEnvelope});
    ampLFO      .reset(new LFO{*o.ampLFO});

    filterEnvelope.reset(new Envelope{*o.filterEnvelope});
    filterLFO     .reset(new LFO{*o.filterLFO});

    filterL.reset(new Filter{*o.filterL});
    if (o.filterR)
        filterR.reset(new Filter{*o.filterR});
}



ADnote::~ADnote() { /* all clean-up done automatically */ }


// Internal: this constructor does the actual initialisation....
ADnote::ADnote(ADnoteParameters& adpars_, Controller& ctl_, Note note_, bool portamento_
              ,ADnote* topVoice_, int subVoice_, int phaseOffset, float *parentFMmod_, bool forFM_)
    : synth{*adpars_.getSynthEngine()}
    , adpars{adpars_}
    , paramsUpdate{adpars}
    , ctl{ctl_}
    , note{note_}
    , stereo{adpars.GlobalPar.PStereo}
    , noteStatus{NOTE_ENABLED}
    , tSpot{0}
    , paramRNG{}
    , paramSeed{0}
    , oscposhi{}
    , oscposlo{}
    , oscfreqhi{}
    , oscfreqlo{}
    , oscposhiFM{}
    , oscposloFM{}
    , oscfreqhiFM{}
    , oscfreqloFM{}
    , unison_base_freq_rap{}
    , unison_freq_rap{}
    , unison_invert_phase{}
    , unison_vibrato{}
    , oldAmplitude{}
    , newAmplitude{}
    , fm_oldAmplitude{}
    , fm_newAmplitude{}
    , fm_oldSmp{}
    , fmfm_oldPhase{}
    , fmfm_oldPMod{}
    , fmfm_oldInterpPhase{}
    , fm_oldOscPhase{}
    , fm_oldOscPMod{}
    , fm_oldOscInterpPhase{}
    , forFM{forFM_}
    , portamento{portamento_}
    , subVoice{}
    , subFMVoice{}
    , subVoiceNr{subVoice_}
    , topVoice{topVoice_}
    , parentFMmod{parentFMmod_}
{
    int phase = (topVoice==this)? 0 : synth.oscilsize - phaseOffset;
             // Start phase for sub-Voices should be negative
             // so that the zero phase in the first cycle will result in a positive phase change.
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        NoteVoicePar[nvoice].phaseOffset = phase;

    construct();
}

// Public Constructor for ordinary (top-level) voices
ADnote::ADnote(ADnoteParameters& adpars_, Controller& ctl_, Note note_, bool portamento_)
    : ADnote(adpars_
            ,ctl_
            ,note_
            ,portamento_
            , this    // marker: "this is a topVoice"
            ,-1       // marker: "this is not a subVoice"
            , 0       // top voice starts without phaseOffset
            , nullptr // no parentFMmod
            , false   // not forFM_
            )
{ }

// Public Constructor used to create "proxy notes" to attach to another voice within the same ADnote
ADnote::ADnote(ADnote *topVoice_, float freq_,
               int phase_offset_, int subVoiceNumber_, float *parentFMmod_, bool forFM_)
    : ADnote(topVoice_->adpars
            ,topVoice_->ctl
            ,topVoice_->note.withFreq(freq_)
            ,topVoice_->portamento
            ,topVoice_
            ,subVoiceNumber_
            ,phase_offset_
            ,parentFMmod_
            ,forFM_)
{ }



namespace{ // Array-cloning helper
    template<typename VAL>
    using VoiceUnisonArray = std::array<unique_ptr<VAL[]>, NUM_VOICES>;

    template<typename OBJ, typename VAL>
    inline void cloneArray(VoiceUnisonArray<VAL> OBJ::*arrMember
                          ,OBJ& newData, OBJ const& oldData
                          ,size_t voice, size_t unisonSiz)
    {
        using Arr = VoiceUnisonArray<VAL>;
        Arr const& oldArray = oldData.*arrMember;
        Arr&       newArray = newData.*arrMember;

        newArray[voice].reset(new VAL[NUM_VOICES]);
        memcpy(newArray[voice].get(), oldArray[voice].get(), unisonSiz * sizeof(VAL));
    }
}

// Copy constructor, used only used for legato (as of 4/2022)
ADnote::ADnote(ADnote const& orig, ADnote *topVoice_, float *parentFMmod_)
    : synth{orig.synth}
    , adpars{orig.adpars} // Probably okay for legato?
    , paramsUpdate{adpars}
    , ctl{orig.ctl}
    , note{orig.note}
    , stereo{orig.stereo}
    , noteStatus{orig.noteStatus}
    , noteGlobal{orig.noteGlobal}
    , tSpot{orig.tSpot}
    , paramRNG{orig.paramRNG}
    , paramSeed{orig.paramSeed}
    , detuneFromParent{orig.detuneFromParent}
    , unisonDetuneFactorFromParent{orig.unisonDetuneFactorFromParent}
    , forFM{orig.forFM}
    , max_unison{orig.max_unison}
    , globaloldamplitude{orig.globaloldamplitude}
    , globalnewamplitude{orig.globalnewamplitude}
    , portamento{orig.portamento}
    , bandwidthDetuneMultiplier{orig.bandwidthDetuneMultiplier}
    , legatoFade{0.0f} // Silent by default
    , legatoFadeStep{0.0f} // Legato disabled
    , pangainL{orig.pangainL}
    , pangainR{orig.pangainR}
    , subVoice{}
    , subFMVoice{}
    , subVoiceNr{orig.subVoiceNr}
    , topVoice{topVoice_? topVoice_ : this}
    , parentFMmod{parentFMmod_}
{
    // These are all arrays, so sizeof is correct
    memcpy(pinking, orig.pinking, sizeof(pinking));
    memcpy(firsttick, orig.firsttick, sizeof(firsttick));

    memcpy(oldAmplitude, orig.oldAmplitude, sizeof(oldAmplitude));
    memcpy(newAmplitude, orig.newAmplitude, sizeof(newAmplitude));
    memcpy(fm_oldAmplitude, orig.fm_oldAmplitude, sizeof(fm_oldAmplitude));
    memcpy(fm_newAmplitude, orig.fm_newAmplitude, sizeof(fm_newAmplitude));

    memcpy(unison_size, orig.unison_size, sizeof(unison_size));
    memcpy(unison_stereo_spread, orig.unison_stereo_spread, sizeof(unison_stereo_spread));
    memcpy(freqbasedmod, orig.freqbasedmod, sizeof(freqbasedmod));

    allocateUnison(max_unison, synth.buffersize);

    for (int voice = 0; voice < NUM_VOICES; ++voice)
    {
        auto& vpar = NoteVoicePar[voice];
        auto& ovpar = orig.NoteVoicePar[voice];

        vpar.enabled = ovpar.enabled;
        vpar.fmEnabled = ovpar.fmEnabled;

        if (ovpar.voiceOut) {
            vpar.voiceOut.reset(synth.buffersize);
            ///TODO: is copying of output buffers contents really necessary?
            memcpy(vpar.voiceOut.get(), ovpar.voiceOut.get(), synth.bufferbytes);
        } else
            vpar.voiceOut.reset();

        // The above vars are checked in killNote() even when the voice is
        // disabled, so short-circuit only after they are set
        if (!vpar.enabled)
            continue;

        // First, copy over everything that isn't behind a pointer
        vpar.voice = ovpar.voice;
        vpar.noiseType = ovpar.noiseType;
        vpar.filterBypass = ovpar.filterBypass;
        vpar.delayTicks = ovpar.delayTicks;
        vpar.phaseOffset = ovpar.phaseOffset;

        vpar.fixedFreq = ovpar.fixedFreq;
        vpar.fixedFreqET = ovpar.fixedFreqET;

        vpar.detune = ovpar.detune;
        vpar.fineDetune = ovpar.fineDetune;
        vpar.bendAdjust = ovpar.bendAdjust;
        vpar.offsetHz = ovpar.offsetHz;

        vpar.volume = ovpar.volume;
        vpar.panning = ovpar.panning;
        vpar.randpanL = ovpar.randpanL;
        vpar.randpanR = ovpar.randpanR;

        vpar.punch = ovpar.punch;

        vpar.fmFreqFixed = ovpar.fmFreqFixed;
        vpar.fmVoice = ovpar.fmVoice;
        vpar.fmPhaseOffset = ovpar.fmPhaseOffset;
        vpar.fmVolume = ovpar.fmVolume;
        vpar.fmDetuneFromBaseOsc = ovpar.fmDetuneFromBaseOsc;
        vpar.fmDetune = ovpar.fmDetune;

        // Now handle allocations
        if (subVoiceNr == -1)
        {
            vpar.oscilSmp.copyWaveform(ovpar.oscilSmp);
            vpar.fmSmp.copyWaveform(ovpar.fmSmp);

        } else {
            vpar.oscilSmp.attachReference(topVoice->NoteVoicePar[voice].oscilSmp);
            vpar.fmSmp.attachReference(topVoice->NoteVoicePar[voice].fmSmp);
        }

        if (ovpar.freqEnvelope)
            vpar.freqEnvelope.reset(new Envelope{*ovpar.freqEnvelope});
        if (ovpar.freqLFO)
            vpar.freqLFO.reset(new LFO{*ovpar.freqLFO});

        if (ovpar.ampEnvelope)
            vpar.ampEnvelope.reset(new Envelope{*ovpar.ampEnvelope});
        if (ovpar.ampLFO)
            vpar.ampLFO.reset(new LFO{*ovpar.ampLFO});

        if (orig.adpars.VoicePar[voice].PFilterEnabled) // (adpars is shared)
        {
            vpar.voiceFilterL.reset(new Filter{*ovpar.voiceFilterL});
            vpar.voiceFilterR.reset(new Filter{*ovpar.voiceFilterR});
        }
        else
        {
            vpar.voiceFilterL.reset();
            vpar.voiceFilterR.reset();
        }

        if (ovpar.filterEnvelope)
            vpar.filterEnvelope.reset(new Envelope{*ovpar.filterEnvelope});
        if (ovpar.filterLFO)
            vpar.filterLFO.reset(new LFO{*ovpar.filterLFO});

        if (ovpar.fmFreqEnvelope)
            vpar.fmFreqEnvelope.reset(new Envelope{*ovpar.fmFreqEnvelope});
        if (ovpar.fmAmpEnvelope)
            vpar.fmAmpEnvelope.reset(new Envelope{*ovpar.fmAmpEnvelope});

        // NoteVoicePar done

        int unison = unison_size[voice];

        cloneArray(&ADnote::oscposhi, *this, orig, voice, unison);
        cloneArray(&ADnote::oscposlo, *this, orig, voice, unison);
        cloneArray(&ADnote::oscfreqhi, *this, orig, voice, unison);
        cloneArray(&ADnote::oscfreqlo, *this, orig, voice, unison);
        cloneArray(&ADnote::oscposhiFM, *this, orig, voice, unison);
        cloneArray(&ADnote::oscposloFM, *this, orig, voice, unison);
        cloneArray(&ADnote::oscfreqhiFM, *this, orig, voice, unison);
        cloneArray(&ADnote::oscfreqloFM, *this, orig, voice, unison);

        cloneArray(&ADnote::unison_base_freq_rap,*this, orig, voice, unison);
        cloneArray(&ADnote::unison_freq_rap,     *this, orig, voice, unison);
        cloneArray(&ADnote::unison_invert_phase, *this, orig, voice, unison);

        unison_vibrato[voice].amplitude = orig.unison_vibrato[voice].amplitude;

        unison_vibrato[voice].step.reset(new float[unison]);
        memcpy(unison_vibrato[voice].step.get(), orig.unison_vibrato[voice].step.get(), unison * sizeof(float));

        unison_vibrato[voice].position.reset(new float[unison]);
        memcpy(unison_vibrato[voice].position.get(), orig.unison_vibrato[voice].position.get(), unison * sizeof(float));

        cloneArray(&ADnote::fm_oldSmp, *this, orig, voice, unison);

        if (parentFMmod != NULL)
        {
            if (NoteVoicePar[voice].fmEnabled == FREQ_MOD)
            {
                cloneArray(&ADnote::fmfm_oldPhase, *this, orig, voice, unison);
                cloneArray(&ADnote::fmfm_oldPMod, *this, orig, voice, unison);
                cloneArray(&ADnote::fmfm_oldInterpPhase, *this, orig, voice, unison);
            }

            if (forFM)
            {
                cloneArray(&ADnote::fm_oldOscPhase, *this, orig, voice, unison);
                cloneArray(&ADnote::fm_oldOscPMod, *this, orig, voice, unison);
                cloneArray(&ADnote::fm_oldOscInterpPhase, *this, orig, voice, unison);
            }
        }

        if (orig.subVoice[voice])
        {
            subVoice[voice].reset(new unique_ptr<ADnote>[orig.unison_size[voice]]);
            for (size_t k = 0; k < orig.unison_size[voice]; ++k)
                subVoice[voice][k].reset(new ADnote(*orig.subVoice[voice][k]
                                               , topVoice
                                               , freqbasedmod[voice]? tmpmod_unison[k].get()
                                                               : parentFMmod));
        }

        if (orig.subFMVoice[voice])
        {
            subFMVoice[voice].reset(new unique_ptr<ADnote>[orig.unison_size[voice]]);
            for (size_t k = 0; k < orig.unison_size[voice]; ++k)
            {
                subFMVoice[voice][k].reset(new ADnote(*orig.subFMVoice[voice][k]
                                                 , topVoice
                                                 , parentFMmod));
            }
        }
    }
}

void ADnote::construct()
{
    // Initialise some legato-specific vars
    legatoFade = 1.0f; // Full volume
    legatoFadeStep = 0.0f; // Legato disabled

    paramSeed = synth.randomINT();

    setRandomPan(synth.numRandom(), noteGlobal.randpanL, noteGlobal.randpanR, synth.getRuntime().panLaw, adpars.GlobalPar.PPanning, adpars.GlobalPar.PWidth);

    noteGlobal.fadeinAdjustment =
        adpars.GlobalPar.Fadein_adjustment / (float)FADEIN_ADJUSTMENT_SCALE;
    noteGlobal.fadeinAdjustment *= noteGlobal.fadeinAdjustment;
    if (adpars.GlobalPar.PPunchStrength)
    {
        noteGlobal.punch.enabled = true;
        noteGlobal.punch.t = 1.0f; //start from 1.0 and to 0.0
        noteGlobal.punch.initialvalue =
            ((power<10>(1.5f * adpars.GlobalPar.PPunchStrength / 127.0f) - 1.0f)
             * velF(note.vel, adpars.GlobalPar.PPunchVelocitySensing));
        float time = power<10>(3.0f * adpars.GlobalPar.PPunchTime / 127.0f) / 10000.0f; // 0.1 .. 100 ms
        float stretch = powf(440.0f / note.freq, adpars.GlobalPar.PPunchStretch / 64.0f);
        noteGlobal.punch.dt = 1.0f / (time * synth.samplerate_f * stretch);
    }
    else
        noteGlobal.punch.enabled = false;

    detuneFromParent = 0.0;
    unisonDetuneFactorFromParent = 1.0;

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (int i = 0; i < 14; i++)
            pinking[nvoice][i] = 0.0;

        NoteVoicePar[nvoice].voiceOut.reset();

        NoteVoicePar[nvoice].fmEnabled = NONE;
        NoteVoicePar[nvoice].fmRingToSide = false;
        NoteVoicePar[nvoice].fmVoice = -1;
        unison_size[nvoice] = 1;

        // If used as a sub voice, enable exactly one voice, the requested
        // one. If not, enable voices that are enabled in settings.
        if (!(adpars.VoicePar[nvoice].Enabled
              && (subVoiceNr == -1 || nvoice == subVoiceNr)))
        {
            NoteVoicePar[nvoice].enabled = false;
            continue; // the voice is disabled
        }
        NoteVoicePar[nvoice].enabled = true;

        int unison = adpars.VoicePar[nvoice].Unison_size;
        if (unison < 1)
            unison = 1;

        bool is_pwm = adpars.VoicePar[nvoice].PFMEnabled == PW_MOD;

        if (adpars.VoicePar[nvoice].Type != 0)
        {
            // Since noise unison of greater than two is touch goofy...
            if (unison > 2)
                unison = 2;
        }
        else if (is_pwm)
        {
            /* Pulse width mod uses pairs of subvoices. */
            unison *= 2;
            // This many is likely to sound like noise anyhow.
            if (unison > 64)
                unison = 64;
        }

        // compute unison
        unison_size[nvoice] = unison;

        unison_base_freq_rap[nvoice].reset(new float[unison]{0});
        unison_freq_rap     [nvoice].reset(new float[unison]{0});
        unison_invert_phase [nvoice].reset(new bool[unison]{false});
        unison_vibrato[nvoice].step .reset(new float[unison]{0});
        unison_vibrato[nvoice].position.reset(new float[unison]{0});

        if (unison >> is_pwm > 1)
        {
            for (int k = 0; k < unison; ++k)
            {
                unison_vibrato[nvoice].position[k] = synth.numRandom() * 1.8f - 0.9f;

                // Give step a random direction. The amplitude doesn't matter right
                // now, only the sign, which will be preserved in
                // computeNoteParameters().
                if (synth.numRandom() < 0.5f)
                    unison_vibrato[nvoice].step[k] = -1.0f;
                else
                    unison_vibrato[nvoice].step[k] = 1.0f;

                if (is_pwm)
                {
                    // Set the next position the same as this one.
                    unison_vibrato[nvoice].position[k+1] =
                        unison_vibrato[nvoice].position[k];
                    ++k; // Skip an iteration.
                    // step and amplitude are handled in computeNoteParameters.
                }
            }
        }
        else // No vibrato for a single voice
        {
            if (is_pwm)
            {
                unison_vibrato[nvoice].position[1] = 0.0f;
            }
            if (is_pwm || unison == 1)
            {
                unison_vibrato[nvoice].position[0] = 0.0f;
            }
        }

        oscposhi[nvoice].reset(new int[unison]{0});// zero-init
        oscposlo[nvoice].reset(new float[unison]{0});
        oscfreqhi[nvoice].reset(new int[unison]{0});
        oscfreqlo[nvoice].reset(new float[unison]{0});
        oscposhiFM[nvoice].reset(new int[unison]{0});
        oscposloFM[nvoice].reset(new float[unison]{0});
        oscfreqhiFM[nvoice].reset(new int[unison]{0});
        oscfreqloFM[nvoice].reset(new float[unison]{0});

        NoteVoicePar[nvoice].voice = adpars.VoicePar[nvoice].PVoice;

        int vc = nvoice;
        if (adpars.VoicePar[nvoice].Pextoscil != -1)
            vc = adpars.VoicePar[nvoice].Pextoscil;

        // prepare wavetable for the voice's oscil or external voice's oscil
        if (subVoiceNr == -1) {
            // this voice manages its own oscillator wavetable
            NoteVoicePar[nvoice].oscilSmp.allocateWaveform(synth.oscilsize);

            // Draw new seed for randomisation of harmonics
            // Since NoteON happens at random times, this actually injects entropy
            adpars.VoicePar[nvoice].OscilSmp->newrandseed();

            if (!adpars.GlobalPar.Hrandgrouping)
                adpars.VoicePar[vc].OscilSmp->newrandseed();

            // Actual OscilSmp rendering done later.
        } else {
            // If subvoice, use oscillator from original voice.
            NoteVoicePar[nvoice].oscilSmp.attachReference(topVoice->NoteVoicePar[nvoice].oscilSmp);
        }

        int oscposhi_start;
        if (NoteVoicePar[nvoice].voice == -1)
            oscposhi_start = adpars.VoicePar[vc].OscilSmp->getPhase();
        else
            oscposhi_start = 0;
        int kth_start = oscposhi_start;
        for (int k = 0; k < unison; ++k)
        {
            oscposhi[nvoice][k] = kth_start % synth.oscilsize;
            // put random starting point for other subvoices
            kth_start = oscposhi_start + (int)(synth.numRandom() * adpars.VoicePar[nvoice].Unison_phase_randomness
                                        / 127.0f * (synth.oscilsize - 1));
        }

        if (adpars.VoicePar[nvoice].Type != 0)
            NoteVoicePar[nvoice].fmEnabled = NONE;
        else
            switch (adpars.VoicePar[nvoice].PFMEnabled)
            {
                case 1:
                    NoteVoicePar[nvoice].fmEnabled = MORPH;
                    freqbasedmod[nvoice] = false;
                    break;
                case 2:
                    NoteVoicePar[nvoice].fmEnabled = RING_MOD;
                    freqbasedmod[nvoice] = false;
                    break;
                case 3:
                    NoteVoicePar[nvoice].fmEnabled = PHASE_MOD;
                    freqbasedmod[nvoice] = true;
                    break;
                case 4:
                    NoteVoicePar[nvoice].fmEnabled = FREQ_MOD;
                    freqbasedmod[nvoice] = true;
                    break;
                case 5:
                    NoteVoicePar[nvoice].fmEnabled = PW_MOD;
                    freqbasedmod[nvoice] = true;
                    break;
                default:
                    NoteVoicePar[nvoice].fmEnabled = NONE;
                    freqbasedmod[nvoice] = false;
                    break;
            }
        NoteVoicePar[nvoice].fmRingToSide = adpars.VoicePar[nvoice].PFMringToSide;
        NoteVoicePar[nvoice].fmVoice = adpars.VoicePar[nvoice].PFMVoice;

        fm_oldSmp[nvoice].reset(new float [unison]{0}); // zero init

        firsttick[nvoice] = 1;
        NoteVoicePar[nvoice].delayTicks =
            (int)((expf(adpars.VoicePar[nvoice].PDelay / 127.0f
            * logf(50.0f)) - 1.0f) / synth.fixed_sample_step_f / 10.0f);

        if (parentFMmod != NULL && NoteVoicePar[nvoice].fmEnabled == FREQ_MOD) {
            fmfm_oldPhase[nvoice].reset(new float [unison]{0}); // zero init
            fmfm_oldPMod [nvoice].reset(new float [unison]{0});
            fmfm_oldInterpPhase[nvoice].reset(new float [unison]{0});
        }
        if (parentFMmod != NULL && forFM) {
            fm_oldOscPhase[nvoice].reset(new float [unison]{0}); // zero init
            fm_oldOscPMod [nvoice].reset(new float [unison]{0});
            fm_oldOscInterpPhase[nvoice].reset(new float [unison]{0});
        }
    }

    max_unison = 1;
    for (size_t nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        if (unison_size[nvoice] > max_unison)
            max_unison = unison_size[nvoice];

    allocateUnison(max_unison, synth.buffersize);

    initParameters();
    initSubVoices();

    globalnewamplitude = noteGlobal.volume
                         * noteGlobal.ampEnvelope->envout_dB()
                         * noteGlobal.ampLFO->amplfoout();
}

void ADnote::allocateUnison(size_t unisonCnt, size_t buffSize)
{
    tmpwave_unison.reset(new Samples[unisonCnt]);
    tmpmod_unison .reset(new Samples[unisonCnt]);
    for (size_t k = 0; k < unisonCnt; ++k)
    {
        tmpwave_unison[k].reset(buffSize);
        tmpmod_unison[k] .reset(buffSize);
    }
}

void ADnote::initSubVoices(void)
{
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].enabled)
            continue;

        if (NoteVoicePar[nvoice].voice != -1)
        {
            subVoice[nvoice].reset(new unique_ptr<ADnote>[unison_size[nvoice]]);
            for (size_t k = 0; k < unison_size[nvoice]; ++k)
            {
                float *freqmod = freqbasedmod[nvoice] ? tmpmod_unison[k].get() : parentFMmod;
                subVoice[nvoice][k].reset(new ADnote(topVoice,
                                                     getVoiceBaseFreq(nvoice),
                                                     oscposhi[nvoice][k],
                                                     NoteVoicePar[nvoice].voice,
                                                     freqmod, forFM));
            }
        }

        if (NoteVoicePar[nvoice].fmVoice != -1)
        {
            bool voiceForFM = NoteVoicePar[nvoice].fmEnabled == FREQ_MOD;
            subFMVoice[nvoice].reset(new unique_ptr<ADnote>[unison_size[nvoice]]);
            for (size_t k = 0; k < unison_size[nvoice]; ++k) {
                subFMVoice[nvoice][k].reset(new ADnote(topVoice,
                                                     getFMVoiceBaseFreq(nvoice),
                                                     oscposhiFM[nvoice][k],
                                                     NoteVoicePar[nvoice].fmVoice,
                                                     parentFMmod, voiceForFM));
            }
        }
    }
}

// Note portamento does not recompute note parameters, since it should be a smooth change in pitch,
//      with no change in timbre (or at least rather a gradual one). It may be desirable to have base
//      frequency sensitive things like filter scaling and envelope stretching take portamento into account,
//      but to do this properly would require more than just recalculating based on a fixed base frequency,
//      and the current code is thus not able to implement that.
void ADnote::performPortamento(Note note_)
{
    portamento = true;
    this->note = note_;

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        if (not NoteVoicePar[i].enabled)
            continue; // sub-Voices can only be attached to enabled voices

        if (subVoice[i])
            for (size_t k = 0; k < unison_size[i]; ++k)
                subVoice[i][k]->performPortamento(note.withFreq(getVoiceBaseFreq(i)));
        if (subFMVoice[i])
            for (size_t k = 0; k < unison_size[i]; ++k)
                subFMVoice[i][k]->performPortamento(note.withFreq(getFMVoiceBaseFreq(i)));
    }
}


void ADnote::legatoFadeIn(Note note_)
{
    portamento = false; // portamento-legato treated separately
    this->note = note_;

    // Re-randomize harmonics
    if (subVoiceNr == -1)
        for (int i = 0; i < NUM_VOICES; ++i)
        {
            adpars.VoicePar[i].OscilSmp->newrandseed();
            auto &extoscil = adpars.VoicePar[i].Pextoscil;
            if (extoscil != -1 && !adpars.GlobalPar.Hrandgrouping)
                adpars.VoicePar[extoscil].OscilSmp->newrandseed();
        }

    // This recalculates stuff like harmonic phase/amplitude randomness,
    // not sure if desirable for legato, at least it ensures sane initialisation.
    // Note: to the contrary, Portamento does not re-init any of these values.
    computeNoteParameters();

    legatoFade = 0.0f; // Start crossfade silent
    legatoFadeStep = synth.fadeStepShort; // Positive steps

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        if (not NoteVoicePar[i].enabled)
            continue; // sub-Voices can only be attached to enabled voices

        if (subVoice[i])
            for (size_t k = 0; k < unison_size[i]; ++k)
                subVoice[i][k]->legatoFadeIn(note.withFreq(getVoiceBaseFreq(i)));
        if (subFMVoice[i])
            for (size_t k = 0; k < unison_size[i]; ++k)
                subFMVoice[i][k]->legatoFadeIn(note.withFreq(getFMVoiceBaseFreq(i)));
    }
}


void ADnote::legatoFadeOut()
{
    for (int i = 0; i < NUM_VOICES; ++i)
    {
        if (not NoteVoicePar[i].enabled)
            continue; // sub-Voices can only be attached to enabled voices

        if (subVoice[i])
            for (size_t k = 0; k < unison_size[i]; ++k)
                subVoice[i][k]->legatoFadeOut();
        if (subFMVoice[i])
            for (size_t k = 0; k < unison_size[i]; ++k)
                subFMVoice[i][k]->legatoFadeOut();
    }

    legatoFade = 1.0f;     // crossfade down from full volume
    legatoFadeStep = -synth.fadeStepShort; // Negative steps

    // transitory state similar to a released Envelope
    noteStatus = NOTE_LEGATOFADEOUT;
}



// Kill a voice of ADnote
void ADnote::killVoice(int nvoice)
{
    oscposhi[nvoice].reset();
    oscposlo[nvoice].reset();
    oscfreqhi[nvoice].reset();
    oscfreqlo[nvoice].reset();
    oscposhiFM[nvoice].reset();
    oscposloFM[nvoice].reset();
    oscfreqhiFM[nvoice].reset();
    oscfreqloFM[nvoice].reset();

    unison_base_freq_rap[nvoice].reset();
    unison_freq_rap[nvoice].reset();
    unison_invert_phase[nvoice].reset();
    fm_oldSmp[nvoice].reset();

    unison_vibrato[nvoice].step.reset();
    unison_vibrato[nvoice].position.reset();

    subVoice[nvoice].reset();
    subFMVoice[nvoice].reset();

    NoteVoicePar[nvoice].freqEnvelope.reset();
    NoteVoicePar[nvoice].freqLFO.reset();
    NoteVoicePar[nvoice].ampEnvelope.reset();
    NoteVoicePar[nvoice].ampLFO.reset();
    NoteVoicePar[nvoice].voiceFilterL.reset();
    NoteVoicePar[nvoice].voiceFilterR.reset();
    NoteVoicePar[nvoice].filterEnvelope.reset();
    NoteVoicePar[nvoice].filterLFO.reset();
    NoteVoicePar[nvoice].fmFreqEnvelope.reset();
    NoteVoicePar[nvoice].fmAmpEnvelope.reset();

    if (NoteVoicePar[nvoice].voiceOut)
        memset(NoteVoicePar[nvoice].voiceOut.get(), 0, synth.bufferbytes);
        // do not delete, yet: perhaps is used by another voice

    if (parentFMmod != NULL && NoteVoicePar[nvoice].fmEnabled == FREQ_MOD) {
        fmfm_oldPhase[nvoice].reset();
        fmfm_oldPMod[nvoice].reset();
        fmfm_oldInterpPhase[nvoice].reset();
    }
    if (parentFMmod != NULL && forFM) {
        fm_oldOscPhase[nvoice].reset();
        fm_oldOscPMod[nvoice].reset();
        fm_oldOscInterpPhase[nvoice].reset();
    }

    NoteVoicePar[nvoice].enabled = false;
}


// Kill the note
void ADnote::killNote()
{
    // Note: Storage for samples is managed automatically by SampleHolder
    //       Subvoices and all sub-components use automatic memory management
    //       and will be discarded by the dtor, which is invoked shortly thereafter

    noteStatus = NOTE_DISABLED; // causes clean-up of this note instance
}




// Init the parameters
void ADnote::initParameters(void)
{
    int nvoice, i;

    // Global Parameters
    noteGlobal.freqEnvelope.reset(new Envelope{adpars.GlobalPar.FreqEnvelope, note.freq, &synth});
    noteGlobal.freqLFO     .reset(new LFO{adpars.GlobalPar.FreqLfo, note.freq, &synth});
    noteGlobal.ampEnvelope .reset(new Envelope{adpars.GlobalPar.AmpEnvelope, note.freq, &synth});
    noteGlobal.ampLFO      .reset(new LFO{adpars.GlobalPar.AmpLfo, note.freq, &synth});

    noteGlobal.ampEnvelope->envout_dB(); // discard the first envelope output

    noteGlobal.filterEnvelope.reset(new Envelope{adpars.GlobalPar.FilterEnvelope, note.freq, &synth});
    noteGlobal.filterLFO     .reset(new LFO{adpars.GlobalPar.FilterLfo, note.freq, &synth});
    noteGlobal.filterL.reset(new Filter{adpars.GlobalPar.GlobalFilter, &synth});
    if (stereo)
        noteGlobal.filterR.reset(new Filter{adpars.GlobalPar.GlobalFilter, &synth});

    // Forbids the Modulation Voice to be greater or equal than voice
    for (i = 0; i < NUM_VOICES; ++i)
        if (NoteVoicePar[i].fmVoice >= i)
            NoteVoicePar[i].fmVoice = -1;

    // Voice Parameter init
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].enabled)
            continue;

        NoteVoicePar[nvoice].noiseType = adpars.VoicePar[nvoice].Type;

        setRandomPan(synth.numRandom()
                    ,NoteVoicePar[nvoice].randpanL
                    ,NoteVoicePar[nvoice].randpanR
                    ,synth.getRuntime().panLaw
                    ,adpars.VoicePar[nvoice].PPanning
                    ,adpars.VoicePar[nvoice].PWidth);

        newAmplitude[nvoice] = 1.0f;
        if (adpars.VoicePar[nvoice].PAmpEnvelopeEnabled)
        {
            NoteVoicePar[nvoice].ampEnvelope.reset(new Envelope{adpars.VoicePar[nvoice].AmpEnvelope, note.freq, &synth});
            NoteVoicePar[nvoice].ampEnvelope->envout_dB(); // discard the first envelope sample
            newAmplitude[nvoice] *= NoteVoicePar[nvoice].ampEnvelope->envout_dB();
        }

        if (adpars.VoicePar[nvoice].PAmpLfoEnabled)
        {
            NoteVoicePar[nvoice].ampLFO.reset(new LFO{adpars.VoicePar[nvoice].AmpLfo, note.freq, &synth});
            newAmplitude[nvoice] *= NoteVoicePar[nvoice].ampLFO->amplfoout();
        }

        // Voice Frequency Parameters Init
        if (adpars.VoicePar[nvoice].PFreqEnvelopeEnabled)
            NoteVoicePar[nvoice].freqEnvelope.reset(new Envelope{adpars.VoicePar[nvoice].FreqEnvelope, note.freq, &synth});

        if (adpars.VoicePar[nvoice].PFreqLfoEnabled)
            NoteVoicePar[nvoice].freqLFO.reset(new LFO{adpars.VoicePar[nvoice].FreqLfo, note.freq, &synth});

        // Voice Filter Parameters Init
        if (adpars.VoicePar[nvoice].PFilterEnabled)
        {
            NoteVoicePar[nvoice].voiceFilterL.reset(new Filter{adpars.VoicePar[nvoice].VoiceFilter, &synth});
            NoteVoicePar[nvoice].voiceFilterR.reset(new Filter{adpars.VoicePar[nvoice].VoiceFilter, &synth});
        }

        if (adpars.VoicePar[nvoice].PFilterEnvelopeEnabled)
            NoteVoicePar[nvoice].filterEnvelope.reset(new Envelope{adpars.VoicePar[nvoice].FilterEnvelope, note.freq, &synth});

        if (adpars.VoicePar[nvoice].PFilterLfoEnabled)
            NoteVoicePar[nvoice].filterLFO.reset(new LFO{adpars.VoicePar[nvoice].FilterLfo, note.freq, &synth});

        // Voice Modulation Parameters Init
        if (NoteVoicePar[nvoice].fmEnabled != NONE
           && NoteVoicePar[nvoice].fmVoice < 0)
        {
            // Perform Anti-aliasing only on MORPH or RING MODULATION

            int vc = nvoice;
            if (adpars.VoicePar[nvoice].PextFMoscil != -1)
                vc = adpars.VoicePar[nvoice].PextFMoscil;

            if (subVoiceNr == -1) {
                // this voice maintains its own oscil wavetable...
                NoteVoicePar[nvoice].fmSmp.allocateWaveform(synth.oscilsize);

                adpars.VoicePar[nvoice].FMSmp->newrandseed();
                if (!adpars.GlobalPar.Hrandgrouping)
                    adpars.VoicePar[vc].FMSmp->newrandseed();

            } else {
                // If subvoice use oscillator from original voice.
                NoteVoicePar[nvoice].fmSmp.attachReference(topVoice->NoteVoicePar[nvoice].fmSmp);
            }

            for (size_t k = 0; k < unison_size[nvoice]; ++k)
                oscposhiFM[nvoice][k] =
                    (oscposhi[nvoice][k] + adpars.VoicePar[vc].FMSmp->
                     getPhase()) % synth.oscilsize;

            NoteVoicePar[nvoice].fmPhaseOffset = 0;
        }

        if (adpars.VoicePar[nvoice].PFMFreqEnvelopeEnabled != 0)
            NoteVoicePar[nvoice].fmFreqEnvelope.reset(new Envelope{adpars.VoicePar[nvoice].FMFreqEnvelope, note.freq, &synth});
        if (adpars.VoicePar[nvoice].PFMAmpEnvelopeEnabled != 0)
            NoteVoicePar[nvoice].fmAmpEnvelope.reset(new Envelope{adpars.VoicePar[nvoice].FMAmpEnvelope, note.freq, &synth});
    }

    computeNoteParameters();

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].enabled)
            continue;

        fm_newAmplitude[nvoice] = NoteVoicePar[nvoice].fmVolume
                                 * ctl.fmamp.relamp;

        if (NoteVoicePar[nvoice].fmAmpEnvelope != NULL)
        {
            fm_newAmplitude[nvoice] *=
                NoteVoicePar[nvoice].fmAmpEnvelope->envout_dB();
        }
    }

    if (subVoiceNr != -1)
        NoteVoicePar[subVoiceNr].voiceOut.reset(synth.buffersize);
}


void ADnote::computeNoteParameters(void)
{
    paramRNG.init(paramSeed);

    noteGlobal.detune = getDetune(adpars.GlobalPar.PDetuneType,
                                     adpars.GlobalPar.PCoarseDetune,
                                     adpars.GlobalPar.PDetune);
    bandwidthDetuneMultiplier = adpars.getBandwidthDetuneMultiplier();

    noteGlobal.volume =
        4.0f                                                           // +12dB boost (similar on PADnote, while SUBnote only boosts +6dB)
        * decibel<-60>(1.0f - adpars.GlobalPar.PVolume / 96.0f)       // -60 dB .. +19.375 dB
        * velF(note.vel, adpars.GlobalPar.PAmpVelocityScaleFunction); // velocity sensing

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].enabled)
            continue;

        if (subVoiceNr == -1) {
            int BendAdj = adpars.VoicePar[nvoice].PBendAdjust - 64;
            if (BendAdj % 24 == 0)
                NoteVoicePar[nvoice].bendAdjust = BendAdj / 24;
            else
                NoteVoicePar[nvoice].bendAdjust = BendAdj / 24.0f;
        } else {
            // No bend adjustments for sub voices. Take from parent via
            // detuneFromParent.
            NoteVoicePar[nvoice].bendAdjust = 0.0f;
        }

        float offset_val = (adpars.VoicePar[nvoice].POffsetHz - 64)/64.0f;
        NoteVoicePar[nvoice].offsetHz =
            15.0f*(offset_val * sqrtf(fabsf(offset_val)));

        NoteVoicePar[nvoice].fixedFreq = adpars.VoicePar[nvoice].Pfixedfreq;
        NoteVoicePar[nvoice].fixedFreqET = adpars.VoicePar[nvoice].PfixedfreqET;

        // use the Globalpars.detunetype if the detunetype is 0
        if (adpars.VoicePar[nvoice].PDetuneType)
        {
            NoteVoicePar[nvoice].detune =
                getDetune(adpars.VoicePar[nvoice].PDetuneType,
                          adpars.VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].fineDetune =
                getDetune(adpars.VoicePar[nvoice].PDetuneType, 0,
                          adpars.VoicePar[nvoice].PDetune); // fine detune
        }
        else
        {
            NoteVoicePar[nvoice].detune =
                getDetune(adpars.GlobalPar.PDetuneType,
                          adpars.VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].fineDetune =
                getDetune(adpars.GlobalPar.PDetuneType, 0,
                          adpars.VoicePar[nvoice].PDetune); // fine detune
        }
        if (adpars.VoicePar[nvoice].PFMDetuneType != 0)
            NoteVoicePar[nvoice].fmDetune =
                getDetune(adpars.VoicePar[nvoice].PFMDetuneType,
                          adpars.VoicePar[nvoice].PFMCoarseDetune,
                          adpars.VoicePar[nvoice].PFMDetune);
        else
            NoteVoicePar[nvoice].fmDetune =
                getDetune(adpars.GlobalPar.PDetuneType, adpars.VoicePar[nvoice].
                          PFMCoarseDetune, adpars.VoicePar[nvoice].PFMDetune);

        NoteVoicePar[nvoice].filterBypass = adpars.VoicePar[nvoice].Pfilterbypass;

        NoteVoicePar[nvoice].fmDetuneFromBaseOsc =
            (adpars.VoicePar[nvoice].PFMDetuneFromBaseOsc != 0);
        NoteVoicePar[nvoice].fmFreqFixed  = adpars.VoicePar[nvoice].PFMFixedFreq;

        if (subVoice[nvoice])
        {
            float basefreq = getVoiceBaseFreq(nvoice);
            if (basefreq != subVoice[nvoice][0]->note.freq)
                for (size_t k = 0; k < unison_size[nvoice]; ++k)
                    subVoice[nvoice][k]->note.freq = basefreq;
        }
        if (subFMVoice[nvoice])
        {
            float basefreq = getFMVoiceBaseFreq(nvoice);
            if (basefreq != subFMVoice[nvoice][0]->note.freq)
                for (size_t k = 0; k < unison_size[nvoice]; ++k)
                    subFMVoice[nvoice][k]->note.freq = basefreq;
        }

        // Compute the Voice's modulator volume (incl. damping)
        float fmvoldamp = powf(440.0f / getVoiceBaseFreq(nvoice),
                               adpars.VoicePar[nvoice].PFMVolumeDamp
                               / 64.0f - 1.0f);
        switch (NoteVoicePar[nvoice].fmEnabled)
        {
            case PHASE_MOD:
            case PW_MOD:
                fmvoldamp = powf(440.0f / getVoiceBaseFreq(nvoice),
                                 adpars.VoicePar[nvoice].PFMVolumeDamp / 64.0f);
                NoteVoicePar[nvoice].fmVolume =
                    (expf(adpars.VoicePar[nvoice].PFMVolume / 127.0f
                          * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;

            case FREQ_MOD:
                NoteVoicePar[nvoice].fmVolume =
                    (expf(adpars.VoicePar[nvoice].PFMVolume / 127.0f
                          * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;

            default:
                if (fmvoldamp > 1.0f)
                    fmvoldamp = 1.0f;
                NoteVoicePar[nvoice].fmVolume =
                    adpars.VoicePar[nvoice].PFMVolume / 127.0f * fmvoldamp;
                break;
        }

        // Voice's modulator velocity sensing
        NoteVoicePar[nvoice].fmVolume *=
            velF(note.vel, adpars.VoicePar[nvoice].PFMVelocityScaleFunction);

        // Voice Amplitude Parameters Init
        if (adpars.VoicePar[nvoice].PVolume == 0)
            NoteVoicePar[nvoice].volume = 0.0f;
        else
            NoteVoicePar[nvoice].volume =
                decibel<-60>(1.0f - adpars.VoicePar[nvoice].PVolume / 127.0f)        // -60 dB .. 0 dB
                * velF(note.vel, adpars.VoicePar[nvoice].PAmpVelocityScaleFunction); // velocity

        if (adpars.VoicePar[nvoice].PVolumeminus)
            NoteVoicePar[nvoice].volume = -NoteVoicePar[nvoice].volume;

        int unison = unison_size[nvoice];

        if (subVoiceNr == -1) {
            int vc = nvoice;
            if (adpars.VoicePar[nvoice].Pextoscil != -1)
                vc = adpars.VoicePar[nvoice].Pextoscil;
            adpars.VoicePar[vc].OscilSmp->getWave(NoteVoicePar[nvoice].oscilSmp,
                                                   getVoiceBaseFreq(nvoice),
                                                   adpars.VoicePar[nvoice].Presonance != 0);

            // I store the first elements to the last position for speedups
            NoteVoicePar[nvoice].oscilSmp.fillInterpolationBuffer();
        }

        int new_phase_offset = (int)((adpars.VoicePar[nvoice].Poscilphase - 64.0f)
                                    / 128.0f * synth.oscilsize + synth.oscilsize * 4);
        int phase_offset_diff = new_phase_offset - NoteVoicePar[nvoice].phaseOffset;
        for (int k = 0; k < unison; ++k) {
            oscposhi[nvoice][k] = (oscposhi[nvoice][k] + phase_offset_diff) % synth.oscilsize;
            if (oscposhi[nvoice][k] < 0)
                // This is necessary, because C '%' operator does not always
                // return a positive result.
                oscposhi[nvoice][k] += synth.oscilsize;
        }
        NoteVoicePar[nvoice].phaseOffset = new_phase_offset;

        if (NoteVoicePar[nvoice].fmEnabled != NONE
            && NoteVoicePar[nvoice].fmVoice < 0)
        {
            if (subVoiceNr == -1) {
                int vc = nvoice;
                if (adpars.VoicePar[nvoice].PextFMoscil != -1)
                    vc = adpars.VoicePar[nvoice].PextFMoscil;

                float freqtmp = 1.0f;
                if (adpars.VoicePar[vc].POscilFM->Padaptiveharmonics != 0
                    || (NoteVoicePar[nvoice].fmEnabled == MORPH)
                    || (NoteVoicePar[nvoice].fmEnabled == RING_MOD))
                    freqtmp = getFMVoiceBaseFreq(nvoice);

                adpars.VoicePar[vc].FMSmp->getWave(NoteVoicePar[nvoice].fmSmp, freqtmp);
                NoteVoicePar[nvoice].fmSmp.fillInterpolationBuffer();
            }

            int new_FMphase_offset = (int)((adpars.VoicePar[nvoice].PFMoscilphase - 64.0f)
                                         / 128.0f * synth.oscilsize_f
                                         + synth.oscilsize_f * 4.0f);
            int FMphase_offset_diff = new_FMphase_offset - NoteVoicePar[nvoice].fmPhaseOffset;
            for (size_t k = 0; k < unison_size[nvoice]; ++k)
            {
                oscposhiFM[nvoice][k] += FMphase_offset_diff;
                oscposhiFM[nvoice][k] %= synth.oscilsize;
                if (oscposhiFM[nvoice][k] < 0)
                    // This is necessary, because C '%' operator does not always
                    // return a positive result.
                    oscposhiFM[nvoice][k] += synth.oscilsize;
            }
            NoteVoicePar[nvoice].fmPhaseOffset = new_FMphase_offset;
        }

        bool is_pwm = NoteVoicePar[nvoice].fmEnabled == PW_MOD;

        unison_stereo_spread[nvoice] =
            adpars.VoicePar[nvoice].Unison_stereo_spread / 127.0f;
        float unison_spread = adpars.getUnisonFrequencySpreadCents(nvoice);
        float unison_real_spread = power<2>((unison_spread * 0.5f) / 1200.0f);
        float unison_vibrato_a = adpars.VoicePar[nvoice].Unison_vibrato / 127.0f;   // 0.0 .. 1.0

        int true_unison = unison >> is_pwm;
        switch (true_unison)
        {
            case 1: // if no unison, set the subvoice to the default note
                unison_base_freq_rap[nvoice][0] = 1.0f;
                break;

            case 2:  // unison for 2 subvoices
                {
                    unison_base_freq_rap[nvoice][0] = 1.0f / unison_real_spread;
                    unison_base_freq_rap[nvoice][1] = unison_real_spread;
                }
                break;

            default: // unison for more than 2 subvoices
                {
                    float unison_values[unison];
                    float min = -1e-6f, max = 1e-6f;
                    for (int k = 0; k < true_unison; ++k)
                    {
                        float step = (k / (float) (true_unison - 1)) * 2.0f - 1.0f;  //this makes the unison spread more uniform
                        float val  = step + (paramRNG.numRandom() * 2.0f - 1.0f) / (true_unison - 1);
                        unison_values[k] = val;
                        if (val > max)
                            max = val;
                        if (val < min)
                            min = val;
                    }
                    float diff = max - min;
                    for (int k = 0; k < true_unison; ++k)
                    {
                        unison_values[k] =
                            (unison_values[k] - (max + min) * 0.5f) / diff;
                            // the lowest value will be -1 and the highest will be 1
                        unison_base_freq_rap[nvoice][k] =
                            power<2>((unison_spread * unison_values[k]) / 1200.0f);
                    }
                }
                break;
        }
        if (is_pwm)
            for (int i = true_unison - 1; i >= 0; i--)
            {
                unison_base_freq_rap[nvoice][2*i + 1] = unison_base_freq_rap[nvoice][i];
                unison_base_freq_rap[nvoice][2*i]     = unison_base_freq_rap[nvoice][i];
            }

        // unison vibratos
        if (true_unison > 1)
        {
            for (int k = 0; k < unison; ++k) // reduce the frequency difference
                                             // for larger vibratos
                unison_base_freq_rap[nvoice][k] =
                    1.0f + (unison_base_freq_rap[nvoice][k] - 1.0f)
                    * (1.0f - unison_vibrato_a);

            unison_vibrato[nvoice].amplitude = (unison_real_spread - 1.0f) * unison_vibrato_a;

            float increments_per_second = 1 / synth.fixed_sample_step_f;
            const float vib_speed = adpars.VoicePar[nvoice].Unison_vibrato_speed / 127.0f;
            float vibrato_base_period  = 0.25f * power<2>((1.0f - vib_speed) * 4.0f);
            for (int k = 0; k < unison; ++k)
            {
                // make period to vary randomly from 50% to 200% vibrato base period
                float vibrato_period = vibrato_base_period * power<2>(paramRNG.numRandom() * 2.0f - 1.0f);
                float m = 4.0f / (vibrato_period * increments_per_second);
                if (unison_vibrato[nvoice].step[k] < 0.0f)
                    m = -m;
                unison_vibrato[nvoice].step[k] = m;

                if (is_pwm)
                {
                    // Set the next position the same as this one.
                    unison_vibrato[nvoice].step[k+1] =
                        unison_vibrato[nvoice].step[k];
                    ++k; // Skip an iteration.
                }
            }
        }
        else // No vibrato for a single voice
        {
            unison_vibrato[nvoice].step[0] = 0.0f;
            unison_vibrato[nvoice].amplitude = 0.0f;

            if (is_pwm)
            {
                unison_vibrato[nvoice].step[1]     = 0.0f;
            }
        }

        // phase invert for unison
        unison_invert_phase[nvoice][0] = false;
        if (unison != 1)
        {
            int inv = adpars.VoicePar[nvoice].Unison_invert_phase;
            switch(inv)
            {
                case 0:
                    for (int k = 0; k < unison; ++k)
                        unison_invert_phase[nvoice][k] = false;
                    break;

                case 1:
                    for (int k = 0; k < unison; ++k)
                        unison_invert_phase[nvoice][k] = _SYS_::F2B(paramRNG.numRandom());
                    break;

                default:
                    for (int k = 0; k < unison; ++k)
                        unison_invert_phase[nvoice][k] = (k % inv == 0) ? true : false;
                    break;
            }
        }
    }
}

// Get Voice's Modullator base frequency
float ADnote::getFMVoiceBaseFreq(int nvoice)
{
    float detune = NoteVoicePar[nvoice].fmDetune / 100.0f;
    float freq;

    if (NoteVoicePar[nvoice].fmFreqFixed)
        return 440.0f * power<2>(detune / 12.0f);

    if (NoteVoicePar[nvoice].fmDetuneFromBaseOsc)
        freq = getVoiceBaseFreq(nvoice);
    else {
        freq = note.freq;
        // To avoid applying global detuning twice: Only detune in main voice
        if (subVoiceNr == -1)
            detune += noteGlobal.detune / 100.0f;
    }

    return freq * power<2>(detune / 12.0f);
}


// Computes the relative frequency of each unison voice and it's vibrato
// This must be called before setfreq* functions
void ADnote::computeUnisonFreqRap(int nvoice)
{
    if (unison_size[nvoice] == 1) // no unison
    {
        unison_freq_rap[nvoice][0] = 1.0f;
        return;
    }
    float relbw = ctl.bandwidth.relbw * bandwidthDetuneMultiplier;
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        float pos  = unison_vibrato[nvoice].position[k];
        float step = unison_vibrato[nvoice].step[k];
        pos += step;
        if (pos <= -1.0f)
        {
            pos  = -1.0f;
            step = -step;
        }
        else if (pos >= 1.0f)
        {
            pos  = 1.0f;
            step = -step;
        }
        float vibrato_val =
            (pos - 0.333333333f * pos * pos * pos) * 1.5f; // make the vibrato lfo smoother
        unison_freq_rap[nvoice][k] =
            1.0f + ((unison_base_freq_rap[nvoice][k] - 1.0f)
            + vibrato_val * unison_vibrato[nvoice].amplitude) * relbw;

        unison_vibrato[nvoice].position[k] = pos;
        step = unison_vibrato[nvoice].step[k] = step;
    }
}


// Computes the frequency of an oscillator
void ADnote::setfreq(int nvoice, float in_freq, float pitchdetune)
{
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        float detunefactor = unison_freq_rap[nvoice][k] * unisonDetuneFactorFromParent;
        float freq  = fabsf(in_freq) * detunefactor;
        if (subVoice[nvoice]) {
            subVoice[nvoice][k]->setPitchDetuneFromParent(pitchdetune);
            subVoice[nvoice][k]->setUnisonDetuneFromParent(detunefactor);
        }
        float speed = freq * synth.oscil_sample_step_f;
        if (isgreater(speed, synth.oscilsize_f))
            speed = synth.oscilsize_f;
        int skip = int(speed);
        oscfreqhi[nvoice][k] = skip;
        oscfreqlo[nvoice][k] = speed - float(skip);
    }
}


// Computes the frequency of an modulator oscillator
void ADnote::setfreqFM(int nvoice, float in_freq, float pitchdetune)
{
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        float detunefactor = unisonDetuneFactorFromParent;
        if (NoteVoicePar[nvoice].fmDetuneFromBaseOsc)
            detunefactor *= unison_freq_rap[nvoice][k];
        float freq = fabsf(in_freq) * detunefactor;
        if (subFMVoice[nvoice]) {
            subFMVoice[nvoice][k]->setPitchDetuneFromParent(pitchdetune);
            subFMVoice[nvoice][k]->setUnisonDetuneFromParent(detunefactor);
        }
        float speed = freq * synth.oscil_sample_step_f;
        if (isgreater(speed, synth.oscilsize_f))
            speed = synth.oscilsize_f;
        int skip = int(speed);
        oscfreqhiFM[nvoice][k] = skip;
        oscfreqloFM[nvoice][k] = speed - float(skip);
    }
}


// Get Voice base frequency
float ADnote::getVoiceBaseFreq(int nvoice)
{
    float detune =
        NoteVoicePar[nvoice].detune / 100.0f + NoteVoicePar[nvoice].fineDetune /
        100.0f * ctl.bandwidth.relbw * bandwidthDetuneMultiplier;

    // To avoid applying global detuning twice: Only detune in main voice
    if (subVoiceNr == -1)
        detune += noteGlobal.detune / 100.0f;

    if (!NoteVoicePar[nvoice].fixedFreq)
        return note.freq * power<2>(detune / 12.0f);
    else // fixed freq is enabled
    {
        float fixedfreq;
        if (subVoiceNr != -1)
            // Fixed frequency is not used in sub voices.
            // We get the base frequency from the parent.
            fixedfreq = note.freq;
        else
            fixedfreq = 440.0f;
        int fixedfreqET = NoteVoicePar[nvoice].fixedFreqET;
        if (fixedfreqET)
        {   // if the frequency varies according the keyboard note
            float tmp = (note.midi - 69.0f) / 12.0f * (power<2>((fixedfreqET - 1) / 63.0f) - 1.0f);
            if (fixedfreqET <= 64)
                fixedfreq *= power<2>(tmp);
            else
                fixedfreq *= power<3>(tmp);
        }
        return fixedfreq * power<2>(detune / 12.0f);
    }
}


// Computes all the parameters for each tick
void ADnote::computeWorkingParameters(void)
{
    float filterCenterPitch =
        adpars.GlobalPar.GlobalFilter->getfreq() // center freq
        + adpars.GlobalPar.PFilterVelocityScale / 127.0f * 6.0f
        * (velF(note.vel, adpars.GlobalPar.PFilterVelocityScaleFunction) - 1);

    float filterQ = adpars.GlobalPar.GlobalFilter->getq();
    float filterFreqTracking =
        adpars.GlobalPar.GlobalFilter->getfreqtracking(note.freq);

    float filterpitch, filterfreq;
    float globalpitch = 0.01f * (noteGlobal.freqEnvelope->envout()
                       + noteGlobal.freqLFO->lfoout() * ctl.modwheel.relmod);
    globaloldamplitude = globalnewamplitude;
    globalnewamplitude = noteGlobal.volume
                         * noteGlobal.ampEnvelope->envout_dB()
                         * noteGlobal.ampLFO->amplfoout();
    float globalfilterpitch = noteGlobal.filterEnvelope->envout()
                              + noteGlobal.filterLFO->lfoout()
                              + filterCenterPitch;

    float tmpfilterfreq = globalfilterpitch + ctl.filtercutoff.relfreq
          + filterFreqTracking;

    tmpfilterfreq = noteGlobal.filterL->getrealfreq(tmpfilterfreq);
    float globalfilterq = filterQ * ctl.filterq.relq;
    noteGlobal.filterL->setfreq_and_q(tmpfilterfreq, globalfilterq);
    if (stereo)
        noteGlobal.filterR->setfreq_and_q(tmpfilterfreq, globalfilterq);

    // compute the portamento, if it is used by this note
    float portamentofreqrap = 1.0f;
    if (portamento) // this voice use portamento
    {
        portamentofreqrap = ctl.portamento.freqrap;
        if (not ctl.portamento.used) // the portamento has finished
            portamento = false;       // this note is no longer "portamented"

    }

    // compute parameters for all voices
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].enabled)
            continue;
        NoteVoicePar[nvoice].delayTicks -= 1;
        if (NoteVoicePar[nvoice].delayTicks > 0)
            continue;

        computeUnisonFreqRap(nvoice);

        // Voice Amplitude
        oldAmplitude[nvoice] = newAmplitude[nvoice];
        newAmplitude[nvoice] = 1.0f;

        if (NoteVoicePar[nvoice].ampEnvelope)
            newAmplitude[nvoice] *= NoteVoicePar[nvoice].ampEnvelope->envout_dB();

        if (NoteVoicePar[nvoice].ampLFO)
            newAmplitude[nvoice] *= NoteVoicePar[nvoice].ampLFO->amplfoout();

        // Voice Filter
        if (NoteVoicePar[nvoice].voiceFilterL)
        {
            filterpitch =
                adpars.VoicePar[nvoice].VoiceFilter->getfreq()
                + adpars.VoicePar[nvoice].PFilterVelocityScale
                / 127.0f * 6.0f       //velocity sensing
                * (velF(note.vel,
                        adpars.VoicePar[nvoice].PFilterVelocityScaleFunction) - 1);
            filterQ = adpars.VoicePar[nvoice].VoiceFilter->getq();
            if (NoteVoicePar[nvoice].filterEnvelope)
                filterpitch += NoteVoicePar[nvoice].filterEnvelope->envout();
            if (NoteVoicePar[nvoice].filterLFO)
                filterpitch += NoteVoicePar[nvoice].filterLFO->lfoout();
            filterfreq = filterpitch + adpars.VoicePar[nvoice].VoiceFilter->getfreqtracking(note.freq);
            filterfreq = NoteVoicePar[nvoice].voiceFilterL->getrealfreq(filterfreq);
            NoteVoicePar[nvoice].voiceFilterL->setfreq_and_q(filterfreq, filterQ);
            if (stereo && NoteVoicePar[nvoice].voiceFilterR)
                NoteVoicePar[nvoice].voiceFilterR->setfreq_and_q(filterfreq, filterQ);

        }
        if (!NoteVoicePar[nvoice].noiseType) // voice is not noise
        {
            // Voice Frequency
            float basevoicepitch = 0.0f;
            basevoicepitch += detuneFromParent;

            basevoicepitch += 12.0f * NoteVoicePar[nvoice].bendAdjust *
                log2f(ctl.pitchwheel.relfreq); //change the frequency by the controller

            float voicepitch = basevoicepitch;
            if (NoteVoicePar[nvoice].freqLFO)
            {
                voicepitch += NoteVoicePar[nvoice].freqLFO->lfoout() / 100.0f
                              * ctl.bandwidth.relbw;
            }

            if (NoteVoicePar[nvoice].freqEnvelope)
            {
                voicepitch += NoteVoicePar[nvoice].freqEnvelope->envout() / 100.0f;
            }

            float nonoffsetfreq = getVoiceBaseFreq(nvoice)
                                * power<2>((voicepitch + globalpitch) / 12.0f);
            nonoffsetfreq *= portamentofreqrap;
            float voicefreq = nonoffsetfreq + NoteVoicePar[nvoice].offsetHz;
            voicepitch += log2f(voicefreq / nonoffsetfreq) * 12.0f;
            setfreq(nvoice, voicefreq, voicepitch);

            // Modulator
            if (NoteVoicePar[nvoice].fmEnabled != NONE)
            {
                float FMpitch;
                if (NoteVoicePar[nvoice].fmFreqFixed)
                    FMpitch = 0.0f;
                else if (NoteVoicePar[nvoice].fmDetuneFromBaseOsc)
                    FMpitch = voicepitch;
                else
                    FMpitch = basevoicepitch;

                float FMrelativepitch = 0.0f;
                if (NoteVoicePar[nvoice].fmFreqEnvelope) {
                    FMrelativepitch +=
                        NoteVoicePar[nvoice].fmFreqEnvelope->envout() / 100.0f;
                    FMpitch += FMrelativepitch;
                    // Do not add any more adjustments to FMpitch after
                    // this. The rest of FMrelativepitch has already been
                    // accounted for in our sub voices when we created them,
                    // using getFMVoiceBaseFreq().
                }

                float FMfreq;
                if (NoteVoicePar[nvoice].fmFreqFixed) {
                    // Apply FM detuning since base frequency is 440Hz.
                    FMrelativepitch += NoteVoicePar[nvoice].fmDetune / 100.0f;
                    FMfreq = power<2>(FMrelativepitch / 12.0f) * 440.0f;
                } else if (NoteVoicePar[nvoice].fmDetuneFromBaseOsc) {
                    // Apply FM detuning since base frequency is from main voice.
                    FMrelativepitch += NoteVoicePar[nvoice].fmDetune / 100.0f;
                    FMfreq = power<2>(FMrelativepitch / 12.0f) * voicefreq;
                } else {
                    // No need to apply FM detuning, since getFMVoiceBaseFreq()
                    // takes it into account.
                    FMfreq = getFMVoiceBaseFreq(nvoice) *
                        power<2>((basevoicepitch + globalpitch + FMrelativepitch) / 12.0f);
                    FMfreq *= portamentofreqrap;
                }
                setfreqFM(nvoice, FMfreq, FMpitch);
                fm_oldAmplitude[nvoice] = fm_newAmplitude[nvoice];
                fm_newAmplitude[nvoice] = NoteVoicePar[nvoice].fmVolume * ctl.fmamp.relamp;
                if (NoteVoicePar[nvoice].fmAmpEnvelope)
                    fm_newAmplitude[nvoice] *= NoteVoicePar[nvoice].fmAmpEnvelope->envout_dB();
            }
        }
    }
}


// Fadein in a way that removes clicks but keep sound "punchy"
void ADnote::fadein(Samples& smps)
{
    int zerocrossings = 0;
    for (int i = 1; i < synth.sent_buffersize; ++i)
        if (smps[i - 1] < 0.0f && smps[i] > 0.0f)
            zerocrossings++; // this is only the positive crossings

    float tmp = (synth.sent_buffersize - 1.0f) / (zerocrossings + 1) / 3.0f;
    if (tmp < 8.0f)
        tmp = 8.0f;
    tmp *= noteGlobal.fadeinAdjustment;

    int fadein = int(tmp); // how many samples is the fade-in
    if (fadein < 8)
        fadein = 8;
    if (fadein > synth.sent_buffersize)
        fadein = synth.sent_buffersize;
    for (int i = 0; i < fadein; ++i) // fade-in
    {
        float tmp = 0.5f - cosf((float)i / (float) fadein * PI) * 0.5f;
        smps[i] *= tmp;
    }
}


// ported from, zynaddubfx 2.4.4

/*
 * Computes the Oscillator (Without Modulation) - LinearInterpolation
 */

/* As the code here is a bit odd due to optimization, here is what happens
 * First the current position and frequency are retrieved from the running
 * state. These are broken up into high and low portions to indicate how many
 * samples are skipped in one step and how many fractional samples are skipped.
 * Outside of this method the fractional samples are just handled with floating
 * point code, but that's a bit slower than it needs to be. In this code the low
 * portions are known to exist between 0.0 and 1.0 and it is known that they are
 * stored in single precision floating point IEEE numbers. This implies that
 * a maximum of 24 bits are significant. The below code does your standard
 * linear interpolation that you'll see throughout this codebase, but by
 * sticking to integers for tracking the overflow of the low portion, around 15%
 * of the execution time was shaved off in the ADnote test.
 */
inline void ADnote::computeVoiceOscillatorLinearInterpolation(int nvoice)
{
    fft::Waveform const& smps = NoteVoicePar[nvoice].oscilSmp;

    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        int    poshi  = oscposhi[nvoice][k];
        int    poslo  = oscposlo[nvoice][k] * (1<<24);
        int    freqhi = oscfreqhi[nvoice][k];
        int    freqlo = oscfreqlo[nvoice][k] * (1<<24);
        Samples& tw   = tmpwave_unison[k];
        assert(oscfreqlo[nvoice][k] < 1.0f);
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            tw[i]  = (smps[poshi] * ((1<<24) - poslo) + smps[poshi + 1] * poslo)/(1.0f*(1<<24));
            poslo += freqlo;
            poshi += freqhi + (poslo>>24);
            poslo &= 0xffffff;
            poshi &= synth.oscilsize - 1;
        }
        oscposhi[nvoice][k] = poshi;
        oscposlo[nvoice][k] = poslo/(1.0f*(1<<24));
    }
}

// end of port

// Applies the Oscillator (Morphing)
void ADnote::applyVoiceOscillatorMorph(int nvoice)
{
    if (isgreater(fm_newAmplitude[nvoice], 1.0f))
        fm_newAmplitude[nvoice] = 1.0f;
    if (isgreater(fm_oldAmplitude[nvoice], 1.0f))
        fm_oldAmplitude[nvoice] = 1.0f;

    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        Samples&  tw = tmpwave_unison[k];
        Samples& mod = tmpmod_unison[k];

        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            float amp = interpolateAmplitude(fm_oldAmplitude[nvoice],
                                       fm_newAmplitude[nvoice], i,
                                       synth.sent_buffersize);
            tw[i] = (tw[i] * (1.0f - amp)) + amp * mod[i];
        }
    }
}


// Applies the Oscillator (Ring Modulation)
void ADnote::applyVoiceOscillatorRingModulation(int nvoice)
{
    float amp;
    bool isSide = NoteVoicePar[nvoice].fmRingToSide;
    if (isgreater(fm_newAmplitude[nvoice], 1.0f))
        fm_newAmplitude[nvoice] = 1.0f;
    if (isgreater(fm_oldAmplitude[nvoice], 1.0f))
        fm_oldAmplitude[nvoice] = 1.0f;
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        Samples&  tw = tmpwave_unison[k];
        Samples& mod = tmpmod_unison[k];

        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            amp = interpolateAmplitude(fm_oldAmplitude[nvoice],
                                       fm_newAmplitude[nvoice], i,
                                       synth.sent_buffersize);
            if (isSide) // sidebands
                tw[i] *= (mod[i] * amp * 2);
            else // ring
                tw[i] *= mod[i] * amp + (1.0f - amp);
        }
    }
}


// Computes the Modulator
void ADnote::computeVoiceModulator(int nvoice, int FMmode)
{
    if (subFMVoice[nvoice]) {
        int subVoiceNumber = NoteVoicePar[nvoice].fmVoice;
        for (size_t k = 0; k < unison_size[nvoice]; ++k) {
            // Sub voices use voiceOut, so just pass NULL.
            subFMVoice[nvoice][k]->noteout(NULL, NULL);
            Samples const& smps = subFMVoice[nvoice][k]->NoteVoicePar[subVoiceNumber].voiceOut;
            // For historical/compatibility reasons we do not reduce volume here
            // if are using stereo. See same section in computeVoiceOscillator.
            memcpy(tmpmod_unison[k].get(), smps.get(), synth.bufferbytes);
        }
    }
    else if (parentFMmod != NULL) {
        if (NoteVoicePar[nvoice].fmEnabled == FREQ_MOD) {
            computeVoiceModulatorForFMFrequencyModulation(nvoice);
        } else {
            computeVoiceModulatorFrequencyModulation(nvoice, FMmode);
        }
    } else {
        computeVoiceModulatorLinearInterpolation(nvoice);
    }

    if (freqbasedmod[nvoice])
    {
        applyAmplitudeOnVoiceModulator(nvoice);
        normalizeVoiceModulatorFrequencyModulation(nvoice, FMmode);

        // Ring and morph modulation do not need normalization, and they take
        // amplitude into account themselves.
    }
}

void ADnote::applyAmplitudeOnVoiceModulator(int nvoice)
{
   // Amplitude interpolation
    if (aboveAmplitudeThreshold(fm_oldAmplitude[nvoice], fm_newAmplitude[nvoice]))
    {
        for (size_t k = 0; k < unison_size[nvoice]; ++k)
        {
            Samples& unison = tmpmod_unison[k];
            for (size_t i = 0; i < size_t(synth.sent_buffersize); ++i)
                unison[i] *= interpolateAmplitude(fm_oldAmplitude[nvoice],
                                                  fm_newAmplitude[nvoice], i,
                                                  synth.sent_buffersize);
        }
    }
    else
    {
        for (size_t k = 0; k < unison_size[nvoice]; ++k)
        {
            Samples& unison = tmpmod_unison[k];
            for (size_t i = 0; i < size_t(synth.sent_buffersize); ++i)
                unison[i] *= fm_newAmplitude[nvoice];
        }
    }
}

// Normalize the modulator for phase/frequency modulation
void ADnote::normalizeVoiceModulatorFrequencyModulation(int nvoice, int FMmode)
{
    if (FMmode == PW_MOD) { // PWM modulation
        for (size_t k = 1; k < unison_size[nvoice]; k += 2) {
            Samples& unison = tmpmod_unison[k];
            for (size_t i = 1; i < size_t(synth.sent_buffersize); ++i)
                unison[i] = -unison[i];
        }
    }

    // normalize: makes all sample-rates, oscil_sizes to produce same sound
    if (FMmode == FREQ_MOD) // Frequency modulation
    {
        for (size_t k = 0; k < unison_size[nvoice]; ++k)
        {
            Samples& tw = tmpmod_unison[k];
            float fmold = fm_oldSmp[nvoice][k];
            for (int i = 0; i < synth.sent_buffersize; ++i)
            {
                fmold = fmold + tw[i] * synth.oscil_norm_factor_fm;
                tw[i] = fmold;
            }
            fm_oldSmp[nvoice][k] = fmold;
        }
    }
    else  // Phase or PWM modulation
    {
        for (size_t k = 0; k < unison_size[nvoice]; ++k)
        {
            Samples& tw = tmpmod_unison[k];
            for (size_t i = 0; i < size_t(synth.sent_buffersize); ++i)
                tw[i] *= synth.oscil_norm_factor_pm;
        }
    }

    if (parentFMmod != NULL) {
        // This is a sub voice. Mix our frequency modulation with the
        // parent modulation.
        float *tmp = parentFMmod;
        for (size_t k = 0; k < unison_size[nvoice]; ++k) {
            Samples& tw = tmpmod_unison[k];
            for (size_t i = 0; i < size_t(synth.sent_buffersize); ++i)
                tw[i] += tmp[i];
        }
    }
}

// Render the modulator with linear interpolation, no modulation on it
void ADnote::computeVoiceModulatorLinearInterpolation(int nvoice)
{
    fft::Waveform const& smps = NoteVoicePar[nvoice].fmSmp;

    // Compute the modulator and store it in tmpmod_unison[][]
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        int poshiFM     =  oscposhiFM[nvoice][k];
        float posloFM   =  oscposloFM[nvoice][k];
        int freqhiFM    = oscfreqhiFM[nvoice][k];
        float freqloFM  = oscfreqloFM[nvoice][k];
        Samples& unison = tmpmod_unison[k];

        for (size_t i = 0; i < size_t(synth.sent_buffersize); ++i)
        {
            unison[i] = smps[poshiFM] * (1.0f - posloFM)
                      + smps[poshiFM+1] * posloFM;

            posloFM += freqloFM;
            if (posloFM >= 1.0f)
            {
                posloFM -= 1.0f;
                poshiFM++;
            }
            poshiFM += freqhiFM;
            poshiFM &= synth.oscilsize - 1;
        }
        oscposhiFM[nvoice][k] = poshiFM;
        oscposloFM[nvoice][k] = posloFM;
    }
}

// Computes the Modulator (Phase Modulation or Frequency Modulation from parent voice)
void ADnote::computeVoiceModulatorFrequencyModulation(int nvoice, int FMmode)
{
    fft::Waveform const& smps = NoteVoicePar[nvoice].fmSmp;

    // do the modulation using parent's modulator, onto a new modulator
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        Samples& unison = tmpmod_unison[k];
        int poshiFM    =  oscposhiFM[nvoice][k];
        float posloFM  =  oscposloFM[nvoice][k];
        int freqhiFM   = oscfreqhiFM[nvoice][k];
        float freqloFM = oscfreqloFM[nvoice][k];
        // When we have parent modulation, we want to maintain the same
        // sound. However, if the carrier and modulator are very far apart in
        // frequency, then the modulation will affect them very differently,
        // since the phase difference is linear, not logarithmic. Compensate for
        // this by favouring the carrier, and adjust the rate of modulation
        // logarithmically, relative to this.
        float oscVsFMratio = float(freqhiFM + freqloFM)
                           / float(oscfreqhi[nvoice][k] + oscfreqlo[nvoice][k]);

        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            float pMod = parentFMmod[i] * oscVsFMratio;
            int FMmodfreqhi = int(pMod);
            float FMmodfreqlo = pMod-FMmodfreqhi;
            if (FMmodfreqhi < 0)
                ++FMmodfreqlo;

            // carrier, which will become the new modulator
            int carposhi = poshiFM + FMmodfreqhi;
            float carposlo = posloFM + FMmodfreqlo;

            if (FMmode == PW_MOD && (k & 1))
                carposhi += NoteVoicePar[nvoice].phaseOffset;

            if (carposlo >= 1.0f)
            {
                ++carposhi;
                carposlo -= 1.0f;
            }
            carposhi &= (synth.oscilsize - 1);

            unison[i] = smps[carposhi] * (1.0f - carposlo)
                      + smps[carposhi+1] * carposlo;
            posloFM += freqloFM;
            if (posloFM >= 1.0f)
            {
                posloFM -= 1.0f;
                poshiFM++;
            }

            poshiFM += freqhiFM;
            poshiFM &= synth.oscilsize - 1;
        }
        oscposhiFM[nvoice][k] = poshiFM;
        oscposloFM[nvoice][k] = posloFM;
    }
}

void ADnote::computeVoiceModulatorForFMFrequencyModulation(int nvoice)
{
    // Here we have a tricky situation: We are generating a modulator which will
    // be used for FM modulation, and the modulator itself is also modulated by
    // a parent voice. Because FM modulation needs to be integrated (it is the
    // derivative function of PM modulation), we cannot modulate the modulator
    // in the same way as the other modulation types. Instead, we start with the
    // original unmodulated function, and then integrate either backwards or
    // forwards until we reach the phase offset from the parent modulation. To
    // preserve accuracy we move in exact steps of the frequency, which is what
    // would have happened if there was no modulation. Then we take the linear
    // interpolation between the two nearest samples, and use that to construct
    // the resulting curve.

    fft::Waveform const& smps = NoteVoicePar[nvoice].fmSmp;

    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        Samples& unison = tmpmod_unison[k];
        int    poshiFM =  oscposhiFM[nvoice][k];
        float  posloFM =  oscposloFM[nvoice][k];
        int   freqhiFM = oscfreqhiFM[nvoice][k];
        float freqloFM = oscfreqloFM[nvoice][k];
        float freqFM   = float(freqhiFM) + freqloFM;
        float oscVsFMratio = freqFM / float(oscfreqhi[nvoice][k] + oscfreqlo[nvoice][k]);
        float oldInterpPhase = fmfm_oldInterpPhase[nvoice][k];
        float currentPhase = fmfm_oldPhase[nvoice][k];
        float currentPMod = fmfm_oldPMod[nvoice][k];

        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            float pMod = parentFMmod[i] * oscVsFMratio;
            while (currentPMod > pMod)
            {
                posloFM -= freqloFM;
                if (posloFM < 0.0f)
                {
                    posloFM += 1.0f;
                    poshiFM--;
                }
                poshiFM -= freqhiFM;
                poshiFM &= synth.oscilsize - 1;

                currentPMod -= freqFM;
                currentPhase -= smps[poshiFM] * (1.0f - posloFM)
                    + smps[poshiFM + 1] * posloFM;
            }
            float pModBelow = pMod - freqFM;
            while (currentPMod < pModBelow)
            {
                currentPMod += freqFM;
                currentPhase += smps[poshiFM] * (1.0f - posloFM)
                    + smps[poshiFM + 1] * posloFM;

                posloFM += freqloFM;
                if (posloFM >= 1.0f)
                {
                    posloFM -= 1.0f;
                    poshiFM++;
                }
                poshiFM += freqhiFM;
                poshiFM &= synth.oscilsize - 1;
            }

            float nextPhase = currentPhase
                + (smps[poshiFM] * (1.0f - posloFM)
                   + smps[poshiFM + 1] * posloFM);

            posloFM += freqloFM;
            if (posloFM >= 1.0f)
            {
                posloFM -= 1.0f;
                poshiFM++;
            }
            poshiFM += freqhiFM;
            poshiFM &= synth.oscilsize - 1;

            float nextAmount  = (pMod - currentPMod) / freqFM;
            float currAmount  = 1.0f - nextAmount;
            float interpPhase = currentPhase * currAmount
                              + nextPhase * nextAmount;
            unison[i] = interpPhase - oldInterpPhase;
            oldInterpPhase = interpPhase;

            currentPhase = nextPhase;
        }
        oscposhiFM[nvoice][k] = poshiFM;
        oscposloFM[nvoice][k] = posloFM;
        fmfm_oldPhase[nvoice][k] = currentPhase;
        fmfm_oldPMod [nvoice][k] = currentPMod;
        fmfm_oldInterpPhase[nvoice][k] = oldInterpPhase;
    }
}

// Computes the Oscillator (Phase Modulation or Frequency Modulation)
void ADnote::computeVoiceOscillatorFrequencyModulation(int nvoice)
{
    // do the modulation
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        Samples& unison = tmpwave_unison[k];
        int poshi    =  oscposhi[nvoice][k];
        float poslo  =  oscposlo[nvoice][k];
        int freqhi   = oscfreqhi[nvoice][k];
        float freqlo = oscfreqlo[nvoice][k];
        // If this ADnote has frequency based modulation, the modulator resides
        // in tmpmod_unison, otherwise it comes from the parent. If there is no
        // modulation at all this function should not be called.
        const float *mod = freqbasedmod[nvoice] ? tmpmod_unison[k].get() : parentFMmod;

        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            int FMmodfreqhi = int(mod[i]);
            float FMmodfreqlo = mod[i]-FMmodfreqhi;
            if (FMmodfreqhi < 0)
                FMmodfreqlo++;

            // carrier
            int carposhi = poshi + FMmodfreqhi;
            float carposlo = poslo + FMmodfreqlo;

            if (carposlo >= 1.0f)
            {
                carposhi++;
                carposlo -= 1.0f;
            }
            carposhi &= (synth.oscilsize - 1);

            unison[i] = NoteVoicePar[nvoice].oscilSmp[carposhi] * (1.0f - carposlo)
                      + NoteVoicePar[nvoice].oscilSmp[carposhi + 1] * carposlo;
            poslo += freqlo;
            if (poslo >= 1.0f)
            {
                poslo -= 1.0f;
                poshi++;
            }

            poshi += freqhi;
            poshi &= synth.oscilsize - 1;
        }
        oscposhi[nvoice][k] = poshi;
        oscposlo[nvoice][k] = poslo;
    }
}

void ADnote::computeVoiceOscillatorForFMFrequencyModulation(int nvoice)
{
    fft::Waveform const& smps = NoteVoicePar[nvoice].oscilSmp;

    // See computeVoiceModulatorForFMFrequencyModulation for details on how this works.
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        Samples& unison = tmpwave_unison[k];
        int poshi = oscposhi[nvoice][k];
        float poslo = oscposlo[nvoice][k];
        int freqhi = oscfreqhi[nvoice][k];
        float freqlo = oscfreqlo[nvoice][k];
        float freq = (float)freqhi + freqlo;
        float oldInterpPhase = fm_oldOscInterpPhase[nvoice][k];
        float currentPhase = fm_oldOscPhase[nvoice][k];
        float currentPMod = fm_oldOscPMod[nvoice][k];

        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            float pMod = parentFMmod[i];
            while (currentPMod > pMod)
            {
                poslo -= freqlo;
                if (poslo < 0.0f)
                {
                    poslo += 1.0f;
                    poshi--;
                }
                poshi -= freqhi;
                poshi &= synth.oscilsize - 1;

                currentPMod -= freq;
                currentPhase -= smps[poshi] * (1.0f - poslo)
                    + smps[poshi + 1] * poslo;
            }
            float pModBelow = pMod - freq;
            while (currentPMod < pModBelow)
            {
                currentPMod += freq;
                currentPhase += smps[poshi] * (1.0f - poslo)
                    + smps[poshi + 1] * poslo;

                poslo += freqlo;
                if (poslo >= 1.0f)
                {
                    poslo -= 1.0f;
                    poshi++;
                }
                poshi += freqhi;
                poshi &= synth.oscilsize - 1;
            }

            float nextPhase = currentPhase
                + (smps[poshi] * (1.0f - poslo)
                   + smps[poshi + 1] * poslo);

            poslo += freqlo;
            if (poslo >= 1.0f)
            {
                poslo -= 1.0f;
                poshi++;
            }
            poshi += freqhi;
            poshi &= synth.oscilsize - 1;

            float nextAmount = (pMod - currentPMod) / freq;
            float currentAmount= 1.0f - nextAmount;
            float interpPhase = currentPhase * currentAmount
                              + nextPhase * nextAmount;
            unison[i] = interpPhase - oldInterpPhase;
            oldInterpPhase = interpPhase;

            currentPhase = nextPhase;
        }
        oscposhi[nvoice][k] = poshi;
        oscposlo[nvoice][k] = poslo;
        fm_oldOscPhase[nvoice][k] = currentPhase;
        fm_oldOscPMod[nvoice][k]  = currentPMod;
        fm_oldOscInterpPhase[nvoice][k] = oldInterpPhase;
    }
}



// Computes the Noise
void ADnote::computeVoiceNoise(int nvoice)
{
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        Samples& tw = tmpwave_unison[k];
        for (size_t i = 0; i < size_t(synth.sent_buffersize); ++i)
            tw[i] = synth.numRandom() * 2.0f - 1.0f;
    }
}


// ported from Zyn 2.5.2
void ADnote::ComputeVoicePinkNoise(int nvoice)
{
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        Samples& tw = tmpwave_unison[k];
        float *f = &pinking[nvoice][k > 0 ? 7 : 0];
        for (int i = 0; i < synth.sent_buffersize; ++i)
        {
            float white = (synth.numRandom() - 0.5 ) / 4.0;
            f[0] = 0.99886*f[0]+white*0.0555179;
            f[1] = 0.99332*f[1]+white*0.0750759;
            f[2] = 0.96900*f[2]+white*0.1538520;
            f[3] = 0.86650*f[3]+white*0.3104856;
            f[4] = 0.55000*f[4]+white*0.5329522;
            f[5] = -0.7616*f[5]-white*0.0168980;
            tw[i] = f[0]+f[1]+f[2]+f[3]+f[4]+f[5]+f[6]+white*0.5362;
            f[6] = white*0.115926;
        }
    }
}

void ADnote::computeVoiceOscillator(int nvoice)
{
    if (subVoice[nvoice]) {
        int subVoiceNumber = NoteVoicePar[nvoice].voice;
        for (size_t k = 0; k < unison_size[nvoice]; ++k) {
            // Sub voices use voiceOut, so just pass NULL.
            subVoice[nvoice][k]->noteout(NULL, NULL);
            Samples& smps = subVoice[nvoice][k]->NoteVoicePar[subVoiceNumber].voiceOut;
            Samples& unison = tmpwave_unison[k];
            if (stereo) {
                // Reduce volume due to stereo being combined to mono.
                for (int i = 0; i < synth.buffersize; ++i) {
                    unison[i] = smps[i] * 0.5f;
                }
            } else {
                memcpy(unison.get(), smps.get(), synth.bufferbytes);
            }
        }
    } else {
        switch (NoteVoicePar[nvoice].noiseType)
        {
            case 0: //  sound
                // There may be frequency modulation coming from the parent,
                // even if this oscillator itself does not have it.
                if (parentFMmod != NULL && forFM)
                    computeVoiceOscillatorForFMFrequencyModulation(nvoice);
                else if (parentFMmod != NULL || freqbasedmod[nvoice])
                    computeVoiceOscillatorFrequencyModulation(nvoice);
                else
                    computeVoiceOscillatorLinearInterpolation(nvoice);
                break;
            case 1:
                computeVoiceNoise(nvoice); // white noise
                break;
            case 2:
                ComputeVoicePinkNoise(nvoice); // pink noise
                break;
            default:
                ComputeVoiceSpotNoise(nvoice); // spot noise
                break;
        }
    }

    // Apply non-frequency modulation onto rendered voice.
    switch(NoteVoicePar[nvoice].fmEnabled)
    {
        case MORPH:
            applyVoiceOscillatorMorph(nvoice);
            break;
        case RING_MOD:
            applyVoiceOscillatorRingModulation(nvoice);
            break;
        default:
            // No additional modulation.
            break;
    }
}

void ADnote::ComputeVoiceSpotNoise(int nvoice)
{
    for (size_t k = 0; k < unison_size[nvoice]; ++k)
    {
        Samples& unison = tmpwave_unison[k];
        for (size_t i = 0; i < size_t(synth.sent_buffersize); ++i)
        {
            if (tSpot <= 0)
            {
                unison[i] = synth.numRandom() * 6.0f - 3.0f;
                tSpot = (synth.randomINT() >> 24);
            }
            else
            {
                unison[i] = 0.0f;
                tSpot--;
            }
        }
    }
}


// Compute the ADnote samples, returns 0 if the note is finished
void ADnote::noteout(float *outl, float *outr)
{
    Config &Runtime = synth.getRuntime();
    Samples& tmpwavel = Runtime.genTmp1;
    Samples& tmpwaver = Runtime.genTmp2;
    Samples& bypassl = Runtime.genTmp3;
    Samples& bypassr = Runtime.genTmp4;
    int i, nvoice;
    if (outl and outr) {
        memset(outl, 0, synth.sent_bufferbytes);
        memset(outr, 0, synth.sent_bufferbytes);
    }
    if (noteStatus == NOTE_DISABLED) return;

    if (subVoiceNr == -1) {
        memset(bypassl.get(), 0, synth.sent_bufferbytes);
        memset(bypassr.get(), 0, synth.sent_bufferbytes);
    }

    if (paramsUpdate.checkUpdated())
        computeNoteParameters();

    computeWorkingParameters();

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].enabled || NoteVoicePar[nvoice].delayTicks > 0)
            continue;

        if (NoteVoicePar[nvoice].fmEnabled != NONE)
            computeVoiceModulator(nvoice, NoteVoicePar[nvoice].fmEnabled);

        computeVoiceOscillator(nvoice);

        // Mix subvoices into voice
        memset(tmpwavel.get(), 0, synth.sent_bufferbytes);
        if (stereo)
            memset(tmpwaver.get(), 0, synth.sent_bufferbytes);
        for (size_t k = 0; k < unison_size[nvoice]; ++k)
        {
            Samples& unison = tmpwave_unison[k];
            if (stereo)
            {
                float stereo_pos = 0.0f;
                bool is_pwm = NoteVoicePar[nvoice].fmEnabled == PW_MOD;
                if (is_pwm)
                {
                    if (unison_size[nvoice] > 2)
                        stereo_pos = k/2 / (float)((unison_size[nvoice] / 2) - 1) * 2.0f - 1.0f;
                } else if (unison_size[nvoice] > 1)
                {
                    stereo_pos = (float) k
                        / (float)(unison_size[nvoice]
                                  - 1) * 2.0f - 1.0f;
                }
                float stereo_spread = unison_stereo_spread[nvoice] * 2.0f; // between 0 and 2.0
                if (stereo_spread > 1.0f)
                {
                    float stereo_pos_1 = (stereo_pos >= 0.0f) ? 1.0f : -1.0f;
                    stereo_pos = (2.0f - stereo_spread) * stereo_pos
                                  + (stereo_spread - 1.0f) * stereo_pos_1;
                }
                else
                    stereo_pos *= stereo_spread;

                if (unison_size[nvoice] == 1 || (is_pwm && unison_size[nvoice] == 2))
                    stereo_pos = 0.0f;
                float upan = (stereo_pos + 1.0f) * 0.5f;
                float lvol = (1.0f - upan) * 2.0f;
                if (lvol > 1.0f)
                    lvol = 1.0f;

                float rvol = upan * 2.0f;
                if (rvol > 1.0f)
                    rvol = 1.0f;

                if (unison_invert_phase[nvoice][k])
                {
                    lvol = -lvol;
                    rvol = -rvol;
                }

                for (i = 0; i < synth.sent_buffersize; ++i)
                    tmpwavel[i] += unison[i] * lvol;
                for (i = 0; i < synth.sent_buffersize; ++i)
                    tmpwaver[i] += unison[i] * rvol;
            }
            else
                for (i = 0; i < synth.sent_buffersize; ++i)
                    tmpwavel[i] += unison[i];
        }

        // reduce the amplitude for large unison sizes
        float unison_amplitude = 1.0f / sqrtf(unison_size[nvoice]);

        // Amplitude
        float oldam = oldAmplitude[nvoice] * unison_amplitude;
        float newam = newAmplitude[nvoice] * unison_amplitude;

        if (aboveAmplitudeThreshold(oldam, newam))
        {
            int rest = synth.sent_buffersize;
            // test if the amplitude if rising and the difference is high
            if (newam > oldam && (newam - oldam) > 0.25f)
            {
                rest = 10;
                if (rest > synth.sent_buffersize)
                    rest = synth.sent_buffersize;
                for (int i = 0; i < synth.sent_buffersize - rest; ++i)
                    tmpwavel[i] *= oldam;
                if (stereo)
                    for (int i = 0; i < synth.sent_buffersize - rest; ++i)
                        tmpwaver[i] *= oldam;
            }
            // Amplitude interpolation
            for (i = 0; i < rest; ++i)
            {
                float amp = interpolateAmplitude(oldam, newam, i, rest);
                tmpwavel[i + (synth.sent_buffersize - rest)] *= amp;
                if (stereo)
                    tmpwaver[i + (synth.sent_buffersize - rest)] *= amp;
            }
        }
        else
        {
            for (i = 0; i < synth.sent_buffersize; ++i)
                tmpwavel[i] *= newam;
            if (stereo)
                for (i = 0; i < synth.sent_buffersize; ++i)
                    tmpwaver[i] *= newam;
        }

        // Fade in
        if (firsttick[nvoice])
        {
            fadein(tmpwavel);
            if (stereo)
                fadein(tmpwaver);
            firsttick[nvoice] = 0;
        }


        // Filter
        if (NoteVoicePar[nvoice].voiceFilterL != NULL)
            NoteVoicePar[nvoice].voiceFilterL->filterout(tmpwavel.get());
        if (stereo && NoteVoicePar[nvoice].voiceFilterR != NULL)
            NoteVoicePar[nvoice].voiceFilterR->filterout(tmpwaver.get());

        // check if the amplitude envelope is finished.
        // if yes, the voice will fadeout
        if (NoteVoicePar[nvoice].ampEnvelope != NULL)
        {
            if (NoteVoicePar[nvoice].ampEnvelope->finished())
            {
                for (i = 0; i < synth.sent_buffersize; ++i)
                    tmpwavel[i] *= 1.0f - (float)i / synth.sent_buffersize_f;
                if (stereo)
                    for (i = 0; i < synth.sent_buffersize; ++i)
                        tmpwaver[i] *= 1.0f - (float)i / synth.sent_buffersize_f;
            }
            // the voice is killed later
        }

        // Put the ADnote samples in VoiceOut (without applying Global volume,
        // because I wish to use this voice as a modulator)
        if (NoteVoicePar[nvoice].voiceOut)
        {
            if (stereo)
                for (i = 0; i < synth.sent_buffersize; ++i)
                    NoteVoicePar[nvoice].voiceOut[i] = tmpwavel[i] + tmpwaver[i];
            else // mono
                for (i = 0; i < synth.sent_buffersize; ++i)
                    NoteVoicePar[nvoice].voiceOut[i] = tmpwavel[i];
            if (NoteVoicePar[nvoice].volume == 0.0f)
                // If we are muted, we are done.
                continue;
        }

        pangainL = adpars.VoicePar[nvoice].pangainL; // assume voice not random pan
        pangainR = adpars.VoicePar[nvoice].pangainR;
        if (adpars.VoicePar[nvoice].PRandom)
        {
            pangainL = NoteVoicePar[nvoice].randpanL;
            pangainR = NoteVoicePar[nvoice].randpanR;
        }

        if (outl != NULL) {
            // Add the voice that do not bypass the filter to out.
            if (!NoteVoicePar[nvoice].filterBypass) // no bypass
            {
                if (stereo)
                {

                    for (i = 0; i < synth.sent_buffersize; ++i) // stereo
                    {
                        outl[i] += tmpwavel[i] * NoteVoicePar[nvoice].volume * pangainL;
                        outr[i] += tmpwaver[i] * NoteVoicePar[nvoice].volume * pangainR;
                    }
                }
                else
                    for (i = 0; i < synth.sent_buffersize; ++i)
                        outl[i] += tmpwavel[i] * NoteVoicePar[nvoice].volume * 0.7f; // mono
            }
            else // bypass the filter
            {
                if (stereo)
                {
                    for (i = 0; i < synth.sent_buffersize; ++i) // stereo
                    {
                        bypassl[i] += tmpwavel[i] * NoteVoicePar[nvoice].volume
                                      * pangainL;
                        bypassr[i] += tmpwaver[i] * NoteVoicePar[nvoice].volume
                                      * pangainR;
                    }
                }
                else
                    for (i = 0; i < synth.sent_buffersize; ++i)
                        bypassl[i] += tmpwavel[i] * NoteVoicePar[nvoice].volume; // mono
            }
            // check if there is necessary to process the voice longer
            // (if the Amplitude envelope isn't finished)
            if (NoteVoicePar[nvoice].ampEnvelope)
                if (NoteVoicePar[nvoice].ampEnvelope->finished())
                    killVoice(nvoice);
        }
    }

    if (outl != NULL) {
        // Processing Global parameters
        noteGlobal.filterL->filterout(outl);

        if (!stereo) // set the right channel=left channel
        {
            memcpy(outr, outl, synth.sent_bufferbytes);
            memcpy(bypassr.get(), bypassl.get(), synth.sent_bufferbytes);
        }
        else
            noteGlobal.filterR->filterout(outr);

        for (i = 0; i < synth.sent_buffersize; ++i)
        {
            outl[i] += bypassl[i];
            outr[i] += bypassr[i];
        }

        pangainL = adpars.GlobalPar.pangainL; // assume it's not random panning ...
        pangainR = adpars.GlobalPar.pangainR;
        if (adpars.GlobalPar.PRandom)         // it is random panning
        {
            pangainL = noteGlobal.randpanL;
            pangainR = noteGlobal.randpanR;
        }

        if (aboveAmplitudeThreshold(globaloldamplitude, globalnewamplitude))
        {
            // Amplitude Interpolation
            for (i = 0; i < synth.sent_buffersize; ++i)
            {
                float tmpvol = interpolateAmplitude(globaloldamplitude,
                                                    globalnewamplitude, i,
                                                    synth.sent_buffersize);
                outl[i] *= tmpvol * pangainL;
                outr[i] *= tmpvol * pangainR;
            }
        }
        else
        {
            for (i = 0; i < synth.sent_buffersize; ++i)
            {
                outl[i] *= globalnewamplitude * pangainL;
                outr[i] *= globalnewamplitude * pangainR;
            }
        }

        // Apply the punch
        if (noteGlobal.punch.enabled)
        {
            for (i = 0; i < synth.sent_buffersize; ++i)
            {
                float punchamp = noteGlobal.punch.initialvalue
                                 * noteGlobal.punch.t + 1.0f;
                outl[i] *= punchamp;
                outr[i] *= punchamp;
                noteGlobal.punch.t -= noteGlobal.punch.dt;
                if (noteGlobal.punch.t < 0.0f)
                {
                    noteGlobal.punch.enabled = false;
                    break;
                }
            }
        }

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
    }

    // Check if the global amplitude is finished.
    // If it does, disable the note
    if (noteGlobal.ampEnvelope->finished())
    {
        if (outl != NULL) {
            for (i = 0; i < synth.sent_buffersize; ++i) // fade-out
            {
                float tmp = 1.0f - (float)i / synth.sent_buffersize_f;
                outl[i] *= tmp;
                outr[i] *= tmp;
            }
        }
        killNote();
        return;
    }
}


// Release the key (NoteOff)
void ADnote::releasekey(void)
{
    if (noteStatus == NOTE_LEGATOFADEOUT)
        return; // keep envelopes in sustained state (thereby blocking NoteOff)

    int nvoice;
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].enabled)
            continue;
        if (NoteVoicePar[nvoice].ampEnvelope)
            NoteVoicePar[nvoice].ampEnvelope->releasekey();
        if (NoteVoicePar[nvoice].freqEnvelope)
            NoteVoicePar[nvoice].freqEnvelope->releasekey();
        if (NoteVoicePar[nvoice].filterEnvelope)
            NoteVoicePar[nvoice].filterEnvelope->releasekey();
        if (NoteVoicePar[nvoice].fmFreqEnvelope)
            NoteVoicePar[nvoice].fmFreqEnvelope->releasekey();
        if (NoteVoicePar[nvoice].fmAmpEnvelope)
            NoteVoicePar[nvoice].fmAmpEnvelope->releasekey();
        if (subVoice[nvoice])
            for (size_t k = 0; k < unison_size[nvoice]; ++k)
                subVoice[nvoice][k]->releasekey();
        if (subFMVoice[nvoice])
            for (size_t k = 0; k < unison_size[nvoice]; ++k)
                subFMVoice[nvoice][k]->releasekey();
    }
    noteGlobal.freqEnvelope->releasekey();
    noteGlobal.filterEnvelope->releasekey();
    noteGlobal.ampEnvelope->releasekey();
}

