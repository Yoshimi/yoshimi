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
using std::memset;


class RandomGen
{
    public:
        RandomGen();

        bool init(unsigned int seed);
        float numRandom(void);
        unsigned int randomINT(void);
    private:
        char random_state[256];
        float random_0_1;

#if (HAVE_RANDOM_R)
        struct random_data random_buf;
        int32_t random_result;
#else
        long int random_result;
#endif /*HAVE_RANDOM_R*/

};


inline RandomGen::RandomGen() :
     random_0_1(0.0),
     random_result(0)
{
     memset(&random_state, 0, sizeof(random_state));
}


inline bool RandomGen::init(unsigned int seed)
{
    memset(random_state, 0, sizeof(random_state));
#if (HAVE_RANDOM_R)
    memset(&random_buf, 0, sizeof(random_buf));

    return 0 == initstate_r(seed, random_state, sizeof(random_state), &random_buf);
#else
    return NULL != initstate(seed, random_state, sizeof(random_state));
#endif /*HAVE_RANDOM_R*/
}


inline float RandomGen::numRandom(void)
{
    int ret;
#if (HAVE_RANDOM_R)
    ret = random_r(&random_buf, &random_result);
#else
    random_result = random();
    ret = 0;
#endif /*HAVE_RANDOM_R*/

    if (!ret)
    {
        random_0_1 = (float)random_result / (float)INT_MAX;
        random_0_1 = (random_0_1 > 1.0f) ? 1.0f : random_0_1;
        random_0_1 = (random_0_1 < 0.0f) ? 0.0f : random_0_1;
#ifndef NORANDOM
        return random_0_1;
#endif
    }
    return 0.5f;
}


inline unsigned int RandomGen::randomINT(void)
{
#if (HAVE_RANDOM_R)
    if (!random_r(&random_buf, &random_result))
#ifndef NORANDOM
        return random_result + INT_MAX / 2;
#endif
    return INT_MAX / 2;
//  return INT_MAX;          ///NOTE(Ichthyo)   : OscilGen.h used to return INT_MAX as a fallback
                             ///WARNING(Ichthyo): the syntax generated when #defined NORANDOM is broken. If random_r returns zero, the function leaves without return value!!!!
#else

#ifndef NORANDOM
    random_result = random();
#else
    random_result = 0;
#endif
    return (unsigned int)random_result + INT_MAX / 2;
#endif /*HAVE_RANDOM_R*/
}

#endif /*RANDOMGEN_H*/
