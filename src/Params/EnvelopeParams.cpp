/*
    EnvelopeParams.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2019, Will Godfrey

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

    Modified May 2019
*/

#include <cmath>
#include <stdlib.h>
#include <iostream>

#include "Misc/XMLwrapper.h"
#include "Params/EnvelopeParams.h"

EnvelopeParams::EnvelopeParams(unsigned char Penvstretch_,
                               unsigned char Pforcedrelease_, SynthEngine *_synth) :
    Presets(_synth),
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
    Penvdt[0] = 0; // no used
    store2defaults();
}


float EnvelopeParams::getdt(char i)
{
    float result = (powf(2.0f, Penvdt[(int)i] / 127.0f * 12.0f) - 1.0f) * 10.0f; // milliseconds
    return result;
}


// ADSR/ASR... initialisations
void EnvelopeParams::ADSRinit(char A_dt, char D_dt, char S_val, char R_dt)
{
    setpresettype("Penvamplitude");
    Envmode = PART::envelope::groupmode::amplitudeLin;
    PA_dt = A_dt;
    PD_dt = D_dt;
    PS_val = S_val;
    PR_dt = R_dt;
    Pfreemode = 0;
    converttofree();
    store2defaults();
}


void EnvelopeParams::ADSRinit_dB(char A_dt, char D_dt, char S_val, char R_dt)
{
    setpresettype("Penvamplitude");
    Envmode = PART::envelope::groupmode::amplitudeLog;
    PA_dt = A_dt;
    PD_dt = D_dt;
    PS_val = S_val;
    PR_dt = R_dt;
    Pfreemode = 0;
    converttofree();
    store2defaults();
}


void EnvelopeParams::ASRinit(char A_val, char A_dt, char R_val, char R_dt)
{
    setpresettype("Penvfrequency");
    Envmode = PART::envelope::groupmode::frequency;
    PA_val = A_val;
    PA_dt = A_dt;
    PR_val = R_val;
    PR_dt = R_dt;
    Pfreemode = 0;
    converttofree();
    store2defaults();
}


void EnvelopeParams::ADSRinit_filter(char A_val, char A_dt, char D_val, char D_dt, char R_dt, char R_val)
{
    setpresettype("Penvfilter");
    Envmode = PART::envelope::groupmode::filter;
    PA_val = A_val;
    PA_dt = A_dt;
    PD_val = D_val;
    PD_dt = D_dt;
    PR_dt = R_dt;
    PR_val = R_val;
    Pfreemode = 0;
    converttofree();
    store2defaults();
}


void EnvelopeParams::ASRinit_bw(char A_val, char A_dt, char R_val, char R_dt)
{
    setpresettype("Penvbandwidth");
    Envmode = PART::envelope::groupmode::bandwidth;
    PA_val = A_val;
    PA_dt = A_dt;
    PR_val = R_val;
    PR_dt = R_dt;
    Pfreemode = 0;
    converttofree();
    store2defaults();
}


// Convert the Envelope to freemode
void EnvelopeParams::converttofree(void)
{
    switch (Envmode)
    {
        case PART::envelope::groupmode::amplitudeLin:
        Penvpoints = 4;
        Penvsustain = 2;
        Penvval[0] = 0;
        Penvdt[1] = PA_dt;
        Penvval[1] = 127;
        Penvdt[2] = PD_dt;
        Penvval[2] = PS_val;
        Penvdt[3] = PR_dt;
        Penvval[3] = 0;
        break;

    case PART::envelope::groupmode::amplitudeLog:
        Penvpoints = 4;
        Penvsustain = 2;
        Penvval[0] = 0;
        Penvdt[1] = PA_dt;
        Penvval[1] = 127;
        Penvdt[2] = PD_dt;
        Penvval[2] = PS_val;
        Penvdt[3] = PR_dt;
        Penvval[3] = 0;
        break;

    case PART::envelope::groupmode::frequency:
        Penvpoints = 3;
        Penvsustain = 1;
        Penvval[0] = PA_val;
        Penvdt[1] = PA_dt;
        Penvval[1] = 64;
        Penvdt[2] = PR_dt;
        Penvval[2] = PR_val;
        break;

    case PART::envelope::groupmode::filter:
        Penvpoints = 4;
        Penvsustain = 2;
        Penvval[0] = PA_val;
        Penvdt[1] = PA_dt;
        Penvval[1] = PD_val;
        Penvdt[2] = PD_dt;
        Penvval[2] = 64;
        Penvdt[3] = PR_dt;
        Penvval[3] = PR_val;
        break;

    case PART::envelope::groupmode::bandwidth:
        Penvpoints = 3;
        Penvsustain = 1;
        Penvval[0] = PA_val;
        Penvdt[1] = PA_dt;
        Penvval[1] = 64;
        Penvdt[2] = PR_dt;
        Penvval[2] = PR_val;
        break;
    }
}


