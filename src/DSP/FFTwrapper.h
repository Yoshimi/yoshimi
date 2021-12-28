/*
    FFTwrapper.h  -  A wrapper for Fast Fourier Transforms

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2021,  Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of ZynAddSubFX original code, modified March 2011
*/

#ifndef FFT_WRAPPER_H
#define FFT_WRAPPER_H

#include <fftw3.h>
#include <stdexcept>
#include <cstring>
#include <mutex>


/* holds the spectrum coefficients */
class FFTFreqs
{
    size_t size;
public:
    float *s;
    float *c;

public: // can not be copied or moved
    FFTFreqs(FFTFreqs&&)                 = delete;
    FFTFreqs(FFTFreqs const&)            = delete;
    FFTFreqs& operator=(FFTFreqs&&)      = delete;
    FFTFreqs& operator=(FFTFreqs const&) = delete;

    // automatic memory-management
    FFTFreqs(size_t tableSize)
        : size(tableSize)
    {
        this->c = (float*)fftwf_malloc(size * sizeof(float));
        this->s = (float*)fftwf_malloc(size * sizeof(float));
        if (!c or !s)
            throw std::bad_alloc();
        reset();
    }

   ~FFTFreqs()
    {
        if (s) fftwf_free(s);
        if (c) fftwf_free(c);
    }

    void reset()
    {
        memset(this->c, 0, size * sizeof(float));
        memset(this->s, 0, size * sizeof(float));
    }
};


/* Standard Fourier Transform operations:
 * setup and access to FFTW calculation plans
 * Template Parameter POL : a policy to use for locking (see below)
 */
template<class POL>
class FFTwrapper : POL
{
    size_t fftsize;
    float *data1;
    float *data2;
    fftwf_plan planFourier;
    fftwf_plan planInverse;

    public:
        FFTwrapper(size_t fftsize_)
            : fftsize(fftsize_)
        {
            auto lock = POL::lock4setup();
            data1 = (float*)fftwf_malloc(fftsize * sizeof(float));
            data2 = (float*)fftwf_malloc(fftsize * sizeof(float));
            planFourier = fftwf_plan_r2r_1d(fftsize, data1, data1, FFTW_R2HC, FFTW_ESTIMATE);
            planInverse = fftwf_plan_r2r_1d(fftsize, data2, data2, FFTW_HC2R, FFTW_ESTIMATE);
        }

       ~FFTwrapper()
        {
            auto lock = POL::lock4setup();
            fftwf_destroy_plan(planFourier);
            fftwf_destroy_plan(planInverse);
            fftwf_free(data1);
            fftwf_free(data2);
        }
        // shall not be copied or moved
        FFTwrapper(FFTwrapper&&)                 = delete;
        FFTwrapper(FFTwrapper const&)            = delete;
        FFTwrapper& operator=(FFTwrapper&&)      = delete;
        FFTwrapper& operator=(FFTwrapper const&) = delete;

        /* Fast Fourier Transform */
        void smps2freqs(float const* smps, FFTFreqs& freqs)
        {
            auto lock = POL::lock4usage();
            size_t half_size{fftsize / 2};
            memcpy(data1, smps, fftsize * sizeof(float));
            fftwf_execute(planFourier);
            memcpy(freqs.c, data1, half_size * sizeof(float));
            for (size_t i = 1; i < half_size; ++i)
                freqs.s[i] = data1[fftsize - i];
            data2[half_size] = 0.0f; ////////////////////TODO this line was there from the first Import by Cal in 2010; but it looks like a mistake,
                                     ////////////////////     since data2 should not be touched by the forward fourier transform.
                                     ////////////////////     Maybe the original author intended to set freqs.s(0) = 0.0 ?
                                     ////////////////////     data?[half_size] actually corresponds to the real part at Nyquist and is ignored.
        }

        /* Fast Inverse Fourier Transform */
        void freqs2smps(FFTFreqs const& freqs, float* smps)
        {
            auto lock = POL::lock4usage();
            size_t half_size{fftsize / 2};
            memcpy(data2, freqs.c, half_size * sizeof(float));
            data2[half_size] = 0.0;
            for (size_t i = 1; i < half_size; ++i)
                data2[fftsize - i] = freqs.s[i];
            fftwf_execute(planInverse);
            memcpy(smps, data2, fftsize * sizeof(float));
        }
};



/* ========= FFTW Locking Policies ============ */
/*
 * Note: creation of FFTW3 calculation plans is *not threadsafe*.
 * See http://fftw.org/fftw3_doc/Thread-safety.html
 * Moreover, when the input/output storage locations within a predefined
 * calculation plan are used, then concurrent invocations of the Fourier operations
 * themselves might lead to data corruption.
 *
 * Originally, ZynAddSubFX (and Yoshimi) were built with a sequential computation model
 * for the sound generation. As of 2021, most SynthEngine code still runs in a single thread.
 * In most cases thus the concurrency issues can be ignored. However, some usages related to PADSynth
 * can be performed concurrently and in the background, and here FFTW3 usage requires locking.
 *
 * Since all SynthEngine code accesses Fourier operations through the FFTwrapper, these
 * different requirements can be accommodated through a /compile time policy parameter/:
 * Mix in the appropriate policy to either make the locking statements NOP, or to implement
 * them with a shared or local lock. Note: Policies can be combined by chaining.
 */

/*
 * Locking Policy: ignore any concurrency issues and access FFTW3 without locking.
 */
class Policy_DoNothing
{
protected:
    struct NoLockNecessary {
       ~NoLockNecessary() { /* suppress warning about unused lock */ }
    };

    struct Guard
    {
        std::mutex& mtx_;
        Guard(std::mutex& m) : mtx_{m} { mtx_.lock(); }
       ~Guard()                        { mtx_.unlock(); }
    };

    NoLockNecessary lock4setup() { return NoLockNecessary{}; }
    NoLockNecessary lock4usage() { return NoLockNecessary{}; }
};


/*
 * Locking Policy: ensure that creation of FFTW plans is locked globally.
 * This policy uses a single shared mutex in static memory
 */
template<class POL = Policy_DoNothing>
class Policy_LockAtCreation : protected POL
{
    static std::mutex mtx_createPlan;
protected:
    using Guard = typename POL::Guard;

    Guard lock4setup() { return Guard{mtx_createPlan}; }
};

template<class POL>
std::mutex Policy_LockAtCreation<POL>::mtx_createPlan{};


/*
 * Locking Policy: ensure that this FFTwrapper /instance/ is never used
 * concurrently for actual Fourier operations.
 * This policy embeds a mutex lock into each instance.
 */
template<class POL = Policy_DoNothing>
class Policy_LockAtUsage : protected POL
{
    std::mutex mtx_usePlan;
protected:
    using Guard = typename POL::Guard;

    Guard lock4usage() { return Guard{mtx_usePlan}; }
};


using FFTcalc = FFTwrapper<Policy_DoNothing>;

using FFTcalcSharedUse = FFTwrapper<Policy_LockAtUsage<>>;
using FFTcalcConcurrent = FFTwrapper<Policy_LockAtCreation<>>;


#endif /*FFT_WRAPPER_H*/
