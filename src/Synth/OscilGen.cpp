/*
    OscilGen.cpp - Waveform generator for ADnote

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011 Alan Calvert
    Copyright 2009 James Morris
    Copyright 2016-2019 Will Godfrey & others
    Copyright 2020 Kristian Amlie & others

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

#include <cmath>
#include <iostream>
#include <cassert>
#include <memory>
#include <vector>
#include <functional>

#include "Effects/Distorsion.h"
#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "Misc/NumericFuncs.h"
#include "Synth/OscilGen.h"

using func::power;
using std::vector;

namespace {// Implementation helpers
    inline float sqr(float v) { return v*v; }

    constexpr float CUTOFF = 1e-10;
    constexpr float LOW_LIMIT = 1e-5;
}


OscilGen::OscilGen(fft::Calc& fft_, Resonance* res_, SynthEngine* _synth, OscilParameters* params_)
    : params{params_}
    , synth{_synth}
    , fft{fft_}
    , tmpsmps{fft_.tableSize()}
    , outoscilSpectrum{fft.spectrumSize()}
    , oscilSpectrum{fft.spectrumSize()}
    , oscilupdate{*params}
    , res{res_}
    , randseed{1}
    , basePrng{}
    , harmonicPrng{}
{
    genDefaults();
}

void OscilGen::changeParams(OscilParameters* params_)
{
    params = params_;
    oscilupdate.changeParams(*params);
}

void OscilGen::defaults()
{
    params->defaults();
    genDefaults();
}

void OscilGen::genDefaults()
{
    oldbasefunc = 0;
    oldbasepar = 64;
    oldhmagtype = 0;
    oldwaveshapingfunction = 0;
    oldwaveshaping = 64;
    oldbasefuncmodulation = 0;
    oldharmonicshift = 0;
    oldbasefuncmodulationpar1 = 0;
    oldbasefuncmodulationpar2 = 0;
    oldbasefuncmodulationpar3 = 0;
    oldmodulation = 0;
    oldmodulationpar1 = 0;
    oldmodulationpar2 = 0;
    oldmodulationpar3 = 0;

    memset(hmag, 0, MAX_AD_HARMONICS * sizeof(float));
    memset(hphase, 0, MAX_AD_HARMONICS * sizeof(float));

    oscilSpectrum.reset();

    oldfilterpars = 0;
    oldsapars = 0;
    prepare();
}


void OscilGen::convert2sine()
{
    float mag[MAX_AD_HARMONICS], phase[MAX_AD_HARMONICS];
    fft::Waveform oscil(fft.tableSize());
    fft::Spectrum freqs(fft.spectrumSize());
    getWave(oscil, 1.0f);
    fft.smps2freqs(oscil, freqs);

    float max = 0.0f;

    mag[0] = 0;
    phase[0] = 0;
    assert (MAX_AD_HARMONICS < fft.spectrumSize());
    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
    {
        mag[i] = sqrtf(sqr(freqs.s(i+1)) + sqr(freqs.c(i+1)));
        phase[i] = atan2(freqs.c(i+1), freqs.s(i+1));
        if (max < mag[i])
            max = mag[i];
    }
    if (max < CUTOFF)
        max = 1.0;

    defaults();

    for (size_t i = 0; i < MAX_AD_HARMONICS - 1; ++i)
    {
        float newmag = mag[i] / max;
        float newphase = phase[i];

        params->Phmag[i] = 64 + int(newmag * 64.0);
        params->Phphase[i] = 64 - int(64.0 * newphase / PI);

        if (params->Phphase[i] > 127)
            params->Phphase[i] = 127;

        if (params->Phmag[i] == 64)
            params->Phphase[i] = 64;
    }
    prepare();
}


// Base Functions - START
float OscilGen::basefunc_pulse(float x, float a)
{
    return (fmodf(x, 1.0f) < a) ? -1.0f : 1.0f;
}


float OscilGen::basefunc_saw(float x, float a)
{
    if (a < LOW_LIMIT)
        a = LOW_LIMIT;
    else if (a > 0.99999f)
        a = 0.99999f;
    x = fmodf(x, 1.0f);
    if (x < a)
        return x / a * 2.0f - 1.0f;
    else
        return (1.0f - x) / (1.0f - a) * 2.0f - 1.0f;
}


float OscilGen::basefunc_triangle(float x, float a)
{
    x = fmodf(x + 0.25f, 1.0f);
    a = 1 - a;
    if (a < LOW_LIMIT)
        a = LOW_LIMIT;
    if (x < 0.5f)
        x = x * 4.0f - 1.0f;
    else
        x = (1.0f - x) * 4.0f - 1.0f;
    x /= -a;
    if (x < -1.0f)
        x = -1.0f;
    if (x > 1.0f)
        x = 1.0f;
    return x;
}


float OscilGen::basefunc_power(float x, float a)
{
    x = fmodf(x, 1.0f);
    if (a < LOW_LIMIT)
        a = LOW_LIMIT;
    else if (a > 0.99999f)
        a = 0.99999f;
    return powf(x, (expf((a - 0.5f) * 10.0f))) * 2.0f - 1.0f;
}


float OscilGen::basefunc_gauss(float x, float a)
{
    x = fmodf(x, 1.0f) * 2.0f - 1.0f;
    if (a < LOW_LIMIT)
        a = LOW_LIMIT;
    return expf(-x * x * (expf(a * 8.0f) + 5.0f)) * 2.0f - 1.0f;
}


float OscilGen::basefunc_diode(float x, float a)
{
    if (a < LOW_LIMIT)
        a = LOW_LIMIT;
    else if (a > 0.99999f)
        a = 0.99999f;
    a = a * 2.0f - 1.0f;
    x =cosf((x + 0.5f) * TWOPI) - a;
    if (x < 0.0f)
        x = 0.0f;
    return x / (1.0f - a) * 2.0f - 1.0f;
}


float OscilGen::basefunc_abssine(float x, float a)
{
    x = fmodf(x, 1.0f);
    if (a < LOW_LIMIT)
        a = LOW_LIMIT;
    else if (a > 0.99999f)
        a = 0.99999f;
    return sinf(powf(x, (expf((a - 0.5f) * 5.0f))) * PI) * 2.0f - 1.0f;
}


float OscilGen::basefunc_pulsesine(float x, float a)
{
    if (a < LOW_LIMIT)
        a = LOW_LIMIT;
    x = (fmodf(x, 1.0f) - 0.5f) * expf((a - 0.5f) * logf(128.0f));
    if (x < -0.5f)
        x = -0.5f;
    else if (_SYS_::F2B(x))
        x = 0.5f;
    x = sinf(x * TWOPI);
    return x;
}


float OscilGen::basefunc_stretchsine(float x, float a)
{
    x = fmodf(x + 0.5f, 1.0f) * 2.0f - 1.0f;
    a =(a - 0.5f) * 4.0f;
    if (a > 0.0f)
        a *= 2.0f;
    a = power<3>(a);
    float b = powf(fabsf(x), a);
    if (x < 0.0f)
        b = -b;
    return -sinf(b * PI);
}


float OscilGen::basefunc_chirp(float x, float a)
{
    x = fmodf(x, 1.0f) * TWOPI;
    a = (a - 0.5f) * 4.0f;
    if (a < 0.0f)
        a *= 2.0f;
    a = power<3>(a);
    return sinf(x / 2.0f) * sinf(a * x * x);
}


float OscilGen::basefunc_absstretchsine(float x, float a)
{
    x = fmodf(x + 0.5f, 1.0f) * 2.0f - 1.0f;
    a = (a - 0.5f) * 9.0f;
    a = power<3>(a);
    float b = powf(fabsf(x), a);
    if (x < 0.0f)
        b = -b;
    return -powf(sinf(b * PI), 2.0f);
}


float OscilGen::basefunc_chebyshev(float x, float a)
{
    a = a * a * a * 30.0f + 1.0f;
    return cosf(acosf(x * 2.0f - 1.0f) * a);
}


float OscilGen::basefunc_sqr(float x, float a)
{
    a = a * a * a * a * 160.0f + 0.001f;
    return -atanf(sinf(x * TWOPI) * a);
}


float OscilGen::basefunc_spike(float x, float a)
{
    float b = a * 0.66666; // the width of the range: if a == 0.5, b == 0.33333

    if (x < 0.5)
    {
        if (x < (0.5 - (b / 2.0)))
            return 0.0;
        else
	{
            x = (x + (b / 2) - 0.5) * (2.0 / b); // shift to zero, and expand to range from 0 to 1
            return x * (2.0 / b); // this is the slope: 1 / (b / 2)
        }
    }
    else
    {
        if (x > (0.5 + (b / 2.0)))
            return 0.0;
        else
	{
            x = (x - 0.5) * (2.0 / b);
            return (1 - x) * (2.0 / b);
        }
    }
}


float OscilGen::basefunc_circle(float x, float a)
{
    // a is parameter: 0 -> 0.5 -> 1 // O.5 = circle
    float b, y;

    b = 2 - (a * 2); // b goes from 2 to 0
    x = x * 4;

    if (x < 2)
    {
        x = x - 1; // x goes from -1 to 1
        if ((x < -b) || (x > b))
            y = 0;
        else
            y = sqrt(1 - (pow(x, 2) / pow(b, 2)));  // normally * a^2, but a stays 1
    }
    else
    {
        x = x - 3; // x goes from -1 to 1 as well
        if ((x < -b) || (x > b))
            y = 0;
        else
            y = -sqrt(1 - (pow(x, 2) / pow(b, 2)));
    }
    return y;
}


float OscilGen::basefunc_hypsec(float x, float a)
{
    x = (fmodf(x, 1.0f) - 0.5f) * expf(1.2f * (a - 0.2f) * logf(128.0f));
    return 1.0f/coshf(x * PI);
}
// Base Functions - END


// Get the base function
void OscilGen::getbasefunction(fft::Waveform& smps)
{
    float par = (params->Pbasefuncpar + 0.5f) / 128.0f;
    if (params->Pbasefuncpar == 64)
        par = 0.5f;

    float basefuncmodulationpar1 = params->Pbasefuncmodulationpar1 / 127.0f;
    float basefuncmodulationpar2 = params->Pbasefuncmodulationpar2 / 127.0f;
    float basefuncmodulationpar3 = params->Pbasefuncmodulationpar3 / 127.0f;

    switch (params->Pbasefuncmodulation)
    {
        case 1:
            basefuncmodulationpar1 =
                (power<2>(basefuncmodulationpar1 * 5.0f) - 1.0f) / 10.0f;
            basefuncmodulationpar3 =
                floorf((power<2>(basefuncmodulationpar3 * 5.0f) - 1.0f));
            if (basefuncmodulationpar3 < 0.9999f)
                basefuncmodulationpar3 = -1.0f;
            break;

        case 2:
            basefuncmodulationpar1 =
                (power<2>(basefuncmodulationpar1 * 5.0f) - 1.0f) / 10.0f;
            basefuncmodulationpar3 =
                1.0f + floorf((power<2>(basefuncmodulationpar3 * 5.0f) - 1.0f));
            break;

        case 3:
            basefuncmodulationpar1 =
                (power<2>(basefuncmodulationpar1 * 7.0f) - 1.0f) / 10.0f;
            basefuncmodulationpar3 =
                0.01f + (power<2>(basefuncmodulationpar3 * 16.0f) - 1.0f) / 10.0f;
            break;

        default:
            break;
    }
    for (size_t i = 0; i < fft.tableSize(); ++i)
    {
        float t = float(i) / fft.tableSize();

        switch (params->Pbasefuncmodulation)
        {
            case 1:
                t = t * basefuncmodulationpar3 + sinf((t + basefuncmodulationpar2)
                        * TWOPI) * basefuncmodulationpar1; // rev
                break;

            case 2:
                t = t + sinf((t * basefuncmodulationpar3 + basefuncmodulationpar2)
                        * TWOPI) * basefuncmodulationpar1; // sine
                break;

            case 3:
                t = t + powf(((1.0f - cosf((t + basefuncmodulationpar2) * TWOPI))
                        * 0.5f), basefuncmodulationpar3) * basefuncmodulationpar1; // power
                break;

            default:
                break;
            }
            t = t - floorf(t);

            switch (params->Pcurrentbasefunc)
            {
                case OSCILLATOR::wave::triangle:
                    smps[i] = basefunc_triangle(t, par);
                    break;

                case OSCILLATOR::wave::pulse:
                    smps[i] = basefunc_pulse(t, par);
                    break;

                case OSCILLATOR::wave::saw:
                    smps[i] = basefunc_saw(t, par);
                    break;

                case OSCILLATOR::wave::power:
                    smps[i] = basefunc_power(t, par);
                    break;

                case OSCILLATOR::wave::gauss:
                    smps[i] = basefunc_gauss(t, par);
                    break;

                case OSCILLATOR::wave::diode:
                    smps[i] = basefunc_diode(t, par);
                    break;

                case OSCILLATOR::wave::absSine:
                    smps[i] = basefunc_abssine(t, par);
                    break;

                case OSCILLATOR::wave::pulseSine:
                    smps[i] = basefunc_pulsesine(t, par);
                    break;

                case OSCILLATOR::wave::stretchSine:
                    smps[i] = basefunc_stretchsine(t, par);
                    break;

                case OSCILLATOR::wave::chirp:
                    smps[i] = basefunc_chirp(t, par);
                    break;

                case OSCILLATOR::wave::absStretchSine:
                    smps[i] = basefunc_absstretchsine(t, par);
                    break;

                case OSCILLATOR::wave::chebyshev:
                    smps[i] = basefunc_chebyshev(t, par);
                    break;

                case OSCILLATOR::wave::square:
                    smps[i] = basefunc_sqr(t, par);
                    break;

                case OSCILLATOR::wave::spike:
                    smps[i] = basefunc_spike(t, par);
                    break;

                case OSCILLATOR::wave::circle:
                    smps[i] = basefunc_circle(t, par);
                    break;

                case OSCILLATOR::wave::hyperSec:
                    smps[i] = basefunc_hypsec(t, par);
                    break;

                default: // sine
                    smps[i] = -sinf(TWOPI * (i) / fft.tableSize());
                    break;
        }
    }
}


// Filter the oscillator
void OscilGen::oscilfilter()
{
    if (params->Pfiltertype == 0)
        return;
    float par = 1.0f - params->Pfilterpar1 / 128.0f;
    float par2 = params->Pfilterpar2 / 127.0f;
    float max = 0.0f;
    float p2;
    float x;
    size_t lenSpectrum = oscilSpectrum.size();

    for (size_t i = 1; i < lenSpectrum; ++i)
    {
        float gain = 1.0f;
        switch (params->Pfiltertype)
        {
            case 1:
            {
                gain = powf((1.0f - par * par * par * 0.99f), i); // lp
                float tmp = par2 * par2 * par2 * par2 * 0.5f + 0.0001f;
                if (gain < tmp)
                    gain = powf(gain, 10.0f) / powf(tmp, 9.0f);
                break;
            }
            case 2:
            {
                gain = 1.0f - powf((1.0f - par * par), float(i + 1)); // hp1
                gain = powf(gain, (par2 * 2.0f + 0.1f));
                break;
            }
            case 3:
            {
                if (par < 0.2f)
                    par = par * 0.25f + 0.15f;
                gain = 1.0f - powf(1.0f - par * par * 0.999f + 0.001f,
                                 i * 0.05f * i + 1.0f); // hp1b
                float tmp = power<5>((par2 * 2.0f));
                gain = powf(gain, tmp);
                break;
            }
            case 4:
            {
                gain = (i + 1) - power<2>(((1.0f - par) * 7.5f)); // bp1
                gain = 1.0f / (1.0f + gain * gain / (i + 1.0f));
                float tmp = power<5>((par2 * 2.0f));
                gain = powf(gain, tmp);
                if (gain < LOW_LIMIT)
                    gain = LOW_LIMIT;
                break;
            }
            case 5:
            {
                gain = i + 1 - power<2>((1.0f - par) * 7.5f); // bs1
                gain = powf(atanf(gain / (i / 10.0f + 1.0f)) / 1.57f, 6.0f);
                gain = powf(gain, (par2 * par2 * 3.9f + 0.1f));
                break;
            }
            case 6:
            {
                gain = (i + 1 > power<2>((1.0f - par) * 10.0f) ? 0.0f : 1.0f)
                            * par2 + (1.0f - par2); // lp2
                break;
            }
            case 7:
            {
                gain = (i + 1 > power<2>((1.0f - par) * 7.0f) ? 1.0f : 0.0f)
                        * par2 + (1.0f - par2); // hp2
                if (params->Pfilterpar1 == 0)
                    gain = 1.0f;
                break;
            }
            case 8:
            {
                gain = (fabsf(power<2>((1.0f - par) * 7.0f) - i) > i / 2 + 1 ? 0.0f : 1.0f)
                        * par2 + (1.0f - par2); // bp2
                break;
            }
            case 9:
            {
                gain = (fabsf(power<2>((1.0f - par) * 7.0f) - i) < i / 2 + 1 ? 0.0f : 1.0f)
                        * par2 + (1.0f - par2); // bs2
                break;
            }
            case 10:
            {
                float tmp = power<5>(par2 * 2.0f - 1.0f);
                tmp = powf((i / 32.0f), tmp) * 32.0f;
                if (params->Pfilterpar2 == 64)
                    tmp = i;
                gain = cosf(par * par * HALFPI * tmp); // cos
                gain *= gain;
                break;
            }
            case 11:
            {
                float tmp = power<5>(par2 * 2.0f - 1.0f);
                tmp = powf((i / 32.0f), tmp) * 32.0f;
                if (params->Pfilterpar2 == 64)
                    tmp = i;
                gain = sinf(par * par * HALFPI * tmp); // sin
                gain *= gain;
                break;
            }
            case 12:
            {
                p2 = 1.0f - par + 0.2f;
                x = i / (64.0f * p2 * p2);
                x = (x > 1.0f) ? 1.0f : x;
                float tmp = powf(1.0f - par2, 2.0f);
                gain = cosf(x * PI) * (1.0f - tmp) + 1.01f + tmp; // low shelf
                break;
            }
            case 13:
            {
                gain = 1.0f;
                if (i == size_t(power<2>(((1.0f - par) * 7.2f))))
                    gain = power<2>(par2 * par2 * 8.0f);
                break;
            }
        }

        oscilSpectrum.s(i) *= gain;
        oscilSpectrum.c(i) *= gain;
        float tmp = sqr(oscilSpectrum.s(i)) + sqr(oscilSpectrum.c(i));
        if (max < tmp)
            max = tmp;
    }

    max = sqrtf(max);
    if (max < CUTOFF)
        max = 1.0f;
    float imax = 1.0f / max;
    for (size_t i = 1; i < lenSpectrum; ++i)
    {
        oscilSpectrum.s(i) *= imax;
        oscilSpectrum.c(i) *= imax;
    }
}


/* Ensure the base function spectrum in the OscilParameters
 * matches the current parameter settings; possibly regenerate
 * this spectrum when using one of the predefined base functions.
 * Remarks:
 * - a "user base function" (generated with OscilGen::useasbase())
 *   will be retained as-is and possibly persisted/loaded from XML.
 * - this function abuses tmpsmps and oscilSpectrum as a temporary
 *   working space; since it is only ever called from OscilGen::prepare()
 *   the oscilSpectrium will be restored / updated immediately afterwards.
 */
