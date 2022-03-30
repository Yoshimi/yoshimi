/*
    WaveInterpolator.h - component for wavetable interpolation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2022, Ichthyostega

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

    This file is a derivative of a ZynAddSubFX original.

*/

#ifndef WAVE_INTERPOLATOR_H
#define WAVE_INTERPOLATOR_H

#include "DSP/FFTwrapper.h"
#include "Misc/Alloc.h"
#include "Misc/SynthHelper.h"

#include <memory>
#include <functional>

using std::unique_ptr;
using std::function;
using synth::interpolateAmplitude;

/** Interface for wavetable interpolation */
class WaveInterpolator
{
    protected:
        WaveInterpolator() = default;

        // "virtual copy" pattern
        virtual WaveInterpolator* buildClone() const  =0;

    public: // can be copy/move constructed, but not assigned...
        WaveInterpolator(WaveInterpolator&&)                 = default;
        WaveInterpolator(WaveInterpolator const&)            = default;
        WaveInterpolator& operator=(WaveInterpolator&&)      = delete;
        WaveInterpolator& operator=(WaveInterpolator const&) = delete;

        virtual ~WaveInterpolator() = default; // this is an interface


        virtual bool matches(fft::Waveform const&) const                   =0;
        virtual float getCurrentPhase()  const                             =0;
        virtual void caculateSamples(float*,float*, float freq,size_t cnt) =0;


        /* build a concrete interpolator instance for stereo interpolation either cubic or linear */
        static WaveInterpolator* create(bool cubic, float phase, bool stereo, fft::Waveform const& wave, float tableFreq);
        static WaveInterpolator* clone(unique_ptr<WaveInterpolator> const&);
        static WaveInterpolator* clone(WaveInterpolator const& orig);

        /* create a delegate for Cross-Fadeing WaveInterpolator */
        static WaveInterpolator* createXFader(function<void(void)> attachXFader
                                             ,function<void(void)> detachXFader
                                             ,function<void(WaveInterpolator*)> switchInterpolator
                                             ,unique_ptr<WaveInterpolator> oldInterpolator
                                             ,unique_ptr<WaveInterpolator> newInterpolator
                                             ,size_t crossFadeLengthSmps
                                             ,size_t bufferSize);
};


/**
 * Abstract Base Class : two channel interpolation
 * with common phase and fixed 180° channel offset
 */
class StereoInterpolatorBase
    : public WaveInterpolator
{
    protected:
        fft::Waveform const& table;
        const float baseFreq;
        const size_t size;

        size_t posHiL;
        size_t posHiR;
        float posLo;

    public:
        using WaveInterpolator::WaveInterpolator;

        StereoInterpolatorBase(fft::Waveform const& wave, float freq)
            : table{wave}
            , baseFreq{freq}
            , size{wave.size()}
            , posHiL{0}
            , posHiR{0}
            , posLo{0}
        { }


        bool matches(fft::Waveform const& otherTable)  const override
        {
            return &table == &otherTable;
        }

        float getCurrentPhase()  const override
        {
            return (posHiL + posLo) / float(size);
        }

        WaveInterpolator* setStartPos(float phase, bool stereo)
        {
            phase = fmodf(phase, 1.0f);
            float offset = phase * size;
            posHiL = size_t(offset);
            posHiR = stereo? (posHiL + size/2) % size
                           : posHiL;
            posLo = offset - posHiL;
            assert (posHiL < size);
            assert (posHiR < size);
            assert (posLo < 1.0);
            return this;
        }
};



class LinearInterpolator
    : public StereoInterpolatorBase
{
        void caculateSamples(float *smpL, float *smpR, float freq, size_t cntSmp)  override
        {
            float speedFactor = freq / baseFreq;
            size_t incHi = size_t(floorf(speedFactor));
            float  incLo = speedFactor - incHi;

            for (size_t i = 0; i < cntSmp; ++i)
            {
                posHiL += incHi;
                posHiR += incHi;
                posLo   += incLo;
                if (posLo >= 1.0)
                {
                    posHiL += 1;
                    posHiR += 1;
                    posLo -= 1.0;
                }
                if (posHiL >= size)
                    posHiL %= size;
                if (posHiR >= size)
                    posHiR %= size;

                smpL[i] = table[posHiL] * (1.0 - posLo) + table[posHiL + 1] * posLo;
                smpR[i] = table[posHiR] * (1.0 - posLo) + table[posHiR + 1] * posLo;
            }
        }

        WaveInterpolator* buildClone()  const override
        {   return new LinearInterpolator(*this); }

    public:
        using StereoInterpolatorBase::StereoInterpolatorBase;
};


