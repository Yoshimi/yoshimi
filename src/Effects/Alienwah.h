/*
    Alienwah.h - "AlienWah" effect

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

#ifndef ALIENWAH_H
#define ALIENWAH_H

#include <complex>

using namespace std;

#include "Effects/Effect.h"
#include "Effects/EffectLFO.h"

class SynthEngine;

class Alienwah : public Effect
{
    public:
        Alienwah(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth);
        ~Alienwah();
        void out(float *smpsl, float *smpsr);

        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar);
        void cleanup(void);

    private:
        // Alienwah Parameters
        bool Pchanged;
        EffectLFO lfo; // lfo-ul Alienwah
        unsigned char Pvolume;
        unsigned char Pdepth;   // the depth of the Alienwah
        unsigned char Pfb;      // feedback
        unsigned char Pdelay;
        unsigned char Pphase;


        // Control Parameters
        void setvolume(unsigned char Pvolume_);
        void setdepth(unsigned char Pdepth_);
        void setfb(unsigned char Pfb_);
        void setdelay(unsigned char Pdelay_);
        void setphase(unsigned char Pphase_);

        // Internal Values
        float fb, depth, phase;
        complex<float> *oldl, *oldr;
        complex<float> oldclfol, oldclfor;
        int oldk;

        SynthEngine *synth;

};

class Alienlimit
{
    public:
        float getlimits(CommandBlock *getData);
};

#endif

