/*
    NumericFuncs.h

    Copyright 2010, Alan Calvert
    Copyright 2014-2019, Will Godfrey

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

    Modified August 2019
*/

#ifndef NUMERICFUNCS_H
#define NUMERICFUNCS_H

#include <cmath>
#include <cstddef>


namespace func {


template<class T>
inline T limit(T val, T min, T max)
{
    return val < min ? min : (val > max ? max : val);
}


inline void invSignal(float *sig, size_t len)
{
    for(size_t i = 0; i < len; ++i)
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
{
    int bit = 32;
    while (bit >= 0)
    {
        bit --;
        if ((value >> bit) == 1)
            return bit;
    }
    return 0xff;
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
{
    bitClear(value, bitFindHigh(value));
}


inline void bitClearAbove(unsigned int& value, int bitLevel)
{
    unsigned int mask = (0xffffffff << bitLevel);
    value = (value & ~mask);
}

inline bool bitTest(unsigned int value, unsigned int bit)
{
    if (value & (1 << bit))
        return true;
    return false;
}


}//(End)namespace func
#endif /*NUMERICFUNCS_H*/
