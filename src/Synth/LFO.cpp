/*
    LFO.cpp - LFO implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017, Will Godfrey & others

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

    This file is derivative of ZynAddSubFX original code
    Modified February 2017
*/

#include <cmath>

#include "Misc/SynthEngine.h"
#include "Synth/LFO.h"


LFO::LFO(LFOParams *_lfopars, float _basefreq, SynthEngine *_synth):
    lfopars(_lfopars),
    basefreq(_basefreq),
    synth(_synth)
{
    if (lfopars->Pstretch == 0)
        lfopars->Pstretch = 1;

    RecomputeFreq(); // need incx early

    if (lfopars->Pcontinous == 0)
    { // pre-init phase
        if (lfopars->Pstartphase == 0)
            x = synth->numRandom();
        else
            x = fmodf(((float)((int)lfopars->Pstartphase - 64) / 127.0f + 1.0f), 1.0f);
    }
    else
    { // pre-init phase, synced to other notes
        float tmp = fmodf(synth->getLFOtime() * incx, 1.0f);
        x = fmodf((((int)lfopars->Pstartphase - 64) / 127.0f + 1.0f + tmp), 1.0f);
    }

    lfodelay = lfopars->Pdelay / 127.0f * 4.0f; // 0..4 sec
    incrnd = nextincrnd = 1.0f;

    Recompute();
    amp1 = (1 - lfornd) + lfornd * synth->numRandom();
    amp2 = (1 - lfornd) + lfornd * synth->numRandom();
    computenextincrnd(); // twice because I want incrnd & nextincrnd to be random
}

inline void LFO::Recompute(void)
{
    // mostly copied from LFO::LFO()
    RecomputeFreq();

    lfornd = lfopars->Prandomness / 127.0f;
    if (lfornd < 0.0f)
        lfornd = 0.0f;
    else if (lfornd > 1.0f)
        lfornd = 1.0f;

    // (orig comment) lfofreqrnd=pow(lfopars->Pfreqrand/127.0,2.0)*2.0*4.0;
    lfofreqrnd = powf(lfopars->Pfreqrand / 127.0f, 2.0f) * 4.0f;

    switch (lfopars->fel)
    {
        case 1:
            lfointensity = lfopars->Pintensity / 127.0f;
            break;

        case 2:
            lfointensity = lfopars->Pintensity / 127.0f * 4.0f;
            break; // in octave

        default:
            lfointensity = powf(2.0f, lfopars->Pintensity / 127.0f * 11.0f) - 1.0f; // in centi
            break;
    }

    lfotype = lfopars->PLFOtype;
    freqrndenabled = (lfopars->Pfreqrand != 0);
    computenextincrnd();
}

inline void LFO::RecomputeFreq(void)
{
    float lfostretch =
        powf(basefreq / 440.0f, (float)((int)lfopars->Pstretch - 64) / 63.0f); // max 2x/octave

    float lfofreq = (powf(2.0f, lfopars->Pfreq * 10.0f) - 1.0f) / 12.0f * lfostretch;
    incx = fabsf(lfofreq) * synth->buffersize_f / synth->samplerate_f;

    // Limit the Frequency (or else...)
    if (incx > 0.49999999f)
        incx = 0.49999999f;
}

// LFO out
float LFO::lfoout(void)
{
    if (lfopars->updated)
        Recompute();

    float out;
    switch (lfotype)
    {
        case 1: // LFO_TRIANGLE
            if (x >= 0.0f && x < 0.25f)
                out = 4.0f * x;
            else if (x > 0.25f && x < 0.75f)
                out = 2.0f - 4.0f * x;
            else
                out = 4.0f * x - 4.0f;
            break;

        case 2: // LFO_SQUARE
            if (x < 0.5f)
                out = -1.0f;
            else
                out = 1.0f;
            break;

        case 3: // LFO_RAMPUP
            out = (x - 0.5f) * 2.0f;
            break;

        case 4: // LFO_RAMPDOWN
            out = (0.5f - x) * 2.0f;
            break;

        case 5: // LFO_EXP_DOWN 1
            out = powf(0.05f, x) * 2.0f - 1.0f;
            break;

        case 6: // LFO_EXP_DOWN 2
            out = powf(0.001f, x) * 2.0f - 1.0f;
            break;

        default:
            out = cosf( x * TWOPI); // LFO_SINE
    }

    if (lfotype == 0 || lfotype == 1)
        out *= lfointensity * (amp1 + x * (amp2 - amp1));
    else
        out *= lfointensity * amp2;
    if (lfodelay < 0.00001f)
    {
        if (!freqrndenabled)
            x += incx;
        else
        {
            float tmp = (incrnd * (1.0f - x) + nextincrnd * x);
            tmp = (tmp > 1.0f) ? 1.0f : tmp;
            x += incx * tmp;
        }
        if (x >= 1)
        {
            x = fmodf(x, 1.0f);
            amp1 = amp2;
            amp2 = (1 - lfornd) + lfornd * synth->numRandom();
            computenextincrnd();
        }
    } else
        lfodelay -= synth->sent_all_buffersize_f / synth->samplerate_f;

    return out;
}


// LFO out (for amplitude)
float LFO::amplfoout(void)
{
    float out;
    out = 1.0f - lfointensity + lfoout();
    if (out < -1.0f)
        out = -1.0f;
    else if (out > 1.0f)
        out = 1.0f;
    return out;
}


void LFO::computenextincrnd(void)
{
    if(!freqrndenabled)
        return;
    incrnd = nextincrnd;
    nextincrnd = powf(0.5f, lfofreqrnd) + synth->numRandom() * (powf(2.0f, lfofreqrnd) - 1.0f);
}
