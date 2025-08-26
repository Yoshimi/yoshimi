/*
    SUBnoteParameters.cpp - Parameters for SUBnote (SUBsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert
    Copyright 2017-2019, Will Godfrey
    Copyright 2020-2022 Kristian Amlie & others
    Copyright 2023 Will Godfrey and others

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

    This file is a derivative of a ZynAddSubFX original.

*/

#include "Misc/SynthEngine.h"
#include "Params/SUBnoteParameters.h"
#include "Params/LFOParams.h"
#include "Misc/NumericFuncs.h"
#include "Misc/XMLStore.h"

using std::make_unique;

using func::setAllPan;
using func::power;


SUBnoteParameters::SUBnoteParameters(SynthEngine& _synth) : ParamBase{_synth}
{
    AmpEnvelope = make_unique<EnvelopeParams>(64, 1, synth);
    AmpEnvelope->ADSRinit_dB(0, 40, 127, 25);
    AmpLfo = make_unique<LFOParams>(80, 0, 64, 0, 0, 0, false, 1, synth);

    FreqEnvelope = make_unique<EnvelopeParams>(64, 0, synth);
    FreqEnvelope->ASRinit(30, 50, 64, 60);
    FreqLfo = make_unique<LFOParams>(70, 0, 64, 0, 0, 0, false, 0, synth);

    BandWidthEnvelope = make_unique<EnvelopeParams>(64, 0, synth);
    BandWidthEnvelope->ASRinit_bw(100, 70, 64, 60);

    GlobalFilter = make_unique<FilterParams>(2, 80, 40, 0, synth);
    GlobalFilterEnvelope = make_unique<EnvelopeParams>(0, 1, synth);
    GlobalFilterEnvelope->ADSRinit_filter(64, 40, 64, 70, 60, 64);
    GlobalFilterLfo = make_unique<LFOParams>(80, 0, 64, 0, 0, 0, false, 2, synth);
    defaults();
}


void SUBnoteParameters::defaults()
{
    PVolume = 96;
    setPan(PPanning = 64, synth.getRuntime().panLaw);
    PRandom = false;
    PWidth = 63;
    PAmpVelocityScaleFunction = 90;
    Pfixedfreq = 0;
    PfixedfreqET = 0;
    PBendAdjust = 88; // 64 + 24
    POffsetHz = 64;
    Pnumstages = 2;
    Pbandwidth = 40;
    Phmagtype = 0;
    Pbwscale = 64;
    Pstereo = true;
    Pstart = 1;

    PDetune = 8192;
    PCoarseDetune = 0;
    PDetuneType = 1;
    PFreqEnvelopeEnabled = false;
    PFreqLfoEnabled = false;
    PBandWidthEnvelopeEnabled = false;

    POvertoneSpread.type = 0;
    POvertoneSpread.par1 = 0;
    POvertoneSpread.par2 = 0;
    POvertoneSpread.par3 = 0;
    updateFrequencyMultipliers();

    for (int n = 0; n < MAX_SUB_HARMONICS; ++n)
    {
        Phmag[n] = 0;
        Phrelbw[n] = 64;
    }
    Phmag[0] = 127;

    PGlobalFilterEnabled = false;
    PGlobalFilterVelocityScale = 64;
    PGlobalFilterVelocityScaleFunction = 64;

    AmpEnvelope->defaults();
    AmpLfo->defaults();
    FreqEnvelope->defaults();
    FreqLfo->defaults();
    BandWidthEnvelope->defaults();
    GlobalFilter->defaults();
    GlobalFilterEnvelope->defaults();
    GlobalFilterLfo->defaults();
}


void SUBnoteParameters::setPan(char pan, unsigned char panLaw)
{
    PPanning = pan;
    if (!PRandom)
        setAllPan(PPanning, pangainL, pangainR, panLaw);
    else
        pangainL = pangainR = 0.7f;
}


