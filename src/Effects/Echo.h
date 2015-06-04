/*
    Echo.h - Echo Effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ECHO_H
#define ECHO_H

#include "Effects/Effect.h"
#include "Effects/Fader.h"

class Echo : public Effect
{
    public:
        Echo(bool insertion_, float *efxoutl_, float *efxoutr_);
        ~Echo();

        void out(float *smpsl, float *smpr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar) const;
        int getnumparams(void);
        void cleanup(void);
        void setdryonly(void);

    private:
        // Parameters
        unsigned char Pvolume;  // <#1 Volume or Dry/Wetness
        unsigned char Ppanning; // <#2 Panning
        unsigned char Pdelay;   // <#3 Delay of the Echo
        unsigned char Plrdelay; // <#4 L/R delay difference
        unsigned char Plrcross; // <#5 L/R Mixing
        unsigned char Pfb;      // <#6 Feedback
        unsigned char Phidamp;  // <#7 Dampening of the Echo

        void setvolume(unsigned char value);
        void setpanning(unsigned char _panning);
        void setdelay(unsigned char _delay);
        void setlrdelay(unsigned char _lrdelay);
        void setlrcross(unsigned char _lrcross);
        void setfb(unsigned char _fb);
        void sethidamp(unsigned char _hidamp);

        // Real Parameters
        float panning, lrcross, fb, hidamp;
        int dl, dr, delay, lrdelay;

        void initdelays(void);
        float *ldelay;
        float *rdelay;
        float  oldl, oldr; // pt. lpf

        int kl, kr;
        Fader *fader6db;
};

#endif

