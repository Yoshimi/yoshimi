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
#include <cassert>
#include <cstring>
#include <mutex>

namespace fft {

/* holds the spectrum coefficients */
class Spectrum
{
    size_t siz;

    float *co;
    float *si;

public: // can not be copied or moved
    Spectrum(Spectrum&&)                 = delete;
    Spectrum(Spectrum const&)            = delete;
    Spectrum& operator=(Spectrum&&)      = delete;
    Spectrum& operator=(Spectrum const&) = delete;

    // automatic memory-management
    Spectrum(size_t tableSize)
        : siz(tableSize)
    {
        this->co = (float*)fftwf_malloc(siz * sizeof(float));
        this->si = (float*)fftwf_malloc(siz * sizeof(float));
        if (!co or !si)
            throw std::bad_alloc();
        reset();
    }

   ~Spectrum()
    {
        if (si) fftwf_free(si);
        if (co) fftwf_free(co);
    }

    void reset()
    {
        memset(this->co, 0, siz * sizeof(float));
        memset(this->si, 0, siz * sizeof(float));
    }

    // array-like access
    float      & c(size_t i)       { return co[i]; }
    float const& c(size_t i) const { return co[i]; }
    float      & s(size_t i)       { return si[i]; }
    float const& s(size_t i) const { return si[i]; }

    size_t size()  const { return siz; }
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
        void smps2freqs(float const* smps, Spectrum& freqs)
        {
            auto lock = POL::lock4usage();
            size_t half_size{fftsize / 2};
            assert (half_size <= freqs.size());
            memcpy(data1, smps, fftsize * sizeof(float));
            fftwf_execute(planFourier);
//          memcpy(freqs.co, data1, half_size * sizeof(float));  ////////////TODO : use »new-array-execute« API and eliminate data copy
            for (size_t i = 0; i < half_size; ++i)
                freqs.c(i) = data1[i];
            for (size_t i = 1; i < half_size; ++i)
                freqs.s(i) = data1[fftsize - i];
            data2[half_size] = 0.0f; ////////////////////TODO this line was there from the first Import by Cal in 2010; but it looks like a mistake,
                                     ////////////////////     since data2 should not be touched by the forward fourier transform.
                                     ////////////////////     Maybe the original author intended to set freqs.s(0) = 0.0 ?
                                     ////////////////////     data?[half_size] actually corresponds to the real part at Nyquist and is ignored.
        }

        /* Fast Inverse Fourier Transform */
        void freqs2smps(Spectrum const& freqs, float* smps)
        {
            auto lock = POL::lock4usage();
            size_t half_size{fftsize / 2};
            assert (half_size <= freqs.size());
//          memcpy(data2, freqs.co, half_size * sizeof(float));  ////////////TODO : use »new-array-execute« API and eliminate data copy
            for (size_t i = 0; i < half_size; ++i)
                data2[i] = freqs.c(i);
            data2[half_size] = 0.0;
            for (size_t i = 1; i < half_size; ++i)
                data2[fftsize - i] = freqs.s(i);
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


using Calc = FFTwrapper<Policy_DoNothing>;

using CalcSharedUse = FFTwrapper<Policy_LockAtUsage<>>;
using CalcConcurrent = FFTwrapper<Policy_LockAtCreation<>>;


}//(End)namespace fft
#endif /*FFT_WRAPPER_H*/
