/*
    ADnoteParameters.cpp - Parameters for ADnote (ADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019 Will Godfrey
    Copyright 2020-2023 Kristian Amlie, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code.
*/

#include <iostream>

#include <cmath>
#include <stdlib.h>

#include "Params/ADnoteParameters.h"
#include "Misc/SynthEngine.h"
#include "Misc/NumericFuncs.h"
#include "Misc/XMLStore.h"

using func::setAllPan;
using func::power;


int ADnoteParameters::ADnote_unison_sizes[] =
{2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 40, 50, 0};

ADnoteParameters::ADnoteParameters(fft::Calc& fft_, SynthEngine& _synth)
    : ParamBase{_synth}
    , fft(fft_)
{
    GlobalPar.FreqEnvelope = new EnvelopeParams(0, 0, synth);
    GlobalPar.FreqEnvelope->ASRinit(64, 50, 64, 60);
    GlobalPar.FreqLfo = new LFOParams(70, 0, 64, 0, 0, 0, false, 0, synth);

    GlobalPar.AmpEnvelope = new EnvelopeParams(64, 1, synth);
    GlobalPar.AmpEnvelope->ADSRinit_dB(0, 40, 127, 25);
    GlobalPar.AmpLfo = new LFOParams(80, 0, 64, 0, 0, 0, false, 1, synth);

    GlobalPar.GlobalFilter = new FilterParams(2, 94, 40, 0, synth);
    GlobalPar.FilterEnvelope = new EnvelopeParams(0, 1, synth);
    GlobalPar.FilterEnvelope->ADSRinit_filter(64, 40, 64, 70, 60, 64);
    GlobalPar.FilterLfo = new LFOParams(80, 0, 64, 0, 0, 0, false, 2, synth);
    GlobalPar.Reson = new Resonance(synth);

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        enableVoice(nvoice);
    defaults();
}


void ADnoteParameters::defaults()
{
    // Frequency Global Parameters
    GlobalPar.PStereo = true; // stereo
    GlobalPar.PDetune = 8192; // zero
    GlobalPar.PCoarseDetune = 0;
    GlobalPar.PDetuneType = 1;
    GlobalPar.FreqEnvelope->defaults();
    GlobalPar.FreqLfo->defaults();
    GlobalPar.PBandwidth = 64;

    // Amplitude Global Parameters
    GlobalPar.PVolume = 90;
    setGlobalPan(GlobalPar.PPanning = 64, synth.getRuntime().panLaw); // center
    GlobalPar.PAmpVelocityScaleFunction = 64;
    GlobalPar.PRandom = false;
    GlobalPar.PWidth = 63;
    GlobalPar.AmpEnvelope->defaults();
    GlobalPar.AmpLfo->defaults();
    GlobalPar.Fadein_adjustment = FADEIN_ADJUSTMENT_SCALE;
    GlobalPar.PPunchStrength = 0;
    GlobalPar.PPunchTime = 60;
    GlobalPar.PPunchStretch = 64;
    GlobalPar.PPunchVelocitySensing = 72;
    GlobalPar.Hrandgrouping = 0;

    // Filter Global Parameters
    GlobalPar.PFilterVelocityScale = 64;
    GlobalPar.PFilterVelocityScaleFunction = 64;
    GlobalPar.GlobalFilter->defaults();
    GlobalPar.FilterEnvelope->defaults();
    GlobalPar.FilterLfo->defaults();
    GlobalPar.Reson->defaults();

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        defaults(nvoice);
    VoicePar[0].Enabled = true;
}


// Defaults a voice
void ADnoteParameters::defaults(const uint nvoice)
{
    VoicePar[nvoice].Enabled = false;

    VoicePar[nvoice].Unison_size = 1;
    VoicePar[nvoice].Unison_frequency_spread = 60;
    VoicePar[nvoice].Unison_stereo_spread = 64;
    VoicePar[nvoice].Unison_vibrato = 64;
    VoicePar[nvoice].Unison_vibrato_speed = 64;
    VoicePar[nvoice].Unison_invert_phase = 0;
    VoicePar[nvoice].Unison_phase_randomness = 127;

    VoicePar[nvoice].Type = 0;
    VoicePar[nvoice].Pfixedfreq = false;
    VoicePar[nvoice].PfixedfreqET = 0;
    VoicePar[nvoice].PBendAdjust = 88; // 64 + 24
    VoicePar[nvoice].POffsetHz   = 64;
    VoicePar[nvoice].Presonance  = false;
    VoicePar[nvoice].Pfilterbypass = false;
    VoicePar[nvoice].Pextoscil = -1;
    VoicePar[nvoice].PextFMoscil = -1;
    VoicePar[nvoice].Poscilphase = 64;
    VoicePar[nvoice].PFMoscilphase = 64;
    VoicePar[nvoice].PDelay = 0;
    VoicePar[nvoice].PVolume = 100;
    VoicePar[nvoice].PVolumeminus = 0;
    setVoicePan(nvoice, VoicePar[nvoice].PPanning = 64, synth.getRuntime().panLaw); // center
    VoicePar[nvoice].PRandom = false;
    VoicePar[nvoice].PWidth = 63;
    VoicePar[nvoice].PDetune = 8192; // 8192 = 0
    VoicePar[nvoice].PCoarseDetune = 0;
    VoicePar[nvoice].PDetuneType = 0;
    VoicePar[nvoice].PFreqLfoEnabled = false;
    VoicePar[nvoice].PFreqEnvelopeEnabled = false;
    VoicePar[nvoice].PAmpEnvelopeEnabled = false;
    VoicePar[nvoice].PAmpLfoEnabled = false;
    VoicePar[nvoice].PAmpVelocityScaleFunction = 127;
    VoicePar[nvoice].PFilterEnabled = false;
    VoicePar[nvoice].PFilterEnvelopeEnabled = false;
    VoicePar[nvoice].PFilterLfoEnabled = false;
    VoicePar[nvoice].PFilterVelocityScale = 0;
    VoicePar[nvoice].PFilterVelocityScaleFunction = 64;
    VoicePar[nvoice].PFMEnabled = 0;
    VoicePar[nvoice].PFMEnabled = 0;
    VoicePar[nvoice].PFMringToSide = false;
    VoicePar[nvoice].PFMFixedFreq = false;

    // I use the internal oscillator (-1)
    VoicePar[nvoice].PVoice = -1;
    VoicePar[nvoice].PFMVoice = -1;

    VoicePar[nvoice].PFMVolume = 90;
    VoicePar[nvoice].PFMVolumeDamp = 64;
    VoicePar[nvoice].PFMDetuneFromBaseOsc = 1;
    VoicePar[nvoice].PFMDetune = 8192;
    VoicePar[nvoice].PFMCoarseDetune = 0;
    VoicePar[nvoice].PFMDetuneType = 0;
    VoicePar[nvoice].PFMFreqEnvelopeEnabled = false;
    VoicePar[nvoice].PFMAmpEnvelopeEnabled = false;
    VoicePar[nvoice].PFMVelocityScaleFunction = 64;

    VoicePar[nvoice].POscil->defaults();
    VoicePar[nvoice].POscilFM->defaults();

    VoicePar[nvoice].AmpEnvelope->defaults();
    VoicePar[nvoice].AmpLfo->defaults();

    VoicePar[nvoice].FreqEnvelope->defaults();
    VoicePar[nvoice].FreqLfo->defaults();

    VoicePar[nvoice].VoiceFilter->defaults();
    VoicePar[nvoice].FilterEnvelope->defaults();
    VoicePar[nvoice].FilterLfo->defaults();

    VoicePar[nvoice].FMFreqEnvelope->defaults();
    VoicePar[nvoice].FMAmpEnvelope->defaults();
}


