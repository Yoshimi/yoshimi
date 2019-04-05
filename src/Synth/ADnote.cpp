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

    This file is a derivative of the ZynAddSubFX original, modified January 2010
*/

#include <iostream>
#include <cmath>

using namespace std;

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Synth/ADnote.h"


ADnote::ADnote(ADnoteParameters *pars_, Controller *ctl_, float freq_,
               float velocity_, int portamento_, int midinote_,
               bool besilent) :
    ready(0),
    adnotepars(pars_),
    stereo(adnotepars->GlobalPar.PStereo),
    midinote(midinote_),
    velocity(velocity_),
    basefreq(freq_),
    NoteEnabled(true),
    ctl(ctl_),
    time(0.0f),
    portamento(portamento_),
    samplerate(adnotepars->getSamplerate()),
    buffersize(adnotepars->getBuffersize()),
    oscilsize(adnotepars->getOscilsize())
{
    NoteGlobalPar.FreqEnvelope = NULL;
    NoteGlobalPar.AmpEnvelope = NULL;
    NoteGlobalPar.FreqLfo = NULL;
    NoteGlobalPar.AmpLfo = NULL;
    //NoteGlobalPar.GlobalFilterL = NULL;
    //NoteGlobalPar.GlobalFilterR = NULL;
    NoteGlobalPar.FilterEnvelope = NULL;
    NoteGlobalPar.FilterLfo = NULL;

    tmpwave = (float*)adnotepars->buffPool->malloc();
    bypassl = (float*)adnotepars->buffPool->malloc();
    bypassr = (float*)adnotepars->buffPool->malloc();

    if (velocity > 1.0f)
        velocity = 1.0f;

    // Initialise some legato-specific vars
    Legato.msg = LM_Norm;
    Legato.fade.length = (int)(samplerate * 0.005f); // 0.005 seems ok.
    if (Legato.fade.length < 1)
        Legato.fade.length = 1; // (if something's fishy)
    Legato.fade.step = (1.0f / Legato.fade.length);
    Legato.decounter = -10;
    Legato.param.freq = basefreq;
    Legato.param.vel = velocity;
    Legato.param.portamento = portamento;
    Legato.param.midinote = midinote;
    Legato.silent = besilent;

    NoteGlobalPar.Detune = getdetune(adnotepars->GlobalPar.PDetuneType,
                                     adnotepars->GlobalPar.PCoarseDetune,
                                     adnotepars->GlobalPar.PDetune);
    bandwidthDetuneMultiplier = adnotepars->getBandwidthDetuneMultiplier();

    if (adnotepars->GlobalPar.PPanning == 0)
        NoteGlobalPar.Panning = zynMaster->numRandom();
    else
        NoteGlobalPar.Panning = adnotepars->GlobalPar.PPanning / 128.0f;

    NoteGlobalPar.FilterCenterPitch =
        adnotepars->GlobalPar.GlobalFilter->getfreq() // center freq
        + adnotepars->GlobalPar.PFilterVelocityScale / 127.0f * 6.0f // velocity sensing
        * (VelF(velocity, adnotepars->GlobalPar.PFilterVelocityScaleFunction) - 1);

    if (adnotepars->GlobalPar.PPunchStrength != 0)
    {
        NoteGlobalPar.Punch.Enabled = 1;
        NoteGlobalPar.Punch.t = 1.0; // start from 1.0 and to 0.0
        NoteGlobalPar.Punch.initialvalue =
            ((powf(10.0f, 1.5f * adnotepars->GlobalPar.PPunchStrength / 127.0f) - 1.0f)
             * VelF(velocity, adnotepars->GlobalPar.PPunchVelocitySensing));
        float time = // 0.1 .. 100 ms
            powf(10.0f, 3.0f * adnotepars->GlobalPar.PPunchTime / 127.0f) / 10000.0f;
        float stretch =
            powf(440.0f / basefreq, adnotepars->GlobalPar.PPunchStretch / 64.0f);
        NoteGlobalPar.Punch.dt = 1.0 / (time * samplerate * stretch);
    }
    else
        NoteGlobalPar.Punch.Enabled = 0;

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        NoteVoicePar[nvoice].OscilSmp = NULL;
        NoteVoicePar[nvoice].FMSmp = NULL;
        NoteVoicePar[nvoice].VoiceOut = NULL;

        NoteVoicePar[nvoice].FMVoice = -1;

        if (!adnotepars->VoicePar[nvoice].Enabled)
        {
            NoteVoicePar[nvoice].Enabled = false;
            continue; // the voice is disabled
        }

        adnotepars->VoicePar[nvoice].OscilSmp->newrandseed();
        NoteVoicePar[nvoice].Enabled = true;
        NoteVoicePar[nvoice].fixedfreq = adnotepars->VoicePar[nvoice].Pfixedfreq;
        NoteVoicePar[nvoice].fixedfreqET = adnotepars->VoicePar[nvoice].PfixedfreqET;

        // use the Globalpars.detunetype if the detunetype is 0
        if (adnotepars->VoicePar[nvoice].PDetuneType != 0)
        {
            NoteVoicePar[nvoice].Detune =
                getdetune(adnotepars->VoicePar[nvoice].PDetuneType,
                          adnotepars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getdetune(adnotepars->VoicePar[nvoice].PDetuneType, 0,
                          adnotepars->VoicePar[nvoice].PDetune); // fine detune
        }
        else
        {
            NoteVoicePar[nvoice].Detune =
                getdetune(adnotepars->GlobalPar.PDetuneType,
                          adnotepars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getdetune(adnotepars->GlobalPar.PDetuneType, 0,
                          adnotepars->VoicePar[nvoice].PDetune); // fine detune
        }
        if (adnotepars->VoicePar[nvoice].PFMDetuneType != 0)
        {
            NoteVoicePar[nvoice].FMDetune =
                getdetune(adnotepars->VoicePar[nvoice].PFMDetuneType,
                          adnotepars->VoicePar[nvoice].PFMCoarseDetune,
                          adnotepars->VoicePar[nvoice].PFMDetune);
        }
        else
        {
            NoteVoicePar[nvoice].FMDetune =
                getdetune(adnotepars->GlobalPar.PDetuneType,
                          adnotepars->VoicePar[nvoice].PFMCoarseDetune,
                          adnotepars->VoicePar[nvoice].PFMDetune);
        }

        oscposhi[nvoice] = 0;
        oscposlo[nvoice] = 0.0f;
        oscposhiFM[nvoice] = 0;
        oscposloFM[nvoice] = 0.0f;
        NoteVoicePar[nvoice].OscilSmp = (float*)adnotepars->smpPool->malloc();
        // Get the voice's oscil or external's voice oscil
        int vc = nvoice;
        if (adnotepars->VoicePar[nvoice].Pextoscil != -1)
            vc = adnotepars->VoicePar[nvoice].Pextoscil;
        if (!adnotepars->GlobalPar.Hrandgrouping)
            adnotepars->VoicePar[vc].OscilSmp->newrandseed();
        oscposhi[nvoice] =
            adnotepars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                             getvoicebasefreq(nvoice),
                                             adnotepars->VoicePar[nvoice].Presonance);

        // I store the first elments to the last position for speedups
        for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
            NoteVoicePar[nvoice].OscilSmp[oscilsize + i] =
                NoteVoicePar[nvoice].OscilSmp[i];
        oscposhi[nvoice] += (int)((adnotepars->VoicePar[nvoice].Poscilphase - 64.0f)
                             / 128.0f * oscilsize + 4 * oscilsize);
        oscposhi[nvoice] %= oscilsize;
        NoteVoicePar[nvoice].FreqLfo = NULL;
        NoteVoicePar[nvoice].FreqEnvelope = NULL;
        NoteVoicePar[nvoice].AmpLfo = NULL;
        NoteVoicePar[nvoice].AmpEnvelope = NULL;
        NoteVoicePar[nvoice].FilterEnvelope = NULL;
        NoteVoicePar[nvoice].FilterLfo = NULL;
        NoteVoicePar[nvoice].VoiceFilter.reset();
        NoteVoicePar[nvoice].FilterCenterPitch = adnotepars->VoicePar[nvoice].VoiceFilter->getfreq();
        NoteVoicePar[nvoice].filterbypass = adnotepars->VoicePar[nvoice].Pfilterbypass;
        switch (adnotepars->VoicePar[nvoice].PFMEnabled)
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

        NoteVoicePar[nvoice].FMVoice = adnotepars->VoicePar[nvoice].PFMVoice;
        NoteVoicePar[nvoice].FMFreqEnvelope = NULL;
        NoteVoicePar[nvoice].FMAmpEnvelope = NULL;

        // Compute the Voice's modulator volume (incl. damping)
        float fmvoldamp = powf(440.0f / getvoicebasefreq(nvoice),
                               adnotepars->VoicePar[nvoice].PFMVolumeDamp / 64.0f - 1.0f);
        switch (NoteVoicePar[nvoice].FMEnabled)
        {
            case PHASE_MOD:
                fmvoldamp =
                    powf(440.0f / getvoicebasefreq(nvoice),
                         adnotepars->VoicePar[nvoice].PFMVolumeDamp / 64.0f);
                NoteVoicePar[nvoice].FMVolume =
                    (expf(adnotepars->VoicePar[nvoice].PFMVolume / 127.0f
                         * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;
            case FREQ_MOD:
                NoteVoicePar[nvoice].FMVolume =
                    (expf(adnotepars->VoicePar[nvoice].PFMVolume / 127.0f
                         * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;
                //    case PITCH_MOD:NoteVoicePar[nvoice].FMVolume=(adnotepars->VoicePar[nvoice].PFMVolume/127.0*8.0)*fmvoldamp;//???????????
                //	          break;
            default:
                if (fmvoldamp > 1.0f)
                    fmvoldamp = 1.0f;
                NoteVoicePar[nvoice].FMVolume =
                    adnotepars->VoicePar[nvoice].PFMVolume / 127.0f * fmvoldamp;
        }

        // Voice's modulator velocity sensing
        NoteVoicePar[nvoice].FMVolume *=
            VelF(velocity, adnotepars->VoicePar[nvoice].PFMVelocityScaleFunction);

        FMoldsmp[nvoice] = 0.0f; // this is for FM (integration)

        firsttick[nvoice] = 1;
        NoteVoicePar[nvoice].DelayTicks =
            (int)((expf(adnotepars->VoicePar[nvoice].PDelay / 127.0f * logf(50.0f))
                   - 1.0f) / buffersize / 10.0f * samplerate);
    }

    initparameters();
    ready = 1;
}


// ADlegatonote: This function is (mostly) a copy of ADnote(...) and
// initparameters() stuck together with some lines removed so that it
// only alter the already playing note (to perform legato). It is
// possible I left stuff that is not required for this.
void ADnote::ADlegatonote(float freq_, float velocity_, int portamento_,
                          int midinote_, bool externcall)
{
    //ADnoteParameters *pars = adnotepars;
    // Controller *ctl_=ctl; (an original comment!)

    basefreq = freq_;
    velocity = (velocity_ > 1.0f) ? 1.0f : velocity_;
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
            } else {
                Legato.fade.m = 1.0f;
                Legato.msg = LM_FadeOut;
                return;
            }
        }
        if (Legato.msg == LM_ToNorm)
            Legato.msg = LM_Norm;
    }

    NoteGlobalPar.Detune = getdetune(adnotepars->GlobalPar.PDetuneType,
                                     adnotepars->GlobalPar.PCoarseDetune,
                                     adnotepars->GlobalPar.PDetune);
    bandwidthDetuneMultiplier = adnotepars->getBandwidthDetuneMultiplier();

    if (adnotepars->GlobalPar.PPanning == 0)
        NoteGlobalPar.Panning = zynMaster->numRandom();
    else
        NoteGlobalPar.Panning = adnotepars->GlobalPar.PPanning / 128.0f;

    NoteGlobalPar.FilterCenterPitch =
        adnotepars->GlobalPar.GlobalFilter->getfreq() // center freq
        + adnotepars->GlobalPar.PFilterVelocityScale / 127.0f * 6.0f // velocity sensing
        * (VelF(velocity, adnotepars->GlobalPar.PFilterVelocityScaleFunction) - 1);

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue; // (gf) Stay the same as first note in legato.

        NoteVoicePar[nvoice].fixedfreq = adnotepars->VoicePar[nvoice].Pfixedfreq;
        NoteVoicePar[nvoice].fixedfreqET = adnotepars->VoicePar[nvoice].PfixedfreqET;

        // use the Globalpars.detunetype if the detunetype is 0
        if (adnotepars->VoicePar[nvoice].PDetuneType != 0)
        {
            NoteVoicePar[nvoice].Detune =
                getdetune(adnotepars->VoicePar[nvoice].PDetuneType,
                          adnotepars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getdetune(adnotepars->VoicePar[nvoice].PDetuneType, 0,
                          adnotepars->VoicePar[nvoice].PDetune); // fine detune
        }
        else
        {
            NoteVoicePar[nvoice].Detune =
                getdetune(adnotepars->GlobalPar.PDetuneType,
                          adnotepars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getdetune(adnotepars->GlobalPar.PDetuneType, 0,
                          adnotepars->VoicePar[nvoice].PDetune); // fine detune
        }
        if (adnotepars->VoicePar[nvoice].PFMDetuneType != 0)
        {
            NoteVoicePar[nvoice].FMDetune =
                getdetune(adnotepars->VoicePar[nvoice].PFMDetuneType,
                          adnotepars->VoicePar[nvoice].PFMCoarseDetune,
                          adnotepars->VoicePar[nvoice].PFMDetune);
        }
        else
        {
            NoteVoicePar[nvoice].FMDetune =
                getdetune(adnotepars->GlobalPar.PDetuneType,
                          adnotepars->VoicePar[nvoice].PFMCoarseDetune,
                          adnotepars->VoicePar[nvoice].PFMDetune);
        }

        // Get the voice's oscil or external's voice oscil
        int vc = nvoice;
        if (adnotepars->VoicePar[nvoice].Pextoscil != -1)
            vc = adnotepars->VoicePar[nvoice].Pextoscil;
        if (!adnotepars->GlobalPar.Hrandgrouping)
            adnotepars->VoicePar[vc].OscilSmp->newrandseed();

        adnotepars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                         getvoicebasefreq(nvoice),
                                                          adnotepars->VoicePar[nvoice].Presonance); // (gf)Modif of the above line.

        // I store the first elments to the last position for speedups
        for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
            NoteVoicePar[nvoice].OscilSmp[oscilsize + i] =
                NoteVoicePar[nvoice].OscilSmp[i];

        NoteVoicePar[nvoice].FilterCenterPitch =
            adnotepars->VoicePar[nvoice].VoiceFilter->getfreq();
        NoteVoicePar[nvoice].filterbypass =
            adnotepars->VoicePar[nvoice].Pfilterbypass;

        NoteVoicePar[nvoice].FMVoice = adnotepars->VoicePar[nvoice].PFMVoice;

        // Compute the Voice's modulator volume (incl. damping)
        float fmvoldamp =
            powf(440.0f / getvoicebasefreq(nvoice),
                adnotepars->VoicePar[nvoice].PFMVolumeDamp / 64.0f - 1.0f);

        switch (NoteVoicePar[nvoice].FMEnabled)
        {
            case PHASE_MOD:
                fmvoldamp =
                    powf(440.0f / getvoicebasefreq(nvoice),
                        adnotepars->VoicePar[nvoice].PFMVolumeDamp / 64.0f);
                NoteVoicePar[nvoice].FMVolume =
                    (expf(adnotepars->VoicePar[nvoice].PFMVolume / 127.0f
                         * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;
            case FREQ_MOD:
                NoteVoicePar[nvoice].FMVolume =
                    (expf(adnotepars->VoicePar[nvoice].PFMVolume / 127.0f
                         * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;
                //    case PITCH_MOD:NoteVoicePar[nvoice].FMVolume=(adnotepars->VoicePar[nvoice].PFMVolume/127.0*8.0)*fmvoldamp;//???????????
                //	          break;
            default:
                if (fmvoldamp > 1.0)
                    fmvoldamp = 1.0;
                NoteVoicePar[nvoice].FMVolume =
                    adnotepars->VoicePar[nvoice].PFMVolume / 127.0 * fmvoldamp;
        }

        // Voice's modulator velocity sensing
        NoteVoicePar[nvoice].FMVolume *=
            VelF(velocity, adnotepars->VoicePar[nvoice].PFMVelocityScaleFunction);

        NoteVoicePar[nvoice].DelayTicks =
            (int)((exp(adnotepars->VoicePar[nvoice].PDelay / 127.0f * logf(50.0f)) - 1.0f)
                   / buffersize / 10.0f * samplerate);
    }

    ///    initparameters();

    ///////////////
    // Altered content of initparameters():

    int tmp[NUM_VOICES];

    NoteGlobalPar.Volume =
        4.0f * powf(0.1f, 3.0f * (1.0f - adnotepars->GlobalPar.PVolume / 96.0f)) // -60 dB .. 0 dB
        * VelF(velocity, adnotepars->GlobalPar.PAmpVelocityScaleFunction); // velocity sensing

    globalnewamplitude = NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB()
                             * NoteGlobalPar.AmpLfo->amplfoout();
    NoteGlobalPar.FilterQ = adnotepars->GlobalPar.GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking = adnotepars->GlobalPar.GlobalFilter->getfreqtracking(basefreq);

    // Forbids the Modulation Voice to be greater or equal than voice
    for (int i = 0; i < NUM_VOICES; ++i)
        if (NoteVoicePar[i].FMVoice >= i)
            NoteVoicePar[i].FMVoice = -1;

    // Voice Parameter init
    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (NoteVoicePar[nvoice].Enabled == 0)
            continue;
        NoteVoicePar[nvoice].noisetype = adnotepars->VoicePar[nvoice].Type;
        // Voice Amplitude Parameters Init
        NoteVoicePar[nvoice].Volume =
            powf(0.1f, 3.0f * (1.0f - adnotepars->VoicePar[nvoice].PVolume / 127.0f)) // -60 dB .. 0 dB
                * VelF(velocity, adnotepars->VoicePar[nvoice].PAmpVelocityScaleFunction); // velocity
        if (adnotepars->VoicePar[nvoice].PVolumeminus)
            NoteVoicePar[nvoice].Volume = -NoteVoicePar[nvoice].Volume;

        if (!adnotepars->VoicePar[nvoice].PPanning)
            NoteVoicePar[nvoice].Panning = zynMaster->numRandom(); // random panning
        else
            NoteVoicePar[nvoice].Panning=adnotepars->VoicePar[nvoice].PPanning / 128.0f;

        newamplitude[nvoice] = 1.0f;
        if (adnotepars->VoicePar[nvoice].PAmpEnvelopeEnabled
            && NoteVoicePar[nvoice].AmpEnvelope)
        {
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();
        }

        if (adnotepars->VoicePar[nvoice].PAmpLfoEnabled
            && NoteVoicePar[nvoice].AmpLfo)
        {
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();
        }

        NoteVoicePar[nvoice].FilterFreqTracking =
            adnotepars->VoicePar[nvoice].VoiceFilter->getfreqtracking(basefreq);

        // Voice Modulation Parameters Init
        if (NoteVoicePar[nvoice].FMEnabled != NONE
            && NoteVoicePar[nvoice].FMVoice < 0)
        {
            adnotepars->VoicePar[nvoice].FMSmp->newrandseed();

            // Perform Anti-aliasing only on MORPH or RING MODULATION
            int vc = nvoice;
            if (adnotepars->VoicePar[nvoice].PextFMoscil != -1)
                vc = adnotepars->VoicePar[nvoice].PextFMoscil;

            float tmp = 1.0;
            if (adnotepars->VoicePar[vc].FMSmp->Padaptiveharmonics
                || NoteVoicePar[nvoice].FMEnabled == MORPH
                || NoteVoicePar[nvoice].FMEnabled == RING_MOD) {
                tmp = getFMvoicebasefreq(nvoice);
            }
            if (!adnotepars->GlobalPar.Hrandgrouping)
                adnotepars->VoicePar[vc].FMSmp->newrandseed();

            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].FMSmp[oscilsize + i] =
                    NoteVoicePar[nvoice].FMSmp[i];
        }

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;
        if (adnotepars->VoicePar[nvoice].PFMAmpEnvelopeEnabled
            && NoteVoicePar[nvoice].FMAmpEnvelope)
        {
            FMnewamplitude[nvoice] *= NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
        }
    }

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (int i = nvoice + 1; i < NUM_VOICES; ++i)
            tmp[i] = 0;
        for (int i = nvoice + 1; i < NUM_VOICES; ++i)
            if (NoteVoicePar[i].FMVoice == nvoice && !tmp[i])
            {
                tmp[i] = 1;
            }
    }
    ///////////////

    // End of the ADlegatonote function.
}


// Kill a voice of ADnote
void ADnote::KillVoice(int nvoice)
{
    adnotepars->smpPool->free(NoteVoicePar[nvoice].OscilSmp);
    NoteVoicePar[nvoice].OscilSmp = NULL;

    if (NoteVoicePar[nvoice].FreqEnvelope)
    {
        adnotepars->envelopePool->destroy(NoteVoicePar[nvoice].FreqEnvelope);
        NoteVoicePar[nvoice].FreqEnvelope = NULL;
    }

    if (NoteVoicePar[nvoice].FreqLfo)
    {
        
        adnotepars->lfoPool->destroy(NoteVoicePar[nvoice].FreqLfo);
        NoteVoicePar[nvoice].FreqLfo = NULL;
    }

    if (NoteVoicePar[nvoice].AmpEnvelope)
    {
        adnotepars->envelopePool->destroy(NoteVoicePar[nvoice].AmpEnvelope);
        NoteVoicePar[nvoice].AmpEnvelope = NULL;
    }
    
    if (NoteVoicePar[nvoice].AmpLfo)
    {
        adnotepars->lfoPool->destroy(NoteVoicePar[nvoice].AmpLfo);
        NoteVoicePar[nvoice].AmpLfo = NULL;
    }
    
    if (NoteVoicePar[nvoice].VoiceFilter)
    {
        Runtime.dead_ptrs.push_back(NoteVoicePar[nvoice].VoiceFilter);
        NoteVoicePar[nvoice].VoiceFilter.reset();
    }
    if (NoteVoicePar[nvoice].FilterEnvelope)
    {
        adnotepars->envelopePool->destroy(NoteVoicePar[nvoice].FilterEnvelope);
        NoteVoicePar[nvoice].FilterEnvelope = NULL;
}

    if (NoteVoicePar[nvoice].FilterLfo)
    {
        adnotepars->lfoPool->destroy(NoteVoicePar[nvoice].FilterLfo);
        NoteVoicePar[nvoice].FilterLfo = NULL;
    }

    if (NoteVoicePar[nvoice].FMFreqEnvelope)
    {
        adnotepars->envelopePool->destroy(NoteVoicePar[nvoice].FMFreqEnvelope);
        NoteVoicePar[nvoice].FMFreqEnvelope = NULL;
    }
    
    if (NoteVoicePar[nvoice].FMAmpEnvelope)
    {
        adnotepars->envelopePool->destroy(NoteVoicePar[nvoice].FMAmpEnvelope);
        NoteVoicePar[nvoice].FMAmpEnvelope = NULL;
    }
    
    if (NoteVoicePar[nvoice].FMEnabled != NONE
        && NoteVoicePar[nvoice].FMVoice < 0)
    {
        adnotepars->smpPool->free(NoteVoicePar[nvoice].FMSmp);
        NoteVoicePar[nvoice].FMSmp = NULL;
    }

    if (NoteVoicePar[nvoice].VoiceOut)
        // do not delete yet, just clear: perhaps is used by another voice
        memset(NoteVoicePar[nvoice].VoiceOut, 0, buffersize * sizeof(float));
    NoteVoicePar[nvoice].Enabled = false;
}

// Kill the note
void ADnote::KillNote()
{
    int nvoice;
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (NoteVoicePar[nvoice].Enabled)
            KillVoice(nvoice);

        // delete VoiceOut
        if (NoteVoicePar[nvoice].VoiceOut != NULL)
            delete NoteVoicePar[nvoice].VoiceOut;
        NoteVoicePar[nvoice].VoiceOut = NULL;
    }

    if (NoteGlobalPar.FreqEnvelope)
    {
        adnotepars->envelopePool->destroy(NoteGlobalPar.FreqEnvelope);
        NoteGlobalPar.FreqEnvelope = NULL;
    }

    if (NoteGlobalPar.FreqLfo)
    {
        adnotepars->lfoPool->destroy(NoteGlobalPar.FreqLfo);
        NoteGlobalPar.FreqLfo = NULL;
    }

    if (NoteGlobalPar.AmpEnvelope)
    {
        adnotepars->envelopePool->destroy(NoteGlobalPar.AmpEnvelope);
        NoteGlobalPar.AmpEnvelope = NULL;
    }

    if (NoteGlobalPar.AmpLfo)
    {
        adnotepars->lfoPool->destroy(NoteGlobalPar.AmpLfo);
        NoteGlobalPar.AmpLfo = NULL;
    }
    Runtime.dead_ptrs.push_back(NoteGlobalPar.GlobalFilterL);
    NoteGlobalPar.GlobalFilterL.reset();
    if (stereo)
    {
        Runtime.dead_ptrs.push_back(NoteGlobalPar.GlobalFilterR);
        NoteGlobalPar.GlobalFilterR.reset();
    }

    if (NoteGlobalPar.FilterEnvelope)
    {
        adnotepars->envelopePool->destroy(NoteGlobalPar.FilterEnvelope);
        NoteGlobalPar.FilterEnvelope = NULL;
    }

    if (NoteGlobalPar.FilterLfo)
    {
        adnotepars->lfoPool->destroy(NoteGlobalPar.FilterLfo);
        NoteGlobalPar.FilterLfo = NULL;
    }
    NoteEnabled = false;
}

ADnote::~ADnote()
{
    if (NoteEnabled)
        KillNote();
    adnotepars->buffPool->free(tmpwave);
    adnotepars->buffPool->free(bypassl);
    adnotepars->buffPool->free(bypassr);
}


// Init the parameters
void ADnote::initparameters()
{
    int nvoice;
    int tmp[NUM_VOICES];

    // Global Parameters
    NoteGlobalPar.FreqEnvelope = adnotepars->envelopePool->
        construct(Envelope(adnotepars->GlobalPar.FreqEnvelope, basefreq));
    NoteGlobalPar.FreqLfo = adnotepars->lfoPool->
        construct(LFO(adnotepars->GlobalPar.FreqLfo, basefreq));

    NoteGlobalPar.AmpEnvelope = adnotepars->envelopePool->
        construct(Envelope(adnotepars->GlobalPar.AmpEnvelope, basefreq));

    NoteGlobalPar.AmpLfo = adnotepars->lfoPool->
        construct(LFO(adnotepars->GlobalPar.AmpLfo, basefreq));

    NoteGlobalPar.Volume =
        4.0 * powf(0.1f, 3.0f * (1.0f - adnotepars->GlobalPar.PVolume / 96.0f)) // -60 dB .. 0 dB
        * VelF(velocity, adnotepars->GlobalPar.PAmpVelocityScaleFunction); // velocity sensing

    NoteGlobalPar.AmpEnvelope->envout_dB(); // discard the first envelope output
    globalnewamplitude =
        NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB()
        * NoteGlobalPar.AmpLfo->amplfoout();

    NoteGlobalPar.GlobalFilterL = boost::shared_ptr<Filter>(new Filter(adnotepars->GlobalPar.GlobalFilter));
    if (stereo)
        NoteGlobalPar.GlobalFilterR = boost::shared_ptr<Filter>(new Filter(adnotepars->GlobalPar.GlobalFilter));

    NoteGlobalPar.FilterEnvelope = adnotepars->envelopePool->
        construct(Envelope(adnotepars->GlobalPar.FilterEnvelope, basefreq));
    NoteGlobalPar.FilterLfo = adnotepars->lfoPool->construct(LFO(adnotepars->GlobalPar.FilterLfo, basefreq));
    NoteGlobalPar.FilterQ = adnotepars->GlobalPar.GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking =
        adnotepars->GlobalPar.GlobalFilter->getfreqtracking(basefreq);

    // Forbids the Modulation Voice to be greater or equal than voice
    for (int i = NUM_VOICES - 1; i >= 0; --i)
        if (NoteVoicePar[i].FMVoice >= i)
            NoteVoicePar[i].FMVoice = -1;

    // Voice Parameter init
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (NoteVoicePar[nvoice].Enabled == 0)
            continue;

        NoteVoicePar[nvoice].noisetype = adnotepars->VoicePar[nvoice].Type;
        // Voice Amplitude Parameters Init
        NoteVoicePar[nvoice].Volume =
            powf(0.1f, 3.0f * (1.0f - adnotepars->VoicePar[nvoice].PVolume / 127.0f)) // -60 dB .. 0 dB
            * VelF(velocity,adnotepars->VoicePar[nvoice].PAmpVelocityScaleFunction); // velocity

        if (adnotepars->VoicePar[nvoice].PVolumeminus != 0)
            NoteVoicePar[nvoice].Volume = -NoteVoicePar[nvoice].Volume;

        if (adnotepars->VoicePar[nvoice].PPanning == 0)
            NoteVoicePar[nvoice].Panning = zynMaster->numRandom(); // random panning
        else
            NoteVoicePar[nvoice].Panning =
                adnotepars->VoicePar[nvoice].PPanning / 128.0;

        newamplitude[nvoice] = 1.0;
        if (adnotepars->VoicePar[nvoice].PAmpEnvelopeEnabled)
        {
            NoteVoicePar[nvoice].AmpEnvelope = adnotepars->envelopePool->
                construct(Envelope(adnotepars->VoicePar[nvoice].AmpEnvelope, basefreq));
            NoteVoicePar[nvoice].AmpEnvelope->envout_dB(); // discard the first envelope sample
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();
        }

        if (adnotepars->VoicePar[nvoice].PAmpLfoEnabled)
        {
            NoteVoicePar[nvoice].AmpLfo = adnotepars->lfoPool->
                construct(LFO(adnotepars->VoicePar[nvoice].AmpLfo, basefreq));
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();
        }

        // Voice Frequency Parameters Init
        if (adnotepars->VoicePar[nvoice].PFreqEnvelopeEnabled)
            NoteVoicePar[nvoice].FreqEnvelope = adnotepars->envelopePool->
                construct(Envelope(adnotepars->VoicePar[nvoice].FreqEnvelope, basefreq));

        if (adnotepars->VoicePar[nvoice].PFreqLfoEnabled)
            NoteVoicePar[nvoice].FreqLfo = adnotepars->lfoPool->
                construct(LFO(adnotepars->VoicePar[nvoice].FreqLfo, basefreq));

        // Voice Filter Parameters Init
        if (adnotepars->VoicePar[nvoice].PFilterEnabled)
            NoteVoicePar[nvoice].VoiceFilter = boost::shared_ptr<Filter>(new Filter(adnotepars->VoicePar[nvoice].VoiceFilter));

        if (adnotepars->VoicePar[nvoice].PFilterEnvelopeEnabled)
            NoteVoicePar[nvoice].FilterEnvelope = adnotepars->envelopePool->
                construct(Envelope(adnotepars->VoicePar[nvoice].FilterEnvelope, basefreq));

        if (adnotepars->VoicePar[nvoice].PFilterLfoEnabled)
            NoteVoicePar[nvoice].FilterLfo =adnotepars->lfoPool->
                construct(LFO(adnotepars->VoicePar[nvoice].FilterLfo, basefreq));

        NoteVoicePar[nvoice].FilterFreqTracking =
            adnotepars->VoicePar[nvoice].VoiceFilter->getfreqtracking(basefreq);

        // Voice Modulation Parameters Init
        if (NoteVoicePar[nvoice].FMEnabled != NONE
            && NoteVoicePar[nvoice].FMVoice < 0)
        {
            adnotepars->VoicePar[nvoice].FMSmp->newrandseed();
            NoteVoicePar[nvoice].FMSmp = (float*)adnotepars->smpPool->malloc();

            // Perform Anti-aliasing only on MORPH or RING MODULATION

            int vc = nvoice;
            if (adnotepars->VoicePar[nvoice].PextFMoscil != -1)
                vc = adnotepars->VoicePar[nvoice].PextFMoscil;

            float tmp = 1.0;
            if (adnotepars->VoicePar[vc].FMSmp->Padaptiveharmonics != 0
                || NoteVoicePar[nvoice].FMEnabled == MORPH
                || NoteVoicePar[nvoice].FMEnabled == RING_MOD)
            {
                tmp = getFMvoicebasefreq(nvoice);
            }
            if (!adnotepars->GlobalPar.Hrandgrouping)
                adnotepars->VoicePar[vc].FMSmp->newrandseed();

            oscposhiFM[nvoice] =
                (oscposhi[nvoice] + adnotepars->VoicePar[vc].FMSmp->get(NoteVoicePar[nvoice].FMSmp,
                 tmp)) % oscilsize;
            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].FMSmp[oscilsize + i] =
                    NoteVoicePar[nvoice].FMSmp[i];
            oscposhiFM[nvoice] +=
                (int)((adnotepars->VoicePar[nvoice].PFMoscilphase - 64.0f)
                      / 128.0f * oscilsize + oscilsize * 4.0f);
            oscposhiFM[nvoice] %= oscilsize;
        }

        if (adnotepars->VoicePar[nvoice].PFMFreqEnvelopeEnabled)
            NoteVoicePar[nvoice].FMFreqEnvelope = adnotepars->envelopePool->
                construct(Envelope(adnotepars->VoicePar[nvoice].FMFreqEnvelope, basefreq));

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;

        if (adnotepars->VoicePar[nvoice].PFMAmpEnvelopeEnabled)
        {
            NoteVoicePar[nvoice].FMAmpEnvelope = adnotepars->envelopePool->
                construct(Envelope(adnotepars->VoicePar[nvoice].FMAmpEnvelope, basefreq));
            FMnewamplitude[nvoice] *= NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
        }
    }

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (int i = nvoice + 1; i < NUM_VOICES; ++i)
            tmp[i] = 0;
        for (int i = nvoice + 1; i < NUM_VOICES; ++i)
        {
            if (NoteVoicePar[i].FMVoice == nvoice && !tmp[i])
            {
                NoteVoicePar[nvoice].VoiceOut = new float[buffersize]; // !!!!!!!!!!!
                tmp[i] = 1;
            }
        }
        if (NoteVoicePar[nvoice].VoiceOut != NULL)
            memset(NoteVoicePar[nvoice].VoiceOut, 0, buffersize * sizeof(float));
    }
}


