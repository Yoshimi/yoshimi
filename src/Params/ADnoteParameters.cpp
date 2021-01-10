/*
    ADnoteParameters.cpp - Parameters for ADnote (ADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019 Will Godfrey
    Copyright 2020-2021 Kristian Amlie, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code.
*/

#include <iostream>

#include <cmath>
#include <stdlib.h>
#include "Misc/NumericFuncs.h"

using namespace std;
using func::setAllPan;

#include "Misc/SynthEngine.h"
#include "Params/ADnoteParameters.h"

int ADnoteParameters::ADnote_unison_sizes[] =
{2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 40, 50, 0};

ADnoteParameters::ADnoteParameters(FFTwrapper *fft_, SynthEngine *_synth) :
    Presets(_synth),
    fft(fft_)
{
    setpresettype("Padsyth");
    GlobalPar.FreqEnvelope = new EnvelopeParams(0, 0, synth);
    GlobalPar.FreqEnvelope->ASRinit(64, 50, 64, 60);
    GlobalPar.FreqLfo = new LFOParams(70, 0, 64, 0, 0, 0, 0, 0, synth);

    GlobalPar.AmpEnvelope = new EnvelopeParams(64, 1, synth);
    GlobalPar.AmpEnvelope->ADSRinit_dB(0, 40, 127, 25);
    GlobalPar.AmpLfo = new LFOParams(80, 0, 64, 0, 0, 0, 0, 1, synth);

    GlobalPar.GlobalFilter = new FilterParams(2, 94, 40, 0, synth);
    GlobalPar.FilterEnvelope = new EnvelopeParams(0, 1, synth);
    GlobalPar.FilterEnvelope->ADSRinit_filter(64, 40, 64, 70, 60, 64);
    GlobalPar.FilterLfo = new LFOParams(80, 0, 64, 0, 0, 0, 0, 2, synth);
    GlobalPar.Reson = new Resonance(synth);

    for (int nvoice = 0; nvoice < NUM_VOICES; ++nvoice)
        enableVoice(nvoice);
    defaults();
}


void ADnoteParameters::defaults(void)
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
    setGlobalPan(GlobalPar.PPanning = 64, synth->getRuntime().panLaw); // center
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
    VoicePar[0].Enabled = 1;
}


