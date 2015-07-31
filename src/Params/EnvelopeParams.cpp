/*
    EnvelopeParams.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified January 2011
*/

#include <cmath>
#include <stdlib.h>

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
    Envmode(1)
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
    float result = (powf(2.0f, Penvdt[(int)i] / 127.0f * 12.0f) - 1.0f) * 10.0f; // miliseconds
    return result;
}

// ADSR/ASR... initialisations
void EnvelopeParams::ADSRinit(char A_dt, char D_dt, char S_val, char R_dt)
{
    setpresettype("Penvamplitude");
    Envmode = 1;
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
    Envmode = 2;
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
    Envmode = 3;
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
    Envmode = 4;
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
    Envmode = 5;
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
    switch (Envmode) {
    case 1:
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
    case 2:
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
    case 3:
        Penvpoints = 3;
        Penvsustain = 1;
        Penvval[0] = PA_val;
        Penvdt[1] = PA_dt;
        Penvval[1] = 64;
        Penvdt[2] = PR_dt;
        Penvval[2] = PR_val;
        break;
    case 4:
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
    case 5:
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

    if ((Pfreemode!=0)||(!xml->minimal)) {
        for (int i=0;i<Penvpoints;i++) {
            xml->beginbranch("POINT",i);
            if (i!=0) xml->addpar("dt",Penvdt[i]);
            xml->addpar("val",Penvval[i]);
            xml->endbranch();
        }
    }
    add2XMLMidi(xml);
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

    for (int i=0;i<Penvpoints;i++) {
        if (xml->enterbranch("POINT",i)==0) continue;
        if (i!=0) Penvdt[i]=xml->getpar127("dt",Penvdt[i]);
        Penvval[i]=xml->getpar127("val",Penvval[i]);
        xml->exitbranch();
    }
    getfromXMLMidi(xml, synth);
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

void EnvelopeParams::changepar(int npar, double value){
    switch(npar){
        case c_Pfreemode:
            Pfreemode = value >= 1 ? 1:0;
            break;
        case c_Penvpoints:
            Penvpoints = MAX_ENVELOPE_POINTS < Penvpoints ? MAX_ENVELOPE_POINTS: value;
            break;
        case c_Penvsustain:
            Penvsustain = value;
            break;
        case c_PA_dt:
            PA_dt = value;
            break;
        case c_PD_dt:
            PD_dt = value;
            break;
        case c_PR_dt:
            PR_dt = value;
            break;
        case c_PA_val:
            PA_val = value;
            break;
        case c_PD_val:
            PD_val = value;
            break;
        case c_PS_val:
            PS_val = value;
            break;
        case c_PR_val:
            PR_val = value;
            break;
        case c_Penvstretch:
            Penvstretch = value;
            break;
        case c_Pforcedrelease:
            Pforcedrelease = value;
            break;
        case c_Plinearenvelope:
            Plinearenvelope = value;
            break;
    }
    return;
}

float EnvelopeParams::getparFloat(int npar){
    switch(npar){
        case c_Pfreemode:
            return Pfreemode;
            break;
        case c_Penvpoints:
            return Penvpoints;
            break;
        case c_Penvsustain:
            return Penvsustain;
            break;
        case c_PA_dt:
            return PA_dt;
            break;
        case c_PD_dt:
            return PD_dt;
            break;
        case c_PR_dt:
            return PR_dt;
            break;
        case c_PA_val:
            return PA_val;
            break;
        case c_PD_val:
            return PD_val;
            break;
        case c_PS_val:
            return PS_val;
            break;
        case c_PR_val:
            return PR_val;
            break;
        case c_Penvstretch:
            return Penvstretch;
            break;
        case c_Pforcedrelease:
            return Pforcedrelease;
            break;
        case c_Plinearenvelope:
            return Plinearenvelope;
            break;
    }
    return -1;
}