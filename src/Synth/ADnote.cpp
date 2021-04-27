/*
    ADnote.cpp - The "additive" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others
    Copyright 2020-2021 Kristian Amlie & Will Godfrey

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

    This file is derivative of original ZynAddSubFX code.

*/

#include <cmath>
#include <cassert>
#include <iostream>

#include "DSP/FFTwrapper.h"
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

using synth::velF;
using synth::getDetune;
using synth::interpolateAmplitude;
using synth::aboveAmplitudeThreshold;
using func::setRandomPan;

using std::isgreater;


ADnote::ADnote(ADnoteParameters *adpars_, Controller *ctl_, float freq_,
               float velocity_, int portamento_, int midinote_, SynthEngine *_synth) :
    adpars(adpars_),
    stereo(adpars->GlobalPar.PStereo),
    midinote(midinote_),
    velocity(velocity_),
    basefreq(freq_),
    NoteStatus(NOTE_ENABLED),
    ctl(ctl_),
    time(0.0f),
    Tspot(0),
    forFM(false),
    portamento(portamento_),
    subVoiceNumber(-1),
    topVoice(this),
    parentFMmod(NULL),
    paramsUpdate(adpars),
    synth(_synth)
{
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        NoteVoicePar[nvoice].phase_offset = 0;

    construct();
}

ADnote::ADnote(ADnote *topVoice_, float freq_, int phase_offset_, int subVoiceNumber_,
               float *parentFMmod_, bool forFM_) :
    adpars(topVoice_->adpars),
    stereo(adpars->GlobalPar.PStereo),
    midinote(topVoice_->midinote),
    velocity(topVoice_->velocity),
    basefreq(freq_),
    NoteStatus(NOTE_ENABLED),
    ctl(topVoice_->ctl),
    time(0.0f),
    forFM(forFM_),
    portamento(topVoice_->portamento),
    subVoiceNumber(subVoiceNumber_),
    topVoice(topVoice_),
    parentFMmod(parentFMmod_),
    paramsUpdate(adpars),
    synth(topVoice_->synth)
{
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        // Start phase: Should be negative so that the zero phase in the first
        // cycle will result in a positive phase change.
        NoteVoicePar[nvoice].phase_offset = synth->oscilsize - phase_offset_;

    construct();
}

// Copy constructor, currently only exists for legato
ADnote::ADnote(const ADnote &orig, ADnote *topVoice_, float *parentFMmod_) :
    adpars(orig.adpars), // Probably okay for legato?
    stereo(orig.stereo),
    midinote(orig.midinote),
    velocity(orig.velocity),
    basefreq(orig.basefreq),
    // For legato. Move this somewhere else if copying
    // notes gets used for another purpose
    NoteStatus(NOTE_KEEPALIVE),
    ctl(orig.ctl),
    NoteGlobalPar(orig.NoteGlobalPar),
    time(orig.time), // This is incremented, but never actually used for some reason
    paramRNG(orig.paramRNG),
    paramSeed(orig.paramSeed),
    detuneFromParent(orig.detuneFromParent),
    unisonDetuneFactorFromParent(orig.unisonDetuneFactorFromParent),
    forFM(orig.forFM),
    max_unison(orig.max_unison),
    globaloldamplitude(orig.globaloldamplitude),
    globalnewamplitude(orig.globalnewamplitude),
    portamento(orig.portamento),
    bandwidthDetuneMultiplier(orig.bandwidthDetuneMultiplier),
    legatoFade(0.0f), // Silent by default
    legatoFadeStep(0.0f), // Legato disabled
    pangainL(orig.pangainL),
    pangainR(orig.pangainR),
    subVoiceNumber(orig.subVoiceNumber),
    topVoice((topVoice_ != NULL) ? topVoice_ : this),
    parentFMmod(parentFMmod_),
    paramsUpdate(adpars),
    synth(orig.synth)
{
    auto &oldgpar = orig.NoteGlobalPar;
    NoteGlobalPar.FreqEnvelope = new Envelope(*oldgpar.FreqEnvelope);
    NoteGlobalPar.FreqLfo = new LFO(*oldgpar.FreqLfo);
    NoteGlobalPar.AmpEnvelope = new Envelope(*oldgpar.AmpEnvelope);
    NoteGlobalPar.AmpLfo = new LFO(*oldgpar.AmpLfo);
    NoteGlobalPar.FilterEnvelope = new Envelope(*oldgpar.FilterEnvelope);
    NoteGlobalPar.FilterLfo = new LFO(*oldgpar.FilterLfo);

    NoteGlobalPar.GlobalFilterL = new Filter(*oldgpar.GlobalFilterL);
    if (stereo)
        NoteGlobalPar.GlobalFilterR = new Filter(*oldgpar.GlobalFilterR);

    // These are all arrays, so sizeof is correct
    memcpy(pinking, orig.pinking, sizeof(pinking));
    memcpy(unison_size, orig.unison_size, sizeof(unison_size));
    memcpy(unison_stereo_spread, orig.unison_stereo_spread,
        sizeof(unison_stereo_spread));
    memcpy(freqbasedmod, orig.freqbasedmod, sizeof(freqbasedmod));
    memcpy(firsttick, orig.firsttick, sizeof(firsttick));

    tmpwave_unison = new float*[max_unison];
    tmpmod_unison = new float*[max_unison];

    for (int i = 0; i < max_unison; ++i)
    {
        tmpwave_unison[i] = (float*)fftwf_malloc(synth->bufferbytes);
        tmpmod_unison[i] = (float*)fftwf_malloc(synth->bufferbytes);
    }

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        auto &oldvpar = orig.NoteVoicePar[i];
        auto &vpar = NoteVoicePar[i];

        vpar.OscilSmp = NULL;
        vpar.FMSmp = NULL;
        vpar.FMEnabled = oldvpar.FMEnabled;

        if (oldvpar.VoiceOut != NULL) {
            vpar.VoiceOut = (float*)fftwf_malloc(synth->bufferbytes);
            // Not sure the memcpy is necessary
            memcpy(vpar.VoiceOut, oldvpar.VoiceOut, synth->bufferbytes);
        } else
            vpar.VoiceOut = NULL;

        // The above vars are checked in killNote() even when the voice is
        // disabled, so short-circuit only after they are set
        vpar.Enabled = oldvpar.Enabled;
        if (!vpar.Enabled)
            continue;

        // First, copy over everything that isn't behind a pointer
        vpar.Voice = oldvpar.Voice;
        vpar.noisetype = oldvpar.noisetype;
        vpar.filterbypass = oldvpar.filterbypass;
        vpar.DelayTicks = oldvpar.DelayTicks;
        vpar.phase_offset = oldvpar.phase_offset;

        vpar.fixedfreq = oldvpar.fixedfreq;
        vpar.fixedfreqET = oldvpar.fixedfreqET;

        vpar.Detune = oldvpar.Detune;
        vpar.FineDetune = oldvpar.FineDetune;
        vpar.BendAdjust = oldvpar.BendAdjust;
        vpar.OffsetHz = oldvpar.OffsetHz;

        vpar.Volume = oldvpar.Volume;
        vpar.Panning = oldvpar.Panning;
        vpar.randpanL = oldvpar.randpanL;
        vpar.randpanR = oldvpar.randpanR;

        vpar.Punch = oldvpar.Punch;

        vpar.FMFreqFixed = oldvpar.FMFreqFixed;
        vpar.FMVoice = oldvpar.FMVoice;
        vpar.FMphase_offset = oldvpar.FMphase_offset;
        vpar.FMVolume = oldvpar.FMVolume;
        vpar.FMDetuneFromBaseOsc = oldvpar.FMDetuneFromBaseOsc;
        vpar.FMDetune = oldvpar.FMDetune;

        // Now handle allocations
        if (subVoiceNumber == -1)
        {
            size_t size = synth->oscilsize + OSCIL_SMP_EXTRA_SAMPLES;

            if (oldvpar.OscilSmp != NULL)
            {
                vpar.OscilSmp = new float[size];
                memcpy(vpar.OscilSmp, oldvpar.OscilSmp, size * sizeof(float));
            }

            if (oldvpar.FMSmp != NULL)
            {
                vpar.FMSmp = (float*)fftwf_malloc(size * sizeof(float));
                memcpy(vpar.FMSmp, oldvpar.FMSmp, size * sizeof(float));
            }
        } else {
            vpar.OscilSmp = topVoice->NoteVoicePar[i].OscilSmp;
            vpar.FMSmp = topVoice->NoteVoicePar[i].FMSmp;
        }

        vpar.FreqEnvelope = oldvpar.FreqEnvelope != NULL ?
            new Envelope(*oldvpar.FreqEnvelope) :
            NULL;
        vpar.FreqLfo = oldvpar.FreqLfo != NULL ?
            new LFO(*oldvpar.FreqLfo) :
            NULL;

        vpar.AmpEnvelope = oldvpar.AmpEnvelope != NULL ?
            new Envelope(*oldvpar.AmpEnvelope) :
            NULL;
        vpar.AmpLfo = oldvpar.AmpLfo != NULL ?
            new LFO(*oldvpar.AmpLfo) :
            NULL;

        if (adpars->VoicePar[i].PFilterEnabled)
        {
            vpar.VoiceFilterL = new Filter(*oldvpar.VoiceFilterL);
            vpar.VoiceFilterR = new Filter(*oldvpar.VoiceFilterR);
        }
        else
        {
            vpar.VoiceFilterL = NULL;
            vpar.VoiceFilterR = NULL;
        }

        vpar.FilterEnvelope = oldvpar.FilterEnvelope != NULL ?
            new Envelope(*oldvpar.FilterEnvelope) :
            NULL;
        vpar.FilterLfo = oldvpar.FilterLfo != NULL ?
            new LFO(*oldvpar.FilterLfo) :
            NULL;

        vpar.FMFreqEnvelope = oldvpar.FMFreqEnvelope != NULL ?
            new Envelope(*oldvpar.FMFreqEnvelope) :
            NULL;
        vpar.FMAmpEnvelope = oldvpar.FMAmpEnvelope != NULL ?
            new Envelope(*oldvpar.FMAmpEnvelope) :
            NULL;

        // NoteVoicePar done

        int unison = unison_size[i];

        oscfreqhi[i] = new int[unison];
        memcpy(oscfreqhi[i], orig.oscfreqhi[i], unison * sizeof(int));

        oscfreqlo[i] = new float[unison];
        memcpy(oscfreqlo[i], orig.oscfreqlo[i], unison * sizeof(float));

        oscfreqhiFM[i] = new int[unison];
        memcpy(oscfreqhiFM[i], orig.oscfreqhiFM[i], unison * sizeof(unsigned int));

        oscfreqloFM[i] = new float[unison];
        memcpy(oscfreqloFM[i], orig.oscfreqloFM[i], unison * sizeof(float));

        oscposhi[i] = new int[unison];
        memcpy(oscposhi[i], orig.oscposhi[i], unison * sizeof(int));

        oscposlo[i] = new float[unison];
        memcpy(oscposlo[i], orig.oscposlo[i], unison * sizeof(float));

        oscposhiFM[i] = new int[unison];
        memcpy(oscposhiFM[i], orig.oscposhiFM[i], unison * sizeof(unsigned int));

        oscposloFM[i] = new float[unison];
        memcpy(oscposloFM[i], orig.oscposloFM[i], unison * sizeof(float));


        unison_base_freq_rap[i] = new float[unison];
        memcpy(unison_base_freq_rap[i], orig.unison_base_freq_rap[i],
            unison * sizeof(float));

        unison_freq_rap[i] = new float[unison];
        memcpy(unison_freq_rap[i], orig.unison_freq_rap[i],
            unison * sizeof(float));

        unison_invert_phase[i] = new bool[unison];
        memcpy(unison_invert_phase[i], orig.unison_invert_phase[i],
            unison * sizeof(bool));

        unison_vibratto[i].amplitude = orig.unison_vibratto[i].amplitude;

        unison_vibratto[i].step = new float[unison];
        memcpy(unison_vibratto[i].step,
            orig.unison_vibratto[i].step, unison * sizeof(float));

        unison_vibratto[i].position = new float[unison];
        memcpy(unison_vibratto[i].position,
            orig.unison_vibratto[i].position, unison * sizeof(float));


        FMoldsmp[i] = new float[unison];
        memcpy(FMoldsmp[i], orig.unison_vibratto[i].position,
            unison * sizeof(float));

        if (parentFMmod != NULL)
        {
            if (NoteVoicePar[i].FMEnabled == FREQ_MOD)
            {
                FMFMoldPhase[i] = new float[unison];
                memcpy(FMFMoldPhase[i], orig.FMFMoldPhase[i],
                    unison * sizeof(float));

                FMFMoldInterpPhase[i] = new float[unison];
                memcpy(FMFMoldInterpPhase[i], orig.FMFMoldInterpPhase[i],
                    unison * sizeof(float));

                FMFMoldPMod[i] = new float[unison];
                memcpy(FMFMoldPMod[i], orig.FMFMoldPMod[i],
                    unison * sizeof(float));
            }

            if (forFM)
            {
                oscFMoldPhase[i] = new float[unison];
                memcpy(oscFMoldPhase[i], orig.oscFMoldPhase[i],
                    unison * sizeof(float));

                oscFMoldInterpPhase[i] = new float[unison];
                memcpy(oscFMoldInterpPhase[i], orig.oscFMoldInterpPhase[i],
                    unison * sizeof(float));

                oscFMoldPMod[i] = new float[unison];
                memcpy(oscFMoldPMod[i], orig.oscFMoldPMod[i],
                    unison * sizeof(float));
            }
        }

        oldamplitude[i] = orig.oldamplitude[i];
        newamplitude[i] = orig.newamplitude[i];
        FMoldamplitude[i] = orig.FMoldamplitude[i];
        FMnewamplitude[i] = orig.FMnewamplitude[i];

        if (orig.subVoice[i] != NULL)
        {
            subVoice[i] = new ADnote*[orig.unison_size[i]];
            for (int k = 0; k < orig.unison_size[i]; ++k)
            {
                subVoice[i][k] = new ADnote(*orig.subVoice[i][k], topVoice, freqbasedmod[i] ? tmpmod_unison[k] : parentFMmod);
            }
        }
        else
            subVoice[i] = NULL;

        if (orig.subFMVoice[i] != NULL)
        {
            subFMVoice[i] = new ADnote*[orig.unison_size[i]];
            for (int k = 0; k < orig.unison_size[i]; ++k)
            {
                subFMVoice[i][k] = new ADnote(*orig.subFMVoice[i][k], topVoice, parentFMmod);
            }
        }
        else
            subFMVoice[i] = NULL;
    }
}