// Defaults a voice
void ADnoteParameters::defaults(int n)
{
    int nvoice = n;
    VoicePar[nvoice].Enabled = 0;

    VoicePar[nvoice].Unison_size = 1;
    VoicePar[nvoice].Unison_frequency_spread = 60;
    VoicePar[nvoice].Unison_stereo_spread = 64;
    VoicePar[nvoice].Unison_vibratto = 64;
    VoicePar[nvoice].Unison_vibratto_speed = 64;
    VoicePar[nvoice].Unison_invert_phase = 0;
    VoicePar[nvoice].Unison_phase_randomness = 127;

    VoicePar[nvoice].Type = 0;
    VoicePar[nvoice].Pfixedfreq = 0;
    VoicePar[nvoice].PfixedfreqET = 0;
    VoicePar[nvoice].PBendAdjust = 88; // 64 + 24
    VoicePar[nvoice].POffsetHz     = 64;
    VoicePar[nvoice].Presonance = 1;
    VoicePar[nvoice].Pfilterbypass = 0;
    VoicePar[nvoice].Pextoscil = -1;
    VoicePar[nvoice].PextFMoscil = -1;
    VoicePar[nvoice].Poscilphase = 64;
    VoicePar[nvoice].PFMoscilphase = 64;
    VoicePar[nvoice].PDelay = 0;
    VoicePar[nvoice].PVolume = 100;
    VoicePar[nvoice].PVolumeminus = 0;
    setVoicePan(nvoice, VoicePar[nvoice].PPanning = 64, synth->getRuntime().panLaw); // center
    VoicePar[nvoice].PRandom = false;
    VoicePar[nvoice].PWidth = 63;
    VoicePar[nvoice].PDetune = 8192; // 8192 = 0
    VoicePar[nvoice].PCoarseDetune = 0;
    VoicePar[nvoice].PDetuneType = 0;
    VoicePar[nvoice].PFreqLfoEnabled = 0;
    VoicePar[nvoice].PFreqEnvelopeEnabled = 0;
    VoicePar[nvoice].PAmpEnvelopeEnabled = 0;
    VoicePar[nvoice].PAmpLfoEnabled = 0;
    VoicePar[nvoice].PAmpVelocityScaleFunction = 127;
    VoicePar[nvoice].PFilterEnabled = 0;
    VoicePar[nvoice].PFilterEnvelopeEnabled = 0;
    VoicePar[nvoice].PFilterLfoEnabled = 0;
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
    VoicePar[nvoice].PFMFreqEnvelopeEnabled = 0;
    VoicePar[nvoice].PFMAmpEnvelopeEnabled = 0;
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
    VoicePar[nvoice].POscil = new OscilParameters(synth);
    VoicePar[nvoice].POscilFM = new OscilParameters(synth);
    VoicePar[nvoice].OscilSmp = new OscilGen(fft, GlobalPar.Reson, synth, VoicePar[nvoice].POscil);
    VoicePar[nvoice].FMSmp = new OscilGen(fft, NULL, synth, VoicePar[nvoice].POscilFM);

    VoicePar[nvoice].AmpEnvelope = new EnvelopeParams(64, 1, synth);
    VoicePar[nvoice].AmpEnvelope->ADSRinit_dB(0, 100, 127, 100);
    VoicePar[nvoice].AmpLfo = new LFOParams(90, 32, 64, 0, 0, 30, 0, 1, synth);

    VoicePar[nvoice].FreqEnvelope = new EnvelopeParams(0, 0, synth);
    VoicePar[nvoice].FreqEnvelope->ASRinit(30, 40, 64, 60);
    VoicePar[nvoice].FreqLfo = new LFOParams(50, 40, 0, 0, 0, 0, 0, 0, synth);

    VoicePar[nvoice].VoiceFilter = new FilterParams(2, 50, 60, 0, synth);
    VoicePar[nvoice].FilterEnvelope = new EnvelopeParams(0, 0, synth);
    VoicePar[nvoice].FilterEnvelope->ADSRinit_filter(90, 70, 40, 70, 10, 40);
    VoicePar[nvoice].FilterLfo = new LFOParams(50, 20, 64, 0, 0, 0, 0, 2, synth);

    VoicePar[nvoice].FMFreqEnvelope = new EnvelopeParams(0, 0, synth);
    VoicePar[nvoice].FMFreqEnvelope->ASRinit(20, 90, 40, 80);
    VoicePar[nvoice].FMAmpEnvelope = new EnvelopeParams(64, 1, synth);
    VoicePar[nvoice].FMAmpEnvelope->ADSRinit(80, 90, 127, 100);
}


