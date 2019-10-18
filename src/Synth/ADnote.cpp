/*
    ADnote.cpp - The "additive" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others

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
    Modified May 2019
*/

#include <cmath>
#include <fftw3.h>
#include <cassert>

#include "Synth/Envelope.h"
#include "Synth/ADnote.h"
#include "Synth/LFO.h"
#include "DSP/Filter.h"
#include "Params/ADnoteParameters.h"
#include "Params/Controller.h"
#include "Misc/SynthEngine.h"
#include "Misc/SynthHelper.h"

#include "globals.h"

using synth::velF;
using synth::getDetune;
using synth::interpolateAmplitude;
using synth::aboveAmplitudeThreshold;

using std::isgreater;


ADnote::ADnote(ADnoteParameters *adpars_, Controller *ctl_, float freq_,
               float velocity_, int portamento_, int midinote_, bool besilent, SynthEngine *_synth) :
    ready(0),
    adpars(adpars_),
    stereo(adpars->GlobalPar.PStereo),
    midinote(midinote_),
    velocity(velocity_),
    basefreq(freq_),
    NoteEnabled(true),
    ctl(ctl_),
    time(0.0f),
    forFM(false),
    portamento(portamento_),
    subVoiceNumber(-1),
    origVoice(NULL),
    parentFMmod(NULL),
    synth(_synth)
{
    Legato.silent = besilent;
    construct();
}

ADnote::ADnote(ADnote *orig, float freq_, int subVoiceNumber_, float *parentFMmod_,
               bool forFM_) :
    ready(0),
    adpars(orig->adpars),
    stereo(adpars->GlobalPar.PStereo),
    midinote(orig->midinote),
    velocity(orig->velocity),
    basefreq(freq_),
    NoteEnabled(true),
    ctl(orig->ctl),
    time(0.0f),
    forFM(forFM_),
    portamento(orig->portamento),
    subVoiceNumber(subVoiceNumber_),
    origVoice(orig),
    parentFMmod(parentFMmod_),
    synth(orig->synth)
{
    Legato.silent = orig->Legato.silent;
    construct();
}