// Get Voice's Modullator base frequency
float ADnote::getFMvoicebasefreq(int nvoice)
{
    return getvoicebasefreq(nvoice)
        * powf(2.0f, (NoteVoicePar[nvoice].FMDetune / 100.0f) / 12.0f);
}


// Computes the frequency of an oscillator
void ADnote::setfreq(int nvoice, float freq)
{
    float speed;
    freq = fabsf(freq);
    speed = freq * (float)oscilsize / (float)samplerate;
    speed = (speed > (float)oscilsize) ? (float)oscilsize : speed;
    F2I(speed, oscfreqhi[nvoice]);
    oscfreqlo[nvoice] = speed - floorf(speed);
}


// Computes the frequency of an modullator oscillator
void ADnote::setfreqFM(int nvoice, float freq)
{
    float speed;
    freq = fabsf(freq);
    speed = freq * (float)oscilsize / (float)samplerate;
    speed = (speed > (float)oscilsize) ? (float)oscilsize : speed;
    F2I(speed, oscfreqhiFM[nvoice]);
    oscfreqloFM[nvoice] = speed - floorf(speed);
}


// Get Voice base frequency
float ADnote::getvoicebasefreq(int nvoice)
{
    float detune =
        NoteVoicePar[nvoice].Detune / 100.0f + NoteVoicePar[nvoice].FineDetune /
            100.0f * ctl->bandwidth.relbw * bandwidthDetuneMultiplier
                + NoteGlobalPar.Detune / 100.0f;

    if (!NoteVoicePar[nvoice].fixedfreq)
        return basefreq * powf(2.0f, detune / 12.0f);
    else
    {   // the fixed freq is enabled
        float fixedfreq = 440.0f;
        int fixedfreqET = NoteVoicePar[nvoice].fixedfreqET;
        if (fixedfreqET != 0)
        {   // if the frequency varies according the keyboard note
            float tmp = (midinote - 69.0f) / 12.0f
                * (powf(2.0f, (fixedfreqET - 1) / 63.0f) - 1.0f);
            if (fixedfreqET <= 64)
                fixedfreq *= powf(2.0f, tmp);
            else
                fixedfreq *= powf(3.0f, tmp);
        }
        return fixedfreq * powf(2.0f, detune / 12.0f);
    }
}


