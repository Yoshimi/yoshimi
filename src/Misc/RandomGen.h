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


// Inlined copy of the Glibc 2.28 implementation of random_r()
// This code should be equivalent to prior Yoshimi versions linked against GNU Glibc.
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
        struct random_data
          {
            int32_t *fptr;      /* Front pointer.  */
            int32_t *rptr;      /* Rear pointer.  */
            int32_t *state;     /* Array of state values.  */
            int rand_type;      /* Type of random number generator.  */
            int rand_deg;       /* Degree of random number generator.  */
            int rand_sep;       /* Distance between front and rear.  */
            int32_t *end_ptr;   /* Pointer behind state table.  */
          };

        char state[256];
        struct random_data buf;

    private: ///////TODO code copied literally from Glibc; to be simplified and inlined

void
inl_srandom_r (unsigned int seed)
{
  int32_t *state;
  long int i;
  int32_t word;
  int32_t *dst;
  int kc;

  state = buf.state;
  /* We must make sure the seed is not 0.  Take arbitrarily 1 in this case.  */
  if (seed == 0)
    seed = 1;
  state[0] = seed;

  dst = state;
  word = seed;
  kc = buf.rand_deg;
  for (i = 1; i < kc; ++i)
    {
      /* This does:
       state[i] = (16807 * state[i - 1]) % 2147483647;
     but avoids overflowing 31 bits.  */
      long int hi = word / 127773;
      long int lo = word % 127773;
      word = 16807 * lo - 2836 * hi;
      if (word < 0)
    word += 2147483647;
      *++dst = word;
    }

  buf.fptr = &state[buf.rand_sep];
  buf.rptr = &state[0];
  kc *= 10;
  while (--kc >= 0)
    inl_random_r();
}

void
inl_initstate_r (unsigned int seed)
{
    //////////////////////////////////WIP : inlined arguments
    char *arg_state = state;
    //////////////////////////////////WIP : inlined arguments

  buf.rand_type = 4;
  buf.rand_sep = 1;
  buf.rand_deg = 63; /* random generation uses this trinomial: x**63 + x + 1.  */

  int32_t *stateA = &((int32_t *) arg_state)[1]; /* First location.  */
  /* Must set END_PTR before srandom.  */
  buf.end_ptr = &stateA[63];

  buf.state = stateA;

  inl_srandom_r (seed);
}

int32_t
inl_random_r ()
{
  int32_t result;
  int32_t *state;

  state = buf.state;

  int32_t *fptr = buf.fptr;
  int32_t *rptr = buf.rptr;
  int32_t *end_ptr = buf.end_ptr;
  uint32_t val;

  val = *fptr += (uint32_t) *rptr;
  /* Chucking least random bit.  */
  result = val >> 1;
  ++fptr;
  if (fptr >= end_ptr)
    {
      fptr = state;
      ++rptr;
    }
  else
    {
      ++rptr;
      if (rptr >= end_ptr)
        rptr = state;
    }
  buf.fptr = fptr;
  buf.rptr = rptr;

  return result;
}

    public:
        TrinomialPRNG()
        {
            memset(&state, 0, sizeof(state));
        }

        bool init(uint32_t seed)
        {
            memset(state, 0, sizeof(state));
            memset(&buf, 0, sizeof(buf));
            inl_initstate_r(seed);
            return true;
        }

        uint32_t prngval()
        {
            // random_result holds number 0...INT_MAX
            return uint32_t(inl_random_r());
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

//    typedef JenkinsPRNG RandomGen;

      typedef TrinomialPRNG RandomGen;

#endif /*NORANDOM*/



#endif /*RANDOMGEN_H*/