void SUBnoteParameters::add2XML(XMLtree& xmlSubSynth)
{
    xmlSubSynth.addPar_int("num_stages"       , Pnumstages);
    xmlSubSynth.addPar_int("harmonic_mag_type", Phmagtype);
    xmlSubSynth.addPar_int("start"            , Pstart);

    XMLtree xmlHarmonics = xmlSubSynth.addElm("HARMONICS");
    for (uint i=0; i<MAX_SUB_HARMONICS; i++)
    {
        if (Phmag[i]==0 and not synth.getRuntime().xmlmax)
            continue;
        XMLtree xmlHarm = xmlHarmonics.addElm("HARMONIC",i);
        xmlHarm.addPar_int("mag"  , Phmag[i]);
        xmlHarm.addPar_int("relbw", Phrelbw[i]);
    }

    XMLtree xmlAmp = xmlSubSynth.addElm("AMPLITUDE_PARAMETERS");
        xmlAmp.addPar_bool("stereo", Pstereo);
        xmlAmp.addPar_int ("volume", PVolume);
        // Yoshimi format for random panning
        xmlAmp.addPar_int ("pan_pos"     , PPanning);
        xmlAmp.addPar_bool("random_pan"  , PRandom);
        xmlAmp.addPar_int ("random_width", PWidth);

        // support legacy format
        if (PRandom)
            xmlAmp.addPar_int("panning", 0);
        else
            xmlAmp.addPar_int("panning", PPanning);

        xmlAmp.addPar_int ("velocity_sensing", PAmpVelocityScaleFunction);

        XMLtree xmlEnv = xmlAmp.addElm("AMPLITUDE_ENVELOPE");
            AmpEnvelope->add2XML(xmlEnv);

        XMLtree xmlLfo = xmlAmp.addElm("AMPLITUDE_LFO");
            AmpLfo->add2XML(xmlLfo);

    XMLtree xmlFreq = xmlSubSynth.addElm("FREQUENCY_PARAMETERS");
        xmlFreq.addPar_bool("fixed_freq"   , Pfixedfreq);
        xmlFreq.addPar_int ("fixed_freq_et", PfixedfreqET);
        xmlFreq.addPar_int ("bend_adjust"  , PBendAdjust);
        xmlFreq.addPar_int ("offset_hz"    , POffsetHz);

        xmlFreq.addPar_int ("detune"              , PDetune);
        xmlFreq.addPar_int ("coarse_detune"       , PCoarseDetune);
        xmlFreq.addPar_int ("overtone_spread_type", POvertoneSpread.type);
        xmlFreq.addPar_int ("overtone_spread_par1", POvertoneSpread.par1);
        xmlFreq.addPar_int ("overtone_spread_par2", POvertoneSpread.par2);
        xmlFreq.addPar_int ("overtone_spread_par3", POvertoneSpread.par3);
        xmlFreq.addPar_int ("detune_type"         , PDetuneType);

        xmlFreq.addPar_int ("bandwidth"           , Pbandwidth);
        xmlFreq.addPar_int ("bandwidth_scale"     , Pbwscale);

        xmlFreq.addPar_bool("freq_envelope_enabled", PFreqEnvelopeEnabled);
        if (PFreqEnvelopeEnabled or synth.getRuntime().xmlmax)
        {
            XMLtree xmlEnv = xmlFreq.addElm("FREQUENCY_ENVELOPE");
                FreqEnvelope->add2XML(xmlEnv);
        }

        xmlFreq.addPar_bool("freq_lfo_enabled", PFreqLfoEnabled);
        if (PFreqLfoEnabled or synth.getRuntime().xmlmax)
        {
            XMLtree xmlLfo = xmlFreq.addElm("FREQUENCY_LFO");
                FreqLfo->add2XML(xmlLfo);
        }

        xmlFreq.addPar_bool("band_width_envelope_enabled", PBandWidthEnvelopeEnabled);
        if (PBandWidthEnvelopeEnabled or synth.getRuntime().xmlmax)
        {
            XMLtree xmlEnv = xmlFreq.addElm("BANDWIDTH_ENVELOPE");
                BandWidthEnvelope->add2XML(xmlEnv);
        }

    XMLtree xmlFilterParams = xmlSubSynth.addElm("FILTER_PARAMETERS");
        xmlFilterParams.addPar_bool("enabled", PGlobalFilterEnabled);
        if (PGlobalFilterEnabled or synth.getRuntime().xmlmax)
        {
            XMLtree xmlFilter = xmlFilterParams.addElm("FILTER");
                GlobalFilter->add2XML(xmlFilter);

            xmlFilterParams.addPar_int("filter_velocity_sensing",   PGlobalFilterVelocityScaleFunction);
            xmlFilterParams.addPar_int("filter_velocity_sensing_amplitude", PGlobalFilterVelocityScale);

            XMLtree xmlEnv = xmlFilterParams.addElm("FILTER_ENVELOPE");
                GlobalFilterEnvelope->add2XML(xmlEnv);

            XMLtree xmlLfo = xmlFilterParams.addElm("FILTER_LFO");
                GlobalFilterLfo->add2XML(xmlLfo);
        }
}


