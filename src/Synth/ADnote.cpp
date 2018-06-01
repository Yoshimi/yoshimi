/*
    ADnote.cpp - The "additive" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2017, Will Godfrey & others

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

    This file is derivative of original ZynAddSubFX code
    Modified September 2017
*/

#include <cmath>
#include <fftw3.h>
#include <cassert>
#include <iostream>

using namespace std;

#include "Synth/Envelope.h"
#include "Synth/LFO.h"
#include "DSP/Filter.h"
#include "Params/ADnoteParameters.h"
#include "Params/Controller.h"
#include "Misc/SynthEngine.h"
#include "Synth/ADnote.h"

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
    portamento(portamento_),
    synth(_synth)
{
    if (velocity > 1.0f)
        velocity = 1.0f;

    // Initialise some legato-specific vars
    Legato.msg = LM_Norm;
    Legato.fade.length = (int)truncf(synth->samplerate_f * 0.005f); // 0.005 seems ok.
    if (Legato.fade.length < 1)  // (if something's fishy)
        Legato.fade.length = 1;
    Legato.fade.step = (1.0f / Legato.fade.length);
    Legato.decounter = -10;
    Legato.param.freq = basefreq;
    Legato.param.vel = velocity;
    Legato.param.portamento = portamento;
    Legato.param.midinote = midinote;
    Legato.silent = besilent;

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

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (int i = 0; i < 14; i++)
            pinking[nvoice][i] = 0.0;

        adpars->VoicePar[nvoice].OscilSmp->newrandseed(); // so it really will be random
        NoteVoicePar[nvoice].OscilSmp = NULL;
        NoteVoicePar[nvoice].FMSmp = NULL;
        NoteVoicePar[nvoice].VoiceOut = NULL;

        NoteVoicePar[nvoice].FMVoice = -1;
        unison_size[nvoice] = 1;

        if (!adpars->VoicePar[nvoice].Enabled)
        {
            NoteVoicePar[nvoice].Enabled = false;
            continue; // the voice is disabled
        }

        int BendAdj = adpars->VoicePar[nvoice].PBendAdjust - 64;
        if (BendAdj % 24 == 0)
            NoteVoicePar[nvoice].BendAdjust = BendAdj / 24;
        else
            NoteVoicePar[nvoice].BendAdjust = BendAdj / 24.0f;

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

        NoteVoicePar[nvoice].OscilSmp = // the extra points contains the first point
            new float[synth->oscilsize + OSCIL_SMP_EXTRA_SAMPLES];

        // Get the voice's oscil or external's voice oscil
        int vc = nvoice;
        if (adpars->VoicePar[nvoice].Pextoscil != -1)
            vc = adpars->VoicePar[nvoice].Pextoscil;
        if (!adpars->GlobalPar.Hrandgrouping)
            adpars->VoicePar[vc].OscilSmp->newrandseed();
        int oscposhi_start =
            adpars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                               getVoiceBaseFreq(nvoice),
                                               adpars->VoicePar[nvoice].Presonance);

        // I store the first elments to the last position for speedups
        for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
            NoteVoicePar[nvoice].OscilSmp[synth->oscilsize + i] = NoteVoicePar[nvoice].OscilSmp[i];

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
                    break;
                case 2:
                    NoteVoicePar[nvoice].FMEnabled = RING_MOD;
                    break;
                case 3:
                    NoteVoicePar[nvoice].FMEnabled = PHASE_MOD;
                    break;
                case 4:
                    NoteVoicePar[nvoice].FMEnabled = FREQ_MOD;
                    break;
                case 5:
                    NoteVoicePar[nvoice].FMEnabled = PW_MOD;
                    break;
                default:
                    NoteVoicePar[nvoice].FMEnabled = NONE;
            }

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
    }

    max_unison = 1;
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        if (unison_size[nvoice] > max_unison)
            max_unison = unison_size[nvoice];

    tmpwave_unison = new float*[max_unison];
    for (int k = 0; k < max_unison; ++k)
    {
        tmpwave_unison[k] = (float*)fftwf_malloc(synth->bufferbytes);
        memset(tmpwave_unison[k], 0, synth->bufferbytes);
    }
    initParameters();
    ready = 1;
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
    if (externcall)
        Legato.msg = LM_Norm;
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

        // Get the voice's oscil or external's voice oscil
        int vc = nvoice;
        if (adpars->VoicePar[nvoice].Pextoscil != -1)
            vc = adpars->VoicePar[nvoice].Pextoscil;
        if (!adpars->GlobalPar.Hrandgrouping)
            adpars->VoicePar[vc].OscilSmp->newrandseed();

        adpars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                           getVoiceBaseFreq(nvoice),
                                           adpars->VoicePar[nvoice].Presonance);                                                       //(gf)Modif of the above line.

        // I store the first elments to the last position for speedups
        for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
            NoteVoicePar[nvoice].OscilSmp[synth->oscilsize + i] =
                NoteVoicePar[nvoice].OscilSmp[i];

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
            (int)truncf((expf(adpars->VoicePar[nvoice].PDelay / 127.0f
                         * logf(50.0f)) - 1.0f) / synth->sent_all_buffersize_f / 10.0f
                         * synth->samplerate_f);
    }

    ///    initParameters();

    ///////////////
    // Altered content of initParameters():

    int nvoice, i, voicetmp[NUM_VOICES];

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

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;

        if (adpars->VoicePar[nvoice].PFMAmpEnvelopeEnabled
           && NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
            FMnewamplitude[nvoice] *=
                NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
    }

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            voicetmp[i] = 0;
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            if (NoteVoicePar[i].FMVoice == nvoice && !voicetmp[i])
                voicetmp[i] = 1;
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

    delete [] NoteVoicePar[nvoice].OscilSmp;
    delete [] unison_base_freq_rap[nvoice];
    delete [] unison_freq_rap[nvoice];
    delete [] unison_invert_phase[nvoice];
    delete [] FMoldsmp[nvoice];
    delete [] unison_vibratto[nvoice].step;
    delete [] unison_vibratto[nvoice].position;

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

    if ((NoteVoicePar[nvoice].FMEnabled != NONE)
       && (NoteVoicePar[nvoice].FMVoice < 0))
        fftwf_free(NoteVoicePar[nvoice].FMSmp);

    if (NoteVoicePar[nvoice].VoiceOut)
        memset(NoteVoicePar[nvoice].VoiceOut, 0, synth->bufferbytes);
        // do not delete, yet: perhaps is used by another voice

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
        if (NoteVoicePar[nvoice].VoiceOut)
        {
            fftwf_free(NoteVoicePar[nvoice].VoiceOut);
            NoteVoicePar[nvoice].VoiceOut = NULL;
        }
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

    for (int k = 0; k < max_unison; ++k)
        fftwf_free(tmpwave_unison[k]);
    delete [] tmpwave_unison;
}