void ADnote::construct()
{
    if (velocity > 1.0f)
        velocity = 1.0f;

    // Initialise some legato-specific vars
    legatoFade = 1.0f; // Full volume
    legatoFadeStep = 0.0f; // Legato disabled

    paramSeed = synth->randomINT();

    setRandomPan(synth->numRandom(), NoteGlobalPar.randpanL, NoteGlobalPar.randpanR, synth->getRuntime().panLaw, adpars->GlobalPar.PPanning, adpars->GlobalPar.PWidth);

    NoteGlobalPar.Fadein_adjustment =
        adpars->GlobalPar.Fadein_adjustment / (float)FADEIN_ADJUSTMENT_SCALE;
    NoteGlobalPar.Fadein_adjustment *= NoteGlobalPar.Fadein_adjustment;
    if (adpars->GlobalPar.PPunchStrength)
    {
        NoteGlobalPar.Punch.Enabled = 1;
        NoteGlobalPar.Punch.t = 1.0f; //start from 1.0 and to 0.0
        NoteGlobalPar.Punch.initialvalue =
            ((powf(10.0f, 1.5f * adpars->GlobalPar.PPunchStrength / 127.0f) - 1.0f)
             * velF(velocity, adpars->GlobalPar.PPunchVelocitySensing));
        float time = powf(10.0f, 3.0f * adpars->GlobalPar.PPunchTime / 127.0f) / 10000.0f; // 0.1 .. 100 ms
        float stretch = powf(440.0f / basefreq, adpars->GlobalPar.PPunchStretch / 64.0f);
        NoteGlobalPar.Punch.dt = 1.0f / (time * synth->samplerate_f * stretch);
    }
    else
        NoteGlobalPar.Punch.Enabled = 0;

    detuneFromParent = 0.0;
    unisonDetuneFactorFromParent = 1.0;

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (int i = 0; i < 14; i++)
            pinking[nvoice][i] = 0.0;

        NoteVoicePar[nvoice].OscilSmp = NULL;
        NoteVoicePar[nvoice].FMSmp = NULL;
        NoteVoicePar[nvoice].VoiceOut = NULL;

        NoteVoicePar[nvoice].FMEnabled = NONE;
        NoteVoicePar[nvoice].FMringToSide = false;
        NoteVoicePar[nvoice].FMVoice = -1;
        unison_size[nvoice] = 1;

        subVoice[nvoice] = NULL;
        subFMVoice[nvoice] = NULL;

        // If used as a sub voice, enable exactly one voice, the requested
        // one. If not, enable voices that are enabled in settings.
        if (!(adpars->VoicePar[nvoice].Enabled
              && (subVoiceNumber == -1 || nvoice == subVoiceNumber)))
        {
            NoteVoicePar[nvoice].Enabled = false;
            continue; // the voice is disabled
        }

        int unison = adpars->VoicePar[nvoice].Unison_size;
        if (unison < 1)
            unison = 1;

        bool is_pwm = adpars->VoicePar[nvoice].PFMEnabled == PW_MOD;

        if (adpars->VoicePar[nvoice].Type != 0)
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

        unison_base_freq_rap[nvoice] = new float[unison];
        unison_freq_rap[nvoice] = new float[unison];
        unison_invert_phase[nvoice] = new bool[unison];
        unison_vibratto[nvoice].step = new float[unison];
        unison_vibratto[nvoice].position = new float[unison];

        if (unison >> is_pwm > 1)
        {
            for (int k = 0; k < unison; ++k)
            {
                unison_vibratto[nvoice].position[k] = synth->numRandom() * 1.8f - 0.9f;

                // Give step a random direction. The amplitude doesn't matter right
                // now, only the sign, which will be preserved in
                // computeNoteParameters().
                if (synth->numRandom() < 0.5f)
                    unison_vibratto[nvoice].step[k] = -1.0f;
                else
                    unison_vibratto[nvoice].step[k] = 1.0f;

                if (is_pwm)
                {
                    // Set the next position the same as this one.
                    unison_vibratto[nvoice].position[k+1] =
                        unison_vibratto[nvoice].position[k];
                    ++k; // Skip an iteration.
                    // step and amplitude are handled in computeNoteParameters.
                }
            }
        }
        else // No vibrato for a single voice
        {
            if (is_pwm)
            {
                unison_vibratto[nvoice].position[1] = 0.0f;
            }
            if (is_pwm || unison == 1)
            {
                unison_vibratto[nvoice].position[0] = 0.0f;
            }
        }

        oscfreqhi[nvoice] = new int[unison];
        oscfreqlo[nvoice] = new float[unison];
        oscfreqhiFM[nvoice] = new int[unison];
        oscfreqloFM[nvoice] = new float[unison];
        oscposhi[nvoice] = new int[unison];
        oscposlo[nvoice] = new float[unison];
        oscposhiFM[nvoice] = new int[unison];
        oscposloFM[nvoice] = new float[unison];

        NoteVoicePar[nvoice].Enabled = true;
        memset(oscposhi[nvoice], 0, unison * sizeof(int));
        memset(oscposlo[nvoice], 0, unison * sizeof(float));
        memset(oscposhiFM[nvoice], 0, unison * sizeof(int));
        memset(oscposloFM[nvoice], 0, unison * sizeof(float));

        NoteVoicePar[nvoice].Voice = adpars->VoicePar[nvoice].PVoice;

        int vc = nvoice;
        if (adpars->VoicePar[nvoice].Pextoscil != -1)
            vc = adpars->VoicePar[nvoice].Pextoscil;
        if (subVoiceNumber == -1) {
            // Draw new seed for randomisation of harmonics
            // Since NoteON happens at random times, this actually injects entropy
            adpars->VoicePar[nvoice].OscilSmp->newrandseed();

            NoteVoicePar[nvoice].OscilSmp = // the extra points contains the first point
                new float[synth->oscilsize + OSCIL_SMP_EXTRA_SAMPLES];

            // Get the voice's oscil or external's voice oscil
            if (!adpars->GlobalPar.Hrandgrouping)
                adpars->VoicePar[vc].OscilSmp->newrandseed();

            // Actual OscilSmp rendering done later.
        } else {
            // If subvoice, use oscillator from original voice.
            NoteVoicePar[nvoice].OscilSmp = topVoice->NoteVoicePar[nvoice].OscilSmp;
        }

        int oscposhi_start;
        if (NoteVoicePar[nvoice].Voice == -1)
            oscposhi_start = adpars->VoicePar[vc].OscilSmp->getPhase();
        else
            oscposhi_start = 0;
        int kth_start = oscposhi_start;
        for (int k = 0; k < unison; ++k)
        {
            oscposhi[nvoice][k] = kth_start % synth->oscilsize;
            // put random starting point for other subvoices
            kth_start = oscposhi_start + (int)(synth->numRandom() * adpars->VoicePar[nvoice].Unison_phase_randomness
                                        / 127.0f * (synth->oscilsize - 1));
        }

        NoteVoicePar[nvoice].FreqLfo = NULL;
        NoteVoicePar[nvoice].FreqEnvelope = NULL;

        NoteVoicePar[nvoice].AmpLfo = NULL;
        NoteVoicePar[nvoice].AmpEnvelope = NULL;

        NoteVoicePar[nvoice].VoiceFilterL = NULL;
        NoteVoicePar[nvoice].VoiceFilterR = NULL;
        NoteVoicePar[nvoice].FilterEnvelope = NULL;
        NoteVoicePar[nvoice].FilterLfo = NULL;

        if (adpars->VoicePar[nvoice].Type != 0)
            NoteVoicePar[nvoice].FMEnabled = NONE;
        else
            switch (adpars->VoicePar[nvoice].PFMEnabled)
            {
                case 1:
                    NoteVoicePar[nvoice].FMEnabled = MORPH;
                    freqbasedmod[nvoice] = false;
                    break;
                case 2:
                    NoteVoicePar[nvoice].FMEnabled = RING_MOD;
                    freqbasedmod[nvoice] = false;
                    break;
                case 3:
                    NoteVoicePar[nvoice].FMEnabled = PHASE_MOD;
                    freqbasedmod[nvoice] = true;
                    break;
                case 4:
                    NoteVoicePar[nvoice].FMEnabled = FREQ_MOD;
                    freqbasedmod[nvoice] = true;
                    break;
                case 5:
                    NoteVoicePar[nvoice].FMEnabled = PW_MOD;
                    freqbasedmod[nvoice] = true;
                    break;
                default:
                    NoteVoicePar[nvoice].FMEnabled = NONE;
                    freqbasedmod[nvoice] = false;
            }
        NoteVoicePar[nvoice].FMringToSide = adpars->VoicePar[nvoice].PFMringToSide;
        NoteVoicePar[nvoice].FMVoice = adpars->VoicePar[nvoice].PFMVoice;
        NoteVoicePar[nvoice].FMFreqEnvelope = NULL;
        NoteVoicePar[nvoice].FMAmpEnvelope = NULL;

        FMoldsmp[nvoice] = new float [unison];
        memset(FMoldsmp[nvoice], 0, unison * sizeof(float));

        firsttick[nvoice] = 1;
        NoteVoicePar[nvoice].DelayTicks =
            (int)((expf(adpars->VoicePar[nvoice].PDelay / 127.0f
            * logf(50.0f)) - 1.0f) / synth->fixed_sample_step_f / 10.0f);

        if (parentFMmod != NULL && NoteVoicePar[nvoice].FMEnabled == FREQ_MOD) {
            FMFMoldPhase[nvoice] = new float [unison];
            memset(FMFMoldPhase[nvoice], 0, unison * sizeof(*FMFMoldPhase[nvoice]));
            FMFMoldInterpPhase[nvoice] = new float [unison];
            memset(FMFMoldInterpPhase[nvoice], 0, unison * sizeof(*FMFMoldInterpPhase[nvoice]));
            FMFMoldPMod[nvoice] = new float [unison];
            memset(FMFMoldPMod[nvoice], 0, unison * sizeof(*FMFMoldPMod[nvoice]));
        }
        if (parentFMmod != NULL && forFM) {
            oscFMoldPhase[nvoice] = new float [unison];
            memset(oscFMoldPhase[nvoice], 0, unison * sizeof(*oscFMoldPhase[nvoice]));
            oscFMoldInterpPhase[nvoice] = new float [unison];
            memset(oscFMoldInterpPhase[nvoice], 0, unison * sizeof(*oscFMoldInterpPhase[nvoice]));
            oscFMoldPMod[nvoice] = new float [unison];
            memset(oscFMoldPMod[nvoice], 0, unison * sizeof(*oscFMoldPMod[nvoice]));
        }
    }

    max_unison = 1;
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        if (unison_size[nvoice] > max_unison)
            max_unison = unison_size[nvoice];

    tmpwave_unison = new float*[max_unison];
    tmpmod_unison = new float*[max_unison];
    for (int k = 0; k < max_unison; ++k)
    {
        tmpwave_unison[k] = (float*)fftwf_malloc(synth->bufferbytes);
        memset(tmpwave_unison[k], 0, synth->bufferbytes);
        tmpmod_unison[k] = (float*)fftwf_malloc(synth->bufferbytes);
        memset(tmpmod_unison[k], 0, synth->bufferbytes);
    }

    initParameters();
    initSubVoices();

    globalnewamplitude = NoteGlobalPar.Volume
                         * NoteGlobalPar.AmpEnvelope->envout_dB()
                         * NoteGlobalPar.AmpLfo->amplfoout();
}

