/*
    LFOParams.cpp - Parameters for LFO

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

    This file is derivative of ZynAddSubFX original code, modified February 2017
*/

#include <cmath>

#include "Params/LFOParams.h"

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
    updated = true;
}


void LFOParams::defaults(void)
{
    Pfreq = Dfreq / 127.0f;
    Pintensity = Dintensity;
    Pstartphase = Dstartphase;
    PLFOtype = DLFOtype;
    Prandomness = Drandomness;
    Pdelay = Ddelay;
    Pcontinous = Dcontinous;
    Pfreqrand = 0;
    Pstretch = 64;
}


void LFOParams::add2XML(XMLwrapper *xml)
{
    xml->addparreal("freq", Pfreq);
    xml->addpar("intensity", Pintensity);
    xml->addpar("start_phase", Pstartphase);
    xml->addpar("lfo_type", PLFOtype);
    xml->addpar("randomness_amplitude", Prandomness);
    xml->addpar("randomness_frequency", Pfreqrand);
    xml->addpar("delay", Pdelay);
    xml->addpar("stretch", Pstretch);
    xml->addparbool("continous",    Pcontinous);
}


void LFOParams::getfromXML(XMLwrapper *xml)
{
    Pfreq = xml->getparreal("freq", Pfreq, 0.0, 1.0);
    Pintensity = xml->getpar127("intensity", Pintensity);
    Pstartphase = xml->getpar127("start_phase", Pstartphase);
    PLFOtype = xml->getpar127("lfo_type", PLFOtype);
    Prandomness = xml->getpar127("randomness_amplitude", Prandomness);
    Pfreqrand = xml->getpar127("randomness_frequency", Pfreqrand);
    Pdelay = xml->getpar127("delay", Pdelay);
    Pstretch = xml->getpar127("stretch", Pstretch);
    Pcontinous = xml->getparbool("continous", Pcontinous);
    updated = true;
}
