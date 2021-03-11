/*
    SynthHelper.h

    Copyright 2011, Alan Calvert
    Copyright 2021, Kristian Amlie

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

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

#define DEFAULT_PARAM_INTERPOLATION_LENGTH_MSECS 50.0f

namespace synth {


// Provides a convenient way to interpolate between samples. You provide a
// starting value, and each time you provide new value, it will start
// interpolating between the values. It takes into account new values that
// appear while an interpolation is happening.
template <typename T>
class InterpolatedValue
{
    public:
        InterpolatedValue(T startValue, int sampleRate_)
            : oldValue(startValue)
            , newValue(startValue)
            , targetValue(startValue)
            , sampleRate(sampleRate_)
        {
            setInterpolationLength(DEFAULT_PARAM_INTERPOLATION_LENGTH_MSECS);
        }

        void setInterpolationLength(float msecs)
        {
            float samples = msecs / 1000.0f * sampleRate;
            // Round up so we are as smooth as possible.
            interpolationLength = ceilf(samples);
            interpolationPos = interpolationLength;
        }

        bool isInterpolating() const
        {
            return interpolationPos < interpolationLength;
        }

        void setTargetValue(T value)
        {
            targetValue = value;
            if (!isInterpolating() && targetValue != newValue)
            {
                newValue = value;
                interpolationPos = 0;
            }
        }

       // The value interpolated from.
        T getOldValue() const
        {
            return oldValue;
        }

       // The value interpolated to (not necessarily the same as the last set
       // target point).
        T getNewValue() const
        {
            return newValue;
        }

        T getTargetValue() const
        {
            return targetValue;
        }

        float factor() const
        {
            return (float)interpolationPos / (float)interpolationLength;
        }

        T getValue() const
        {
            return getOldValue() * (1.0f - factor()) + getNewValue() * factor();
        }

        T getAndAdvanceValue()
        {
            T v = getValue();
            advanceValue();
            return v;
        }

        void advanceValue()
        {
            if (interpolationPos >= interpolationLength)
                return;

            if (++interpolationPos < interpolationLength)
                return;

            oldValue = newValue;
            if (targetValue != newValue)
            {
                newValue = targetValue;
                interpolationPos = 0;
            }
        }

        void advanceValue(int samples)
        {
            if (interpolationPos + samples < interpolationLength)
                interpolationPos += samples;
            else if (interpolationPos + samples >= interpolationLength * 2)
            {
                oldValue = newValue = targetValue;
                interpolationPos = interpolationLength;
            }
            else
            {
                oldValue = newValue;
                newValue = targetValue;
                interpolationPos += samples - interpolationLength;
            }
        }

    private:
        T oldValue;
        T newValue;
        T targetValue;

        int interpolationLength;
        int interpolationPos;

    protected:
        int sampleRate;
};

// Default-initialized variant of InterpolatedValue. Use only if you must (for
// example in an array), normally it is better to use the fully initialized
// one. If you use this, then you should call setSampleRate immediately after
// construction.
template <typename T>
class InterpolatedValueDfl : public InterpolatedValue<T>
{
    public:
        InterpolatedValueDfl()
            : InterpolatedValue<T>(0, 44100)
        {
        }

        void setSampleRate(int sampleRate)
        {
            this->sampleRate = sampleRate;
            this->setInterpolationLength(DEFAULT_PARAM_INTERPOLATION_LENGTH_MSECS);
        }
};


inline bool aboveAmplitudeThreshold(float a, float b)
{
    return ((2.0f * fabsf(b - a) / fabsf(b + a + 0.0000000001f)) > 0.0001f);
}


inline float interpolateAmplitude(float a, float b, int x, int size)
{
    return a + (b - a) * (float)x / (float)size;
}


inline float velF(float velocity, unsigned char scaling)
{
    if (scaling == 127 || velocity > 0.99f)
        return 1.0f;
    else
        return powf(velocity, (powf(8.0f, (64.0f - (float)scaling) / 64.0f)));
}



inline float getDetune(unsigned char type,
                       unsigned short int coarsedetune,
                       unsigned short int finedetune)
{
    float det = 0.0f;
    float octdet = 0.0f;
    float cdet = 0.0f;
    float findet = 0.0f;
    int octave = coarsedetune / 1024; // get Octave

    if (octave >= 8)
        octave -= 16;
    octdet = octave * 1200.0f;

    int cdetune = coarsedetune % 1024; // coarse and fine detune
    if (cdetune > 512)
        cdetune -= 1024;
    int fdetune = finedetune - 8192;

    switch (type)
    {
        // case 1 is used for the default (see below)
        case 2:
            cdet = fabs(cdetune * 10.0f);
            findet = fabs(fdetune / 8192.0f) * 10.0f;
            break;

        case 3:
            cdet = fabsf(cdetune * 100.0f);
            findet = powf(10.0f, fabs(fdetune / 8192.0f) * 3.0f) / 10.0f - 0.1f;
            break;

        case 4:
            cdet = fabs(cdetune * 701.95500087f); // perfect fifth
            findet = (powf(2.0f, fabs(fdetune / 8192.0f) * 12.0f) - 1.0f) / 4095.0f * 1200.0f;
            break;

            // case ...: need to update N_DETUNE_TYPES, if you'll add more
        default:
            cdet = fabs(cdetune * 50.0f);
            findet = fabs(fdetune / 8192.0f) * 35.0f; // almost like "Paul's Sound Designer 2"
            break;
    }
    if (finedetune < 8192)
        findet = -findet;
    if (cdetune < 0)
        cdet = -cdet;
    det = octdet + cdet + findet;
    return det;
}


}//(End)namespace synth
#endif /*SYNTHHELPER_H*/