// Computes all the parameters for each tick
void ADnote::computecurrentparameters(void)
{
    int nvoice;
    float voicefreq, voicepitch, filterpitch, filterfreq, FMfreq,
                FMrelativepitch, globalpitch, globalfilterpitch;
    globalpitch =
        0.01f * (NoteGlobalPar.FreqEnvelope->envout()
            + NoteGlobalPar.FreqLfo->lfoout() * ctl->modwheel.relmod);
    globaloldamplitude = globalnewamplitude;
    globalnewamplitude =
        NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB()
            * NoteGlobalPar.AmpLfo->amplfoout();

    globalfilterpitch = NoteGlobalPar.FilterEnvelope->envout()
                        + NoteGlobalPar.FilterLfo->lfoout()
                        + NoteGlobalPar.FilterCenterPitch;

    float tmpfilterfreq = globalfilterpitch
                                + ctl->filtercutoff.relfreq
                                + NoteGlobalPar.FilterFreqTracking;

    tmpfilterfreq = NoteGlobalPar.GlobalFilterL->getrealfreq(tmpfilterfreq);

    float globalfilterq = NoteGlobalPar.FilterQ * ctl->filterq.relq;
    NoteGlobalPar.GlobalFilterL->setfreq_and_q(tmpfilterfreq,globalfilterq);
    if (stereo)
        NoteGlobalPar.GlobalFilterR->setfreq_and_q(tmpfilterfreq, globalfilterq);

    // compute the portamento, if it is used by this note
    float portamentofreqrap = 1.0f;
    if (portamento)
    {   // this voice use portamento
        portamentofreqrap = ctl->portamento.freqrap;
        if (!ctl->portamento.used)
        {   //the portamento has finished
            portamento = 0; // this note is no longer "portamented"
        }
    }

    // compute parameters for all voices
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue;
        NoteVoicePar[nvoice].DelayTicks -= 1;
        if (NoteVoicePar[nvoice].DelayTicks > 0)
            continue;

        //==================
        //   Voice Amplitude

        oldamplitude[nvoice] = newamplitude[nvoice];
        newamplitude[nvoice] = 1.0f;

        if (NoteVoicePar[nvoice].AmpEnvelope)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();

        if (NoteVoicePar[nvoice].AmpLfo)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();

        //=============
        // Voice Filter

        if (NoteVoicePar[nvoice].VoiceFilter)
        {
            filterpitch = NoteVoicePar[nvoice].FilterCenterPitch;

            if (NoteVoicePar[nvoice].FilterEnvelope)
                filterpitch += NoteVoicePar[nvoice].FilterEnvelope->envout();

            if (NoteVoicePar[nvoice].FilterLfo)
                filterpitch += NoteVoicePar[nvoice].FilterLfo->lfoout();

            filterfreq = filterpitch + NoteVoicePar[nvoice].FilterFreqTracking;
            filterfreq = NoteVoicePar[nvoice].VoiceFilter->getrealfreq(filterfreq);

            NoteVoicePar[nvoice].VoiceFilter->setfreq(filterfreq);
        }

        if (NoteVoicePar[nvoice].noisetype == 0)
        {   // compute only if the voice isn't noise

            //==================
            // Voice Frequency

            voicepitch = 0.0f;
            if (NoteVoicePar[nvoice].FreqLfo)
                voicepitch += NoteVoicePar[nvoice].FreqLfo->lfoout() / 100.0f
                              * ctl->bandwidth.relbw;

            if (NoteVoicePar[nvoice].FreqEnvelope)
                voicepitch += NoteVoicePar[nvoice].FreqEnvelope->envout() / 100.0f;
            voicefreq = getvoicebasefreq(nvoice)
                        * powf(2.0f, (voicepitch + globalpitch) / 12.0f); // Hz frequency
            voicefreq *= ctl->pitchwheel.relfreq; // change the frequency by the controller
            setfreq(nvoice, voicefreq * portamentofreqrap);

            //==================
            //  Modulator

            if (NoteVoicePar[nvoice].FMEnabled != NONE)
            {
                FMrelativepitch = NoteVoicePar[nvoice].FMDetune / 100.0f;
                if (NoteVoicePar[nvoice].FMFreqEnvelope != NULL)
                    FMrelativepitch += NoteVoicePar[nvoice].FMFreqEnvelope->envout() / 100;
                FMfreq = powf(2.0f, FMrelativepitch / 12.0f) * voicefreq * portamentofreqrap;
                setfreqFM(nvoice, FMfreq);

                FMoldamplitude[nvoice] = FMnewamplitude[nvoice];
                FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;
                if (NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
                    FMnewamplitude[nvoice] *= NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
            }
        }
    }
    time += (float)buffersize / (float)samplerate;
}


