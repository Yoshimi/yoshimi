/*
    LFOParams.cpp - Parameters for LFO

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019, Will Godfrey
    Copyright 2020 Kristian Amlie
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

    This file is derivative of ZynAddSubFX original code.

*/

#include <sys/types.h>
#include <cmath>

#include "Params/LFOParams.h"
#include "Misc/NumericFuncs.h"

using func::power;


LFOParams::LFOParams(float Pfreq_, float Pintensity_,
                     float Pstartphase_, unsigned char PLFOtype_,
                     float Prandomness_, float Pdelay_,
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
    Pbpm = LFOSWITCH::BPM;
    Pfreqrand = LFODEF::freqRnd.def;
    Pstretch = LFODEF::stretch.def;
}


void LFOParams::setPfreq(int32_t n)
{

    PfreqI = n;
    Pfreq = (power<2>((float(n) / float(Fmul2I)) * 10.0f) - 1.0f) / 12.0f;
    presetsUpdated();
}


void LFOParams::add2XML(XMLwrapper *xml)
{
    float freqF = float(PfreqI) / float(Fmul2I);
    if (Pbpm)
        // Save quantized, so that we can make the scale finer in the future, if
        // necessary.
        freqF = func::quantizedLFOfreqBPM(freqF);
    xml->addpar("freqI", freqF * float(Fmul2I));
    xml->addparreal("freq", freqF);
    xml->addparcombi("intensity", Pintensity);
    xml->addparcombi("start_phase", Pstartphase);
    xml->addpar("lfo_type", PLFOtype);
    xml->addparcombi("randomness_amplitude", Prandomness);
    xml->addparcombi("randomness_frequency", Pfreqrand);
    xml->addparcombi("delay", Pdelay);
    xml->addparcombi("stretch", Pstretch);
    xml->addparbool("continous",    Pcontinous);
    xml->addparbool("bpm", Pbpm);
}


void LFOParams::getfromXML(XMLwrapper *xml)
{
    //PfreqI = xml->getpar("freqI", -1, 0, Fmul2I);
    //if (PfreqI == -1)
    PfreqI = xml->getparreal("freq", Pfreq, 0.0, 1.0) * float(Fmul2I);
    setPfreq(PfreqI);

    Pintensity = xml->getparcombi("intensity", Pintensity,0,127);
    Pstartphase = xml->getparcombi("start_phase", Pstartphase,0,127);
    PLFOtype = xml->getpar127("lfo_type", PLFOtype);
    Prandomness = xml->getparcombi("randomness_amplitude", Prandomness,0,127);
    Pfreqrand = xml->getparcombi("randomness_frequency", Pfreqrand,0,127);
    Pdelay = xml->getparcombi("delay", Pdelay,0,127);
    Pstretch = xml->getparcombi("stretch", Pstretch,0,127);
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

    unsigned char type = 0;

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
                        def = LFODEF::voiceAmpFreq.def;
                    else
                        def = LFODEF::ampFreq.def;
                    break;
                case TOPLEVEL::insertType::frequency:
                    if (engine >= PART::engine::addVoice1)
                        def = LFODEF::voiceFreqFreq.def;
                    else
                        def = LFODEF::freqFreq.def;
                    break;
                case TOPLEVEL::insertType::filter:
                    if (engine >= PART::engine::addVoice1)
                        def = LFODEF::voiceFiltFreq.def;
                    else
                        def = LFODEF::filtFreq.def;
                    break;
            }
            break;
        case LFOINSERT::control::depth:
            if (engine >= PART::engine::addVoice1)
            {
                switch(insertType)
                {
                    case TOPLEVEL::insertType::amplitude:
                        def = LFODEF::voiceAmpDepth.def;
                        break;
                    case TOPLEVEL::insertType::frequency:
                        def = LFODEF::voiceFreqDepth.def;
                        break;
                    case TOPLEVEL::insertType::filter:
                        def = LFODEF::voiceFiltDepth.def;
                        break;
                    default:
                        def = LFODEF::depth.def; // is this ever used?
                        break;
                }
            }
            break;
        case LFOINSERT::control::delay:
            if (engine >= PART::engine::addVoice1 && insertType == TOPLEVEL::insertType::amplitude)
                def = LFODEF::voiceAmpDelay.def;
            else
                def = LFODEF::delay.def;
            break;
        case LFOINSERT::control::start:
            if (engine < PART::engine::addVoice1 || insertType != TOPLEVEL::insertType::frequency)
                def = LFODEF::start.def;
            break;
        case LFOINSERT::control::amplitudeRandomness:
            def = LFODEF::ampRnd.def;
            break;
        case LFOINSERT::control::type:
            max = LFODEF::type.max;
            def = LFODEF::type.def;
            type &= ~learnable;
            break;
        case LFOINSERT::control::continuous:
            max = true;
            def = LFOSWITCH::continuous;
            type &= ~learnable;
            break;
        case LFOINSERT::control::bpm:
            max = true;
            def = LFOSWITCH::BPM;
            type &= ~learnable;
            break;
        case LFOINSERT::control::frequencyRandomness:
            def = LFODEF::freqRnd.def;
            break;
        case LFOINSERT::control::stretch:
            def = LFODEF::stretch.def;
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
