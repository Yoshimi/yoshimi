/*
    Reverb.h - Reverberation effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
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

    Modified march 2019
*/

#ifndef REVERB_H
#define REVERB_H

#include "Misc/Alloc.h"
#include "Effects/Effect.h"

#define REV_COMBS 8
#define REV_APS 4


class Unison;
class AnalogFilter;

class SynthEngine;

class Reverb : public Effect
{
    public:
        Reverb(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth);
        ~Reverb();
        void out(float *rawL, float *rawR);
        void cleanup(void);

        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar);


    private:
        static constexpr size_t NUM_TYPES = 3;

        // Parametrii
        bool Pchanged;
        unsigned char Pvolume;
        unsigned char Ptime;
        unsigned char Pidelay;
        unsigned char Pidelayfb;
//        unsigned char Prdelay;  // **** RHL ****
//        unsigned char Perbalance;   // **** RHL ****
        unsigned char Plpf;
        unsigned char Phpf; // todo 0..63 lpf, 64 = off, 65..127 = hpf(TODO)
        unsigned char Plohidamp;
        unsigned char Ptype;
        unsigned char Proomsize;
        unsigned char Pbandwidth;

        // parameter control
        void setvolume(unsigned char Pvolume_);
        void settime(unsigned char Ptime_);
        void setlohidamp(unsigned char Plohidamp_);
        void setidelay(unsigned char Pidelay_);
        void setidelayfb(unsigned char Pidelayfb_);
        void sethpf(unsigned char Phpf_);
        void setlpf(unsigned char Plpf_);
        void settype(unsigned char Ptype_);
        void setroomsize(unsigned char Proomsize_);
        void setbandwidth(unsigned char Pbandwidth_);

//        float erbalance;    // **** RHL ****

        // Parametrii 2
        int lohidamptype; // 0 = disable, 1 = highdamp (lowpass), 2 = lowdamp (highpass)
        int idelaylen;
//        int rdelaylen;  // **** RHL ****
        int idelayk;
        float lohifb;
        float idelayfb;
        float roomsize;
        float rs; // rs is used to "normalise" the volume according to the roomsize
        size_t comblen[REV_COMBS * 2];   // length for each CombFilter feedback line (random)
        size_t aplen[REV_APS * 2];       // length for each AllPass feedback line (random)
        Unison *bandwidth;

        // Internal Variables
        float *comb[REV_COMBS * 2];   // N CombFilter pipelines for each channel
        size_t combk[REV_COMBS * 2];  // current offset of the comb insertion point (cycling)
        float combfb[REV_COMBS * 2];  // feedback coefficient of each Comb-filter
        float lpcomb[REV_COMBS * 2];  // LowPass filtered output feedback from Comb
        float *ap[REV_APS * 2];       // AllPass-filter
        size_t apk[REV_APS * 2];      // current offset of the AllPass insertion point (cycling)
        float *idelay;                // Input delay line
        AnalogFilter *lpf;            // LowPass-filter on the input
        AnalogFilter *hpf;            // HighPass-filter on the input
        synth::InterpolatedValue<float> lpffr;
        synth::InterpolatedValue<float> hpffr;
        Samples inputbuf;

        void preprocessInput(float *rawL, float *rawR, Samples& inputFeed);
        void calculateReverb(size_t ch, Samples& inputFeed, float *output);
        void setupPipelines();
        void clearBuffers();
};

class Revlimit
{
    public:
        float getlimits(CommandBlock *getData);
};

#endif