// Fadein in a way that removes clicks but keep sound "punchy"
void ADnote::fadein(float *smps)
{
    //unsigned int buffersize = zynMaster->getBuffersize();
    int zerocrossings = 0;
    for (int i = 1; i < buffersize; ++i)
        if (smps[i - 1] < 0.0 && smps[i] > 0.0f)
            zerocrossings++; // this is only the possitive crossings

    float tmp = (buffersize - 1.0f) / (zerocrossings + 1) / 3.0f;
    if (tmp < 8.0f)
        tmp = 8.0f;

    int n;
    F2I(tmp, n); // how many samples is the fade-in
    if (n > buffersize)
        n = buffersize;
    for (int i = 0; i < n; ++i)
    {   // fade-in
        float tmp = 0.5f - cosf((float)i / (float)n * PI) * 0.5f;
        smps[i] *= tmp;
    }
}


// Computes the Oscillator (Without Modulation) - LinearInterpolation
void ADnote::ComputeVoiceOscillator_LinearInterpolation(int nvoice)
{
    int poshi;
    float poslo;
    poshi = oscposhi[nvoice];
    poslo = oscposlo[nvoice];
    float *smps = NoteVoicePar[nvoice].OscilSmp;
    for (int i = 0; i < buffersize; ++i)
    {
        tmpwave[i] = smps[poshi] * (1.0f - poslo) + smps[poshi + 1] * poslo;
        poslo += oscfreqlo[nvoice];
        if (poslo >= 1.0f)
        {
            poslo -= 1.0f;
            poshi++;
        }
        poshi += oscfreqhi[nvoice];
        poshi &= oscilsize - 1;
    }
    oscposhi[nvoice] = poshi;
    oscposlo[nvoice] = poslo;
}