// Init the voice parameters
void ADnoteParameters::enableVoice(int nvoice)
{
    VoicePar[nvoice].POscil = new OscilParameters(fft, synth);
    VoicePar[nvoice].POscilFM = new OscilParameters(fft, synth);
    VoicePar[nvoice].OscilSmp = new OscilGen(fft, GlobalPar.Reson, &synth, VoicePar[nvoice].POscil);
    VoicePar[nvoice].FMSmp = new OscilGen(fft, NULL, &synth, VoicePar[nvoice].POscilFM);

    VoicePar[nvoice].AmpEnvelope = new EnvelopeParams(64, 1, synth);
    VoicePar[nvoice].AmpEnvelope->ADSRinit_dB(0, 100, 127, 100);
    VoicePar[nvoice].AmpLfo = new LFOParams(90, 32, 64, 0, 0, 30, false, 1, synth);

    VoicePar[nvoice].FreqEnvelope = new EnvelopeParams(0, 0, synth);
    VoicePar[nvoice].FreqEnvelope->ASRinit(30, 40, 64, 60);
    VoicePar[nvoice].FreqLfo = new LFOParams(50, 40, 0, 0, 0, 0, false, 0, synth);

    VoicePar[nvoice].VoiceFilter = new FilterParams(2, 50, 60, 0, synth);
    VoicePar[nvoice].FilterEnvelope = new EnvelopeParams(0, 0, synth);
    VoicePar[nvoice].FilterEnvelope->ADSRinit_filter(90, 70, 40, 70, 10, 40);
    VoicePar[nvoice].FilterLfo = new LFOParams(50, 20, 64, 0, 0, 0, false, 2, synth);

    VoicePar[nvoice].FMFreqEnvelope = new EnvelopeParams(0, 0, synth);
    VoicePar[nvoice].FMFreqEnvelope->ASRinit(20, 90, 40, 80);
    VoicePar[nvoice].FMAmpEnvelope = new EnvelopeParams(64, 1, synth);
    VoicePar[nvoice].FMAmpEnvelope->ADSRinit(80, 90, 127, 100);
}


// Get the Multiplier of the fine detunes of the voices
float ADnoteParameters::getBandwidthDetuneMultiplier()
{
    float bw = (GlobalPar.PBandwidth - 64.0f) / 64.0f;
    bw = power<2>(bw * pow(fabs(bw), 0.2f) * 5.0f);
    return bw;
}


// Get the unison spread in cents for a voice
float ADnoteParameters::getUnisonFrequencySpreadCents(int nvoice)
{
    float unison_spread = VoicePar[nvoice].Unison_frequency_spread / 127.0f;
    unison_spread = powf(unison_spread * 2.0f, 2.0f) * 50.0f; // cents
    return unison_spread;
}


// Kill the voice
void ADnoteParameters::killVoice(int nvoice)
{
    delete VoicePar[nvoice].OscilSmp;
    delete VoicePar[nvoice].FMSmp;
    delete VoicePar[nvoice].POscil;
    delete VoicePar[nvoice].POscilFM;

    delete VoicePar[nvoice].AmpEnvelope;
    delete VoicePar[nvoice].AmpLfo;

    delete VoicePar[nvoice].FreqEnvelope;
    delete VoicePar[nvoice].FreqLfo;

    delete VoicePar[nvoice].VoiceFilter;
    delete VoicePar[nvoice].FilterEnvelope;
    delete VoicePar[nvoice].FilterLfo;

    delete VoicePar[nvoice].FMFreqEnvelope;
    delete VoicePar[nvoice].FMAmpEnvelope;
}


ADnoteParameters::~ADnoteParameters()
{
    delete GlobalPar.FreqEnvelope;
    delete GlobalPar.FreqLfo;
    delete GlobalPar.AmpEnvelope;
    delete GlobalPar.AmpLfo;
    delete GlobalPar.GlobalFilter;
    delete GlobalPar.FilterEnvelope;
    delete GlobalPar.FilterLfo;
    delete GlobalPar.Reson;

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        killVoice(nvoice);
}


void ADnoteParameters::setGlobalPan(char pan, unsigned char panLaw)
{
    GlobalPar.PPanning = pan;
    if (!GlobalPar.PRandom)
        setAllPan(GlobalPar.PPanning, GlobalPar.pangainL, GlobalPar.pangainR, panLaw);
    else
        GlobalPar.pangainL = GlobalPar.pangainR = 0.7f;
}


void ADnoteParameters::setVoicePan(int nvoice, char pan, unsigned char panLaw)
{
    VoicePar[nvoice].PPanning = pan;
    if (!VoicePar[nvoice].PRandom)
        setAllPan(VoicePar[nvoice].PPanning, VoicePar[nvoice].pangainL, VoicePar[nvoice].pangainR, panLaw);
    else
        VoicePar[nvoice].pangainL = VoicePar[nvoice].pangainR = 0.7f;
}


