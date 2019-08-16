/*
    Envelope.cpp - Envelope implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011 Alan Calvert

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

    This file is a derivative of a ZynAddSubFX original, modified January 2011
*/

#include "Synth/Envelope.h"
#include "Misc/SynthEngine.h"
#include "Misc/NumericFuncs.h"
#include "Params/EnvelopeParams.h"

using func::dB2rap;
using func::rap2dB;


Envelope::Envelope(EnvelopeParams *envpars, float basefreq, SynthEngine *_synth):
    synth(_synth)
{
    envpoints = envpars->Penvpoints;
    if (envpoints > MAX_ENVELOPE_POINTS)
        envpoints = MAX_ENVELOPE_POINTS;
    envsustain = (envpars->Penvsustain == 0) ? -1 : envpars->Penvsustain;
    forcedrelase = envpars->Pforcedrelease;
    envstretch = powf(440.0f / basefreq, envpars->Penvstretch / 64.0f);
    linearenvelope = envpars->Plinearenvelope;

    if (!envpars->Pfreemode)
        envpars->converttofree();

    float bufferdt = synth->sent_all_buffersize_f / synth->samplerate_f;

    int mode = envpars->Envmode;

    // for amplitude envelopes
    if (mode == ENVMODE::amplitudeLin && linearenvelope == 0)
        mode = ENVMODE::amplitudeLog; // change to log envelope
    if (mode == ENVMODE::amplitudeLog && linearenvelope != 0)
        mode = ENVMODE::amplitudeLin; // change to linear

    for (int i = 0; i < MAX_ENVELOPE_POINTS; ++i)
    {
        float tmp = envpars->getdt(i) / 1000.0f * envstretch;
        if (tmp > bufferdt)
            envdt[i] = bufferdt / tmp;
        else
            envdt[i] = 2.0f; // any value larger than 1

        switch (mode)
        {
            case 2:
                envval[i] = (1.0f - envpars->Penvval[i] / 127.0f) * MIN_ENVELOPE_DB;
                break;

            case 3:
                envval[i] =
                    (powf(2.0f, 6.0f * fabsf(envpars->Penvval[i] - 64.0f) / 64.0f) - 1.0f) * 100.0f;
                if (envpars->Penvval[i] < 64)
                    envval[i] = -envval[i];
                break;

            case 4:
                envval[i] = (envpars->Penvval[i] - 64.0f) / 64.0f * 6.0f; // 6 octaves (filtru)
                break;

            case 5:
                envval[i] = (envpars->Penvval[i] - 64.0f) / 64.0f * 10.0f;
                break;

            default:
                envval[i] = envpars->Penvval[i] / 127.0f;
        }
    }

    envdt[0] = 1.0f;

    currentpoint = 1; // the envelope starts from 1
    keyreleased = 0;
    t = 0.0f;
    envfinish = 0;
    inct = envdt[1];
    envoutval = 0.0f;
}


// Release the key (note envelope)
void Envelope::releasekey(void)
{
    if (keyreleased == 1)
        return;
    keyreleased = 1;
    if (forcedrelase != 0)
        t = 0.0f;
}


// Envelope Output
float Envelope::envout(void)
{
    float out;
    if (envfinish)
    {   // if the envelope is finished
        envoutval = envval[envpoints - 1];
        return envoutval;
    }
    if (currentpoint == envsustain + 1 && !keyreleased)
    {   // if it is sustaining now
        envoutval = envval[envsustain];
        return envoutval;
    }

    if (keyreleased && forcedrelase)
    {   // do the forced release
        int tmp = (envsustain < 0) ? (envpoints - 1) : (envsustain + 1);
        // if there is no sustain point, use the last point for release

        if (envdt[tmp] <0.00000001f)
            out = envval[tmp];
        else
            out = envoutval + (envval[tmp] - envoutval) * t;
        t += envdt[tmp] * envstretch;

        if (t >= 1.0f)
        {
            currentpoint = envsustain + 2;
            forcedrelase = 0;
            t = 0.0f;
            inct = envdt[currentpoint];
            if (currentpoint >= envpoints || envsustain < 0)
                envfinish = 1;
        }
        return out;
    }
    if (inct >= 1.0f)
        out = envval[currentpoint];
    else
        out = envval[currentpoint - 1] + (envval[currentpoint]
              - envval[currentpoint - 1]) * t;

    t += inct;
    if (t >= 1.0f)
    {
        if (currentpoint >= envpoints - 1)
            envfinish = 1;
        else
            currentpoint++;
        t = 0.0f;
        inct = envdt[currentpoint];
    }

    envoutval = out;
    return out;
}


// Envelope Output (dB)
float Envelope::envout_dB(void)
{
    float out;
    if (linearenvelope != 0)
        return envout();

    if (currentpoint == 1 && (keyreleased == 0 || forcedrelase == 0))
    {   // first point is always lineary interpolated
        float v1 = dB2rap(envval[0]);
        float v2 = dB2rap(envval[1]);
        out = v1 + (v2 - v1) * t;

        t += inct;
        if (t >= 1.0f)
        {
            t = 0.0f;
            inct = envdt[2];
            currentpoint++;
            out = v2;
        }

        if (out > 0.001f)
            envoutval = rap2dB(out);
        else
            envoutval = MIN_ENVELOPE_DB;
    } else
        out = dB2rap(envout());

    return out;
}
