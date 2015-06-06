/*
    EffectLFO.cpp - Stereo LFO used by some effects

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

#include <cstdlib>

#include "Misc/Master.h"
#include "Effects/EffectLFO.h"

EffectLFO::EffectLFO()
{
    xl = xr = 0.0;
    Pfreq = 40;
    Prandomness = 0;
    PLFOtype = 0;
    Pstereo = 96;

    updateParams();

    ampl1 = (1 - lfornd) + lfornd * RND;
    ampl2 = (1 - lfornd) + lfornd * RND;
    ampr1 = (1 - lfornd) + lfornd * RND;
    ampr2 = (1 - lfornd) + lfornd * RND;
}

EffectLFO::~EffectLFO() { }

// Update the changed parameters
void EffectLFO::updateParams(void)
{
    float lfofreq = (powf(2, Pfreq / 127.0 * 10.0) - 1.0) * 0.03;
    incx = fabsf(lfofreq) * (float)zynMaster->getBuffersize()
                / (float)zynMaster->getSamplerate();
    if (incx > 0.49999999)
        incx = 0.499999999; //Limit the Frequency

    lfornd = Prandomness / 127.0;
    lfornd = (lfornd > 1.0) ? 1.0 : lfornd;

    if (PLFOtype > 1)
        PLFOtype = 1; // this has to be updated if more lfo's are added
    lfotype = PLFOtype;

    xr = fmodf(xl + (Pstereo - 64.0) / 127.0 + 1.0, 1.0);
}

// Compute the shape of the LFO
float EffectLFO::getLfoShape(float x)
{
    float out;
    switch (lfotype)
    {
        case 1: // EffectLFO_TRIANGLE
            if (x > 0.0 && x < 0.25)
                out = 4.0 * x;
            else if (x > 0.25 && x < 0.75)
                out = 2 - 4 * x;
            else
                out = 4.0 * x - 4.0;
            break;
            // \todo more to be added here; also ::updateParams() need to be
            // updated (to allow more lfotypes)
        default:
            out = cosf(x * 2 * PI); // EffectLFO_SINE
    }
    return out;
}

// LFO output
void EffectLFO::effectLfoOut(float *outl, float *outr)
{
    float out;

    out = getLfoShape(xl);
    if (lfotype == 0 || lfotype == 1)
        out *= (ampl1 + xl * (ampl2 - ampl1));
    xl += incx;
    if (xl > 1.0)
    {
        xl -= 1.0;
        ampl1 = ampl2;
        ampl2 = (1.0 - lfornd) + lfornd * RND;
    }
    *outl = (out + 1.0) * 0.5;

    out = getLfoShape(xr);
    if (lfotype == 0 || lfotype == 1)
        out *= (ampr1 + xr * (ampr2 - ampr1));
    xr += incx;
    if (xr > 1.0)
    {
        xr -= 1.0;
        ampr1 = ampr2;
        ampr2 = (1.0 - lfornd) + lfornd * RND;
    }
    *outr = (out + 1.0) * 0.5;
}