void ADnoteParameters::add2XML_voice(XMLtree& xmlVoice, const uint nvoice)
{
    if (nvoice >= NUM_VOICES)
        return;
    xmlVoice.addPar_bool("enabled", VoicePar[nvoice].Enabled);

    bool oscil_used_by_other_voice{false};
    bool fmosc_used_by_other_voice{false};
    for (uint i = 0; i < NUM_VOICES; ++i)
    {
        oscil_used_by_other_voice = (int(nvoice) == VoicePar[i].Pextoscil);
        fmosc_used_by_other_voice = (int(nvoice) == VoicePar[i].PextFMoscil);
    }
    if (not (VoicePar[nvoice].Enabled
            or oscil_used_by_other_voice
            or fmosc_used_by_other_voice
            or synth.getRuntime().xmlmax))
        return;

    xmlVoice.addPar_int ("type", VoicePar[nvoice].Type);

    xmlVoice.addPar_int ("unison_size",             VoicePar[nvoice].Unison_size);
    xmlVoice.addPar_int ("unison_frequency_spread", VoicePar[nvoice].Unison_frequency_spread);
    xmlVoice.addPar_int ("unison_stereo_spread",    VoicePar[nvoice].Unison_stereo_spread);
    xmlVoice.addPar_int ("unison_vibratto",         VoicePar[nvoice].Unison_vibrato);
    xmlVoice.addPar_int ("unison_vibratto_speed",   VoicePar[nvoice].Unison_vibrato_speed);
    xmlVoice.addPar_int ("unison_invert_phase",     VoicePar[nvoice].Unison_invert_phase);
    xmlVoice.addPar_int ("unison_phase_randomness", VoicePar[nvoice].Unison_phase_randomness);

    xmlVoice.addPar_int ("delay"          , VoicePar[nvoice].PDelay);
    xmlVoice.addPar_bool("resonance"      , VoicePar[nvoice].Presonance);

    xmlVoice.addPar_int ("input_voice"    , VoicePar[nvoice].PVoice);
    xmlVoice.addPar_int ("ext_oscil"      , VoicePar[nvoice].Pextoscil);
    xmlVoice.addPar_int ("ext_fm_oscil"   , VoicePar[nvoice].PextFMoscil);

    xmlVoice.addPar_int ("oscil_phase"    , VoicePar[nvoice].Poscilphase);
    xmlVoice.addPar_int ("oscil_fm_phase" , VoicePar[nvoice].PFMoscilphase);

    xmlVoice.addPar_bool("filter_enabled" , VoicePar[nvoice].PFilterEnabled);
    xmlVoice.addPar_bool("filter_bypass"  , VoicePar[nvoice].Pfilterbypass);

    xmlVoice.addPar_int ("fm_enabled"     , VoicePar[nvoice].PFMEnabled);

    XMLtree xmlOscil = xmlVoice.addElm("OSCIL");
        VoicePar[nvoice].POscil->add2XML(xmlOscil);


    XMLtree xmlAmp = xmlVoice.addElm("AMPLITUDE_PARAMETERS");
        // Yoshimi format for random panning
        xmlAmp.addPar_int ("pan_pos"     , VoicePar[nvoice].PPanning);
        xmlAmp.addPar_bool("random_pan"  , VoicePar[nvoice].PRandom);
        xmlAmp.addPar_int ("random_width", VoicePar[nvoice].PWidth);

        // support legacy format
        if (VoicePar[nvoice].PRandom)
            xmlAmp.addPar_int("panning", 0);
        else
            xmlAmp.addPar_int("panning", VoicePar[nvoice].PPanning);

        xmlAmp.addPar_int ("volume"              , VoicePar[nvoice].PVolume);
        xmlAmp.addPar_bool("volume_minus"        , VoicePar[nvoice].PVolumeminus);
        xmlAmp.addPar_int ("velocity_sensing"    , VoicePar[nvoice].PAmpVelocityScaleFunction);
        xmlAmp.addPar_bool("amp_envelope_enabled", VoicePar[nvoice].PAmpEnvelopeEnabled);

        if (VoicePar[nvoice].PAmpEnvelopeEnabled or synth.getRuntime().xmlmax)
        {
            XMLtree xmlEnv = xmlAmp.addElm("AMPLITUDE_ENVELOPE");
                VoicePar[nvoice].AmpEnvelope->add2XML(xmlEnv);
        }
        xmlAmp.addPar_bool("amp_lfo_enabled", VoicePar[nvoice].PAmpLfoEnabled);
        if (VoicePar[nvoice].PAmpLfoEnabled or synth.getRuntime().xmlmax)
        {
            XMLtree xmlLFO = xmlAmp.addElm("AMPLITUDE_LFO");
                VoicePar[nvoice].AmpLfo->add2XML(xmlLFO);
        }


    XMLtree xmlFreq = xmlVoice.addElm("FREQUENCY_PARAMETERS");
        xmlFreq.addPar_bool("fixed_freq"   , VoicePar[nvoice].Pfixedfreq);
        xmlFreq.addPar_int ("fixed_freq_et", VoicePar[nvoice].PfixedfreqET);
        xmlFreq.addPar_int ("bend_adjust"  , VoicePar[nvoice].PBendAdjust);
        xmlFreq.addPar_int ("offset_hz"    , VoicePar[nvoice].POffsetHz);
        xmlFreq.addPar_int ("detune"       , VoicePar[nvoice].PDetune);
        xmlFreq.addPar_int ("coarse_detune", VoicePar[nvoice].PCoarseDetune);
        xmlFreq.addPar_int ("detune_type"  , VoicePar[nvoice].PDetuneType);

        xmlFreq.addPar_bool("freq_envelope_enabled", VoicePar[nvoice].PFreqEnvelopeEnabled);
        if (VoicePar[nvoice].PFreqEnvelopeEnabled or synth.getRuntime().xmlmax)
        {
            XMLtree xmlEnv = xmlFreq.addElm("FREQUENCY_ENVELOPE");
                VoicePar[nvoice].FreqEnvelope->add2XML(xmlEnv);
        }
        xmlFreq.addPar_bool("freq_lfo_enabled", VoicePar[nvoice].PFreqLfoEnabled);
        if (VoicePar[nvoice].PFreqLfoEnabled or synth.getRuntime().xmlmax)
        {
            XMLtree xmlLFO = xmlFreq.addElm("FREQUENCY_LFO");
                VoicePar[nvoice].FreqLfo->add2XML(xmlLFO);
        }


    if (VoicePar[nvoice].PFilterEnabled or synth.getRuntime().xmlmax)
    {
        XMLtree xmlFilterParams = xmlVoice.addElm("FILTER_PARAMETERS");
            xmlFilterParams.addPar_int("velocity_sensing_amplitude", VoicePar[nvoice].PFilterVelocityScale);
            xmlFilterParams.addPar_int("velocity_sensing",   VoicePar[nvoice].PFilterVelocityScaleFunction);
            XMLtree xmlFilter = xmlFilterParams.addElm("FILTER");
                VoicePar[nvoice].VoiceFilter->add2XML(xmlFilter);

            xmlFilterParams.addPar_bool("filter_envelope_enabled", VoicePar[nvoice].PFilterEnvelopeEnabled);
            if (VoicePar[nvoice].PFilterEnvelopeEnabled or synth.getRuntime().xmlmax)
            {
                XMLtree xmlEnv = xmlFilterParams.addElm("FILTER_ENVELOPE");
                    VoicePar[nvoice].FilterEnvelope->add2XML(xmlEnv);
            }

            xmlFilterParams.addPar_bool("filter_lfo_enabled", VoicePar[nvoice].PFilterLfoEnabled);
            if (VoicePar[nvoice].PFilterLfoEnabled or synth.getRuntime().xmlmax)
            {
                XMLtree xmlLFO = xmlFilterParams.addElm("FILTER_LFO");
                    VoicePar[nvoice].FilterLfo->add2XML(xmlLFO);
            }
    }

    if (VoicePar[nvoice].PFMEnabled
        or fmosc_used_by_other_voice
        or synth.getRuntime().xmlmax)
    {
        XMLtree xmlFM = xmlVoice.addElm("FM_PARAMETERS");
            xmlFM.addPar_int("input_voice"     , VoicePar[nvoice].PFMVoice);
            xmlFM.addPar_int("volume"          , VoicePar[nvoice].PFMVolume);
            xmlFM.addPar_int("volume_damp"     , VoicePar[nvoice].PFMVolumeDamp);
            xmlFM.addPar_int("velocity_sensing", VoicePar[nvoice].PFMVelocityScaleFunction);

            xmlFM.addPar_bool("amp_envelope_enabled", VoicePar[nvoice].PFMAmpEnvelopeEnabled);
            if (VoicePar[nvoice].PFMAmpEnvelopeEnabled or synth.getRuntime().xmlmax)
            {
                XMLtree xmlEnv = xmlFM.addElm("AMPLITUDE_ENVELOPE");
                    VoicePar[nvoice].FMAmpEnvelope->add2XML(xmlEnv);
            }

            XMLtree xmlMod = xmlFM.addElm("MODULATOR");
                xmlMod.addPar_bool("detune_from_base_osc", VoicePar[nvoice].PFMDetuneFromBaseOsc);
                xmlMod.addPar_int ("detune"              , VoicePar[nvoice].PFMDetune);
                xmlMod.addPar_int ("coarse_detune"       , VoicePar[nvoice].PFMCoarseDetune);
                xmlMod.addPar_int ("detune_type"         , VoicePar[nvoice].PFMDetuneType);
                xmlMod.addPar_bool("fixed_freq"          , VoicePar[nvoice].PFMFixedFreq);

                xmlMod.addPar_bool("freq_envelope_enabled", VoicePar[nvoice].PFMFreqEnvelopeEnabled);
                if (VoicePar[nvoice].PFMFreqEnvelopeEnabled or synth.getRuntime().xmlmax)
                {
                    XMLtree xmlEnv = xmlMod.addElm("FREQUENCY_ENVELOPE");
                        VoicePar[nvoice].FMFreqEnvelope->add2XML(xmlEnv);
                }

                XMLtree xmlOscil = xmlMod.addElm("OSCIL");
                    VoicePar[nvoice].POscilFM->add2XML(xmlOscil);
    }
}