class CubicInterpolator
    : public StereoInterpolatorBase
{
        void caculateSamples(float *smpL, float *smpR, float freq, size_t cntSmp)  override
        {
            float speedFactor = freq / baseFreq;
            size_t incHi = size_t(floorf(speedFactor));
            float  incLo = speedFactor - incHi;

            float xm1, x0, x1, x2, a, b, c;
            for (size_t i = 0; i < cntSmp; ++i)
            {
                posHiL += incHi;
                posHiR += incHi;
                posLo   += incLo;
                if (posLo >= 1.0)
                {
                    posHiL += 1;
                    posHiR += 1;
                    posLo -= 1.0;
                }
                if (posHiL >= size)
                    posHiL %= size;
                if (posHiR >= size)
                    posHiR %= size;

                // left
                xm1 = table[posHiL];
                x0 = table[posHiL + 1];
                x1 = table[posHiL + 2];
                x2 = table[posHiL + 3];
                a = (3.0 * (x0 - x1) - xm1 + x2) * 0.5;
                b = 2.0 * x1 + xm1 - (5.0 * x0 + x2) * 0.5;
                c = (x1 - xm1) * 0.5;
                smpL[i] = (((a * posLo) + b) * posLo + c) * posLo + x0;
                // right
                xm1 = table[posHiR];
                x0 = table[posHiR + 1];
                x1 = table[posHiR + 2];
                x2 = table[posHiR + 3];
                a = (3.0 * (x0 - x1) - xm1 + x2) * 0.5;
                b = 2.0 * x1 + xm1 - (5.0 * x0 + x2) * 0.5;
                c = (x1 - xm1) * 0.5;
                smpR[i] = (((a * posLo) + b) * posLo + c) * posLo + x0;
            }
        }

        WaveInterpolator* buildClone()  const override
        {   return new CubicInterpolator(*this); }

    public:
        using StereoInterpolatorBase::StereoInterpolatorBase;
};



/**
 * Specially rigged wavetable interpolator which actually calculates two
 * delegate interpolators and then cross-fades the generated samples.
 * When the cross-fade is complete, a given clean-up-Functor is invoked,
 * which typically discards this delegate and installs the the target
 * interpolator instead for ongoing regular operation.
 * @note since the interpolator base implementation just assigns new samples into the
 *       given buffer (which is good for performance reasons in the standard case),
 *       unfortunately we need to allocate a secondary working buffer
 */