void EnvelopeParams::add2XML(XMLwrapper *xml)
{
    xml->addparbool("free_mode",Pfreemode);
    xml->addpar("env_points",Penvpoints);
    xml->addpar("env_sustain",Penvsustain);
    xml->addpar("env_stretch",Penvstretch);
    xml->addparbool("forced_release",Pforcedrelease);
    xml->addparbool("linear_envelope",Plinearenvelope);
    xml->addpar("A_dt",PA_dt);
    xml->addpar("D_dt",PD_dt);
    xml->addpar("R_dt",PR_dt);
    xml->addpar("A_val",PA_val);
    xml->addpar("D_val",PD_val);
    xml->addpar("S_val",PS_val);
    xml->addpar("R_val",PR_val);

    if ((Pfreemode!=0)||(!xml->minimal))
    {
        for (int i=0;i<Penvpoints;i++)
        {
            xml->beginbranch("POINT",i);
            if (i!=0) xml->addpar("dt",Penvdt[i]);
            xml->addpar("val",Penvval[i]);
            xml->endbranch();
        }
    }
}


void EnvelopeParams::getfromXML(XMLwrapper *xml)
{
    Pfreemode=xml->getparbool("free_mode",Pfreemode);
    Penvpoints=xml->getpar127("env_points",Penvpoints);
    Penvsustain=xml->getpar127("env_sustain",Penvsustain);
    Penvstretch=xml->getpar127("env_stretch",Penvstretch);
    Pforcedrelease=xml->getparbool("forced_release",Pforcedrelease);
    Plinearenvelope=xml->getparbool("linear_envelope",Plinearenvelope);

    PA_dt=xml->getpar127("A_dt",PA_dt);
    PD_dt=xml->getpar127("D_dt",PD_dt);
    PR_dt=xml->getpar127("R_dt",PR_dt);
    PA_val=xml->getpar127("A_val",PA_val);
    PD_val=xml->getpar127("D_val",PD_val);
    PS_val=xml->getpar127("S_val",PS_val);
    PR_val=xml->getpar127("R_val",PR_val);

    for (int i=0;i<Penvpoints;i++)
    {
        if (xml->enterbranch("POINT",i)==0) continue;
        if (i!=0) Penvdt[i]=xml->getpar127("dt",Penvdt[i]);
        Penvval[i]=xml->getpar127("val",Penvval[i]);
        xml->exitbranch();
    }

    if (!Pfreemode)
        converttofree();
}


void EnvelopeParams::defaults(void)
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