void ADnoteParameters::add2XML(XMLtree& xmlAddSynth)
{
    xmlAddSynth.addPar_bool("stereo", GlobalPar.PStereo);

    XMLtree xmlAmp = xmlAddSynth.addElm("AMPLITUDE_PARAMETERS");
    {
        xmlAmp.addPar_int ("volume"      , GlobalPar.PVolume);
        // Yoshimi format for random panning
        xmlAmp.addPar_int ("pan_pos"     , GlobalPar.PPanning);
        xmlAmp.addPar_bool("random_pan"  , GlobalPar.PRandom);
        xmlAmp.addPar_int ("random_width", GlobalPar.PWidth);

        // support legacy format
        if (GlobalPar.PRandom)
            xmlAmp.addPar_int("panning", 0);
        else
            xmlAmp.addPar_int("panning", GlobalPar.PPanning);

        xmlAmp.addPar_int("velocity_sensing"      , GlobalPar.PAmpVelocityScaleFunction);
        xmlAmp.addPar_int("fadein_adjustment"     , GlobalPar.Fadein_adjustment);
        xmlAmp.addPar_int("punch_strength"        , GlobalPar.PPunchStrength);
        xmlAmp.addPar_int("punch_time"            , GlobalPar.PPunchTime);
        xmlAmp.addPar_int("punch_stretch"         , GlobalPar.PPunchStretch);
        xmlAmp.addPar_int("punch_velocity_sensing", GlobalPar.PPunchVelocitySensing);
        xmlAmp.addPar_int("harmonic_randomness_grouping", GlobalPar.Hrandgrouping);

        XMLtree xmlEnv = xmlAmp.addElm("AMPLITUDE_ENVELOPE");
            GlobalPar.AmpEnvelope->add2XML(xmlEnv);

        XMLtree xmlLFO = xmlAmp.addElm("AMPLITUDE_LFO");
            GlobalPar.AmpLfo->add2XML(xmlLFO);
    }

    XMLtree xmlFreq = xmlAddSynth.addElm("FREQUENCY_PARAMETERS");
    {
        xmlFreq.addPar_int("detune"       , GlobalPar.PDetune);
        xmlFreq.addPar_int("coarse_detune", GlobalPar.PCoarseDetune);
        xmlFreq.addPar_int("detune_type"  , GlobalPar.PDetuneType);

        xmlFreq.addPar_int("bandwidth"    , GlobalPar.PBandwidth);

        XMLtree xmlEnv = xmlFreq.addElm("FREQUENCY_ENVELOPE");
            GlobalPar.FreqEnvelope->add2XML(xmlEnv);

        XMLtree xmlLFO = xmlFreq.addElm("FREQUENCY_LFO");
            GlobalPar.FreqLfo->add2XML(xmlLFO);
    }

    XMLtree xmlFilterParams = xmlAddSynth.addElm("FILTER_PARAMETERS");
    {
        xmlFilterParams.addPar_int("velocity_sensing_amplitude", GlobalPar.PFilterVelocityScale);
        xmlFilterParams.addPar_int("velocity_sensing",   GlobalPar.PFilterVelocityScaleFunction);

        XMLtree xmlFilter = xmlFilterParams.addElm("FILTER");
            GlobalPar.GlobalFilter->add2XML(xmlFilter);

        XMLtree xmlEnv = xmlFilterParams.addElm("FILTER_ENVELOPE");
            GlobalPar.FilterEnvelope->add2XML(xmlEnv);

        XMLtree xmlLFO = xmlFilterParams.addElm("FILTER_LFO");
            GlobalPar.FilterLfo->add2XML(xmlLFO);
    }

    XMLtree xmlRes = xmlAddSynth.addElm("RESONANCE");
        GlobalPar.Reson->add2XML(xmlRes);

    for (uint nvoice=0; nvoice<NUM_VOICES; nvoice++)
    {
        XMLtree xmlVoice = xmlAddSynth.addElm("VOICE", nvoice);
            add2XML_voice(xmlVoice, nvoice);
    }
}


