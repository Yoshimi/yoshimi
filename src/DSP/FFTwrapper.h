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
#include <utility>
#include <cassert>
#include <cstring>
#include <memory>
#include <mutex>

namespace fft {

/* Explanation of Memory usage and layout
 *
 * Yoshimi uses the "half-complex" format of libFFTW3
 * http://fftw.org/fftw3_doc/The-Halfcomplex_002dformat-DFT.html#The-Halfcomplex_002dformat-DFT
 *
 * Note: the transforms implemented in libFFTW3 are unnormalised, so invoking smps2freqs followed by
 * invoking freqs2smps on the same data will result in the original waveform scaled by N.
 *
 * Generally speaking, Fourier transform is an operation on complex numbers; however, in signal processing
 * the waveform is a function of real numbers and thus the imaginary part will always be zero. For such
 * a function, the spectrum (result of the Fourier transform) exhibits the "Hermite Symmetry": Given a
 * waveform with N samples, line(N/2) in the spectrum corresponds to the Nyquist frequency and will
 * have an imaginary part of 0*i. And line(N-k) will be the conjugate of line(k), i.e. have the same
 * real part and an imaginary part with flipped sign. This can be exploited to yield a speedup of
 * factor two in the Fourier operations, but requires the non-redundant information to be arranged
 * in memory according to the following scheme:
 * r0, r1, r2, ..., r(N/2-1), r(N/2), i(N/2-1), ..., i2, i1, [ i0 ]
 *
 * Here, r0 is the spectral line for freq = 0Hz, i.e. the DC-offset, r(N/2) is the spectral line for
 * the Nyquist frequency and can be ignored in practice. The following imaginary parts (the "sine
 * coefficients) encode the phase information; i0 is always zero and thus likewise ignored.
 *
 * In practice, the Synth code typically works directly on the "cosine" and "sine" coefficients,
 * indexing them with 0 ... N-1. Thus, for practical reasons, the Spectrum type provides accessor
 * functions c(i) and s(i). To simplify handling, an additional slot at index N will be allocated,
 * so that s(0) = coeff[N-0] is a valid expression (not out-of-bounds), but never passed to libFFTW3,
 * while coeff[N/2] is always set to zero and never accessed.
 *
 * For optimised implementation with SIMD operations (SSE, AVX, Altivec), libFFTW3 requires strict
 * alignment rules, which can be ensured by allocating memory through fftw_malloc (which allocates
 * some extra slots and then adjusts the start point to match the required alignment. Thus, all
 * data is encapsulated into the fft::Spectrum and fft::Waveform types, which automate allocations.
 * The Synth->oscilsize corresponds to FFTwrapper::tableSize().
 *
 * Lib FFTW3 builds a "FFT plan" for each operation, to optimise for the table size, the alignment,
 * for in-place vs. in/out data (Yoshimi always uses the latter case). In theory, this plan could
 * be optimised further by automatic performance tuning at start-up; but this would require to
 * run test-transforms on each application start-up and thus we just use the default FFTW_ESTIMATE,
 * which never touches the data pointers on plan generation and just guesses a suitable execution
 * plan. Another relevant flag is FFTW_PRESERVE_INPUT, which forces libFFTW to preserve input data;
 * FFTW could gain some additional performance when it is allowed to corrupt input data, however
 * for the usage pattern in a Synth it is more important to avoid additional allocations and
 * copying of data; thus we run each OscilGen with the fixed initial data allocation and pass
 * these data arrays directly to libFFTW, ensuring proper alignment and memory layout.
 */

template<class POL>
class FFTwrapper;



/* ===== automatically manage fftw_malloc allocated memory ===== */

struct Deleter
{
    void operator()(float* target) { fftwf_free(target); }
};

class Data
    : public std::unique_ptr<float, Deleter>
{
    static float* allocate(size_t elemCnt)
    {
        if (elemCnt == 0) // allow to create an empty Data holder
            return nullptr;
        size_t allocsize = (elemCnt) * sizeof(float);
        void* mem = fftwf_malloc(allocsize);
        if (!mem)
            throw std::bad_alloc();
        return static_cast<float*>(mem);
    }

public:
    using _unique_ptr = std::unique_ptr<float, Deleter>;

    Data(size_t fftsize)
        : _unique_ptr{allocate(fftsize)}
    { }

    /** discard existing allocation and possibly create/manage new allocation */
    void reset(size_t newSize =0)
    {
        _unique_ptr::reset(allocate(newSize));
    }

    float      & operator[](size_t i)       { return get()[i]; }
    float const& operator[](size_t i) const { return get()[i]; }
};




/* Spectrum coefficients - properly arranged for Fourier-operations through libFFTW3 */
class Spectrum
{
    size_t siz;    // tableSize == 2*spectrumSize
    Data coeff;

