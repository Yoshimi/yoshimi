/*
    EffectLFO.h - Stereo LFO used by some effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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

    This file is a derivative of a ZynAddSubFX original, modified October 2009
*/

#ifndef EFFECT_LFO_H
#define EFFECT_LFO_H

class SynthEngine;

class EffectLFO
{
    public:
        EffectLFO(SynthEngine *_synth);
        ~EffectLFO();
        void effectlfoout(float *outl, float *outr);
        void updateparams(void);
        unsigned char Pfreq;
        unsigned char Prandomness;
        unsigned char PLFOtype;
        unsigned char Pstereo; // 64 = center
    private:
        float getlfoshape(float x);

        float xl,xr;
        float incx;
        float ampl1, ampl2, ampr1, ampr2; // necessary for "randomness"
        float lfornd;
        char lfotype;

        SynthEngine *synth;
};

#endif
