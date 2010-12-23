/*
    ADnote.cpp - The "additive" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

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

    This file is derivative of original ZynAddSubFX code, modified November 2010
*/

#include <cmath>

using namespace std;

#include "Synth/Envelope.h"
#include "Synth/LFO.h"
#include "DSP/Filter.h"
#include "Params/ADnoteParameters.h"
#include "Params/Controller.h"
#include "Misc/SynthEngine.h"
#include "Synth/ADnote.h"

ADnote::ADnote(ADnoteParameters *pars, Controller *ctl_, float freq_,
               float velocity_, int portamento_, int midinote_,
               bool besilent) :
    ready(0),
    partparams(pars),
    stereo(pars->GlobalPar.PStereo),
    midinote(midinote_),
    velocity(velocity_),
    basefreq(freq_),
    NoteEnabled(true),
    ctl(ctl_),
    time(0.0f),
    portamento(portamento_)
{
    if (velocity > 1.0f)
        velocity = 1.0f;
    tmpwavel = (float*)fftwf_malloc(synth->bufferbytes);
    tmpwaver = (float*)fftwf_malloc(synth->bufferbytes);
    bypassl = (float*)fftwf_malloc(synth->bufferbytes);
    bypassr = (float*)fftwf_malloc(synth->bufferbytes);

    // Initialise some legato-specific vars
    Legato.msg = LM_Norm;
    Legato.fade.length = lrintf(synth->samplerate_f * 0.005f); // 0.005 seems ok.
    if (Legato.fade.length < 1)  // (if something's fishy)
        Legato.fade.length = 1;
    Legato.fade.step = (1.0f / Legato.fade.length);
    Legato.decounter = -10;
    Legato.param.freq = basefreq;
    Legato.param.vel = velocity;
    Legato.param.portamento = portamento;
    Legato.param.midinote = midinote;
    Legato.silent = besilent;

    NoteGlobalPar.Detune = getDetune(pars->GlobalPar.PDetuneType, pars->GlobalPar.PCoarseDetune, pars->GlobalPar.PDetune);
    bandwidthDetuneMultiplier = pars->getBandwidthDetuneMultiplier();

    if (pars->GlobalPar.PPanning == 0)
        NoteGlobalPar.Panning = synth->numRandom();
    else
        NoteGlobalPar.Panning = pars->GlobalPar.PPanning / 128.0f;

    NoteGlobalPar.FilterCenterPitch =
        pars->GlobalPar.GlobalFilter->getfreq() //center freq
            + pars->GlobalPar.PFilterVelocityScale / 127.0f * 6.0f * (velF(velocity, pars->GlobalPar.PFilterVelocityScaleFunction) - 1);

    if (pars->GlobalPar.PPunchStrength != 0)
    {
        NoteGlobalPar.Punch.Enabled = 1;
        NoteGlobalPar.Punch.t = 1.0f; //start from 1.0 and to 0.0
        NoteGlobalPar.Punch.initialvalue =
            ((powf(10.0f, 1.5f * pars->GlobalPar.PPunchStrength / 127.0f) - 1.0f)
             * velF(velocity, pars->GlobalPar.PPunchVelocitySensing));
        float time = powf(10.0f, 3.0f * pars->GlobalPar.PPunchTime / 127.0f) / 10000.0f; // 0.1 .. 100 ms
        float stretch = powf(440.0f / basefreq, pars->GlobalPar.PPunchStretch / 64.0f);
        NoteGlobalPar.Punch.dt = 1.0f / (time * synth->samplerate_f * stretch);
    }
    else
        NoteGlobalPar.Punch.Enabled = 0;

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        NoteVoicePar[nvoice].OscilSmp = NULL;
        NoteVoicePar[nvoice].FMSmp = NULL;
        NoteVoicePar[nvoice].VoiceOut = NULL;

        NoteVoicePar[nvoice].FMVoice = -1;
        unison_size[nvoice] = 1;

        if (!pars->VoicePar[nvoice].Enabled)
        {
            NoteVoicePar[nvoice].Enabled = false;
            continue; // the voice is disabled
        }
        pars->VoicePar[nvoice].OscilSmp->newrandseed();
        unison_stereo_spread[nvoice] =
            pars->VoicePar[nvoice].Unison_stereo_spread / 127.0f;
        int unison = pars->VoicePar[nvoice].Unison_size;
        if (unison < 1)
            unison = 1;

        // compute unison
        unison_size[nvoice] = unison;

        unison_base_freq_rap[nvoice] = new float[unison];
        unison_freq_rap[nvoice] = new float[unison];
        unison_invert_phase[nvoice] = new bool[unison];
        float unison_spread = pars->getUnisonFrequencySpreadCents(nvoice);
        float unison_real_spread = powf(2.0f, (unison_spread * 0.5f) / 1200.0f);
        float unison_vibratto_a = pars->VoicePar[nvoice].Unison_vibratto / 127.0f;                                  //0.0 .. 1.0

        switch (unison)
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
                    for (int k = 0; k < unison; ++k)
                    {
                        float step = (k / (float) (unison - 1)) * 2.0f - 1.0f;  //this makes the unison spread more uniform
                        float val  = step + (synth->numRandom() * 2.0f - 1.0f) / (unison - 1);
                        unison_values[k] = val;
                        if (val > max)
                            max = val;
                        if (val < min)
                            min = val;
                    }
                    float diff = max - min;
                    for (int k = 0; k < unison; ++k)
                    {
                        unison_values[k] =
                            (unison_values[k] - (max + min) * 0.5f) / diff; // the lowest value will be -1 and the highest will be 1
                        unison_base_freq_rap[nvoice][k] =
                            powf(2.0f, (unison_spread * unison_values[k]) / 1200.0f);
                    }
                }
        }

        // unison vibrattos
        if (unison > 1)
        {
            for (int k = 0; k < unison; ++k) // reduce the frequency difference for larger vibrattos
                unison_base_freq_rap[nvoice][k] = 1.0f + (unison_base_freq_rap[nvoice][k] - 1.0f) * (1.0f - unison_vibratto_a);
        }
        unison_vibratto[nvoice].step = new float[unison];
        unison_vibratto[nvoice].position = new float[unison];
        unison_vibratto[nvoice].amplitude = (unison_real_spread - 1.0f) * unison_vibratto_a;

        float increments_per_second = synth->samplerate_f / synth->buffersize_f;
        float vibratto_base_period  = 0.25f * powf(2.0f, (1.0f - pars->VoicePar[nvoice].Unison_vibratto_speed / 127.0f) * 4.0f);
        for (int k = 0; k < unison; ++k)
        {
            unison_vibratto[nvoice].position[k] = synth->numRandom() * 1.8f - 0.9f;
            float vibratto_period = vibratto_base_period * powf(2.0f, synth->numRandom() * 2.0f - 1.0f);                                                        //make period to vary randomly from 50% to 200% vibratto base period
            float m = 4.0f / (vibratto_period * increments_per_second);
            if (synth->numRandom() < 0.5f)
                m = -m;
            unison_vibratto[nvoice].step[k] = m;
        }

        if (unison == 1) // no vibratto for a single voice
        {
            unison_vibratto[nvoice].step[0] = 0.0f;
            unison_vibratto[nvoice].position[0] = 0.0f;
            unison_vibratto[nvoice].amplitude = 0.0f;
        }

        // phase invert for unison
        unison_invert_phase[nvoice][0] = false;
        if (unison != 1)
        {
            int inv = pars->VoicePar[nvoice].Unison_invert_phase;
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
        NoteVoicePar[nvoice].fixedfreq = pars->VoicePar[nvoice].Pfixedfreq;
        NoteVoicePar[nvoice].fixedfreqET = pars->VoicePar[nvoice].PfixedfreqET;

        // use the Globalpars.detunetype if the detunetype is 0
        if (pars->VoicePar[nvoice].PDetuneType != 0)
        {
            NoteVoicePar[nvoice].Detune =
                getDetune(pars->VoicePar[nvoice].PDetuneType,
                          pars->VoicePar[nvoice].PCoarseDetune, 8192);                                                                        //coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getDetune(pars->VoicePar[nvoice].PDetuneType, 0,
                          pars->VoicePar[nvoice].PDetune); // fine detune
        }
        else
        {
            NoteVoicePar[nvoice].Detune =
                getDetune(pars->GlobalPar.PDetuneType, pars->VoicePar[nvoice].
                          PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getDetune(pars->GlobalPar.PDetuneType, 0,
                          pars->VoicePar[nvoice].PDetune); // fine detune
        }
        if (pars->VoicePar[nvoice].PFMDetuneType != 0)
            NoteVoicePar[nvoice].FMDetune =
                getDetune(pars->VoicePar[nvoice].PFMDetuneType,
                          pars->VoicePar[nvoice].PFMCoarseDetune,
                          pars->VoicePar[nvoice].PFMDetune);
        else
            NoteVoicePar[nvoice].FMDetune =
                getDetune(pars->GlobalPar.PDetuneType, pars->VoicePar[nvoice].
                          PFMCoarseDetune, pars->VoicePar[nvoice].PFMDetune);

        memset(oscposhi[nvoice], 0, unison * sizeof(int));
        memset(oscposlo[nvoice], 0, unison * sizeof(float));
        memset(oscposhiFM[nvoice], 0, unison * sizeof(int));
        memset(oscposloFM[nvoice], 0, unison * sizeof(float));

        NoteVoicePar[nvoice].OscilSmp = // the extra points contains the first point
            new float[synth->oscilsize + OSCIL_SMP_EXTRA_SAMPLES];

        // Get the voice's oscil or external's voice oscil
        int vc = nvoice;
        if (pars->VoicePar[nvoice].Pextoscil != -1)
            vc = pars->VoicePar[nvoice].Pextoscil;
        if (!pars->GlobalPar.Hrandgrouping)
            pars->VoicePar[vc].OscilSmp->newrandseed();
        int oscposhi_start =
            pars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp, getVoiceBaseFreq(nvoice), pars->VoicePar[nvoice].Presonance);

        // I store the first elments to the last position for speedups
        for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
            NoteVoicePar[nvoice].OscilSmp[synth->oscilsize + i] = NoteVoicePar[nvoice].OscilSmp[i];

        oscposhi_start += lrintf((pars->VoicePar[nvoice].Poscilphase - 64.0f) / 128.0f * synth->oscilsize_f + synth->oscilsize_f * 4.0f);
        oscposhi_start %= synth->oscilsize;

        for (int k = 0; k < unison; ++k)
        {
            oscposhi[nvoice][k] = oscposhi_start;
            oscposhi_start = (int)(synth->numRandom() * (synth->oscilsize - 1)); //put random starting point for other subvoices
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
            pars->VoicePar[nvoice].VoiceFilter->getfreq();
        NoteVoicePar[nvoice].filterbypass = pars->VoicePar[nvoice].Pfilterbypass;

        switch (pars->VoicePar[nvoice].PFMEnabled)
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
                NoteVoicePar[nvoice].FMEnabled = PITCH_MOD;
                break;
            default:
                NoteVoicePar[nvoice].FMEnabled = NONE;
        }

        NoteVoicePar[nvoice].FMVoice = pars->VoicePar[nvoice].PFMVoice;
        NoteVoicePar[nvoice].FMFreqEnvelope = NULL;
        NoteVoicePar[nvoice].FMAmpEnvelope = NULL;

        // Compute the Voice's modulator volume (incl. damping)
        float fmvoldamp = powf(440.0f / getVoiceBaseFreq(nvoice), pars->VoicePar[nvoice].PFMVolumeDamp / 64.0f - 1.0f);
        switch (NoteVoicePar[nvoice].FMEnabled)
        {
        case PHASE_MOD:
            fmvoldamp = powf(440.0f / getVoiceBaseFreq(nvoice), pars->VoicePar[nvoice].PFMVolumeDamp / 64.0f);
            NoteVoicePar[nvoice].FMVolume = (expf(pars->VoicePar[nvoice].PFMVolume / 127.0f * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
            break;
        case FREQ_MOD:
            NoteVoicePar[nvoice].FMVolume =
                (expf(pars->VoicePar[nvoice].PFMVolume / 127.0f * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
            break;
        //    case PITCH_MOD:NoteVoicePar[nvoice].FMVolume=(pars->VoicePar[nvoice].PFMVolume/127.0*8.0)*fmvoldamp;//???????????
        //	          break;
        default:
            if (fmvoldamp > 1.0f)
                fmvoldamp = 1.0f;
            NoteVoicePar[nvoice].FMVolume = pars->VoicePar[nvoice].PFMVolume / 127.0f * fmvoldamp;
        }

        //Voice's modulator velocity sensing
        NoteVoicePar[nvoice].FMVolume *= velF(velocity, partparams->VoicePar[nvoice].PFMVelocityScaleFunction);

        FMoldsmp[nvoice] = new float [unison];
        memset(FMoldsmp[nvoice], 0, unison * sizeof(float));

        firsttick[nvoice] = 1;
        NoteVoicePar[nvoice].DelayTicks = lrintf((expf(pars->VoicePar[nvoice].PDelay / 127.0f * logf(50.0f)) - 1.0f) / synth->buffersize / 10.0f * synth->samplerate_f);
    }

    max_unison = 1;
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        if (unison_size[nvoice] > max_unison)
            max_unison = unison_size[nvoice];

    tmpwave_unison = new float*[max_unison];
    for (int k = 0; k < max_unison; ++k)
    {
        tmpwave_unison[k] = (float*)fftwf_malloc(synth->bufferbytes);
        memset(tmpwave_unison[k], 0, synth->buffersize * sizeof(float));
    }

    initParameters();
    ready = 1;
}


// ADlegatonote: This function is (mostly) a copy of ADnote(...) and
// initParameters() stuck together with some lines removed so that it
// only alter the already playing note (to perform legato). It is
// possible I left stuff that is not required for this.
void ADnote::ADlegatonote(float freq_, float velocity_, int portamento_, int midinote_, bool externcall)
{
    ADnoteParameters *pars = partparams;
    //Controller *ctl_=ctl;

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

    NoteGlobalPar.Detune = getDetune(pars->GlobalPar.PDetuneType, pars->GlobalPar.PCoarseDetune, pars->GlobalPar.PDetune);
    bandwidthDetuneMultiplier = pars->getBandwidthDetuneMultiplier();

    if (pars->GlobalPar.PPanning == 0)
        NoteGlobalPar.Panning = synth->numRandom();
    else
        NoteGlobalPar.Panning = pars->GlobalPar.PPanning / 128.0f;

    NoteGlobalPar.FilterCenterPitch =
        pars->GlobalPar.GlobalFilter->getfreq() + pars->GlobalPar.PFilterVelocityScale / 127.0f * 6.0f  * (velF(velocity, pars->GlobalPar.PFilterVelocityScaleFunction) - 1);

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue; //(gf) Stay the same as first note in legato.

        NoteVoicePar[nvoice].fixedfreq = pars->VoicePar[nvoice].Pfixedfreq;
        NoteVoicePar[nvoice].fixedfreqET = pars->VoicePar[nvoice].PfixedfreqET;

        //use the Globalpars.detunetype if the detunetype is 0
        if (pars->VoicePar[nvoice].PDetuneType != 0)
        {
            NoteVoicePar[nvoice].Detune =
                getDetune(pars->VoicePar[nvoice].PDetuneType,
                          pars->VoicePar[nvoice].PCoarseDetune, 8192);                                                                        //coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getDetune(pars->VoicePar[nvoice].PDetuneType, 0,
                          pars->VoicePar[nvoice].PDetune); // fine detune
        }
        else
        {
            NoteVoicePar[nvoice].Detune =
                getDetune(pars->GlobalPar.PDetuneType,
                          pars->VoicePar[nvoice].PCoarseDetune, 8192);                                                                        //coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getDetune(pars->GlobalPar.PDetuneType, 0,
                          pars->VoicePar[nvoice].PDetune);                               //fine detune
        }
        if (pars->VoicePar[nvoice].PFMDetuneType != 0)
            NoteVoicePar[nvoice].FMDetune =
                getDetune(pars->VoicePar[nvoice].PFMDetuneType,
                          pars->VoicePar[nvoice].PFMCoarseDetune,
                          pars->VoicePar[nvoice].PFMDetune);
        else
            NoteVoicePar[nvoice].FMDetune =
                getDetune(pars->GlobalPar.PDetuneType,
                          pars->VoicePar[nvoice].PFMCoarseDetune,
                          pars->VoicePar[nvoice].PFMDetune);

        // Get the voice's oscil or external's voice oscil
        int vc = nvoice;
        if (pars->VoicePar[nvoice].Pextoscil != -1)
            vc = pars->VoicePar[nvoice].Pextoscil;
        if (!pars->GlobalPar.Hrandgrouping)
            pars->VoicePar[vc].OscilSmp->newrandseed();

        pars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                         getVoiceBaseFreq(nvoice),
                                         pars->VoicePar[nvoice].Presonance);                                                       //(gf)Modif of the above line.

        // I store the first elments to the last position for speedups
        for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
            NoteVoicePar[nvoice].OscilSmp[synth->oscilsize + i] =
                NoteVoicePar[nvoice].OscilSmp[i];

        NoteVoicePar[nvoice].FilterCenterPitch =
            pars->VoicePar[nvoice].VoiceFilter->getfreq();
        NoteVoicePar[nvoice].filterbypass =
            pars->VoicePar[nvoice].Pfilterbypass;

        NoteVoicePar[nvoice].FMVoice = pars->VoicePar[nvoice].PFMVoice;

        // Compute the Voice's modulator volume (incl. damping)
        float fmvoldamp = powf(440.0f / getVoiceBaseFreq(nvoice), pars->VoicePar[nvoice].PFMVolumeDamp / 64.0f - 1.0f);

        switch (NoteVoicePar[nvoice].FMEnabled)
        {
            case PHASE_MOD:
                fmvoldamp = powf(440.0f / getVoiceBaseFreq(nvoice), pars->VoicePar[nvoice].PFMVolumeDamp / 64.0f);
                NoteVoicePar[nvoice].FMVolume = (expf(pars->VoicePar[nvoice].PFMVolume / 127.0f * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;
            case FREQ_MOD:
                NoteVoicePar[nvoice].FMVolume = (expf(pars->VoicePar[nvoice].PFMVolume / 127.0f * FM_AMP_MULTIPLIER) - 1.0) * fmvoldamp * 4.0f;
                break;
            //    case PITCH_MOD:NoteVoicePar[nvoice].FMVolume=(pars->VoicePar[nvoice].PFMVolume/127.0*8.0)*fmvoldamp;//???????????
            //	          break;
            default:
                if (fmvoldamp > 1.0f)
                    fmvoldamp = 1.0f;
                NoteVoicePar[nvoice].FMVolume =
                    pars->VoicePar[nvoice].PFMVolume / 127.0f * fmvoldamp;
        }

        // Voice's modulator velocity sensing
        NoteVoicePar[nvoice].FMVolume *=
            velF(velocity, partparams->VoicePar[nvoice].PFMVelocityScaleFunction);

        NoteVoicePar[nvoice].DelayTicks =
            lrintf((expf(pars->VoicePar[nvoice].PDelay / 127.0f * logf(50.0f)) - 1.0f) / synth->buffersize_f / 10.0f * synth->samplerate_f);
    }

    ///    initParameters();

    ///////////////
    // Altered content of initParameters():

    int nvoice, i, tmp[NUM_VOICES];

    NoteGlobalPar.Volume = 4.0f * powf(0.1f, 3.0f * (1.0f - partparams->GlobalPar.PVolume / 96.0f))  //-60 dB .. 0 dB
            * velF(velocity, partparams->GlobalPar.PAmpVelocityScaleFunction);                                                      //velocity sensing
    globalnewamplitude = NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB() * NoteGlobalPar.AmpLfo->amplfoout();
    NoteGlobalPar.FilterQ = partparams->GlobalPar.GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking = partparams->GlobalPar.GlobalFilter->getfreqtracking(basefreq);

    // Forbids the Modulation Voice to be greater or equal than voice
    for (i = 0; i < NUM_VOICES; ++i)
        if (NoteVoicePar[i].FMVoice >= i)
            NoteVoicePar[i].FMVoice = -1;

    // Voice Parameter init
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (NoteVoicePar[nvoice].Enabled == 0)
            continue;

        NoteVoicePar[nvoice].noisetype = partparams->VoicePar[nvoice].Type;
        // Voice Amplitude Parameters Init
        NoteVoicePar[nvoice].Volume = powf(0.1f, 3.0f * (1.0f - partparams->VoicePar[nvoice].PVolume / 127.0f))                  // -60 dB .. 0 dB
            * velF(velocity, partparams->VoicePar[nvoice].PAmpVelocityScaleFunction);                                //velocity

        if (partparams->VoicePar[nvoice].PVolumeminus != 0)
            NoteVoicePar[nvoice].Volume = -NoteVoicePar[nvoice].Volume;

        if (partparams->VoicePar[nvoice].PPanning == 0)
            NoteVoicePar[nvoice].Panning = synth->numRandom(); // random panning
        else
            NoteVoicePar[nvoice].Panning =
                partparams->VoicePar[nvoice].PPanning / 128.0f;

        newamplitude[nvoice] = 1.0f;
        if (partparams->VoicePar[nvoice].PAmpEnvelopeEnabled
           && NoteVoicePar[nvoice].AmpEnvelope != NULL)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();

        if (partparams->VoicePar[nvoice].PAmpLfoEnabled
             && NoteVoicePar[nvoice].AmpLfo != NULL)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();

        NoteVoicePar[nvoice].FilterFreqTracking =
            partparams->VoicePar[nvoice].VoiceFilter->getfreqtracking(basefreq);

        // Voice Modulation Parameters Init
        if (NoteVoicePar[nvoice].FMEnabled != NONE
            && NoteVoicePar[nvoice].FMVoice < 0)
        {
            partparams->VoicePar[nvoice].FMSmp->newrandseed();

            //Perform Anti-aliasing only on MORPH or RING MODULATION

            int vc = nvoice;
            if (partparams->VoicePar[nvoice].PextFMoscil != -1)
                vc = partparams->VoicePar[nvoice].PextFMoscil;

            float tmp = 1.0f;
            if (partparams->VoicePar[vc].FMSmp->Padaptiveharmonics != 0
               || NoteVoicePar[nvoice].FMEnabled == MORPH
               || NoteVoicePar[nvoice].FMEnabled == RING_MOD)
                tmp = getFMVoiceBaseFreq(nvoice);

            if (!partparams->GlobalPar.Hrandgrouping)
                partparams->VoicePar[vc].FMSmp->newrandseed();

            ///oscposhiFM[nvoice]=(oscposhi[nvoice]+partparams->VoicePar[vc].FMSmp->get(NoteVoicePar[nvoice].FMSmp,tmp)) % synth->oscilsize;
            // /	oscposhi[nvoice]+partparams->VoicePar[vc].FMSmp->get(NoteVoicePar[nvoice].FMSmp,tmp); //(gf) Modif of the above line.
            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].FMSmp[synth->oscilsize + i] =
                    NoteVoicePar[nvoice].FMSmp[i];
            ///oscposhiFM[nvoice]+=(int)((partparams->VoicePar[nvoice].PFMoscilphase-64.0)/128.0*synth->oscilsize+synth->oscilsize*4);
            ///oscposhiFM[nvoice]%=synth->oscilsize;
        }

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;

        if (partparams->VoicePar[nvoice].PFMAmpEnvelopeEnabled
           && NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
            FMnewamplitude[nvoice] *=
                NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
    }

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            tmp[i] = 0;
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            if (NoteVoicePar[i].FMVoice == nvoice && tmp[i] == 0)
                tmp[i] = 1;
    }
    ///////////////

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
        delete (NoteVoicePar[nvoice].FreqEnvelope);
    NoteVoicePar[nvoice].FreqEnvelope = NULL;

    if (NoteVoicePar[nvoice].FreqLfo != NULL)
        delete (NoteVoicePar[nvoice].FreqLfo);
    NoteVoicePar[nvoice].FreqLfo = NULL;

    if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
        delete (NoteVoicePar[nvoice].AmpEnvelope);
    NoteVoicePar[nvoice].AmpEnvelope = NULL;

    if (NoteVoicePar[nvoice].AmpLfo != NULL)
        delete (NoteVoicePar[nvoice].AmpLfo);
    NoteVoicePar[nvoice].AmpLfo = NULL;

    if (NoteVoicePar[nvoice].VoiceFilterL != NULL)
        delete (NoteVoicePar[nvoice].VoiceFilterL);
    NoteVoicePar[nvoice].VoiceFilterL = NULL;

    if (NoteVoicePar[nvoice].VoiceFilterR != NULL)
        delete (NoteVoicePar[nvoice].VoiceFilterR);
    NoteVoicePar[nvoice].VoiceFilterR = NULL;

    if (NoteVoicePar[nvoice].FilterEnvelope != NULL)
        delete (NoteVoicePar[nvoice].FilterEnvelope);
    NoteVoicePar[nvoice].FilterEnvelope = NULL;

    if (NoteVoicePar[nvoice].FilterLfo != NULL)
        delete (NoteVoicePar[nvoice].FilterLfo);
    NoteVoicePar[nvoice].FilterLfo = NULL;

    if (NoteVoicePar[nvoice].FMFreqEnvelope != NULL)
        delete (NoteVoicePar[nvoice].FMFreqEnvelope);
    NoteVoicePar[nvoice].FMFreqEnvelope = NULL;

    if (NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
        delete (NoteVoicePar[nvoice].FMAmpEnvelope);
    NoteVoicePar[nvoice].FMAmpEnvelope = NULL;

    if ((NoteVoicePar[nvoice].FMEnabled != NONE)
       && (NoteVoicePar[nvoice].FMVoice < 0))
        delete [] NoteVoicePar[nvoice].FMSmp;

    if (NoteVoicePar[nvoice].VoiceOut != NULL)
        memset(NoteVoicePar[nvoice].VoiceOut, 0, synth->buffersize
                * sizeof(float));//do not delete, yet: perhaps is used by another voice

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

        // delete VoiceOut
        if (NoteVoicePar[nvoice].VoiceOut != NULL)
            delete NoteVoicePar[nvoice].VoiceOut;
        NoteVoicePar[nvoice].VoiceOut = NULL;
    }

    delete NoteGlobalPar.FreqEnvelope;
    delete NoteGlobalPar.FreqLfo;
    delete NoteGlobalPar.AmpEnvelope;
    delete NoteGlobalPar.AmpLfo;
    delete NoteGlobalPar.GlobalFilterL;
    if (stereo != 0)
        delete NoteGlobalPar.GlobalFilterR;
    delete NoteGlobalPar.FilterEnvelope;
    delete NoteGlobalPar.FilterLfo;

    NoteEnabled = false;
}

