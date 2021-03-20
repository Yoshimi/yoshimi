/*
    LFO.cpp - LFO implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019, Will Godfrey & others
    Copyright 2020-2021 Kristian Amlie & others

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

*/

#include <cmath>

#include "Misc/SynthEngine.h"
#include "Synth/LFO.h"


LFO::LFO(LFOParams *_lfopars, float _basefreq, SynthEngine *_synth):
    lfopars(_lfopars),
    lfoUpdate(lfopars),
    basefreq(_basefreq),
    sampandholdvalue(0.0f),
    issampled(0), // initialized to 0 for correct startup
    synth(_synth)
{
    if (lfopars->Pstretch == 0)
        lfopars->Pstretch = 1;

    RecomputeFreq(); // need incx early

    if (lfopars->Pcontinous == 0)
    { // pre-init phase
        if (lfopars->Pstartphase == 0)
            startPhase = synth->numRandom();
        else
            startPhase = fmodf(((float)((int)lfopars->Pstartphase - 64) / 127.0f + 1.0f), 1.0f);

        if (lfopars->Pbpm != 0)
        {
            prevMonotonicBeat = synth->getMonotonicBeat();
            prevBpmFrac = getBpmFrac();
            startPhase = remainderf(startPhase - prevMonotonicBeat
                                    * prevBpmFrac.first / prevBpmFrac.second, 1.0f);
        }
    }
    else if (lfopars->Pbpm == 0)
    { // pre-init phase, synced to other notes
        startPhase = fmodf(synth->getLFOtime() * incx, 1.0f);
        startPhase = fmodf((((int)lfopars->Pstartphase - 64) / 127.0f + 1.0f + startPhase), 1.0f);
    }
    else // Pcontinous == 1 && Pbpm == 1.
        startPhase = fmodf((((int)lfopars->Pstartphase - 64) / 127.0f + 1.0f), 1.0f);

    x = startPhase;

    lfoelapsed = 0.0f;
    incrnd = nextincrnd = 1.0f;

    Recompute();
    if (lfopars->fel == 0) // this is a Frequency LFO
        x -= 0.25f; // change the starting phase
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

    if (lfopars->Pcontinous != 0 && lfopars->Pbpm != 0)
        // When we are BPM synced to the host, it's nice to have direct feedback
        // when changing phase. This works because we reset the phase completely
        // on every cycle.
        startPhase = fmodf((((int)lfopars->Pstartphase - 64) / 127.0f + 1.0f), 1.0f);
}

inline void LFO::RecomputeFreq(void)
{
    float lfostretch =
        powf(basefreq / 440.0f, (float)((int)lfopars->Pstretch - 64) / 63.0f); // max 2x/octave

    float lfofreq = lfopars->Pfreq * lfostretch;
    incx = fabsf(lfofreq) / synth->samplerate_f;
}

// LFO out
float LFO::lfoout()
{
    if (lfoUpdate.checkUpdated())
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

        case 7: // LFO_SAMPLE_&_HOLD
            if (x < 0.5f)
            {
                if (issampled == 0)
                {
                    issampled = 1;
                    sampandholdvalue = synth->numRandom();
                    //out = (sampandholdvalue - 0.5f) * 2.0f;
                }
                out = (sampandholdvalue - 0.5f) * 2.0f;
            }
            else
            {
                if (issampled == 1)
                {
                    issampled = 0;
                    sampandholdvalue = synth->numRandom();
                    //out = (sampandholdvalue - 0.5f) * 2.0f;
                }
                out = (sampandholdvalue - 0.5f) * 2.0f;
            }
            break;

        case 8: // LFO_RANDOM_SQUARE_UP
            if (x < 0.5f)
            {
                if (issampled == 1)
                    issampled = 0;
                out = -1.0f;
            }
            else
            {
                if (issampled == 0)
                {
                    issampled = 1;
                    sampandholdvalue = synth->numRandom();
                }
                out = sampandholdvalue;
            }
            break;

        case 9: // LFO_RANDOM_SQUARE_DOWN
            if (x < 0.5f)
            {
                if (issampled == 1)
                    issampled = 0;
                out = 1.0f;
            }
            else
            {
                if (issampled == 0)
                {
                    issampled = 1;
                    sampandholdvalue = synth->numRandom();
                }
                out = sampandholdvalue - 1.0f;
            }
            break;

        default:
            out = cosf(x * TWOPI); // LFO_SINE
    }

    if (lfotype == 0 || lfotype == 1)
        out *= lfointensity * (amp1 + x * (amp2 - amp1));
    else
        out *= lfointensity * amp2;

    float lfodelay = lfopars->Pdelay / 127.0f * 4.0f; // 0..4 sec
    if (lfoelapsed >= lfodelay)
    {
        float oldx = x;
        if (lfopars->Pbpm == 0)
        {
            float incxMult = incx * synth->sent_buffersize_f;
            // Limit the Frequency (or else...)
            if (incxMult > 0.49999999f)
                incxMult = 0.49999999f;

            if (!freqrndenabled)
                x += incxMult;
            else
            {
                float tmp = (incrnd * (1.0f - x) + nextincrnd * x);
                tmp = (tmp > 1.0f) ? 1.0f : tmp;
                x += incxMult * tmp;
            }
            x = fmodf(x, 1.0f);
        }
        else
        {
            std::pair<float, float> frac = getBpmFrac();
            float newBeat;
            if (lfopars->Pcontinous == 0)
            {
                if (frac != prevBpmFrac)
                {
                    // Since we reset the phase on every cycle, if the BPM
                    // fraction changes we need to adapt startPhase or we will
                    // get an abrupt phase change.
                    startPhase = remainderf(x - prevMonotonicBeat * frac.first / frac.second, 1.0f);
                    prevBpmFrac = frac;
                }
                newBeat = synth->getMonotonicBeat();
                prevMonotonicBeat = newBeat;
            }
            else
                newBeat = synth->getSongBeat();
            x = fmodf(newBeat * frac.first / frac.second + startPhase, 1.0f);
        }

        if (oldx >= 0.5f && x < 0.5f)
        {
            amp1 = amp2;
            amp2 = (1 - lfornd) + lfornd * synth->numRandom();
            computenextincrnd();
        }
    } else
        lfoelapsed += synth->sent_buffersize_f / synth->samplerate_f;

    return out;
}


// LFO out (for amplitude)
float LFO::amplfoout()
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
    if (!freqrndenabled)
        return;
    incrnd = nextincrnd;
    nextincrnd = powf(0.5f, lfofreqrnd) + synth->numRandom() * (powf(2.0f, lfofreqrnd) - 1.0f);
}