void ADnote::initSubVoices(void)
{
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;

        if (NoteVoicePar[nvoice].Voice != -1)
        {
            subVoice[nvoice] = new ADnote*[unison_size[nvoice]];
            for (int k = 0; k < unison_size[nvoice]; ++k) {
                float *freqmod = freqbasedmod[nvoice] ? tmpmod_unison[k] : parentFMmod;
                subVoice[nvoice][k] = new ADnote(topVoice,
                                                 getVoiceBaseFreq(nvoice),
                                                 oscposhi[nvoice][k],
                                                 NoteVoicePar[nvoice].Voice,
                                                 freqmod, forFM);
            }
        }

        if (NoteVoicePar[nvoice].FMVoice != -1)
        {
            bool voiceForFM = NoteVoicePar[nvoice].FMEnabled == FREQ_MOD;
            subFMVoice[nvoice] = new ADnote*[unison_size[nvoice]];
            for (int k = 0; k < unison_size[nvoice]; ++k) {
                subFMVoice[nvoice][k] = new ADnote(topVoice,
                                                   getFMVoiceBaseFreq(nvoice),
                                                   oscposhiFM[nvoice][k],
                                                   NoteVoicePar[nvoice].FMVoice,
                                                   parentFMmod, voiceForFM);
            }
        }
    }
}

void ADnote::legatoFadeIn(float freq_, float velocity_, int portamento_, int midinote_)
{
    basefreq = freq_;
    velocity = velocity_;
    if (velocity > 1.0)
        velocity = 1.0;
    portamento = portamento_;
    midinote = midinote_;

    if (!portamento) // Do not crossfade portamento
    {
        legatoFade = 0.0f; // Start silent
        legatoFadeStep = synth->fadeStepShort; // Positive steps

        // Re-randomize harmonics, but only if we're not doing portamento
        if (subVoiceNumber == -1)
            for (int i = 0; i < NUM_VOICES; ++i)
            {
                adpars->VoicePar[i].OscilSmp->newrandseed();
                auto &extoscil = adpars->VoicePar[i].Pextoscil;
                if (extoscil != -1 && !adpars->GlobalPar.Hrandgrouping)
                    adpars->VoicePar[extoscil].OscilSmp->newrandseed();
            }

        // This recalculates certain things like harmonic phase/amplitude randomness,
        // which we probably don't want with portamento. This may not even be
        // desirable with plain legato, but it at least makes some sense in that
        // case. Portamento should be a smooth change in pitch, with no change in
        // timbre, or at least a gradual one. It may be desirable to have base
        // frequency sensitive things like filter scaling and envelope stretching
        // take portamento into account, but to do this properly would require more
        // than just recalculating based on basefreq.
        computeNoteParameters();
    }

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        auto &vpar = NoteVoicePar[i];

        if (!vpar.Enabled)
            continue;

        if (subVoice[i] != NULL)
            for (int k = 0; k < unison_size[i]; ++k)
                subVoice[i][k]->legatoFadeIn(getVoiceBaseFreq(i), velocity_, portamento_, midinote_);
        if (subFMVoice[i] != NULL)
            for (int k = 0; k < unison_size[i]; ++k)
                subFMVoice[i][k]->legatoFadeIn(getFMVoiceBaseFreq(i), velocity_, portamento_, midinote_);
    }
}

// This exists purely to avoid boilerplate. It might be useful
// elsewhere, but converting the relevant code to be more
// RAII-friendly would probably be more worthwhile.
template<class T> inline void copyOrAssign(T *lhs, const T *rhs)
{
        if (rhs != NULL)
        {
            if (lhs != NULL)
                *lhs = *rhs;
            else
                lhs = new T(*rhs);
        }
        else
        {
            delete lhs;
            lhs = NULL;
        }
}

void ADnote::legatoFadeOut(const ADnote &orig)
{
    basefreq = orig.basefreq;
    velocity = orig.velocity;
    portamento = orig.portamento;
    midinote = orig.midinote;

    auto &gpar = NoteGlobalPar;
    auto &oldgpar = orig.NoteGlobalPar;

    // These should never be null
    *gpar.FreqEnvelope = *oldgpar.FreqEnvelope;
    *gpar.FreqLfo = *oldgpar.FreqLfo;
    *gpar.AmpEnvelope = *oldgpar.AmpEnvelope;
    *gpar.AmpLfo = *oldgpar.AmpLfo;
    *gpar.FilterEnvelope = *oldgpar.FilterEnvelope;
    *gpar.FilterLfo = *oldgpar.FilterLfo;

    gpar.Fadein_adjustment = oldgpar.Fadein_adjustment;
    gpar.Punch = oldgpar.Punch;

    paramSeed = orig.paramSeed;

    globalnewamplitude = orig.globalnewamplitude;
    globaloldamplitude = orig.globaloldamplitude;

    // Supporting virtual copy assignment would be hairy
    // so we have to use the copy constructor here
    delete gpar.GlobalFilterL;
    gpar.GlobalFilterL = new Filter(*oldgpar.GlobalFilterL);
    if (stereo)
    {
        delete gpar.GlobalFilterR;
        gpar.GlobalFilterR = new Filter(*oldgpar.GlobalFilterR);
    }

    memcpy(pinking, orig.pinking, sizeof(pinking));
    memcpy(firsttick, orig.firsttick, sizeof(firsttick));

    memcpy(oldamplitude, orig.oldamplitude, sizeof(oldamplitude));
    memcpy(newamplitude, orig.newamplitude, sizeof(newamplitude));
    memcpy(FMoldamplitude, orig.FMoldamplitude, sizeof(FMoldamplitude));
    memcpy(FMnewamplitude, orig.FMnewamplitude, sizeof(FMnewamplitude));

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        auto &vpar = NoteVoicePar[i];
        auto &oldvpar = orig.NoteVoicePar[i];

        vpar.Enabled = oldvpar.Enabled;
        if (!vpar.Enabled)
            continue;

        vpar.DelayTicks = oldvpar.DelayTicks;
        vpar.Punch = oldvpar.Punch;
        vpar.phase_offset = oldvpar.phase_offset;

        int unison = adpars->VoicePar[i].Unison_size;
        memcpy(oscposhi[i], orig.oscposhi[i], unison * sizeof(int));
        memcpy(oscposlo[i], orig.oscposlo[i], unison * sizeof(float));
        memcpy(oscposhiFM[i], orig.oscposhiFM[i], unison * sizeof(int));
        memcpy(oscposloFM[i], orig.oscposloFM[i], unison * sizeof(float));

        copyOrAssign(vpar.FreqLfo, oldvpar.FreqLfo);
        copyOrAssign(vpar.FreqEnvelope, oldvpar.FreqEnvelope);

        copyOrAssign(vpar.AmpLfo, oldvpar.AmpLfo);
        copyOrAssign(vpar.AmpEnvelope, oldvpar.AmpEnvelope);

        delete vpar.VoiceFilterL;
        vpar.VoiceFilterL = NULL;
        if (oldvpar.VoiceFilterL != NULL)
            vpar.VoiceFilterL = new Filter(*oldvpar.VoiceFilterL);
        delete vpar.VoiceFilterR;
        vpar.VoiceFilterR = NULL;
        if (oldvpar.VoiceFilterR != NULL)
            vpar.VoiceFilterR = new Filter(*oldvpar.VoiceFilterR);

        copyOrAssign(vpar.FilterLfo, oldvpar.FilterLfo);
        copyOrAssign(vpar.FilterEnvelope, oldvpar.FilterEnvelope);

        copyOrAssign(vpar.FMFreqEnvelope, oldvpar.FMFreqEnvelope);
        copyOrAssign(vpar.FMAmpEnvelope, oldvpar.FMAmpEnvelope);

        if (subVoice[i] != NULL)
            for (int k = 0; k < unison_size[i]; ++k)
                subVoice[i][k]->legatoFadeOut(*orig.subVoice[i][k]);
        if (subFMVoice[i] != NULL)
            for (int k = 0; k < unison_size[i]; ++k)
                subFMVoice[i][k]->legatoFadeOut(*orig.subFMVoice[i][k]);
    }

    legatoFade = 1.0f; // Start at full volume
    legatoFadeStep = -synth->fadeStepShort; // Negative steps
}