    template<class POL> friend class FFTwrapper;  // allowed to access raw data

public: // can not be copied or moved
    Spectrum(Spectrum&&)                 = delete;
    Spectrum(Spectrum const&)            = delete;
    Spectrum& operator=(Spectrum&&)      = delete;

    // copy-assign other spectrum values
    Spectrum& operator=(Spectrum const& src)
    {
        if (this != &src)
        {
            assert(src.size() == siz/2);
            for (size_t i=0; i <= siz; ++i)
                coeff[i] = src.coeff[i];
        }
        return *this;
    }

    // automatic memory-management
    Spectrum(size_t spectrumSize)
        : siz{2*spectrumSize}
        , coeff{siz+1}
    {
        reset();
    }

    void reset()
    {
        size_t allocsize = (siz+1) * sizeof(float);
        memset(coeff.get(), 0, allocsize);
    }

    size_t size()  const { return siz/2; }

    // array-like access
    float      & c(size_t i)       { assert(i<=siz/2); return coeff[i];       }
    float const& c(size_t i) const { assert(i<=siz/2); return coeff[i];       }
    float      & s(size_t i)       { assert(i<=siz/2); return coeff[siz - i]; }
    float const& s(size_t i) const { assert(i<=siz/2); return coeff[siz - i]; }
};



/* Waveform data - properly aligned for libFFTW3 Fourier-operations */
class Waveform
{
    size_t siz;
    Data samples;

    template<class POL> friend class FFTwrapper;  // allowed to access raw data

public:
    static constexpr size_t INTERPOLATION_BUFFER = 5;

    // can only be moved, not copied
    Waveform(Waveform&&)            = default;
    Waveform(Waveform const&)       = delete;
    Waveform& operator=(Waveform&&) = delete;

    // copy-assign other waveform sample data
    Waveform& operator=(Waveform const& src)
    {
        if (this != &src)
        {
            assert(src.size() == siz);
            for (size_t i=0; i < siz+INTERPOLATION_BUFFER; ++i)
            {
                samples[i] = src[i];
            }
        }
        return *this;
    }

    // automatic memory-management
    Waveform(size_t tableSize)
        : siz{tableSize}
        , samples{siz+INTERPOLATION_BUFFER}
    {
        reset();
    }

    void reset()
    {
        size_t allocsize = (siz+INTERPOLATION_BUFFER) * sizeof(float);
        memset(samples.get(), 0, allocsize);
    }

    /* redundantly append the first elements for interpolation */
    void fillInterpolationBuffer()
    {
        assert(samples);
        for (size_t i = 0; i < INTERPOLATION_BUFFER; ++i)
            samples[siz+i] = samples[i];
    }

    size_t size()  const { return siz; }

    // array subscript operator
    float      & operator[](size_t i)       { assert(i<siz+INTERPOLATION_BUFFER); return samples[i]; }
    float const& operator[](size_t i) const { assert(i<siz+INTERPOLATION_BUFFER); return samples[i]; }


    friend void swap(Waveform& w1, Waveform& w2)
    {
        using std::swap;
        swap(w1.samples, w2.samples);
        swap(w1.siz,     w2.siz);
    }

protected:
    /* derived special Waveform holders may be created empty,
     * allowing for statefull lifecycle and explicitly managed data.
     * See WaveformHolder in ADnote.h */
    Waveform()
        : siz(0)
        , samples{0}
    {  }