// Computes the Oscillator (Morphing)
void ADnote::ComputeVoiceOscillatorMorph(int nvoice)
{
    int i;
    float amp;
    ComputeVoiceOscillator_LinearInterpolation(nvoice);
    FMnewamplitude[nvoice] = (FMnewamplitude[nvoice] > 1.0f) ? 1.0f : FMnewamplitude[nvoice];

    FMoldamplitude[nvoice] = (FMoldamplitude[nvoice] > 1.0f) ? 1.0f : FMoldamplitude[nvoice];
    if (NoteVoicePar[nvoice].FMVoice >= 0)
    {
        // if I use VoiceOut[] as modullator
        int FMVoice = NoteVoicePar[nvoice].FMVoice;
        for (i = 0; i < buffersize; ++i)
        {
            amp = InterpolateAmplitude(FMoldamplitude[nvoice],
                                        FMnewamplitude[nvoice], i, buffersize);
            tmpwave[i] = tmpwave[i] * (1.0 - amp) + amp
                         * NoteVoicePar[FMVoice].VoiceOut[i];
        }
    } else {
        int poshiFM = oscposhiFM[nvoice];
        float posloFM = oscposloFM[nvoice];

        for (i = 0; i < buffersize; ++i)
        {
            amp = InterpolateAmplitude(FMoldamplitude[nvoice],
                                        FMnewamplitude[nvoice], i, buffersize);
            tmpwave[i] = tmpwave[i] * (1.0 - amp) + amp
                          * (NoteVoicePar[nvoice].FMSmp[poshiFM] * (1 - posloFM)
                          + NoteVoicePar[nvoice].FMSmp[poshiFM + 1] * posloFM);
            posloFM += oscfreqloFM[nvoice];
            if (posloFM >= 1.0f)
            {
                posloFM -= 1.0f;
                poshiFM++;
            }
            poshiFM += oscfreqhiFM[nvoice];
            poshiFM &= oscilsize - 1;
        }
        oscposhiFM[nvoice] = poshiFM;
        oscposloFM[nvoice] = posloFM;
    }
}