void SUBnoteParameters::updateFrequencyMultipliers()
{
    float par1 = POvertoneSpread.par1 / 255.0f;
    float par1pow = power<10>(-(1.0f - POvertoneSpread.par1 / 255.0f) * 3.0f);
    float par2 = POvertoneSpread.par2 / 255.0f;
    float par3 = 1.0f - POvertoneSpread.par3 / 255.0f;
    float result;
    float tmp = 0.0f;
    int   thresh = 0;

    for (int n = 0; n < MAX_SUB_HARMONICS; ++n)
    {
        float n1     = n + 1.0f;
        switch(POvertoneSpread.type)
        {
            case 1:
                thresh = (int)(100.0f * par2 * par2) + 1;
                if (n1 < thresh)
                    result = n1;
                else
                    result = n1 + 8.0f * (n1 - thresh) * par1pow;
                break;

            case 2:
                thresh = (int)(100.0f * par2 * par2) + 1;
                if (n1 < thresh)
                    result = n1;
                else
                    result = n1 + 0.9f * (thresh - n1) * par1pow;
                break;

            case 3:
                tmp = par1pow * 100.0f + 1.0f;
                result = powf(n / tmp, 1.0f - 0.8f * par2) * tmp + 1.0f;
                break;

            case 4:
                result = n * (1.0f - par1pow) +
                    powf(0.1f * n, 3.0f * par2 + 1.0f) *
                    10.0f * par1pow + 1.0f;
                break;

            case 5:
                result = n1 + 2.0f * sinf(n * par2 * par2 * PI * 0.999f) *
                    sqrt(par1pow);
                break;

            case 6:
                tmp    = powf(2.0f * par2, 2.0f) + 0.1f;
                result = n * powf(par1 * powf(0.8f * n, tmp) + 1.0f, tmp) +
                    1.0f;
                break;

            case 7:
                result = (n1 + par1) / (par1 + 1);
                break;

            default:
                result = n1;
                break;
        }
        float iresult = floor(result + 0.5f);
        POvertoneFreqMult[n] = iresult + par3 * (result - iresult);
    }
}


