/*
    Resonance.h - Resonance

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2018 Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original

    Modified October 2018
*/

#ifndef RESONANCE_H
#define RESONANCE_H

#include "DSP/FFTwrapper.h"
#include "Misc/XMLwrapper.h"
#include "Params/Presets.h"

class SynthEngine;

class Resonance : public Presets
{
    public:
        Resonance(SynthEngine *_synth);
        ~Resonance() { }
        void setpoint(int n, unsigned char p);
        void applyres(int n, FFTFREQS fftdata, float freq);
        void smooth(void);
        void interpolatepeaks(int type);
        void randomize(int type);

        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);

        float getfreqpos(float freq);
        float getfreqx(float x);
        float getfreqresponse(float freq);
        float getcenterfreq(void);
        float getoctavesfreq(void);
        void sendcontroller(unsigned short int ctl, float par);

        // parameters
        unsigned char Penabled;                 //if the ressonance is enabled
        unsigned char Prespoints[MAX_RESONANCE_POINTS];	// how many points define the resonance function
        unsigned char PmaxdB;			        // how many dB the signal may be amplified
        unsigned char Pcenterfreq,Poctavesfreq; // the center frequency of the res. func., and the number of octaves
        unsigned char Pprotectthefundamental;   // the fundamental (1-st harmonic) is not damped, even it resonance function is low

        // controllers
        float ctlcenter; // center frequency(relative)
        float ctlbw;     // bandwidth(relative)
};

class ResonanceLimits
{
    public:
        float getLimits(CommandBlock *getData);
};

#endif
