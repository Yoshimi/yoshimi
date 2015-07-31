/*
    LFOParams.h - Parameters for LFO

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

    This file is derivative of original ZynAddSubFX code, modified January 2011
*/

#ifndef LFO_PARAMS_H
#define LFO_PARAMS_H

#include "Misc/XMLwrapper.h"
#include "Params/Presets.h"
#include "Params/ControllableByMIDI.h"
class LFO;
class SynthEngine;

class LFOParams : public Presets, public ControllableByMIDI
{
    public:
        LFOParams(float Pfreq_, unsigned char Pintensity_, unsigned char Pstartphase_,
                  unsigned char PLFOtype_, unsigned char Prandomness_,
                  unsigned char Pdelay_, unsigned char Pcontinous, int fel_, SynthEngine *_synth);
        ~LFOParams() { }

        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);

        void changepar(int npar, double value);
        unsigned char getparChar(int npar){ return -1;};
        float getparFloat(int npar);

        enum {
            c_Pfreq,
            c_Pintensity,
            c_Pstartphase,
            c_PLFOtype,
            c_Prandomness,
            c_Pfreqrand,
            c_Pdelay,
            c_Pcontinous,
            c_Pstretch
        };

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

        int fel;         // kind of LFO - 0 frequency, 1 amplitude, 2 filter
        // static int time; // used by Pcontinous - moved to SynthEngine to make it per-instance

        void addLFO(LFO *lfo);
        void removeLFO(LFO *lfo);

    private:
        // Default parameters
        unsigned char Dfreq;
        unsigned char Dintensity;
        unsigned char Dstartphase;
        unsigned char DLFOtype;
        unsigned char Drandomness;
        unsigned char Ddelay;
        unsigned char Dcontinous;

        list<LFO*> lfos;
};

#endif
