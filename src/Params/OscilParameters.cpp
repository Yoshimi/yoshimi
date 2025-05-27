/*
    OscilGen.h - Waveform generator for ADnote

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019 Will Godfrey & others.
    Copyright 2019-2020 Kristian Amlie
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

    This file is derivative of original ZynAddSubFX code.
*/

#include <math.h>

#include "Params/OscilParameters.h"
#include "Misc/SynthEngine.h"
#include "Misc/XMLStore.h"


OscilParameters::OscilParameters(fft::Calc const& fft, SynthEngine& _synth)
    : ParamBase{_synth}
    , basefuncSpectrum{fft.spectrumSize()}
{
    defaults();
}


void OscilParameters::updatebasefuncSpectrum(fft::Spectrum const& src)
{
    basefuncSpectrum = src;
}

void OscilParameters::defaults()
{
    basefuncSpectrum.reset();

    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
    {
        Phmag[i] = 64;
        Phphase[i] = 64;
    }
    Phmag[0] = 127;
    Phmagtype = 0;
    Prand = 64;    // no randomness by default

    Pcurrentbasefunc = OSCILLATOR::wave::sine;
    Pbasefuncpar = 64;

    Pbasefuncmodulation = 0;
    Pbasefuncmodulationpar1 = 64;
    Pbasefuncmodulationpar2 = 64;
    Pbasefuncmodulationpar3 = 32;

    Pmodulation = 0;
    Pmodulationpar1 = 64;
    Pmodulationpar2 = 64;
    Pmodulationpar3 = 32;

    Pwaveshapingfunction = 0;
    Pwaveshaping = 64;
    Pfiltertype = 0;
    Pfilterpar1 = 64;
    Pfilterpar2 = 64;
    Pfilterbeforews = 0;
    Psatype = 0;
    Psapar = 64;

    Pamprandpower = 64;
    Pamprandtype = 0;

    Pharmonicshift = 0;
    Pharmonicshiftfirst = 0;

    Padaptiveharmonics = 0;
    Padaptiveharmonicspower = 100;
    Padaptiveharmonicsbasefreq = 128;
    Padaptiveharmonicspar = 50;
}

void OscilParameters::add2XML(XMLtree& xml)
{
    xml.addPar_int("harmonic_mag_type", Phmagtype);

    xml.addPar_int("base_function"                , Pcurrentbasefunc);
    xml.addPar_int("base_function_par"            , Pbasefuncpar);
    xml.addPar_int("base_function_modulation"     , Pbasefuncmodulation);
    xml.addPar_int("base_function_modulation_par1", Pbasefuncmodulationpar1);
    xml.addPar_int("base_function_modulation_par2", Pbasefuncmodulationpar2);
    xml.addPar_int("base_function_modulation_par3", Pbasefuncmodulationpar3);

    xml.addPar_int("modulation"     , Pmodulation);
    xml.addPar_int("modulation_par1", Pmodulationpar1);
    xml.addPar_int("modulation_par2", Pmodulationpar2);
    xml.addPar_int("modulation_par3", Pmodulationpar3);

    xml.addPar_int("wave_shaping"              , Pwaveshaping);
    xml.addPar_int("wave_shaping_function"     , Pwaveshapingfunction);
    xml.addPar_int("filter_before_wave_shaping", Pfilterbeforews);

    xml.addPar_int("filter_type"               , Pfiltertype);
    xml.addPar_int("filter_par1"               , Pfilterpar1);
    xml.addPar_int("filter_par2"               , Pfilterpar2);

    xml.addPar_int("spectrum_adjust_type"      , Psatype);
    xml.addPar_int("spectrum_adjust_par"       , Psapar);

    xml.addPar_int("rand"                      , Prand);
    xml.addPar_int("amp_rand_type"             , Pamprandtype);
    xml.addPar_int("amp_rand_power"            , Pamprandpower);

    xml.addPar_int("harmonic_shift"            , Pharmonicshift);
    xml.addPar_bool("harmonic_shift_first"     , Pharmonicshiftfirst);

    xml.addPar_int("adaptive_harmonics"        , Padaptiveharmonics);
    xml.addPar_int("adaptive_harmonics_base_frequency", Padaptiveharmonicsbasefreq);
    xml.addPar_int("adaptive_harmonics_power"  , Padaptiveharmonicspower);
    xml.addPar_int("adaptive_harmonics_par"    , Padaptiveharmonicspar);

    XMLtree xmlHarmonics = xml.addElm("HARMONICS");
        for (uint n = 0; n < MAX_AD_HARMONICS; ++n)
        {
            if (Phmag[n] == 64 and Phphase[n] == 64)
                continue;
            XMLtree xmlHarm = xmlHarmonics.addElm("HARMONIC", n+1);
                xmlHarm.addPar_int("mag"  , Phmag[n]);
                xmlHarm.addPar_int("phase", Phphase[n]);
        }

    if (Pcurrentbasefunc == OSCILLATOR::wave::user)
    {
        float max{0.0};
        for (size_t i = 0; i < basefuncSpectrum.size(); ++i)
        {
            if (max < fabsf(basefuncSpectrum.c(i)))
                max = fabsf(basefuncSpectrum.c(i));
            if (max < fabsf(basefuncSpectrum.s(i)))
                max = fabsf(basefuncSpectrum.s(i));
        }
        if (max < 0.00000001)
            max = 1.0;

        XMLtree xmlBaseFunc = xml.addElm("BASE_FUNCTION");
            for (size_t i = 1; i < basefuncSpectrum.size(); ++i)
            {
                float xc = basefuncSpectrum.c(i) / max;
                float xs = basefuncSpectrum.s(i) / max;
                if (fabsf(xs) > 0.00001 and fabsf(xs) > 0.00001)
                {
                    XMLtree xmlHarm = xmlBaseFunc.addElm("BF_HARMONIC", i);
                        xmlHarm.addPar_real("cos", xc);
                        xmlHarm.addPar_real("sin", xs);
                }
            }
    }
}