class XFadeDelegate
    : public WaveInterpolator
{
    unique_ptr<WaveInterpolator> oldInterpolator;
    unique_ptr<WaveInterpolator> newInterpolator;
    function<void(void)>              attach_instance;
    function<void(void)>              detach_instance;
    function<void(WaveInterpolator*)> install_followup;

    const size_t fadeLengthSmps;
    const size_t bufferSize;

    synth::SFadeCurve mixCurve;
    Samples tmpL,tmpR;

    size_t progress, mixStep;
    float mixIn,mixOut,
          mixInPrev,mixOutPrev;

    private:
        bool matches(fft::Waveform const& otherTable) const override
        {
            return newInterpolator->matches(otherTable);
        }

        float getCurrentPhase()  const override
        {
            return newInterpolator->getCurrentPhase();
        }

        /** Delegate to both attached interpolators and then calculate cross-faded samples. */
        void caculateSamples(float *smpL, float *smpR, float noteFreq, size_t cntSmp)  override
        {
            oldInterpolator->caculateSamples(tmpL.get(),tmpR.get(), noteFreq, cntSmp);
            newInterpolator->caculateSamples(smpL,smpR, noteFreq, cntSmp);

            static_assert(1.0 / (PADnoteParameters::XFADE_UPDATE_MAX/1000 * 96000) > std::numeric_limits<float>::epsilon(),
                          "mixing step resolution represented as float");
            // step = 20000ms/1000ms/s * 96kHz ≈ 1.92e6 < 2^-23 ==> 1-1/step can be represented as float
            for (size_t i = 0;
                 i < cntSmp and progress < fadeLengthSmps;
                 ++i, ++progress)
            {
                if (progress % bufferSize == 0)
                {// k-Step : start linear fade sub-segment
                    mixInPrev = mixIn;
                    mixOutPrev = mixOut;
                    mixIn = mixCurve.nextStep();     // S-shaped exponential mix curve
                    mixOut = sqrtf(1 - mixIn*mixIn); // Equal-Power mix, since waveform typically not correlated
                    mixStep = progress;              // recall progress value at start of (linear) interpolation segment
                }
                size_t offset = progress - mixStep;
                float volOut  = interpolateAmplitude(mixOutPrev,mixOut, offset, bufferSize);
                float volIn   = interpolateAmplitude(mixInPrev, mixIn,  offset, bufferSize);
                smpL[i] = tmpL[i] * volOut  +  smpL[i] * volIn;
                smpR[i] = tmpR[i] * volOut  +  smpR[i] * volIn;
            }
            // When fadeLengthSmps is reached in the middle of a buffer, remainder was filled from otherInterpolator.
            // Use given clean-up functor to detach and discard this instance and install otherInterpolator instead.
            if (progress >= fadeLengthSmps)
                install_followup(
                    newInterpolator.release());
        }


        // Note: since cloning is only used for Legato notes (as of 3/2022), which are then cross-faded
        //       it is pointless to clone an ongoing wavetable crossfade, and moreover this could lead to
        //       whole tree of crossfade delegates, when playing several Legato notes during an extended
        //       x-fade. Thus "cloning" only the new target wavetable interpolator, to preserve phase info.
        WaveInterpolator* buildClone()  const override
        {
            return WaveInterpolator::clone(newInterpolator);
        }

    public:
        XFadeDelegate(function<void(void)> attachXFader
                     ,function<void(void)> detachXFader
                     ,function<void(WaveInterpolator*)> switchInterpolator
                     ,unique_ptr<WaveInterpolator> oldInterpolator
                     ,unique_ptr<WaveInterpolator> newInterpolator
                     ,size_t fadeLen, size_t buffSiz)
            : oldInterpolator{move(oldInterpolator)}
            , newInterpolator{move(newInterpolator)}
            , attach_instance{attachXFader}
            , detach_instance{detachXFader}
            , install_followup{switchInterpolator}
            , fadeLengthSmps{fadeLen}
            , bufferSize{buffSiz}
            , mixCurve{fadeLen/buffSiz}
            , tmpL{bufferSize}
            , tmpR{bufferSize}
            , progress{0}
            , mixStep{0}
            , mixIn{0}
            , mixOut{1}
            , mixInPrev{0}
            , mixOutPrev{0}
        {
            attach_instance(); // ensure old wavetable stays alive
        }
       ~XFadeDelegate()
        {
            detach_instance(); // one user less
        }
};




/* === Factory functions ===  */

inline WaveInterpolator* WaveInterpolator::create(bool cubic
                                                 ,float phase
                                                 ,bool stereo
                                                 ,fft::Waveform const& wave
                                                 ,float tableFreq)
{
    StereoInterpolatorBase* ipo;
    if (cubic)
        ipo = new CubicInterpolator(wave,tableFreq);
    else
        ipo = new LinearInterpolator(wave,tableFreq);

    return ipo->setStartPos(phase,stereo);
}


inline WaveInterpolator* WaveInterpolator::createXFader(function<void(void)> attachXFader
                                                       ,function<void(void)> detachXFader
                                                       ,function<void(WaveInterpolator*)> switchInterpolator
                                                       ,unique_ptr<WaveInterpolator> oldInterpolator
                                                       ,unique_ptr<WaveInterpolator> newInterpolator
                                                       ,size_t fadeLen, size_t buffSiz)
                                                     // Note: wrapped into unique_ptr to prevent memory leaks on error
{
    if (oldInterpolator and newInterpolator and fadeLen > 0)
        return new XFadeDelegate(attachXFader
                                ,detachXFader
                                ,switchInterpolator
                                ,move(oldInterpolator)
                                ,move(newInterpolator)
                                ,fadeLen
                                ,buffSiz);
    else
        return newInterpolator.release();
}


inline WaveInterpolator* WaveInterpolator::clone(WaveInterpolator const& orig)
{
    return orig.buildClone();
}

inline WaveInterpolator* WaveInterpolator::clone(unique_ptr<WaveInterpolator> const& orig)
{
    return orig? clone(*orig)
               : nullptr;
}


#endif /*WAVE_INTERPOLATOR_H*/
