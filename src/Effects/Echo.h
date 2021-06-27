/*
    Echo.h - Echo Effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018-2019, Will Godfrey

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

    Modified March 2019
*/

#ifndef ECHO_H
#define ECHO_H

#include "Effects/Effect.h"

// The ratio which, when exceeded, causes the echo effect to update its internal
// delay. If not exceeded, the delay remains constant even if the BPM
// changes. This is to combat fluctuations in inaccurate BPM sources, such as
// ALSA. Must be a number above 1.0f.
#define ECHO_INACCURATE_BPM_THRESHOLD 1.02f

class SynthEngine;

class Echo : public Effect
{
    public:
        Echo(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth);
        ~Echo();

        void out(float *smpsl, float *smpr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar);
        int getnumparams(void);
        void cleanup(void);
        void setdryonly(void);

    private:
        // Parameters
        bool Pchanged;
        unsigned char Pvolume;  // 1 Volume or Dry/Wetness
        unsigned char Pdelay;   // 3 Delay of the Echo
        unsigned char Plrdelay; // 4 L/R delay difference
        unsigned char Pfb;      // 6 Feedback
        unsigned char Phidamp;  // 7 Dampening of the Echo
        bool Pbpm;

        void setvolume(unsigned char Pvolume_);
        void setdelay(unsigned char Pdelay_);
        void setlrdelay(unsigned char Plrdelay_);
        void setfb(unsigned char Pfb_);
        void sethidamp(unsigned char Phidamp_);

        // Real Parameters
        synth::InterpolatedValue<float> fb, hidamp;
        int dl, dr, delay, lrdelay;

        void initdelays(void);
        float *ldelay;
        float *rdelay;
        int maxdelay;
        float  oldl, oldr; // pt. lpf

        float prevBeat; // Used to calculate BPM.

        int realposl, realposr;
        synth::InterpolatedValue<int> lxfade, rxfade;
};

class Echolimit
{
    public:
        float getlimits(CommandBlock *getData);
};


#endif