    /** give up ownership without discarding data */
    void detach()
    {
        samples.release();
        siz = 0;
    }
    /** allow derived classes to connect an existing allocation.
     * @warning subverts unique ownership; use with care. */
    void attach(Waveform const& other)
    {
        Data::_unique_ptr& rawHolder{samples};
        rawHolder.reset(other.samples.get());
        siz = other.siz;
    }
};




/* FFT-Operation execution plan : the standard setup.
 * Yoshimi uses only a single setup scheme, comprised of a fourier and an inverse fourier transform
 * for real valued functions with »half complex« spectrum representation (FFTW_R2HC, FFTW_HC2R).
 * Calculation is always performed on working data allocations provided at invocation time, operating
 * from input to output data (not in-place, different pointers passed),  where input data must not be
 * corrupted or changed (FFTW_PRESERVE_INPUT). No dynamic measurement and optimisation is performed
 * at startup time when creating the plan (FFTW_ESTIMATE)
 */
struct FFTplan
{
    fftwf_plan fourier{nullptr};
    fftwf_plan inverse{nullptr};

    // may be copied but not assigned
    FFTplan(FFTplan const&)            = default;
    FFTplan& operator=(FFTplan&&)      = delete;
    FFTplan& operator=(FFTplan const&) = delete;

// private:  /////////////////////////////////////////////TODO
    // can not be generated directly,
    // only through the managing FFTplanRepo
    FFTplan(size_t fftsize)
    {
        //////////////////////////////////////////////////TODO handle locking in FFTplanRepo
        static std::mutex mtx_plan;
        std::lock_guard<std::mutex> lock(mtx_plan);
        //////////////////////////////////////////////////TODO handle locking in FFTplanRepo
        // dummy allocation used as placeholder for plan generation
        Data samples{fftsize};
        Data spectrum{fftsize};
        fourier = fftwf_plan_r2r_1d(fftsize, samples.get(), spectrum.get(), FFTW_R2HC, FFTW_ESTIMATE | FFTW_PRESERVE_INPUT);
        inverse = fftwf_plan_r2r_1d(fftsize, spectrum.get(), samples.get(), FFTW_HC2R, FFTW_ESTIMATE | FFTW_PRESERVE_INPUT);
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
    FFTplan plan;

    public:
        FFTwrapper(size_t fftsize_)
            : fftsize{fftsize_}
            , plan{fftsize}  ////////////////////TODO invoke Policy -> FFTplanRepo
        { }

       ~FFTwrapper()
        {
            /////////////////////////////////////TODO should be handled automatically by FFTplanRepo
            auto lock = POL::lock4setup();
            fftwf_destroy_plan(plan.fourier);
            fftwf_destroy_plan(plan.inverse);
        }
        // shall not be copied or moved
        FFTwrapper(FFTwrapper&&)                 = delete;
        FFTwrapper(FFTwrapper const&)            = delete;
        FFTwrapper& operator=(FFTwrapper&&)      = delete;
        FFTwrapper& operator=(FFTwrapper const&) = delete;

        size_t tableSize()    const { return fftsize; }
        size_t spectrumSize() const { return fftsize / 2; }

        /* Fast Fourier Transform */
        void smps2freqs(Waveform const& smps, Spectrum& freqs)
        {
            auto lock = POL::lock4usage();
            size_t half_size{spectrumSize()};
            assert (half_size == freqs.size());
            assert (fftsize == smps.size());
            fftwf_execute_r2r(plan.fourier, smps.samples.get(), freqs.coeff.get());
            freqs.c(half_size) = 0.0; // Nyquist line is irrelevant and never used
            freqs.s(0) = 0.0;         // Phase of DC offset (not calculated by libFFTW3)
        }

        /* Fast Inverse Fourier Transform */
        void freqs2smps(Spectrum const& freqs, Waveform& smps)
        {
            auto lock = POL::lock4usage();
            assert (spectrumSize() == freqs.size());
            fftwf_execute_r2r(plan.inverse, freqs.coeff.get(), smps.samples.get());
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
