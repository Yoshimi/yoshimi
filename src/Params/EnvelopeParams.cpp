/*
    EnvelopeParams.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2019-2023, Will Godfrey

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

#include <cmath>
#include <stdlib.h>
#include <iostream>

#include "Misc/XMLwrapper.h"
#include "Misc/NumericFuncs.h"
#include "Params/EnvelopeParams.h"

using func::power;


EnvelopeParams::EnvelopeParams(uchar Penvstretch_, uchar Pforcedrelease_, SynthEngine& _synth) :
    ParamBase(_synth),
    Pfreemode(1),
    Penvpoints(1),
    Penvsustain(1),
    Penvstretch(Penvstretch_),
    Pforcedrelease(Pforcedrelease_),
    Plinearenvelope(0),
    Envmode(PART::envelope::groupmode::amplitudeLin)
{
    int i;

    PA_dt = 10;
    PD_dt = 10;
    PR_dt = 10;
    PA_val = 64;
    PD_val = 64;
    PS_val = 64;
    PR_val = 64;

    for (i = 0; i < MAX_ENVELOPE_POINTS; ++i)
    {
        Penvdt[i] = 32;
        Penvval[i] = 64;
    }
    Penvdt[0] = 0; // not used
    store2defaults();
}


float EnvelopeParams::getdt(size_t i)
{
    float result = (power<2>(Penvdt[i] / 127.0f * 12.0f) - 1.0f) * 10.0f; // milliseconds
    return result;
}


// ADSR/ASR... initialisations
void EnvelopeParams::ADSRinit(float A_dt, float D_dt, float S_val, float R_dt)
{
    Envmode = PART::envelope::groupmode::amplitudeLin;
    PA_dt = A_dt;
    PD_dt = D_dt;
    PS_val = S_val;
    PR_dt = R_dt;
    Pfreemode = 0;
    converttofree();
    store2defaults();
}


void EnvelopeParams::ADSRinit_dB(float A_dt, float D_dt, float S_val, float R_dt)
{
    Envmode = PART::envelope::groupmode::amplitudeLog;
    PA_dt = A_dt;
    PD_dt = D_dt;
    PS_val = S_val;
    PR_dt = R_dt;
    Pfreemode = 0;
    converttofree();
    store2defaults();
}


void EnvelopeParams::ASRinit(float A_val, float A_dt, float R_val, float R_dt)
{
    Envmode = PART::envelope::groupmode::frequency;
    PA_val = A_val;
    PA_dt = A_dt;
    PR_val = R_val;
    PR_dt = R_dt;
    Pfreemode = ENVSWITCH::defFreeMode;
    converttofree();
    store2defaults();
}


void EnvelopeParams::ADSRinit_filter(float A_val, float A_dt, float D_val, float D_dt, float R_dt, float R_val)
{
    Envmode = PART::envelope::groupmode::filter;
    PA_val = A_val;
    PA_dt = A_dt;
    PD_val = D_val;
    PD_dt = D_dt;
    PR_dt = R_dt;
    PR_val = R_val;
    Pfreemode = ENVSWITCH::defFreeMode;
    converttofree();
    store2defaults();
}


void EnvelopeParams::ASRinit_bw(float A_val, float A_dt, float R_val, float R_dt)
{
    Envmode = PART::envelope::groupmode::bandwidth;
    PA_val = A_val;
    PA_dt = A_dt;
    PR_val = R_val;
    PR_dt = R_dt;
    Pfreemode = ENVSWITCH::defFreeMode;
    converttofree();
    store2defaults();
}


// Convert the Envelope to freemode
void EnvelopeParams::converttofree()
{
    switch (Envmode)
    {
        case PART::envelope::groupmode::amplitudeLin:
        Penvpoints = ENVDEF::count.def;
        Penvsustain = ENVDEF::point.def;
        Penvval[0] = 0;
        Penvdt[1] = PA_dt;
        Penvval[1] = 127;
        Penvdt[2] = PD_dt;
        Penvval[2] = PS_val;
        Penvdt[3] = PR_dt;
        Penvval[3] = 0;
        break;

    case PART::envelope::groupmode::amplitudeLog:
        Penvpoints = ENVDEF::count.def;
        Penvsustain = ENVDEF::point.def;
        Penvval[0] = 0;
        Penvdt[1] = PA_dt;
        Penvval[1] = 127;
        Penvdt[2] = PD_dt;
        Penvval[2] = PS_val;
        Penvdt[3] = PR_dt;
        Penvval[3] = 0;
        break;

    case PART::envelope::groupmode::frequency:
        Penvpoints = ENVDEF::freqCount.def;
        Penvsustain = ENVDEF::freqPoint.def;
        Penvval[0] = PA_val;
        Penvdt[1] = PA_dt;
        Penvval[1] = 64;
        Penvdt[2] = PR_dt;
        Penvval[2] = PR_val;
        break;

    case PART::envelope::groupmode::filter:
        Penvpoints = ENVDEF::count.def;
        Penvsustain = ENVDEF::point.def;
        Penvval[0] = PA_val;
        Penvdt[1] = PA_dt;
        Penvval[1] = PD_val;
        Penvdt[2] = PD_dt;
        Penvval[2] = 64;
        Penvdt[3] = PR_dt;
        Penvval[3] = PR_val;
        break;

    case PART::envelope::groupmode::bandwidth:
        Penvpoints = ENVDEF::bandCount.def;
        Penvsustain = ENVDEF::bandPoint.def;
        Penvval[0] = PA_val;
        Penvdt[1] = PA_dt;
        Penvval[1] = 64;
        Penvdt[2] = PR_dt;
        Penvval[2] = PR_val;
        break;
    }
}


void EnvelopeParams::add2XML(XMLwrapper& xml)
{
    xml.addparbool("free_mode",Pfreemode);
    xml.addpar("env_points",Penvpoints);
    xml.addpar("env_sustain",Penvsustain);
    xml.addpar("env_stretch",Penvstretch);
    xml.addparbool("forced_release",Pforcedrelease);
    xml.addparbool("linear_envelope",Plinearenvelope);
    xml.addparcombi("A_dt",PA_dt);
    xml.addparcombi("D_dt",PD_dt);
    xml.addparcombi("R_dt",PR_dt);
    xml.addparcombi("A_val",PA_val);
    xml.addparcombi("D_val",PD_val);
    xml.addparcombi("S_val",PS_val);
    xml.addparcombi("R_val",PR_val);

    if ((Pfreemode!=0)||(!xml.minimal))
    {
        for (size_t i=0; i<Penvpoints; ++i)
        {
            xml.beginbranch("POINT",i);
            if (i > 0)
                xml.addparcombi("dt",Penvdt[i]);
            xml.addparcombi("val",Penvval[i]);
            xml.endbranch();
        }
    }
}


void EnvelopeParams::getfromXML(XMLwrapper& xml)
{
    Pfreemode=xml.getparbool("free_mode",Pfreemode);
    Penvpoints=xml.getpar127("env_points",Penvpoints);
    Penvsustain=xml.getpar127("env_sustain",Penvsustain);
    Penvstretch=xml.getpar127("env_stretch",Penvstretch);
    Pforcedrelease=xml.getparbool("forced_release",Pforcedrelease);
    Plinearenvelope=xml.getparbool("linear_envelope",Plinearenvelope);

    PA_dt=xml.getparcombi("A_dt",PA_dt,0,127);
    PD_dt=xml.getparcombi("D_dt",PD_dt,0,127);
    PR_dt=xml.getparcombi("R_dt",PR_dt,0,127);
    PA_val=xml.getparcombi("A_val",PA_val,0,127);
    PD_val=xml.getparcombi("D_val",PD_val,0,127);
    PS_val=xml.getparcombi("S_val",PS_val,0,127);
    PR_val=xml.getparcombi("R_val",PR_val,0,127);

    for (size_t i=0;i<Penvpoints;i++)
    {
        if (xml.enterbranch("POINT",i)==0) continue;
        if (i > 0)
            Penvdt[i]=xml.getparcombi("dt",Penvdt[i], 0,127);
        Penvval[i]=xml.getparcombi("val",Penvval[i], 0,127);
        xml.exitbranch();
    }

    if (!Pfreemode)
        converttofree();
}


void EnvelopeParams::defaults()
{
    Penvstretch = Denvstretch;
    Pforcedrelease = Dforcedrelease;
    Plinearenvelope = Dlinearenvelope;
    PA_dt = DA_dt;
    PD_dt = DD_dt;
    PR_dt = DR_dt;
    PA_val = DA_val;
    PD_val = DD_val;
    PS_val = DS_val;
    PR_val = DR_val;
    Pfreemode = 0;
    converttofree();
}


void EnvelopeParams::store2defaults()
{
    Denvstretch = Penvstretch;
    Dforcedrelease = Pforcedrelease;
    Dlinearenvelope = Plinearenvelope;
    DA_dt = PA_dt;
    DD_dt = PD_dt;
    DR_dt = PR_dt;
    DA_val = PA_val;
    DD_val = PD_val;
    DS_val = PS_val;
    DR_val = PR_val;
}

float envelopeLimit::getEnvelopeLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int engine = getData->data.engine;
    if (engine >= PART::engine::addMod1 && engine < PART::engine::addVoiceModEnd)
        engine = PART::engine::addMod1;
    else if (engine >= PART::engine::addVoice1 && engine < PART::engine::addMod1)
        engine = PART::engine::addVoice1;
    int parameter = getData->data.parameter;

    unsigned char type = 0;

    // envelope defaults
    int min = 0;
    int max = 127;
    float def = 64;
    unsigned char learnable = TOPLEVEL::type::Learnable;
    type |= learnable;

    if (control == ENVELOPEINSERT::control::enableFreeMode || control == ENVELOPEINSERT::control::edit)
    {
        max = 1;
        type &= ~learnable;
        def = 0;
    }

    switch (parameter)
    {
        case TOPLEVEL::insertType::amplitude:
        {
            switch (control)
            {
                case ENVELOPEINSERT::control::attackTime:
                    if (engine == PART::engine::addMod1)
                        def = ENVDEF::modAmpAttackTime.def;
                    else
                        def = ENVDEF::ampAttackTime.def;
                    break;
                case ENVELOPEINSERT::control::decayTime:
                    if (engine == PART::engine::addVoice1)
                        def = ENVDEF::voiceAmpDecayTime.def;
                    else if (engine == PART::engine::addMod1)
                        def = ENVDEF::modAmpDecayTime.def;
                    else
                        def = ENVDEF::ampDecayTime.def;
                    break;
                case ENVELOPEINSERT::control::sustainLevel:
                    def = ENVDEF::ampSustainValue.def;
                    break;
                case ENVELOPEINSERT::control::releaseTime:
                    if (engine == PART::engine::addVoice1)
                        def = ENVDEF::voiceAmpReleaseTime.def;
                    else if (engine == PART::engine::addMod1)
                        def = ENVDEF::modAmpReleaseTime.def;
                    else
                        def = ENVDEF::ampReleaseTime.def;
                    break;
                case ENVELOPEINSERT::control::stretch:
                    def = ENVDEF::ampStretch.def;
                    break;
                case ENVELOPEINSERT::control::forcedRelease:
                    type |= TOPLEVEL::type::Integer;
                    def = ENVSWITCH::defForce;
                    type &= ~learnable;
                    break;
                case ENVELOPEINSERT::control::linearEnvelope:
                    type |= TOPLEVEL::type::Integer;
                    max = 1;
                    def = ENVSWITCH::defLinear;
                    type &= ~learnable;
                    break;
                //case ENVELOPEINSERT::control::edit:
                    //break;
                case ENVELOPEINSERT::control::enableFreeMode:
                    type |= TOPLEVEL::type::Integer;
                    def = ENVSWITCH::defFreeMode;
                    break;
                case ENVELOPEINSERT::control::points:
                    type |= TOPLEVEL::type::Integer;
                    def = ENVDEF::count.def;
                    break;
                case ENVELOPEINSERT::control::sustainPoint:
                    type &= ~learnable;
                    def = ENVDEF::point.def;
                    break;
                default:
                    type |= TOPLEVEL::type::Error;
                    break;
            }
            break;
        }

        case TOPLEVEL::insertType::frequency:
        {
            switch (control)
            {
                case ENVELOPEINSERT::control::attackLevel:
                    if (engine == PART::engine::addMod1)
                        def = ENVDEF::modFreqAtValue.def;
                    else if (engine == PART::engine::addVoice1)
                        def = ENVDEF::voiceFreqAtValue.def;
                    else if (engine == PART::engine::subSynth)
                        def = ENVDEF::subFreqAtValue.def;
                    else
                        def = ENVDEF::freqAttackValue.def;
                    break;
                case ENVELOPEINSERT::control::attackTime:
                    if (engine == PART::engine::addMod1)
                        def = ENVDEF::modFreqAtTime.def;
                    else if (engine == PART::engine::addVoice1)
                        def = ENVDEF::voiceFreqAtTime.def;
                    else
                        def = ENVDEF::freqAttackTime.def;
                    break;
                case ENVELOPEINSERT::control::releaseTime:
                    if (engine == PART::engine::addMod1)
                        def = ENVDEF::modFreqReleaseTime.def;
                    else
                        def = ENVDEF::freqReleaseTime.def;
                    break;
                case ENVELOPEINSERT::control::releaseLevel:
                    if (engine == PART::engine::addMod1)
                        def = ENVDEF::modFreqReleaseValue.def;
                    else
                        def = ENVDEF::freqReleaseValue.def;
                    break;
                case ENVELOPEINSERT::control::stretch:
                    if (engine == PART::engine::subSynth)
                        def = ENVDEF::subFreqStretch.def;
                    else
                        def = ENVDEF::freqStretch.def;
                    break;
                case ENVELOPEINSERT::control::forcedRelease:
                    max = 1;
                    def = ENVSWITCH::defForceFreq;
                    type &= ~learnable;
                    break;
                //case ENVELOPEINSERT::control::edit:
                    //break;
                case ENVELOPEINSERT::control::enableFreeMode:
                    def = ENVSWITCH::defFreeMode;
                    break;
                case ENVELOPEINSERT::control::points:
                    def = ENVDEF::freqCount.def;
                    break;
                case ENVELOPEINSERT::control::sustainPoint:
                    type &= ~learnable;
                    def = ENVDEF::freqPoint.def;
                    break;
                default:
                    type |= TOPLEVEL::type::Error;
                    break;
            }
            break;
        }

        case TOPLEVEL::insertType::filter:
        {
            switch (control)
            {
                case ENVELOPEINSERT::control::attackLevel:
                    if (engine == PART::engine::addVoice1)
                        def = ENVDEF::voiceFiltAtValue.def;
                    else
                        def = ENVDEF::filtAttackValue.def;
                    break;
                case ENVELOPEINSERT::control::attackTime:
                    if (engine == PART::engine::addVoice1)
                        def = ENVDEF::voiceFiltAtTime.def;
                    else
                        def = ENVDEF::filtAttackTime.def;
                    break;
                case ENVELOPEINSERT::control::decayLevel:
                    if (engine == PART::engine::addVoice1)
                        def = ENVDEF::voiceFiltDeValue.def;
                    else
                        def = ENVDEF::filtDecayValue.def;
                    break;
                case ENVELOPEINSERT::control::decayTime:
                    def = ENVDEF::filtDecayTime.def;
                    break;
                case ENVELOPEINSERT::control::releaseTime:
                    if (engine == PART::engine::addVoice1)
                        def = ENVDEF::voiceFiltRelTime.def;
                    else
                        def = ENVDEF::filtReleaseTime.def;
                    break;
                case ENVELOPEINSERT::control::releaseLevel:
                    if (engine == PART::engine::addVoice1)
                        def = ENVDEF::voiceFiltRelValue.def;
                    else
                        def = ENVDEF::filtReleaseValue.def;
                    break;
                case ENVELOPEINSERT::control::stretch:
                    def = ENVDEF::filtStretch.def;
                    break;
                case ENVELOPEINSERT::control::forcedRelease:
                    max = 1;
                    if (engine == PART::engine::addVoice1)
                        def = ENVSWITCH::defForceVoiceFilt;
                    else
                        def = ENVSWITCH::defForce;
                    type &= ~learnable;
                    break;
                //case ENVELOPEINSERT::control::edit:
                    //break;
                case ENVELOPEINSERT::control::enableFreeMode:
                    def = ENVSWITCH::defFreeMode;
                    break;
                case ENVELOPEINSERT::control::points:
                    def = ENVDEF::count.def;
                    break;
                case ENVELOPEINSERT::control::sustainPoint:
                    type &= ~learnable;
                    def = ENVDEF::point.def;
                    break;
                default:
                    type |= TOPLEVEL::type::Error;
                    break;
            }
            break;
        }
        case TOPLEVEL::insertType::bandwidth:
        {
            if (engine != PART::engine::subSynth)
            {
                type |= TOPLEVEL::type::Error;
                return 1;
            }
            switch (control)
            {
                case ENVELOPEINSERT::control::attackLevel:
                    def = ENVDEF::subBandAttackValue.def;
                    break;
                case ENVELOPEINSERT::control::attackTime:
                        def = ENVDEF::subBandAttackTime.def;
                    break;
                case ENVELOPEINSERT::control::releaseTime:
                    def = ENVDEF::subBandReleaseTime.def;
                    break;
                case ENVELOPEINSERT::control::releaseLevel:
                    def = ENVDEF::subBandReleaseValue.def;
                    break;
                case ENVELOPEINSERT::control::stretch:
                    def = ENVDEF::subBandStretch.def;
                    break;
                case ENVELOPEINSERT::control::forcedRelease:
                    max = 1;
                    def = ENVSWITCH::defForceBand;
                    type &= ~learnable;
                    break;
                //case ENVELOPEINSERT::control::edit:
                    //break;
                case ENVELOPEINSERT::control::enableFreeMode:
                    def = ENVSWITCH::defFreeMode;
                    break;
                case ENVELOPEINSERT::control::points:
                    def = ENVDEF::bandCount.def;
                    break;
                case ENVELOPEINSERT::control::sustainPoint:
                    def = ENVDEF::bandPoint.def;
                    break;
                default:
                    type |= TOPLEVEL::type::Error;
                    break;
            }
            break;
        }
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