void OscilGen::changebasefunction()
{
    if (params->Pcurrentbasefunc != OSCILLATOR::wave::user)
    {
        if (params->Pcurrentbasefunc == OSCILLATOR::wave::sine)
        {// in this case basefuncSpectrum is not used
            oscilSpectrum.reset();
        }
        else
        {// generate spectrum for predefined base function
            getbasefunction(tmpsmps);
            fft.smps2freqs(tmpsmps, oscilSpectrum);
            oscilSpectrum.c(0) = 0.0f; // DC offset
        }
        params->updatebasefuncSpectrum(oscilSpectrum);
    }// note: no update in case of "user" base function

    oldbasefunc = params->Pcurrentbasefunc;
    oldbasepar = params->Pbasefuncpar;
    oldbasefuncmodulation = params->Pbasefuncmodulation;
    oldbasefuncmodulationpar1 = params->Pbasefuncmodulationpar1;
    oldbasefuncmodulationpar2 = params->Pbasefuncmodulationpar2;
    oldbasefuncmodulationpar3 = params->Pbasefuncmodulationpar3;
}


// Waveshape
void OscilGen::waveshape()
{
    oldwaveshapingfunction = params->Pwaveshapingfunction;
    oldwaveshaping = params->Pwaveshaping;
    if (params->Pwaveshapingfunction == 0)
        return;

    size_t eighth_i = fft.tableSize() / 8;
    float eighth_f = float(fft.tableSize()) / 8.0f;
    size_t len = fft.spectrumSize();

    oscilSpectrum.c(0) = 0.0f; // remove the DC
    // reduce the amplitude of the freqs near the nyquist
    for (size_t i = 1; i < eighth_i; ++i)
    {
        float damp = float(i) / eighth_f;
        oscilSpectrum.s(len - i) *= damp;
        oscilSpectrum.c(len - i) *= damp;
    }
    fft.freqs2smps(oscilSpectrum, tmpsmps);

    // Normalize
    float max = 0.0f;
    for (size_t i = 0; i < fft.tableSize(); ++i)
        if (max < fabsf(tmpsmps[i]))
            max = fabsf(tmpsmps[i]);
    if (max < CUTOFF)
        max = 1.0f;
    max = 1.0f / max;
    for (size_t i = 0; i < fft.tableSize(); ++i)
        tmpsmps[i] *= max;

    float* rawData = &tmpsmps[0];  // TODO: switch relevant buffers in SynthEngine also to fft::Waveform and automatic memory management

    // Do the waveshaping
    waveShapeSmps(fft.tableSize(), rawData, params->Pwaveshapingfunction, params->Pwaveshaping);

    fft.smps2freqs(tmpsmps, oscilSpectrum); // perform FFT
}


