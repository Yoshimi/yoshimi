/*
    LFOParams.h - Parameters for LFO

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

    This file is derivative of original ZynAddSubFX code.

*/

#ifndef LFO_PARAMS_H
#define LFO_PARAMS_H

#include <sys/types.h>

#include "Misc/XMLwrapper.h"
#include "Params/Presets.h"

class SynthEngine;

class LFOParams : public Presets
{
    public:
        LFOParams(float Pfreq_, unsigned char Pintensity_, unsigned char Pstartphase_,
                  unsigned char PLFOtype_, unsigned char Prandomness_,
                  unsigned char Pdelay_, unsigned char Pcontinous, int fel_, SynthEngine *_synth);
        ~LFOParams() { }

        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void setPfreq(int32_t n);
        void getfromXML(XMLwrapper *xml);
        void setPintensity(unsigned char n) { Pintensity = n; presetsUpdated(); }
        void setPstartphase(unsigned char n) { Pstartphase = n; }
        void setPLFOtype(unsigned char n) { PLFOtype = n; presetsUpdated(); }
        void setPrandomness(unsigned char n) { Prandomness = n; presetsUpdated(); }
        void setPfreqrand(unsigned char n) { Pfreqrand = n; presetsUpdated(); }
        void setPdelay(unsigned char n) { Pdelay = n; }
        void setPbpm(unsigned char n) { Pbpm = n; }
        void setPcontinous(unsigned char n) { Pcontinous = n; presetsUpdated(); }
        void setPstretch(unsigned char n) { Pstretch = n; presetsUpdated(); }

        // MIDI Parameters
        int32_t PfreqI;
        float Pfreq;
        unsigned char Pintensity;
        unsigned char Pstartphase;
        unsigned char PLFOtype;
        unsigned char Prandomness;
        unsigned char Pfreqrand;
        unsigned char Pdelay;
        unsigned char Pbpm;
        unsigned char Pcontinous;
        unsigned char Pstretch;

        int fel;         // kind of LFO - 0 frequency, 1 amplitude, 2 filter
       // static int time; // used by Pcontinous - moved to SynthEngine to make it per-instance

    private:
        // Default parameters
        unsigned char Dfreq;
        unsigned char Dintensity;
        unsigned char Dstartphase;
        unsigned char DLFOtype;
        unsigned char Drandomness;
        unsigned char Ddelay;
        unsigned char Dcontinous;
};

class LFOlimit
{
    public:
        float getLFOlimits(CommandBlock *getData);
};

#endif