// Computes the Oscillator (Ring Modulation)
void ADnote::ComputeVoiceOscillatorRingModulation(int nvoice)
{
    float amp;
    ComputeVoiceOscillator_LinearInterpolation(nvoice);
    if (FMnewamplitude[nvoice] > 1.0f)
        FMnewamplitude[nvoice] = 1.0f;
    if (FMoldamplitude[nvoice] > 1.0f)
        FMoldamplitude[nvoice] = 1.0f;
    if (NoteVoicePar[nvoice].FMVoice >= 0)
    {
        // if I use VoiceOut[] as modullator
        for (int i = 0; i < buffersize; ++i)
        {
            amp = InterpolateAmplitude(FMoldamplitude[nvoice],
                                       FMnewamplitude[nvoice], i, buffersize);
            int FMVoice = NoteVoicePar[nvoice].FMVoice;
            // yoshi note:-
            // originally this nested loop was for (i=0;i<SOUND_BUFFER_SIZE;i++)
            // which would neatly clobber the outer loop,
            // not sure if that's good or bad ??
            for (int j = 0; j < buffersize; ++j)
                tmpwave[j] *= (1.0f - amp) + amp * NoteVoicePar[FMVoice].VoiceOut[j];
        }
    }
    else
    {
        int poshiFM=oscposhiFM[nvoice];
        float posloFM = oscposloFM[nvoice];

        for (int i = 0; i < buffersize; ++i)
        {
            amp = InterpolateAmplitude(FMoldamplitude[nvoice],
                                       FMnewamplitude[nvoice], i, buffersize);
            tmpwave[i] *= (NoteVoicePar[nvoice].FMSmp[poshiFM] * (1.0f - posloFM)
                          + NoteVoicePar[nvoice].FMSmp[poshiFM + 1] * posloFM)
                          * amp + (1.0f - amp);
            posloFM += oscfreqloFM[nvoice];
            if (posloFM >= 1.0f)
            {
                posloFM -= 1.0f;
                poshiFM++;
            }
            poshiFM += oscfreqhiFM[nvoice];
            poshiFM &= oscilsize - 1;
        }
        oscposhiFM[nvoice] = poshiFM;
        oscposloFM[nvoice] = posloFM;
    }
}


