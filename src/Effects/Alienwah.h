/*
    Alienwah.h - "AlienWah" effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018-2019, Will Godfrey

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
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

#include "Effects/Effect.h"
#include "Effects/EffectLFO.h"

static const int alienPRESET_SIZE = 11;
static const int alienNUM_PRESETS = 4;
static const unsigned char alienPresets[alienNUM_PRESETS][alienPRESET_SIZE] = {
        // AlienWah1
        { 127, 64, 70, 0, 0, 62, 60, 105, 25, 0, 64 },
        // AlienWah2
        { 127, 64, 73, 106, 0, 101, 60, 105, 17, 0, 64 },
        // AlienWah3
        { 127, 64, 63, 0, 1, 100, 112, 105, 31, 0, 42 },
        // AlienWah4
        { 93, 64, 25, 0, 1, 66, 101, 11, 47, 0, 86 }
};


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
        std::complex<float> *oldl, *oldr;
        std::complex<float> oldclfol, oldclfor;
        int oldk;

};

class Alienlimit
{
    public:
        float getlimits(CommandBlock *getData);
};

#endif

