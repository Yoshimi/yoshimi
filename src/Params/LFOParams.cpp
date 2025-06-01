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
#include <cassert>
#include <cmath>

#include "Params/LFOParams.h"
#include "Misc/NumericFuncs.h"
#include "Misc/XMLStore.h"

using func::power;


LFOParams::LFOParams(float Pfreq_, float Pintensity_,
                     float Pstartphase_, uchar PLFOtype_,
                     float Prandomness_, float Pdelay_,
                     bool  Pcontinous_, int fel_, SynthEngine& _synth) :
    ParamBase(_synth),
    fel(fel_),
    Dfreq(Pfreq_),
    Dintensity(Pintensity_),
    Dstartphase(Pstartphase_),
    DLFOtype(PLFOtype_),
    Drandomness(Prandomness_),
    Ddelay(Pdelay_),
    Dcontinous(Pcontinous_)
{
    defaults();
    paramsChanged();
}


void LFOParams::defaults()
{
    setPfreq(Dfreq << Cshift2I);
    Pintensity  = Dintensity;
    Pstartphase = Dstartphase;
    PLFOtype    = DLFOtype;
    Prandomness = Drandomness;
    Pdelay      = Ddelay;
    Pcontinous  = Dcontinous;
    Pbpm        = LFOSWITCH::BPM;
    Pfreqrand   = LFODEF::freqRnd.def;
    Pstretch    = LFODEF::stretch.def;
}


void LFOParams::setPfreq(int32_t n)
{
    PfreqI = n;
    Pfreq = (power<2>((float(n) / float(Fmul2I)) * 10.0f) - 1.0f) / 12.0f;
    paramsChanged();
}


void LFOParams::add2XML(XMLtree& xmlLFO)
{
    float freqF = float(PfreqI) / float(Fmul2I);
    if (Pbpm)// Save quantised, so that we can make the scale finer in the future, if necessary.
        freqF = func::quantizedLFOfreqBPM(freqF);

    xmlLFO.addPar_int ("freqI", freqF * float(Fmul2I));
    xmlLFO.addPar_real("freq",  freqF);
    xmlLFO.addPar_frac("intensity"           , Pintensity);
    xmlLFO.addPar_frac("start_phase"         , Pstartphase);
    xmlLFO.addPar_int ("lfo_type"            , PLFOtype);
    xmlLFO.addPar_frac("randomness_amplitude", Prandomness);
    xmlLFO.addPar_frac("randomness_frequency", Pfreqrand);
    xmlLFO.addPar_frac("delay"               , Pdelay);
    xmlLFO.addPar_frac("stretch"             , Pstretch);
    xmlLFO.addPar_bool("continous"           , Pcontinous);
    xmlLFO.addPar_bool("bpm"                 , Pbpm);
}


void LFOParams::getfromXML(XMLtree& xmlLFO)
{
    assert(xmlLFO);
    PfreqI = xmlLFO.getPar_real("freq", Pfreq, 0.0, 1.0) * float(Fmul2I);
    setPfreq(PfreqI);

    Pintensity  = xmlLFO.getPar_frac("intensity"           , Pintensity ,0,127);
    Pstartphase = xmlLFO.getPar_frac("start_phase"         , Pstartphase,0,127);
    PLFOtype    = xmlLFO.getPar_127 ("lfo_type"            , PLFOtype);
    Prandomness = xmlLFO.getPar_frac("randomness_amplitude", Prandomness,0,127);
    Pfreqrand   = xmlLFO.getPar_frac("randomness_frequency", Pfreqrand  ,0,127);
    Pdelay      = xmlLFO.getPar_frac("delay"               , Pdelay     ,0,127);
    Pstretch    = xmlLFO.getPar_frac("stretch"             , Pstretch   ,0,127);
    Pcontinous  = xmlLFO.getPar_bool("continous"           , Pcontinous);
    Pbpm        = xmlLFO.getPar_bool("bpm"                 , Pbpm);
    paramsChanged();
}

float LFOlimit::getLFOlimits(CommandBlock* getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int engine  = getData->data.engine;
    int insertType = getData->data.parameter;

    uchar type = 0;

    // LFO defaults
    int min = 0;
    int max = 127;
    float def = 0;
    type |= TOPLEVEL::type::Integer;
    uchar learnable = TOPLEVEL::type::Learnable;
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
        type &= ~TOPLEVEL::type::Integer;
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
            type &= ~TOPLEVEL::type::Integer;
            if (engine >= PART::engine::addVoice1 && insertType == TOPLEVEL::insertType::amplitude)
                def = LFODEF::voiceAmpDelay.def;
            else
                def = LFODEF::delay.def;
            break;
        case LFOINSERT::control::start:
            type &= ~TOPLEVEL::type::Integer;
            if (engine < PART::engine::addVoice1 || insertType != TOPLEVEL::insertType::frequency)
                def = LFODEF::start.def;
            break;
        case LFOINSERT::control::amplitudeRandomness:
            type &= ~TOPLEVEL::type::Integer;
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
            type &= ~TOPLEVEL::type::Integer;
            def = LFODEF::freqRnd.def;
            break;
        case LFOINSERT::control::stretch:
            type &= ~TOPLEVEL::type::Integer;
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