ADnote::~ADnote()
{
    if (NoteEnabled)
        killNote();
    fftwf_free(tmpwavel);
    fftwf_free(tmpwaver);
    fftwf_free(bypassl);
    fftwf_free(bypassr);
    for (int k = 0; k < max_unison; ++k)
        fftwf_free(tmpwave_unison[k]);
    delete [] tmpwave_unison;
}


// Init the parameters
void ADnote::initParameters()
{
    int nvoice, i, tmp[NUM_VOICES];

    // Global Parameters
    NoteGlobalPar.FreqEnvelope = new Envelope(partparams->GlobalPar.FreqEnvelope, basefreq);
    NoteGlobalPar.FreqLfo = new LFO(partparams->GlobalPar.FreqLfo, basefreq);
    NoteGlobalPar.AmpEnvelope = new Envelope(partparams->GlobalPar.AmpEnvelope, basefreq);
    NoteGlobalPar.AmpLfo = new LFO(partparams->GlobalPar.AmpLfo, basefreq);
    NoteGlobalPar.Volume = 4.0f * powf(0.1f, 3.0f * (1.0f - partparams->GlobalPar.PVolume / 96.0f))  //-60 dB .. 0 dB
            * velF(velocity, partparams->GlobalPar.PAmpVelocityScaleFunction); // velocity sensing

    NoteGlobalPar.AmpEnvelope->envout_dB(); // discard the first envelope output
    globalnewamplitude = NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB() * NoteGlobalPar.AmpLfo->amplfoout();

    NoteGlobalPar.GlobalFilterL = new Filter(partparams->GlobalPar.GlobalFilter);
    if (stereo != 0)
        NoteGlobalPar.GlobalFilterR = new Filter(partparams->GlobalPar.GlobalFilter);
    NoteGlobalPar.FilterEnvelope = new Envelope(partparams->GlobalPar.FilterEnvelope, basefreq);
    NoteGlobalPar.FilterLfo = new LFO(partparams->GlobalPar.FilterLfo, basefreq);
    NoteGlobalPar.FilterQ = partparams->GlobalPar.GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking = partparams->GlobalPar.GlobalFilter->getfreqtracking(basefreq);

    // Forbids the Modulation Voice to be greater or equal than voice
    for (i = 0; i < NUM_VOICES; ++i)
        if (NoteVoicePar[i].FMVoice >= i)
            NoteVoicePar[i].FMVoice = -1;

    // Voice Parameter init
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;

        NoteVoicePar[nvoice].noisetype = partparams->VoicePar[nvoice].Type;
        /* Voice Amplitude Parameters Init */
        NoteVoicePar[nvoice].Volume = powf(0.1f, 3.0f * (1.0f - partparams->VoicePar[nvoice].PVolume / 127.0f)) // -60 dB .. 0 dB
                * velF(velocity, partparams->VoicePar[nvoice].PAmpVelocityScaleFunction);                       //velocity

        if (partparams->VoicePar[nvoice].PVolumeminus != 0)
            NoteVoicePar[nvoice].Volume = -NoteVoicePar[nvoice].Volume;

        if (partparams->VoicePar[nvoice].PPanning == 0)
            NoteVoicePar[nvoice].Panning = synth->numRandom(); // random panning
        else
            NoteVoicePar[nvoice].Panning =
                partparams->VoicePar[nvoice].PPanning / 128.0f;

        newamplitude[nvoice] = 1.0;
        if (partparams->VoicePar[nvoice].PAmpEnvelopeEnabled)
        {
            NoteVoicePar[nvoice].AmpEnvelope =
                new Envelope(partparams->VoicePar[nvoice].AmpEnvelope, basefreq);
            NoteVoicePar[nvoice].AmpEnvelope->envout_dB(); // discard the first envelope sample
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();
        }

        if (partparams->VoicePar[nvoice].PAmpLfoEnabled)
        {
            NoteVoicePar[nvoice].AmpLfo =
                new LFO(partparams->VoicePar[nvoice].AmpLfo, basefreq);
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();
        }

        // Voice Frequency Parameters Init
        if (partparams->VoicePar[nvoice].PFreqEnvelopeEnabled)
            NoteVoicePar[nvoice].FreqEnvelope =
                new Envelope(partparams->VoicePar[nvoice].FreqEnvelope, basefreq);

        if (partparams->VoicePar[nvoice].PFreqLfoEnabled)
            NoteVoicePar[nvoice].FreqLfo =
                new LFO(partparams->VoicePar[nvoice].FreqLfo, basefreq);

        // Voice Filter Parameters Init
        if (partparams->VoicePar[nvoice].PFilterEnabled)
        {
            NoteVoicePar[nvoice].VoiceFilterL =
                new Filter(partparams->VoicePar[nvoice].VoiceFilter);
            NoteVoicePar[nvoice].VoiceFilterR =
                new Filter(partparams->VoicePar[nvoice].VoiceFilter);
        }

        if (partparams->VoicePar[nvoice].PFilterEnvelopeEnabled)
            NoteVoicePar[nvoice].FilterEnvelope =
                new Envelope(partparams->VoicePar[nvoice].FilterEnvelope,
                             basefreq);

        if (partparams->VoicePar[nvoice].PFilterLfoEnabled)
            NoteVoicePar[nvoice].FilterLfo =
                new LFO(partparams->VoicePar[nvoice].FilterLfo, basefreq);

        NoteVoicePar[nvoice].FilterFreqTracking =
            partparams->VoicePar[nvoice].VoiceFilter->getfreqtracking(basefreq);

        // Voice Modulation Parameters Init
        if (NoteVoicePar[nvoice].FMEnabled != NONE
           && NoteVoicePar[nvoice].FMVoice < 0)
        {
            partparams->VoicePar[nvoice].FMSmp->newrandseed();
            NoteVoicePar[nvoice].FMSmp =
                new float[synth->oscilsize + OSCIL_SMP_EXTRA_SAMPLES];

            // Perform Anti-aliasing only on MORPH or RING MODULATION

            int vc = nvoice;
            if (partparams->VoicePar[nvoice].PextFMoscil != -1)
                vc = partparams->VoicePar[nvoice].PextFMoscil;

            float tmp = 1.0;
            if (partparams->VoicePar[vc].FMSmp->Padaptiveharmonics != 0
               || NoteVoicePar[nvoice].FMEnabled == MORPH
               || NoteVoicePar[nvoice].FMEnabled == RING_MOD)
                tmp = getFMVoiceBaseFreq(nvoice);

            if (!partparams->GlobalPar.Hrandgrouping)
                partparams->VoicePar[vc].FMSmp->newrandseed();

            for (int k = 0; k < unison_size[nvoice]; ++k)
                oscposhiFM[nvoice][k] =
                    (oscposhi[nvoice][k] + partparams->VoicePar[vc].FMSmp->
                        get(NoteVoicePar[nvoice].FMSmp, tmp)) % synth->oscilsize;

            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].FMSmp[synth->oscilsize + i] =
                    NoteVoicePar[nvoice].FMSmp[i];
            int oscposhiFM_add =
                lrintf((partparams->VoicePar[nvoice].PFMoscilphase - 64.0f) / 128.0f * synth->oscilsize_f + synth->oscilsize_f * 4.0f);
            for (int k = 0; k < unison_size[nvoice]; ++k)
            {
                oscposhiFM[nvoice][k] += oscposhiFM_add;
                oscposhiFM[nvoice][k] %= synth->oscilsize;
            }
        }

        if (partparams->VoicePar[nvoice].PFMFreqEnvelopeEnabled)
            NoteVoicePar[nvoice].FMFreqEnvelope =
                new Envelope(partparams->VoicePar[nvoice].FMFreqEnvelope,
                             basefreq);

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume
                                 * ctl->fmamp.relamp;

        if (partparams->VoicePar[nvoice].PFMAmpEnvelopeEnabled)
        {
            NoteVoicePar[nvoice].FMAmpEnvelope =
                new Envelope(partparams->VoicePar[nvoice].FMAmpEnvelope,
                             basefreq);
            FMnewamplitude[nvoice] *=
                NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
        }
    }

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            tmp[i] = 0;
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            if (NoteVoicePar[i].FMVoice == nvoice && tmp[i] == 0)
            {
                NoteVoicePar[nvoice].VoiceOut = new float[synth->buffersize];
                tmp[i] = 1;
            }

        if (NoteVoicePar[nvoice].VoiceOut != NULL)
            memset(NoteVoicePar[nvoice].VoiceOut, 0, synth->buffersize * sizeof(float));
    }
}


