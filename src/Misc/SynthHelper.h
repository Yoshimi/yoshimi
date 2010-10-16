/*
    SynthHelper.h

    Copyright 2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SYNTHHELPER_H
#define SYNTHHELPER_H

#include <cmath>

class SynthHelper {
    public:
        SynthHelper() {};
        ~SynthHelper() {};

        bool aboveAmplitudeThreshold(float a, float b);
        float interpolateAmplitude(float a, float b, int x, int size);
        float velF(float velocity, unsigned char scaling);
        float getDetune(unsigned char type, unsigned short int coarsedetune,
                        unsigned short int finedetune);
};

inline bool SynthHelper::aboveAmplitudeThreshold(float a, float b)
{
    return ((2.0 * fabsf(b - a) / fabsf(b + a + 0.0000000001f)) > 0.0001f);
}

inline float SynthHelper::interpolateAmplitude(float a, float b, int x, int size)
{
    return a + (b - a) * (float)x / (float)size;
}


inline float SynthHelper::velF(float velocity, unsigned char scaling)
{
    float x = powf(VELOCITY_MAX_SCALE, (64.0f - scaling) / 64.0f);
    if (scaling == 127 || velocity > 0.99f)
        return 1.0f;
    else
        return powf(velocity, x);
}


// Get the detune in cents
inline float SynthHelper::getDetune(unsigned char type, unsigned short int coarsedetune,
                                    unsigned short int finedetune)
{
    float det = 0.0f;
    float octdet = 0.0f;
    float cdet = 0.0f;
    float findet = 0.0f;

    // Get Octave
    int octave = coarsedetune / 1024;
    if (octave >= 8)
        octave -= 16;
    octdet = octave * 1200.0f;

    // Coarse and fine detune
    int cdetune = coarsedetune % 1024;
    if (cdetune > 512)
        cdetune -= 1024;

    int fdetune = finedetune - 8192;

    switch (type)
    {
    //	case 1: is used for the default (see below)
        case 2:
            cdet = fabsf(cdetune * 10.0f);
            findet = fabsf(fdetune / 8192.0f) * 10.0f;
            break;
        case 3:
            cdet = fabsf(cdetune * 100.0f);
            findet = powf(10.0f, fabsf(fdetune / 8192.0f) * 3.0f) / 10.0f - 0.1f;
            break;
        case 4:
            cdet = fabsf(cdetune * 701.95500087f); // perfect fifth
            findet = (powf(2.0f, fabsf(fdetune / 8192.0f) * 12.0f) - 1.0f) / 4095.0f * 1200.0f;
            break;
            // case ...: need to update N_DETUNE_TYPES, if you'll add more
        default:
            cdet = fabsf(cdetune * 50.0f);
            findet = fabsf(fdetune / 8192.0f) * 35.0f; // almost like "Paul's Sound Designer 2"
            break;
    }
    if (finedetune < 8192u)
        findet = -findet;
    if (cdetune < 0)
        cdet = -cdet;

    det = octdet + cdet + findet;
    return det;
}

#endif