// Kill a voice of ADnote
void ADnote::killVoice(int nvoice)
{
    delete [] oscfreqhi[nvoice];
    delete [] oscfreqlo[nvoice];
    delete [] oscfreqhiFM[nvoice];
    delete [] oscfreqloFM[nvoice];
    delete [] oscposhi[nvoice];
    delete [] oscposlo[nvoice];
    delete [] oscposhiFM[nvoice];
    delete [] oscposloFM[nvoice];

    delete [] unison_base_freq_rap[nvoice];
    delete [] unison_freq_rap[nvoice];
    delete [] unison_invert_phase[nvoice];
    delete [] FMoldsmp[nvoice];
    delete [] unison_vibratto[nvoice].step;
    delete [] unison_vibratto[nvoice].position;

    if (subVoice[nvoice] != NULL) {
        for (int k = 0; k < unison_size[nvoice]; ++k)
            delete subVoice[nvoice][k];
        delete [] subVoice[nvoice];
    }
    subVoice[nvoice] = NULL;

    if (subFMVoice[nvoice] != NULL) {
        for (int k = 0; k < unison_size[nvoice]; ++k)
            delete subFMVoice[nvoice][k];
        delete [] subFMVoice[nvoice];
    }
    subFMVoice[nvoice] = NULL;

    if (NoteVoicePar[nvoice].FreqEnvelope != NULL)
        delete NoteVoicePar[nvoice].FreqEnvelope;
    NoteVoicePar[nvoice].FreqEnvelope = NULL;

    if (NoteVoicePar[nvoice].FreqLfo != NULL)
        delete NoteVoicePar[nvoice].FreqLfo;
    NoteVoicePar[nvoice].FreqLfo = NULL;

    if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
        delete NoteVoicePar[nvoice].AmpEnvelope;
    NoteVoicePar[nvoice].AmpEnvelope = NULL;

    if (NoteVoicePar[nvoice].AmpLfo != NULL)
        delete NoteVoicePar[nvoice].AmpLfo;
    NoteVoicePar[nvoice].AmpLfo = NULL;

    if (NoteVoicePar[nvoice].VoiceFilterL != NULL)
        delete NoteVoicePar[nvoice].VoiceFilterL;
    NoteVoicePar[nvoice].VoiceFilterL = NULL;

    if (NoteVoicePar[nvoice].VoiceFilterR != NULL)
        delete NoteVoicePar[nvoice].VoiceFilterR;
    NoteVoicePar[nvoice].VoiceFilterR = NULL;

    if (NoteVoicePar[nvoice].FilterEnvelope != NULL)
        delete NoteVoicePar[nvoice].FilterEnvelope;
    NoteVoicePar[nvoice].FilterEnvelope = NULL;

    if (NoteVoicePar[nvoice].FilterLfo != NULL)
        delete NoteVoicePar[nvoice].FilterLfo;
    NoteVoicePar[nvoice].FilterLfo = NULL;

    if (NoteVoicePar[nvoice].FMFreqEnvelope != NULL)
        delete NoteVoicePar[nvoice].FMFreqEnvelope;
    NoteVoicePar[nvoice].FMFreqEnvelope = NULL;

    if (NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
        delete NoteVoicePar[nvoice].FMAmpEnvelope;
    NoteVoicePar[nvoice].FMAmpEnvelope = NULL;

    if (NoteVoicePar[nvoice].VoiceOut)
        memset(NoteVoicePar[nvoice].VoiceOut, 0, synth->bufferbytes);
        // do not delete, yet: perhaps is used by another voice

    if (parentFMmod != NULL && NoteVoicePar[nvoice].FMEnabled == FREQ_MOD) {
        delete [] FMFMoldPhase[nvoice];
        delete [] FMFMoldInterpPhase[nvoice];
        delete [] FMFMoldPMod[nvoice];
    }
    if (parentFMmod != NULL && forFM) {
        delete [] oscFMoldPhase[nvoice];
        delete [] oscFMoldInterpPhase[nvoice];
        delete [] oscFMoldPMod[nvoice];
    }

    NoteVoicePar[nvoice].Enabled = false;
}


// Kill the note
void ADnote::killNote()
{
    int nvoice;
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (NoteVoicePar[nvoice].Enabled)
            killVoice(nvoice);

        // Parent deletes oscillator samples.
        if (subVoiceNumber == -1)
            delete [] NoteVoicePar[nvoice].OscilSmp;
        if ((NoteVoicePar[nvoice].FMEnabled != NONE)
            && (NoteVoicePar[nvoice].FMVoice < 0)
            && (subVoiceNumber == -1))
            fftwf_free(NoteVoicePar[nvoice].FMSmp);
    }


    delete NoteGlobalPar.FreqEnvelope;
    delete NoteGlobalPar.FreqLfo;
    delete NoteGlobalPar.AmpEnvelope;
    delete NoteGlobalPar.AmpLfo;
    delete NoteGlobalPar.GlobalFilterL;
    if (stereo)
        delete NoteGlobalPar.GlobalFilterR;
    delete NoteGlobalPar.FilterEnvelope;
    delete NoteGlobalPar.FilterLfo;

    NoteStatus = NOTE_DISABLED;
}


ADnote::~ADnote()
{
    if (NoteStatus != NOTE_DISABLED)
        killNote();

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (NoteVoicePar[nvoice].VoiceOut)
        {
            fftwf_free(NoteVoicePar[nvoice].VoiceOut);
            NoteVoicePar[nvoice].VoiceOut = NULL;
        }
    }
    for (int k = 0; k < max_unison; ++k) {
        fftwf_free(tmpwave_unison[k]);
        fftwf_free(tmpmod_unison[k]);
    }
    delete [] tmpwave_unison;
    delete [] tmpmod_unison;
}


// Init the parameters
void ADnote::initParameters(void)
{
    int nvoice, i;

    // Global Parameters
    NoteGlobalPar.FreqEnvelope = new Envelope(adpars->GlobalPar.FreqEnvelope, basefreq, synth);
    NoteGlobalPar.FreqLfo = new LFO(adpars->GlobalPar.FreqLfo, basefreq, synth);
    NoteGlobalPar.AmpEnvelope = new Envelope(adpars->GlobalPar.AmpEnvelope, basefreq, synth);
    NoteGlobalPar.AmpLfo = new LFO(adpars->GlobalPar.AmpLfo, basefreq, synth);

    NoteGlobalPar.AmpEnvelope->envout_dB(); // discard the first envelope output
    NoteGlobalPar.GlobalFilterL = new Filter(adpars->GlobalPar.GlobalFilter, synth);
    if (stereo)
        NoteGlobalPar.GlobalFilterR = new Filter(adpars->GlobalPar.GlobalFilter, synth);
    NoteGlobalPar.FilterEnvelope =
        new Envelope(adpars->GlobalPar.FilterEnvelope, basefreq, synth);
    NoteGlobalPar.FilterLfo = new LFO(adpars->GlobalPar.FilterLfo, basefreq, synth);

    // Forbids the Modulation Voice to be greater or equal than voice
    for (i = 0; i < NUM_VOICES; ++i)
        if (NoteVoicePar[i].FMVoice >= i)
            NoteVoicePar[i].FMVoice = -1;

    // Voice Parameter init
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;

        NoteVoicePar[nvoice].noisetype = adpars->VoicePar[nvoice].Type;

        setRandomPan(synth->numRandom(), NoteVoicePar[nvoice].randpanL, NoteVoicePar[nvoice].randpanR, synth->getRuntime().panLaw, adpars->VoicePar[nvoice].PPanning, adpars->VoicePar[nvoice].PWidth);

        newamplitude[nvoice] = 1.0f;
        if (adpars->VoicePar[nvoice].PAmpEnvelopeEnabled)
        {
            NoteVoicePar[nvoice].AmpEnvelope =
                new Envelope(adpars->VoicePar[nvoice].AmpEnvelope, basefreq, synth);
            NoteVoicePar[nvoice].AmpEnvelope->envout_dB(); // discard the first envelope sample
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();
        }

        if (adpars->VoicePar[nvoice].PAmpLfoEnabled)
        {
            NoteVoicePar[nvoice].AmpLfo =
                new LFO(adpars->VoicePar[nvoice].AmpLfo, basefreq, synth);
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();
        }

        // Voice Frequency Parameters Init
        if (adpars->VoicePar[nvoice].PFreqEnvelopeEnabled)
            NoteVoicePar[nvoice].FreqEnvelope =
                new Envelope(adpars->VoicePar[nvoice].FreqEnvelope, basefreq, synth);

        if (adpars->VoicePar[nvoice].PFreqLfoEnabled)
            NoteVoicePar[nvoice].FreqLfo =
                new LFO(adpars->VoicePar[nvoice].FreqLfo, basefreq, synth);

        // Voice Filter Parameters Init
        if (adpars->VoicePar[nvoice].PFilterEnabled)
        {
            NoteVoicePar[nvoice].VoiceFilterL =
                new Filter(adpars->VoicePar[nvoice].VoiceFilter, synth);
            NoteVoicePar[nvoice].VoiceFilterR =
                new Filter(adpars->VoicePar[nvoice].VoiceFilter, synth);
        }

        if (adpars->VoicePar[nvoice].PFilterEnvelopeEnabled)
            NoteVoicePar[nvoice].FilterEnvelope =
                new Envelope(adpars->VoicePar[nvoice].FilterEnvelope,
                             basefreq, synth);

        if (adpars->VoicePar[nvoice].PFilterLfoEnabled)
            NoteVoicePar[nvoice].FilterLfo =
                new LFO(adpars->VoicePar[nvoice].FilterLfo, basefreq, synth);

        // Voice Modulation Parameters Init
        if (NoteVoicePar[nvoice].FMEnabled != NONE
           && NoteVoicePar[nvoice].FMVoice < 0)
        {
            // Perform Anti-aliasing only on MORPH or RING MODULATION

            int vc = nvoice;
            if (adpars->VoicePar[nvoice].PextFMoscil != -1)
                vc = adpars->VoicePar[nvoice].PextFMoscil;

            if (subVoiceNumber == -1) {
                adpars->VoicePar[nvoice].FMSmp->newrandseed();
                NoteVoicePar[nvoice].FMSmp =
                    (float*)fftwf_malloc((synth->oscilsize + OSCIL_SMP_EXTRA_SAMPLES) * sizeof(float));

                if (!adpars->GlobalPar.Hrandgrouping)
                    adpars->VoicePar[vc].FMSmp->newrandseed();
            } else {
                // If subvoice use oscillator from original voice.
                NoteVoicePar[nvoice].FMSmp = topVoice->NoteVoicePar[nvoice].FMSmp;
            }

            for (int k = 0; k < unison_size[nvoice]; ++k)
                oscposhiFM[nvoice][k] =
                    (oscposhi[nvoice][k] + adpars->VoicePar[vc].FMSmp->
                     getPhase()) % synth->oscilsize;

            NoteVoicePar[nvoice].FMphase_offset = 0;
        }

        if (adpars->VoicePar[nvoice].PFMFreqEnvelopeEnabled != 0)
            NoteVoicePar[nvoice].FMFreqEnvelope =
                new Envelope(adpars->VoicePar[nvoice].FMFreqEnvelope,
                             basefreq, synth);

        if (adpars->VoicePar[nvoice].PFMAmpEnvelopeEnabled != 0)
            NoteVoicePar[nvoice].FMAmpEnvelope =
                new Envelope(adpars->VoicePar[nvoice].FMAmpEnvelope,
                             basefreq, synth);
    }

    computeNoteParameters();

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume
                                 * ctl->fmamp.relamp;

        if (NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
        {
            FMnewamplitude[nvoice] *=
                NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
        }
    }

    if (subVoiceNumber != -1)
    {
        NoteVoicePar[subVoiceNumber].VoiceOut = (float*)fftwf_malloc(synth->bufferbytes);
        memset(NoteVoicePar[subVoiceNumber].VoiceOut, 0, synth->bufferbytes);
    }
}