// Get Voice's Modullator base frequency
float ADnote::getFMVoiceBaseFreq(int nvoice) const
{
    //float detune = NoteVoicePar[nvoice].FMDetune / 100.0f;
    return getVoiceBaseFreq(nvoice) * powf(2.0f, (NoteVoicePar[nvoice].FMDetune / 100.0f) / 12.0f);
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
        if (pos >= 1.0f)
        {
            pos  = 1.0f;
            step = -step;
        }
        float vibratto_val = (pos - 0.333333333f * pos * pos * pos) * 1.5f; // make the vibratto lfo smoother
        unison_freq_rap[nvoice][k] = 1.0f + ((unison_base_freq_rap[nvoice][k] - 1.0f) + vibratto_val * unison_vibratto[nvoice].amplitude) * relbw;

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
        if (speed > synth->oscilsize_f)
            speed = synth->oscilsize_f;

        // F2I(speed, oscfreqhi[nvoice][k]);
        oscfreqhi[nvoice][k] = float2int(speed);
        oscfreqlo[nvoice][k] = speed - floorf(speed);
    }
}

// Computes the frequency of an modullator oscillator
void ADnote::setfreqFM(int nvoice, float in_freq)
{
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        float freq = fabsf(in_freq) * unison_freq_rap[nvoice][k];
        float speed = freq * synth->oscilsize_f / synth->samplerate_f;
        if (speed > synth->oscilsize_f)
            speed = synth->oscilsize_f;

        //F2I(speed, oscfreqhiFM[nvoice][k]);
        oscfreqhiFM[nvoice][k] = float2int(speed);
        oscfreqloFM[nvoice][k] = speed - floorf(speed);
    }
}


