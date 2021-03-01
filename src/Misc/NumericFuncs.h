/*
    NumericFuncs.h

    Copyright 2010, Alan Calvert
    Copyright 2014-2021, Will Godfrey and others

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

#ifndef NUMERICFUNCS_H
#define NUMERICFUNCS_H


#include <cmath>
#include <cstddef>
#include "globals.h"

namespace func {


template<class T>
inline T limit(T val, T min, T max)
{ // currently doesn't seem to be used
    return val < min ? min : (val > max ? max : val);
}


inline void invSignal(float *sig, size_t len)
{ // only used by phaser effect
    for (size_t i = 0; i < len; ++i)
        sig[i] *= -1.0f;
}


inline float dB2rap(float dB) {
#if defined(HAVE_EXP10F)
    return exp10f((dB) / 20.0f);
#else
    return powf(10.0, (dB) / 20.0f);
#endif
}


inline float rap2dB(float rap)
{
    return 20.0f * log10f(rap);
}




// no more than 32 bit please!
inline unsigned int nearestPowerOf2(unsigned int x, unsigned int min, unsigned int max)
{
    if (x <= min)
        return min;
    if (x >= max)
        return max;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}



inline unsigned int bitFindHigh(unsigned int value)
{ // return the highest bit currently set
    if (value == 0)
        return 0xff;

    int bit = sizeof(unsigned int) * 8 - 1;
    while (!(value & (1 << bit)))
        --bit;

    return bit;
}


inline void bitSet(unsigned int& value, unsigned int bit)
{
    value |= (1 << bit);
}


inline void bitClear(unsigned int& value, unsigned int bit)
{
    unsigned int mask = -1;
    mask ^= (1 << bit);
    value &= mask;
}


inline void bitClearHigh(unsigned int& value)
{ // clear the current highest bit
    bitClear(value, bitFindHigh(value));
}


inline void bitClearAbove(unsigned int& value, int bitLevel)
{ // clear all bits above the designated one
    unsigned int mask = (0xffffffff << bitLevel);
    value = (value & ~mask);
}

inline bool bitTest(unsigned int value, unsigned int bit)
{
    if (value & (1 << bit))
        return true;
    return false;
}

inline void setRandomPan(float rand, float& left, float& right, unsigned char compensation, char pan, char range)
{
    float min = float (pan - range) / 126.0f;
    if (min < 0)
        min = 0;
    float max = float (pan + range) / 126.0f;;
    if (max > 1)
        max = 1;
    float t = rand * (max-min) + min;
    switch (compensation)
    {
        case MAIN::panningType::cut: // ZynAddSubFX - per side 0dB mono -6dB
            if (_SYS_::F2B(t))
            {
                right = 0.5f;
                left = (1.0f - t);
            }
            else
            {
                right = t;
                left = 0.5f;
            }
            break;
        case MAIN::panningType::normal: // Yoshimi - per side + 3dB mono -3dB
            left = cosf(t * HALFPI);
            right = sinf(t * HALFPI);
            break;
        case MAIN::panningType::boost: // boost - per side + 6dB mono 0dB
            left = (1.0 - t);
            right = t;
            break;
        default: // no panning
            left = 0.7;
            right = 0.7;
    }
}

inline void setAllPan(float position, float& left, float& right, unsigned char compensation)
{
    float t = ((position > 0) ? (position - 1) : 0.0f) / 126.0f;
    switch (compensation)
    {
        case MAIN::panningType::cut: // ZynAddSubFX - per side 0dB mono -6dB
            if (_SYS_::F2B(t))
            {
                right = 0.5f;
                left = (1.0f - t);
            }
            else
            {
                right = t;
                left = 0.5f;
            }
            break;
        case MAIN::panningType::normal: // Yoshimi - per side + 3dB mono -3dB
            left = cosf(t * HALFPI);
            right = sinf(t * HALFPI);
            break;
        case MAIN::panningType::boost: // boost - per side + 6dB mono 0dB
            left = (1.0 - t);
            right = t;
            break;
        default: // no panning
            left = 0.7;
            right = 0.7;
    }
}

inline float quantizedLFOfreqBPM(float value)
{
    // The quantizer below will need to be altered if this is ever
    // changed. Remember that intervals need to be preserved too, not just the
    // total number of steps, otherwise saved instruments will get incorrect
    // values.
    static_assert(LFO_BPM_STEPS == 33, "Need to adjust LFO_BPM_STEPS quantizer.");

    // We leave some room at the ends, so the full range is LFO_BPM_STEPS + 2.
    float tmp = roundf(value*(LFO_BPM_STEPS + 2));
    if (tmp < 1)
        tmp = 1;
    else if (tmp >= LFO_BPM_STEPS + 2)
        tmp = LFO_BPM_STEPS + 1;
    return tmp / (LFO_BPM_STEPS + 2);
}

// The reason we return this as a fraction instead of a straight float is that
// dividing by three is not possible to preserve perfectly in float, and this
// can add up to quite a lot of error over many beats.
inline std::pair<float, float> LFOfreqBPMFraction(float value)
{
    // The switch statement below will need to be altered if this is ever
    // changed. Remember that intervals need to be preserved too, not just the
    // total number of steps, otherwise saved instruments will get incorrect
    // values.
    static_assert(LFO_BPM_STEPS == 33, "Need to adjust LFO_BPM_STEPS table.");

    switch ((int)roundf(value * (LFO_BPM_STEPS + 2))) {
    case 0:
        // Some room to expand in the future. Fallthrough.
    case 1:
        return std::pair<float, float>(1, 16);
    case 2:
        return std::pair<float, float>(1, 15);
    case 3:
        return std::pair<float, float>(1, 14);
    case 4:
        return std::pair<float, float>(1, 13);
    case 5:
        return std::pair<float, float>(1, 12);
    case 6:
        return std::pair<float, float>(1, 11);
    case 7:
        return std::pair<float, float>(1, 10);
    case 8:
        return std::pair<float, float>(1, 9);
    case 9:
        return std::pair<float, float>(1, 8);
    case 10:
        return std::pair<float, float>(1, 7);
    case 11:
        return std::pair<float, float>(1, 6);
    case 12:
        return std::pair<float, float>(1, 5);
    case 13:
        return std::pair<float, float>(1, 4);
    case 14:
        return std::pair<float, float>(1, 3);
    case 15:
        return std::pair<float, float>(1, 2);
    case 16:
        return std::pair<float, float>(2, 3);
    case 17:
        return std::pair<float, float>(1, 1);
    case 18:
        return std::pair<float, float>(3, 2);
    case 19:
        return std::pair<float, float>(2, 1);
    case 20:
        return std::pair<float, float>(3, 1);
    case 21:
        return std::pair<float, float>(4, 1);
    case 22:
        return std::pair<float, float>(5, 1);
    case 23:
        return std::pair<float, float>(6, 1);
    case 24:
        return std::pair<float, float>(7, 1);
    case 25:
        return std::pair<float, float>(8, 1);
    case 26:
        return std::pair<float, float>(9, 1);
    case 27:
        return std::pair<float, float>(10, 1);
    case 28:
        return std::pair<float, float>(11, 1);
    case 29:
        return std::pair<float, float>(12, 1);
    case 30:
        return std::pair<float, float>(13, 1);
    case 31:
        return std::pair<float, float>(14, 1);
    case 32:
        return std::pair<float, float>(15, 1);
    case 34:
        // Some room to expand in the future. Fallthrough.
    case 33:
        return std::pair<float, float>(16, 1);
    default:
        return std::pair<float, float>(1, 1);
    }
}

// This conversion was written for CLI input.
// It may be useful elsewhere.
inline float BPMfractionLFOfreq(int num, int div)
{
    static_assert(LFO_BPM_STEPS == 33, "Need to adjust LFO_BPM_STEPS table.");

    int res = 0;

    // these checks could probably be improved!
    if (num < 1)
        num = 1;
    else if (num > 16)
        num = 16;
    if (div < 1)
        div = 1;
    else if (div > 16)
        div = 16;

    if (num == 3 && div == 2)
            res = 18;
    else if(num == 2 && div == 3)
        res = 16;
    else if (num == 1)
    {
        if (div == 1)
            res = 17;
        else
            res = 17 - div;
    }
    else if (div == 1)
        res = 17 + num;
    return (res / float(LFO_BPM_STEPS + 2));
}

}//(End)namespace func
#endif /*NUMERICFUNCS_H*/