// Init the parameters
void ADnote::initParameters(void)
{
    int nvoice, i, voicetmp[NUM_VOICES];

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
            adpars->VoicePar[nvoice].FMSmp->newrandseed();
            NoteVoicePar[nvoice].FMSmp =
                (float*)fftwf_malloc((synth->oscilsize + OSCIL_SMP_EXTRA_SAMPLES) * sizeof(float));

            // Perform Anti-aliasing only on MORPH or RING MODULATION

            int vc = nvoice;
            if (adpars->VoicePar[nvoice].PextFMoscil != -1)
                vc = adpars->VoicePar[nvoice].PextFMoscil;

            float freqtmp = 1.0f;
            if (adpars->VoicePar[vc].FMSmp->Padaptiveharmonics != 0
               || (NoteVoicePar[nvoice].FMEnabled == MORPH)
               || (NoteVoicePar[nvoice].FMEnabled == RING_MOD))
               freqtmp = getFMVoiceBaseFreq(nvoice);

            if (!adpars->GlobalPar.Hrandgrouping)
                adpars->VoicePar[vc].FMSmp->newrandseed();

            for (int k = 0; k < unison_size[nvoice]; ++k)
                oscposhiFM[nvoice][k] =
                    (oscposhi[nvoice][k] + adpars->VoicePar[vc].FMSmp->
                        get(NoteVoicePar[nvoice].FMSmp, freqtmp)) % synth->oscilsize;

            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].FMSmp[synth->oscilsize + i] =
                    NoteVoicePar[nvoice].FMSmp[i];
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

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            voicetmp[i] = 0;
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
        {
            if (NoteVoicePar[i].FMVoice == nvoice && voicetmp[i] == 0)
            {
                NoteVoicePar[nvoice].VoiceOut = (float*)fftwf_malloc(synth->bufferbytes);
                voicetmp[i] = 1;
            }
            if(NoteVoicePar[nvoice].VoiceOut)
                memset(NoteVoicePar[nvoice].VoiceOut, 0, synth->bufferbytes);
        }
    }
}