// Get Voice base frequency
float ADnote::getVoiceBaseFreq(int nvoice) const
{
    float detune =
        NoteVoicePar[nvoice].Detune / 100.0f + NoteVoicePar[nvoice].FineDetune / 100.0f * ctl->bandwidth.relbw * bandwidthDetuneMultiplier + NoteGlobalPar.Detune / 100.0f;

    if (NoteVoicePar[nvoice].fixedfreq == 0)
        return basefreq * powf(2.0f, detune / 12.0f);
    else // the fixed freq is enabled
    {
        float fixedfreq = 440.0f;
        int fixedfreqET = NoteVoicePar[nvoice].fixedfreqET;
        if (fixedfreqET != 0)
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
    int nvoice;
    float voicefreq, voicepitch, filterpitch, filterfreq, FMfreq,
             FMrelativepitch, globalpitch, globalfilterpitch;
    globalpitch = 0.01f * (NoteGlobalPar.FreqEnvelope->envout() + NoteGlobalPar.FreqLfo->lfoout() * ctl->modwheel.relmod);
    globaloldamplitude = globalnewamplitude;
    globalnewamplitude = NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB() * NoteGlobalPar.AmpLfo->amplfoout();

    globalfilterpitch = NoteGlobalPar.FilterEnvelope->envout() + NoteGlobalPar.FilterLfo->lfoout() + NoteGlobalPar.FilterCenterPitch;

    float tmpfilterfreq = globalfilterpitch + ctl->filtercutoff.relfreq + NoteGlobalPar.FilterFreqTracking;

    tmpfilterfreq = NoteGlobalPar.GlobalFilterL->getrealfreq(tmpfilterfreq);

    float globalfilterq = NoteGlobalPar.FilterQ * ctl->filterq.relq;
    NoteGlobalPar.GlobalFilterL->setfreq_and_q(tmpfilterfreq, globalfilterq);
    if (stereo != 0)
        NoteGlobalPar.GlobalFilterR->setfreq_and_q(tmpfilterfreq, globalfilterq);

    // compute the portamento, if it is used by this note
    float portamentofreqrap = 1.0;
    if (portamento != 0) // this voice use portamento
    {
        portamentofreqrap = ctl->portamento.freqrap;
        if (ctl->portamento.used == 0) //the portamento has finished
            portamento = 0; //this note is no longer "portamented"

    }

    // compute parameters for all voices
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;
        NoteVoicePar[nvoice].DelayTicks -= 1;
        if (NoteVoicePar[nvoice].DelayTicks > 0)
            continue;

        computeUnisonFreqRap(nvoice);

        // Voice Amplitude
        oldamplitude[nvoice] = newamplitude[nvoice];
        newamplitude[nvoice] = 1.0;

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
            filterfreq = NoteVoicePar[nvoice].VoiceFilterL->getrealfreq(
                filterfreq);
            NoteVoicePar[nvoice].VoiceFilterL->setfreq(filterfreq);
            if (stereo && NoteVoicePar[nvoice].VoiceFilterR)
                NoteVoicePar[nvoice].VoiceFilterR->setfreq(filterfreq);
        }
        if (NoteVoicePar[nvoice].noisetype == 0) // compute only if the voice isn't noise
        {
            // Voice Frequency
            voicepitch = 0.0f;
            if (NoteVoicePar[nvoice].FreqLfo != NULL)
                voicepitch += NoteVoicePar[nvoice].FreqLfo->lfoout() / 100.0f * ctl->bandwidth.relbw;

            if (NoteVoicePar[nvoice].FreqEnvelope != NULL)
                voicepitch += NoteVoicePar[nvoice].FreqEnvelope->envout()
                              / 100.0f;
            voicefreq = getVoiceBaseFreq(nvoice)
                        * powf(2.0f, (voicepitch + globalpitch) / 12.0f); // Hz frequency
            voicefreq *= ctl->pitchwheel.relfreq; //change the frequency by the controller
            setfreq(nvoice, voicefreq * portamentofreqrap);

            // Modulator
            if (NoteVoicePar[nvoice].FMEnabled != NONE)
            {
                FMrelativepitch = NoteVoicePar[nvoice].FMDetune / 100.0f;
                if (NoteVoicePar[nvoice].FMFreqEnvelope != NULL)
                    FMrelativepitch +=
                        NoteVoicePar[nvoice].FMFreqEnvelope->envout() / 100.0f;
                FMfreq = powf(2.0f, FMrelativepitch / 12.0f) * voicefreq * portamentofreqrap;
                setfreqFM(nvoice, FMfreq);

                FMoldamplitude[nvoice] = FMnewamplitude[nvoice];
                FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;
                if (NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
                    FMnewamplitude[nvoice] *= NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
            }
        }
    }
    time += synth->buffersize_f / synth->samplerate_f;
}

