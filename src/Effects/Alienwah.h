/*
    Alienwah.h - "AlienWah" effect

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

#ifndef ALIENWAH_H
#define ALIENWAH_H

#include <complex>

using namespace std;

#include "globals.h"
#include "Effects/Effect.h"
#include "Effects/EffectLFO.h"

#define MAX_ALIENWAH_DELAY 100

class Alienwah : public Effect
{
    public:
        Alienwah(bool insertion_, float *efxoutl_, float *efxoutr_);
        ~Alienwah();
        void out(float *smpsl, float *smpsr);

        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar) const;
        void cleanup(void);

    private:
        // Alienwah Parameters
        EffectLFO lfo; // lfo-ul Alienwah
        unsigned char Pvolume;
        unsigned char Ppanning;
        unsigned char Pdepth;   // the depth of the Alienwah
        unsigned char Pfb;      // feedback
        unsigned char Plrcross; // feedback
        unsigned char Pdelay;
        unsigned char Pphase;


        // Control Parameters
        void setvolume(unsigned char _volume);
        void setpanning(unsigned char _panning);
        void setdepth(unsigned char _depth);
        void setfb(unsigned char _fb);
        void setlrcross(unsigned char _lrcross);
        void setdelay(unsigned char _delay);
        void setphase(unsigned char _phase);

        // Internal Values
        float panning, fb, depth, lrcross, phase;
        complex<float> *oldl, *oldr;
        complex<float> oldclfol, oldclfor;
        int oldk;
};

#endif