void SUBnoteParameters::getfromXML(XMLtree& xmlSubSynth)
{
    assert(xmlSubSynth);
    Pnumstages = xmlSubSynth.getPar_127("num_stages"       ,Pnumstages);
    Phmagtype  = xmlSubSynth.getPar_127("harmonic_mag_type",Phmagtype);
    Pstart     = xmlSubSynth.getPar_127("start"            ,Pstart);

    if (XMLtree xmlHarmonics = xmlSubSynth.getElm("HARMONICS"))
    {
        Phmag[0]=0; // default if disabled...

        for (uint i=0; i < MAX_SUB_HARMONICS; i++)
            if (XMLtree xmlHarm = xmlHarmonics.getElm("HARMONIC",i))
            {
                Phmag[i]  =xmlHarm.getPar_127("mag"  ,Phmag[i]);
                Phrelbw[i]=xmlHarm.getPar_127("relbw",Phrelbw[i]);
            }
    }

    if (XMLtree xmlAmp = xmlSubSynth.getElm("AMPLITUDE_PARAMETERS"))
    {
        Pstereo = xmlAmp.getPar_bool("stereo", Pstereo);
        PVolume = xmlAmp.getPar_127 ("volume", PVolume);
        int val = xmlAmp.getPar_127 ("random_width", UNUSED);
        if (val < 64)
        {// new Yoshimi format
            PWidth = val;
            setPan(xmlAmp.getPar_127("pan_pos",PPanning), synth.getRuntime().panLaw);
            PRandom = xmlAmp.getPar_bool("random_pan", PRandom);
        }
        else
        {// legacy
            setPan(xmlAmp.getPar_127("panning",PPanning), synth.getRuntime().panLaw);
            if (PPanning == 0)
            {
                PPanning = 64;
                PRandom = true;
                PWidth = 63;
            }
            else
                PRandom = false;
        }
        PAmpVelocityScaleFunction=xmlAmp.getPar_127("velocity_sensing",PAmpVelocityScaleFunction);

        if (XMLtree xmlEnv = xmlAmp.getElm("AMPLITUDE_ENVELOPE"))
            AmpEnvelope->getfromXML(xmlEnv);
        else
            AmpEnvelope->defaults();

        if (XMLtree xmlLfo = xmlAmp.getElm("AMPLITUDE_LFO"))
            AmpLfo->getfromXML(xmlLfo);
        else
            AmpLfo->defaults();
    }

    if (XMLtree xmlFreq = xmlSubSynth.getElm("FREQUENCY_PARAMETERS"))
    {
        Pfixedfreq   = xmlFreq.getPar_bool("fixed_freq"  ,Pfixedfreq);
        PfixedfreqET = xmlFreq.getPar_127("fixed_freq_et",PfixedfreqET);
        PBendAdjust  = xmlFreq.getPar_127("bend_adjust"  ,PBendAdjust);
        POffsetHz    = xmlFreq.getPar_127("offset_hz"    ,POffsetHz);

        PDetune      = xmlFreq.getPar_int("detune"       ,PDetune,      0,16383);
        PCoarseDetune= xmlFreq.getPar_int("coarse_detune",PCoarseDetune,0,16383);
        PDetuneType  = xmlFreq.getPar_127("detune_type"  ,PDetuneType);

        Pbandwidth   = xmlFreq.getPar_127("bandwidth"    ,Pbandwidth);
        Pbwscale     = xmlFreq.getPar_127("bandwidth_scale",Pbwscale);
        POvertoneSpread.type =
            xmlFreq.getPar_127("overtone_spread_type", POvertoneSpread.type);
        POvertoneSpread.par1 =
            xmlFreq.getPar_int("overtone_spread_par1", POvertoneSpread.par1, 0, 255);
        POvertoneSpread.par2 =
            xmlFreq.getPar_int("overtone_spread_par2", POvertoneSpread.par2, 0, 255);
        POvertoneSpread.par3 =
            xmlFreq.getPar_int("overtone_spread_par3", POvertoneSpread.par3, 0, 255);
        updateFrequencyMultipliers();

        PFreqEnvelopeEnabled=xmlFreq.getPar_bool("freq_envelope_enabled",PFreqEnvelopeEnabled);
        if (XMLtree xmlEnv = xmlFreq.getElm("FREQUENCY_ENVELOPE"))
            FreqEnvelope->getfromXML(xmlEnv);
        else
            FreqEnvelope->defaults();

        PFreqLfoEnabled=xmlFreq.getPar_bool("freq_lfo_enabled",PFreqLfoEnabled);
        if (XMLtree xmlLfo = xmlFreq.getElm("FREQUENCY_LFO"))
            FreqLfo->getfromXML(xmlLfo);
        else
            FreqLfo->defaults();

        PBandWidthEnvelopeEnabled=xmlFreq.getPar_bool("band_width_envelope_enabled",PBandWidthEnvelopeEnabled);
        if (XMLtree xmlEnv = xmlFreq.getElm("BANDWIDTH_ENVELOPE"))
            BandWidthEnvelope->getfromXML(xmlEnv);
        else
            BandWidthEnvelope->defaults();
    }

    if (XMLtree xmlFilterParams = xmlSubSynth.getElm("FILTER_PARAMETERS"))
    {
        PGlobalFilterEnabled  = xmlFilterParams.getPar_bool("enabled",PGlobalFilterEnabled);
        if (XMLtree xmlFilter = xmlFilterParams.getElm("FILTER"))
            GlobalFilter->getfromXML(xmlFilter);

        PGlobalFilterVelocityScaleFunction=xmlFilterParams.getPar_127("filter_velocity_sensing",PGlobalFilterVelocityScaleFunction);
        PGlobalFilterVelocityScale        =xmlFilterParams.getPar_127("filter_velocity_sensing_amplitude",PGlobalFilterVelocityScale);

        if (XMLtree xmlEnv = xmlFilterParams.getElm("FILTER_ENVELOPE"))
            GlobalFilterEnvelope->getfromXML(xmlEnv);
        else
            GlobalFilterEnvelope->defaults();

        if (XMLtree xmlLfo = xmlFilterParams.getElm("FILTER_LFO"))
            GlobalFilterLfo->getfromXML(xmlLfo);
        else
            GlobalFilterLfo->defaults();
    }
}