void EnvelopeParams::store2defaults(void)
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
    if (engine >= PART::engine::addMod1 && engine <= PART::engine::addMod8)
        engine = PART::engine::addMod1;
    else if (engine >= PART::engine::addVoice1 && engine <= PART::engine::addVoice8)
        engine = PART::engine::addVoice1;
    int parameter = getData->data.parameter;

    unsigned char type = 0;

    // envelope defaults
    int min = 0;
    int max = 127;
    float def = 64;
    type |= TOPLEVEL::type::Integer;
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
                        def = 80;
                    else
                        def = 0;
                    break;
                case ENVELOPEINSERT::control::decayTime:
                    if (engine == PART::engine::addVoice1)
                        def = 100;
                    else if (engine == PART::engine::addMod1)
                        def = 90;
                    else
                        def = 40;
                    break;
                case ENVELOPEINSERT::control::sustainLevel:
                    def = 127;
                    break;
                case ENVELOPEINSERT::control::releaseTime:
                    if (engine == PART::engine::addVoice1 || engine == PART::engine::addMod1)
                        def = 100;
                    else
                        def = 25;
                    break;
                case ENVELOPEINSERT::control::stretch:
                    break;
                case ENVELOPEINSERT::control::forcedRelease:
                    def = 1;
                    type &= ~learnable;
                    break;
                case ENVELOPEINSERT::control::linearEnvelope:
                    max = 1;
                    def = 0;
                    type &= ~learnable;
                    break;
                case ENVELOPEINSERT::control::edit:
                    break;
                case ENVELOPEINSERT::control::enableFreeMode:
                    break;
                case ENVELOPEINSERT::control::points:
                    break;
                case ENVELOPEINSERT::control::sustainPoint:
                    def = 2;
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
                        def = 20;
                    else if (engine == PART::engine::addVoice1 || engine == PART::engine::subSynth)
                        def = 30;
                    break;
                case ENVELOPEINSERT::control::attackTime:
                    if (engine == PART::engine::addMod1)
                        def = 90;
                    else if (engine == PART::engine::addVoice1)
                        def = 40;
                    else
                        def = 50;
                    break;
                case ENVELOPEINSERT::control::releaseTime:
                    if (engine == PART::engine::addMod1)
                        def = 80;
                    else
                        def = 60;
                    break;
                case ENVELOPEINSERT::control::releaseLevel:
                    if (engine == PART::engine::addMod1)
                        def = 40;
                    break;
                case ENVELOPEINSERT::control::stretch:
                    if (engine != PART::engine::subSynth)
                        def = 0;
                    break;
                case ENVELOPEINSERT::control::forcedRelease:
                    max = 1;
                    def = 0;
                    type &= ~learnable;
                    break;
                case ENVELOPEINSERT::control::edit:
                    break;
                case ENVELOPEINSERT::control::enableFreeMode:
                    break;
                case ENVELOPEINSERT::control::points:
                    break;
                case ENVELOPEINSERT::control::sustainPoint:
                    def = 1;
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
                        def = 90;
                    break;
                case ENVELOPEINSERT::control::attackTime:
                    if (engine == PART::engine::addVoice1)
                        def = 70;
                    else
                        def = 40;
                    break;
                case ENVELOPEINSERT::control::decayLevel:
                    if (engine == PART::engine::addVoice1)
                        def = 40;
                    break;
                case ENVELOPEINSERT::control::decayTime:
                    def = 70;
                    break;
                case ENVELOPEINSERT::control::releaseTime:
                    if (engine == PART::engine::addVoice1)
                        def = 10;
                    else
                        def = 60;
                    break;
                case ENVELOPEINSERT::control::releaseLevel:
                    if (engine == PART::engine::addVoice1)
                        def = 40;
                    break;
                case ENVELOPEINSERT::control::stretch:
                    def = 0;
                    break;
                case ENVELOPEINSERT::control::forcedRelease:
                    max = 1;
                    if (engine == PART::engine::addVoice1)
                        def = 0;
                    else
                        def = 1;
                    type &= ~learnable;
                    break;
                case ENVELOPEINSERT::control::edit:
                    break;
                case ENVELOPEINSERT::control::enableFreeMode:
                    break;
                case ENVELOPEINSERT::control::points:
                    break;
                case ENVELOPEINSERT::control::sustainPoint:
                    def = 2;
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
                    def = 100;
                    break;
                case ENVELOPEINSERT::control::attackTime:
                        def = 70;
                    break;
                case ENVELOPEINSERT::control::releaseTime:
                    def = 60;
                    break;
                case ENVELOPEINSERT::control::releaseLevel:
                    break;
                case ENVELOPEINSERT::control::stretch:
                    break;
                case ENVELOPEINSERT::control::forcedRelease:
                    max = 1;
                    def = 0;
                    type &= ~learnable;
                    break;
                case ENVELOPEINSERT::control::edit:
                    break;
                case ENVELOPEINSERT::control::enableFreeMode:
                    break;
                case ENVELOPEINSERT::control::points:
                    break;
                case ENVELOPEINSERT::control::sustainPoint:
                    def = 1;
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