// Get Voice's Modullator base frequency
float ADnote::getFMVoiceBaseFreq(int nvoice)
{
    float detune = NoteVoicePar[nvoice].FMDetune / 100.0f;
    return getVoiceBaseFreq(nvoice) * powf(2.0f, detune / 12.0f);
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
void ADnote::setfreq(int nvoice, float in_freq)
{
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float freq  = fabsf(in_freq) * unison_freq_rap[nvoice][k];
        float speed = freq * synth->oscilsize_f / synth->samplerate_f;
        if (isgreater(speed, synth->oscilsize_f))
            speed = synth->oscilsize_f;
        //F2I(speed, oscfreqhi[nvoice][k]);
        oscfreqhi[nvoice][k] = float2int(speed);
        oscfreqlo[nvoice][k] = speed - floor(speed);
    }
}


// Computes the frequency of an modullator oscillator
void ADnote::setfreqFM(int nvoice, float in_freq)
{
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float freq = fabsf(in_freq) * unison_freq_rap[nvoice][k];
        float speed = freq * synth->oscilsize_f / synth->samplerate_f;
        if (isgreater(speed, synth->oscilsize_f))
            speed = synth->oscilsize_f;
        //F2I(speed, oscfreqhiFM[nvoice][k]);
        oscfreqhiFM[nvoice][k] = float2int(speed);
        oscfreqloFM[nvoice][k] = speed - floor(speed);
    }
}