// Do the Frequency Modulation of the Oscil
void OscilGen::modulation()
{
    oldmodulation = params->Pmodulation;
    oldmodulationpar1 = params->Pmodulationpar1;
    oldmodulationpar2 = params->Pmodulationpar2;
    oldmodulationpar3 = params->Pmodulationpar3;
    if (params->Pmodulation == 0)
        return;

    float modulationpar1 = params->Pmodulationpar1 / 127.0f;
    float modulationpar2 = 0.5 - params->Pmodulationpar2 / 127.0f;
    float modulationpar3 = params->Pmodulationpar3 / 127.0f;

    switch (params->Pmodulation)
    {
        case 1:
            modulationpar1 = (power<2>(modulationpar1 * 7.0f) - 1.0f) / 100.0f;
            modulationpar3 = floorf((power<2>(modulationpar3 * 5.0f) - 1.0f));
            if (modulationpar3 < 0.9999f)
                modulationpar3 = -1.0f;
            break;

        case 2:
            modulationpar1 = (power<2>(modulationpar1 * 7.0f) - 1.0f) / 100.0f;
            modulationpar3 = 1.0f + floorf((power<2>(modulationpar3 * 5.0f) - 1.0f));
            break;

        case 3:
            modulationpar1 = (power<2>(modulationpar1 * 9.0f) - 1.0f) / 100.0f;
            modulationpar3 = 0.01f + (power<2>(modulationpar3 * 16.0f) - 1.0f) / 10.0f;
            break;
    }

    size_t eighth_i = fft.tableSize() / 8;
    float eighth_f = float(fft.tableSize()) / 8.0f;
    size_t len = fft.spectrumSize();

    oscilSpectrum.c(0) = 0.0f; // remove the DC
    // reduce the amplitude of the freqs near the nyquist
    for (size_t i = 1; i < eighth_i; ++i)
    {
        float damp = float(i) / eighth_f;
        oscilSpectrum.s(len - i) *= damp;
        oscilSpectrum.c(len - i) *= damp;
    }
    fft.freqs2smps(oscilSpectrum, tmpsmps);
    size_t extra_points = 2;
    float *in = new float[fft.tableSize() + extra_points];

    // Normalize
    float max = 0.0f;
    for (size_t i = 0; i < fft.tableSize(); ++i)
    {
        float absx = fabsf(tmpsmps[i]);
        if (max < absx)
            max = absx;
    }
    if (max < CUTOFF)
        max = 1.0f;
    max = 1.0f / max;
    for (size_t i = 0; i < fft.tableSize(); ++i)
        in[i] = tmpsmps[i] * max;
    for (size_t i = 0; i < extra_points; ++i)
        in[i + fft.tableSize()] = tmpsmps[i] * max;

    // Do the modulation
    for (size_t i = 0 ; i < fft.tableSize(); ++i)
    {
        float t = float(i) / float(fft.tableSize());
        switch (params->Pmodulation)
        {
            case 1:
                t = t * modulationpar3 + sinf((t + modulationpar2) * TWOPI)
                    * modulationpar1; // rev
                break;

            case 2:
                t = t + sinf((t * modulationpar3 + modulationpar2) * TWOPI)
                    * modulationpar1; // sine
                break;

            case 3:
                t = t + powf(((1.0f - cosf((t + modulationpar2) * TWOPI))
                    * 0.5f), modulationpar3) * modulationpar1; // power
                break;
        }

        t = (t - floorf(t)) * float(fft.tableSize());

        int poshi = int(t);
        float poslo = t - poshi;

        tmpsmps[i] = in[poshi] * (1.0f - poslo) + in[poshi + 1] * poslo;
    }

    delete [] in;
    fft.smps2freqs(tmpsmps, oscilSpectrum); // perform FFT
}