void ADnote::computeNoteParameters(void)
{
    paramRNG.init(paramSeed);

    NoteGlobalPar.Detune = getDetune(adpars->GlobalPar.PDetuneType,
                                     adpars->GlobalPar.PCoarseDetune,
                                     adpars->GlobalPar.PDetune);
    bandwidthDetuneMultiplier = adpars->getBandwidthDetuneMultiplier();

    NoteGlobalPar.Volume =
        4.0f * powf(0.1f, 3.0f * (1.0f - adpars->GlobalPar.PVolume / 96.0f))  //-60 dB .. 0 dB
        * velF(velocity, adpars->GlobalPar.PAmpVelocityScaleFunction); // velocity sensing

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;

        if (subVoiceNumber == -1) {
            int BendAdj = adpars->VoicePar[nvoice].PBendAdjust - 64;
            if (BendAdj % 24 == 0)
                NoteVoicePar[nvoice].BendAdjust = BendAdj / 24;
            else
                NoteVoicePar[nvoice].BendAdjust = BendAdj / 24.0f;
        } else {
            // No bend adjustments for sub voices. Take from parent via
            // detuneFromParent.
            NoteVoicePar[nvoice].BendAdjust = 0.0f;
        }

        float offset_val = (adpars->VoicePar[nvoice].POffsetHz - 64)/64.0f;
        NoteVoicePar[nvoice].OffsetHz =
            15.0f*(offset_val * sqrtf(fabsf(offset_val)));

        NoteVoicePar[nvoice].fixedfreq = adpars->VoicePar[nvoice].Pfixedfreq;
        NoteVoicePar[nvoice].fixedfreqET = adpars->VoicePar[nvoice].PfixedfreqET;

        // use the Globalpars.detunetype if the detunetype is 0
        if (adpars->VoicePar[nvoice].PDetuneType)
        {
            NoteVoicePar[nvoice].Detune =
                getDetune(adpars->VoicePar[nvoice].PDetuneType,
                          adpars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getDetune(adpars->VoicePar[nvoice].PDetuneType, 0,
                          adpars->VoicePar[nvoice].PDetune); // fine detune
        }
        else
        {
            NoteVoicePar[nvoice].Detune =
                getDetune(adpars->GlobalPar.PDetuneType,
                          adpars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getDetune(adpars->GlobalPar.PDetuneType, 0,
                          adpars->VoicePar[nvoice].PDetune); // fine detune
        }
        if (adpars->VoicePar[nvoice].PFMDetuneType != 0)
            NoteVoicePar[nvoice].FMDetune =
                getDetune(adpars->VoicePar[nvoice].PFMDetuneType,
                          adpars->VoicePar[nvoice].PFMCoarseDetune,
                          adpars->VoicePar[nvoice].PFMDetune);
        else
            NoteVoicePar[nvoice].FMDetune =
                getDetune(adpars->GlobalPar.PDetuneType, adpars->VoicePar[nvoice].
                          PFMCoarseDetune, adpars->VoicePar[nvoice].PFMDetune);

        NoteVoicePar[nvoice].filterbypass = adpars->VoicePar[nvoice].Pfilterbypass;

        NoteVoicePar[nvoice].FMDetuneFromBaseOsc =
            (adpars->VoicePar[nvoice].PFMDetuneFromBaseOsc != 0);
        NoteVoicePar[nvoice].FMFreqFixed  = adpars->VoicePar[nvoice].PFMFixedFreq;

        if (subVoice[nvoice] != NULL)
        {
            float basefreq = getVoiceBaseFreq(nvoice);
            if (basefreq != subVoice[nvoice][0]->basefreq)
                for (int k = 0; k < unison_size[nvoice]; ++k)
                    subVoice[nvoice][k]->basefreq = basefreq;
        }
        if (subFMVoice[nvoice] != NULL)
        {
            float basefreq = getFMVoiceBaseFreq(nvoice);
            if (basefreq != subFMVoice[nvoice][0]->basefreq)
                for (int k = 0; k < unison_size[nvoice]; ++k)
                    subFMVoice[nvoice][k]->basefreq = basefreq;
        }

        // Compute the Voice's modulator volume (incl. damping)
        float fmvoldamp = powf(440.0f / getVoiceBaseFreq(nvoice),
                               adpars->VoicePar[nvoice].PFMVolumeDamp
                               / 64.0f - 1.0f);
        switch (NoteVoicePar[nvoice].FMEnabled)
        {
            case PHASE_MOD:
            case PW_MOD:
                fmvoldamp = powf(440.0f / getVoiceBaseFreq(nvoice),
                                 adpars->VoicePar[nvoice].PFMVolumeDamp / 64.0f);
                NoteVoicePar[nvoice].FMVolume =
                    (expf(adpars->VoicePar[nvoice].PFMVolume / 127.0f
                          * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;

            case FREQ_MOD:
                NoteVoicePar[nvoice].FMVolume =
                    (expf(adpars->VoicePar[nvoice].PFMVolume / 127.0f
                          * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;

            default:
                if (fmvoldamp > 1.0f)
                    fmvoldamp = 1.0f;
                NoteVoicePar[nvoice].FMVolume =
                    adpars->VoicePar[nvoice].PFMVolume / 127.0f * fmvoldamp;
        }

        // Voice's modulator velocity sensing
        NoteVoicePar[nvoice].FMVolume *=
            velF(velocity, adpars->VoicePar[nvoice].PFMVelocityScaleFunction);

        // Voice Amplitude Parameters Init
        if (adpars->VoicePar[nvoice].PVolume == 0)
            NoteVoicePar[nvoice].Volume = 0.0f;
        else
            NoteVoicePar[nvoice].Volume =
                powf(0.1f, 3.0f * (1.0f - adpars->VoicePar[nvoice].PVolume / 127.0f)) // -60 dB .. 0 dB
                * velF(velocity, adpars->VoicePar[nvoice].PAmpVelocityScaleFunction); // velocity

        if (adpars->VoicePar[nvoice].PVolumeminus)
            NoteVoicePar[nvoice].Volume = -NoteVoicePar[nvoice].Volume;

        int unison = unison_size[nvoice];

        if (subVoiceNumber == -1) {
            int vc = nvoice;
            if (adpars->VoicePar[nvoice].Pextoscil != -1)
                vc = adpars->VoicePar[nvoice].Pextoscil;
            adpars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                               getVoiceBaseFreq(nvoice),
                                               adpars->VoicePar[nvoice].Presonance);

            // I store the first elements to the last position for speedups
            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].OscilSmp[synth->oscilsize + i] = NoteVoicePar[nvoice].OscilSmp[i];

        }

        int new_phase_offset = (int)((adpars->VoicePar[nvoice].Poscilphase - 64.0f)
                                    / 128.0f * synth->oscilsize + synth->oscilsize * 4);
        int phase_offset_diff = new_phase_offset - NoteVoicePar[nvoice].phase_offset;
        for (int k = 0; k < unison; ++k) {
            oscposhi[nvoice][k] = (oscposhi[nvoice][k] + phase_offset_diff) % synth->oscilsize;
            if (oscposhi[nvoice][k] < 0)
                // This is necessary, because C '%' operator does not always
                // return a positive result.
                oscposhi[nvoice][k] += synth->oscilsize;
        }
        NoteVoicePar[nvoice].phase_offset = new_phase_offset;

        if (NoteVoicePar[nvoice].FMEnabled != NONE
            && NoteVoicePar[nvoice].FMVoice < 0)
        {
            if (subVoiceNumber == -1) {
                int vc = nvoice;
                if (adpars->VoicePar[nvoice].PextFMoscil != -1)
                    vc = adpars->VoicePar[nvoice].PextFMoscil;

                float freqtmp = 1.0f;
                if (adpars->VoicePar[vc].POscilFM->Padaptiveharmonics != 0
                    || (NoteVoicePar[nvoice].FMEnabled == MORPH)
                    || (NoteVoicePar[nvoice].FMEnabled == RING_MOD))
                    freqtmp = getFMVoiceBaseFreq(nvoice);

                adpars->VoicePar[vc].FMSmp->
                         get(NoteVoicePar[nvoice].FMSmp, freqtmp);

                for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                    NoteVoicePar[nvoice].FMSmp[synth->oscilsize + i] =
                        NoteVoicePar[nvoice].FMSmp[i];
            }

            int new_FMphase_offset = (int)((adpars->VoicePar[nvoice].PFMoscilphase - 64.0f)
                                         / 128.0f * synth->oscilsize_f
                                         + synth->oscilsize_f * 4.0f);
            int FMphase_offset_diff = new_FMphase_offset - NoteVoicePar[nvoice].FMphase_offset;
            for (int k = 0; k < unison_size[nvoice]; ++k)
            {
                oscposhiFM[nvoice][k] += FMphase_offset_diff;
                oscposhiFM[nvoice][k] %= synth->oscilsize;
                if (oscposhiFM[nvoice][k] < 0)
                    // This is necessary, because C '%' operator does not always
                    // return a positive result.
                    oscposhiFM[nvoice][k] += synth->oscilsize;
            }
            NoteVoicePar[nvoice].FMphase_offset = new_FMphase_offset;
        }

        bool is_pwm = NoteVoicePar[nvoice].FMEnabled == PW_MOD;

        unison_stereo_spread[nvoice] =
            adpars->VoicePar[nvoice].Unison_stereo_spread / 127.0f;
        float unison_spread = adpars->getUnisonFrequencySpreadCents(nvoice);
        float unison_real_spread = powf(2.0f, (unison_spread * 0.5f) / 1200.0f);
        float unison_vibratto_a = adpars->VoicePar[nvoice].Unison_vibratto / 127.0f;                                  //0.0 .. 1.0

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
                            powf(2.0f, (unison_spread * unison_values[k]) / 1200.0f);
                    }
                }
        }
        if (is_pwm)
            for (int i = true_unison - 1; i >= 0; i--)
            {
                unison_base_freq_rap[nvoice][2*i + 1] =
                    unison_base_freq_rap[nvoice][i];
                unison_base_freq_rap[nvoice][2*i] =
                    unison_base_freq_rap[nvoice][i];
            }

        // unison vibrattos
        if (true_unison > 1)
        {
            for (int k = 0; k < unison; ++k) // reduce the frequency difference
                                             // for larger vibrattos
                unison_base_freq_rap[nvoice][k] =
                    1.0f + (unison_base_freq_rap[nvoice][k] - 1.0f)
                    * (1.0f - unison_vibratto_a);

            unison_vibratto[nvoice].amplitude = (unison_real_spread - 1.0f) * unison_vibratto_a;

            float increments_per_second = 1 / synth->fixed_sample_step_f;
            const float vib_speed = adpars->VoicePar[nvoice].Unison_vibratto_speed / 127.0f;
            float vibratto_base_period  = 0.25f * powf(2.0f, (1.0f - vib_speed) * 4.0f);
            for (int k = 0; k < unison; ++k)
            {
                // make period to vary randomly from 50% to 200% vibratto base period
                float vibratto_period = vibratto_base_period * powf(2.0f, paramRNG.numRandom() * 2.0f - 1.0f);
                float m = 4.0f / (vibratto_period * increments_per_second);
                if (unison_vibratto[nvoice].step[k] < 0.0f)
                    m = -m;
                unison_vibratto[nvoice].step[k] = m;

                if (is_pwm)
                {
                    // Set the next position the same as this one.
                    unison_vibratto[nvoice].step[k+1] =
                        unison_vibratto[nvoice].step[k];
                    ++k; // Skip an iteration.
                }
            }
        }
        else // No vibrato for a single voice
        {
            unison_vibratto[nvoice].step[0] = 0.0f;
            unison_vibratto[nvoice].amplitude = 0.0f;

            if (is_pwm)
            {
                unison_vibratto[nvoice].step[1]     = 0.0f;
            }
        }

        // phase invert for unison
        unison_invert_phase[nvoice][0] = false;
        if (unison != 1)
        {
            int inv = adpars->VoicePar[nvoice].Unison_invert_phase;
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
    float detune = NoteVoicePar[nvoice].FMDetune / 100.0f;
    float freq;

    if (NoteVoicePar[nvoice].FMFreqFixed)
        return 440.0f * powf(2.0f, detune / 12.0f);

    if (NoteVoicePar[nvoice].FMDetuneFromBaseOsc)
        freq = getVoiceBaseFreq(nvoice);
    else {
        freq = basefreq;
        // To avoid applying global detuning twice: Only detune in main voice
        if (subVoiceNumber == -1)
            detune += NoteGlobalPar.Detune / 100.0f;
    }

    return freq * powf(2.0f, detune / 12.0f);
}


// Computes the relative frequency of each unison voice and it's vibratto
// This must be called before setfreq* functions
void ADnote::computeUnisonFreqRap(int nvoice)
{
    if (unison_size[nvoice] == 1) // no unison
    {
        unison_freq_rap[nvoice][0] = 1.0f;
        return;
    }
    float relbw = ctl->bandwidth.relbw * bandwidthDetuneMultiplier;
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float pos  = unison_vibratto[nvoice].position[k];
        float step = unison_vibratto[nvoice].step[k];
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
        float vibratto_val =
            (pos - 0.333333333f * pos * pos * pos) * 1.5f; // make the vibratto lfo smoother
        unison_freq_rap[nvoice][k] =
            1.0f + ((unison_base_freq_rap[nvoice][k] - 1.0f)
            + vibratto_val * unison_vibratto[nvoice].amplitude) * relbw;

        unison_vibratto[nvoice].position[k] = pos;
        step = unison_vibratto[nvoice].step[k] = step;
    }
}


// Computes the frequency of an oscillator
void ADnote::setfreq(int nvoice, float in_freq, float pitchdetune)
{
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float detunefactor = unison_freq_rap[nvoice][k] * unisonDetuneFactorFromParent;
        float freq  = fabsf(in_freq) * detunefactor;
        if (subVoice[nvoice] != NULL) {
            subVoice[nvoice][k]->setPitchDetuneFromParent(pitchdetune);
            subVoice[nvoice][k]->setUnisonDetuneFromParent(detunefactor);
        }
        float speed = freq * synth->oscil_sample_step_f;
        if (isgreater(speed, synth->oscilsize_f))
            speed = synth->oscilsize_f;
        int tmp = int(speed);
        oscfreqhi[nvoice][k] = tmp;
        oscfreqlo[nvoice][k] = speed - float(tmp);
    }
}