// Get Voice base frequency
float ADnote::getVoiceBaseFreq(int nvoice)
{
    float detune =
        NoteVoicePar[nvoice].Detune / 100.0f + NoteVoicePar[nvoice].FineDetune /
        100.0f * ctl->bandwidth.relbw * bandwidthDetuneMultiplier
        + NoteGlobalPar.Detune / 100.0f;

    if (!NoteVoicePar[nvoice].fixedfreq)
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
            float voicepitch = 0.0f;
            if (NoteVoicePar[nvoice].FreqLfo != NULL)
            {
                voicepitch += NoteVoicePar[nvoice].FreqLfo->lfoout() / 100.0f
                              * ctl->bandwidth.relbw;
            }

            if (NoteVoicePar[nvoice].FreqEnvelope != NULL)
            {
                voicepitch += NoteVoicePar[nvoice].FreqEnvelope->envout() / 100.0f;
            }

            float voicefreq = getVoiceBaseFreq(nvoice)
                              * powf(2.0f, (voicepitch + globalpitch) / 12.0f);
            voicefreq *=
                powf(ctl->pitchwheel.relfreq, NoteVoicePar[nvoice].BendAdjust); //change the frequency by the controller
            setfreq(nvoice, voicefreq * portamentofreqrap + NoteVoicePar[nvoice].OffsetHz);

            // Modulator
            if (NoteVoicePar[nvoice].FMEnabled != NONE)
            {
                float FMrelativepitch = NoteVoicePar[nvoice].FMDetune / 100.0f;
                if (NoteVoicePar[nvoice].FMFreqEnvelope != NULL)
                    FMrelativepitch +=
                        NoteVoicePar[nvoice].FMFreqEnvelope->envout() / 100.0f;
                float FMfreq;
                if (NoteVoicePar[nvoice].FMFreqFixed)
                    FMfreq = powf(2.0f, FMrelativepitch / 12.0f) * 440.0f;
                else
                    FMfreq = powf(2.0f, FMrelativepitch / 12.0f) * voicefreq * portamentofreqrap;
                setfreqFM(nvoice, FMfreq);
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

    int fadein = float2int((tmp < 8.0f) ? 8.0f : tmp); // how many samples is the fade-in
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
 * First the current possition and frequency are retrieved from the running
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


/*
 * Computes the Oscillator (Without Modulation) - CubicInterpolation

 * The differences from the Linear are to little to deserve to be used.
 * This is because I am using a large zynMaster->getOscilsize(), >512
 */
inline void ADnote::computeVoiceOscillatorCubicInterpolation(int nvoice)
{
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        int    poshi = oscposhi[nvoice][k];
        float  poslo = oscposlo[nvoice][k];
        int    freqhi = oscfreqhi[nvoice][k];
        int    freqlo = oscfreqlo[nvoice][k];
        float  *smps = NoteVoicePar[nvoice].OscilSmp;
        float  *tw = tmpwave_unison[k];
        float  xm1, x0, x1, x2, a, b, c;
        for (int i = 0; i < (synth->sent_buffersize); ++i)
        {
            xm1 = smps[poshi];
            x0 = smps[poshi + 1];
            x1 = smps[poshi + 2];
            x2 = smps[poshi + 3];
            a = (3.0 * (x0 - x1) - xm1 + x2) * 0.5;
            b = 2.0 * x1 + xm1 - (5.0 * x0 + x2) * 0.5;
            c = (x1 - xm1) * 0.5;
            tw[i] = (((a * poslo) + b) * poslo + c) * poslo + x0;
            poslo += freqlo;
            if (poslo >= 1.0)
            {
                poslo -= 1.0;
                ++poshi;
            }
            poshi += freqhi;
            poshi &= synth->oscilsize - 1;
        }
        oscposhi[nvoice][k] = poshi;
        oscposlo[nvoice][k] = poslo;
    }
}


// Computes the Oscillator (Morphing)
void ADnote::computeVoiceOscillatorMorph(int nvoice)
{
    computeVoiceOscillatorLinearInterpolation(nvoice);
    if (isgreater(FMnewamplitude[nvoice], 1.0f))
        FMnewamplitude[nvoice] = 1.0f;
    if (isgreater(FMoldamplitude[nvoice], 1.0f))
        FMoldamplitude[nvoice] = 1.0f;

    if (NoteVoicePar[nvoice].FMVoice >= 0)
    {
        // if I use VoiceOut[] as modullator
        int FMVoice = NoteVoicePar[nvoice].FMVoice;
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
            {
                float amp = interpolateAmplitude(FMoldamplitude[nvoice],
                                           FMnewamplitude[nvoice], i,
                                           synth->sent_buffersize);
                tw[i] = (tw[i] * (1.0f - amp)) + amp * NoteVoicePar[FMVoice].VoiceOut[i];
            }
        }
    }
    else
    {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            int poshiFM = oscposhiFM[nvoice][k];
            float posloFM  = oscposloFM[nvoice][k];
            int freqhiFM = oscfreqhiFM[nvoice][k];
            float freqloFM = oscfreqloFM[nvoice][k];
            float *tw = tmpwave_unison[k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
            {
                float amp = interpolateAmplitude(FMoldamplitude[nvoice],
                                           FMnewamplitude[nvoice], i,
                                           synth->sent_buffersize);
                tw[i] = (tw[i] * (1.0f - amp)) + amp * (NoteVoicePar[nvoice].FMSmp[poshiFM]
                          * (1 - posloFM) + NoteVoicePar[nvoice].FMSmp[poshiFM + 1]
                          * posloFM);
                posloFM += freqloFM;
                if (isgreaterequal(posloFM, 1.0f))
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
}


// Computes the Oscillator (Ring Modulation)
void ADnote::computeVoiceOscillatorRingModulation(int nvoice)
{
    float amp;
    computeVoiceOscillatorLinearInterpolation(nvoice);
    if (isgreater(FMnewamplitude[nvoice], 1.0f))
        FMnewamplitude[nvoice] = 1.0f;
    if (isgreater(FMoldamplitude[nvoice], 1.0f))
        FMoldamplitude[nvoice] = 1.0f;
    if (NoteVoicePar[nvoice].FMVoice >= 0)
    {
        // if I use VoiceOut[] as modullator
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            for (int i = 0; i < synth->sent_buffersize; ++i)
            {
                amp = interpolateAmplitude(FMoldamplitude[nvoice],
                                           FMnewamplitude[nvoice],
                                           i, synth->sent_buffersize);
                int FMVoice = NoteVoicePar[nvoice].FMVoice;
                tw[i] *= (1.0f - amp) + amp * NoteVoicePar[FMVoice].VoiceOut[i];
            }
        }
    }
    else
    {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            int poshiFM = oscposhiFM[nvoice][k];
            float posloFM  = oscposloFM[nvoice][k];
            int freqhiFM = oscfreqhiFM[nvoice][k];
            float freqloFM = oscfreqloFM[nvoice][k];
            float *tw = tmpwave_unison[k];

            for (int i = 0; i < synth->sent_buffersize; ++i)
            {
                amp = interpolateAmplitude(FMoldamplitude[nvoice],
                                           FMnewamplitude[nvoice], i,
                                           synth->sent_buffersize);
                tw[i] *= (NoteVoicePar[nvoice].FMSmp[poshiFM] * (1.0 - posloFM)
                          + NoteVoicePar[nvoice].FMSmp[poshiFM + 1] * posloFM)
                          * amp + (1.0 - amp);
                posloFM += freqloFM;
                if (isgreaterequal(posloFM, 1.0f))
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
}


// Computes the Oscillator (Phase Modulation or Frequency Modulation)
void ADnote::computeVoiceOscillatorFrequencyModulation(int nvoice, int FMmode)
{
    int carposhi = 0;
    int i, FMmodfreqhi = 0;
    float FMmodfreqlo = 0.0f;
    float carposlo = 0.0f;

    if (NoteVoicePar[nvoice].FMVoice >= 0)
    {
        // if I use VoiceOut[] as modulator
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            const float *smps = NoteVoicePar[NoteVoicePar[nvoice].FMVoice].VoiceOut;
            if (FMmode == PW_MOD && (k & 1))
                for (int i = 0; i < synth->buffersize; ++i)
                    tw[i] = -smps[i];
                else
                    memcpy(tw, smps, synth->bufferbytes);
        }
    }
    else
    {
        // Compute the modulator and store it in tmpwave_unison[][]
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            int poshiFM = oscposhiFM[nvoice][k];
            float posloFM  = oscposloFM[nvoice][k];
            int freqhiFM = oscfreqhiFM[nvoice][k];
            float freqloFM = oscfreqloFM[nvoice][k];
            float *tw = tmpwave_unison[k];
            const float *smps = NoteVoicePar[nvoice].FMSmp;

            for (i = 0; i < synth->sent_buffersize; ++i)
            {
                tw[i] = (smps[poshiFM] * ((1<<24) - posloFM)
                         + smps[poshiFM + 1] * posloFM) / (1.0f*(1<<24));
                if (FMmode == PW_MOD && (k & 1))
                    tw[i] = -tw[i];

                posloFM += freqloFM;
                if (isgreaterequal(posloFM, 1.0f))
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
    // Amplitude interpolation
    if (aboveAmplitudeThreshold(FMoldamplitude[nvoice], FMnewamplitude[nvoice]))
    {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            for (i = 0; i < synth->sent_buffersize; ++i)
                tw[i] *= interpolateAmplitude(FMoldamplitude[nvoice],
                                              FMnewamplitude[nvoice], i,
                                              synth->sent_buffersize);
        }
    }
    else
    {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            for (i = 0; i < synth->sent_buffersize; ++i)
                tw[i] *= FMnewamplitude[nvoice];
        }
    }


    // normalize: makes all sample-rates, oscil_sizes to produce same sound
    if (FMmode == FREQ_MOD) // Frequency modulation
    {
        float normalize = synth->oscilsize_f / 262144.0f * 44100.0f / synth->samplerate_f;
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            float  fmold = FMoldsmp[nvoice][k];
            for (i = 0; i < synth->sent_buffersize; ++i)
            {
                fmold = fmodf(fmold + tw[i] * normalize, synth->oscilsize_f);
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
            float *tw = tmpwave_unison[k];
            for (i = 0; i < synth->sent_buffersize; ++i)
                tw[i] *= normalize;
        }
    }

    // do the modulation
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float *tw = tmpwave_unison[k];
        int poshi = oscposhi[nvoice][k];
        float poslo = oscposlo[nvoice][k];
        int freqhi = oscfreqhi[nvoice][k];
        float freqlo = oscfreqlo[nvoice][k];

        for (i = 0; i < synth->sent_buffersize; ++i)
        {
            //F2I(tw[i], FMmodfreqhi);
            FMmodfreqhi = float2int(tw[i]);
            FMmodfreqlo = fmodf(tw[i] + 0.0000000001f, 1.0f);
            if (FMmodfreqhi < 0)
                FMmodfreqlo++;

            // carrier
            carposhi = poshi + FMmodfreqhi;
            carposlo = poslo + FMmodfreqlo;

            if (FMmode == PW_MOD && (k & 1))
                carposhi += NoteVoicePar[nvoice].phase_offset;

            if(isgreaterequal(carposlo, 1.0f))
            {
                carposhi++;
                carposlo -= 1.0f;
            }
            carposhi &= (synth->oscilsize - 1);

            tw[i] = NoteVoicePar[nvoice].OscilSmp[carposhi] * (1.0f - carposlo)
                    + NoteVoicePar[nvoice].OscilSmp[carposhi + 1] * carposlo;
            poslo += freqlo;
            if (isgreaterequal(poslo, 1.0f))
            {
                poslo = fmodf(poslo, 1.0f);
                poshi++;
            }

            poshi += freqhi;
            poshi &= synth->oscilsize - 1;
        }
        oscposhi[nvoice][k] = poshi;
        oscposlo[nvoice][k] = poslo;
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


// Compute the ADnote samples, returns 0 if the note is finished
int ADnote::noteout(float *outl, float *outr)
{
    Config &Runtime = synth->getRuntime();
    tmpwavel = Runtime.genTmp1;
    tmpwaver = Runtime.genTmp2;
    bypassl = Runtime.genTmp3;
    bypassr = Runtime.genTmp4;
    int i, nvoice;
    memset(outl, 0, synth->sent_bufferbytes);
    memset(outr, 0, synth->sent_bufferbytes);

    if (!NoteEnabled)
        return 0;

    memset(bypassl, 0, synth->sent_bufferbytes);
    memset(bypassr, 0, synth->sent_bufferbytes);
    computeCurrentParameters();

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled || NoteVoicePar[nvoice].DelayTicks > 0)
            continue;
        switch (NoteVoicePar[nvoice].noisetype)
        {
            case 0: //  sound
                switch(NoteVoicePar[nvoice].FMEnabled)
                {
                    case MORPH:
                        computeVoiceOscillatorMorph(nvoice);
                        break;
                    case RING_MOD:
                        computeVoiceOscillatorRingModulation(nvoice);
                        break;
                    case FREQ_MOD:
                    case PHASE_MOD:
                    case PW_MOD:
                        computeVoiceOscillatorFrequencyModulation(nvoice, NoteVoicePar[nvoice].FMEnabled);
                        break;
                    default:
                        computeVoiceOscillatorLinearInterpolation(nvoice);
                        /*
                         * cubic lost something over the years
                         * it runs but sounds horrible :(
                         */
                        //if (Runtime.Interpolation)
                            //computeVoiceOscillatorCubicInterpolation(nvoice);
                        break;
                }
                break;
            case 1:
                computeVoiceNoise(nvoice); // white noise
                break;
            default:
                ComputeVoicePinkNoise(nvoice); // pink noise
                break;
        }

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

        // Put the ADnote samples in VoiceOut (without appling Global volume,
        // because I wish to use this voice as a modulator)
        if (NoteVoicePar[nvoice].VoiceOut)
        {
            if (stereo)
                for (i = 0; i < synth->sent_buffersize; ++i)
                    NoteVoicePar[nvoice].VoiceOut[i] = tmpwavel[i] + tmpwaver[i];
            else // mono
                for (i = 0; i < synth->sent_buffersize; ++i)
                    NoteVoicePar[nvoice].VoiceOut[i] = tmpwavel[i];
        }

        pangainL = adpars->VoicePar[nvoice].pangainL; // assume voice not random pan
        pangainR = adpars->VoicePar[nvoice].pangainR;
        if (adpars->randomVoicePan(nvoice)) // is random panning
        {
            pangainL = NoteVoicePar[nvoice].randpanL;
            pangainR = NoteVoicePar[nvoice].randpanR;
        }
        // Add the voice that do not bypass the filter to out
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
        // check if there is necesary to proces the voice longer
        // (if the Amplitude envelope isn't finished)
        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
            if (NoteVoicePar[nvoice].AmpEnvelope->finished())
                killVoice(nvoice);
    }

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


    // Check if the global amplitude is finished.
    // If it does, disable the note
    if (NoteGlobalPar.AmpEnvelope->finished())
    {
        for (i = 0; i < synth->sent_buffersize; ++i) // fade-out
        {
            float tmp = 1.0f - (float)i / synth->sent_buffersize_f;
            outl[i] *= tmp;
            outr[i] *= tmp;
        }
        killNote();
    }
    return 1;
}


// Relase the key (NoteOff)
void ADnote::relasekey(void)
{
    int nvoice;
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;
        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
            NoteVoicePar[nvoice].AmpEnvelope->relasekey();
        if (NoteVoicePar[nvoice].FreqEnvelope != NULL)
            NoteVoicePar[nvoice].FreqEnvelope->relasekey();
        if (NoteVoicePar[nvoice].FilterEnvelope != NULL)
            NoteVoicePar[nvoice].FilterEnvelope->relasekey();
        if (NoteVoicePar[nvoice].FMFreqEnvelope != NULL)
            NoteVoicePar[nvoice].FMFreqEnvelope->relasekey();
        if (NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
            NoteVoicePar[nvoice].FMAmpEnvelope->relasekey();
    }
    NoteGlobalPar.FreqEnvelope->relasekey();
    NoteGlobalPar.FilterEnvelope->relasekey();
    NoteGlobalPar.AmpEnvelope->relasekey();
}

// for future reference ... re replacing pow(x, y) by exp(y * log(x))