// Adjust the spectrum
void OscilGen::spectrumadjust()
{
    if (params->Psatype == 0)
        return;
    float par = params->Psapar / 127.0f;
    switch (params->Psatype)
    {
        case 1:
            par = 1.0f - par * 2.0f;
            if (par >= 0.0f)
                par = power<5>(par);
            else
                par = power<8>(par);
            break;

        case 2:
            par = power<10>((1.0f - par) * 3.0f) * 0.25f;
            break;

        case 3:
            par = power<10>((1.0f - par) * 3.0f) * 0.25f;
            break;
    }

    float max = 0.0f;
    size_t len = oscilSpectrum.size();
    for (size_t i = 0; i < len; ++i)
    {
        float tmp = sqr(oscilSpectrum.c(i)) + sqr(oscilSpectrum.s(i));
        if (max < tmp)
            max = tmp;
    }
    max = 2.0f * sqrtf(max) / fft.tableSize();  ////TODO why factor 2 here?
    if (max < CUTOFF)
        max = 1.0;

    for (size_t i = 0; i < len; ++i)
    {
        float mag = sqrtf(sqr(oscilSpectrum.s(i)) + sqr(oscilSpectrum.c(i))) / max;
        float phase = atan2f(oscilSpectrum.s(i), oscilSpectrum.c(i));

        switch (params->Psatype)
        {
            case 1:
                mag = powf(mag, par);
                break;
            case 2:
                if (mag < par)
                    mag = 0.0f;
                break;
            case 3:
                mag /= par;
                if (mag > 1.0f)
                    mag = 1.0f;
                break;
        }
        oscilSpectrum.c(i) = mag * cosf(phase);
        oscilSpectrum.s(i) = mag * sinf(phase);
    }
}


