/*
    Echo.h - Echo Effect

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

#ifndef ECHO_H
#define ECHO_H

#include "Effects/Effect.h"

class Echo : public Effect
{
    public:
        Echo(bool insertion_, float *efxoutl_, float *efxoutr_);
        ~Echo();

        void out(float *smpsl, float *smpr);
        void setPreset(unsigned char npreset);
        void changePar(int npar, unsigned char value);
        unsigned char getPar(int npar) const;
        int getNumParams(void);
        void Cleanup(void);
        void setDryonly(void);

    private:
        // Parameters
        unsigned char Pvolume;  // <#1 Volume or Dry/Wetness
        unsigned char Ppanning; // <#2 Panning
        unsigned char Pdelay;   // <#3 Delay of the Echo
        unsigned char Plrdelay; // <#4 L/R delay difference
        unsigned char Plrcross; // <#5 L/R Mixing
        unsigned char Pfb;      // <#6 Feedback
        unsigned char Phidamp;  // <#7 Dampening of the Echo

        void setVolume(unsigned char value);
        void setPanning(unsigned char _panning);
        void setDelay(unsigned char _delay);
        void setLrDelay(unsigned char _lrdelay);
        void setLrCross(unsigned char _lrcross);
        void setFb(unsigned char _fb);
        void setHiDamp(unsigned char _hidamp);

        // Real Parameters
        float panning, lrcross, fb, hidamp;
        int dl, dr, delay, lrdelay;

        void initDelays(void);
        float *ldelay;
        float *rdelay;
        float  oldl, oldr; // pt. lpf

        int kl, kr;
};

#endif

