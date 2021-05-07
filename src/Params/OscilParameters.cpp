/*
    OscilGen.h - Waveform generator for ADnote

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019 Will Godfrey & others.
    Copyright 2019-2020 Kristian Amlie

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

#include <math.h>

#include "Misc/SynthEngine.h"
#include "Params/OscilParameters.h" // **** RHL ****

OscilParameters::OscilParameters(SynthEngine *_synth) :
    Presets(_synth),
    ADvsPAD(false)
{
    setpresettype("Poscilgen");
    FFTwrapper::newFFTFREQS(&basefuncFFTfreqs, MAX_OSCIL_SIZE);
    defaults();
}

OscilParameters::~OscilParameters()
{
    FFTwrapper::deleteFFTFREQS(&basefuncFFTfreqs);
}

void OscilParameters::updatebasefuncFFTfreqs(const FFTFREQS *src, int samples)
{
    memcpy(basefuncFFTfreqs.c, src->c, samples * sizeof(float));
    memcpy(basefuncFFTfreqs.s, src->s, samples * sizeof(float));
}

void OscilParameters::defaults()
{
    memset(basefuncFFTfreqs.s, 0, MAX_OSCIL_SIZE * sizeof(float));
    memset(basefuncFFTfreqs.c, 0, MAX_OSCIL_SIZE * sizeof(float));

    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
    {
        Phmag[i] = 64;
        Phphase[i] = 64;
    }
    Phmag[0] = 127;
    Phmagtype = 0;
    if (ADvsPAD)
        Prand = 127; // max phase randomness (useful if the oscil will be
                     // imported to a ADsynth from a PADsynth
    else
        Prand = 64; // no randomness

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

void OscilParameters::add2XML(XMLwrapper *xml)
{
    xml->addpar("harmonic_mag_type", Phmagtype);

    xml->addpar("base_function", Pcurrentbasefunc);
    xml->addpar("base_function_par", Pbasefuncpar);
    xml->addpar("base_function_modulation", Pbasefuncmodulation);
    xml->addpar("base_function_modulation_par1", Pbasefuncmodulationpar1);
    xml->addpar("base_function_modulation_par2", Pbasefuncmodulationpar2);
    xml->addpar("base_function_modulation_par3", Pbasefuncmodulationpar3);

    xml->addpar("modulation", Pmodulation);
    xml->addpar("modulation_par1", Pmodulationpar1);
    xml->addpar("modulation_par2", Pmodulationpar2);
    xml->addpar("modulation_par3", Pmodulationpar3);

    xml->addpar("wave_shaping", Pwaveshaping);
    xml->addpar("wave_shaping_function", Pwaveshapingfunction);

    xml->addpar("filter_type", Pfiltertype);
    xml->addpar("filter_par1", Pfilterpar1);
    xml->addpar("filter_par2", Pfilterpar2);
    xml->addpar("filter_before_wave_shaping", Pfilterbeforews);

    xml->addpar("spectrum_adjust_type", Psatype);
    xml->addpar("spectrum_adjust_par", Psapar);

    xml->addpar("rand", Prand);
    xml->addpar("amp_rand_type", Pamprandtype);
    xml->addpar("amp_rand_power", Pamprandpower);

    xml->addpar("harmonic_shift", Pharmonicshift);
    xml->addparbool("harmonic_shift_first", Pharmonicshiftfirst);

    xml->addpar("adaptive_harmonics", Padaptiveharmonics);
    xml->addpar("adaptive_harmonics_base_frequency", Padaptiveharmonicsbasefreq);
    xml->addpar("adaptive_harmonics_power", Padaptiveharmonicspower);
    xml->addpar("adaptive_harmonics_par", Padaptiveharmonicspar);

    xml->beginbranch("HARMONICS");
        for (int n = 0; n < MAX_AD_HARMONICS; ++n)
        {
            if (Phmag[n] == 64 && Phphase[n] == 64)
                continue;
            xml->beginbranch("HARMONIC", n + 1);
                xml->addpar("mag", Phmag[n]);
                xml->addpar("phase", Phphase[n]);
            xml->endbranch();
        }
    xml->endbranch();

    if (Pcurrentbasefunc == 127)
    {
        float max = 0.0;
        for (int i = 0; i < synth->halfoscilsize; ++i)
        {
            if (max < fabsf(basefuncFFTfreqs.c[i]))
                max = fabsf(basefuncFFTfreqs.c[i]);
            if (max < fabsf(basefuncFFTfreqs.s[i]))
                max = fabsf(basefuncFFTfreqs.s[i]);
        }
        if (max < 0.00000001)
            max = 1.0;

        xml->beginbranch("BASE_FUNCTION");
            for (int i = 1; i < synth->halfoscilsize; ++i)
            {
                float xc = basefuncFFTfreqs.c[i] / max;
                float xs = basefuncFFTfreqs.s[i] / max;
                if (fabsf(xs) > 0.00001 && fabsf(xs) > 0.00001)
                {
                    xml->beginbranch("BF_HARMONIC", i);
                        xml->addparreal("cos", xc);
                        xml->addparreal("sin", xs);
                    xml->endbranch();
                }
            }
        xml->endbranch();
    }
}


void OscilParameters::getfromXML(XMLwrapper *xml)
{

    Phmagtype = xml->getpar127("harmonic_mag_type", Phmagtype);

    Pcurrentbasefunc = xml->getpar127("base_function", Pcurrentbasefunc);
    Pbasefuncpar = xml->getpar127("base_function_par", Pbasefuncpar);

    Pbasefuncmodulation = xml->getpar127("base_function_modulation", Pbasefuncmodulation);
    Pbasefuncmodulationpar1 = xml->getpar127("base_function_modulation_par1", Pbasefuncmodulationpar1);
    Pbasefuncmodulationpar2 = xml->getpar127("base_function_modulation_par2", Pbasefuncmodulationpar2);
    Pbasefuncmodulationpar3 = xml->getpar127("base_function_modulation_par3", Pbasefuncmodulationpar3);

    Pmodulation = xml->getpar127("modulation", Pmodulation);
    Pmodulationpar1 = xml->getpar127("modulation_par1", Pmodulationpar1);
    Pmodulationpar2 = xml->getpar127("modulation_par2", Pmodulationpar2);
    Pmodulationpar3 = xml->getpar127("modulation_par3", Pmodulationpar3);

    Pwaveshaping = xml->getpar127("wave_shaping", Pwaveshaping);
    Pwaveshapingfunction = xml->getpar127("wave_shaping_function", Pwaveshapingfunction);

    Pfiltertype = xml->getpar127("filter_type", Pfiltertype);
    Pfilterpar1 = xml->getpar127("filter_par1", Pfilterpar1);
    Pfilterpar2 = xml->getpar127("filter_par2", Pfilterpar2);
    Pfilterbeforews = xml->getpar127("filter_before_wave_shaping", Pfilterbeforews);

    Psatype = xml->getpar127("spectrum_adjust_type", Psatype);
    Psapar = xml->getpar127("spectrum_adjust_par", Psapar);

    Prand = xml->getpar127("rand", Prand);
    Pamprandtype = xml->getpar127("amp_rand_type", Pamprandtype);
    Pamprandpower = xml->getpar127("amp_rand_power", Pamprandpower);

    Pharmonicshift = xml->getpar("harmonic_shift", Pharmonicshift, -64, 64);
    Pharmonicshiftfirst = xml->getparbool("harmonic_shift_first", Pharmonicshiftfirst);

    Padaptiveharmonics = xml->getpar("adaptive_harmonics", Padaptiveharmonics, 0, 127);
    Padaptiveharmonicsbasefreq = xml->getpar("adaptive_harmonics_base_frequency", Padaptiveharmonicsbasefreq,0,255);
    Padaptiveharmonicspower = xml->getpar("adaptive_harmonics_power", Padaptiveharmonicspower, 0, 200);
    Padaptiveharmonicspar = xml->getpar("adaptive_harmonics_par", Padaptiveharmonicspar, 0, 100);

    if (xml->enterbranch("HARMONICS"))
    {
        Phmag[0] = 64;
        Phphase[0] = 64;
        for (int n = 0; n < MAX_AD_HARMONICS; ++n)
        {
            if (xml->enterbranch("HARMONIC",n + 1) == 0)
                continue;
            Phmag[n] = xml->getpar127("mag", 64);
            Phphase[n] = xml->getpar127("phase", 64);

            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if (xml->enterbranch("BASE_FUNCTION"))
    {
        for (int i = 1; i < synth->halfoscilsize; ++i)
        {
            if (xml->enterbranch("BF_HARMONIC", i))
            {
                basefuncFFTfreqs.c[i] = xml->getparreal("cos", 0.0);
                basefuncFFTfreqs.s[i] = xml->getparreal("sin", 0.0);

                xml->exitbranch();
            }
        }
        xml->exitbranch();

        float max = 0.0;

        basefuncFFTfreqs.c[0] = 0.0;
        for (int i = 0; i < synth->halfoscilsize; ++i)
        {
            if (max < fabsf(basefuncFFTfreqs.c[i]))
                max = fabsf(basefuncFFTfreqs.c[i]);
            if (max < fabsf(basefuncFFTfreqs.s[i]))
                max = fabsf(basefuncFFTfreqs.s[i]);
        }
        if (max < 0.00000001)
            max = 1.0;

        for (int i = 0; i < synth->halfoscilsize; ++i)
        {
            if (basefuncFFTfreqs.c[i])
                basefuncFFTfreqs.c[i] /= max;
            if (basefuncFFTfreqs.s[i])
                basefuncFFTfreqs.s[i] /= max;
        }
    }

    presetsUpdated();
}

float OscilParameters::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int insert = getData->data.insert;

    unsigned char type = 0;

    // oscillator defaults
    int min = 0;
    int max = 127;
    float def = 0;
    type |= TOPLEVEL::type::Integer;
    unsigned char learnable = TOPLEVEL::type::Learnable;
    type |= learnable;

    if (insert == TOPLEVEL::insert::harmonicAmplitude || insert == TOPLEVEL::insert::harmonicPhaseBandwidth)
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
