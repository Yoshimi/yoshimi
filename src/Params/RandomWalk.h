/*
    RandomWalk.h - slow random fluctuations of parameter values

    Copyright 2022,  Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the License,
    or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License (version 2
    or later) for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef RANDOM_WALK_H
#define RANDOM_WALK_H

#include "globals.h"
#include "Misc/RandomGen.h"
#include "Misc/NumericFuncs.h"

#include <cassert>


/**
 * Generate a sequence of random value fluctuations around an anchor point (parameter).
 * This value object generates a factor, which randomly walks around 1.0, with a configurable spread.
 * Initially the value is 1.0 and by default there is no spread and thus the value will be constant.
 * The random walk itself is performed on a logarithmic scale, since the result shall be delivered
 * as number factor; this means that e.g. factor 2 has the same probability as factor 1/2, with
 * 1.0 being the most likely expectation value. To control the distribution, the (single) parameter
 * value #setSpread can be set; this parameter operates again on a non-linear scale, with value 0
 * to disable the random walk and value 96 corresponding to a span of +100%|-50% (i.e. Factor 2).
 * This parameter scale is focused on low spread values, while still allowing extreme randomisation.
 */
class RandomWalk
{
    float pos{0};    // on a log2-scale
    float spread{0}; // likewise log2 (spread=1.0 ==> spread-factor 2.0)
    RandomGen& prng;

public:
    RandomWalk(RandomGen& randSrc)
        : prng{randSrc}
    { }
   ~RandomWalk() = default;

    // can be moved/copied but not assigned
    RandomWalk(RandomWalk&&)                 = default;
    RandomWalk(RandomWalk const&)            = default;
    RandomWalk& operator=(RandomWalk&&)      = delete;
    RandomWalk& operator=(RandomWalk const&) = delete;

    // test if this RandomWalk is enabled
    explicit operator bool()  const
    { return spread != 0; }

    void setSpread(uchar spreadParam)
    { spread = log2(param2spread(spreadParam)); }

    uchar getSpread()  const
    { return spread2param(func::power<2>(spread)); }

    float getSpreadCent()  const
    { return 1200*log2f(spread); }

    float getSpreadPercent()  const
    { return 100.0f * (func::power<2>(spread) - 1.0f); }

    void reset()
    {
        pos = 0;
        spread = 0;
    }


    /** calculate the offset-factor representing the current walk position */
    float getFactor()  const
    {
        return pos == 0? 1.0f
                       : pos > 0? func::power<2>  (+pos)
                                : func::powFrac<2>(-pos);
    }

    /** Perform a single random-walk step. */
    void walkStep()
    {
        if (spread <= 0) reset();
        else
        {// perform random step...
            float rnd = prng.numRandom();
            if (0 < rnd and rnd < 1) // just stay put else
            {// strictly symmetrical distribution to avoid drift
                float offset = 2.0f * rnd - 1;   //  ]-1 ... +1[
                assert(-1 < offset and offset < 1);
                pos += spread * offset;          //  random walk
                float dist = fabsf(pos/spread);
                if (dist > 1 and pos*offset > 0)
                    pos /= dist;  // damp excess outward trend
            }
        }
    }

/*
 *  p≔1   ⟹ factor 1.004  ~ 7 cent
 *  p≔47  ⟹ factor 1.059  ~ 1 semitone root12(2) = 1.059
 *  p≔60  ⟹ factor 1.12
 *  p≔90  ⟹ factor 1.71
 *  p≔96  ⟹ factor 2.0    = 1 Octave
 *  p≔110 ⟹ factor 3.24
 *  p≔115 ⟹ factor 3.99   ~ 2 Octaves
 *  p≔127 ⟹ factor 6.99
 */

    static double param2spread(uchar param)
    {
        if (param == 0) return 0.0f;
        if (param >127) param = 127;
        // calculate 1 + (4 ^ (p/96 - 1))^4
        double arg = param/96.0 - 1.0;
        double exp4 = exp(log(4) * arg); // 4^arg
        return 1 + exp4*exp4*exp4*exp4;
    }

    static uchar spread2param(double spread)
    {
        if (spread == 0.0)
            return 0;
        // s = 1 + (4 ^ (p/96 - 1))^4
        // root4(s) = 1 + (4 ^ (p/96 - 1))
        // p/96 - 1 = log4(root4(s) - 1)
        // p = 96·(log4(root4(s) - 1) + 1)
        double root4 = exp(log(spread)/4);
        double log4 = log(root4 - 1)/log(4);
        return uchar(96 * (log4 + 1));
    }
};


#endif /*RANDOM_WALK_H*/