float SUBnoteParameters::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int insert = getData->data.insert;

    unsigned char type = 0;

    // subsynth defaults
    int min = 0;
    int max = 127;
    int def = 0;
    type |= TOPLEVEL::type::Integer;
    unsigned char learnable = TOPLEVEL::type::Learnable;
    type |= learnable;

    if (insert == TOPLEVEL::insert::harmonicAmplitude || insert == TOPLEVEL::insert::harmonicBandwidth)
    { // do harmonics stuff
        if (control >= MAX_SUB_HARMONICS)
        {
            getData->data.type = TOPLEVEL::type::Error;
            return 1;
        }

        if (insert == TOPLEVEL::insert::harmonicBandwidth)
            def = 64;
        else if (control == 0)
            def = 127;

        getData->data.type = type;

        switch (request)
        {
            case TOPLEVEL::type::Adjust:
                if (value < 0)
                    value = 0;
                else if (value > 127)
                    value = 127;
                break;
            case TOPLEVEL::type::Minimum:
                value = 0;
                break;
            case TOPLEVEL::type::Maximum:
                value = 127;
                break;
        }
        return value;
    }

    switch (control)
    {
        case SUBSYNTH::control::volume:
            def = 96;
            break;

        case SUBSYNTH::control::velocitySense:
            def = 90;
            break;

        case SUBSYNTH::control::panning:
            def = 64;
            break;

        case SUBSYNTH::control::enableRandomPan:
            max = 1;
            break;

        case SUBSYNTH::control::randomWidth:
            def = 63;
            max = 63;
            break;

        case SUBSYNTH::control::bandwidth:
            def = 40;
            break;

        case SUBSYNTH::control::bandwidthScale:
            min = -64;
            max = 63;
            break;

        case SUBSYNTH::control::enableBandwidthEnvelope:
            max = 1;
            break;

        case SUBSYNTH::control::detuneFrequency:
            min = -8192;
            max = 8191;
            break;

        case SUBSYNTH::control::equalTemperVariation:
            break;

        case SUBSYNTH::control::baseFrequencyAs440Hz:
            type &= ~learnable;
            max = 1;
            break;

        case SUBSYNTH::control::octave:
            min = -8;
            max = 7;
            break;

        case SUBSYNTH::control::detuneType:
            type &= ~learnable;
            min = 1;
            max = 4;
            break;

        case SUBSYNTH::control::coarseDetune:
            type &= ~learnable;
            min = -64;
            max = 63;
            break;

        case SUBSYNTH::control::pitchBendAdjustment:
            def = 88;
            break;

        case SUBSYNTH::control::pitchBendOffset:
            def = 64;
            break;

        case SUBSYNTH::control::enableFrequencyEnvelope:
            max = 1;
            break;

        case SUBSYNTH::control::overtoneParameter1:
        case SUBSYNTH::control::overtoneParameter2:
        case SUBSYNTH::control::overtoneForceHarmonics:
            max = 255;
            break;
        case SUBSYNTH::control::overtonePosition:
            type &= ~learnable;
            max = 7;
            break;

        case SUBSYNTH::control::enableFilter:
            max = 1;
            break;

        case SUBSYNTH::control::filterStages:
            type &= ~learnable;
            min = 1;
            def = 1;
            max = 5;
            break;

        case SUBSYNTH::control::magType:
            type &= ~learnable;
            max = 4;
            break;

        case SUBSYNTH::control::startPosition:
            type &= ~learnable;
            def = 1;
            max = 2;
            break;

        case SUBSYNTH::control::clearHarmonics:
            type &= ~learnable;
            max = 0;
            break;

        case SUBSYNTH::control::stereo:
            def = 1;
            max = 1;
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
