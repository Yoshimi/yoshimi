/*
    Chorus.h - Chorus and Flange effects

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

    This file is a derivative of a ZynAddSubFX original.

    Modified March 2019
*/

#ifndef CHORUS_H
#define CHORUS_H

#include "Effects/Effect.h"
#include "Effects/EffectLFO.h"

static const int chorusPRESET_SIZE = 12;
static const int chorusNUM_PRESETS = 10;
static const unsigned char chorusPresets[chorusNUM_PRESETS][chorusPRESET_SIZE] = {
        // Chorus1
        { 64, 64, 50, 0, 0, 90, 40, 85, 64, 119, 0, 0 },
        // Chorus2
        {64, 64, 45, 0, 0, 98, 56, 90, 64, 19, 0, 0 },
        // Chorus3
        {64, 64, 29, 0, 1, 42, 97, 95, 90, 127, 0, 0 },
        // Celeste1
        {64, 64, 26, 0, 0, 42, 115, 18, 90, 127, 0, 0 },
        // Celeste2
        {64, 64, 29, 117, 0, 50, 115, 9, 31, 127, 0, 1 },
        // Flange1
        {64, 64, 57, 0, 0, 60, 23, 3, 62, 0, 0, 0 },
        // Flange2
        {64, 64, 33, 34, 1, 40, 35, 3, 109, 0, 0, 0 },
        // Flange3
        {64, 64, 53, 34, 1, 94, 35, 3, 54, 0, 0, 1 },
        // Flange4
        {64, 64, 40, 0, 1, 62, 12, 19, 97, 0, 0, 0 },
        // Flange5
        {64, 64, 55, 105, 0, 24, 39, 19, 17, 0, 0, 1 }
};

class SynthEngine;

class Chorus : public Effect
{
    public:
        Chorus(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth);
        ~Chorus() { }

        void out(float *smpsl, float *smpsr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar);
        void cleanup();

    private:
        // Chorus Parameters
        bool Pchanged;
        unsigned char Pvolume;
        unsigned char Pdepth;      // the depth of the Chorus(ms)
        unsigned char Pdelay;      // the delay (ms)
        unsigned char Pfb;         // feedback
        unsigned char Pflangemode; // how the LFO is scaled, to result chorus or flange
        unsigned char Poutsub;     // if I wish to subtract the output instead of the adding it
        EffectLFO lfo;             // lfo-ul chorus


        // Parameter Controls
        void setvolume(unsigned char Pvolume_);
        void setdepth(unsigned char Pdepth_);
        void setdelay(unsigned char Pdelay_);
        void setfb(unsigned char Pfb_);
        float getdelay(float xlfo);

        // Internal Values
        float depth;
        float delay;
        synth::InterpolatedValue<float> fb;
        float dl1;
        float dl2;
        float dr1;
        float dr2;
        float lfol;
        float lfor;

        float *delayl;
        float *delayr;
        int maxdelay;
        int dlk;
        int drk;
        int dlhi;
        int dlhi2;
        float dllo;
        float mdel;
};
class Choruslimit
{
    public:
        float getlimits(CommandBlock *getData);
};

#endif

