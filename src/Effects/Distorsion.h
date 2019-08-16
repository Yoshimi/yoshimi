/*
    Distorsion.h - Distortion Effect

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

#ifndef DISTORSION_H
#define DISTORSION_H

#include "Misc/WaveShapeSamples.h"
#include "DSP/AnalogFilter.h"
#include "Effects/Effect.h"

class SynthEngine;

class Distorsion : public Effect, WaveShapeSamples
{
    public:
        Distorsion(bool insertion, float *efxoutl_, float *efxoutr_, SynthEngine *_synth);
        ~Distorsion();
        void out(float *smpsl, float *smpr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar);
        void cleanup(void);
        void applyfilters(float *efxoutl, float *efxoutr);

    private:
        // Parameters
        bool Pchanged;
        unsigned char Pvolume;       // Volume or E/R
        unsigned char Pdrive;        // the input amplification
        unsigned char Plevel;        // the output amplification
        unsigned char Ptype;         // Distortion type
        unsigned char Pnegate;       // if the input is negated
        unsigned char Plpf;          // Lowpass filter
        unsigned char Phpf;          // Highpass filter
        unsigned char Pstereo;       // 0 = mono, 1 = stereo
        unsigned char Pprefiltering; // if you want to do the filtering before the distortion

        void setvolume(unsigned char Pvolume_);
        void setlpf(unsigned char Plpf_);
        void sethpf(unsigned char Phpf_);

        InterpolatedParameter level;

        // Real Parameters
        AnalogFilter *lpfl;
        AnalogFilter *lpfr;
        AnalogFilter *hpfl;
        AnalogFilter *hpfr;
        InterpolatedParameter lpffr;
        InterpolatedParameter hpffr;

        SynthEngine *synth;
};

class Distlimit
{
    public:
        float getlimits(CommandBlock *getData);
};

#endif