// Computes the frequency of an modulator oscillator
void ADnote::setfreqFM(int nvoice, float in_freq, float pitchdetune)
{
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float detunefactor = unisonDetuneFactorFromParent;
        if (NoteVoicePar[nvoice].FMDetuneFromBaseOsc)
            detunefactor *= unison_freq_rap[nvoice][k];
        float freq = fabsf(in_freq) * detunefactor;
        if (subFMVoice[nvoice] != NULL) {
            subFMVoice[nvoice][k]->setPitchDetuneFromParent(pitchdetune);
            subFMVoice[nvoice][k]->setUnisonDetuneFromParent(detunefactor);
        }
        float speed = freq * synth->oscil_sample_step_f;
        if (isgreater(speed, synth->oscilsize_f))
            speed = synth->oscilsize_f;
        int tmp = int(speed);
        oscfreqhiFM[nvoice][k] = tmp;
        oscfreqloFM[nvoice][k] = speed - float(tmp);
    }
}


// Get Voice base frequency
float ADnote::getVoiceBaseFreq(int nvoice)
{
    float detune =
        NoteVoicePar[nvoice].Detune / 100.0f + NoteVoicePar[nvoice].FineDetune /
        100.0f * ctl->bandwidth.relbw * bandwidthDetuneMultiplier;

    // To avoid applying global detuning twice: Only detune in main voice
    if (subVoiceNumber == -1)
        detune += NoteGlobalPar.Detune / 100.0f;

    if (!NoteVoicePar[nvoice].fixedfreq)
        return basefreq * powf(2.0f, detune / 12.0f);
    else // fixed freq is enabled
    {
        float fixedfreq;
        if (subVoiceNumber != -1)
            // Fixed frequency is not used in sub voices. We get the basefreq
            // from the parent.
            fixedfreq = basefreq;
        else
            fixedfreq = 440.0f;
        int fixedfreqET = NoteVoicePar[nvoice].fixedfreqET;
        if (fixedfreqET)
        {   // if the frequency varies according the keyboard note
            float tmp = (midinote - 69.0f) / 12.0f * (powf(2.0f, (fixedfreqET - 1) / 63.0f) - 1.0f);
            if (fixedfreqET <= 64)
                fixedfreq *= powf(2.0f, tmp);
            else
                fixedfreq *= powf(3.0f, tmp);
        }
        return fixedfreq * powf(2.0f, detune / 12.0f);
    }
}


// Computes all the parameters for each tick
void ADnote::computeWorkingParameters(void)
{
    float filterCenterPitch =
        adpars->GlobalPar.GlobalFilter->getfreq() // center freq
        + adpars->GlobalPar.PFilterVelocityScale / 127.0f * 6.0f
        * (velF(velocity, adpars->GlobalPar.PFilterVelocityScaleFunction) - 1);

    float filterQ = adpars->GlobalPar.GlobalFilter->getq();
    float filterFreqTracking =
        adpars->GlobalPar.GlobalFilter->getfreqtracking(basefreq);

    float filterpitch, filterfreq;
    float globalpitch = 0.01f * (NoteGlobalPar.FreqEnvelope->envout()
                       + NoteGlobalPar.FreqLfo->lfoout() * ctl->modwheel.relmod);
    globaloldamplitude = globalnewamplitude;
    globalnewamplitude = NoteGlobalPar.Volume
                         * NoteGlobalPar.AmpEnvelope->envout_dB()
                         * NoteGlobalPar.AmpLfo->amplfoout();
    float globalfilterpitch = NoteGlobalPar.FilterEnvelope->envout()
                              + NoteGlobalPar.FilterLfo->lfoout()
                              + filterCenterPitch;

    float tmpfilterfreq = globalfilterpitch + ctl->filtercutoff.relfreq
          + filterFreqTracking;

    tmpfilterfreq = NoteGlobalPar.GlobalFilterL->getrealfreq(tmpfilterfreq);
    float globalfilterq = filterQ * ctl->filterq.relq;
    NoteGlobalPar.GlobalFilterL->setfreq_and_q(tmpfilterfreq, globalfilterq);
    if (stereo)
        NoteGlobalPar.GlobalFilterR->setfreq_and_q(tmpfilterfreq, globalfilterq);

    // compute the portamento, if it is used by this note
    float portamentofreqrap = 1.0f;
    if (portamento) // this voice use portamento
    {
        portamentofreqrap = ctl->portamento.freqrap;
        if (!ctl->portamento.used) // the portamento has finished
            portamento = 0;        // this note is no longer "portamented"

    }

    // compute parameters for all voices
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;
        NoteVoicePar[nvoice].DelayTicks -= 1;
        if (NoteVoicePar[nvoice].DelayTicks > 0)
            continue;

        computeUnisonFreqRap(nvoice);

        // Voice Amplitude
        oldamplitude[nvoice] = newamplitude[nvoice];
        newamplitude[nvoice] = 1.0f;

        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();

        if (NoteVoicePar[nvoice].AmpLfo != NULL)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();

        // Voice Filter
        if (NoteVoicePar[nvoice].VoiceFilterL != NULL)
        {
            filterpitch =
                adpars->VoicePar[nvoice].VoiceFilter->getfreq()
                + adpars->VoicePar[nvoice].PFilterVelocityScale
                / 127.0f * 6.0f       //velocity sensing
                * (velF(velocity,
                        adpars->VoicePar[nvoice].PFilterVelocityScaleFunction) - 1);
            filterQ = adpars->VoicePar[nvoice].VoiceFilter->getq();
            if (NoteVoicePar[nvoice].FilterEnvelope != NULL)
                filterpitch += NoteVoicePar[nvoice].FilterEnvelope->envout();
            if (NoteVoicePar[nvoice].FilterLfo != NULL)
                filterpitch += NoteVoicePar[nvoice].FilterLfo->lfoout();
            filterfreq = filterpitch + adpars->VoicePar[nvoice].VoiceFilter->getfreqtracking(basefreq);
            filterfreq = NoteVoicePar[nvoice].VoiceFilterL->getrealfreq(filterfreq);
            NoteVoicePar[nvoice].VoiceFilterL->setfreq_and_q(filterfreq, filterQ);
            if (stereo && NoteVoicePar[nvoice].VoiceFilterR)
                NoteVoicePar[nvoice].VoiceFilterR->setfreq_and_q(filterfreq, filterQ);

        }
        if (!NoteVoicePar[nvoice].noisetype) // voice is not noise
        {
            // Voice Frequency
            float basevoicepitch = 0.0f;
            basevoicepitch += detuneFromParent;

            basevoicepitch += 12.0f * NoteVoicePar[nvoice].BendAdjust *
                log2f(ctl->pitchwheel.relfreq); //change the frequency by the controller

            float voicepitch = basevoicepitch;
            if (NoteVoicePar[nvoice].FreqLfo != NULL)
            {
                voicepitch += NoteVoicePar[nvoice].FreqLfo->lfoout() / 100.0f
                              * ctl->bandwidth.relbw;
            }

            if (NoteVoicePar[nvoice].FreqEnvelope != NULL)
            {
                voicepitch += NoteVoicePar[nvoice].FreqEnvelope->envout() / 100.0f;
            }

            float nonoffsetfreq = getVoiceBaseFreq(nvoice)
                              * powf(2.0f, (voicepitch + globalpitch) / 12.0f);
            nonoffsetfreq *= portamentofreqrap;
            float voicefreq = nonoffsetfreq + NoteVoicePar[nvoice].OffsetHz;
            voicepitch += log2f(voicefreq / nonoffsetfreq) * 12.0f;
            setfreq(nvoice, voicefreq, voicepitch);

            // Modulator
            if (NoteVoicePar[nvoice].FMEnabled != NONE)
            {
                float FMpitch;
                if (NoteVoicePar[nvoice].FMFreqFixed)
                    FMpitch = 0.0f;
                else if (NoteVoicePar[nvoice].FMDetuneFromBaseOsc)
                    FMpitch = voicepitch;
                else
                    FMpitch = basevoicepitch;

                float FMrelativepitch = 0.0f;
                if (NoteVoicePar[nvoice].FMFreqEnvelope != NULL) {
                    FMrelativepitch +=
                        NoteVoicePar[nvoice].FMFreqEnvelope->envout() / 100.0f;
                    FMpitch += FMrelativepitch;
                    // Do not add any more adjustments to FMpitch after
                    // this. The rest of FMrelativepitch has already been
                    // accounted for in our sub voices when we created them,
                    // using getFMVoiceBaseFreq().
                }

                float FMfreq;
                if (NoteVoicePar[nvoice].FMFreqFixed) {
                    // Apply FM detuning since base frequency is 440Hz.
                    FMrelativepitch += NoteVoicePar[nvoice].FMDetune / 100.0f;
                    FMfreq = powf(2.0f, FMrelativepitch / 12.0f) * 440.0f;
                } else if (NoteVoicePar[nvoice].FMDetuneFromBaseOsc) {
                    // Apply FM detuning since base frequency is from main voice.
                    FMrelativepitch += NoteVoicePar[nvoice].FMDetune / 100.0f;
                    FMfreq = powf(2.0f, FMrelativepitch / 12.0f) * voicefreq;
                } else {
                    // No need to apply FM detuning, since getFMVoiceBaseFreq()
                    // takes it into account.
                    FMfreq = getFMVoiceBaseFreq(nvoice) *
                        powf(2.0f, (basevoicepitch + globalpitch + FMrelativepitch) / 12.0f);
                    FMfreq *= portamentofreqrap;
                }
                setfreqFM(nvoice, FMfreq, FMpitch);
                FMoldamplitude[nvoice] = FMnewamplitude[nvoice];
                FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;
                if (NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
                    FMnewamplitude[nvoice] *= NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
            }
        }
    }
    time += synth->sent_buffersize_f / synth->samplerate_f;
}


// Fadein in a way that removes clicks but keep sound "punchy"
void ADnote::fadein(float *smps)
{
    int zerocrossings = 0;
    for (int i = 1; i < synth->sent_buffersize; ++i)
        if (smps[i - 1] < 0.0f && smps[i] > 0.0f)
            zerocrossings++; // this is only the positive crossings

    float tmp = (synth->sent_buffersize - 1.0f) / (zerocrossings + 1) / 3.0f;
    if (tmp < 8.0f)
        tmp = 8.0f;
    tmp *= NoteGlobalPar.Fadein_adjustment;

    int fadein = int(tmp); // how many samples is the fade-in
    if (fadein < 8)
        fadein = 8;
    if (fadein > synth->sent_buffersize)
        fadein = synth->sent_buffersize;
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
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        int    poshi  = oscposhi[nvoice][k];
        int    poslo  = oscposlo[nvoice][k] * (1<<24);
        int    freqhi = oscfreqhi[nvoice][k];
        int    freqlo = oscfreqlo[nvoice][k] * (1<<24);
        float *smps   = NoteVoicePar[nvoice].OscilSmp;
        float *tw     = tmpwave_unison[k];
        assert(oscfreqlo[nvoice][k] < 1.0f);
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            tw[i]  = (smps[poshi] * ((1<<24) - poslo) + smps[poshi + 1] * poslo)/(1.0f*(1<<24));
            poslo += freqlo;
            poshi += freqhi + (poslo>>24);
            poslo &= 0xffffff;
            poshi &= synth->oscilsize - 1;
        }
        oscposhi[nvoice][k] = poshi;
        oscposlo[nvoice][k] = poslo/(1.0f*(1<<24));
    }
}

// end of port