void ADnote::construct()
{
    if (velocity > 1.0f)
        velocity = 1.0f;

    // Initialise some legato-specific vars
    Legato.msg = LM_Norm;
    Legato.fade.length = int(synth->samplerate_f * 0.005f); // 0.005 seems ok.
    if (Legato.fade.length < 1)  // (if something's fishy)
        Legato.fade.length = 1;
    Legato.fade.step = (1.0f / Legato.fade.length);
    Legato.decounter = -10;
    Legato.param.freq = basefreq;
    Legato.param.vel = velocity;
    Legato.param.portamento = portamento;
    Legato.param.midinote = midinote;

    NoteGlobalPar.Detune = getDetune(adpars->GlobalPar.PDetuneType,
                                     adpars->GlobalPar.PCoarseDetune,
                                     adpars->GlobalPar.PDetune);
    bandwidthDetuneMultiplier = adpars->getBandwidthDetuneMultiplier();

    if (adpars->randomGlobalPan())
    {
        float t = synth->numRandom();
        NoteGlobalPar.randpanL = cosf(t * HALFPI);
        NoteGlobalPar.randpanR = cosf((1.0f - t) * HALFPI);
    }
    else
        NoteGlobalPar.randpanL = NoteGlobalPar.randpanR = 0.7f;
    NoteGlobalPar.FilterCenterPitch =
        adpars->GlobalPar.GlobalFilter->getfreq() // center freq
        + adpars->GlobalPar.PFilterVelocityScale / 127.0f * 6.0f
        * (velF(velocity, adpars->GlobalPar.PFilterVelocityScaleFunction) - 1);

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

        unison_stereo_spread[nvoice] =
            adpars->VoicePar[nvoice].Unison_stereo_spread / 127.0f;
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
        float unison_spread = adpars->getUnisonFrequencySpreadCents(nvoice);
        float unison_real_spread = powf(2.0f, (unison_spread * 0.5f) / 1200.0f);
        float unison_vibratto_a = adpars->VoicePar[nvoice].Unison_vibratto / 127.0f;                                  //0.0 .. 1.0

        int true_unison = unison / (is_pwm ? 2 : 1);
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
                        float val  = step + (synth->numRandom() * 2.0f - 1.0f) / (true_unison - 1);
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
        if(unison > 2 || (!is_pwm && unison > 1))
        {
            for (int k = 0; k < unison; ++k) // reduce the frequency difference
                                             // for larger vibrattos
                unison_base_freq_rap[nvoice][k] =
                    1.0f + (unison_base_freq_rap[nvoice][k] - 1.0f)
                    * (1.0f - unison_vibratto_a);
        }
        unison_vibratto[nvoice].step = new float[unison];
        unison_vibratto[nvoice].position = new float[unison];
        unison_vibratto[nvoice].amplitude = (unison_real_spread - 1.0f) * unison_vibratto_a;

        float increments_per_second = synth->samplerate_f / synth->sent_all_buffersize_f;
        const float vib_speed = adpars->VoicePar[nvoice].Unison_vibratto_speed / 127.0f;
        float vibratto_base_period  = 0.25f * powf(2.0f, (1.0f - vib_speed) * 4.0f);
        for (int k = 0; k < unison; ++k)
        {
            unison_vibratto[nvoice].position[k] = synth->numRandom() * 1.8f - 0.9f;
            // make period to vary randomly from 50% to 200% vibratto base period
            float vibratto_period = vibratto_base_period * powf(2.0f, synth->numRandom() * 2.0f - 1.0f);
            float m = 4.0f / (vibratto_period * increments_per_second);
            if (synth->numRandom() < 0.5f)
                m = -m;
            unison_vibratto[nvoice].step[k] = m;

            // Ugly, but the alternative is likely uglier.
            if (is_pwm)
                for (int i = 0; i < unison; i += 2)
                {
                    unison_vibratto[nvoice].step[i+1] =
                        unison_vibratto[nvoice].step[i];
                    unison_vibratto[nvoice].position[i+1] =
                        unison_vibratto[nvoice].position[i];
                }
        }

        if (unison <= 2) // no vibratto for a single voice
        {
            if (is_pwm)
            {
                unison_vibratto[nvoice].step[1]     = 0.0f;
                unison_vibratto[nvoice].position[1] = 0.0f;
            }
            if (is_pwm || unison == 1)
            {
                unison_vibratto[nvoice].step[0] = 0.0f;
                unison_vibratto[nvoice].position[0] = 0.0f;
                unison_vibratto[nvoice].amplitude = 0.0f;
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
                        unison_invert_phase[nvoice][k] = (synth->numRandom() > 0.5f);
                    break;

                default:
                    for (int k = 0; k < unison; ++k)
                        unison_invert_phase[nvoice][k] = (k % inv == 0) ? true : false;
                    break;
            }
        }

        oscfreqhi[nvoice] = new int[unison];
        oscfreqlo[nvoice] = new float[unison];
        oscfreqhiFM[nvoice] = new unsigned int[unison];
        oscfreqloFM[nvoice] = new float[unison];
        oscposhi[nvoice] = new int[unison];
        oscposlo[nvoice] = new float[unison];
        oscposhiFM[nvoice] = new unsigned int[unison];
        oscposloFM[nvoice] = new float[unison];

        NoteVoicePar[nvoice].Enabled = true;
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
            adpars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                               getVoiceBaseFreq(nvoice),
                                               adpars->VoicePar[nvoice].Presonance);

            // I store the first elements to the last position for speedups
            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].OscilSmp[synth->oscilsize + i] = NoteVoicePar[nvoice].OscilSmp[i];
        } else {
            // If subvoice, use oscillator from original voice.
            NoteVoicePar[nvoice].OscilSmp = origVoice->NoteVoicePar[nvoice].OscilSmp;
        }

        int oscposhi_start =
            adpars->VoicePar[vc].OscilSmp->getPhase();
        NoteVoicePar[nvoice].phase_offset = (int)((adpars->VoicePar[nvoice].Poscilphase - 64.0f)
                                    / 128.0f * synth->oscilsize + synth->oscilsize * 4);
        oscposhi_start += NoteVoicePar[nvoice].phase_offset;

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

        NoteVoicePar[nvoice].FilterCenterPitch =
            adpars->VoicePar[nvoice].VoiceFilter->getfreq()
            + adpars->VoicePar[nvoice].PFilterVelocityScale
            / 127.0f * 6.0f       //velocity sensing
            * (velF(velocity,
                    adpars->VoicePar[nvoice].PFilterVelocityScaleFunction) - 1);
        NoteVoicePar[nvoice].filterbypass = adpars->VoicePar[nvoice].Pfilterbypass;

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

        NoteVoicePar[nvoice].FMDetuneFromBaseOsc =
            (adpars->VoicePar[nvoice].PFMDetuneFromBaseOsc != 0);
        NoteVoicePar[nvoice].FMVoice = adpars->VoicePar[nvoice].PFMVoice;
        NoteVoicePar[nvoice].FMFreqEnvelope = NULL;
        NoteVoicePar[nvoice].FMAmpEnvelope = NULL;
        NoteVoicePar[nvoice].FMFreqFixed  = adpars->VoicePar[nvoice].PFMFixedFreq;

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

        FMoldsmp[nvoice] = new float [unison];
        memset(FMoldsmp[nvoice], 0, unison * sizeof(float));

        firsttick[nvoice] = 1;
        NoteVoicePar[nvoice].DelayTicks =
            (int)((expf(adpars->VoicePar[nvoice].PDelay / 127.0f
                         * logf(50.0f)) - 1.0f) / synth->sent_all_buffersize_f / 10.0f
                         * synth->samplerate_f);

        if (parentFMmod != NULL && NoteVoicePar[nvoice].FMEnabled == FREQ_MOD) {
            FMFMoldsmpModded[nvoice] = new float [unison];
            memset(FMFMoldsmpModded[nvoice], 0, unison * sizeof(*FMFMoldsmpModded[nvoice]));
            FMFMoldsmpOrig[nvoice] = new float [unison];
            memset(FMFMoldsmpOrig[nvoice], 0, unison * sizeof(*FMFMoldsmpOrig[nvoice]));
        }
        if (parentFMmod != NULL && forFM) {
            oscFMoldsmpModded[nvoice] = new float [unison];
            memset(oscFMoldsmpModded[nvoice], 0, unison * sizeof(*oscFMoldsmpModded[nvoice]));
            oscFMoldsmpOrig[nvoice] = new float [unison];
            memset(oscFMoldsmpOrig[nvoice], 0, unison * sizeof(*oscFMoldsmpOrig[nvoice]));
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

    ready = 1;
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
                subVoice[nvoice][k] = new ADnote((origVoice != NULL) ? origVoice : this,
                                                 getVoiceBaseFreq(nvoice),
                                                 NoteVoicePar[nvoice].Voice,
                                                 freqmod, forFM);
            }
        }

        if (NoteVoicePar[nvoice].FMVoice != -1)
        {
            bool voiceForFM = NoteVoicePar[nvoice].FMEnabled == FREQ_MOD;
            subFMVoice[nvoice] = new ADnote*[unison_size[nvoice]];
            for (int k = 0; k < unison_size[nvoice]; ++k) {
                subFMVoice[nvoice][k] = new ADnote((origVoice != NULL) ? origVoice : this,
                                                   getFMVoiceBaseFreq(nvoice),
                                                   NoteVoicePar[nvoice].FMVoice,
                                                   parentFMmod, voiceForFM);
            }
        }
    }
}


