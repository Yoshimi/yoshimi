/*
    Alienwah.h - "AlienWah" effect

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

        void setPreset(unsigned char npreset);
        void changePar(int npar, unsigned char value);
        unsigned char getPar(int npar) const;
        void Cleanup(void);

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
        void setVolume(unsigned char _volume);
        void setPanning(unsigned char _panning);
        void setDepth(unsigned char _depth);
        void setFb(unsigned char _fb);
        void setLrCross(unsigned char _lrcross);
        void setDelay(unsigned char _delay);
        void setPhase(unsigned char _phase);

        // Internal Values
        float panning, fb, depth, lrcross, phase;
        complex<float> *oldl, *oldr;
        complex<float> oldclfol, oldclfor;
        int oldk;

};

#endif

