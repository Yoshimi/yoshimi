/*
    LFOParams.cpp - Parameters for LFO

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019, Will Godfrey
    Copyright 2020 Kristian Amlie

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

#include <sys/types.h>
#include <cmath>

#include "Params/LFOParams.h"
#include "Misc/NumericFuncs.h"

//#include <iostream>
//int LFOParams::time = 0;

LFOParams::LFOParams(float Pfreq_, unsigned char Pintensity_,
                     unsigned char Pstartphase_, unsigned char PLFOtype_,
                     unsigned char Prandomness_, unsigned char Pdelay_,
                     unsigned char Pcontinous_, int fel_, SynthEngine *_synth) :
    Presets(_synth),
    fel(fel_),
    Dfreq(Pfreq_),
    Dintensity(Pintensity_),
    Dstartphase(Pstartphase_),
    DLFOtype(PLFOtype_),
    Drandomness(Prandomness_),
    Ddelay(Pdelay_),
    Dcontinous(Pcontinous_)
{
    switch (fel)
    {
        case 0:
            setpresettype("Plfofrequency");
            break;
        case 1:
            setpresettype("Plfoamplitude");
            break;
        case 2:
            setpresettype("Plfofilter");
            break;
    };
    defaults();
    presetsUpdated();
}


void LFOParams::defaults(void)
{
    setPfreq(Dfreq << Cshift2I);
    Pintensity = Dintensity;
    Pstartphase = Dstartphase;
    PLFOtype = DLFOtype;
    Prandomness = Drandomness;
    Pdelay = Ddelay;
    Pcontinous = Dcontinous;
    Pbpm = 0;
    Pfreqrand = 0;
    Pstretch = 64;
}


void LFOParams::setPfreq(int32_t n)
{

    PfreqI = n;
    Pfreq = (powf(2.0f, (float(n) / float(Fmul2I)) * 10.0f) - 1.0f) / 12.0f;
    presetsUpdated();
}


void LFOParams::add2XML(XMLwrapper *xml)
{
    float freqF = float(PfreqI) / Fmul2I;
    if (Pbpm)
        // Save quantized, so that we can make the scale finer in the future, if
        // necessary.
        freqF = func::quantizedLFOfreqBPM(freqF);
    xml->addpar("freqI", freqF * Fmul2I);
    xml->addparreal("freq", freqF);
    xml->addpar("intensity", Pintensity);
    xml->addpar("start_phase", Pstartphase);
    xml->addpar("lfo_type", PLFOtype);
    xml->addpar("randomness_amplitude", Prandomness);
    xml->addpar("randomness_frequency", Pfreqrand);
    xml->addpar("delay", Pdelay);
    xml->addpar("stretch", Pstretch);
    xml->addparbool("continous",    Pcontinous);
    xml->addparbool("bpm", Pbpm);
}


void LFOParams::getfromXML(XMLwrapper *xml)
{
    //PfreqI = xml->getpar("freqI", -1, 0, Fmul2I);
    //if (PfreqI == -1)
    PfreqI = xml->getparreal("freq", Pfreq, 0.0, 1.0) * Fmul2I;
    setPfreq(PfreqI);

    Pintensity = xml->getpar127("intensity", Pintensity);
    Pstartphase = xml->getpar127("start_phase", Pstartphase);
    PLFOtype = xml->getpar127("lfo_type", PLFOtype);
    Prandomness = xml->getpar127("randomness_amplitude", Prandomness);
    Pfreqrand = xml->getpar127("randomness_frequency", Pfreqrand);
    Pdelay = xml->getpar127("delay", Pdelay);
    Pstretch = xml->getpar127("stretch", Pstretch);
    Pcontinous = xml->getparbool("continous", Pcontinous);
    Pbpm = xml->getparbool("bpm", Pbpm);
    presetsUpdated();
}

float LFOlimit::getLFOlimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int engine = getData->data.engine;
    int insertType = getData->data.parameter;

    unsigned char type =0;

    // LFO defaults
    int min = 0;
    int max = 127;
    float def = 0;
    type |= TOPLEVEL::type::Integer;
    unsigned char learnable = TOPLEVEL::type::Learnable;
    type |= learnable;

    switch (control)
    {
        case LFOINSERT::control::speed:
            max = 1.0f;
            type &= ~TOPLEVEL::type::Integer;
            switch(insertType)
            {
                case TOPLEVEL::insertType::amplitude:
                    if (engine >= PART::engine::addVoice1)
                        def = 0.708000;
                    else
                        def = 0.62999f;
                    break;
                case TOPLEVEL::insertType::frequency:
                    if (engine >= PART::engine::addVoice1)
                        def = 0.393000f;
                    else
                        def = 0.550999f;
                    break;
                case TOPLEVEL::insertType::filter:
                    if (engine >= PART::engine::addVoice1)
                        def = 0.393000f;
                    else
                        def = 0.62999f;
                    break;
            }
            break;
        case LFOINSERT::control::depth:
            if (engine >= PART::engine::addVoice1)
            {
                switch(insertType)
                {
                    case TOPLEVEL::insertType::amplitude:
                        def = 32;
                        break;
                    case TOPLEVEL::insertType::frequency:
                        def = 40;
                        break;
                    case TOPLEVEL::insertType::filter:
                        def = 20;
                        break;
                }
            }
            break;
        case LFOINSERT::control::delay:
            if (engine >= PART::engine::addVoice1 && insertType == TOPLEVEL::insertType::amplitude)
                def = 30;
            break;
        case LFOINSERT::control::start:
            if (engine < PART::engine::addVoice1 || insertType != TOPLEVEL::insertType::frequency)
                def = 64;
            break;
        case LFOINSERT::control::amplitudeRandomness:
            break;
        case LFOINSERT::control::type:
            max = 9;
            type &= ~learnable;
            break;
        case LFOINSERT::control::continuous:
            max = 1;
            type &= ~learnable;
            break;
        case LFOINSERT::control::bpm:
            max = 1;
            type &= ~learnable;
            break;
        case LFOINSERT::control::frequencyRandomness:
            break;
        case LFOINSERT::control::stretch:
            def = 64;
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
