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

#include <sys/types.h>
#include <stdint.h>
#include <cstdlib>
#include <cstring>

using std::memset;


// stub implementation for test code : always returns fixed values
class NorandomPRNG
{
    public:
        void init(uint32_t) { }
        uint32_t prngval()  { return INT32_MAX / 2; }
        float numRandom()   { return 0.5f; }
        uint32_t randomINT(){ return INT32_MAX / 2; }  // 0 < randomINT() < INT_MAX
};



// Inlined copy of the Glibc 2.28 implementation of random_r()
// This code behaves equivalent to Yoshimi versions (< 1.5.10) linked against GNU Glibc-2.24.
// Generates 31bit random numbers based on a linear feedback shift register approach, employing trinomials.
// Using 256 bytes of random state, which (according to the formula given in the comment in random_r.c of Glibc)
// gives a period length of at least deg*(2^deg - 1); with deg=63 this is > 5.8e20
//
// The following PRNG implementation is
// Copyright (C) 1995-2018 Free Software Foundation, Inc.
// It was released within GLibc under the LGPL 2.1 or any later version.
// Based on code Copyright (C) 1983 Regents of the University of California.
// This code was derived from the Berkeley source:
// @(#)random.c    5.5 (Berkeley) 7/6/88
// It was reworked for the GNU C Library by Roland McGrath.
// Rewritten to be reentrant by Ulrich Drepper, 1995

class TrinomialPRNG
{
        uint32_t state[63];
        uint32_t *fptr;      /* Front pointer.  */
        uint32_t *rptr;      /* Rear pointer.  */

    public:
        TrinomialPRNG() : fptr(NULL), rptr(NULL) { }

        void init(uint32_t seed)
        {
            int kc = 63; /* random generation uses this trinomial: x**63 + x + 1.  */

            /* We must make sure the seed is not 0.  Take arbitrarily 1 in this case.  */
            if (seed == 0)
              seed = 1;
            state[0] = seed;

            uint32_t *dst = state;
            int32_t word = seed;  // must be signed, see below
            for (int i = 1; i < kc; ++i)
            {
                /* This does:
                   state[i] = (16807 * state[i - 1]) % 2147483647;
                   but avoids overflowing 31 bits. */
                // Ichthyo 12/2018 : the above comment is only true for seed <= INT_MAX
                //                   For INT_MAX < seed <= UINT_MAX the calculation diverges from correct
                //                   modulus result, however, its values show a similar distribution pattern.
                //                   Moreover the original code used long int for 'hi' and 'lo'.
                //                   It behaves identical when using uint32_t, but not with int32_t
                uint32_t hi = word / 127773;
                uint32_t lo = word % 127773;
                word = 16807 * lo - 2836 * hi;
                if (word < 0)
                    word += 2147483647;
                *++dst = word;
            }

            fptr = &state[1];
            rptr = &state[0];
            kc *= 10;
            while (--kc >= 0)
                prngval();
        }


        uint32_t prngval()
        {
            uint32_t val = *fptr += *rptr;
            uint32_t result = val >> 1;  // Chucking least random bit.
                                         // Rationale: it has a less-then optimal repetition cycle.
            uint32_t *end = &state[63];
            ++fptr;
            if (fptr >= end)
              {
                fptr = state;
                ++rptr;
              }
            else
              {
                ++rptr;
                if (rptr >= end)
                  rptr = state;
              }
            // random_result holds number 0...INT_MAX
            return result;
        }


        float numRandom()
        {
            return prngval() / float(INT32_MAX);
        }

        // random number in the range 0...INT_MAX
        uint32_t randomINT()
        {
            return prngval();
        }
};



// Pseudo Random Number generator based on jsf32 by Bob Jenkins
// "A small noncryptographic PRNG", October 2007
// http://burtleburtle.net/bob/rand/smallprng.html
// Runs fast and generates 32bit random numbers of high quality; although there is no guaranteed
// minimum cycle length, practical tests yielded 2^47 numbers (128 TiB) until repetition.
// We literally use the original Implementation, released by Jenkins 10/2007 into public domain.
class JenkinsPRNG
{
        // 128 bit state
        uint32_t a, b, c, d;

    public:
        JenkinsPRNG() : a(0),b(0),c(0),d(0) { }

        void init(uint32_t seed)
        {
            a = 0xf1ea5eed;
            b = c = d = seed;
            for (int i = 0; i < 20; ++i)
                prngval();
        }

        uint32_t prngval()
        {
            uint32_t e = a - rot(b, 27);
            a = b ^ rot(c, 17);
            b = c + d;
            c = d + e;
            d = e + a;
            return d;
        }

        float numRandom()
        {
            return float(prngval() >> 1) / float(INT32_MAX);
        }

        // random number in the range 0...INT_MAX
        uint32_t randomINT()
        {
            return prngval() >> 1;
        }

    private:
        uint32_t rot(uint32_t x, uint32_t k) { return (x << k)|(x >> (32-k)); }
};





/* ===== Configure the actual PRNG to use ===== */


#ifdef NORANDOM
    typedef NorandomPRNG  RandomGen;
#else

//  typedef JenkinsPRNG RandomGen;

    typedef TrinomialPRNG RandomGen;

#endif /*NORANDOM*/



#endif /*RANDOMGEN_H*/