void OscilGen::shiftharmonics()
{
    if (params->Pharmonicshift == 0)
        return;

    float hc, hs;
    size_t len = oscilSpectrum.size();
    int harmonicshift = -params->Pharmonicshift;

    if (harmonicshift > 0)
    {
        for (size_t j = len-1; j > 0; j--)
        {
            int oldh = j - harmonicshift;
            if (oldh < 1)
                hc = hs = 0.0f;
            else
            {
                hc = oscilSpectrum.c(oldh);
                hs = oscilSpectrum.s(oldh);
            }
            oscilSpectrum.c(j) = hc;
            oscilSpectrum.s(j) = hs;
        }
    }
    else
    {
        for (size_t i = 1; i < len; ++i)
        {
            size_t oldh = i + abs(harmonicshift);
            if (oldh >= len)
                hc = hs = 0.0f;
            else
            {
                hc = oscilSpectrum.c(oldh);
                hs = oscilSpectrum.s(oldh);
                if (fabsf(hc) < CUTOFF)
                    hc = 0.0f;
                if (fabsf(hs) < CUTOFF)
                    hs = 0.0f;
            }

            oscilSpectrum.c(i) = hc;
            oscilSpectrum.s(i) = hs;
        }
    }

    oscilSpectrum.c(0) = 0.0f;
}


/* Brings the pseudo random generators within this OscilGen instance into a reproducible state.
 * The basePrng is (re)seeded through this function, called from prepare() and thus when a new
 * OscilGen instance is created, or when resetting to defaults prior to loading a preset.
 * With each NoteON, a new randseed is drawn from this basePrng, and that local randseed is
 * used for each call to get() to reset the harmonicPrng. Since NoteON happens at random times,
 * after playing more than one note the relation between SynthEngine::prng and OscilGen::basePrng
 * is essentially random.
 * Note: reseed(int) is also used for automated testing, see SynthEngine::setReproducibleState(int) */
