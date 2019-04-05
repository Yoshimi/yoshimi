/*
    LFOParams.h - Parameters for LFO

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/

#ifndef LFO_PARAMS_H
#define LFO_PARAMS_H

#include "Misc/XMLwrapper.h"
#include "Params/Presets.h"

class LFOParams : public Presets
{
    public:
        LFOParams(char Pfreq_, char Pintensity_, char Pstartphase_, char PLFOtype_,
                  char Prandomness_, char Pdelay_, char Pcontinous, char fel_);
        ~LFOParams() { };

        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);

        // MIDI Parameters
        float Pfreq;
        unsigned char Pintensity;
        unsigned char Pstartphase;
        unsigned char PLFOtype;
        unsigned char Prandomness;
        unsigned char Pfreqrand;
        unsigned char Pdelay;
        unsigned char Pcontinous;
        unsigned char Pstretch;

        int fel;         // what kind is the LFO
                         // (0 - frequency, 1 - amplitude, 2 - filter)
        static int time; // is used by Pcontinous parameter

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

#endif