// ADlegatonote: This function is (mostly) a copy of ADnote(...) and
// initParameters() stuck together with some lines removed so that it
// only alter the already playing note (to perform legato). It is
// possible I left stuff that is not required for this.
void ADnote::ADlegatonote(float freq_, float velocity_, int portamento_,
                          int midinote_, bool externcall)
{
    basefreq = freq_;
    velocity = velocity_;
    if (velocity > 1.0)
        velocity = 1.0f;
    portamento = portamento_;
    midinote = midinote_;

    // Manage legato stuff
    if (externcall) {
        Legato.msg = LM_Norm;
        for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice) {
            if (NoteVoicePar[nvoice].Enabled) {
                if (subVoice[nvoice] != NULL)
                    for (int k = 0; k < unison_size[nvoice]; ++k) {
                        subVoice[nvoice][k]->ADlegatonote(freq_, velocity_, portamento_,
                                                        midinote_, externcall);
                    }
                if (subFMVoice[nvoice] != NULL)
                    for (int k = 0; k < unison_size[nvoice]; ++k) {
                        subFMVoice[nvoice][k]->ADlegatonote(freq_, velocity_, portamento_,
                                                          midinote_, externcall);
                    }
            }
        }
    }
    if (Legato.msg != LM_CatchUp)
    {
        Legato.lastfreq = Legato.param.freq;
        Legato.param.freq = freq_;
        Legato.param.vel = velocity_;
        Legato.param.portamento = portamento_;
        Legato.param.midinote = midinote_;
        if (Legato.msg == LM_Norm)
        {
            if (Legato.silent)
            {
                Legato.fade.m = 0.0f;
                Legato.msg = LM_FadeIn;
            }
            else
            {
                Legato.fade.m = 1.0f;
                Legato.msg = LM_FadeOut;
                return;
            }
        }
        if (Legato.msg == LM_ToNorm)
            Legato.msg = LM_Norm;
    }

    NoteGlobalPar.Detune = getDetune(adpars->GlobalPar.PDetuneType,
                                     adpars->GlobalPar.PCoarseDetune,
                                     adpars->GlobalPar.PDetune);
    bandwidthDetuneMultiplier = adpars->getBandwidthDetuneMultiplier();

    if (adpars->randomGlobalPan())
    {
        float t = synth->numRandom();
        NoteGlobalPar.randpanL = cosf(t * HALFPI);
        NoteGlobalPar.randpanR = cosf((1.0f - t) * HALFPI);
    }
    else
        NoteGlobalPar.randpanL = NoteGlobalPar.randpanR = 0.7f;

    NoteGlobalPar.FilterCenterPitch =
        adpars->GlobalPar.GlobalFilter->getfreq()
        + adpars->GlobalPar.PFilterVelocityScale / 127.0f * 6.0f
        * (velF(velocity, adpars->GlobalPar.PFilterVelocityScaleFunction) - 1);

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (NoteVoicePar[nvoice].Enabled == 0)
            continue; //(gf) Stay the same as first note in legato.

        NoteVoicePar[nvoice].fixedfreq = adpars->VoicePar[nvoice].Pfixedfreq;
        NoteVoicePar[nvoice].fixedfreqET = adpars->VoicePar[nvoice].PfixedfreqET;

        if (adpars->VoicePar[nvoice].PDetuneType)
        {
            NoteVoicePar[nvoice].Detune =
                getDetune(adpars->VoicePar[nvoice].PDetuneType,
                          adpars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getDetune(adpars->VoicePar[nvoice].PDetuneType, 0,
                          adpars->VoicePar[nvoice].PDetune); // fine detune
        }
        else // use the Globalpars.detunetype if the detunetype is 0
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
                getDetune(adpars->GlobalPar.PDetuneType,
                          adpars->VoicePar[nvoice].PFMCoarseDetune,
                          adpars->VoicePar[nvoice].PFMDetune);

        // Only generate oscillator for original voices. In sub voices we use
        // the parents' voices, so they are already generated.
        if (subVoiceNumber == -1) {
            // Get the voice's oscil or external's voice oscil
            int vc = nvoice;
            if (adpars->VoicePar[nvoice].Pextoscil != -1)
                vc = adpars->VoicePar[nvoice].Pextoscil;
            if (!adpars->GlobalPar.Hrandgrouping)
                adpars->VoicePar[vc].OscilSmp->newrandseed();

            adpars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                               getVoiceBaseFreq(nvoice),
                                               adpars->VoicePar[nvoice].Presonance);                                                       //(gf)Modif of the above line.

            // I store the first elements to the last position for speedups
            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].OscilSmp[synth->oscilsize + i] =
                    NoteVoicePar[nvoice].OscilSmp[i];
        }

        NoteVoicePar[nvoice].FilterCenterPitch =
            adpars->VoicePar[nvoice].VoiceFilter->getfreq()
            + adpars->VoicePar[nvoice].PFilterVelocityScale
            / 127.0f * 6.0f       //velocity sensing
            * (velF(velocity,
                    adpars->VoicePar[nvoice].PFilterVelocityScaleFunction) - 1);
        NoteVoicePar[nvoice].filterbypass =
            adpars->VoicePar[nvoice].Pfilterbypass;

        NoteVoicePar[nvoice].FMVoice = adpars->VoicePar[nvoice].PFMVoice;

        // Compute the Voice's modulator volume (incl. damping)
        float fmvoldamp =
            powf(440.0f / getVoiceBaseFreq(nvoice),
                 adpars->VoicePar[nvoice].PFMVolumeDamp / 64.0f - 1.0f);

        switch (NoteVoicePar[nvoice].FMEnabled)
        {
            case PHASE_MOD:
            case PW_MOD:
                fmvoldamp =
                    powf(440.0f / getVoiceBaseFreq(nvoice),
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

        NoteVoicePar[nvoice].DelayTicks =
            int((expf(adpars->VoicePar[nvoice].PDelay / 127.0f
                         * logf(50.0f)) - 1.0f) / synth->sent_all_buffersize_f / 10.0f
                         * synth->samplerate_f);
    }

    ///////////////
    // Altered content of initParameters():

    int nvoice, i;

    NoteGlobalPar.Volume =
        4.0f * powf(0.1f, 3.0f * (1.0f - adpars->GlobalPar.PVolume / 96.0f))  //-60 dB .. 0 dB
        * velF(velocity, adpars->GlobalPar.PAmpVelocityScaleFunction); // velocity sensing
    globalnewamplitude = NoteGlobalPar.Volume
                         * NoteGlobalPar.AmpEnvelope->envout_dB()
                         * NoteGlobalPar.AmpLfo->amplfoout();
    NoteGlobalPar.FilterQ = adpars->GlobalPar.GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking =
        adpars->GlobalPar.GlobalFilter->getfreqtracking(basefreq);

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
        // Voice Amplitude Parameters Init
        if (adpars->VoicePar[nvoice].PVolume == 0)
            NoteVoicePar[nvoice].Volume = 0.0f;
        else
            NoteVoicePar[nvoice].Volume =
                powf(0.1f, 3.0f * (1.0f - adpars->VoicePar[nvoice].PVolume / 127.0f)) // -60 dB .. 0 dB
                * velF(velocity, adpars->VoicePar[nvoice].PAmpVelocityScaleFunction); // velocity

        if (adpars->VoicePar[nvoice].PVolumeminus)
            NoteVoicePar[nvoice].Volume = -NoteVoicePar[nvoice].Volume;

        if (adpars->randomVoicePan(nvoice))
        {
            float t = synth->numRandom();
            NoteVoicePar[nvoice].randpanL = cosf(t * HALFPI);
            NoteVoicePar[nvoice].randpanR = cosf((1.0f - t) * HALFPI);
        }
        else
            NoteVoicePar[nvoice].randpanL = NoteVoicePar[nvoice].randpanR = 0.7f;

        newamplitude[nvoice] = 1.0f;
        if (adpars->VoicePar[nvoice].PAmpEnvelopeEnabled
           && NoteVoicePar[nvoice].AmpEnvelope)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();

        if (adpars->VoicePar[nvoice].PAmpLfoEnabled
             && NoteVoicePar[nvoice].AmpLfo)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();

        NoteVoicePar[nvoice].FilterFreqTracking =
            adpars->VoicePar[nvoice].VoiceFilter->getfreqtracking(basefreq);

        // Voice Modulation Parameters Init
        if (NoteVoicePar[nvoice].FMEnabled != NONE
            && NoteVoicePar[nvoice].FMVoice < 0)
        {
            // Only generate modulator oscillator for original voices. In sub
            // voices we use the parents' voices, so they are already generated.
            if (subVoiceNumber == -1) {
                adpars->VoicePar[nvoice].FMSmp->newrandseed();

                //Perform Anti-aliasing only on MORPH or RING MODULATION

                int vc = nvoice;
                if (adpars->VoicePar[nvoice].PextFMoscil != -1)
                    vc = adpars->VoicePar[nvoice].PextFMoscil;

                if (!adpars->GlobalPar.Hrandgrouping)
                    adpars->VoicePar[vc].FMSmp->newrandseed();

                for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                    NoteVoicePar[nvoice].FMSmp[synth->oscilsize + i] =
                        NoteVoicePar[nvoice].FMSmp[i];
            }
        }

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;

        if (adpars->VoicePar[nvoice].PFMAmpEnvelopeEnabled
           && NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
            FMnewamplitude[nvoice] *=
                NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
    }

    // End of the ADlegatonote function.
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
        delete [] FMFMoldsmpModded[nvoice];
        delete [] FMFMoldsmpOrig[nvoice];
    }
    if (parentFMmod != NULL && forFM) {
        delete [] oscFMoldsmpModded[nvoice];
        delete [] oscFMoldsmpOrig[nvoice];
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

    NoteEnabled = false;
}