// Get the Multiplier of the fine detunes of the voices
float ADnoteParameters::getBandwidthDetuneMultiplier(void)
{
    float bw = (GlobalPar.PBandwidth - 64.0f) / 64.0f;
    bw = powf(2.0f, bw * pow(fabs(bw), 0.2f) * 5.0f);
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


void ADnoteParameters::add2XMLsection(XMLwrapper *xml, int n)
{
    int nvoice = n;
    if (nvoice >= NUM_VOICES)
        return;

    // currently not used
    // bool yoshiFormat = synth->usingYoshiType;
    int oscilused = 0, fmoscilused = 0; // if the oscil or fmoscil are used by another voice

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        if (VoicePar[i].Pextoscil == nvoice)
            oscilused = 1;
        if (VoicePar[i].PextFMoscil == nvoice)
            fmoscilused = 1;
    }

    xml->addparbool("enabled", VoicePar[nvoice].Enabled);
    if (((VoicePar[nvoice].Enabled == 0) && (oscilused==0) && (fmoscilused==0)) && (xml->minimal)) return;

    xml->addpar("type", VoicePar[nvoice].Type);

    xml->addpar("unison_size", VoicePar[nvoice].Unison_size);
    xml->addpar("unison_frequency_spread",
                VoicePar[nvoice].Unison_frequency_spread);
    xml->addpar("unison_stereo_spread", VoicePar[nvoice].Unison_stereo_spread);
    xml->addpar("unison_vibratto", VoicePar[nvoice].Unison_vibratto);
    xml->addpar("unison_vibratto_speed", VoicePar[nvoice].Unison_vibratto_speed);
    xml->addpar("unison_invert_phase", VoicePar[nvoice].Unison_invert_phase);
    xml->addpar("unison_phase_randomness", VoicePar[nvoice].Unison_phase_randomness);

    xml->addpar("delay", VoicePar[nvoice].PDelay);
    xml->addparbool("resonance", VoicePar[nvoice].Presonance);

    xml->addpar("input_voice", VoicePar[nvoice].PVoice);
    xml->addpar("ext_oscil", VoicePar[nvoice].Pextoscil);
    xml->addpar("ext_fm_oscil", VoicePar[nvoice].PextFMoscil);

    xml->addpar("oscil_phase", VoicePar[nvoice].Poscilphase);
    xml->addpar("oscil_fm_phase", VoicePar[nvoice].PFMoscilphase);

    xml->addparbool("filter_enabled", VoicePar[nvoice].PFilterEnabled);
    xml->addparbool("filter_bypass", VoicePar[nvoice].Pfilterbypass);

    xml->addpar("fm_enabled", VoicePar[nvoice].PFMEnabled);

    xml->beginbranch("OSCIL");
        VoicePar[nvoice].POscil->add2XML(xml);
    xml->endbranch();


    xml->beginbranch("AMPLITUDE_PARAMETERS");
        // new yoshi type
        xml->addpar("pan_pos", VoicePar[nvoice].PPanning);
        xml->addparbool("random_pan", VoicePar[nvoice].PRandom);
        xml->addpar("random_width", VoicePar[nvoice].PWidth);

        // legacy
        if (VoicePar[nvoice].PRandom)
            xml->addpar("panning", 0);
        else
            xml->addpar("panning", VoicePar[nvoice].PPanning);

        xml->addpar("volume", VoicePar[nvoice].PVolume);
        xml->addparbool("volume_minus", VoicePar[nvoice].PVolumeminus);
        xml->addpar("velocity_sensing", VoicePar[nvoice].PAmpVelocityScaleFunction);
        xml->addparbool("amp_envelope_enabled", VoicePar[nvoice].PAmpEnvelopeEnabled);
        if ((VoicePar[nvoice].PAmpEnvelopeEnabled!=0) || (!xml->minimal))
        {
            xml->beginbranch("AMPLITUDE_ENVELOPE");
                VoicePar[nvoice].AmpEnvelope->add2XML(xml);
            xml->endbranch();
        }
        xml->addparbool("amp_lfo_enabled", VoicePar[nvoice].PAmpLfoEnabled);
        if ((VoicePar[nvoice].PAmpLfoEnabled != 0) || (!xml->minimal))
        {
            xml->beginbranch("AMPLITUDE_LFO");
                VoicePar[nvoice].AmpLfo->add2XML(xml);
            xml->endbranch();
        }
    xml->endbranch();

    xml->beginbranch("FREQUENCY_PARAMETERS");
        xml->addparbool("fixed_freq", VoicePar[nvoice].Pfixedfreq);
        xml->addpar("fixed_freq_et", VoicePar[nvoice].PfixedfreqET);
        xml->addpar("bend_adjust", VoicePar[nvoice].PBendAdjust);
        xml->addpar("offset_hz", VoicePar[nvoice].POffsetHz);
        xml->addpar("detune", VoicePar[nvoice].PDetune);
        xml->addpar("coarse_detune", VoicePar[nvoice].PCoarseDetune);
        xml->addpar("detune_type", VoicePar[nvoice].PDetuneType);

        xml->addparbool("freq_envelope_enabled", VoicePar[nvoice].PFreqEnvelopeEnabled);
        if ((VoicePar[nvoice].PFreqEnvelopeEnabled != 0) || (!xml->minimal))
        {
            xml->beginbranch("FREQUENCY_ENVELOPE");
                VoicePar[nvoice].FreqEnvelope->add2XML(xml);
            xml->endbranch();
        }
        xml->addparbool("freq_lfo_enabled", VoicePar[nvoice].PFreqLfoEnabled);
        if ((VoicePar[nvoice].PFreqLfoEnabled != 0) || (!xml->minimal))
        {
            xml->beginbranch("FREQUENCY_LFO");
                VoicePar[nvoice].FreqLfo->add2XML(xml);
            xml->endbranch();
        }
    xml->endbranch();

    if ((VoicePar[nvoice].PFilterEnabled != 0) || (!xml->minimal))
    {
        xml->beginbranch("FILTER_PARAMETERS");
            xml->addpar("velocity_sensing_amplitude", VoicePar[nvoice].PFilterVelocityScale);
            xml->addpar("velocity_sensing", VoicePar[nvoice].PFilterVelocityScaleFunction);
            xml->beginbranch("FILTER");
                VoicePar[nvoice].VoiceFilter->add2XML(xml);
            xml->endbranch();

            xml->addparbool("filter_envelope_enabled", VoicePar[nvoice].PFilterEnvelopeEnabled);
            if ((VoicePar[nvoice].PFilterEnvelopeEnabled != 0) || (!xml->minimal))
            {
                xml->beginbranch("FILTER_ENVELOPE");
                    VoicePar[nvoice].FilterEnvelope->add2XML(xml);
                xml->endbranch();
            }

            xml->addparbool("filter_lfo_enabled", VoicePar[nvoice].PFilterLfoEnabled);
            if ((VoicePar[nvoice].PFilterLfoEnabled !=0) || (!xml->minimal))
            {
                xml->beginbranch("FILTER_LFO");
                    VoicePar[nvoice].FilterLfo->add2XML(xml);
                xml->endbranch();
            }
            xml->endbranch();
    }

    if ((VoicePar[nvoice].PFMEnabled != 0) || (fmoscilused !=0 ) || (!xml->minimal))
    {
        xml->beginbranch("FM_PARAMETERS");
            xml->addpar("input_voice", VoicePar[nvoice].PFMVoice);

            xml->addpar("volume", VoicePar[nvoice].PFMVolume);
            xml->addpar("volume_damp", VoicePar[nvoice].PFMVolumeDamp);
            xml->addpar("velocity_sensing", VoicePar[nvoice].PFMVelocityScaleFunction);

            xml->addparbool("amp_envelope_enabled", VoicePar[nvoice].PFMAmpEnvelopeEnabled);
            if ((VoicePar[nvoice].PFMAmpEnvelopeEnabled != 0) || (!xml->minimal))
            {
                xml->beginbranch("AMPLITUDE_ENVELOPE");
                    VoicePar[nvoice].FMAmpEnvelope->add2XML(xml);
                xml->endbranch();
            }
            xml->beginbranch("MODULATOR");
                xml->addparbool("detune_from_base_osc", VoicePar[nvoice].PFMDetuneFromBaseOsc);
                xml->addpar("detune", VoicePar[nvoice].PFMDetune);
                xml->addpar("coarse_detune", VoicePar[nvoice].PFMCoarseDetune);
                xml->addpar("detune_type", VoicePar[nvoice].PFMDetuneType);

                xml->addparbool("freq_envelope_enabled", VoicePar[nvoice].PFMFreqEnvelopeEnabled);
                xml->addparbool("fixed_freq", VoicePar[nvoice].PFMFixedFreq);
                if ((VoicePar[nvoice].PFMFreqEnvelopeEnabled != 0) || (!xml->minimal))
                {
                    xml->beginbranch("FREQUENCY_ENVELOPE");
                        VoicePar[nvoice].FMFreqEnvelope->add2XML(xml);
                    xml->endbranch();
                }

                xml->beginbranch("OSCIL");
                    VoicePar[nvoice].POscilFM->add2XML(xml);
                xml->endbranch();

            xml->endbranch();
        xml->endbranch();
    }
}