void OscilGen::reseed(int value)
{
    basePrng.init(value);
    newrandseed();
    resetHarmonicPrng();
}



// Prepare the Oscillator
void OscilGen::prepare()
{
    // reseed local PRNGs from SynthEngine PRNG
    reseed(synth->randomINT() + INT_MAX/2);

    changebasefunction();

    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
        hphase[i] = (params->Phphase[i] - 64.0f) / 64.0f * PI / (i + 1);

    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
    {
        float hmagnew = 1.0f - fabsf(params->Phmag[i] / 64.0f - 1.0f);
        switch (params->Phmagtype)
        {
            case 1:
                hmag[i] = expf(hmagnew * logf(0.01f));
                break;

            case 2:
                hmag[i] = expf(hmagnew * logf(0.001f));
                break;

            case 3:
                hmag[i] = expf(hmagnew * logf(0.0001f));
                break;

            case 4:
                hmag[i] = expf(hmagnew * logf(0.00001f));
                break;

            default:
                hmag[i] = 1.0f - hmagnew;
                break;
        }

        if (params->Phmag[i] < 64)
            hmag[i] = -hmag[i];
    }

    // remove the harmonics where Phmag[i]==64
    for (size_t i = 0; i < MAX_AD_HARMONICS; ++i)
        if (params->Phmag[i] == 64)
            hmag[i] = 0.0f;

    size_t len = oscilSpectrum.size();
    assert (MAX_AD_HARMONICS < len);
    oscilSpectrum.reset();
    if (params->Pcurrentbasefunc == OSCILLATOR::wave::sine)
    {   // the sine case
        for (size_t i = 0; i < MAX_AD_HARMONICS; ++i)
        {
            oscilSpectrum.c(i+1) = -hmag[i] * sinf(hphase[i] * (i+1)) / 2.0f;
            oscilSpectrum.s(i+1) =  hmag[i] * cosf(hphase[i] * (i+1)) / 2.0f;
        }
    }
    else
    {
        for (size_t j = 0; j < MAX_AD_HARMONICS; ++j)
        {
            if (params->Phmag[j] == 64)
                continue;
            for (size_t i = 1; i < len; ++i)
            {
                size_t k = i * (j + 1);
                if (k >= len)
                    break;
                float a = params->getbasefuncSpectrum().c(i);
                float b = params->getbasefuncSpectrum().s(i);
                float c = hmag[j] * cosf(hphase[j] * k);
                float d = hmag[j] * sinf(hphase[j] * k);
                oscilSpectrum.c(k) += a * c - b * d;
                oscilSpectrum.s(k) += a * d + b * c;
            }
        }
    }

    if (params->Pharmonicshiftfirst)
        shiftharmonics();

    if (params->Pfilterbeforews == 0)
    {
        waveshape();
        oscilfilter();
    }
    else
    {
        oscilfilter();
        waveshape();
    }

    modulation();
    spectrumadjust();
    if (!params->Pharmonicshiftfirst)
        shiftharmonics();

    oscilSpectrum.c(0) = 0.0f;

    oldhmagtype = params->Phmagtype;
    oldharmonicshift = params->Pharmonicshift + params->Pharmonicshiftfirst * 256;
}


namespace { // Implementation details...

using Accessor = std::function<float&(size_t)>;

inline void adaptiveharmonic(Accessor spec, size_t size,
                             float currFreq, unsigned char bfreq,
                             unsigned char type, unsigned char ppow, unsigned char ppar)
{
    if (type == 0)
        return;// adaptive harmonics switched OFF

    assert(currFreq >= 1.0);
    assert(size > 0);
    std::unique_ptr<float[]> inf{new float[size]};
    for (size_t i = 0; i < size; ++i)
    {
        inf[i] = spec(i);
        spec(i) = 0.0f;
    }
    inf[0] = 0.0f;

    float adapted = 0.0f;
    float baseFreq = 30.0f * power<10>(bfreq / 128.0f);
    float power = (ppow + 1.0f) / 101.0f;

    float rap = currFreq / baseFreq;

    rap = powf(rap, power);

    bool down = false;
    if (rap > 1.0f)
    {
        rap = 1.0f / rap;
        down = true;
    }

    for (size_t i = 0; i < size - 2; ++i)
    {
        float h = i * rap;
        size_t high(h);
        float low = fmodf(h, 1.0f);

        if (high >= size - 2)
            break;

        if (down)
        {
            spec(high)   += inf[i] * (1.0f - low);
            spec(high+1) += inf[i] * low;
        }
        else
        {
            adapted = inf[high] * (1.0f - low) + inf[high+1] * low;
            if (fabsf(adapted) < CUTOFF)
                adapted = 0.0f;
            if (i == 0)
            {   //correct the amplitude of the first harmonic
                adapted *= rap;
            }
            spec(i) = adapted;
        }
    }

    spec(1) += spec(0);
    spec(0) =  0.0f;

    if (type <= 1)
        return;

    //-----Implant the extended spectrum onto the base spectrum------

    // "Padaptiveharmonics" == type of adaptive spectrum to add
    // Values: 0==OFF(default), 1=ON, 2="Square", 3="2xSub", 4="2xAdd", 3xSub, 3xAdd, 4xSub, 4xAdd
    float fade = 1.0f - powf((1.0f - 0.01f * ppar), 1.5f);

    for (size_t i = 1; i < size; ++i)
    {
        inf[i] = spec(i) * fade;
        spec(i) = spec(i) * (1.0f - fade);
    }

    if (type == 2)
    {   // "Square" : enforce the even partials
        for (size_t i = 1; i < size; ++i)
            if (((i-1) % 2) == 0)
                spec(i) += inf[i]; // i=1 corresponds to the fundamental,...
    }
    else
    {
        // handle all other modes
        int nh = (type - 3) / 2 + 2;
        int sub_vs_add = (type - 3) % 2;
        if (sub_vs_add == 0)
        {
            for (size_t i = 1; i < size; ++i)
            {
                if ((i % nh) == 0)
                {
                    spec(i) += inf[i];
                }
            }
        }
        else
        {
            for (size_t i = 1; i < (size-1) / nh; ++i)
                spec(nh*i) += inf[i];
        }
    }
}
}//(End) implementation details (adaptive harmonics)