// Computes the Oscillator (Phase Modulation or Frequency Modulation)
void ADnote::ComputeVoiceOscillatorFrequencyModulation(int nvoice, int FMmode)
{
    int carposhi;
    int FMmodfreqhi;
    float FMmodfreqlo, carposlo;

    if (NoteVoicePar[nvoice].FMVoice >= 0)
    {
        // if I use VoiceOut[] as modulator
        for (int i = 0; i < buffersize; ++i)
            tmpwave[i] = NoteVoicePar[NoteVoicePar[nvoice].FMVoice].VoiceOut[i];
    } else {
        // Compute the modulator and store it in tmpwave[]
        int poshiFM = oscposhiFM[nvoice];
        float posloFM = oscposloFM[nvoice];

        for (int i = 0; i < buffersize; ++i)
        {
            tmpwave[i] = (NoteVoicePar[nvoice].FMSmp[poshiFM] * (1.0f - posloFM)
                         + NoteVoicePar[nvoice].FMSmp[poshiFM + 1] * posloFM);
            posloFM += oscfreqloFM[nvoice];
            if (posloFM >= 1.0f)
            {
                posloFM = fmodf(posloFM, 1.0f);
                poshiFM++;
            }
            poshiFM += oscfreqhiFM[nvoice];
            poshiFM &= oscilsize - 1;
        }
        oscposhiFM[nvoice] = poshiFM;
        oscposloFM[nvoice] = posloFM;
    }
    // Amplitude interpolation
    if (AboveAmplitudeThreshold(FMoldamplitude[nvoice], FMnewamplitude[nvoice]))
    {
        for (int i = 0; i < buffersize; ++i)
        {
            tmpwave[i] *= InterpolateAmplitude(FMoldamplitude[nvoice],
                                                FMnewamplitude[nvoice], i,
                                                buffersize);
        }
    } else for (int i = 0; i < buffersize; ++i)
        tmpwave[i] *= FMnewamplitude[nvoice];

    // normalize makes all sample-rates, oscil_sizes toproduce same sound
    if (FMmode != 0)
    {   // Frequency modulation
        float normalize = (float)oscilsize / 262144.0f * 44100.0f / (float)samplerate;
        for (int i = 0; i < buffersize; ++i)
        {
            FMoldsmp[nvoice] = fmodf(FMoldsmp[nvoice] + tmpwave[i] * normalize,
                                     oscilsize);
            tmpwave[i] = FMoldsmp[nvoice];
        }
    }
    else
    {    // Phase modulation
        float normalize = (float)oscilsize / 262144.0f;
        for (int i = 0; i < buffersize; ++i)
            tmpwave[i] *= normalize;
    }

    for (int i = 0; i < buffersize; ++i)
    {
        F2I(tmpwave[i], FMmodfreqhi);
        FMmodfreqlo = fmodf(tmpwave[i] + 0.0000000001f, 1.0f);
        if (FMmodfreqhi < 0)
            FMmodfreqlo++;

        // carrier
        carposhi = oscposhi[nvoice] + FMmodfreqhi;
        carposlo = oscposlo[nvoice] + FMmodfreqlo;

        if (carposlo >= 1.0f)
        {
            carposhi++;
            carposlo = fmodf(carposlo, 1.0f);
        }
        carposhi &= (oscilsize - 1);

        tmpwave[i] = NoteVoicePar[nvoice].OscilSmp[carposhi] * (1.0f - carposlo)
                     + NoteVoicePar[nvoice].OscilSmp[carposhi + 1] * carposlo;

        oscposlo[nvoice] += oscfreqlo[nvoice];
        if (oscposlo[nvoice] >= 1.0f)
        {
            oscposlo[nvoice] = fmodf(oscposlo[nvoice], 1.0f);
            oscposhi[nvoice]++;
        }

        oscposhi[nvoice] += oscfreqhi[nvoice];
        oscposhi[nvoice] &= oscilsize - 1;
    }
}


// Calculeaza Oscilatorul cu PITCH MODULATION
void ADnote::ComputeVoiceOscillatorPitchModulation(int nvoice)
{
//TODO
}