void OscilParameters::getfromXML(XMLtree& xml)
{
    assert(xml);
    Phmagtype = xml.getPar_127("harmonic_mag_type", Phmagtype);

    Pcurrentbasefunc = xml.getPar_127("base_function", Pcurrentbasefunc);
    Pbasefuncpar     = xml.getPar_127("base_function_par", Pbasefuncpar);

    Pbasefuncmodulation     = xml.getPar_127("base_function_modulation"     , Pbasefuncmodulation);
    Pbasefuncmodulationpar1 = xml.getPar_127("base_function_modulation_par1", Pbasefuncmodulationpar1);
    Pbasefuncmodulationpar2 = xml.getPar_127("base_function_modulation_par2", Pbasefuncmodulationpar2);
    Pbasefuncmodulationpar3 = xml.getPar_127("base_function_modulation_par3", Pbasefuncmodulationpar3);

    Pmodulation             = xml.getPar_127("modulation"              , Pmodulation);
    Pmodulationpar1         = xml.getPar_127("modulation_par1"         , Pmodulationpar1);
    Pmodulationpar2         = xml.getPar_127("modulation_par2"         , Pmodulationpar2);
    Pmodulationpar3         = xml.getPar_127("modulation_par3"         , Pmodulationpar3);

    Pwaveshaping            = xml.getPar_127("wave_shaping"            , Pwaveshaping);
    Pwaveshapingfunction    = xml.getPar_127("wave_shaping_function"   , Pwaveshapingfunction);
    Pfilterbeforews         = xml.getPar_127("filter_before_wave_shaping", Pfilterbeforews);

    Pfiltertype             = xml.getPar_127("filter_type"             , Pfiltertype);
    Pfilterpar1             = xml.getPar_127("filter_par1"             , Pfilterpar1);
    Pfilterpar2             = xml.getPar_127("filter_par2"             , Pfilterpar2);

    Prand                   = xml.getPar_127("rand"                    , Prand);
    Pamprandtype            = xml.getPar_127("amp_rand_type"           , Pamprandtype);
    Pamprandpower           = xml.getPar_127("amp_rand_power"          , Pamprandpower);

    Psatype                 = xml.getPar_127("spectrum_adjust_type"    , Psatype);
    Psapar                  = xml.getPar_127("spectrum_adjust_par"     , Psapar);

    Pharmonicshift          = xml.getPar_int ("harmonic_shift"         , Pharmonicshift, -64, 64);
    Pharmonicshiftfirst     = xml.getPar_bool("harmonic_shift_first"   , Pharmonicshiftfirst);

    Padaptiveharmonics      = xml.getPar_int ("adaptive_harmonics"     , Padaptiveharmonics     , 0, 127);
    Padaptiveharmonicsbasefreq=xml.getPar_int("adaptive_harmonics_base_frequency", Padaptiveharmonicsbasefreq,0,255);
    Padaptiveharmonicspower = xml.getPar_int("adaptive_harmonics_power", Padaptiveharmonicspower, 0, 200);
    Padaptiveharmonicspar   = xml.getPar_int("adaptive_harmonics_par"  , Padaptiveharmonicspar  , 0, 100);

    if (XMLtree xmlHarmonics = xml.getElm("HARMONICS"))
    {
        Phmag[0] = 64;
        Phphase[0] = 64;
        for (uint n = 0; n < MAX_AD_HARMONICS; ++n)
            if (XMLtree xmlHarm = xmlHarmonics.getElm("HARMONIC", n+1))
            {
                Phmag[n]   = xmlHarm.getPar_127("mag"  , 64);
                Phphase[n] = xmlHarm.getPar_127("phase", 64);
            }
    }

    if (XMLtree xmlBaseFunc = xml.getElm("BASE_FUNCTION"))
    {
        for (uint i = 1; i < basefuncSpectrum.size(); ++i)
            if (XMLtree xmlHarm = xmlBaseFunc.getElm("BF_HARMONIC", i))
            {
                basefuncSpectrum.c(i) = xmlHarm.getPar_real("cos", 0.0);
                basefuncSpectrum.s(i) = xmlHarm.getPar_real("sin", 0.0);
            }

        float max = 0.0;

        basefuncSpectrum.c(0) = 0.0;
        for (size_t i = 0; i < basefuncSpectrum.size(); ++i)
        {
            if (max < fabsf(basefuncSpectrum.c(i)))
                max = fabsf(basefuncSpectrum.c(i));
            if (max < fabsf(basefuncSpectrum.s(i)))
                max = fabsf(basefuncSpectrum.s(i));
        }
        if (max < 0.00000001)
            max = 1.0;

        for (size_t i = 0; i < basefuncSpectrum.size(); ++i)
        {
            if (basefuncSpectrum.c(i))
                basefuncSpectrum.c(i) /= max;
            if (basefuncSpectrum.s(i))
                basefuncSpectrum.s(i) /= max;
        }
    }

    paramsChanged();
}