void ADnoteParameters::getfromXML(XMLtree& xmlAddSynth)
{
    assert(xmlAddSynth);
    GlobalPar.PStereo = xmlAddSynth.getPar_bool("stereo", GlobalPar.PStereo);

    if (XMLtree xmlAmp = xmlAddSynth.getElm("AMPLITUDE_PARAMETERS"))
    {
        GlobalPar.PVolume = xmlAmp.getPar_127("volume", GlobalPar.PVolume);
        int val = xmlAmp.getPar_127("random_width", UNUSED);
        if (val < 64)
        {// new Yoshimi format
            GlobalPar.PWidth = val;
            setGlobalPan(xmlAmp.getPar_127("pan_pos", GlobalPar.PPanning), synth.getRuntime().panLaw);
            GlobalPar.PRandom = xmlAmp.getPar_bool("random_pan", GlobalPar.PRandom);
        }
        else
        {// legacy
            setGlobalPan(xmlAmp.getPar_127("panning", GlobalPar.PPanning), synth.getRuntime().panLaw);

            if (GlobalPar.PPanning == 0)
            {
                GlobalPar.PPanning = 64;
                GlobalPar.PRandom = true;
                GlobalPar.PWidth = 63;
            }
            else
                GlobalPar.PRandom = false;
        }

        GlobalPar.PAmpVelocityScaleFunction =
            xmlAmp.getPar_127("velocity_sensing", GlobalPar.PAmpVelocityScaleFunction);
        GlobalPar.Fadein_adjustment     = xmlAmp.getPar_127("fadein_adjustment"     , GlobalPar.Fadein_adjustment);
        GlobalPar.PPunchStrength        = xmlAmp.getPar_127("punch_strength"        , GlobalPar.PPunchStrength);
        GlobalPar.PPunchTime            = xmlAmp.getPar_127("punch_time"            , GlobalPar.PPunchTime);
        GlobalPar.PPunchStretch         = xmlAmp.getPar_127("punch_stretch"         , GlobalPar.PPunchStretch);
        GlobalPar.PPunchVelocitySensing = xmlAmp.getPar_127("punch_velocity_sensing", GlobalPar.PPunchVelocitySensing);
        GlobalPar.Hrandgrouping         = xmlAmp.getPar_127("harmonic_randomness_grouping", GlobalPar.Hrandgrouping);

        if (XMLtree xmlEnv = xmlAmp.getElm("AMPLITUDE_ENVELOPE"))
            GlobalPar.AmpEnvelope->getfromXML(xmlEnv);
        else
            GlobalPar.AmpEnvelope->defaults();

        if (XMLtree xmlLFO = xmlAmp.getElm("AMPLITUDE_LFO"))
            GlobalPar.AmpLfo->getfromXML(xmlLFO);
        else
            GlobalPar.AmpLfo->defaults();
    }

    if (XMLtree xmlFreq = xmlAddSynth.getElm("FREQUENCY_PARAMETERS"))
    {
        GlobalPar.PDetune       = xmlFreq.getPar_int("detune"       , GlobalPar.PDetune,       0, 16383);
        GlobalPar.PCoarseDetune = xmlFreq.getPar_int("coarse_detune", GlobalPar.PCoarseDetune, 0, 16383);
        GlobalPar.PDetuneType   = xmlFreq.getPar_127("detune_type"  , GlobalPar.PDetuneType);

        GlobalPar.PBandwidth    = xmlFreq.getPar_127("bandwidth"    , GlobalPar.PBandwidth);

        if (XMLtree xmlEnv = xmlFreq.getElm("FREQUENCY_ENVELOPE"))
            GlobalPar.FreqEnvelope->getfromXML(xmlEnv);
        else
            GlobalPar.FreqEnvelope->defaults();

        if (XMLtree xmlLFO = xmlFreq.getElm("FREQUENCY_LFO"))
            GlobalPar.FreqLfo->getfromXML(xmlLFO);
        else
            GlobalPar.FreqLfo->defaults();
    }


    if (XMLtree xmlFilterParams = xmlAddSynth.getElm("FILTER_PARAMETERS"))
    {
        GlobalPar.PFilterVelocityScale =
            xmlFilterParams.getPar_127("velocity_sensing_amplitude", GlobalPar.PFilterVelocityScale);
        GlobalPar.PFilterVelocityScaleFunction =
            xmlFilterParams.getPar_127("velocity_sensing", GlobalPar.PFilterVelocityScaleFunction);

        if (XMLtree xmlFilter = xmlFilterParams.getElm("FILTER"))
            GlobalPar.GlobalFilter->getfromXML(xmlFilter);
        else
            GlobalPar.GlobalFilter->defaults();

        if (XMLtree xmlEnv = xmlFilterParams.getElm("FILTER_ENVELOPE"))
            GlobalPar.FilterEnvelope->getfromXML(xmlEnv);
        else
            GlobalPar.FilterEnvelope->defaults();

        if (XMLtree xmlLFO = xmlFilterParams.getElm("FILTER_LFO"))
            GlobalPar.FilterLfo->getfromXML(xmlLFO);
        else
            GlobalPar.FilterLfo->defaults();
    }

    if (XMLtree xmlRes = xmlAddSynth.getElm("RESONANCE"))
        GlobalPar.Reson->getfromXML(xmlRes);
    else
        GlobalPar.Reson->defaults();

    for (uint nvoice = 0; nvoice < NUM_VOICES; nvoice++)
    {
        VoicePar[nvoice].Enabled = false;
        if (XMLtree xmlVoice = xmlAddSynth.getElm("VOICE", nvoice))
            getfromXML_voice(xmlVoice, nvoice);
    }
}