// Get the oscillator function
void OscilGen::getWave(fft::Waveform& smps, float freqHz, bool applyResonance, bool forGUI)
{
    bool forPAD = false;
    buildSpectrum(freqHz, applyResonance, forGUI, forPAD);
    fft.freqs2smps(outoscilSpectrum, smps);
    for (size_t i = 0; i < fft.tableSize(); ++i)
        smps[i] *= 0.25f; // correct the amplitude
}

// Get the current spectrum for rendering in PADSynth (synth->halfoscilsize)
// Note: Spectrum slot=0 (DC-Offset) will be discarded.
//       In the result, index=0 is the fundamental.
//       See PADnoteParameters::generatespectrum_otherModes()
vector<float> OscilGen::getSpectrumForPAD(float freqHz)
{
    bool applyResonance = false;
    bool forGUI = false;
    bool forPAD = true;
    buildSpectrum(freqHz, applyResonance, forGUI, forPAD);

    vector<float> harmonics(oscilSpectrum.size()); // zero-init
    for (size_t i = 1; i < outoscilSpectrum.size(); ++i)
        harmonics[i-1] = sqrtf(sqr(outoscilSpectrum.c(i)) + sqr(outoscilSpectrum.s(i)));

    return harmonics;
}


// Core implementation of OscilGen
// - possibly prepare() will be called to generate the raw spectrum
// - typically invoked for each buffer to generate the Wavetable
//   including current phase randomisation
// - also used to generate the base spectrum for PADsynth
void OscilGen::buildSpectrum(float freqHz, bool applyResonance, bool forGUI, bool forPAD)
{
    assert(freqHz > 0.0);
    if (oldbasepar != params->Pbasefuncpar
        || oldbasefunc != params->Pcurrentbasefunc
        || oldhmagtype != params->Phmagtype
        || oldwaveshaping != params->Pwaveshaping
        || oldwaveshapingfunction != params->Pwaveshapingfunction)
        oscilupdate.forceUpdate();
    if (oldfilterpars != params->Pfiltertype * 256 + params->Pfilterpar1 + params->Pfilterpar2 * 65536
        + params->Pfilterbeforews * 16777216)
    {
        oscilupdate.forceUpdate();
        oldfilterpars =
            params->Pfiltertype * 256 + params->Pfilterpar1 + params->Pfilterpar2 * 65536 + params->Pfilterbeforews * 16777216;
    }
    if (oldsapars != params->Psatype * 256 + params->Psapar)
    {
        oscilupdate.forceUpdate();
        oldsapars = params->Psatype * 256 + params->Psapar;
    }

    if (oldbasefuncmodulation != params->Pbasefuncmodulation
        || oldbasefuncmodulationpar1 != params->Pbasefuncmodulationpar1
        || oldbasefuncmodulationpar2 != params->Pbasefuncmodulationpar2
        || oldbasefuncmodulationpar3 != params->Pbasefuncmodulationpar3)
        oscilupdate.forceUpdate();

    if (oldmodulation != params->Pmodulation
        || oldmodulationpar1 != params->Pmodulationpar1
        || oldmodulationpar2 != params->Pmodulationpar2
        || oldmodulationpar3 != params->Pmodulationpar3)
        oscilupdate.forceUpdate();

    if (oldharmonicshift != params->Pharmonicshift + params->Pharmonicshiftfirst * 256)
        oscilupdate.forceUpdate();

    if (oscilupdate.checkUpdated())
        prepare();

    // start harmonic randomisation from local randseed, drawn in ADnote::ADnote()
    // see also comment at OscilGen::reseed()
    resetHarmonicPrng();

    outoscilSpectrum.reset();

    size_t specLen = outoscilSpectrum.size();
    size_t nyquist = size_t(0.5f * synth->samplerate_f / freqHz) + 2;
    if (forPAD)
        nyquist = specLen;
    if (nyquist > specLen)
        nyquist = specLen;

    size_t realnyquist = nyquist;

    if (params->Padaptiveharmonics)
        nyquist = specLen;
    for (size_t i = 1; i < nyquist - 1; ++i)
    {
        outoscilSpectrum.c(i) = oscilSpectrum.c(i);
        outoscilSpectrum.s(i) = oscilSpectrum.s(i);
    }

    {// Generate adaptive harmonics
        unsigned char bfreq = params->Padaptiveharmonicsbasefreq;
        unsigned char type = params->Padaptiveharmonics;
        unsigned char ppow = params->Padaptiveharmonicspower;
        unsigned char ppar = params->Padaptiveharmonicspar;

        Accessor cosPart = [this](size_t i) -> float& { return outoscilSpectrum.c(i); };
        Accessor sinPart = [this](size_t i) -> float& { return outoscilSpectrum.s(i); };
        float currFreq = forGUI? 440.0f : freqHz;
        adaptiveharmonic(cosPart, specLen, currFreq, bfreq, type, ppow, ppar);
        adaptiveharmonic(sinPart, specLen, currFreq, bfreq, type, ppow, ppar);
    }

    nyquist = realnyquist;
    if (params->Padaptiveharmonics)
    {   // do the antialiasing in the case of adaptive harmonics
        for (size_t i = nyquist; i < specLen; ++i)
            outoscilSpectrum.s(i) = outoscilSpectrum.c(i) = 0.0f;
    }

    // Randomness (each harmonic), the block type is computed
    // in ADnote by setting start position according to this setting
    if (params->Prand > 64 && !forGUI && !forPAD)
    {
        float rnd, angle, a, b, c, d;
        rnd = PI * powf((params->Prand - 64.0f) / 64.0f, 2.0f);
        for (size_t i = 1; i < nyquist - 1; ++i)
        {   // to Nyquist only for AntiAliasing
            angle = rnd * i * harmonicPrng.numRandom();
            a = outoscilSpectrum.c(i);
            b = outoscilSpectrum.s(i);
            c = cosf(angle);
            d = sinf(angle);
            outoscilSpectrum.c(i) = a * c - b * d;
            outoscilSpectrum.s(i) = a * d + b * c;
        }
    }

    // Harmonic Amplitude Randomness
    if (!forGUI && !forPAD)
    {
        float power = params->Pamprandpower / 127.0f;
        float normalize = 1.0f / (1.2f - power);
        switch (params->Pamprandtype)
        {
            case 1:
                power = power * 2.0f - 0.5f;
                power = func::power<15>(power);
                for (size_t i = 1; i < nyquist - 1; ++i)
                {
                    float amp = powf(harmonicPrng.numRandom(), power) * normalize;
                    outoscilSpectrum.c(i) *= amp;
                    outoscilSpectrum.s(i) *= amp;
                }
                break;

            case 2:
                power = power * 2.0f - 0.5f;
                power = func::power<15>(power) * 2.0f;
                float rndfreq = TWOPI * harmonicPrng.numRandom();
                for (size_t i = 1 ; i < nyquist - 1; ++i)
                {
                    float amp = powf(fabsf(sinf(i * rndfreq)), power) * normalize;
                    outoscilSpectrum.c(i) *= amp;
                    outoscilSpectrum.s(i) *= amp;
                }
                break;
        }
    }

    if (applyResonance && !forGUI)
        res->applyres(nyquist - 1, outoscilSpectrum, freqHz);

    // Full RMS normalize
    float sum = 0;
    for (size_t j = 1; j < specLen; ++j)
    {
        sum += sqr(outoscilSpectrum.c(j)) + sqr(outoscilSpectrum.s(j));
    }
    if (sum < CUTOFF)
        sum = 1.0f;
    sum = 1.0f / sqrtf(sum);
    for (size_t j = 1; j < specLen; ++j)
    {
        outoscilSpectrum.c(j) *= sum;
        outoscilSpectrum.s(j) *= sum;
    }
}