// Fadein in a way that removes clicks but keep sound "punchy"
void ADnote::fadein(float *smps) const
{
    int zerocrossings = 0;
    for (int i = 1; i < synth->buffersize; ++i)
        if (smps[i - 1] < 0.0f && smps[i] > 0.0f)
            zerocrossings++; // this is only the possitive crossings

    float tmp = (synth->buffersize - 1.0f) / (zerocrossings + 1) / 3.0f;
    if (tmp < 8.0f)
        tmp = 8.0f;

    // F2I(tmp, n); // how many samples is the fade-in
    int fadein = float2int((tmp < 8.0f) ? 8.0f : tmp); // how many samples is the fade-in
    if (fadein > synth->buffersize)
        fadein = synth->buffersize;
    for (int i = 0; i < fadein; ++i) // fade-in
    {
        //float tmp = 0.5f - cosf((float)i / (float) n * PI) * 0.5f;
        smps[i] *= (0.5f - cosf(PI * i / fadein) * 0.5f);
    }
}

// Computes the Oscillator (Without Modulation) - LinearInterpolation
void ADnote::computeVoiceOscillator_LinearInterpolation(int nvoice)
{
    int i, poshi;
    float poslo;

    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        poshi = oscposhi[nvoice][k];
        poslo = oscposlo[nvoice][k];
        int freqhi = oscfreqhi[nvoice][k];
        float freqlo = oscfreqlo[nvoice][k];
        float *smps = NoteVoicePar[nvoice].OscilSmp;
        //float *tw = tmpwave_unison[k];
        for (i = 0; i < synth->buffersize; ++i)
        {
            tmpwave_unison[k][i] = smps[poshi] * (1.0f - poslo) + smps[poshi + 1] * poslo;
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


// Computes the Oscillator (Morphing)
void ADnote::computeVoiceOscillatorMorph(int nvoice)
{
    int i;
    float amp;
    computeVoiceOscillator_LinearInterpolation(nvoice);
    if (FMnewamplitude[nvoice] > 1.0f)
        FMnewamplitude[nvoice] = 1.0f;
    if (FMoldamplitude[nvoice] > 1.0f)
        FMoldamplitude[nvoice] = 1.0f;

    if (NoteVoicePar[nvoice].FMVoice >= 0)
    {
        //if I use VoiceOut[] as modullator
        int FMVoice = NoteVoicePar[nvoice].FMVoice;
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            //float *tw = tmpwave_unison[k];
            for (i = 0; i < synth->buffersize; ++i)
            {
                amp = interpolateAmplitude(FMoldamplitude[nvoice], FMnewamplitude[nvoice], i, synth->buffersize);
                tmpwave_unison[k][i] *= (1.0f - amp) + amp * NoteVoicePar[FMVoice].VoiceOut[i];
            }
        }
    }
    else {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            int poshiFM = oscposhiFM[nvoice][k];
            float posloFM  = oscposloFM[nvoice][k];
            int freqhiFM = oscfreqhiFM[nvoice][k];
            float freqloFM = oscfreqloFM[nvoice][k];
            //float *tw = tmpwave_unison[k];

            for (i = 0; i < synth->buffersize; ++i)
            {
                amp = interpolateAmplitude(FMoldamplitude[nvoice], FMnewamplitude[nvoice], i, synth->buffersize);
                tmpwave_unison[k][i] *= (1.0f - amp) + amp * (NoteVoicePar[nvoice].FMSmp[poshiFM] * (1 - posloFM) + NoteVoicePar[nvoice].FMSmp[poshiFM + 1] * posloFM);
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
}


// Computes the Oscillator (Ring Modulation)
void ADnote::computeVoiceOscillatorRingModulation(int nvoice)
{
    int      i;
    float amp;
    computeVoiceOscillator_LinearInterpolation(nvoice);
    if (FMnewamplitude[nvoice] > 1.0f)
        FMnewamplitude[nvoice] = 1.0f;
    if (FMoldamplitude[nvoice] > 1.0f)
        FMoldamplitude[nvoice] = 1.0f;
    if (NoteVoicePar[nvoice].FMVoice >= 0)
    {
        // if I use VoiceOut[] as modullator
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            //float *tw = tmpwave_unison[k];
            for (i = 0; i < synth->buffersize; ++i)
            {
                amp = interpolateAmplitude(FMoldamplitude[nvoice],
                                           FMnewamplitude[nvoice],
                                           i, synth->buffersize);
                int FMVoice = NoteVoicePar[nvoice].FMVoice;
                for (i = 0; i < synth->buffersize; ++i)
                    tmpwave_unison[k][i] *= (1.0f - amp) + amp * NoteVoicePar[FMVoice].VoiceOut[i];
            }
        }
    }
    else {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            int poshiFM = oscposhiFM[nvoice][k];
            float  posloFM  = oscposloFM[nvoice][k];
            int freqhiFM = oscfreqhiFM[nvoice][k];
            float freqloFM = oscfreqloFM[nvoice][k];
            //float *tw = tmpwave_unison[k];

            for (i = 0; i < synth->buffersize; ++i)
            {
                amp = interpolateAmplitude(FMoldamplitude[nvoice], FMnewamplitude[nvoice], i, synth->buffersize);
                tmpwave_unison[k][i] *= (NoteVoicePar[nvoice].FMSmp[poshiFM] * (1.0 - posloFM) + NoteVoicePar[nvoice].FMSmp[poshiFM + 1] * posloFM) * amp + (1.0 - amp);
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
}


// Computes the Oscillator (Phase Modulation or Frequency Modulation)
void ADnote::computeVoiceOscillatorFrequencyModulation(int nvoice, int FMmode)
{
    int carposhi = 0;
    int i, FMmodfreqhi = 0;
    float FMmodfreqlo = 0;
    float carposlo = 0;

    if (NoteVoicePar[nvoice].FMVoice >= 0)
    {
        //if I use VoiceOut[] as modulator
        for (int k = 0; k < unison_size[nvoice]; ++k)
            memcpy(tmpwave_unison[k], NoteVoicePar[NoteVoicePar[nvoice].FMVoice].VoiceOut, synth->bufferbytes);
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

            for (i = 0; i < synth->buffersize; ++i)
            {
                tw[i] = (NoteVoicePar[nvoice].FMSmp[poshiFM] * (1.0f - posloFM) + NoteVoicePar[nvoice].FMSmp[poshiFM + 1] * posloFM);
                posloFM += freqloFM;
                if (posloFM >= 1.0f)
                {
                    posloFM = fmodf(posloFM, 1.0f);
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
            for (i = 0; i < synth->buffersize; ++i)
                tw[i] *= interpolateAmplitude(FMoldamplitude[nvoice], FMnewamplitude[nvoice], i, synth->buffersize);
        }
    }
    else
    {
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            for (i = 0; i < synth->buffersize; ++i)
                tw[i] *= FMnewamplitude[nvoice];
        }
    }


    //normalize: makes all sample-rates, oscil_sizes to produce same sound
    if (FMmode != 0) // Frequency modulation
    {
        float normalize = synth->oscilsize_f / 262144.0f * 44100.0f / synth->samplerate_f;
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            float  fmold = FMoldsmp[nvoice][k];
            for (i = 0; i < synth->buffersize; ++i)
            {
                fmold = fmodf(fmold + tw[i] * normalize, synth->oscilsize_f);
                tw[i] = fmold;
            }
            FMoldsmp[nvoice][k] = fmold;
        }
    }
    else  // Phase modulation
    {
        float normalize = synth->oscilsize / 262144.0f;
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            float *tw = tmpwave_unison[k];
            for (i = 0; i < synth->buffersize; ++i)
                tw[i] *= normalize;
        }
    }

    // do the modulation
    for (int k = 0; k < unison_size[nvoice]; ++k)
    {
        int poshi = oscposhi[nvoice][k];
        float poslo = oscposlo[nvoice][k];
        int freqhi = oscfreqhi[nvoice][k];
        float freqlo = oscfreqlo[nvoice][k];

        for (i = 0; i < synth->buffersize; ++i)
        {
            // F2I(tw[i], FMmodfreqhi);
            FMmodfreqhi = float2int(tmpwave_unison[k][i]);
            FMmodfreqlo = fmodf(tmpwave_unison[k][i] + 0.0000000001f, 1.0f);
            if (FMmodfreqhi < 0)
                FMmodfreqlo++;

            //carrier
            carposhi = poshi + FMmodfreqhi;
            carposlo = poslo + FMmodfreqlo;

            if (carposlo >= 1.0f)
            {
                carposhi++;
                carposlo = fmodf(carposlo, 1.0f);
            }
            carposhi &= (synth->oscilsize - 1);

            tmpwave_unison[k][i] = NoteVoicePar[nvoice].OscilSmp[carposhi] * (1.0f - carposlo) + NoteVoicePar[nvoice].OscilSmp[carposhi + 1] * carposlo;
            poslo += freqlo;
            if (poslo >= 1.0f)
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


// Calculeaza Oscilatorul cu PITCH MODULATION
void ADnote::computeVoiceOscillatorPitchModulation(int nvoice)
{
//TODO
}

// Computes the Noise
void ADnote::computeVoiceNoise(int nvoice)
{
    for (int k = 0; k < unison_size[nvoice]; ++k) {
        for (int i = 0; i < synth->buffersize; ++i)
            tmpwave_unison[k][i] = synth->numRandom() * 2.0f - 1.0f;
    }
}


// Compute the ADnote samples, returns 0 if the note is finished
int ADnote::noteout(float *outl, float *outr)
{
    int i, nvoice;

    memset(outl, 0, synth->buffersize * sizeof(float));
    memset(outr, 0, synth->buffersize * sizeof(float));

    if (!NoteEnabled)
        return 0;

    memset(bypassl, 0, synth->buffersize * sizeof(float));
    memset(bypassr, 0, synth->buffersize * sizeof(float));
    computeCurrentParameters();

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled || NoteVoicePar[nvoice].DelayTicks > 0)
            continue;
        if (NoteVoicePar[nvoice].noisetype == 0) //voice mode=sound
            switch(NoteVoicePar[nvoice].FMEnabled)
        {
            case MORPH:
                computeVoiceOscillatorMorph(nvoice);
                break;
            case RING_MOD:
                computeVoiceOscillatorRingModulation(nvoice);
                break;
            case PHASE_MOD:
                computeVoiceOscillatorFrequencyModulation(nvoice, 0);
                break;
            case FREQ_MOD:
                computeVoiceOscillatorFrequencyModulation(nvoice, 1);
                break;
            //case PITCH_MOD:computeVoiceOscillatorPitchModulation(nvoice);break;
            default:
                computeVoiceOscillator_LinearInterpolation(nvoice);
                //if (config.cfg.Interpolation) ComputeVoiceOscillator_CubicInterpolation(nvoice);
            }
        else
            computeVoiceNoise(nvoice);
        // Voice Processing


        //mix subvoices into voice
        memset(tmpwavel, 0, synth->buffersize * sizeof(float));
        if (stereo)
            memset(tmpwaver, 0, synth->buffersize * sizeof(float));
        for (int k = 0; k < unison_size[nvoice]; ++k)
        {
            //float *tw = tmpwave_unison[k];
            if (stereo)
            {
                float stereo_pos    = 0;
                if (unison_size[nvoice] > 1)
                    stereo_pos = k / (float)(unison_size[nvoice] - 1) * 2.0f - 1.0f;
                float stereo_spread = unison_stereo_spread[nvoice] * 2.0f; //between 0 and 2.0
                if (stereo_spread > 1.0f)
                {
                    float stereo_pos_1 = (stereo_pos >= 0.0f) ? 1.0f : -1.0f;
                    stereo_pos = (2.0f - stereo_spread) * stereo_pos
                                  + (stereo_spread - 1.0f) * stereo_pos_1;
                }
                else
                    stereo_pos *= stereo_spread;

                if (unison_size[nvoice] == 1)
                    stereo_pos = 0.0f;
                float panning = (stereo_pos + 1.0f) * 0.5f;

                float lvol = (1.0f - panning) * 2.0f;
                if (lvol > 1.0f)
                    lvol = 1.0f;

                float rvol = panning * 2.0f;
                if (rvol > 1.0f)
                    rvol = 1.0f;

                if (unison_invert_phase[nvoice][k])
                {
                    lvol = -lvol;
                    rvol = -rvol;
                }

                for (i = 0; i < synth->buffersize; ++i)
                    tmpwavel[i] += tmpwave_unison[k][i] * lvol;
                for (i = 0; i < synth->buffersize; ++i)
                    tmpwaver[i] += tmpwave_unison[k][i] * rvol;
            }
            else
                for (i = 0; i < synth->buffersize; ++i)
                    tmpwavel[i] += tmpwave_unison[k][i];
        }

        float unison_amplitude = 1.0f / sqrtf(unison_size[nvoice]); //reduce the amplitude for large unison sizes
        // Amplitude
        float oldam = oldamplitude[nvoice] * unison_amplitude;
        float newam = newamplitude[nvoice] * unison_amplitude;

        if (aboveAmplitudeThreshold(oldam, newam))
        {
            int rest = synth->buffersize;
            //test if the amplitude if raising and the difference is high
            if (newam > oldam && (newam - oldam) > 0.25f)
            {
                rest = 10;
                if (rest > synth->buffersize)
                    rest = synth->buffersize;
                for (int i = 0; i < synth->buffersize - rest; ++i)
                    tmpwavel[i] *= oldam;
                if (stereo)
                    for (int i = 0; i < synth->buffersize - rest; ++i)
                        tmpwaver[i] *= oldam;
            }
            // Amplitude interpolation
            for (i = 0; i < rest; ++i)
            {
                float amp = interpolateAmplitude(oldam, newam, i, rest);
                tmpwavel[i + (synth->buffersize - rest)] *= amp;
                if (stereo)
                    tmpwaver[i + (synth->buffersize - rest)] *= amp;
            }
        }
        else
        {
            for (i = 0; i < synth->buffersize; ++i)
                tmpwavel[i] *= newam;
            if (stereo)
                for (i = 0; i < synth->buffersize; ++i)
                    tmpwaver[i] *= newam;
        }

        // Fade in
        if (firsttick[nvoice] != 0)
        {
            fadein(&tmpwavel[0]);
            if (stereo)
                fadein(&tmpwaver[0]);
            firsttick[nvoice] = 0;
        }


        // Filter
        if (NoteVoicePar[nvoice].VoiceFilterL != NULL)
            NoteVoicePar[nvoice].VoiceFilterL->filterout(&tmpwavel[0]);
        if (stereo && NoteVoicePar[nvoice].VoiceFilterR != NULL)
            NoteVoicePar[nvoice].VoiceFilterR->filterout(&tmpwaver[0]);

        //check if the amplitude envelope is finished, if yes, the voice will be fadeout
        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
        {
            if (NoteVoicePar[nvoice].AmpEnvelope->finished() != 0)
            {
                for (i = 0; i < synth->buffersize; ++i)
                    tmpwavel[i] *= 1.0f - (float)i / synth->buffersize_f;
                if (stereo)
                    for (i = 0; i < synth->buffersize; ++i)
                        tmpwaver[i] *= 1.0f - (float)i / synth->buffersize_f;
            }
            //the voice is killed later
        }


        // Put the ADnote samples in VoiceOut (without appling Global volume, because I wish to use this voice as a modullator)
        if (NoteVoicePar[nvoice].VoiceOut != NULL)
        {
            if (stereo)
                for (i = 0; i < synth->buffersize; ++i)
                    NoteVoicePar[nvoice].VoiceOut[i] = tmpwavel[i] + tmpwaver[i];
            else // mono
                for (i = 0; i < synth->buffersize; ++i)
                    NoteVoicePar[nvoice].VoiceOut[i] = tmpwavel[i];
        }


        // Add the voice that do not bypass the filter to out
        if (NoteVoicePar[nvoice].filterbypass == 0) // no bypass
        {
            if (stereo)
            {
                for (i = 0; i < synth->buffersize; ++i) // stereo
                {
                    outl[i] += tmpwavel[i] * NoteVoicePar[nvoice].Volume
                               * NoteVoicePar[nvoice].Panning * 2.0f;
                    outr[i] += tmpwaver[i] * NoteVoicePar[nvoice].Volume
                               * (1.0f - NoteVoicePar[nvoice].Panning) * 2.0f;
                }
            }
            else
                for (i = 0; i < synth->buffersize; ++i)
                    outl[i] += tmpwavel[i] * NoteVoicePar[nvoice].Volume; // mono
        }
        else // bypass the filter
        {
            if (stereo)
            {
                for (i = 0; i < synth->buffersize; ++i) // stereo
                {
                    bypassl[i] += tmpwavel[i] * NoteVoicePar[nvoice].Volume
                                  * NoteVoicePar[nvoice].Panning * 2.0f;
                    bypassr[i] += tmpwaver[i] * NoteVoicePar[nvoice].Volume
                                  * (1.0f - NoteVoicePar[nvoice].Panning) * 2.0f;
                }
            }
            else
                for (i = 0; i < synth->buffersize; ++i)
                    bypassl[i] += tmpwavel[i] * NoteVoicePar[nvoice].Volume; // mono
        }
        // chech if there is necesary to proces the voice longer (if the Amplitude envelope isn't finished)
        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
            if (NoteVoicePar[nvoice].AmpEnvelope->finished() != 0)
                killVoice(nvoice);
    }

    // Processing Global parameters
    NoteGlobalPar.GlobalFilterL->filterout(&outl[0]);

    if (stereo == 0) // set the right channel=left channel
    {
        memcpy(outr, outl, synth->buffersize * sizeof(float));
        memcpy(bypassr, bypassl, synth->buffersize * sizeof(float));
    }
    else
        NoteGlobalPar.GlobalFilterR->filterout(&outr[0]);

    for (i = 0; i < synth->buffersize; ++i)
    {
        outl[i] += bypassl[i];
        outr[i] += bypassr[i];
    }

    if (aboveAmplitudeThreshold(globaloldamplitude, globalnewamplitude))
    {
        // Amplitude Interpolation
        for (i = 0; i < synth->buffersize; ++i)
        {
            float tmpvol = interpolateAmplitude(globaloldamplitude,
                                                globalnewamplitude, i, synth->buffersize);
            outl[i] *= tmpvol * NoteGlobalPar.Panning;
            outr[i] *= tmpvol * (1.0f - NoteGlobalPar.Panning);
        }
    }
    else
    {
        for (i = 0; i < synth->buffersize; ++i)
        {
            outl[i] *= globalnewamplitude * NoteGlobalPar.Panning;
            outr[i] *= globalnewamplitude * (1.0 - NoteGlobalPar.Panning);
        }
    }

    // Apply the punch
    if (NoteGlobalPar.Punch.Enabled)
    {
        for (i = 0; i < synth->buffersize; ++i)
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
            memset(outl, 0, synth->buffersize * sizeof(float));
            memset(outr, 0, synth->buffersize * sizeof(float));
        }
    switch(Legato.msg) {
    case LM_CatchUp:  // Continue the catch-up...
        if (Legato.decounter == -10)
            Legato.decounter = Legato.fade.length;
        for (i = 0; i < synth->buffersize; ++i) { //Yea, could be done without the loop...
            Legato.decounter--;
            if (Legato.decounter < 1) {
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
        for (i = 0; i < synth->buffersize; ++i)
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
        for (i = 0; i < synth->buffersize; ++i)
        {
            Legato.decounter--;
            if (Legato.decounter < 1)
            {
                //for (int j = i; j < synth->buffersize; j++)
                //{
                //    outl[j] = 0.0;
                //    outr[j] = 0.0;
                //}
                memset(outl, 0, synth->buffersize * sizeof(float));
                memset(outr, 0, synth->buffersize * sizeof(float));
                Legato.decounter = -10;
                Legato.silent = true;
                // Fading-out done, now set the catch-up :
                Legato.decounter = Legato.fade.length;
                Legato.msg = LM_CatchUp;
                float catchupfreq =
                    Legato.param.freq * (Legato.param.freq / Legato.lastfreq); // This freq should make this now silent note to catch-up (or should I say resync ?) with the heard note for the same length it stayed at the previous freq during the fadeout.
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
    if (NoteGlobalPar.AmpEnvelope->finished() != 0)
    {
        for (i = 0; i < synth->buffersize; ++i) // fade-out
        {
            float tmp = 1.0f - (float)i / synth->buffersize_f;
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
        if (NoteVoicePar[nvoice].Enabled == 0)
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

// Check if the note is finished
int ADnote::finished() const
{
    if (NoteEnabled)
        return 0;
    else
        return 1;
}
