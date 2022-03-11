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
    const fft::Waveform* table;
    float baseFreq;

    size_t posHiL;
    size_t posHiR;
    float posLo;

    public: // can be copy/move constructed, but not assigned...
       ~WaveInterpolator()                                   = default;
        WaveInterpolator(WaveInterpolator&&)                 = default;
        WaveInterpolator(WaveInterpolator const&)            = default;
        WaveInterpolator& operator=(WaveInterpolator&&)      = delete;
        WaveInterpolator& operator=(WaveInterpolator const&) = delete;

        WaveInterpolator(fft::Waveform const& wave, float freq)
            : table{&wave}
            , baseFreq{freq}
            , posHiL{0}
            , posHiR{0}
            , posLo{0}
        { }

        void useTable(fft::Waveform const& wave, float freq)
        {
            table = &wave;
            baseFreq = freq;
        }

        void setStartPos(float phase, bool stereo)
        {
            size_t size = table->size();
            posHiL = size_t(phase * (size-1));
            posHiR = stereo? (posHiL + size/2) % size
                           : posHiL;
            posLo = 0;
        }


        /////TODO call through virtual function

        void interpolateLinear(float *outl, float *outr, float freq, size_t cnt)
        {
            float speedFactor = freq / baseFreq;
            size_t incHi = size_t(floorf(speedFactor));
            float incLo = speedFactor - incHi;

            fft::Waveform const& smps = *table;
            size_t size = smps.size();
            for (size_t i = 0; i < cnt; ++i)
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

                outl[i] = smps[posHiL] * (1.0 - posLo) + smps[posHiL + 1] * posLo;
                outr[i] = smps[posHiR] * (1.0 - posLo) + smps[posHiR + 1] * posLo;
            }
        }

        void interpolateCubic(float *outl, float *outr, float freq, size_t cnt)
        {
            float speedFactor = freq / baseFreq;
            size_t incHi = size_t(floorf(speedFactor));
            float incLo = speedFactor - incHi;

            fft::Waveform const& smps = *table;
            size_t size = smps.size();
            float xm1, x0, x1, x2, a, b, c;
            for (size_t i = 0; i < cnt; ++i)
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
                xm1 = smps[posHiL];
                x0 = smps[posHiL + 1];
                x1 = smps[posHiL + 2];
                x2 = smps[posHiL + 3];
                a = (3.0 * (x0 - x1) - xm1 + x2) * 0.5;
                b = 2.0 * x1 + xm1 - (5.0 * x0 + x2) * 0.5;
                c = (x1 - xm1) * 0.5;
                outl[i] = (((a * posLo) + b) * posLo + c) * posLo + x0;
                // right
                xm1 = smps[posHiR];
                x0 = smps[posHiR + 1];
                x1 = smps[posHiR + 2];
                x2 = smps[posHiR + 3];
                a = (3.0 * (x0 - x1) - xm1 + x2) * 0.5;
                b = 2.0 * x1 + xm1 - (5.0 * x0 + x2) * 0.5;
                c = (x1 - xm1) * 0.5;
                outr[i] = (((a * posLo) + b) * posLo + c) * posLo + x0;
            }
        }

    private:
};

#endif /*WAVE_INTERPOLATOR_H*/