// Applies the Oscillator (Morphing)
void ADnote::applyVoiceOscillatorMorph(int nvoice)
{
    if (isgreater(FMnewamplitude[nvoice], 1.0f))
        FMnewamplitude[nvoice] = 1.0f;
    if (isgreater(FMoldamplitude[nvoice], 1.0f))
        FMoldamplitude[nvoice] = 1.0f;

    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpwave_unison[k];
        float *mod = tmpmod_unison[k];

        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float amp = interpolateAmplitude(FMoldamplitude[nvoice],
                                       FMnewamplitude[nvoice], i,
                                       synth->sent_buffersize);
            tw[i] = (tw[i] * (1.0f - amp)) + amp * mod[i];
        }
    }
}


// Applies the Oscillator (Ring Modulation)
void ADnote::applyVoiceOscillatorRingModulation(int nvoice)
{
    float amp;
    bool isSide = NoteVoicePar[nvoice].FMringToSide;
    if (isgreater(FMnewamplitude[nvoice], 1.0f))
        FMnewamplitude[nvoice] = 1.0f;
    if (isgreater(FMoldamplitude[nvoice], 1.0f))
        FMoldamplitude[nvoice] = 1.0f;
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpwave_unison[k];
        float *mod = tmpmod_unison[k];

        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            amp = interpolateAmplitude(FMoldamplitude[nvoice],
                                       FMnewamplitude[nvoice], i,
                                       synth->sent_buffersize);
            if (isSide) // sidebands
                tw[i] *= (mod[i] * amp * 2);
                //tw[i] *= (mod[i] + 1.0f) * 0.5f * amp + (1.0f - amp);
            else // ring
                tw[i] *= mod[i] * amp + (1.0f - amp);
        }
    }
}


// Computes the Modulator
void ADnote::computeVoiceModulator(int nvoice, int FMmode)
{
    if (subFMVoice[nvoice] != NULL) {
        int subVoiceNumber = NoteVoicePar[nvoice].FMVoice;
        for (int k = 0; k < unison_size[nvoice]; ++k) {
            // Sub voices use VoiceOut, so just pass NULL.
            subFMVoice[nvoice][k]->noteout(NULL, NULL);
            const float *smps = subFMVoice[nvoice][k]->NoteVoicePar[subVoiceNumber].VoiceOut;
            // For historical/compatibility reasons we do not reduce volume here
            // if are using stereo. See same section in computeVoiceOscillator.
            memcpy(tmpmod_unison[k], smps, synth->bufferbytes);
        }
    }
    else if (parentFMmod != NULL) {
        if (NoteVoicePar[nvoice].FMEnabled == FREQ_MOD) {
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
    if (aboveAmplitudeThreshold(FMoldamplitude[nvoice], FMnewamplitude[nvoice]))
    {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpmod_unison[k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
                tw[i] *= interpolateAmplitude(FMoldamplitude[nvoice],
                                              FMnewamplitude[nvoice], i,
                                              synth->sent_buffersize);
        }
    }
    else
    {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpmod_unison[k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
                tw[i] *= FMnewamplitude[nvoice];
        }
    }
}

// Normalize the modulator for phase/frequency modulation
void ADnote::normalizeVoiceModulatorFrequencyModulation(int nvoice, int FMmode)
{
    if (FMmode == PW_MOD) { // PWM modulation
        for (int k = 1; k < unison_size[nvoice]; k += 2) {
            float *tw = tmpmod_unison[k];
            for (int i = 1; i < synth->sent_buffersize; ++i)
                tw[i] = -tw[i];
        }
    }

    // normalize: makes all sample-rates, oscil_sizes to produce same sound
    if (FMmode == FREQ_MOD) // Frequency modulation
    {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpmod_unison[k];
            float  fmold = FMoldsmp[nvoice][k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
            {
                fmold = fmold + tw[i] * synth->oscil_norm_factor_fm;
                tw[i] = fmold;
            }
            FMoldsmp[nvoice][k] = fmold;
        }
    }
    else  // Phase or PWM modulation
    {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpmod_unison[k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
                tw[i] *= synth->oscil_norm_factor_pm;
        }
    }

    if (parentFMmod != NULL) {
        // This is a sub voice. Mix our frequency modulation with the
        // parent modulation.
        float *tmp = parentFMmod;
        for (int k = 0; k < unison_size[nvoice]; ++k) {
            float *tw = tmpmod_unison[k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
                tw[i] += tmp[i];
        }
    }
}

// Render the modulator with linear interpolation, no modulation on it
void ADnote::computeVoiceModulatorLinearInterpolation(int nvoice)
{
    // Compute the modulator and store it in tmpmod_unison[][]
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        int poshiFM = oscposhiFM[nvoice][k];
        float posloFM  = oscposloFM[nvoice][k];
        int freqhiFM = oscfreqhiFM[nvoice][k];
        float freqloFM = oscfreqloFM[nvoice][k];
        float *tw = tmpmod_unison[k];
        const float *smps = NoteVoicePar[nvoice].FMSmp;

        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            tw[i] = (smps[poshiFM] * (1 - posloFM)
                     + smps[poshiFM + 1] * posloFM) / (1.0f);

            posloFM += freqloFM;
            if (posloFM >= 1.0f)
            {
                posloFM -= 1.0f;
                poshiFM++;
            }
            poshiFM += freqhiFM;
            poshiFM &= synth->oscilsize - 1;
        }
        oscposhiFM[nvoice][k] = poshiFM;
        oscposloFM[nvoice][k] = posloFM;
    }
}

// Computes the Modulator (Phase Modulation or Frequency Modulation from parent voice)
void ADnote::computeVoiceModulatorFrequencyModulation(int nvoice, int FMmode)
{
    // do the modulation using parent's modulator, onto a new modulator
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpmod_unison[k];
        int poshiFM = oscposhiFM[nvoice][k];
        float posloFM = oscposloFM[nvoice][k];
        int freqhiFM = oscfreqhiFM[nvoice][k];
        float freqloFM = oscfreqloFM[nvoice][k];
        // When we have parent modulation, we want to maintain the same
        // sound. However, if the carrier and modulator are very far apart in
        // frequency, then the modulation will affect them very differently,
        // since the phase difference is linear, not logarithmic. Compensate for
        // this by favoring the carrier, and adjust the rate of modulation
        // logarithmically, relative to this.
        float oscVsFMratio = ((float)freqhiFM + freqloFM)
            / ((float)oscfreqhi[nvoice][k] + oscfreqlo[nvoice][k]);
        const float *smps = NoteVoicePar[nvoice].FMSmp;

        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float pMod = parentFMmod[i] * oscVsFMratio;
            int FMmodfreqhi = int(pMod);
            float FMmodfreqlo = pMod-FMmodfreqhi;
            if (FMmodfreqhi < 0)
                FMmodfreqlo++;

            // carrier, which will become the new modulator
            int carposhi = poshiFM + FMmodfreqhi;
            float carposlo = posloFM + FMmodfreqlo;

            if (FMmode == PW_MOD && (k & 1))
                carposhi += NoteVoicePar[nvoice].phase_offset;

            if (carposlo >= 1.0f)
            {
                carposhi++;
                carposlo -= 1.0f;
            }
            carposhi &= (synth->oscilsize - 1);

            tw[i] = smps[carposhi] * (1.0f - carposlo)
                    + smps[carposhi + 1] * carposlo;
            posloFM += freqloFM;
            if (posloFM >= 1.0f)
            {
                posloFM -= 1.0f;
                poshiFM++;
            }

            poshiFM += freqhiFM;
            poshiFM &= synth->oscilsize - 1;
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
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpmod_unison[k];
        int poshiFM = oscposhiFM[nvoice][k];
        float posloFM = oscposloFM[nvoice][k];
        int freqhiFM = oscfreqhiFM[nvoice][k];
        float freqloFM = oscfreqloFM[nvoice][k];
        float freqFM = (float)freqhiFM + freqloFM;
        float oscVsFMratio = freqFM
            / ((float)oscfreqhi[nvoice][k] + oscfreqlo[nvoice][k]);
        const float *smps = NoteVoicePar[nvoice].FMSmp;
        float oldInterpPhase = FMFMoldInterpPhase[nvoice][k];
        float currentPhase = FMFMoldPhase[nvoice][k];
        float currentPMod = FMFMoldPMod[nvoice][k];

        for (int i = 0; i < synth->sent_buffersize; ++i)
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
                poshiFM &= synth->oscilsize - 1;

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
                poshiFM &= synth->oscilsize - 1;
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
            poshiFM &= synth->oscilsize - 1;

            float nextAmount = (pMod - currentPMod) / freqFM;
            float currentAmount= 1.0f - nextAmount;
            float interpPhase = currentPhase * currentAmount
                + nextPhase * nextAmount;
            tw[i] = interpPhase - oldInterpPhase;
            oldInterpPhase = interpPhase;

            currentPhase = nextPhase;
        }
        oscposhiFM[nvoice][k] = poshiFM;
        oscposloFM[nvoice][k] = posloFM;
        FMFMoldPhase[nvoice][k] = currentPhase;
        FMFMoldPMod[nvoice][k] = currentPMod;
        FMFMoldInterpPhase[nvoice][k] = oldInterpPhase;
    }
}

// Computes the Oscillator (Phase Modulation or Frequency Modulation)
void ADnote::computeVoiceOscillatorFrequencyModulation(int nvoice)
{
    // do the modulation
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpwave_unison[k];
        int poshi = oscposhi[nvoice][k];
        float poslo = oscposlo[nvoice][k];
        int freqhi = oscfreqhi[nvoice][k];
        float freqlo = oscfreqlo[nvoice][k];
        // If this ADnote has frequency based modulation, the modulator resides
        // in tmpmod_unison, otherwise it comes from the parent. If there is no
        // modulation at all this function should not be called.
        const float *mod = freqbasedmod[nvoice] ? tmpmod_unison[k] : parentFMmod;

        for (int i = 0; i < synth->sent_buffersize; ++i)
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
            carposhi &= (synth->oscilsize - 1);

            tw[i] = NoteVoicePar[nvoice].OscilSmp[carposhi] * (1.0f - carposlo)
                    + NoteVoicePar[nvoice].OscilSmp[carposhi + 1] * carposlo;
            poslo += freqlo;
            if (poslo >= 1.0f)
            {
                poslo -= 1.0f;
                poshi++;
            }

            poshi += freqhi;
            poshi &= synth->oscilsize - 1;
        }
        oscposhi[nvoice][k] = poshi;
        oscposlo[nvoice][k] = poslo;
    }
}

void ADnote::computeVoiceOscillatorForFMFrequencyModulation(int nvoice)
{
    // See computeVoiceModulatorForFMFrequencyModulation for details on how this
    // works.
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpwave_unison[k];
        int poshi = oscposhi[nvoice][k];
        float poslo = oscposlo[nvoice][k];
        int freqhi = oscfreqhi[nvoice][k];
        float freqlo = oscfreqlo[nvoice][k];
        float freq = (float)freqhi + freqlo;
        const float *smps = NoteVoicePar[nvoice].OscilSmp;
        float oldInterpPhase = oscFMoldInterpPhase[nvoice][k];
        float currentPhase = oscFMoldPhase[nvoice][k];
        float currentPMod = oscFMoldPMod[nvoice][k];

        for (int i = 0; i < synth->sent_buffersize; ++i)
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
                poshi &= synth->oscilsize - 1;

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
                poshi &= synth->oscilsize - 1;
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
            poshi &= synth->oscilsize - 1;

            float nextAmount = (pMod - currentPMod) / freq;
            float currentAmount= 1.0f - nextAmount;
            float interpPhase = currentPhase * currentAmount
                + nextPhase * nextAmount;
            tw[i] = interpPhase - oldInterpPhase;
            oldInterpPhase = interpPhase;

            currentPhase = nextPhase;
        }
        oscposhi[nvoice][k] = poshi;
        oscposlo[nvoice][k] = poslo;
        oscFMoldPhase[nvoice][k] = currentPhase;
        oscFMoldPMod[nvoice][k] = currentPMod;
        oscFMoldInterpPhase[nvoice][k] = oldInterpPhase;
    }
}



// Computes the Noise
void ADnote::computeVoiceNoise(int nvoice)
{
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpwave_unison[k];
        for (int i = 0; i < synth->sent_buffersize; ++i)
            tw[i] = synth->numRandom() * 2.0f - 1.0f;
    }
}


