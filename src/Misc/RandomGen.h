/*
    RandomGen.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2018, Will Godfrey & others

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

    Created by refactoring (from SynthEngine.h) December 2018
*/

#ifndef RANDOMGEN_H
#define RANDOMGEN_H

#include <cstdint>
#include <cstdlib>
#include <cstring>

using std::int32_t;
using std::uint32_t;
using std::memset;


// stub implementation for test code : always returns fixed values
class NorandomPRNG
{
    public:
        bool init(uint32_t) { return true; }
        uint32_t prngval()  { return INT_MAX / 2; }
        float numRandom()   { return 0.5f; }
        uint32_t randomINT(){ return INT_MAX / 2; }  // 0 < randomINT() < INT_MAX
};


// Standard implementation for Yoshimi until 1.5.10
// Relies on the random_r() family of functions from the C standard library, which generates 31 bit random numbers.
// Using 256 bytes of random state, which (according to the formula given in the comment in random_r.c of Glibc)
// gives a period length of at least deg*(2^deg - 1); with deg=63 this is > 5.8e20
class StdlibPRNG
{
        char random_state[256];
        struct random_data random_buf;

    public:
        StdlibPRNG()
        {
            memset(&random_state, 0, sizeof(random_state));
        }

        bool init(uint32_t seed)
        {
            memset(random_state, 0, sizeof(random_state));
            memset(&random_buf, 0, sizeof(random_buf));
            return 0 == initstate_r(seed, random_state, sizeof(random_state), &random_buf);
        }

        uint32_t prngval()
        {
            int32_t random_result;
            random_r(&random_buf, &random_result);
            // can not fail, since &random_buf can not be NULL
            // random_result holds number 0...INT_MAX
            return random_result;
        }

        float numRandom()
        {
            float random_0_1 = float(prngval()) / (float)INT_MAX;
            random_0_1 = (random_0_1 > 1.0f) ? 1.0f : random_0_1;
            random_0_1 = (random_0_1 < 0.0f) ? 0.0f : random_0_1;
            return random_0_1;
        }

        // random number in the range 0...INT_MAX
        uint32_t randomINT()
        {
            return prngval();
        }
};


// Fallback implementation for systems without a random_r() implementation
// uses the legacy random() / srandom() functions
class LegacyPRNG
{
        char random_state[256];

    public:
        LegacyPRNG()
        {
            memset(&random_state, 0, sizeof(random_state));
        }

        bool init(uint32_t seed)
        {
            memset(random_state, 0, sizeof(random_state));
            return NULL != initstate(seed, random_state, sizeof(random_state));
        }

        uint32_t prngval()
        {
            return uint32_t(random());
        }

        float numRandom()
        {
            float random_0_1 = float(prngval()) / (float)INT_MAX;
            random_0_1 = (random_0_1 > 1.0f) ? 1.0f : random_0_1;
            random_0_1 = (random_0_1 < 0.0f) ? 0.0f : random_0_1;
            return random_0_1;
        }

        // random number in the range 0...INT_MAX
        uint32_t randomINT()
        {
            return prngval();
        }
};



// Pseudo Random Number generator based on jsf32
// by Bob Jenkins "A small noncryptographic PRNG", October 2007
// http://burtleburtle.net/bob/rand/smallprng.html
// Runs fast and generates 32bit random numbers of high quality; although there is no guaranteed
// minimum cycle length, practical tests yielded 2^47 numbers (128 TiB) until repetition.
class JenkinsPRNG
{
        // 128 bit state
        uint32_t a, b, c, d;

    public:
        JenkinsPRNG() : a(0),b(0),c(0),d(0) { }

        bool init(uint32_t seed)
        {
            a = 0xf1ea5eed;
            b = c = d = seed;
            for (int i = 0; i < 20; ++i)
                (void)prngval();
            return true;
        }

        uint32_t prngval()
        {
            uint32_t e = a - ((b << 27) | (b >> 5));
            a = b ^ ((c << 17) | (c >> 15));
            b = c + d;
            c = d + e;
            d = e + a;
            return d;
        }

        float numRandom()
        {
            return float(prngval() >> 1) / float(INT_MAX);
        }

        // random number in the range 0...INT_MAX
        uint32_t randomINT()
        {
            return prngval() >> 1;
        }
};





/* ===== Configure the actual PRNG to use ===== */


#ifdef NORANDOM
    typedef NorandomPRNG  RandomGen;
#else

//#if (HAVE_RANDOM_R)
//    typedef StdlibPRNG  RandomGen;
//#else
//    typedef LegacyPRNG  RandomGen;
//#endif /*HAVE_RANDOM_R*/

      typedef JenkinsPRNG RandomGen;

#endif /*NORANDOM*/



#endif /*RANDOMGEN_H*/
