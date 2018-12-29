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


class RandomGen
{
    public:
        RandomGen();

        bool init(uint32_t seed);
        uint32_t prngval();
        float numRandom();
        uint32_t randomINT();
    private:
        char random_state[256];

#if (HAVE_RANDOM_R)
        struct random_data random_buf;
#endif /*HAVE_RANDOM_R*/

};


inline RandomGen::RandomGen()
{
     memset(&random_state, 0, sizeof(random_state));
}


inline bool RandomGen::init(uint32_t seed)
{
    memset(random_state, 0, sizeof(random_state));
#if (HAVE_RANDOM_R)
    memset(&random_buf, 0, sizeof(random_buf));

    return 0 == initstate_r(seed, random_state, sizeof(random_state), &random_buf);
#else
    return NULL != initstate(seed, random_state, sizeof(random_state));
#endif /*HAVE_RANDOM_R*/
}


inline uint32_t RandomGen::prngval()
{
#ifdef NORANDOM
    return INT_MAX / 2;
#else

#if (HAVE_RANDOM_R)
    int32_t random_result;
    random_r(&random_buf, &random_result);
    // can not fail, since &random_buf can not be NULL
    // random_result holds number 0...INT_MAX
    return random_result;
#else
    return uint32_t(random());
#endif /*HAVE_RANDOM_R*/

#endif /*NORANDOM*/
}


inline float RandomGen::numRandom()
{
#ifndef NORANDOM
    float random_0_1 = float(prngval()) / (float)INT_MAX;
    random_0_1 = (random_0_1 > 1.0f) ? 1.0f : random_0_1;
    random_0_1 = (random_0_1 < 0.0f) ? 0.0f : random_0_1;
    return random_0_1;
#else
    return 0.5f;
#endif
}


// random number in the range 0...INT_MAX
inline uint32_t RandomGen::randomINT()
{
    return prngval();
}

#endif /*RANDOMGEN_H*/
