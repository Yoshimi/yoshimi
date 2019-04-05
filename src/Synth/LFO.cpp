/*
    LFO.cpp - LFO implementation

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

#include <cmath>

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Synth/LFO.h"


LFO::LFO(LFOParams *lfopars, float basefreq)
{
    if (lfopars->Pstretch == 0)
        lfopars->Pstretch = 1;
    float lfostretch =
        powf(basefreq / 440.0f, (lfopars->Pstretch - 64.0f) / 63.0f); // max 2x/octave

    float lfofreq =
        (powf(2.0f, lfopars->Pfreq * 10.0f) - 1.0f) / 12.0f * lfostretch;
    incx = fabsf(lfofreq) * (float)zynMaster->getBuffersize()
           / (float)zynMaster->getSamplerate();

    if (lfopars->Pcontinous == 0)
    {
        if (lfopars->Pstartphase == 0)
            x = zynMaster->numRandom();
        else
            x = fmodf((float)((lfopars->Pstartphase - 64.0) / 127.0 + 1.0),
                     (float)1.0);
    }
    else
    {
        float tmp = fmodf(lfopars->time * incx, (float)1.0);
        x = fmodf((float)((lfopars->Pstartphase - 64.0) / 127.0 + 1.0 + tmp),
                 (float)1.0);
    }

    // Limit the Frequency(or else...)
    if (incx > 0.49999999)
        incx = 0.499999999;

    lfornd = lfopars->Prandomness / 127.0;
    if (lfornd < 0.0)
        lfornd = 0.0;
    else if (lfornd > 1.0)
        lfornd = 1.0;

    // (orig comment) lfofreqrnd=pow(lfopars->Pfreqrand/127.0,2.0)*2.0*4.0;
    lfofreqrnd = powf(lfopars->Pfreqrand / 127.0f, 2.0f) * 4.0;

    switch (lfopars->fel)
    {
        case 1:
            lfointensity = lfopars->Pintensity / 127.0;
            break;
        case 2:
            lfointensity = lfopars->Pintensity / 127.0 * 4.0;
            break; // in octave
        default:
            lfointensity = powf(2.0f, lfopars->Pintensity / 127.0f * 11.0f) - 1.0; // in centi
            x -= 0.25; // chance the starting phase
            break;
    }

    amp1 = (1 - lfornd) + lfornd * zynMaster->numRandom();
    amp2 = (1 - lfornd) + lfornd * zynMaster->numRandom();
    lfotype = lfopars->PLFOtype;
    lfodelay = lfopars->Pdelay / 127.0 * 4.0; // 0..4 sec
    incrnd = nextincrnd = 1.0;
    freqrndenabled = (lfopars->Pfreqrand != 0);
    computenextincrnd();
    computenextincrnd(); // twice because I want incrnd & nextincrnd to be random
}

// LFO out
float LFO::lfoout(void)
{
    float out;
    switch (lfotype)
    {
        case 1: // LFO_TRIANGLE
            if (x >= 0.0 && x < 0.25)
                out = 4.0 * x;
            else if (x > 0.25 && x < 0.75)
                out = 2 - 4 * x;
            else
                out = 4.0 * x - 4.0;
            break;
        case 2: // LFO_SQUARE
            if (x < 0.5)
                out = -1;
            else
                out = 1;
            break;
        case 3: // LFO_RAMPUP
            out = (x - 0.5) * 2.0;
            break;
        case 4: // LFO_RAMPDOWN
            out = (0.5 - x) * 2.0;
            break;
        case 5: // LFO_EXP_DOWN 1
            out = powf(0.05f, x) * 2.0 - 1.0;
            break;
        case 6: // LFO_EXP_DOWN 2
            out = powf(0.001f, x) * 2.0 - 1.0;
            break;
        default:
            out = cosf( x * 2.0 * PI); // LFO_SINE
    }

    if (lfotype == 0 || lfotype == 1)
        out *= lfointensity * (amp1 + x * (amp2 - amp1));
    else
        out *= lfointensity * amp2;
    if (lfodelay < 0.00001)
    {
        if (freqrndenabled == 0)
            x += incx;
        else
        {
            float tmp = (incrnd * (1.0 - x) + nextincrnd * x);
            tmp = (tmp > 1.0) ? 1.0 : tmp;
            x += incx * tmp;
        }
        if (x >= 1)
        {
            x = fmodf(x, (float)1.0);
            amp1 = amp2;
            amp2 = (1 - lfornd) + lfornd * zynMaster->numRandom();

            computenextincrnd();
        }
    } else
        lfodelay -= (float)zynMaster->getBuffersize()
                     / (float)zynMaster->getSamplerate();
    return out;
}

// LFO out (for amplitude)
float LFO::amplfoout(void)
{
    float out;
    out = 1.0 - lfointensity + lfoout();
    out = (out <- 1.0) ? -1.0 : out;
    out = (out > 1.0) ? 1.0 : out;
    return out;
}


void LFO::computenextincrnd(void)
{
    if (freqrndenabled == 0)
        return;
    incrnd = nextincrnd;
    nextincrnd = powf(0.5f, lfofreqrnd) + zynMaster->numRandom() * (powf(2.0f, lfofreqrnd) - 1.0f);
}