void ADnoteParameters::getfromXML_voice(XMLtree& xmlVoice, const uint nvoice)
{
    if (nvoice >= NUM_VOICES) return;

    VoicePar[nvoice].Enabled = xmlVoice.getPar_bool("enabled", false);
    VoicePar[nvoice].Type    = xmlVoice.getPar_127 ("type", VoicePar[nvoice].Type);

    VoicePar[nvoice].Unison_size             = xmlVoice.getPar_127("unison_size"            , VoicePar[nvoice].Unison_size);
    VoicePar[nvoice].Unison_frequency_spread = xmlVoice.getPar_127("unison_frequency_spread", VoicePar[nvoice].Unison_frequency_spread);
    VoicePar[nvoice].Unison_stereo_spread    = xmlVoice.getPar_127("unison_stereo_spread"   , VoicePar[nvoice].Unison_stereo_spread);
    VoicePar[nvoice].Unison_vibrato          = xmlVoice.getPar_127("unison_vibratto"        , VoicePar[nvoice].Unison_vibrato);
    VoicePar[nvoice].Unison_vibrato_speed    = xmlVoice.getPar_127("unison_vibratto_speed"  , VoicePar[nvoice].Unison_vibrato_speed);
    VoicePar[nvoice].Unison_invert_phase     = xmlVoice.getPar_127("unison_invert_phase"    , VoicePar[nvoice].Unison_invert_phase);
    VoicePar[nvoice].Unison_phase_randomness = xmlVoice.getPar_127("unison_phase_randomness", VoicePar[nvoice].Unison_phase_randomness);

    VoicePar[nvoice].PDelay        = xmlVoice.getPar_127 ("delay"         , VoicePar[nvoice].PDelay);
    VoicePar[nvoice].Presonance    = xmlVoice.getPar_bool("resonance"     , VoicePar[nvoice].Presonance);

    VoicePar[nvoice].PVoice        = xmlVoice.getPar_int ("input_voice"   , VoicePar[nvoice].PVoice, -1, nvoice - 1);
    VoicePar[nvoice].Pextoscil     = xmlVoice.getPar_int ("ext_oscil"     , -1,  -1, nvoice-1);
    VoicePar[nvoice].PextFMoscil   = xmlVoice.getPar_int ("ext_fm_oscil"  , -1,  -1, nvoice-1);

    VoicePar[nvoice].Poscilphase   = xmlVoice.getPar_127 ("oscil_phase"   , VoicePar[nvoice].Poscilphase);
    VoicePar[nvoice].PFMoscilphase = xmlVoice.getPar_127 ("oscil_fm_phase", VoicePar[nvoice].PFMoscilphase);

    VoicePar[nvoice].PFilterEnabled= xmlVoice.getPar_bool("filter_enabled", VoicePar[nvoice].PFilterEnabled);
    VoicePar[nvoice].Pfilterbypass = xmlVoice.getPar_bool("filter_bypass" , VoicePar[nvoice].Pfilterbypass);

    VoicePar[nvoice].PFMEnabled    = xmlVoice.getPar_127 ("fm_enabled"    , VoicePar[nvoice].PFMEnabled);

    if (XMLtree xmlOscil = xmlVoice.getElm("OSCIL"))
        VoicePar[nvoice].POscil->getfromXML(xmlOscil);

    if (XMLtree xmlAmp = xmlVoice.getElm("AMPLITUDE_PARAMETERS"))
    {
        int val = xmlAmp.getPar_127("random_width", UNUSED);
        if (val < 64)
        {// new Yoshimi format
            VoicePar[nvoice].PWidth = val;
            setVoicePan(nvoice, xmlAmp.getPar_127("pan_pos", VoicePar[nvoice].PPanning), synth.getRuntime().panLaw);
            VoicePar[nvoice].PRandom = xmlAmp.getPar_bool("random_pan", VoicePar[nvoice].PRandom);
        }
        else
        {// legacy
            setVoicePan(nvoice, xmlAmp.getPar_127("panning", VoicePar[nvoice].PPanning), synth.getRuntime().panLaw);
            if (VoicePar[nvoice].PPanning == 0)
            {
                VoicePar[nvoice].PPanning = 64;
                VoicePar[nvoice].PRandom = true;
                VoicePar[nvoice].PWidth = 63;
            }
            else
                VoicePar[nvoice].PRandom = false;
        }
        VoicePar[nvoice].PVolume      = xmlAmp.getPar_127 ("volume"      , VoicePar[nvoice].PVolume);
        VoicePar[nvoice].PVolumeminus = xmlAmp.getPar_bool("volume_minus", VoicePar[nvoice].PVolumeminus);
        VoicePar[nvoice].PAmpVelocityScaleFunction =
                xmlAmp.getPar_127("velocity_sensing", VoicePar[nvoice].PAmpVelocityScaleFunction);

        VoicePar[nvoice].PAmpEnvelopeEnabled = xmlAmp.getPar_bool("amp_envelope_enabled",VoicePar[nvoice].PAmpEnvelopeEnabled);
        if (XMLtree xmlEnv = xmlAmp.getElm("AMPLITUDE_ENVELOPE"))
            VoicePar[nvoice].AmpEnvelope->getfromXML(xmlEnv);
        else
            VoicePar[nvoice].AmpEnvelope->defaults();

        VoicePar[nvoice].PAmpLfoEnabled = xmlAmp.getPar_bool("amp_lfo_enabled",VoicePar[nvoice].PAmpLfoEnabled);
        if (XMLtree xmlLFO = xmlAmp.getElm("AMPLITUDE_LFO"))
            VoicePar[nvoice].AmpLfo->getfromXML(xmlLFO);
        else
            VoicePar[nvoice].AmpLfo->defaults();
    }

    if (XMLtree xmlFreq = xmlVoice.getElm("FREQUENCY_PARAMETERS"))
    {
        VoicePar[nvoice].Pfixedfreq   = xmlFreq.getPar_bool("fixed_freq"   , VoicePar[nvoice].Pfixedfreq);
        VoicePar[nvoice].PfixedfreqET = xmlFreq.getPar_127 ("fixed_freq_et", VoicePar[nvoice].PfixedfreqET);
        VoicePar[nvoice].PBendAdjust  = xmlFreq.getPar_127 ("bend_adjust"  , VoicePar[nvoice].PBendAdjust);
        VoicePar[nvoice].POffsetHz    = xmlFreq.getPar_127 ("offset_hz"    , VoicePar[nvoice].POffsetHz);

        VoicePar[nvoice].PDetune      = xmlFreq.getPar_int ("detune"       , VoicePar[nvoice].PDetune, 0, 16383);
        VoicePar[nvoice].PCoarseDetune= xmlFreq.getPar_int ("coarse_detune", VoicePar[nvoice].PCoarseDetune, 0, 16383);
        VoicePar[nvoice].PDetuneType  = xmlFreq.getPar_127 ("detune_type"  , VoicePar[nvoice].PDetuneType);

        VoicePar[nvoice].PFreqEnvelopeEnabled = xmlFreq.getPar_bool("freq_envelope_enabled", VoicePar[nvoice].PFreqEnvelopeEnabled);
        if (XMLtree xmlEnv = xmlFreq.getElm("FREQUENCY_ENVELOPE"))
            VoicePar[nvoice].FreqEnvelope->getfromXML(xmlEnv);
        else
            VoicePar[nvoice].FreqEnvelope->defaults();

        VoicePar[nvoice].PFreqLfoEnabled = xmlFreq.getPar_bool("freq_lfo_enabled", VoicePar[nvoice].PFreqLfoEnabled);
        if (XMLtree xmlLFO = xmlFreq.getElm("FREQUENCY_LFO"))
            VoicePar[nvoice].FreqLfo->getfromXML(xmlLFO);
        else
            VoicePar[nvoice].FreqLfo->defaults();
    }

    if (XMLtree xmlFilterParams = xmlVoice.getElm("FILTER_PARAMETERS"))
    {
        VoicePar[nvoice].PFilterVelocityScale =
                xmlFilterParams.getPar_127("velocity_sensing_amplitude", VoicePar[nvoice].PFilterVelocityScale);
        VoicePar[nvoice].PFilterVelocityScaleFunction =
                xmlFilterParams.getPar_127("velocity_sensing", VoicePar[nvoice].PFilterVelocityScaleFunction);

        if (XMLtree xmlFilter = xmlFilterParams.getElm("FILTER"))
            VoicePar[nvoice].VoiceFilter->getfromXML(xmlFilter);
        else
            VoicePar[nvoice].VoiceFilter->defaults();

        VoicePar[nvoice].PFilterEnvelopeEnabled = xmlFilterParams.getPar_bool("filter_envelope_enabled", VoicePar[nvoice].PFilterEnvelopeEnabled);
        if (XMLtree xmlEnv = xmlFilterParams.getElm("FILTER_ENVELOPE"))
            VoicePar[nvoice].FilterEnvelope->getfromXML(xmlEnv);
        else
            VoicePar[nvoice].FilterEnvelope->defaults();

        VoicePar[nvoice].PFilterLfoEnabled = xmlFilterParams.getPar_bool("filter_lfo_enabled", VoicePar[nvoice].PFilterLfoEnabled);
        if (XMLtree xmlLFO = xmlFilterParams.getElm("FILTER_LFO"))
            VoicePar[nvoice].FilterLfo->getfromXML(xmlLFO);
        else
            VoicePar[nvoice].FilterLfo->defaults();
    }

    if (XMLtree xmlFM = xmlVoice.getElm("FM_PARAMETERS"))
    {
        VoicePar[nvoice].PFMVoice      = xmlFM.getPar_int("input_voice", VoicePar[nvoice].PFMVoice, -1, nvoice - 1);

        VoicePar[nvoice].PFMVolume     = xmlFM.getPar_127("volume"     , VoicePar[nvoice].PFMVolume);
        VoicePar[nvoice].PFMVolumeDamp = xmlFM.getPar_127("volume_damp", VoicePar[nvoice].PFMVolumeDamp);
        VoicePar[nvoice].PFMVelocityScaleFunction =
                xmlFM.getPar_127("velocity_sensing", VoicePar[nvoice].PFMVelocityScaleFunction);

        VoicePar[nvoice].PFMAmpEnvelopeEnabled = xmlFM.getPar_bool("amp_envelope_enabled", VoicePar[nvoice].PFMAmpEnvelopeEnabled);
        if (XMLtree xmlEnv = xmlFM.getElm("AMPLITUDE_ENVELOPE"))
            VoicePar[nvoice].FMAmpEnvelope->getfromXML(xmlEnv);
        else
            VoicePar[nvoice].FMAmpEnvelope->defaults();

        if (XMLtree xmlMod = xmlFM.getElm("MODULATOR"))
        {
            bool loadFMFreqParams = true;
            VoicePar[nvoice].PFMDetuneFromBaseOsc = xmlMod.getPar_bool("detune_from_base_osc", 127);
            if (VoicePar[nvoice].PFMDetuneFromBaseOsc == 127)
            {
                // In the past it was not possible to choose whether to include
                // detuning from the base oscillator. For local modulators it
                // was always enabled, for imported voice modulators it was
                // always disabled. To load old patches correctly, we apply this
                // old behaviour here if the XML element is missing from the
                // patch. New patches will always save one or the other.
                //
                // In a similar fashion, it was not possible to apply frequency
                // parameters to imported voice modulators in the past, however
                // it was possible to save them if you edited them before
                // switching to an imported voice. Now that frequency parameters
                // are respected, we need to ignore those parameters for old
                // instruments that saved them, but didn't use them, otherwise
                // the instrument will sound different.
                if (VoicePar[nvoice].PFMVoice >= 0)
                {
                    VoicePar[nvoice].PFMDetuneFromBaseOsc = 0;
                    loadFMFreqParams = false;

                    // In the past the fixed frequency of the imported voice was
                    // respected. Now, the fixed frequency of the modulator is
                    // respected. So if we load an old patch, fetch that setting
                    // from the imported voice.
                    VoicePar[nvoice].PFMFixedFreq =
                        VoicePar[VoicePar[nvoice].PFMVoice].Pfixedfreq;
                }
                else
                {
                    VoicePar[nvoice].PFMDetuneFromBaseOsc = 1;
                }
            }
            if (loadFMFreqParams)
            {
                VoicePar[nvoice].PFMDetune       = xmlMod.getPar_int("detune"       , VoicePar[nvoice].PFMDetune      , 0, 16383);
                VoicePar[nvoice].PFMCoarseDetune = xmlMod.getPar_int("coarse_detune", VoicePar[nvoice].PFMCoarseDetune, 0, 16383);
                VoicePar[nvoice].PFMDetuneType   = xmlMod.getPar_127("detune_type"  , VoicePar[nvoice].PFMDetuneType);
                VoicePar[nvoice].PFMFixedFreq    = xmlMod.getPar_bool("fixed_freq"  , VoicePar[nvoice].PFMFixedFreq);

                VoicePar[nvoice].PFMFreqEnvelopeEnabled = xmlMod.getPar_bool("freq_envelope_enabled", VoicePar[nvoice].PFMFreqEnvelopeEnabled);
                if (XMLtree xmlEnv = xmlMod.getElm("FREQUENCY_ENVELOPE"))
                    VoicePar[nvoice].FMFreqEnvelope->getfromXML(xmlEnv);
                else
                    VoicePar[nvoice].FMFreqEnvelope->defaults();
            }

            if (XMLtree xmlOscil = xmlMod.getElm("OSCIL"))
                VoicePar[nvoice].POscilFM->getfromXML(xmlOscil);
        }
    }
}


