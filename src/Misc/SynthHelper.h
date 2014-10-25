/*
    SynthHelper.h

    Copyright 2011, Alan Calvert

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
        SynthHelper() {}
        ~SynthHelper() {}

        bool aboveAmplitudeThreshold(float a, float b);
        float interpolateAmplitude(float a, float b, int x, int size);
        float velF(float velocity, unsigned char scaling);
        float getDetune(unsigned char type, unsigned short int coarsedetune,
                        unsigned short int finedetune) const;
};

inline bool SynthHelper::aboveAmplitudeThreshold(float a, float b)
{
    return ((2.0f * fabsf(b - a) / fabsf(b + a + 0.0000000001f)) > 0.0001f);
}

inline float SynthHelper::interpolateAmplitude(float a, float b, int x, int size)
{
    return a + (b - a) * (float)x / (float)size;
}


inline float SynthHelper::velF(float velocity, unsigned char scaling)
{
    if (scaling == 127 || velocity > 0.99f)
        return 1.0f;
    else
        return powf(velocity, (powf(8.0f, (64.0f - (float)scaling) / 64.0f)));
}

// -----------------------------


#endif
