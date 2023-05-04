/*
    Phaser.h - Phaser effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert
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

    This file is a derivative of the ZynAddSubFX original.

    Modified March 2019
*/

#ifndef PHASER_H
#define PHASER_H

#include "Effects/Effect.h"
#include "Effects/EffectLFO.h"

const int phaserPRESET_SIZE = 15;
const int phaserNUM_PRESETS = 12;
const unsigned char phaserPresets[phaserNUM_PRESETS][phaserPRESET_SIZE] = {
        // Phaser
        // 0   1    2    3  4   5     6   7   8    9 10   11 12  13 14
        {64, 64, 36,  0,   0, 64,  110, 64,  1,  0,   0, 20, 0, 0,  0 },
        {64, 64, 35,  0,   0, 88,  40,  64,  3,  0,   0, 20, 0,  0, 0 },
        {64, 64, 31,  0,   0, 66,  68,  107, 2,  0,   0, 20, 0,  0, 0 },
        {39, 64, 22,  0,   0, 66,  67,  10,  5,  0,   1, 20, 0,  0, 0 },
        {64, 64, 20,  0,   1, 110, 67,  78,  10, 0,   0, 20, 0,  0, 0 },
        {64, 64, 53,  100, 0, 58,  37,  78,  3,  0,   0, 20, 0,  0, 0 },
        // APhaser
        // 0   1    2   3   4   5     6   7   8    9 10   11 12  13 14
        {64, 64, 14,  0,   1, 64,  64,  40,  4,  10,  0, 110,1,  20, 1 },
        {64, 64, 14,  5,   1, 64,  70,  40,  6,  10,  0, 110,1,  20, 1 },
        {64, 64, 9,   0,   0, 64,  60,  40,  8,  10,  0, 40, 0,  20, 1 },
        {64, 64, 14,  10,  0, 64,  45,  80,  7,  10,  1, 110,1,  20, 1 },
        {25, 64, 127, 10,  0, 64,  25,  16,  8,  100, 0, 25, 0,  20, 1 },
        {64, 64, 1,   10,  1, 64,  70,  40,  12, 10,  0, 110,1,  20, 1 }
    };

class SynthEngine;

class Phaser : public Effect
{
    public:
        Phaser(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth);
        ~Phaser();
        void out(float *smpsl, float *smpsr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar);
        void cleanup(void);
        void setdryonly(void);

    private:
        // Phaser Parameters
        bool Pchanged;
        EffectLFO lfo;           // <lfo-ul Phaser
        unsigned char Pvolume;
//        unsigned char Ppanning;
        unsigned char Pdistortion;  // Model distortion added by FET element
        unsigned char Pdepth;    // <depth of Phaser sweep
        unsigned char Pwidth;       //Phaser width (LFO amplitude)
        unsigned char Pfb;       // <feedback
        unsigned char Poffset;      //Model mismatch between variable resistors
//        unsigned char Plrcross;  // <feedback
        unsigned char Pstages;
        unsigned char Poutsub;   // <substract the output instead of adding it
        unsigned char Pphase;
        unsigned char Phyper;       //lfo^2 -- converts tri into hyper-sine
        unsigned char Panalog;

        // Control Parameters
        void setvolume(unsigned char Pvolume_);
        void setdepth(unsigned char Pdepth_);
        void setfb(unsigned char Pfb_);
        void setdistortion(unsigned char Pdistortion_);
        void setwidth(unsigned char Pwidth_);
        void setoffset(unsigned char Poffset_);
        void setstages(unsigned char Pstages_);
        void setphase(unsigned char Pphase_);

        // Internal Values
        // int insertion; // inherited from Effect
        bool    barber; // Barber pole phasing flag
        float   distortion;
        float   width;
        float   offsetpct;
        float   fb;
        float   depth;
        float   fbl;
        float   fbr;
        float   phase;
        float   invperiod;
        float   offset[12];

        float  *oldl;
        float  *oldr;
        float  *xn1l;
        float  *xn1r;
        float  *yn1l;
        float  *yn1r;

        float   diffl;
        float   diffr;
        float   oldlgain;
        float   oldrgain;

        float mis;
        float Rmin;     // 3N5457 typical on resistance at Vgs = 0
        float Rmax;     // Resistor parallel to FET
        float Rmx;      // Rmin/Rmax to avoid division in loop
        float Rconst;   // Handle parallel resistor relationship
        float C;        // Capacitor
        float CFs;      // A constant derived from capacitor and resistor relationships
        void analog_setup();
        void AnalogPhase(float *smpsl, float *smpsr);
        //analog case
        float applyPhase(float x, float g, float fb,
                         float &hpf, float *yn1, float *xn1);

        void NormalPhase(float *smpsl, float *smpsr);
        float applyPhase(float x, float g, float *old);
};

class Phaserlimit
{
    public:
        float getlimits(CommandBlock *getData);
};

#endif