float ADnoteParameters::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int engine = getData->data.engine;

    unsigned char type = 0;

    // addnote defaults
    int min = 0;
    float def = 0;
    int max = 127;
    type |= TOPLEVEL::type::Integer;
    unsigned char learnable = TOPLEVEL::type::Learnable;

    if (engine == PART::engine::addSynth)
    {
        switch (control)
        {
            case ADDSYNTH::control::volume:
                type |= learnable;
                def = 90;
                break;

            case ADDSYNTH::control::velocitySense:
                type |= learnable;
                def = 64;
                break;

            case ADDSYNTH::control::panning:
                type |= learnable;
                def = 64;
                break;

            case ADDSYNTH::control::enableRandomPan:
                max = 1;
                break;

            case ADDSYNTH::control::randomWidth:
                type |= learnable;
                def = 63;
                max = 63;
                break;

            case ADDSYNTH::control::detuneFrequency:
                type |= learnable;
                min = -8192;
                max = 8191;
                break;

            case ADDSYNTH::control::octave:
                type |= learnable;
                min = -8;
                max = 7;
                break;

            case ADDSYNTH::control::detuneType:
                min = 1;
                max = 4;
                break;

            case ADDSYNTH::control::coarseDetune:
                min = -64;
                max = 63;
                break;

            case ADDSYNTH::control::relativeBandwidth:
                type |= learnable;
                def = 64;
                break;

            case ADDSYNTH::control::stereo:
                type |= learnable;
                def = 1;
                max = 1;
                break;

            case ADDSYNTH::control::randomGroup:
                max = 1;
                break;

            case ADDSYNTH::control::dePop:
                type |= learnable;
                def = FADEIN_ADJUSTMENT_SCALE;
                break;

            case ADDSYNTH::control::punchStrength:
                type |= learnable;
                break;

            case ADDSYNTH::control::punchDuration:
                type |= learnable;
                def = 60;
                break;

            case ADDSYNTH::control::punchStretch:
                type |= learnable;
                def = 64;
                break;

            case ADDSYNTH::control::punchVelocity:
                type |= learnable;
                def = 72;
                break;

            default:
                type |= TOPLEVEL::type::Error;
                break;
        }
        getData->data.type = type;
        if (type & TOPLEVEL::type::Error)
            return 1;

        switch (request)
        {
            case TOPLEVEL::type::Adjust:
                if (value < min)
                    value = min;
                else if (value > max)
                    value = max;
            break;
            case TOPLEVEL::type::Minimum:
                value = min;
                break;
            case TOPLEVEL::type::Maximum:
                value = max;
                break;
            case TOPLEVEL::type::Default:
                value = def;
                break;
        }
        return value;
    }

    switch (control)
    {
        case ADDVOICE::control::volume:
            type |= learnable;
            def = 100;
            break;

        case ADDVOICE::control::velocitySense:
            type |= learnable;
            def = 127;
            break;

        case ADDVOICE::control::panning:
            type |= learnable;
            def = 64;
            break;
        case ADDVOICE::control::enableRandomPan:
            max = 1;
            break;

        case ADDVOICE::control::randomWidth:
            def = 63;
            max = 63;
            break;

        case ADDVOICE::control::invertPhase:
            max = 1;
            break;

        case ADDVOICE::control::enableAmplitudeEnvelope:
            type |= learnable;
            max = 1;
            break;
        case ADDVOICE::control::enableAmplitudeLFO:
            type |= learnable;
            max = 1;
            break;

        case ADDVOICE::control::modulatorType:
            type |= learnable;
            max = 5;
            break;

        case ADDVOICE::control::externalModulator:
            min = -1;
            def = -1;
            max = 6;
            break;

        case ADDVOICE::control::externalOscillator:
            min = -1;
            def = -1;
            max = 6;
            break;

        case ADDVOICE::control::detuneFrequency:
            type |= learnable;
            min = -8192;
            max = 8191;
            break;

        case ADDVOICE::control::equalTemperVariation:
            type |= learnable;
            break;

        case ADDVOICE::control::baseFrequencyAs440Hz:
            max = 1;
            break;

        case ADDVOICE::control::octave:
            type |= learnable;
            min = -8;
            max = 7;
            break;

        case ADDVOICE::control::detuneType:
            max = 4;
            break;

        case ADDVOICE::control::coarseDetune:
            min = -64;
            max = 63;
            break;

        case ADDVOICE::control::pitchBendAdjustment:
            type |= learnable;
            def = 88;
            break;

        case ADDVOICE::control::pitchBendOffset:
            type |= learnable;
            def = 64;
            break;

        case ADDVOICE::control::enableFrequencyEnvelope:
            type |= learnable;
            max = 1;
            break;
        case ADDVOICE::control::enableFrequencyLFO:
            type |= learnable;
            max = 1;
            break;

        case ADDVOICE::control::unisonFrequencySpread:
            type |= learnable;
            def = 60;
            break;

        case ADDVOICE::control::unisonPhaseRandomise:
            type |= learnable;
            def = 127;
            break;

        case ADDVOICE::control::unisonStereoSpread:
            type |= learnable;
            def = 64;
            break;

        case ADDVOICE::control::unisonVibratoDepth:
            type |= learnable;
            def = 64;
            break;

        case ADDVOICE::control::unisonVibratoSpeed:
            type |= learnable;
            def = 64;
            break;

        case ADDVOICE::control::unisonSize:
            min = 2;
            def = 2;
            max = 50;
            break;

        case ADDVOICE::control::unisonPhaseInvert:
            max = 5;
            break;

        case ADDVOICE::control::enableUnison:
            type |= learnable;
            max = 1;
            break;

        case ADDVOICE::control::bypassGlobalFilter:
            max = 1;
            break;

        case ADDVOICE::control::enableFilter:
            type |= learnable;
            max = 1;
            break;
        case ADDVOICE::control::enableFilterEnvelope:
            type |= learnable;
            max = 1;
            break;
        case ADDVOICE::control::enableFilterLFO:
            type |= learnable;
            max = 1;
            break;

        case ADDVOICE::control::modulatorAmplitude:
            type |= learnable;
            def = 90;
            break;

        case ADDVOICE::control::modulatorVelocitySense:
            type |= learnable;
            def = 64;
            break;

        case ADDVOICE::control::modulatorHFdamping:
            type |= learnable;
            min = -64;
            max = 63;
            break;
        case ADDVOICE::control::enableModulatorAmplitudeEnvelope:
            type |= learnable;
            max = 1;
            break;
        case ADDVOICE::control::modulatorDetuneFrequency:
            type |= learnable;
            min = -8192;
            max = 8191;
            break;
        case ADDVOICE::control::modulatorDetuneFromBaseOsc:
            def = 1;
            max = 1;
            break;
        case ADDVOICE::control::modulatorFrequencyAs440Hz:
            max = 1;
            break;
        case ADDVOICE::control::modulatorOctave:
            type |= learnable;
            min = -8;
            max = 7;
            break;
        case ADDVOICE::control::modulatorDetuneType:
            max = 4;
            break;
        case ADDVOICE::control::modulatorCoarseDetune:
            min = -64;
            max = 63;
            break;
        case ADDVOICE::control::enableModulatorFrequencyEnvelope:
            type |= learnable;
            max = 1;
            break;
        case ADDVOICE::control::modulatorOscillatorPhase:
            type |= learnable;
            min = -64;
            max = 63;
            break;
        case ADDVOICE::control::modulatorOscillatorSource:
            min = -1;
            def = -1;
            max = 6;
            break;

        case ADDVOICE::control::delay:
            type |= learnable;
            break;
        case ADDVOICE::control::enableVoice:
            type |= learnable;
            if (engine == PART::engine::addVoice1)
                def = 1;
            max = 1;
            break;
        case ADDVOICE::control::enableResonance:
            def = 1;
            max = 1;
            break;

        case ADDVOICE::control::voiceOscillatorPhase:
            type |= learnable;
            min = -64;
            max = 63;
            break;
        case ADDVOICE::control::voiceOscillatorSource:
            min = -1;
            def = -1;
            max = 6;
            break;

        case ADDVOICE::control::soundType:
            max = 3;
            break;

        default:
            type |= TOPLEVEL::type::Error;
            break;
    }
    getData->data.type = type;
    if (type & TOPLEVEL::type::Error)
        return 1;

    switch (request)
    {
        case TOPLEVEL::type::Adjust:
            if (value < min)
                value = min;
            else if (value > max)
                value = max;
        break;
        case TOPLEVEL::type::Minimum:
            value = min;
            break;
        case TOPLEVEL::type::Maximum:
            value = max;
            break;
        case TOPLEVEL::type::Default:
            value = def;
            break;
    }
    return value;
}
