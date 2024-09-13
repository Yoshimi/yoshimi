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

#include "globals.h"
#include "Misc/Alloc.h"
#include "Effects/Effect.h"

#define REV_COMBS 8
#define REV_APS 4

static const int reverbPRESET_SIZE = 13;
static const int reverbNUM_PRESETS = 13;
static const uchar reverbPresets[reverbNUM_PRESETS][reverbPRESET_SIZE] = {
        // Cathedral1
        {80,  64,  63,  24,  0,  0,  0, 85,  5,  83,   1,  64,  20 },
        // Cathedral2
        {80,  64,  69,  35,  0,  0,  0, 127, 0,  71,   0,  64,  20 },
        // Cathedral3
        {80,  64,  69,  24,  0,  0,  0, 127, 75, 78,   1,  85,  20 },
        // Hall1
        {90,  64,  51,  10,  0,  0,  0, 127, 21, 78,   1,  64,  20 },
        // Hall2
        {90,  64,  53,  20,  0,  0,  0, 127, 75, 71,   1,  64,  20 },
        // Room1
        {100, 64,  33,  0,   0,  0,  0, 127, 0,  106,  0,  30,  20 },
        // Room2
        {100, 64,  21,  26,  0,  0,  0, 62,  0,  77,   1,  45,  20 },
        // Basement
        {110, 64,  14,  0,   0,  0,  0, 127, 5,  71,   0,  25,  20 },
        // Tunnel
        {85,  80,  84,  20,  42, 0,  0, 51,  0,  78,   1,  105, 20 },
        // Echoed1
        {95,  64,  26,  60,  71, 0,  0, 114, 0,  64,   1,  64,  20 },
        // Echoed2
        {90,  64,  40,  88,  71, 0,  0, 114, 0,  88,   1,  64,  20 },
        // VeryLong1
        {90,  64,  93,  15,  0,  0,  0, 114, 0,  77,   0,  95,  20 },
        // VeryLong2
        {90,  64,  111, 30,  0,  0,  0, 114, 90, 74,   1,  80,  20 }
};

class Unison;
class AnalogFilter;

class SynthEngine;

class Reverb : public Effect
{
    public:
       ~Reverb();
        Reverb(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine&);
        void out(float* rawL, float* rawR) override;
        void cleanup() override;

        void setpreset(uchar npreset) override;
        void changepar(int npar, uchar value) override;
        uchar getpar(int npar) const override;


    private:
        static constexpr size_t NUM_TYPES = 3;

        // Parameters
        bool Pchanged;
        uchar Pvolume;
        uchar Ptime;
        uchar Pidelay;
        uchar Pidelayfb;
//        uchar Prdelay;  // **** RHL ****
//        uchar Perbalance;   // **** RHL ****
        uchar Plpf;
        uchar Phpf; // todo 0..63 lpf, 64 = off, 65..127 = hpf(TODO)
        uchar Plohidamp;
        uchar Ptype;
        uchar Proomsize;
        uchar Pbandwidth;

        // parameter control
        void setvolume(uchar Pvolume_);
        void settime(uchar Ptime_);
        void setlohidamp(uchar Plohidamp_);
        void setidelay(uchar Pidelay_);
        void setidelayfb(uchar Pidelayfb_);
        void sethpf(uchar Phpf_);
        void setlpf(uchar Plpf_);
        void settype(uchar Ptype_);
        void setroomsize(uchar Proomsize_);
        void setbandwidth(uchar Pbandwidth_);

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
