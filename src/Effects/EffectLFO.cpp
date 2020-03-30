/*
    EffectLFO.cpp - Stereo LFO used by some effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified March 2011
*/

#include <cstdlib>

#include "Misc/SynthEngine.h"
#include "Effects/EffectLFO.h"

EffectLFO::EffectLFO(SynthEngine *_synth) :
    Pfreq(40),
    Prandomness(0),
    PLFOtype(0),
    Pstereo(64),
    xl(0.0f),
    xr(0.0f),
    ampl1(_synth->numRandom()),
    ampl2(_synth->numRandom()),
    ampr1(_synth->numRandom()),
    ampr2(_synth->numRandom()),
    lfornd(0.0f),
    synth(_synth)
{
    updateparams();
}


EffectLFO::~EffectLFO()
{

}


// Update the changed parameters
void EffectLFO::updateparams(void)
{
    float lfofreq = (powf(2.0f, Pfreq / 127.0f * 10.0f) - 1.0f) * 0.03f;
    incx = fabsf(lfofreq) * synth->fixed_sample_step_f;
    if (incx > 0.49999999f)
        incx = 0.499999999f; // Limit the Frequency

    lfornd = Prandomness / 127.0f;
    lfornd = (lfornd > 1.0f) ? 1.0f : lfornd;

    if (PLFOtype > 1)
        PLFOtype = 1; // this has to be updated if more lfo's are added
    lfotype = PLFOtype;
    xr = fmodf(xl + (Pstereo - 64.0f) / 127.0f + 1.0f, 1.0f);
}


// Compute the shape of the LFO
float EffectLFO::getlfoshape(float x)
{
    float out;
    switch (lfotype)
    {
        case 1: // EffectLFO_TRIANGLE
            if (x > 0.0f && x < 0.25f)
                out = 4.0f * x;
            else if (x > 0.25f && x < 0.75f)
                out = 2.0f - 4.0f * x;
            else
                out = 4.0f * x - 4.0f;
            break;
            // \todo more to be added here; also ::updateParams() need to be
            // updated (to allow more lfotypes)
        default:
            out = cosf(x * TWOPI); // EffectLFO_SINE
    }
    return out;
}


// LFO output
void EffectLFO::effectlfoout(float *outl, float *outr)
{
    float out;

    out = getlfoshape(xl);
    if (lfotype == 0 || lfotype == 1)
        out *= (ampl1 + xl * (ampl2 - ampl1));
    xl += incx;
    if (xl > 1.0f)
    {
        xl -= 1.0f;
        ampl1 = ampl2;
        ampl2 = (1.0f - lfornd) + lfornd * synth->numRandom();
    }
    *outl = (out + 1.0f) * 0.5f;

    out = getlfoshape(xr);
    if (lfotype == 0 || lfotype == 1)
        out *= (ampr1 + xr * (ampr2 - ampr1));
    xr += incx;
    if (xr > 1.0f)
    {
        xr -= 1.0f;
        ampr1 = ampr2;
        ampr2 = (1.0f - lfornd) + lfornd * synth->numRandom();
    }
    *outr = (out + 1.0f) * 0.5f;
}