// Compute the ADnote samples, returns 0 if the note is finishedint ADnote::noteout(float *outl, float *outr)
 int ADnote::noteout(float *outl, float *outr)
{
    if (!NoteEnabled)
        return 0;

    int nvoice;
    memset(outl, 0, buffersize * sizeof(float));
    memset(outr, 0, buffersize * sizeof(float));
    memset(bypassl, 0, buffersize * sizeof(float));
    memset(bypassr, 0, buffersize * sizeof(float));
    computecurrentparameters();
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled
            || NoteVoicePar[nvoice].DelayTicks > 0)
            continue;
        if (NoteVoicePar[nvoice].noisetype == 0)
        {   // voice mode = sound
            switch (NoteVoicePar[nvoice].FMEnabled)
            {
                case MORPH:
                    ComputeVoiceOscillatorMorph(nvoice);
                    break;
                case RING_MOD:
                    ComputeVoiceOscillatorRingModulation(nvoice);
                    break;
                case PHASE_MOD:
                    ComputeVoiceOscillatorFrequencyModulation(nvoice, 0);
                    break;
                case FREQ_MOD:
                    ComputeVoiceOscillatorFrequencyModulation(nvoice, 1);
                    break;
                    //case PITCH_MOD:ComputeVoiceOscillatorPitchModulation(nvoice);break;
                default:
                    ComputeVoiceOscillator_LinearInterpolation(nvoice);
                    // if (Runtime.settings.Interpolation) ComputeVoiceOscillator_CubicInterpolation(nvoice);
                    break;
            }
        }
        else // add some noise
            for (int i = 0; i < buffersize; ++i)
                tmpwave[i] = zynMaster->numRandom() * 2.0f - 1.0f;

        // Voice Processing ...

        // Amplitude
        if (AboveAmplitudeThreshold(oldamplitude[nvoice], newamplitude[nvoice]))
        {
            int rest = buffersize;
            // test if the amplitude if raising and the difference is high
            if (newamplitude[nvoice] > oldamplitude[nvoice]
                && (newamplitude[nvoice] - oldamplitude[nvoice]) > 0.25f)
            {
                rest = 10;
                if (rest > buffersize)
                    rest = buffersize;
                for (int i = 0; i < buffersize - rest; ++i)
                    tmpwave[i] *= oldamplitude[nvoice];
            }
            // Amplitude interpolation
            for (int i = 0; i < rest; ++i)
            {
                tmpwave[i + (buffersize - rest)] *=
                    InterpolateAmplitude(oldamplitude[nvoice],
                                         newamplitude[nvoice], i, rest);
            }
        } else for (int i = 0; i < buffersize; ++i)
            tmpwave[i] *= newamplitude[nvoice];

        // Fade in
        if (firsttick[nvoice] != 0)
        {
            fadein(&tmpwave[0]);
            firsttick[nvoice] = 0;
        }

        // Filter
        if (NoteVoicePar[nvoice].VoiceFilter != NULL)
            NoteVoicePar[nvoice].VoiceFilter->filterout(&tmpwave[0]);

        // check if the amplitude envelope is finished, if yes, the voice will be fadeout
        if (NoteVoicePar[nvoice].AmpEnvelope)
        {
            if (NoteVoicePar[nvoice].AmpEnvelope->finished())
                for (int i = 0; i < buffersize; ++i)
                    tmpwave[i] *= 1.0f - (float)i / (float)buffersize;
            // the voice is killed later
        }

        // Put the ADnote samples in VoiceOut (without appling Global volume,
        // because I wish to use this voice as a modullator)
        if (NoteVoicePar[nvoice].VoiceOut != NULL)
            for (int i = 0; i < buffersize; ++i)
                NoteVoicePar[nvoice].VoiceOut[i] = tmpwave[i];

        // Add the voice that do not bypass the filter to out
        if (NoteVoicePar[nvoice].filterbypass == 0)
        {   // no bypass
            if (!stereo)
                for (int i = 0; i < buffersize; ++i)
                   outl[i] += tmpwave[i] * NoteVoicePar[nvoice].Volume; // mono
            else for (int i = 0; i < buffersize; ++i)
            {   // stereo
                outl[i] += tmpwave[i] * NoteVoicePar[nvoice].Volume
                           * (1.0 - NoteVoicePar[nvoice].Panning) * 2.0f;
                outr[i] += tmpwave[i] * NoteVoicePar[nvoice].Volume
                           * NoteVoicePar[nvoice].Panning * 2.0f;
            }
        }
        else
        {   // bypass the filter
            if (!stereo)
                for (int i = 0 ; i < buffersize; ++i)
                    bypassl[i] += tmpwave[i] * NoteVoicePar[nvoice].Volume; // mono
            else for (int i = 0; i < buffersize; ++i)
            {   // stereo
                bypassl[i] += tmpwave[i] * NoteVoicePar[nvoice].Volume
                              * (1.0f - NoteVoicePar[nvoice].Panning) * 2.0f;
                bypassr[i] += tmpwave[i] * NoteVoicePar[nvoice].Volume
                              * NoteVoicePar[nvoice].Panning * 2.0f;
            }
        }
        // chech if there is necesary to proces the voice longer (if the Amplitude envelope isn't finished)
        if (NoteVoicePar[nvoice].AmpEnvelope)
        {
            if (NoteVoicePar[nvoice].AmpEnvelope->finished())
                KillVoice(nvoice);
        }
    }


    // Processing Global parameters
    NoteGlobalPar.GlobalFilterL->filterout(&outl[0]);

    if (!stereo)
    {
        for (int i = 0; i < buffersize; ++i)
        {   // set the right channel=left channel
            outr[i] = outl[i];
            bypassr[i] = bypassl[i];
        }
    } else
        NoteGlobalPar.GlobalFilterR->filterout(&outr[0]);

    for (int i = 0; i < buffersize; ++i)
    {
        outl[i] += bypassl[i];
        outr[i] += bypassr[i];
    }

    if (AboveAmplitudeThreshold(globaloldamplitude, globalnewamplitude))
    {
        // Amplitude Interpolation
        for (int i = 0; i < buffersize; ++i)
        {
            float tmpvol =
                InterpolateAmplitude(globaloldamplitude, globalnewamplitude, i,
                                     buffersize);
            outl[i] *= tmpvol * (1.0f - NoteGlobalPar.Panning);
            outr[i] *= tmpvol * NoteGlobalPar.Panning;
        }
    } else {
        for (int i = 0; i < buffersize; ++i)
        {
            outl[i] *= globalnewamplitude * (1.0f - NoteGlobalPar.Panning);
            outr[i] *= globalnewamplitude * NoteGlobalPar.Panning;
        }
    }

    // Apply the punch
    if (NoteGlobalPar.Punch.Enabled)
    {
        for (int i = 0; i < buffersize; ++i)
        {
            float punchamp =
                NoteGlobalPar.Punch.initialvalue * NoteGlobalPar.Punch.t + 1.0f;
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
    if (Legato.silent && Legato.msg != LM_FadeIn)
    {   // Silencer
        memset(outl, 0, buffersize * sizeof(float));
        memset(outr, 0, buffersize * sizeof(float));
    }
    switch (Legato.msg)
    {
        case LM_CatchUp : // Continue the catch-up...
            if (Legato.decounter == -10)
                Legato.decounter = Legato.fade.length;
            for (int i = 0; i < buffersize; ++i) { // Yea, could be done without the loop...
                Legato.decounter--;
                if (Legato.decounter < 1)
                {
                    // Catching-up done, we can finally set
                    // the note to the actual parameters.
                    Legato.decounter = -10;
                    Legato.msg = LM_ToNorm;
                    ADlegatonote(Legato.param.freq, Legato.param.vel,
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
                    memset(outl, 0, buffersize * sizeof(float));
                    memset(outr, 0, buffersize * sizeof(float));
                    Legato.decounter = -10;
                    Legato.silent = true;
                    // Fading-out done, now set the catch-up :
                    Legato.decounter = Legato.fade.length;
                    Legato.msg = LM_CatchUp;
                    float catchupfreq =
                        Legato.param.freq * (Legato.param.freq / Legato.lastfreq);
                        // This freq should make this now silent note to catch-up
                        // (or should I say resync ?) with the heard note for the
                        // same length it stayed at the previous freq during the fadeout.

                    ADlegatonote(catchupfreq, Legato.param.vel,
                                 Legato.param.portamento,
                                 Legato.param.midinote, false);
                    break;
                }
                Legato.fade.m -= Legato.fade.step;
                outl[i] *= Legato.fade.m;
                outr[i] *= Legato.fade.m;
            }
            break;
        default :
            break;
    }


    // Check if the global amplitude is finished.
    // If it does, disable the note
    if (NoteGlobalPar.AmpEnvelope->finished())
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


// Release the key (NoteOff)
void ADnote::relasekey(void)
{
    int nvoice;
    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (NoteVoicePar[nvoice].Enabled == 0)
            continue;
        if (NoteVoicePar[nvoice].AmpEnvelope)
            NoteVoicePar[nvoice].AmpEnvelope->relasekey();
        if (NoteVoicePar[nvoice].FreqEnvelope)
            NoteVoicePar[nvoice].FreqEnvelope->relasekey();
        if (NoteVoicePar[nvoice].FilterEnvelope)
            NoteVoicePar[nvoice].FilterEnvelope->relasekey();
        if (NoteVoicePar[nvoice].FMFreqEnvelope)
            NoteVoicePar[nvoice].FMFreqEnvelope->relasekey();
        if (NoteVoicePar[nvoice].FMAmpEnvelope)
            NoteVoicePar[nvoice].FMAmpEnvelope->relasekey();
    }
    NoteGlobalPar.FreqEnvelope->relasekey();
    NoteGlobalPar.FilterEnvelope->relasekey();
    NoteGlobalPar.AmpEnvelope->relasekey();
}