int OscilGen::getPhase()
{
    if (params->Prand >= 64)
        return 0;

    int outpos;
    outpos = int(fft.tableSize() * (basePrng.numRandom() * 2.0f - 1.0f) * (params->Prand - 64.0f) / 64.0f);
    outpos = (outpos + 2 * fft.tableSize()) % fft.tableSize();
    return outpos;
}


// Current base function spectrum intensities for display in the UI
void OscilGen::getBasefuncSpectrumIntensities(size_t n, float *spc)
{
    size_t specLen = outoscilSpectrum.size();
    if (n > specLen)
        n = specLen;

    for (size_t i = 1; i < n; ++i)
    {
        if (params->Pcurrentbasefunc == OSCILLATOR::wave::sine)
            spc[i-1] = (i == 1) ? 1.0f : 0.0f;
        else
            spc[i-1] = sqrtf(sqr(params->getbasefuncSpectrum().c(i)) + sqr(params->getbasefuncSpectrum().s(i)));
    }
}


// Effective oscillator spectrum intensities for display in the UI
void OscilGen::getOscilSpectrumIntensities(size_t n, float* spc)
{
    size_t specLen = outoscilSpectrum.size();
    if (n > specLen)
        n = specLen;

    for (size_t i = 1; i < n; ++i)
        spc[i-1] = sqrtf(sqr(oscilSpectrum.c(i)) + sqr(oscilSpectrum.s(i)));

    // display of full OscilGen spectrum: show also the effect of adaptive harmonics

    uchar bfreq = params->Padaptiveharmonicsbasefreq;
    uchar type  = params->Padaptiveharmonics;
    uchar ppow  = params->Padaptiveharmonicspower;
    uchar ppar  = params->Padaptiveharmonicspar;

    Accessor accessLine = [spc](size_t i) -> float& { return spc[i]; };

    float currFreq = 440.0; // GUI display shows adaptive harmonics with dummy "current" frequency
    adaptiveharmonic(accessLine, n, currFreq, bfreq, type, ppow, ppar);

    /////TODO: do we really need the following side-effect on the spectrum stored in the UI's OscilGen?
    /////      might be just a consequence of the 'tricky' way the original code calculated the adaptive harmonics
    assert(n <= specLen);
    for (size_t i = 0; i < n; ++i)
        outoscilSpectrum.s(i) = outoscilSpectrum.c(i) = spc[i];
    for (size_t i = n; i < specLen; ++i)
        outoscilSpectrum.s(i) = outoscilSpectrum.c(i) = 0.0f;
}


// Convert the current oscillator spectrum into a
// "user base function", which can then be further mixed and processed.
void OscilGen::useasbase()
{
    params->updatebasefuncSpectrum(oscilSpectrum);
    oldbasefunc = params->Pcurrentbasefunc = OSCILLATOR::wave::user;
    prepare();
}


// Get the base function display for the "Osciloscope" in the UI
void OscilGen::displayBasefuncForGui(fft::Waveform& smps)
{
    if (params->Pcurrentbasefunc != OSCILLATOR::wave::sine)
    {
        fft.freqs2smps(params->getbasefuncSpectrum(), smps);
    }
    else
        getbasefunction(smps); // the sine case
}


// Get the current effective Oscillator waveform
// for display in the "Osciloscope" in the UI
void OscilGen::displayWaveformForGui(fft::Waveform& smps)
{
    float dummyFreq = 1.0;
    bool applyResonance = false;
    bool forGuiDisplay = true;
    OscilGen::getWave(smps, dummyFreq, applyResonance, forGuiDisplay);
}