// ported from Zyn 2.5.2
void ADnote::ComputeVoicePinkNoise(int nvoice)
{
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpwave_unison[k];
        float *f = &pinking[nvoice][k > 0 ? 7 : 0];
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float white = (synth->numRandom() - 0.5 ) / 4.0;
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
    if (subVoice[nvoice] != NULL) {
        int subVoiceNumber = NoteVoicePar[nvoice].Voice;
        for (int k = 0; k < unison_size[nvoice]; ++k) {
            // Sub voices use VoiceOut, so just pass NULL.
            subVoice[nvoice][k]->noteout(NULL, NULL);
            const float *smps = subVoice[nvoice][k]->NoteVoicePar[subVoiceNumber].VoiceOut;
            float *tw = tmpwave_unison[k];
            if (stereo) {
                // Reduce volume due to stereo being combined to mono.
                for (int i = 0; i < synth->buffersize; ++i) {
                    tw[i] = smps[i] * 0.5f;
                }
            } else {
                memcpy(tw, smps, synth->bufferbytes);
            }
        }
    } else {
        switch (NoteVoicePar[nvoice].noisetype)
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
        }
    }

    // Apply non-frequency modulation onto rendered voice.
    switch(NoteVoicePar[nvoice].FMEnabled)
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
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpwave_unison[k];
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            if (Tspot <= 0)
            {
                tw[i] = synth->numRandom() * 6.0f - 3.0f;
                Tspot = (synth->randomINT() >> 24);
            }
            else
            {
                tw[i] = 0.0f;
                Tspot--;
            }
        }
    }
}


// Compute the ADnote samples, returns 0 if the note is finished
int ADnote::noteout(float *outl, float *outr)
{
    Config &Runtime = synth->getRuntime();
    float *tmpwavel = Runtime.genTmp1;
    float *tmpwaver = Runtime.genTmp2;
    float *bypassl = Runtime.genTmp3;
    float *bypassr = Runtime.genTmp4;
    int i, nvoice;
    if (outl != NULL) {
        memset(outl, 0, synth->sent_bufferbytes);
        memset(outr, 0, synth->sent_bufferbytes);
    }

    if (NoteStatus == NOTE_DISABLED)
        return 0;

    if (subVoiceNumber == -1) {
        memset(bypassl, 0, synth->sent_bufferbytes);
        memset(bypassr, 0, synth->sent_bufferbytes);
    }

    if (paramsUpdate.checkUpdated())
        computeNoteParameters();

    computeWorkingParameters();

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled || NoteVoicePar[nvoice].DelayTicks > 0)
            continue;

        if (NoteVoicePar[nvoice].FMEnabled != NONE)
            computeVoiceModulator(nvoice, NoteVoicePar[nvoice].FMEnabled);

        computeVoiceOscillator(nvoice);

        // Mix subvoices into voice
        memset(tmpwavel, 0, synth->sent_bufferbytes);
        if (stereo)
            memset(tmpwaver, 0, synth->sent_bufferbytes);
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            if (stereo)
            {
                float stereo_pos = 0.0f;
                bool is_pwm = NoteVoicePar[nvoice].FMEnabled == PW_MOD;
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

                for (i = 0; i < synth->sent_buffersize; ++i)
                    tmpwavel[i] += tw[i] * lvol;
                for (i = 0; i < synth->sent_buffersize; ++i)
                    tmpwaver[i] += tw[i] * rvol;
            }
            else
                for (i = 0; i < synth->sent_buffersize; ++i)
                    tmpwavel[i] += tw[i];
        }

        // reduce the amplitude for large unison sizes
        float unison_amplitude = 1.0f / sqrtf(unison_size[nvoice]);

        // Amplitude
        float oldam = oldamplitude[nvoice] * unison_amplitude;
        float newam = newamplitude[nvoice] * unison_amplitude;

        if (aboveAmplitudeThreshold(oldam, newam))
        {
            int rest = synth->sent_buffersize;
            // test if the amplitude if rising and the difference is high
            if (newam > oldam && (newam - oldam) > 0.25f)
            {
                rest = 10;
                if (rest > synth->sent_buffersize)
                    rest = synth->sent_buffersize;
                for (int i = 0; i < synth->sent_buffersize - rest; ++i)
                    tmpwavel[i] *= oldam;
                if (stereo)
                    for (int i = 0; i < synth->sent_buffersize - rest; ++i)
                        tmpwaver[i] *= oldam;
            }
            // Amplitude interpolation
            for (i = 0; i < rest; ++i)
            {
                float amp = interpolateAmplitude(oldam, newam, i, rest);
                tmpwavel[i + (synth->sent_buffersize - rest)] *= amp;
                if (stereo)
                    tmpwaver[i + (synth->sent_buffersize - rest)] *= amp;
            }
        }
        else
        {
            for (i = 0; i < synth->sent_buffersize; ++i)
                tmpwavel[i] *= newam;
            if (stereo)
                for (i = 0; i < synth->sent_buffersize; ++i)
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
        if (NoteVoicePar[nvoice].VoiceFilterL != NULL)
            NoteVoicePar[nvoice].VoiceFilterL->filterout(tmpwavel);
        if (stereo && NoteVoicePar[nvoice].VoiceFilterR != NULL)
            NoteVoicePar[nvoice].VoiceFilterR->filterout(tmpwaver);

        // check if the amplitude envelope is finished.
        // if yes, the voice will fadeout
        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
        {
            if (NoteVoicePar[nvoice].AmpEnvelope->finished())
            {
                for (i = 0; i < synth->sent_buffersize; ++i)
                    tmpwavel[i] *= 1.0f - (float)i / synth->sent_buffersize_f;
                if (stereo)
                    for (i = 0; i < synth->sent_buffersize; ++i)
                        tmpwaver[i] *= 1.0f - (float)i / synth->sent_buffersize_f;
            }
            // the voice is killed later
        }

        // Put the ADnote samples in VoiceOut (without applying Global volume,
        // because I wish to use this voice as a modulator)
        if (NoteVoicePar[nvoice].VoiceOut)
        {
            if (stereo)
                for (i = 0; i < synth->sent_buffersize; ++i)
                    NoteVoicePar[nvoice].VoiceOut[i] = tmpwavel[i] + tmpwaver[i];
            else // mono
                for (i = 0; i < synth->sent_buffersize; ++i)
                    NoteVoicePar[nvoice].VoiceOut[i] = tmpwavel[i];
            if (NoteVoicePar[nvoice].Volume == 0.0f)
                // If we are muted, we are done.
                continue;
        }

        pangainL = adpars->VoicePar[nvoice].pangainL; // assume voice not random pan
        pangainR = adpars->VoicePar[nvoice].pangainR;
        if (adpars->VoicePar[nvoice].PRandom)
        {
            pangainL = NoteVoicePar[nvoice].randpanL;
            pangainR = NoteVoicePar[nvoice].randpanR;
        }

        if (outl != NULL) {
            // Add the voice that do not bypass the filter to out.
            if (!NoteVoicePar[nvoice].filterbypass) // no bypass
            {
                if (stereo)
                {

                    for (i = 0; i < synth->sent_buffersize; ++i) // stereo
                    {
                        outl[i] += tmpwavel[i] * NoteVoicePar[nvoice].Volume * pangainL;
                        outr[i] += tmpwaver[i] * NoteVoicePar[nvoice].Volume * pangainR;
                    }
                }
                else
                    for (i = 0; i < synth->sent_buffersize; ++i)
                        outl[i] += tmpwavel[i] * NoteVoicePar[nvoice].Volume * 0.7f; // mono
            }
            else // bypass the filter
            {
                if (stereo)
                {
                    for (i = 0; i < synth->sent_buffersize; ++i) // stereo
                    {
                        bypassl[i] += tmpwavel[i] * NoteVoicePar[nvoice].Volume
                                      * pangainL;
                        bypassr[i] += tmpwaver[i] * NoteVoicePar[nvoice].Volume
                                      * pangainR;
                    }
                }
                else
                    for (i = 0; i < synth->sent_buffersize; ++i)
                        bypassl[i] += tmpwavel[i] * NoteVoicePar[nvoice].Volume; // mono
            }
            // check if there is necessary to process the voice longer
            // (if the Amplitude envelope isn't finished)
            if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
                if (NoteVoicePar[nvoice].AmpEnvelope->finished())
                    killVoice(nvoice);
        }
    }

    if (outl != NULL) {
        // Processing Global parameters
        NoteGlobalPar.GlobalFilterL->filterout(outl);

        if (!stereo) // set the right channel=left channel
        {
            memcpy(outr, outl, synth->sent_bufferbytes);
            memcpy(bypassr, bypassl, synth->sent_bufferbytes);
        }
        else
            NoteGlobalPar.GlobalFilterR->filterout(outr);

        for (i = 0; i < synth->sent_buffersize; ++i)
        {
            outl[i] += bypassl[i];
            outr[i] += bypassr[i];
        }

        pangainL = adpars->GlobalPar.pangainL; // assume it's not random panning ...
        pangainR = adpars->GlobalPar.pangainR;
        if (adpars->GlobalPar.PRandom)         // it is random panning
        {
            pangainL = NoteGlobalPar.randpanL;
            pangainR = NoteGlobalPar.randpanR;
        }

        if (aboveAmplitudeThreshold(globaloldamplitude, globalnewamplitude))
        {
            // Amplitude Interpolation
            for (i = 0; i < synth->sent_buffersize; ++i)
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
            for (i = 0; i < synth->sent_buffersize; ++i)
            {
                outl[i] *= globalnewamplitude * pangainL;
                outr[i] *= globalnewamplitude * pangainR;
            }
        }

        // Apply the punch
        if (NoteGlobalPar.Punch.Enabled)
        {
            for (i = 0; i < synth->sent_buffersize; ++i)
            {
                float punchamp = NoteGlobalPar.Punch.initialvalue
                                 * NoteGlobalPar.Punch.t + 1.0f;
                outl[i] *= punchamp;
                outr[i] *= punchamp;
                NoteGlobalPar.Punch.t -= NoteGlobalPar.Punch.dt;
                if (NoteGlobalPar.Punch.t < 0.0f)
                {
                    NoteGlobalPar.Punch.Enabled = 0;
                    break;
                }
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
    }

    // Check if the global amplitude is finished.
    // If it does, disable the note
    if (NoteGlobalPar.AmpEnvelope->finished())
    {
        if (outl != NULL) {
            for (i = 0; i < synth->sent_buffersize; ++i) // fade-out
            {
                float tmp = 1.0f - (float)i / synth->sent_buffersize_f;
                outl[i] *= tmp;
                outr[i] *= tmp;
            }
        }
        killNote();
    }
    return 1;
}


// Release the key (NoteOff)
void ADnote::releasekey(void)
{
    int nvoice;
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;
        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
            NoteVoicePar[nvoice].AmpEnvelope->releasekey();
        if (NoteVoicePar[nvoice].FreqEnvelope != NULL)
            NoteVoicePar[nvoice].FreqEnvelope->releasekey();
        if (NoteVoicePar[nvoice].FilterEnvelope != NULL)
            NoteVoicePar[nvoice].FilterEnvelope->releasekey();
        if (NoteVoicePar[nvoice].FMFreqEnvelope != NULL)
            NoteVoicePar[nvoice].FMFreqEnvelope->releasekey();
        if (NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
            NoteVoicePar[nvoice].FMAmpEnvelope->releasekey();
        if (subVoice[nvoice] != NULL)
            for (int k = 0; k < unison_size[nvoice]; ++k)
                subVoice[nvoice][k]->releasekey();
        if (subFMVoice[nvoice] != NULL)
            for (int k = 0; k < unison_size[nvoice]; ++k)
                subFMVoice[nvoice][k]->releasekey();
    }
    NoteGlobalPar.FreqEnvelope->releasekey();
    NoteGlobalPar.FilterEnvelope->releasekey();
    NoteGlobalPar.AmpEnvelope->releasekey();
    if (NoteStatus == NOTE_KEEPALIVE)
        NoteStatus = NOTE_ENABLED;
}

// for future reference ... re replacing pow(x, y) by exp(y * log(x))