void ADnoteParameters::add2XML(XMLwrapper *xml)
{
    // currently not used
    // bool yoshiFormat = synth->usingYoshiType;
    xml->information.ADDsynth_used = 1;

    xml->addparbool("stereo", GlobalPar.PStereo);

    xml->beginbranch("AMPLITUDE_PARAMETERS");
        xml->addpar("volume", GlobalPar.PVolume);
        // new yoshi type
        xml->addpar("pan_pos", GlobalPar.PPanning);
        xml->addparbool("random_pan", GlobalPar.PRandom);
        xml->addpar("random_width", GlobalPar.PWidth);

        // legacy
        if (GlobalPar.PRandom)
            xml->addpar("panning", 0);
        else
            xml->addpar("panning", GlobalPar.PPanning);

        xml->addpar("velocity_sensing", GlobalPar.PAmpVelocityScaleFunction);
        xml->addpar("fadein_adjustment", GlobalPar.Fadein_adjustment);
        xml->addpar("punch_strength", GlobalPar.PPunchStrength);
        xml->addpar("punch_time", GlobalPar.PPunchTime);
        xml->addpar("punch_stretch", GlobalPar.PPunchStretch);
        xml->addpar("punch_velocity_sensing", GlobalPar.PPunchVelocitySensing);
        xml->addpar("harmonic_randomness_grouping", GlobalPar.Hrandgrouping);

        xml->beginbranch("AMPLITUDE_ENVELOPE");
            GlobalPar.AmpEnvelope->add2XML(xml);
        xml->endbranch();

        xml->beginbranch("AMPLITUDE_LFO");
            GlobalPar.AmpLfo->add2XML(xml);
        xml->endbranch();
    xml->endbranch();

    xml->beginbranch("FREQUENCY_PARAMETERS");
        xml->addpar("detune", GlobalPar.PDetune);

        xml->addpar("coarse_detune", GlobalPar.PCoarseDetune);
        xml->addpar("detune_type", GlobalPar.PDetuneType);

        xml->addpar("bandwidth", GlobalPar.PBandwidth);

        xml->beginbranch("FREQUENCY_ENVELOPE");
            GlobalPar.FreqEnvelope->add2XML(xml);
        xml->endbranch();

        xml->beginbranch("FREQUENCY_LFO");
            GlobalPar.FreqLfo->add2XML(xml);
        xml->endbranch();
    xml->endbranch();


    xml->beginbranch("FILTER_PARAMETERS");
        xml->addpar("velocity_sensing_amplitude", GlobalPar.PFilterVelocityScale);
        xml->addpar("velocity_sensing", GlobalPar.PFilterVelocityScaleFunction);

        xml->beginbranch("FILTER");
            GlobalPar.GlobalFilter->add2XML(xml);
        xml->endbranch();

        xml->beginbranch("FILTER_ENVELOPE");
            GlobalPar.FilterEnvelope->add2XML(xml);
        xml->endbranch();

        xml->beginbranch("FILTER_LFO");
            GlobalPar.FilterLfo->add2XML(xml);
        xml->endbranch();
    xml->endbranch();

    xml->beginbranch("RESONANCE");
        GlobalPar.Reson->add2XML(xml);
    xml->endbranch();

    for (int nvoice=0;nvoice<NUM_VOICES;nvoice++)
    {
        xml->beginbranch("VOICE",nvoice);
        add2XMLsection(xml,nvoice);
        xml->endbranch();
    }
}


