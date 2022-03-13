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


class WaveInterpolator
{
    protected:
        fft::Waveform const& table;
        const float baseFreq;
        const size_t size;

        size_t posHiL;
        size_t posHiR;
        float posLo;

        WaveInterpolator(fft::Waveform const& wave, float freq)
            : table{wave}
            , baseFreq{freq}
            , size{wave.size()}
            , posHiL{0}
            , posHiR{0}
            , posLo{0}
        { }

    public: // can be copy/move constructed, but not assigned...
        WaveInterpolator(WaveInterpolator&&)                 = default;
        WaveInterpolator(WaveInterpolator const&)            = default;
        WaveInterpolator& operator=(WaveInterpolator&&)      = delete;
        WaveInterpolator& operator=(WaveInterpolator const&) = delete;

        virtual ~WaveInterpolator() = default; // this is an interface


        float getCurrentPhase()  const
        {
            return (posHiL + posLo) / float(size);
        }

        void setStartPos(float phase, bool stereo)
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
        }


        void caculateSamples(float *smpL, float *smpR, float freq, size_t cntSmp)
        {
            float speedFactor = freq / baseFreq;
            size_t incHi = size_t(floorf(speedFactor));
            float  incLo = speedFactor - incHi;

            doCalculate(smpL,smpR, cntSmp, incHi,incLo);
        }


        /* build a concrete interpolator instance for stereo interpolation either cubic or linear */
        static WaveInterpolator* create(bool cubic, fft::Waveform const& wave, float tableFreq);
        static WaveInterpolator* clone(WaveInterpolator const& orig);

    protected:
        virtual void doCalculate(float*,float*,size_t, size_t incHi, float incLo)  =0;
        virtual WaveInterpolator* buildClone() const                               =0;
};



class LinearInterpolator
    : public WaveInterpolator
{
        void doCalculate(float* smpL
                        ,float* smpR
                        ,size_t cntSmp
                        ,size_t incHi
                        ,float incLo)  override
        {
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
        using WaveInterpolator::WaveInterpolator;
};


class CubicInterpolator
    : public WaveInterpolator
{
        void doCalculate(float* smpL
                        ,float* smpR
                        ,size_t cntSmp
                        ,size_t incHi
                        ,float incLo)  override
        {
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
        using WaveInterpolator::WaveInterpolator;
};



/* === Factory functions ===  */

inline WaveInterpolator* WaveInterpolator::create(bool cubic, fft::Waveform const& wave, float tableFreq)
{
    if (cubic)
        return new CubicInterpolator(wave,tableFreq);
    else
        return new LinearInterpolator(wave,tableFreq);
}

inline WaveInterpolator* WaveInterpolator::clone(WaveInterpolator const& orig)
{
    return orig.buildClone();
}


#endif /*WAVE_INTERPOLATOR_H*/
