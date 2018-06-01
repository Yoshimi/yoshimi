/*
    WaveShapeSamples.h - "AlienWah" effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified October 2010
*/

#ifndef WAVESHAPESAMPLES_H
#define WAVESHAPESAMPLES_H

#include <cmath>

class WaveShapeSamples
{
    public:
        WaveShapeSamples() { }
        ~WaveShapeSamples() { }
        void waveShapeSmps(int n, float *smps, unsigned char type, unsigned char drive);
};

// Waveshape, used by OscilGen::waveshape and Distorsion::process
inline void WaveShapeSamples::waveShapeSmps(int n, float *smps, unsigned char type, unsigned char drive)
{
    int i;
    float ws = drive / 127.0f;
    float tmpv;

    switch (type)
    {
        case 1:
            ws = powf( 10.0f, ws * ws * 3.0f) - 1.0f + 0.001f; // Arctangent
            for (i = 0; i < n; ++i)
                smps[i] = atanf(smps[i] * ws) / atanf(ws);
            break;
        case 2:
            ws = ws * ws * 32.0f + 0.0001f; // Asymmetric
            tmpv = (ws < 1.0f) ? sinf(ws) + 0.1f : 1.1f;
            for (i = 0; i < n; ++i)
                smps[i] = sinf(smps[i] * (0.1f + ws - ws * smps[i])) / tmpv;
            break;
        case 3:
            ws = ws * ws * ws * 20.0f + 0.0001f; // Pow
            for (i = 0; i < n; ++i)
            {
                smps[i] *= ws;
                if (fabsf(smps[i]) < 1.0f)
                {
                    smps[i] = (smps[i] - powf(smps[i], 3.0f)) * 3.0f;
                    if (ws < 1.0f)
                        smps[i] /= ws;
                } else
                    smps[i] = 0.0f;
            }
            break;
        case 4:
            ws = ws * ws * ws * 32.0f + 0.0001f; // Sine
            tmpv = (ws < 1.57f) ? sinf(ws) : 1.0f;
            for (i = 0; i < n; ++i)
                smps[i] = sinf(smps[i] * ws) / tmpv;
            break;
        case 5:
            ws = ws * ws + 0.000001f; // Quantisize
            for (i = 0; i < n; ++i)
                smps[i] = floorf(smps[i] / ws + 0.5f) * ws;
            break;
        case 6:
            ws = ws * ws * ws * 32.0f + 0.0001f; // Zigzag
            tmpv = (ws < 1.0f) ? sinf(ws) : 1.0f;
            for (i = 0; i < n; ++i)
                smps[i] = asinf(sinf(smps[i] * ws)) / tmpv;
            break;
        case 7:
            ws = powf(2.0f, -ws * ws * 8.0f); // Limiter
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i];
                if (fabsf(tmp) > ws)
                {
                    smps[i] = (tmp >= 0.0f) ? 1.0f : -1.0f;
                }
                else
                    smps[i] /= ws;
            }
            break;
        case 8:
            ws = powf(2.0f, -ws * ws * 8.0f); // Upper Limiter
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i];
                if (tmp > ws)
                    smps[i] = ws;
                smps[i] *= 2.0f;
            }
            break;
        case 9:
            ws = powf(2.0f, -ws * ws * 8.0f); // Lower Limiter
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i];
                if (tmp < -ws)
                    smps[i] = -ws;
                smps[i] *= 2.0f;
            }
            break;
        case 10:
            ws = (powf(2.0f, ws * 6.0f) - 1.0f) / powf(2.0f, 6.0f); // Inverse Limiter
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i];
                if (fabsf(tmp) > ws)
                {
                    smps[i] = (tmp >= 0.0f) ? (tmp - ws) : (tmp + ws);
                }
                else
                    smps[i] = 0.0f;
            }
            break;
        case 11:
            ws = powf(5.0f, ws * ws * 1.0f) - 1.0f; // Clip
            for (i = 0; i < n; ++i)
                smps[i] = smps[i] * (ws + 0.5f) * 0.9999f - floorf(0.5f + smps[i] * (ws + 0.5f) * 0.9999f);
            break;
        case 12:
            ws = ws * ws * ws * 30.0f + 0.001f; // Asym2
            tmpv = (ws < 0.3f) ? ws : 1.0f;
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i] * ws;
                if (tmp > -2.0f && tmp < 1.0f)
                    smps[i] = tmp * (1.0f - tmp) * (tmp + 2.0f) / tmpv;
                else
                    smps[i] = 0.0f;
            }
            break;
        case 13:
            ws = ws * ws * ws * 32.0f + 0.0001f; // Pow2
            tmpv = (ws < 1.0f) ? (ws * (1.0f + ws) / 2.0f) : 1.0f;
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i] * ws;
                if (tmp >- 1.0 && tmp < 1.618034f)
                    smps[i] = tmp * (1.0f - tmp) / tmpv;
                else if (tmp > 0.0f)
                    smps[i] = -1.0f;
                else
                    smps[i] = -2.0f;
            }
            break;
        case 14:
            ws = powf(ws, 5.0f) * 80.0f + 0.0001f; // sigmoid
            tmpv = (ws > 10.0f) ? 0.5f : 0.5f - 1.0f / (expf(ws) + 1.0f);
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i] * ws;
                if (tmp < -10.0f)
                    tmp = -10.0f;
                else if (tmp > 10.0f)
                    tmp = 10.0f;
                tmp = 0.5f - 1.0f / (expf(tmp) + 1.0f);
                smps[i] = tmp / tmpv;
            }
            break;
        // todo update to Distorsion::changepar (Ptype max) if there is added
        // more waveshapings functions
    }
}

#endif