void ADnoteParameters::getfromXML(XMLwrapper *xml)
{
    GlobalPar.PStereo = (xml->getparbool("stereo", GlobalPar.PStereo)) != 0;

    if (xml->enterbranch("AMPLITUDE_PARAMETERS"))
    {
        GlobalPar.PVolume = xml->getpar127("volume", GlobalPar.PVolume);
        int test = xml->getpar127("random_width", UNUSED);
        if (test < 64) // new Yoshi type
        {
            GlobalPar.PWidth = test;
            setGlobalPan(xml->getpar127("pan_pos", GlobalPar.PPanning), synth->getRuntime().panLaw);
            GlobalPar.PRandom = xml->getparbool("random_pan", GlobalPar.PRandom);
        }
        else // legacy
        {
            setGlobalPan(xml->getpar127("panning", GlobalPar.PPanning), synth->getRuntime().panLaw);

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
            xml->getpar127("velocity_sensing", GlobalPar.PAmpVelocityScaleFunction);
        GlobalPar.Fadein_adjustment = xml->getpar127("fadein_adjustment", GlobalPar.Fadein_adjustment);
        GlobalPar.PPunchStrength =
            xml->getpar127("punch_strength", GlobalPar.PPunchStrength);
        GlobalPar.PPunchTime = xml->getpar127("punch_time", GlobalPar.PPunchTime);
        GlobalPar.PPunchStretch = xml->getpar127("punch_stretch", GlobalPar.PPunchStretch);
        GlobalPar.PPunchVelocitySensing =
            xml->getpar127("punch_velocity_sensing", GlobalPar.PPunchVelocitySensing);
        GlobalPar.Hrandgrouping =
            xml->getpar127("harmonic_randomness_grouping", GlobalPar.Hrandgrouping);

        if (xml->enterbranch("AMPLITUDE_ENVELOPE"))
        {
            GlobalPar.AmpEnvelope->getfromXML(xml);
            xml->exitbranch();
        }

        if (xml->enterbranch("AMPLITUDE_LFO"))
        {
            GlobalPar.AmpLfo->getfromXML(xml);
            xml->exitbranch();
        }

        xml->exitbranch();
    }

    if (xml->enterbranch("FREQUENCY_PARAMETERS"))
    {
        GlobalPar.PDetune = xml->getpar("detune", GlobalPar.PDetune, 0, 16383);
        GlobalPar.PCoarseDetune =
            xml->getpar("coarse_detune", GlobalPar.PCoarseDetune, 0, 16383);
        GlobalPar.PDetuneType =
            xml->getpar127("detune_type", GlobalPar.PDetuneType);

        GlobalPar.PBandwidth = xml->getpar127("bandwidth", GlobalPar.PBandwidth);

        xml->enterbranch("FREQUENCY_ENVELOPE");
            GlobalPar.FreqEnvelope->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("FREQUENCY_LFO");
            GlobalPar.FreqLfo->getfromXML(xml);
        xml->exitbranch();

        xml->exitbranch();
    }


    if (xml->enterbranch("FILTER_PARAMETERS"))
    {
        GlobalPar.PFilterVelocityScale =
            xml->getpar127("velocity_sensing_amplitude", GlobalPar.PFilterVelocityScale);
        GlobalPar.PFilterVelocityScaleFunction =
            xml->getpar127("velocity_sensing", GlobalPar.PFilterVelocityScaleFunction);

        xml->enterbranch("FILTER");
            GlobalPar.GlobalFilter->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("FILTER_ENVELOPE");
            GlobalPar.FilterEnvelope->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("FILTER_LFO");
            GlobalPar.FilterLfo->getfromXML(xml);
        xml->exitbranch();

        xml->exitbranch();
    }

    if (xml->enterbranch("RESONANCE"))
    {
        GlobalPar.Reson->getfromXML(xml);
        xml->exitbranch();
    }

    for (int nvoice = 0; nvoice < NUM_VOICES; nvoice++)
    {
        VoicePar[nvoice].Enabled=0;
        if (xml->enterbranch("VOICE", nvoice) == 0)
            continue;
        getfromXMLsection(xml, nvoice);
        xml->exitbranch();
    }
}

void ADnoteParameters::getfromXMLsection(XMLwrapper *xml, int n)
{
    int nvoice = n;
    if (nvoice >= NUM_VOICES) return;

    VoicePar[nvoice].Enabled = xml->getparbool("enabled", 0);

    VoicePar[nvoice].Unison_size =
        xml->getpar127("unison_size", VoicePar[nvoice].Unison_size);
    VoicePar[nvoice].Unison_frequency_spread =
        xml->getpar127("unison_frequency_spread", VoicePar[nvoice].Unison_frequency_spread);
    VoicePar[nvoice].Unison_stereo_spread =
        xml->getpar127("unison_stereo_spread", VoicePar[nvoice].Unison_stereo_spread);
    VoicePar[nvoice].Unison_vibratto =
        xml->getpar127("unison_vibratto", VoicePar[nvoice].Unison_vibratto);
    VoicePar[nvoice].Unison_vibratto_speed =
        xml->getpar127("unison_vibratto_speed", VoicePar[nvoice].Unison_vibratto_speed);
    VoicePar[nvoice].Unison_invert_phase =
        xml->getpar127("unison_invert_phase", VoicePar[nvoice].Unison_invert_phase);
    VoicePar[nvoice].Unison_phase_randomness =
        xml->getpar127("unison_phase_randomness", VoicePar[nvoice].Unison_phase_randomness);

    VoicePar[nvoice].Type = xml->getpar127("type", VoicePar[nvoice].Type);
    VoicePar[nvoice].PDelay = xml->getpar127("delay", VoicePar[nvoice].PDelay);
    VoicePar[nvoice].Presonance = xml->getparbool("resonance", VoicePar[nvoice].Presonance);

    VoicePar[nvoice].PVoice =
        xml->getpar("input_voice", VoicePar[nvoice].PVoice, -1, nvoice - 1);
    VoicePar[nvoice].Pextoscil = xml->getpar("ext_oscil", -1, -1, nvoice - 1);
    VoicePar[nvoice].PextFMoscil = xml->getpar("ext_fm_oscil", -1, -1,nvoice - 1);

    VoicePar[nvoice].Poscilphase =
        xml->getpar127("oscil_phase", VoicePar[nvoice].Poscilphase);
    VoicePar[nvoice].PFMoscilphase =
        xml->getpar127("oscil_fm_phase", VoicePar[nvoice].PFMoscilphase);

    VoicePar[nvoice].PFilterEnabled =
        xml->getparbool("filter_enabled",VoicePar[nvoice].PFilterEnabled);
    VoicePar[nvoice].Pfilterbypass =
        xml->getparbool("filter_bypass",VoicePar[nvoice].Pfilterbypass);

    VoicePar[nvoice].PFMEnabled = xml->getpar127("fm_enabled",VoicePar[nvoice].PFMEnabled);

    if (xml->enterbranch("OSCIL"))
    {
        VoicePar[nvoice].POscil->getfromXML(xml);
        xml->exitbranch();
    }

    if (xml->enterbranch("AMPLITUDE_PARAMETERS"))
    {
        int test = xml->getpar127("random_width", UNUSED);
        if (test < 64) // new Yoshi type
        {
            VoicePar[nvoice].PWidth = test;
            setVoicePan(nvoice, xml->getpar127("pan_pos", VoicePar[nvoice].PPanning), synth->getRuntime().panLaw);
            VoicePar[nvoice].PRandom = xml->getparbool("random_pan", VoicePar[nvoice].PRandom);
        }
        else  // legacy
        {
            setVoicePan(nvoice, xml->getpar127("panning", VoicePar[nvoice].PPanning), synth->getRuntime().panLaw);
            if (VoicePar[nvoice].PPanning == 0)
            {
                VoicePar[nvoice].PPanning = 64;
                VoicePar[nvoice].PRandom = true;
                VoicePar[nvoice].PWidth = 63;
            }
        }

        VoicePar[nvoice].PVolume = xml->getpar127("volume", VoicePar[nvoice].PVolume);
        VoicePar[nvoice].PVolumeminus =
            xml->getparbool("volume_minus", VoicePar[nvoice].PVolumeminus);
        VoicePar[nvoice].PAmpVelocityScaleFunction =
            xml->getpar127("velocity_sensing", VoicePar[nvoice].PAmpVelocityScaleFunction);

        VoicePar[nvoice].PAmpEnvelopeEnabled =
            xml->getparbool("amp_envelope_enabled",VoicePar[nvoice].PAmpEnvelopeEnabled);
        if (xml->enterbranch("AMPLITUDE_ENVELOPE"))
        {
            VoicePar[nvoice].AmpEnvelope->getfromXML(xml);
            xml->exitbranch();
        }

        VoicePar[nvoice].PAmpLfoEnabled =
            xml->getparbool("amp_lfo_enabled",VoicePar[nvoice].PAmpLfoEnabled);
        if (xml->enterbranch("AMPLITUDE_LFO"))
        {
            VoicePar[nvoice].AmpLfo->getfromXML(xml);
            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if (xml->enterbranch("FREQUENCY_PARAMETERS"))
    {
        VoicePar[nvoice].Pfixedfreq =
            xml->getparbool("fixed_freq", VoicePar[nvoice].Pfixedfreq);
        VoicePar[nvoice].PfixedfreqET =
            xml->getpar127("fixed_freq_et", VoicePar[nvoice].PfixedfreqET);
        VoicePar[nvoice].PBendAdjust =
            xml->getpar127("bend_adjust", VoicePar[nvoice].PBendAdjust);
        VoicePar[nvoice].POffsetHz =
            xml->getpar127("offset_hz", VoicePar[nvoice].POffsetHz);


        VoicePar[nvoice].PDetune =
            xml->getpar("detune", VoicePar[nvoice].PDetune, 0, 16383);

        VoicePar[nvoice].PCoarseDetune =
            xml->getpar("coarse_detune", VoicePar[nvoice].PCoarseDetune, 0, 16383);
        VoicePar[nvoice].PDetuneType =
            xml->getpar127("detune_type", VoicePar[nvoice].PDetuneType);

        VoicePar[nvoice].PFreqEnvelopeEnabled =
            xml->getparbool("freq_envelope_enabled", VoicePar[nvoice].PFreqEnvelopeEnabled);
        if (xml->enterbranch("FREQUENCY_ENVELOPE"))
        {
            VoicePar[nvoice].FreqEnvelope->getfromXML(xml);
            xml->exitbranch();
        }

        VoicePar[nvoice].PFreqLfoEnabled =
            xml->getparbool("freq_lfo_enabled", VoicePar[nvoice].PFreqLfoEnabled);
        if (xml->enterbranch("FREQUENCY_LFO"))
        {
            VoicePar[nvoice].FreqLfo->getfromXML(xml);
            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if (xml->enterbranch("FILTER_PARAMETERS"))
    {
        VoicePar[nvoice].PFilterVelocityScale =
            xml->getpar127("velocity_sensing_amplitude",
            VoicePar[nvoice].PFilterVelocityScale);
        VoicePar[nvoice].PFilterVelocityScaleFunction =
            xml->getpar127("velocity_sensing",
            VoicePar[nvoice].PFilterVelocityScaleFunction);

        if (xml->enterbranch("FILTER"))
        {
            VoicePar[nvoice].VoiceFilter->getfromXML(xml);
            xml->exitbranch();
        }

        VoicePar[nvoice].PFilterEnvelopeEnabled =
            xml->getparbool("filter_envelope_enabled", VoicePar[nvoice].PFilterEnvelopeEnabled);
        if (xml->enterbranch("FILTER_ENVELOPE"))
        {
            VoicePar[nvoice].FilterEnvelope->getfromXML(xml);
            xml->exitbranch();
        }

        VoicePar[nvoice].PFilterLfoEnabled =
            xml->getparbool("filter_lfo_enabled", VoicePar[nvoice].PFilterLfoEnabled);
        if (xml->enterbranch("FILTER_LFO"))
        {
            VoicePar[nvoice].FilterLfo->getfromXML(xml);
            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if (xml->enterbranch("FM_PARAMETERS"))
    {
        VoicePar[nvoice].PFMVoice =
            xml->getpar("input_voice", VoicePar[nvoice].PFMVoice, -1, nvoice - 1);

        VoicePar[nvoice].PFMVolume = xml->getpar127("volume", VoicePar[nvoice].PFMVolume);
        VoicePar[nvoice].PFMVolumeDamp =
            xml->getpar127("volume_damp", VoicePar[nvoice].PFMVolumeDamp);
        VoicePar[nvoice].PFMVelocityScaleFunction =
            xml->getpar127("velocity_sensing", VoicePar[nvoice].PFMVelocityScaleFunction);

        VoicePar[nvoice].PFMAmpEnvelopeEnabled =
            xml->getparbool("amp_envelope_enabled", VoicePar[nvoice].PFMAmpEnvelopeEnabled);
        if (xml->enterbranch("AMPLITUDE_ENVELOPE"))
        {
            VoicePar[nvoice].FMAmpEnvelope->getfromXML(xml);
            xml->exitbranch();
        }

        if (xml->enterbranch("MODULATOR"))
        {
            bool loadFMFreqParams = true;
            VoicePar[nvoice].PFMDetuneFromBaseOsc =
                xml->getparbool("detune_from_base_osc", 127);
            if (VoicePar[nvoice].PFMDetuneFromBaseOsc == 127) {
                // In the past it was not possible to choose whether to include
                // detuning from the base oscillator. For local modulators it
                // was always enabled, for imported voice modulators it was
                // always disabled. To load old patches correctly, we apply this
                // old behavior here if the XML element is missing from the
                // patch. New patches will always save one or the other.
                //
                // In a similar fashion, it was not possible to apply frequency
                // parameters to imported voice modulators in the past, however
                // it was possible to save them if you edited them before
                // switching to an imported voice. Now that frequency parameters
                // are respected, we need to ignore those parameters for old
                // instruments that saved them, but didn't use them, otherwise
                // the instrument will sound different.
                if (VoicePar[nvoice].PFMVoice >= 0) {
                    VoicePar[nvoice].PFMDetuneFromBaseOsc = 0;
                    loadFMFreqParams = false;

                    // In the past the fixed frequency of the imported voice was
                    // respected. Now, the fixed frequency of the modulator is
                    // respected. So if we load an old patch, fetch that setting
                    // from the imported voice.
                    VoicePar[nvoice].PFMFixedFreq =
                        VoicePar[VoicePar[nvoice].PFMVoice].Pfixedfreq;
                } else {
                    VoicePar[nvoice].PFMDetuneFromBaseOsc = 1;
                }
            }
            if (loadFMFreqParams) {
                VoicePar[nvoice].PFMDetune =
                    xml->getpar("detune",VoicePar[nvoice].PFMDetune, 0, 16383);
                VoicePar[nvoice].PFMCoarseDetune =
                    xml->getpar("coarse_detune", VoicePar[nvoice].PFMCoarseDetune, 0, 16383);
                VoicePar[nvoice].PFMDetuneType =
                    xml->getpar127("detune_type", VoicePar[nvoice].PFMDetuneType);

                VoicePar[nvoice].PFMFreqEnvelopeEnabled =
                    xml->getparbool("freq_envelope_enabled",
                                    VoicePar[nvoice].PFMFreqEnvelopeEnabled);
                VoicePar[nvoice].PFMFixedFreq = xml->getparbool("fixed_freq", VoicePar[nvoice].PFMFixedFreq);
                if (xml->enterbranch("FREQUENCY_ENVELOPE"))
                {
                    VoicePar[nvoice].FMFreqEnvelope->getfromXML(xml);
                    xml->exitbranch();
                }
            }

            if (xml->enterbranch("OSCIL"))
            {
                VoicePar[nvoice].POscilFM->getfromXML(xml);
                xml->exitbranch();
            }

            xml->exitbranch();
        }
        xml->exitbranch();
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
                def = FADEIN_ADJUSTMENT_SCALE;

            case ADDSYNTH::control::punchStrength: // just ensures it doesn't get caught by default
                break;

            case ADDSYNTH::control::punchDuration:
                def = 60;
                break;

            case ADDSYNTH::control::punchStretch:
                def = 64;
                break;

            case ADDSYNTH::control::punchVelocity:
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