float OscilParameters::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int insert = getData->data.insert;

    uchar type = 0;

    // oscillator defaults
    int min = 0;
    int max = 127;
    float def = 0;
    type |= TOPLEVEL::type::Integer;
    uchar learnable = TOPLEVEL::type::Learnable;
    type |= learnable;

    if (insert == TOPLEVEL::insert::harmonicAmplitude || insert == TOPLEVEL::insert::harmonicPhase)
    { // do harmonics stuff
        if (insert == TOPLEVEL::insert::harmonicAmplitude && control == 0)
            def = 127;
        else
            def = 64;
        getData->data.type = type;
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
        case OSCILLATOR::control::phaseRandomness:
            break;
        case OSCILLATOR::control::magType:
            max = 4;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::harmonicAmplitudeRandomness:
            def = 64;
            break;
        case OSCILLATOR::control::harmonicRandomnessType:
            max = 2;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::baseFunctionParameter:
            min = -64;
            max = 63;
            break;
        case OSCILLATOR::control::baseFunctionType:
            max = OSCILLATOR::wave::hyperSec;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::baseModulationParameter1:
            def = 64;
            break;
        case OSCILLATOR::control::baseModulationParameter2:
            def = 64;
            break;
        case OSCILLATOR::control::baseModulationParameter3:
            def = 32;
            break;
        case OSCILLATOR::control::baseModulationType:
            max = 3;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::autoClear:
            max = 1;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::useAsBaseFunction:
            max = 1;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::waveshapeParameter:
            min = -64;
            max = 63;
            break;
        case OSCILLATOR::control::waveshapeType:
            max = 10;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::filterParameter1:
            def = 64;
            break;
        case OSCILLATOR::control::filterParameter2:
            def = 64;
            break;
        case OSCILLATOR::control::filterBeforeWaveshape:
            max = 1;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::filterType:
            max = 13;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::modulationParameter1:
            def = 64;
            break;
        case OSCILLATOR::control::modulationParameter2:
            def = 64;
            break;
        case OSCILLATOR::control::modulationParameter3:
            def = 32;
            break;
        case OSCILLATOR::control::modulationType:
            max = 3;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::spectrumAdjustParameter:
            def = 64;
            break;
        case OSCILLATOR::control::spectrumAdjustType:
            max = 3;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::harmonicShift:
            min = -64;
            max = 64;
            break;
        case OSCILLATOR::control::clearHarmonicShift:
            max = 1;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::shiftBeforeWaveshapeAndFilter:
            max = 1;
            type &= ~learnable;
            break;

        case OSCILLATOR::control::adaptiveHarmonicsParameter:
            max = 100;
            def = 50;
            break;
        case OSCILLATOR::control::adaptiveHarmonicsBase:
            max = 255;
            def = 128;
            break;
        case OSCILLATOR::control::adaptiveHarmonicsPower:
            max = 200;
            def = 100;
            break;
        case OSCILLATOR::control::adaptiveHarmonicsType:
            max = 8;
            type &= ~learnable;
            break;

        case OSCILLATOR::control::clearHarmonics:
            max = 1;
            type &= ~learnable;
            break;
        case OSCILLATOR::control::convertToSine:
            max = 1;
            type &= ~learnable;
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
