/*
    Alienwah.h - "AlienWah" effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

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

    This file is a derivative of the ZynAddSubFX original, modified January 2010
*/

#ifndef ALIENWAH_H
#define ALIENWAH_H

#include <complex>

using namespace std;

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
        unsigned char getpar(int npar);
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
        void setvolume(unsigned char Pvolume_);
        void setpanning(unsigned char Ppanning_);
        void setdepth(unsigned char Pdepth_);
        void setfb(unsigned char Pfb_);
        void setlrcross(unsigned char Plrcross_);
        void setdelay(unsigned char Pdelay_);
        void setphase(unsigned char Pphase_);

        // Internal Values
        float panning, fb, depth, lrcross, phase;
        complex<float> *oldl, *oldr;
        complex<float> oldclfol, oldclfor;
        int oldk;

};

#endif

