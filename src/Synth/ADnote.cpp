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

#include <cmath>

using namespace std;

#include "Misc/Util.h"
#include "Misc/Master.h"
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
    time(0.0),
    portamento(portamento_),
    samplerate(pars->getSamplerate()),
    buffersize(pars->getBuffersize()),
    oscilsize(pars->getOscilsize())
{
    if (velocity > 1.0)
        velocity = 1.0;

    tmpwave = new float[buffersize];
    bypassl = new float[buffersize];
    bypassr = new float[buffersize];

    // Initialise some legato-specific vars
    Legato.msg = LM_Norm;
    Legato.fade.length = (int)(samplerate * 0.005); // 0.005 seems ok.
    if (Legato.fade.length < 1)
        Legato.fade.length = 1; // (if something's fishy)
    Legato.fade.step = (1.0 / Legato.fade.length);
    Legato.decounter = -10;
    Legato.param.freq = basefreq;
    Legato.param.vel = velocity;
    Legato.param.portamento = portamento;
    Legato.param.midinote = midinote;
    Legato.silent = besilent;

    NoteGlobalPar.Detune = getdetune(pars->GlobalPar.PDetuneType,
                                     pars->GlobalPar.PCoarseDetune,
                                     pars->GlobalPar.PDetune);
    bandwidthDetuneMultiplier = pars->getBandwidthDetuneMultiplier();

    if (pars->GlobalPar.PPanning == 0)
        NoteGlobalPar.Panning = zynMaster->numRandom();
    else
        NoteGlobalPar.Panning = pars->GlobalPar.PPanning / 128.0;

    NoteGlobalPar.FilterCenterPitch =
        pars->GlobalPar.GlobalFilter->getfreq() // center freq
        + pars->GlobalPar.PFilterVelocityScale / 127.0 * 6.0 // velocity sensing
        * (VelF(velocity, pars->GlobalPar.PFilterVelocityScaleFunction) - 1);

    if (pars->GlobalPar.PPunchStrength != 0)
    {
        NoteGlobalPar.Punch.Enabled = 1;
        NoteGlobalPar.Punch.t = 1.0; // start from 1.0 and to 0.0
        NoteGlobalPar.Punch.initialvalue =
            ((powf(10.0f, 1.5f * pars->GlobalPar.PPunchStrength / 127.0f) - 1.0)
             * VelF(velocity, pars->GlobalPar.PPunchVelocitySensing));
        float time = // 0.1 .. 100 ms
            powf(10.0f, 3.0f * pars->GlobalPar.PPunchTime / 127.0f) / 10000.0f;
        float stretch =
            powf(440.0f / basefreq, pars->GlobalPar.PPunchStretch / 64.0f);
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

        if (!pars->VoicePar[nvoice].Enabled)
        {
            NoteVoicePar[nvoice].Enabled = false;
            continue; // the voice is disabled
        }

        pars->VoicePar[nvoice].OscilSmp->newrandseed();
        NoteVoicePar[nvoice].Enabled = true;
        NoteVoicePar[nvoice].fixedfreq = pars->VoicePar[nvoice].Pfixedfreq;
        NoteVoicePar[nvoice].fixedfreqET = pars->VoicePar[nvoice].PfixedfreqET;

        // use the Globalpars.detunetype if the detunetype is 0
        if (pars->VoicePar[nvoice].PDetuneType != 0)
        {
            NoteVoicePar[nvoice].Detune =
                getdetune(pars->VoicePar[nvoice].PDetuneType,
                          pars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getdetune(pars->VoicePar[nvoice].PDetuneType, 0,
                          pars->VoicePar[nvoice].PDetune); // fine detune
        }
        else
        {
            NoteVoicePar[nvoice].Detune =
                getdetune(pars->GlobalPar.PDetuneType,
                          pars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getdetune(pars->GlobalPar.PDetuneType, 0,
                          pars->VoicePar[nvoice].PDetune); // fine detune
        }
        if (pars->VoicePar[nvoice].PFMDetuneType != 0)
        {
            NoteVoicePar[nvoice].FMDetune =
                getdetune(pars->VoicePar[nvoice].PFMDetuneType,
                          pars->VoicePar[nvoice].PFMCoarseDetune,
                          pars->VoicePar[nvoice].PFMDetune);
        }
        else
        {
            NoteVoicePar[nvoice].FMDetune =
                getdetune(pars->GlobalPar.PDetuneType,
                          pars->VoicePar[nvoice].PFMCoarseDetune,
                          pars->VoicePar[nvoice].PFMDetune);
        }

        oscposhi[nvoice] = 0;
        oscposlo[nvoice] = 0.0;
        oscposhiFM[nvoice] = 0;
        oscposloFM[nvoice] = 0.0;

        NoteVoicePar[nvoice].OscilSmp =
            new float[oscilsize + OSCIL_SMP_EXTRA_SAMPLES]; // the extra points contains the first point

        // Get the voice's oscil or external's voice oscil
        int vc = nvoice;
        if (pars->VoicePar[nvoice].Pextoscil != -1)
            vc = pars->VoicePar[nvoice].Pextoscil;
        if (!pars->GlobalPar.Hrandgrouping)
            pars->VoicePar[vc].OscilSmp->newrandseed();
        oscposhi[nvoice] =
            pars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                             getvoicebasefreq(nvoice),
                                             pars->VoicePar[nvoice].Presonance);

        // I store the first elments to the last position for speedups
        for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
            NoteVoicePar[nvoice].OscilSmp[oscilsize + i] =
                NoteVoicePar[nvoice].OscilSmp[i];

        oscposhi[nvoice] += (int)((pars->VoicePar[nvoice].Poscilphase - 64.0)
                             / 128.0 * oscilsize + 4 * oscilsize);
        oscposhi[nvoice] %= oscilsize;

        NoteVoicePar[nvoice].FreqLfo = NULL;
        NoteVoicePar[nvoice].FreqEnvelope = NULL;

        NoteVoicePar[nvoice].AmpLfo = NULL;
        NoteVoicePar[nvoice].AmpEnvelope = NULL;

        NoteVoicePar[nvoice].VoiceFilter = NULL;
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
        float fmvoldamp = powf(440.0f / getvoicebasefreq(nvoice),
                               pars->VoicePar[nvoice].PFMVolumeDamp / 64.0f - 1.0f);
        switch (NoteVoicePar[nvoice].FMEnabled)
        {
            case PHASE_MOD:
                fmvoldamp =
                    powf(440.0f / getvoicebasefreq(nvoice),
                         pars->VoicePar[nvoice].PFMVolumeDamp / 64.0f);
                NoteVoicePar[nvoice].FMVolume =
                    (expf(pars->VoicePar[nvoice].PFMVolume / 127.0f
                         * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;
            case FREQ_MOD:
                NoteVoicePar[nvoice].FMVolume =
                    (expf(pars->VoicePar[nvoice].PFMVolume / 127.0f
                         * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;
                //    case PITCH_MOD:NoteVoicePar[nvoice].FMVolume=(pars->VoicePar[nvoice].PFMVolume/127.0*8.0)*fmvoldamp;//???????????
                //	          break;
            default:
                if (fmvoldamp > 1.0)
                    fmvoldamp = 1.0;
                NoteVoicePar[nvoice].FMVolume =
                    pars->VoicePar[nvoice].PFMVolume / 127.0 * fmvoldamp;
        }

        // Voice's modulator velocity sensing
        NoteVoicePar[nvoice].FMVolume *=
            VelF(velocity, partparams->VoicePar[nvoice].PFMVelocityScaleFunction);

        FMoldsmp[nvoice] = 0.0; // this is for FM (integration)

        firsttick[nvoice] = 1;
        NoteVoicePar[nvoice].DelayTicks =
            (int)((expf(pars->VoicePar[nvoice].PDelay / 127.0f * logf(50.0f))
                   - 1.0) / buffersize / 10.0f * samplerate);
    }

    initparameters();
    ready = 1;
}


// ADlegatonote: This function is (mostly) a copy of ADnote(...) and
// initparameters() stuck together with some lines removed so that it
// only alter the already playing note (to perform legato). It is
// possible I left stuff that is not required for this.
void ADnote::ADlegatonote(float freq_, float velocity_,
                          int portamento_, int midinote_, bool externcall)
{
    ADnoteParameters *pars = partparams;
    // Controller *ctl_=ctl; (an original comment!)

    basefreq = freq_;
    velocity = (velocity_ > 1.0) ? 1.0 : velocity_;
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
                Legato.fade.m = 0.0;
                Legato.msg = LM_FadeIn;
            } else {
                Legato.fade.m = 1.0;
                Legato.msg = LM_FadeOut;
                return;
            }
        }
        if (Legato.msg == LM_ToNorm)
            Legato.msg = LM_Norm;
    }

    NoteGlobalPar.Detune = getdetune(pars->GlobalPar.PDetuneType,
                                     pars->GlobalPar.PCoarseDetune,
                                     pars->GlobalPar.PDetune);
    bandwidthDetuneMultiplier = pars->getBandwidthDetuneMultiplier();

    if (pars->GlobalPar.PPanning == 0)
        NoteGlobalPar.Panning = zynMaster->numRandom();
    else
        NoteGlobalPar.Panning = pars->GlobalPar.PPanning / 128.0;

    NoteGlobalPar.FilterCenterPitch =
        pars->GlobalPar.GlobalFilter->getfreq() // center freq
        + pars->GlobalPar.PFilterVelocityScale / 127.0 * 6.0 // velocity sensing
        * (VelF(velocity, pars->GlobalPar.PFilterVelocityScaleFunction) - 1);

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        if (!NoteVoicePar[nvoice].Enabled)
            continue; // (gf) Stay the same as first note in legato.

        NoteVoicePar[nvoice].fixedfreq = pars->VoicePar[nvoice].Pfixedfreq;
        NoteVoicePar[nvoice].fixedfreqET = pars->VoicePar[nvoice].PfixedfreqET;

        // use the Globalpars.detunetype if the detunetype is 0
        if (pars->VoicePar[nvoice].PDetuneType != 0)
        {
            NoteVoicePar[nvoice].Detune =
                getdetune(pars->VoicePar[nvoice].PDetuneType,
                          pars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getdetune(pars->VoicePar[nvoice].PDetuneType, 0,
                          pars->VoicePar[nvoice].PDetune); // fine detune
        }
        else
        {
            NoteVoicePar[nvoice].Detune =
                getdetune(pars->GlobalPar.PDetuneType,
                          pars->VoicePar[nvoice].PCoarseDetune, 8192); // coarse detune
            NoteVoicePar[nvoice].FineDetune =
                getdetune(pars->GlobalPar.PDetuneType, 0,
                          pars->VoicePar[nvoice].PDetune); // fine detune
        }
        if (pars->VoicePar[nvoice].PFMDetuneType != 0)
        {
            NoteVoicePar[nvoice].FMDetune =
                getdetune(pars->VoicePar[nvoice].PFMDetuneType,
                          pars->VoicePar[nvoice].PFMCoarseDetune,
                          pars->VoicePar[nvoice].PFMDetune);
        }
        else
        {
            NoteVoicePar[nvoice].FMDetune =
                getdetune(pars->GlobalPar.PDetuneType,
                          pars->VoicePar[nvoice].PFMCoarseDetune,
                          pars->VoicePar[nvoice].PFMDetune);
        }

        // Get the voice's oscil or external's voice oscil
        int vc = nvoice;
        if (pars->VoicePar[nvoice].Pextoscil != -1)
            vc = pars->VoicePar[nvoice].Pextoscil;
        if (!pars->GlobalPar.Hrandgrouping)
            pars->VoicePar[vc].OscilSmp->newrandseed();

        pars->VoicePar[vc].OscilSmp->get(NoteVoicePar[nvoice].OscilSmp,
                                         getvoicebasefreq(nvoice),
                                                          pars->VoicePar[nvoice].Presonance); // (gf)Modif of the above line.

        // I store the first elments to the last position for speedups
        for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
            NoteVoicePar[nvoice].OscilSmp[oscilsize + i] =
                NoteVoicePar[nvoice].OscilSmp[i];

        NoteVoicePar[nvoice].FilterCenterPitch =
            pars->VoicePar[nvoice].VoiceFilter->getfreq();
        NoteVoicePar[nvoice].filterbypass =
            pars->VoicePar[nvoice].Pfilterbypass;

        NoteVoicePar[nvoice].FMVoice = pars->VoicePar[nvoice].PFMVoice;

        // Compute the Voice's modulator volume (incl. damping)
        float fmvoldamp =
            powf(440.0f / getvoicebasefreq(nvoice),
                pars->VoicePar[nvoice].PFMVolumeDamp / 64.0f - 1.0f);

        switch (NoteVoicePar[nvoice].FMEnabled)
        {
            case PHASE_MOD:
                fmvoldamp =
                    powf(440.0f / getvoicebasefreq(nvoice),
                        pars->VoicePar[nvoice].PFMVolumeDamp / 64.0f);
                NoteVoicePar[nvoice].FMVolume =
                    (expf(pars->VoicePar[nvoice].PFMVolume / 127.0f
                         * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;
            case FREQ_MOD:
                NoteVoicePar[nvoice].FMVolume =
                    (expf(pars->VoicePar[nvoice].PFMVolume / 127.0f
                         * FM_AMP_MULTIPLIER) - 1.0f) * fmvoldamp * 4.0f;
                break;
                //    case PITCH_MOD:NoteVoicePar[nvoice].FMVolume=(pars->VoicePar[nvoice].PFMVolume/127.0*8.0)*fmvoldamp;//???????????
                //	          break;
            default:
                if (fmvoldamp > 1.0)
                    fmvoldamp = 1.0;
                NoteVoicePar[nvoice].FMVolume =
                    pars->VoicePar[nvoice].PFMVolume / 127.0 * fmvoldamp;
        }

        // Voice's modulator velocity sensing
        NoteVoicePar[nvoice].FMVolume *=
            VelF(velocity, partparams->VoicePar[nvoice].PFMVelocityScaleFunction);

        NoteVoicePar[nvoice].DelayTicks =
            (int)((exp(pars->VoicePar[nvoice].PDelay / 127.0f * logf(50.0f)) - 1.0)
                   / buffersize / 10.0 * samplerate);
    }

    ///    initparameters();

    ///////////////
    // Altered content of initparameters():

    int nvoice, i, tmp[NUM_VOICES];

    NoteGlobalPar.Volume =
        4.0 * powf(0.1f, 3.0f * (1.0f - partparams->GlobalPar.PVolume / 96.0f)) // -60 dB .. 0 dB
        * VelF(velocity, partparams->GlobalPar.PAmpVelocityScaleFunction); // velocity sensing

    globalnewamplitude =
        NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB()
        * NoteGlobalPar.AmpLfo->amplfoout();

    NoteGlobalPar.FilterQ = partparams->GlobalPar.GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking =
        partparams->GlobalPar.GlobalFilter->getfreqtracking(basefreq);

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
        NoteVoicePar[nvoice].Volume =
            powf(0.1f, 3.0f * (1.0f - partparams->VoicePar[nvoice].PVolume / 127.0f)) // -60 dB .. 0 dB
                * VelF(velocity, partparams->VoicePar[nvoice].PAmpVelocityScaleFunction); // velocity
        if (partparams->VoicePar[nvoice].PVolumeminus != 0)
            NoteVoicePar[nvoice].Volume = -NoteVoicePar[nvoice].Volume;

        if (partparams->VoicePar[nvoice].PPanning == 0)
            NoteVoicePar[nvoice].Panning = zynMaster->numRandom(); // random panning
        else
            NoteVoicePar[nvoice].Panning=partparams->VoicePar[nvoice].PPanning / 128.0;

        newamplitude[nvoice] = 1.0;
        if (partparams->VoicePar[nvoice].PAmpEnvelopeEnabled
            && NoteVoicePar[nvoice].AmpEnvelope != NULL)
        {
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();
        }

        if (partparams->VoicePar[nvoice].PAmpLfoEnabled
            && NoteVoicePar[nvoice].AmpLfo != NULL)
        {
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();
        }

        NoteVoicePar[nvoice].FilterFreqTracking =
            partparams->VoicePar[nvoice].VoiceFilter->getfreqtracking(basefreq);

        // Voice Modulation Parameters Init
        if (NoteVoicePar[nvoice].FMEnabled != NONE
            && NoteVoicePar[nvoice].FMVoice < 0)
        {
            partparams->VoicePar[nvoice].FMSmp->newrandseed();

            // Perform Anti-aliasing only on MORPH or RING MODULATION
            int vc = nvoice;
            if (partparams->VoicePar[nvoice].PextFMoscil != -1)
                vc = partparams->VoicePar[nvoice].PextFMoscil;

            float tmp = 1.0;
            if (partparams->VoicePar[vc].FMSmp->Padaptiveharmonics != 0
                || NoteVoicePar[nvoice].FMEnabled == MORPH
                || NoteVoicePar[nvoice].FMEnabled == RING_MOD) {
                tmp = getFMvoicebasefreq(nvoice);
            }
            if (!partparams->GlobalPar.Hrandgrouping)
                partparams->VoicePar[vc].FMSmp->newrandseed();

            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].FMSmp[oscilsize + i] =
                    NoteVoicePar[nvoice].FMSmp[i];
        }

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;
        if (partparams->VoicePar[nvoice].PFMAmpEnvelopeEnabled
            && NoteVoicePar[nvoice].FMAmpEnvelope != NULL)
        {
            FMnewamplitude[nvoice] *= NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
        }
    }

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            tmp[i] = 0;
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            if (NoteVoicePar[i].FMVoice == nvoice && tmp[i] == 0)
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

    delete [] NoteVoicePar[nvoice].OscilSmp;

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

    if (NoteVoicePar[nvoice].VoiceFilter != NULL)
        delete NoteVoicePar[nvoice].VoiceFilter;
    NoteVoicePar[nvoice].VoiceFilter = NULL;

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

    if (NoteVoicePar[nvoice].FMEnabled != NONE
        && NoteVoicePar[nvoice].FMVoice < 0)
        delete [] NoteVoicePar[nvoice].FMSmp;

    if (NoteVoicePar[nvoice].VoiceOut != NULL)
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
        KillNote();
    delete [] tmpwave;
    delete [] bypassl;
    delete [] bypassr;
}


// Init the parameters
void ADnote::initparameters()
{
    int nvoice, i, tmp[NUM_VOICES];

    // Global Parameters
    NoteGlobalPar.FreqEnvelope =
        new Envelope(partparams->GlobalPar.FreqEnvelope, basefreq);
    NoteGlobalPar.FreqLfo = new LFO(partparams->GlobalPar.FreqLfo, basefreq);

    NoteGlobalPar.AmpEnvelope =
        new Envelope(partparams->GlobalPar.AmpEnvelope, basefreq);
    NoteGlobalPar.AmpLfo = new LFO(partparams->GlobalPar.AmpLfo, basefreq);

    NoteGlobalPar.Volume =
        4.0 * powf(0.1f, 3.0f * (1.0f - partparams->GlobalPar.PVolume / 96.0f)) // -60 dB .. 0 dB
        * VelF(velocity, partparams->GlobalPar.PAmpVelocityScaleFunction); // velocity sensing

    NoteGlobalPar.AmpEnvelope->envout_dB(); // discard the first envelope output
    globalnewamplitude =
        NoteGlobalPar.Volume * NoteGlobalPar.AmpEnvelope->envout_dB()
        * NoteGlobalPar.AmpLfo->amplfoout();

    NoteGlobalPar.GlobalFilterL =
        new Filter(partparams->GlobalPar.GlobalFilter);
    if (stereo)
        NoteGlobalPar.GlobalFilterR = new Filter(partparams->GlobalPar.GlobalFilter);

    NoteGlobalPar.FilterEnvelope = new Envelope(partparams->GlobalPar.FilterEnvelope,
                                                basefreq);
    NoteGlobalPar.FilterLfo = new LFO(partparams->GlobalPar.FilterLfo, basefreq);
    NoteGlobalPar.FilterQ = partparams->GlobalPar.GlobalFilter->getq();
    NoteGlobalPar.FilterFreqTracking =
        partparams->GlobalPar.GlobalFilter->getfreqtracking(basefreq);

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
        NoteVoicePar[nvoice].Volume =
            powf(0.1f, 3.0f * (1.0f - partparams->VoicePar[nvoice].PVolume / 127.0f)) // -60 dB .. 0 dB
            * VelF(velocity,partparams->VoicePar[nvoice].PAmpVelocityScaleFunction); // velocity

        if (partparams->VoicePar[nvoice].PVolumeminus != 0)
            NoteVoicePar[nvoice].Volume = -NoteVoicePar[nvoice].Volume;

        if (partparams->VoicePar[nvoice].PPanning == 0)
            NoteVoicePar[nvoice].Panning = zynMaster->numRandom(); // random panning
        else
            NoteVoicePar[nvoice].Panning =
                partparams->VoicePar[nvoice].PPanning / 128.0;

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
            NoteVoicePar[nvoice].VoiceFilter =
                new Filter(partparams->VoicePar[nvoice].VoiceFilter);
        }

        if (partparams->VoicePar[nvoice].PFilterEnvelopeEnabled)
            NoteVoicePar[nvoice].FilterEnvelope =
                new Envelope(partparams->VoicePar[nvoice].FilterEnvelope, basefreq);

        if (partparams->VoicePar[nvoice].PFilterLfoEnabled != 0)
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
                new float[oscilsize + OSCIL_SMP_EXTRA_SAMPLES];

            // Perform Anti-aliasing only on MORPH or RING MODULATION

            int vc = nvoice;
            if (partparams->VoicePar[nvoice].PextFMoscil != -1)
                vc = partparams->VoicePar[nvoice].PextFMoscil;

            float tmp = 1.0;
            if (partparams->VoicePar[vc].FMSmp->Padaptiveharmonics != 0
                || NoteVoicePar[nvoice].FMEnabled == MORPH
                || NoteVoicePar[nvoice].FMEnabled == RING_MOD)
            {
                tmp = getFMvoicebasefreq(nvoice);
            }
            if (!partparams->GlobalPar.Hrandgrouping)
                partparams->VoicePar[vc].FMSmp->newrandseed();

            oscposhiFM[nvoice] =
                (oscposhi[nvoice] + partparams->VoicePar[vc].FMSmp->get(NoteVoicePar[nvoice].FMSmp,
                 tmp)) % oscilsize;
            for (int i = 0; i < OSCIL_SMP_EXTRA_SAMPLES; ++i)
                NoteVoicePar[nvoice].FMSmp[oscilsize + i] =
                    NoteVoicePar[nvoice].FMSmp[i];
            oscposhiFM[nvoice] +=
                (int)((partparams->VoicePar[nvoice].PFMoscilphase - 64.0)
                      / 128.0 * oscilsize + oscilsize * 4);
            oscposhiFM[nvoice] %= oscilsize;
        }

        if (partparams->VoicePar[nvoice].PFMFreqEnvelopeEnabled)
            NoteVoicePar[nvoice].FMFreqEnvelope =
                new Envelope(partparams->VoicePar[nvoice].FMFreqEnvelope, basefreq);

        FMnewamplitude[nvoice] = NoteVoicePar[nvoice].FMVolume * ctl->fmamp.relamp;

        if (partparams->VoicePar[nvoice].PFMAmpEnvelopeEnabled)
        {
            NoteVoicePar[nvoice].FMAmpEnvelope =
                new Envelope(partparams->VoicePar[nvoice].FMAmpEnvelope, basefreq);
            FMnewamplitude[nvoice] *= NoteVoicePar[nvoice].FMAmpEnvelope->envout_dB();
        }
    }

    for (nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
    {
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
            tmp[i] = 0;
        for (i = nvoice + 1; i < NUM_VOICES; ++i)
        {
            if (NoteVoicePar[i].FMVoice == nvoice && tmp[i] == 0)
            {
                NoteVoicePar[nvoice].VoiceOut = new float[buffersize];
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
        NoteVoicePar[nvoice].Detune / 100.0 + NoteVoicePar[nvoice].FineDetune /
            100.0 * ctl->bandwidth.relbw * bandwidthDetuneMultiplier
                + NoteGlobalPar.Detune / 100.0;

    if (NoteVoicePar[nvoice].fixedfreq == 0)
        return basefreq * powf(2.0f, detune / 12.0f);
    else
    {   // the fixed freq is enabled
        float fixedfreq = 440.0;
        int fixedfreqET = NoteVoicePar[nvoice].fixedfreqET;
        if (fixedfreqET != 0)
        {   // if the frequency varies according the keyboard note
            float tmp = (midinote - 69.0) / 12.0
                * (powf(2.0f, (fixedfreqET - 1) / 63.0f) - 1.0);
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
        0.01 * (NoteGlobalPar.FreqEnvelope->envout()
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
    if (stereo != 0)
        NoteGlobalPar.GlobalFilterR->setfreq_and_q(tmpfilterfreq, globalfilterq);

    // compute the portamento, if it is used by this note
    float portamentofreqrap = 1.0;
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
        newamplitude[nvoice] = 1.0;

        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpEnvelope->envout_dB();

        if (NoteVoicePar[nvoice].AmpLfo != NULL)
            newamplitude[nvoice] *= NoteVoicePar[nvoice].AmpLfo->amplfoout();

        //=============
        // Voice Filter

        if (NoteVoicePar[nvoice].VoiceFilter != NULL)
        {
            filterpitch = NoteVoicePar[nvoice].FilterCenterPitch;

            if (NoteVoicePar[nvoice].FilterEnvelope != NULL)
                filterpitch += NoteVoicePar[nvoice].FilterEnvelope->envout();

            if (NoteVoicePar[nvoice].FilterLfo != NULL)
                filterpitch += NoteVoicePar[nvoice].FilterLfo->lfoout();

            filterfreq = filterpitch + NoteVoicePar[nvoice].FilterFreqTracking;
            filterfreq = NoteVoicePar[nvoice].VoiceFilter->getrealfreq(filterfreq);

            NoteVoicePar[nvoice].VoiceFilter->setfreq(filterfreq);
        }

        if (NoteVoicePar[nvoice].noisetype == 0)
        {   // compute only if the voice isn't noise

            //==================
            // Voice Frequency

            voicepitch = 0.0;
            if (NoteVoicePar[nvoice].FreqLfo != NULL)
                voicepitch += NoteVoicePar[nvoice].FreqLfo->lfoout() / 100.0
                              * ctl->bandwidth.relbw;

            if (NoteVoicePar[nvoice].FreqEnvelope != NULL)
                voicepitch += NoteVoicePar[nvoice].FreqEnvelope->envout() / 100.0;
            voicefreq = getvoicebasefreq(nvoice)
                        * powf(2.0f, (voicepitch + globalpitch) / 12.0f); // Hz frequency
            voicefreq *= ctl->pitchwheel.relfreq; // change the frequency by the controller
            setfreq(nvoice, voicefreq * portamentofreqrap);

            //==================
            //  Modulator

            if (NoteVoicePar[nvoice].FMEnabled != NONE)
            {
                FMrelativepitch = NoteVoicePar[nvoice].FMDetune / 100.0;
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
        if (smps[i - 1] < 0.0 && smps[i] > 0.0)
            zerocrossings++; // this is only the possitive crossings

    float tmp = (buffersize - 1.0) / (zerocrossings + 1) / 3.0;
    if (tmp < 8.0)
        tmp = 8.0;

    int n;
    F2I(tmp, n); // how many samples is the fade-in
    if (n > buffersize)
        n = buffersize;
    for (int i = 0; i < n; ++i)
    {   // fade-in
        float tmp = 0.5 - cosf((float)i / (float)n * PI) * 0.5;
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
        tmpwave[i] = smps[poshi] * (1.0 - poslo) + smps[poshi + 1] * poslo;
        poslo += oscfreqlo[nvoice];
        if (poslo >= 1.0)
        {
            poslo -= 1.0;
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
    FMnewamplitude[nvoice] = (FMnewamplitude[nvoice] > 1.0) ? 1.0 : FMnewamplitude[nvoice];

    FMoldamplitude[nvoice] = (FMoldamplitude[nvoice] > 1.0) ? 1.0 : FMoldamplitude[nvoice];
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
            if (posloFM >= 1.0)
            {
                posloFM -= 1.0;
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
    if (FMnewamplitude[nvoice] > 1.0)
        FMnewamplitude[nvoice] = 1.0;
    if (FMoldamplitude[nvoice] > 1.0)
        FMoldamplitude[nvoice] = 1.0;
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
                tmpwave[j] *= (1.0 - amp) + amp * NoteVoicePar[FMVoice].VoiceOut[j];
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
            tmpwave[i] *= (NoteVoicePar[nvoice].FMSmp[poshiFM] * (1.0 - posloFM)
                          + NoteVoicePar[nvoice].FMSmp[poshiFM + 1] * posloFM)
                          * amp + (1.0 - amp);
            posloFM += oscfreqloFM[nvoice];
            if (posloFM >= 1.0)
            {
                posloFM -= 1.0;
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
            tmpwave[i] = (NoteVoicePar[nvoice].FMSmp[poshiFM] * (1.0 - posloFM)
                         + NoteVoicePar[nvoice].FMSmp[poshiFM + 1] * posloFM);
            posloFM += oscfreqloFM[nvoice];
            if (posloFM >= 1.0)
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
        float normalize = (float)oscilsize / 262144.0 * 44100.0 / (float)samplerate;
        for (int i = 0; i < buffersize; ++i)
        {
            FMoldsmp[nvoice] = fmodf(FMoldsmp[nvoice] + tmpwave[i] * normalize,
                                     oscilsize);
            tmpwave[i] = FMoldsmp[nvoice];
        }
    }
    else
    {    // Phase modulation
        float normalize = (float)oscilsize / 262144.0;
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

        if (carposlo >= 1.0)
        {
            carposhi++;
            carposlo = fmodf(carposlo, 1.0f);
        }
        carposhi &= (oscilsize - 1);

        tmpwave[i] = NoteVoicePar[nvoice].OscilSmp[carposhi] * (1.0 - carposlo)
                     + NoteVoicePar[nvoice].OscilSmp[carposhi + 1] * carposlo;

        oscposlo[nvoice] += oscfreqlo[nvoice];
        if (oscposlo[nvoice] >= 1.0)
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
                tmpwave[i] = zynMaster->numRandom() * 2.0 - 1.0;

        // Voice Processing ...

        // Amplitude
        if (AboveAmplitudeThreshold(oldamplitude[nvoice], newamplitude[nvoice]))
        {
            int rest = buffersize;
            // test if the amplitude if raising and the difference is high
            if (newamplitude[nvoice] > oldamplitude[nvoice]
                && (newamplitude[nvoice] - oldamplitude[nvoice]) > 0.25)
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
        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
        {
            if (NoteVoicePar[nvoice].AmpEnvelope->finished() != 0)
                for (int i = 0; i < buffersize; ++i)
                    tmpwave[i] *= 1.0 - (float)i / (float)buffersize;
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
                           * (1.0 - NoteVoicePar[nvoice].Panning) * 2.0;
                outr[i] += tmpwave[i] * NoteVoicePar[nvoice].Volume
                           * NoteVoicePar[nvoice].Panning * 2.0;
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
                              * (1.0 - NoteVoicePar[nvoice].Panning) * 2.0;
                bypassr[i] += tmpwave[i] * NoteVoicePar[nvoice].Volume
                              * NoteVoicePar[nvoice].Panning * 2.0;
            }
        }
        // chech if there is necesary to proces the voice longer (if the Amplitude envelope isn't finished)
        if (NoteVoicePar[nvoice].AmpEnvelope != NULL)
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
            outl[i] *= tmpvol * (1.0 - NoteGlobalPar.Panning);
            outr[i] *= tmpvol * NoteGlobalPar.Panning;
        }
    } else {
        for (int i = 0; i < buffersize; ++i)
        {
            outl[i] *= globalnewamplitude * (1.0 - NoteGlobalPar.Panning);
            outr[i] *= globalnewamplitude * NoteGlobalPar.Panning;
        }
    }

    // Apply the punch
    if (NoteGlobalPar.Punch.Enabled)
    {
        for (int i = 0; i < buffersize; ++i)
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
    if (NoteGlobalPar.AmpEnvelope->finished() != 0)
    {
        for (int i = 0; i < buffersize; ++i)
        {   // fade-out
            float tmp = 1.0 - (float)i / (float)buffersize;
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