ADnote::~ADnote()
{
    if (NoteEnabled)
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
    NoteGlobalPar.Volume =
        4.0f * powf(0.1f, 3.0f * (1.0f - adpars->GlobalPar.PVolume / 96.0f))  //-60 dB .. 0 dB
        * velF(velocity, adpars->GlobalPar.PAmpVelocityScaleFunction); // velocity sensing

    NoteGlobalPar.AmpEnvelope->envout_dB(); // discard the first envelope output
    globalnewamplitude = NoteGlobalPar.Volume
                         * NoteGlobalPar.AmpEnvelope->envout_dB()
                         * NoteGlobalPar.AmpLfo->amplfoout();
    NoteGlobalPar.GlobalFilterL = new Filter(adpars->GlobalPar.GlobalFilter, synth);
    if (stereo)
        NoteGlobalPar.GlobalFilterR = new Filter(adpars->GlobalPar.GlobalFilter, synth);
    NoteGlobalPar.FilterEnvelope =
        new Envelope(adpars->GlobalPar.FilterEnvelope, basefreq, synth);
    NoteGlobalPar.FilterLfo = new LFO(adpars->GlobalPar.FilterLfo, basefreq, synth);
    NoteGlobalPar.FilterQ = adpars->GlobalPar.GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking =
        adpars->GlobalPar.GlobalFilter->getfreqtracking(basefreq);

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

        // Voice Amplitude Parameters Init
        if (adpars->VoicePar[nvoice].PVolume == 0)
            NoteVoicePar[nvoice].Volume = 0.0f;
        else
            NoteVoicePar[nvoice].Volume =
                powf(0.1f, 3.0f * (1.0f - adpars->VoicePar[nvoice].PVolume / 127.0f)) // -60 dB .. 0 dB
                * velF(velocity, adpars->VoicePar[nvoice].PAmpVelocityScaleFunction); // velocity

        if (adpars->VoicePar[nvoice].PVolumeminus)
            NoteVoicePar[nvoice].Volume = -NoteVoicePar[nvoice].Volume;

        if (adpars->randomVoicePan(nvoice))
        {
            float t = synth->numRandom();
            NoteVoicePar[nvoice].randpanL = cosf(t * HALFPI);
            NoteVoicePar[nvoice].randpanR = cosf((1.0f - t) * HALFPI);
        }
        else
            NoteVoicePar[nvoice].randpanL = NoteVoicePar[nvoice].randpanR = 0.7f;

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

        NoteVoicePar[nvoice].FilterFreqTracking =
            adpars->VoicePar[nvoice].VoiceFilter->getfreqtracking(basefreq);

        // Voice Modulation Parameters Init
        if (NoteVoicePar[nvoice].FMEnabled != NONE
           && NoteVoicePar[nvoice].FMVoice < 0)
        {
            // Perform Anti-aliasing only on MORPH or RING MODULATION

            int vc = nvoice;
            if (adpars->VoicePar[nvoice].PextFMoscil != -1)
                vc = adpars->VoicePar[nvoice].PextFMoscil;

            float freqtmp = 1.0f;
            if (adpars->VoicePar[vc].POscilFM->Padaptiveharmonics != 0
               || (NoteVoicePar[nvoice].FMEnabled == MORPH)
               || (NoteVoicePar[nvoice].FMEnabled == RING_MOD))
               freqtmp = getFMVoiceBaseFreq(nvoice);

            if (subVoiceNumber == -1) {
                adpars->VoicePar[nvoice].FMSmp->newrandseed();
                NoteVoicePar[nvoice].FMSmp =
                    (float*)fftwf_malloc((synth->oscilsize + OSCIL_SMP_EXTRA_SAMPLES) * sizeof(float));

                if (!adpars->GlobalPar.Hrandgrouping)
                    adpars->VoicePar[vc].FMSmp->newrandseed();

                adpars->VoicePar[vc].FMSmp->
                         get(NoteVoicePar[nvoice].FMSmp, freqtmp);

                for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                    NoteVoicePar[nvoice].FMSmp[synth->oscilsize + i] =
                        NoteVoicePar[nvoice].FMSmp[i];
            } else {
                // If subvoice use oscillator from original voice.
                NoteVoicePar[nvoice].FMSmp = origVoice->NoteVoicePar[nvoice].FMSmp;
            }

            for (int k = 0; k < unison_size[nvoice]; ++k)
                oscposhiFM[nvoice][k] =
                    (oscposhi[nvoice][k] + adpars->VoicePar[vc].FMSmp->
                     getPhase()) % synth->oscilsize;

            int oscposhiFM_add = (int)((adpars->VoicePar[nvoice].PFMoscilphase - 64.0f)
                                         / 128.0f * synth->oscilsize_f
                                         + synth->oscilsize_f * 4.0f);
            for (int k = 0; k < unison_size[nvoice]; ++k)
            {
                oscposhiFM[nvoice][k] += oscposhiFM_add;
                oscposhiFM[nvoice][k] %= synth->oscilsize;
            }
        }

        if (adpars->VoicePar[nvoice].PFMFreqEnvelopeEnabled != 0)
            NoteVoicePar[nvoice].FMFreqEnvelope =
                new Envelope(adpars->VoicePar[nvoice].FMFreqEnvelope,
                             basefreq, synth);

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume
                                 * ctl->fmamp.relamp;

        if (adpars->VoicePar[nvoice].PFMAmpEnvelopeEnabled != 0)
        {
            NoteVoicePar[nvoice].FMAmpEnvelope =
                new Envelope(adpars->VoicePar[nvoice].FMAmpEnvelope,
                             basefreq, synth);
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
        float speed = freq * synth->oscilsize_f / synth->samplerate_f;
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
        float speed = freq * synth->oscilsize_f / synth->samplerate_f;
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

    // Fixed frequency is disabled for sub voices. We get the basefreq from the
    // parent.
    if (!NoteVoicePar[nvoice].fixedfreq || subVoiceNumber != -1)
        return basefreq * powf(2.0f, detune / 12.0f);
    else // fixed freq is enabled
    {
        float fixedfreq = 440.0f;
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
void ADnote::computeCurrentParameters(void)
{
    float filterpitch, filterfreq;
    float globalpitch = 0.01f * (NoteGlobalPar.FreqEnvelope->envout()
                       + NoteGlobalPar.FreqLfo->lfoout() * ctl->modwheel.relmod);
    globaloldamplitude = globalnewamplitude;
    globalnewamplitude = NoteGlobalPar.Volume
                         * NoteGlobalPar.AmpEnvelope->envout_dB()
                         * NoteGlobalPar.AmpLfo->amplfoout();
    float globalfilterpitch = NoteGlobalPar.FilterEnvelope->envout()
                              + NoteGlobalPar.FilterLfo->lfoout()
                              + NoteGlobalPar.FilterCenterPitch;

    float tmpfilterfreq = globalfilterpitch + ctl->filtercutoff.relfreq
          + NoteGlobalPar.FilterFreqTracking;

    tmpfilterfreq = NoteGlobalPar.GlobalFilterL->getrealfreq(tmpfilterfreq);
    float globalfilterq = NoteGlobalPar.FilterQ * ctl->filterq.relq;
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
            filterpitch = NoteVoicePar[nvoice].FilterCenterPitch;
            if (NoteVoicePar[nvoice].FilterEnvelope != NULL)
                filterpitch += NoteVoicePar[nvoice].FilterEnvelope->envout();
            if (NoteVoicePar[nvoice].FilterLfo != NULL)
                filterpitch += NoteVoicePar[nvoice].FilterLfo->lfoout();
            filterfreq = filterpitch + NoteVoicePar[nvoice].FilterFreqTracking;
            filterfreq = NoteVoicePar[nvoice].VoiceFilterL->getrealfreq(filterfreq);
            NoteVoicePar[nvoice].VoiceFilterL->setfreq(filterfreq);
            if (stereo && NoteVoicePar[nvoice].VoiceFilterR)
                NoteVoicePar[nvoice].VoiceFilterR->setfreq(filterfreq);

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
            tw[i] *= mod[i] * amp + (1.0 - amp);
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
    } else if (NoteVoicePar[nvoice].FMVoice >= 0) {
        // if I use VoiceOut[] as modulator
        for (int k = 0; k < unison_size[nvoice]; ++k) {
            const float *smps = NoteVoicePar[NoteVoicePar[nvoice].FMVoice].VoiceOut;
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

    if (freqbasedmod[nvoice])
        normalizeVoiceModulatorFrequencyModulation(nvoice, FMmode);
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
        float normalize = synth->oscilsize_f / 262144.0f * 44100.0f / synth->samplerate_f;
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpmod_unison[k];
            float  fmold = FMoldsmp[nvoice][k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
            {
                fmold = fmold + tw[i] * normalize;
                tw[i] = fmold;
            }
            FMoldsmp[nvoice][k] = fmold;
        }
    }
    else  // Phase or PWM modulation
    {
        float normalize = synth->oscilsize / 262144.0f;
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpmod_unison[k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
                tw[i] *= normalize;
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

            if(carposlo >= 1.0f)
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
    // in the same way as the others. Instead, we start with the original
    // unmodulated function, and then integrate either backwards or forwards
    // until we reach the phase offset from the parent modulation. Then we take
    // the linear interpolation between the two nearest samples, and use that to
    // construct the resulting curve. This is a lot more expensive than straight
    // modulation, since we have to count every sample between the phase
    // position we're at, and the one we're going to. However, it preserves the
    // original sound well.
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpmod_unison[k];
        int poshiFM = oscposhiFM[nvoice][k];
        float posloFM = oscposloFM[nvoice][k];
        int freqhiFM = oscfreqhiFM[nvoice][k];
        float freqloFM = oscfreqloFM[nvoice][k];
        float oscVsFMratio = ((float)freqhiFM + freqloFM)
            / ((float)oscfreqhi[nvoice][k] + oscfreqlo[nvoice][k]);
        const float *smps = NoteVoicePar[nvoice].FMSmp;
        float oldsmpModded = FMFMoldsmpModded[nvoice][k];
        float oldsmpOrig = FMFMoldsmpOrig[nvoice][k];

        // Cache the samples we calculate for a certain nearby range. This is
        // possible since the base frequency never changes within one
        // `sent_buffersize`.
        const int cacheSize = synth->samplerate * 2 + synth->sent_buffersize;
        float cachedSamples[cacheSize];
        int cachedBackwards, cachedForwards, cacheCenter;
        cachedBackwards = cachedForwards = cacheCenter = cacheSize / 2 - 1;

        // The last cached sample was the previous sample.
        cachedSamples[cacheCenter++] = oldsmpOrig;

        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float pMod = parentFMmod[i] * oscVsFMratio;
            float sampleDistanceF = pMod / ((float)freqhiFM + freqloFM);
            int cachePosNear = cacheCenter + (int)roundf(sampleDistanceF);
            int cachePosFar = cachePosNear + ((sampleDistanceF < 0) ? -1 : +1);

            if (cachePosFar < 0 || cachePosFar >= cacheSize) {
                // This is just a precaution to avoid blowing the boundaries of
                // the cache. This was found by experimentation to only be hit
                // on very extreme, low frequency modulations, at which point
                // the usefulness for audio is questionable. At these rates it
                // is ridiculously memory and CPU intensive to keep going, so
                // just bail out with a zero curve instead.
                memset(tw, 0, synth->sent_buffersize * sizeof(float));
                return;
            }

            if (cacheCenter > cachedForwards) {
                cachedSamples[cacheCenter] = cachedSamples[cachedForwards] +
                    (smps[poshiFM] * (1.0f - posloFM)
                     + smps[poshiFM + 1] * posloFM);
                cachedForwards = cacheCenter;
            }

            int carposhi = poshiFM;
            float carposlo = posloFM;

            // Traverse samples backwards and forwards, and cache the results.
            if (cachedBackwards > cachePosFar) {
                for (int pos = cachedBackwards-1; pos >= cachePosFar; --pos) {
                    carposhi -= freqhiFM;
                    carposlo -= freqloFM;
                    if (carposlo < 0) {
                        carposlo++;
                        carposhi--;
                    }
                    if (carposhi < 0)
                        carposhi += synth->oscilsize;

                    cachedSamples[pos] = cachedSamples[pos+1] -
                        (smps[carposhi] * (1.0f - carposlo)
                         + smps[carposhi + 1] * carposlo);
                }
                cachedBackwards = cachePosFar;
            }

            if (cachedForwards < cachePosFar) {
                for (int pos = cachedForwards+1; pos <= cachePosFar; ++pos) {
                    carposhi += freqhiFM;
                    carposlo += freqloFM;
                    if (carposlo >= 1.0f) {
                        carposlo--;
                        carposhi++;
                    }
                    carposhi &= synth->oscilsize - 1;

                    cachedSamples[pos] = cachedSamples[pos-1] +
                        (smps[carposhi] * (1.0f - carposlo)
                         + smps[carposhi + 1] * carposlo);
                }
                cachedForwards = cachePosFar;
            }

            float interp = sampleDistanceF - truncf(sampleDistanceF);
            float finalsmp = cachedSamples[cachePosNear] * (1.0f - interp) +
                cachedSamples[cachePosFar] * interp;
            tw[i] = finalsmp - oldsmpModded;
            oldsmpModded = finalsmp;

            // New center is one sample forward.
            cacheCenter++;

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
        FMFMoldsmpModded[nvoice][k] = oldsmpModded;
        FMFMoldsmpOrig[nvoice][k] = cachedSamples[cacheCenter - 1];
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

            if(carposlo >= 1.0f)
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
        float *mod = freqbasedmod[nvoice] ? tmpmod_unison[k] : parentFMmod;
        int poshi = oscposhi[nvoice][k];
        float poslo = oscposlo[nvoice][k];
        int freqhi = oscfreqhi[nvoice][k];
        float freqlo = oscfreqlo[nvoice][k];
        const float *smps = NoteVoicePar[nvoice].OscilSmp;
        float oldsmpModded = oscFMoldsmpModded[nvoice][k];
        float oldsmpOrig = oscFMoldsmpOrig[nvoice][k];

        const int cacheSize = synth->samplerate * 2 + synth->sent_buffersize;
        float cachedSamples[cacheSize];
        int cachedBackwards, cachedForwards, cacheCenter;
        cachedBackwards = cachedForwards = cacheCenter = cacheSize / 2 - 1;

        cachedSamples[cacheCenter++] = oldsmpOrig;

        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float sampleDistanceF = mod[i] / ((float)freqhi + freqlo);
            int cachePosNear = cacheCenter + (int)roundf(sampleDistanceF);
            int cachePosFar = cachePosNear + ((sampleDistanceF < 0) ? -1 : +1);

            if (cachePosFar < 0 || cachePosFar >= cacheSize) {
                memset(tw, 0, synth->sent_buffersize * sizeof(float));
                return;
            }

            if (cacheCenter > cachedForwards) {
                cachedSamples[cacheCenter] = cachedSamples[cachedForwards] +
                    (smps[poshi] * (1.0f - poslo)
                     + smps[poshi + 1] * poslo);
                cachedForwards = cacheCenter;
            }

            int carposhi = poshi;
            float carposlo = poslo;

            // Traverse samples backwards and forwards, and cache the results.
            if (cachedBackwards > cachePosFar) {
                for (int pos = cachedBackwards-1; pos >= cachePosFar; --pos) {
                    carposhi -= freqhi;
                    carposlo -= freqlo;
                    if (carposlo < 0) {
                        carposlo++;
                        carposhi--;
                    }
                    if (carposhi < 0)
                        carposhi += synth->oscilsize;

                    cachedSamples[pos] = cachedSamples[pos+1] -
                        (smps[carposhi] * (1.0f - carposlo)
                         + smps[carposhi + 1] * carposlo);
                }
                cachedBackwards = cachePosFar;
            }

            if (cachedForwards < cachePosFar) {
                for (int pos = cachedForwards+1; pos <= cachePosFar; ++pos) {
                    carposhi += freqhi;
                    carposlo += freqlo;
                    if (carposlo >= 1.0f) {
                        carposlo--;
                        carposhi++;
                    }
                    carposhi &= synth->oscilsize - 1;

                    cachedSamples[pos] = cachedSamples[pos-1] +
                        (smps[carposhi] * (1.0f - carposlo)
                         + smps[carposhi + 1] * carposlo);
                }
                cachedForwards = cachePosFar;
            }

            float interp = sampleDistanceF - truncf(sampleDistanceF);
            float finalsmp = cachedSamples[cachePosNear] * (1.0f - interp) +
                cachedSamples[cachePosFar] * interp;
            tw[i] = finalsmp - oldsmpModded;
            oldsmpModded = finalsmp;

            // New center is one sample forward.
            cacheCenter++;

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
        oscFMoldsmpModded[nvoice][k] = oldsmpModded;
        oscFMoldsmpOrig[nvoice][k] = cachedSamples[cacheCenter - 1];
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
    } else if (NoteVoicePar[nvoice].Voice >= 0) {
        for (int k = 0; k < unison_size[nvoice]; ++k) {
            const float *smps = NoteVoicePar[NoteVoicePar[nvoice].Voice].VoiceOut;
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
        static int spot = 0;
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpwave_unison[k];
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            if (spot <= 0)
            {
                tw[i] = synth->numRandom() * 6.0f - 3.0f;
                spot = (synth->randomINT() >> 24);
            }
            else
            {
                tw[i] = 0.0f;
                spot--;
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

    if (!NoteEnabled)
        return 0;

    if (subVoiceNumber == -1) {
        memset(bypassl, 0, synth->sent_bufferbytes);
        memset(bypassr, 0, synth->sent_bufferbytes);
    }

    computeCurrentParameters();

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled || NoteVoicePar[nvoice].DelayTicks > 0)
            continue;

        if (NoteVoicePar[nvoice].Volume == 0.0f
            && NoteVoicePar[nvoice].VoiceOut == NULL) {

            // If the voice is muted and we are not producing sound for any sub
            // voices, then save the cycles.
            killVoice(nvoice);
            continue;
        }

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
                    if(unison_size[nvoice] > 2)
                        stereo_pos = k/2 / (float)((unison_size[nvoice] / 2) - 1) * 2.0f - 1.0f;
                } else if(unison_size[nvoice] > 1)
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
        if (adpars->randomVoicePan(nvoice)) // is random panning
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
        if (adpars->randomGlobalPan())         // it is random panning
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

        // Apply legato-specific sound signal modifications
        if (Legato.silent)    // Silencer
            if (Legato.msg != LM_FadeIn)
            {
                memset(outl, 0, synth->sent_bufferbytes);
                memset(outr, 0, synth->sent_bufferbytes);
            }
        switch(Legato.msg)
        {
            case LM_CatchUp:  // Continue the catch-up...
                if (Legato.decounter == -10)
                    Legato.decounter = Legato.fade.length;
                for (i = 0; i < synth->sent_buffersize; ++i)
                { // Yea, could be done without the loop...
                    Legato.decounter--;
                    if (Legato.decounter < 1)
                    {
                        synth->part[synth->legatoPart]->legatoFading &= 6;
                        // Catching-up done, we can finally set
                        // the note to the actual parameters.
                        Legato.decounter = -10;
                        Legato.msg = LM_ToNorm;
                        ADlegatonote(Legato.param.freq,
                                     Legato.param.vel,
                                     Legato.param.portamento,
                                     Legato.param.midinote,
                                     false);
                        break;
                    }
                }
                break;

            case LM_FadeIn:  // Fade-in
                if (Legato.decounter == -10)
                    Legato.decounter = Legato.fade.length;
                Legato.silent = false;
                for (i = 0; i < synth->sent_buffersize; ++i)
                {
                    Legato.decounter--;
                    if (Legato.decounter < 1)
                    {
                        Legato.decounter = -10;
                        Legato.msg = LM_Norm;
                        break;
                    }
                    Legato.fade.m += Legato.fade.step;
                    outl[i] *= Legato.fade.m;
                    outr[i] *= Legato.fade.m;
                }
                break;

            case LM_FadeOut:  // Fade-out, then set the catch-up
                if (Legato.decounter == -10)
                    Legato.decounter = Legato.fade.length;
                for (i = 0; i < synth->sent_buffersize; ++i)
                {
                    Legato.decounter--;
                    if (Legato.decounter < 1)
                    {
                        for (int j = i; j < synth->sent_buffersize; j++)
                            outl[j] = outr[j] = 0.0f;
                        Legato.decounter = -10;
                        Legato.silent = true;
                        // Fading-out done, now set the catch-up :
                        Legato.decounter = Legato.fade.length;
                        Legato.msg = LM_CatchUp;
                        float catchupfreq =
                            Legato.param.freq * (Legato.param.freq / Legato.lastfreq);
                        // This freq should make this now silent note to catch-up
                        //  (or should I say resync ?) with the heard note for the
                        // same length it stayed at the previous freq during the fadeout.
                        ADlegatonote(catchupfreq, Legato.param.vel, Legato.param.portamento,
                                     Legato.param.midinote, false);
                        break;
                    }
                    Legato.fade.m -= Legato.fade.step;
                    outl[i] *= Legato.fade.m;
                    outr[i] *= Legato.fade.m;
                }
                break;

            default:
                break;
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
}

// for future reference ... re replacing pow(x, y) by exp(y * log(x))
