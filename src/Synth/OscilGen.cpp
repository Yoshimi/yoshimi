/*
    OscilGen.cpp - Waveform generator for ADnote

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011 Alan Calvert
    Copyright 2009 James Morris
    Copyright 2016-2019 Will Godfrey & others
    Copyright 2020 Kristian Amlie & others

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

    This file is a derivative of a ZynAddSubFX original.

*/

#include <cmath>
#include <iostream>

using namespace std;

#include "Effects/Distorsion.h"
#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "Synth/OscilGen.h"

//char OscilGen::random_state[256];
//struct random_data OscilGen::random_buf;
//char OscilGen::harmonic_random_state[256];
//struct random_data OscilGen::harmonic_random_buf;

OscilGen::OscilGen(FFTwrapper *fft_, Resonance *res_, SynthEngine *_synth, OscilParameters *params_) :
    params(params_),
    synth(_synth),
    tmpsmps((float*)fftwf_malloc(_synth->oscilsize * sizeof(float))),
    fft(fft_),
    oscilupdate(params),
    res(res_),
    randseed(1)
{
    FFTwrapper::newFFTFREQS(&outoscilFFTfreqs, synth->halfoscilsize);
    if (!tmpsmps)
        synth->getRuntime().Log("Very bad error, failed to allocate OscilGen::tmpsmps");
    else
        memset(tmpsmps, 0, synth->oscilsize * sizeof(float));
    FFTwrapper::newFFTFREQS(&oscilFFTfreqs, synth->halfoscilsize);
    genDefaults();
}

OscilGen::~OscilGen()
{
    FFTwrapper::deleteFFTFREQS(&oscilFFTfreqs);
    if (tmpsmps)
    {
        fftwf_free(tmpsmps);
        FFTwrapper::deleteFFTFREQS(&outoscilFFTfreqs);
    }
}

void OscilGen::changeParams(OscilParameters *params_)
{
    params = params_;
    oscilupdate.changePresets(params);
}

void OscilGen::defaults(void)
{
    params->defaults();
    genDefaults();
}

void OscilGen::genDefaults(void)
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

    memset(oscilFFTfreqs.s, 0, synth->halfoscilsize * sizeof(float));
    memset(oscilFFTfreqs.c, 0, synth->halfoscilsize * sizeof(float));

    oldfilterpars = 0;
    oldsapars = 0;
    prepare();
}


void OscilGen::convert2sine()
{
    float mag[MAX_AD_HARMONICS], phase[MAX_AD_HARMONICS];
    float oscil[synth->oscilsize];
    FFTFREQS freqs;
    FFTwrapper::newFFTFREQS(&freqs, synth->halfoscilsize);
    get(oscil, -1.0f);
    FFTwrapper *fft = new FFTwrapper(synth->oscilsize);
    fft->smps2freqs(oscil, &freqs);
    delete fft;

    float max = 0.0f;

    mag[0] = 0;
    phase[0] = 0;
    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
    {
        mag[i] = sqrtf(powf(freqs.s[i + 1], 2.0f ) + powf(freqs.c[i + 1], 2.0f));
        phase[i] = atan2f(freqs.c[i + 1], freqs.s[i + 1]);
        if (max < mag[i])
            max = mag[i];
    }
    if (max < 0.00001f)
        max = 1.0f;

    defaults();

    for (int i = 0; i < MAX_AD_HARMONICS - 1; ++i)
    {
        float newmag = mag[i] / max;
        float newphase = phase[i];

        params->Phmag[i] = (int)(newmag * 64.0f) + 64;

        params->Phphase[i] = 64 - (int)(64.0f * newphase / PI);
        if (params->Phphase[i] > 127)
            params->Phphase[i] = 127;

        if (params->Phmag[i] == 64)
            params->Phphase[i] = 64;
    }
    FFTwrapper::deleteFFTFREQS(&freqs);
    prepare();
}


// Base Functions - START
float OscilGen::basefunc_pulse(float x, float a)
{
    return (fmodf(x, 1.0f) < a) ? -1.0f : 1.0f;
}


float OscilGen::basefunc_saw(float x, float a)
{
    if (a < 0.00001f)
        a = 0.00001f;
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
    if (a < 0.00001f)
        a = 0.00001f;
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
    if (a < 0.00001f)
        a = 0.00001f;
    else if (a > 0.99999f)
        a = 0.99999f;
    return powf(x, (expf((a - 0.5f) * 10.0f))) * 2.0f - 1.0f;
}


float OscilGen::basefunc_gauss(float x, float a)
{
    x = fmodf(x, 1.0f) * 2.0f - 1.0f;
    if (a < 0.00001f)
        a = 0.00001f;
    return expf(-x * x * (expf(a * 8.0f) + 5.0f)) * 2.0f - 1.0f;
}


float OscilGen::basefunc_diode(float x, float a)
{
    if (a < 0.00001f)
        a = 0.00001f;
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
    if (a < 0.00001f)
        a = 0.00001f;
    else if (a > 0.99999f)
        a = 0.99999f;
    return sinf(powf(x, (expf((a - 0.5f) * 5.0f))) * PI) * 2.0f - 1.0f;
}


float OscilGen::basefunc_pulsesine(float x, float a)
{
    if (a < 0.00001f)
        a = 0.00001f;
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
    a = powf(3.0f, a);
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
    a = powf(3.0f, a);
    return sinf(x / 2.0f) * sinf(a * x * x);
}


float OscilGen::basefunc_absstretchsine(float x, float a)
{
    x = fmodf(x + 0.5f, 1.0f) * 2.0f - 1.0f;
    a = (a - 0.5f) * 9.0f;
    a = powf(3.0f, a);
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
void OscilGen::getbasefunction(float *smps)
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
                (powf(2.0f, basefuncmodulationpar1 * 5.0f) - 1.0f) / 10.0f;
            basefuncmodulationpar3 =
                floorf((powf(2.0f, basefuncmodulationpar3 * 5.0f) - 1.0f));
            if (basefuncmodulationpar3 < 0.9999f)
                basefuncmodulationpar3 = -1.0f;
            break;

        case 2:
            basefuncmodulationpar1 =
                (powf(2.0f, basefuncmodulationpar1 * 5.0f) - 1.0f) / 10.0f;
            basefuncmodulationpar3 =
                1.0f + floorf((powf(2.0f, basefuncmodulationpar3 * 5.0f) - 1.0f));
            break;

        case 3:
            basefuncmodulationpar1 =
                (powf(2.0f, basefuncmodulationpar1 * 7.0f) - 1.0f) / 10.0f;
            basefuncmodulationpar3 =
                0.01f + (powf(2.0f, basefuncmodulationpar3 * 16.0f) - 1.0f) / 10.0f;
            break;

        default:
            break;
    }
    for (int i = 0; i < synth->oscilsize; ++i)
    {
        float t = (float)i / synth->oscilsize_f;

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
                    smps[i] = -sinf(TWOPI * (float)i / synth->oscilsize_f);
        }
    }
}


// Filter the oscillator
void OscilGen::oscilfilter(void)
{
    if (params->Pfiltertype == 0)
        return;
    float par = 1.0f - params->Pfilterpar1 / 128.0f;
    float par2 = params->Pfilterpar2 / 127.0f;
    float max = 0.0f;
    //float tmp = 0.0f;
    float p2;
    float x;

    for (int i = 1; i < synth->halfoscilsize; ++i)
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
                gain = 1.0f - powf((1.0f - par * par), (float)(i + 1)); // hp1
                gain = powf(gain, (par2 * 2.0f + 0.1f));
                break;
            }
            case 3:
            {
                if (par < 0.2f)
                    par = par * 0.25f + 0.15f;
                gain = 1.0f - powf(1.0f - par * par * 0.999f + 0.001f,
                                 i * 0.05f * i + 1.0f); // hp1b
                float tmp = powf(5.0f, (par2 * 2.0f));
                gain = powf(gain, tmp);
                break;
            }
            case 4:
            {
                gain = (i + 1) - powf(2.0f, ((1.0f - par) * 7.5f)); // bp1
                gain = 1.0f / (1.0f + gain * gain / (i + 1.0f));
                float tmp = powf(5.0f, (par2 * 2.0f));
                gain = powf(gain, tmp);
                if (gain < 1e-5f)
                    gain = 1e-5f;
                break;
            }
            case 5:
            {
                gain = i + 1 - powf(2.0f, (1.0f - par) * 7.5f); // bs1
                gain = powf(atanf(gain / (i / 10.0f + 1.0f)) / 1.57f, 6.0f);
                gain = powf(gain, (par2 * par2 * 3.9f + 0.1f));
                break;
            }
            case 6:
            {
                //float tmp = powf(par2, 0.33f);
                gain = (i + 1 > powf(2.0f, (1.0f - par) * 10.0f) ? 0.0f : 1.0f)
                            * par2 + (1.0f - par2); // lp2
                break;
            }
            case 7:
            {
                //float tmp = powf(par2, 0.33f);
                // tmp=1.0-(1.0-par2)*(1.0-par2);
                gain = (i + 1 > powf(2.0f, (1.0f - par) * 7.0f) ? 1.0f : 0.0f)
                        * par2 + (1.0f - par2); // hp2
                if (params->Pfilterpar1 == 0)
                    gain = 1.0f;
                break;
            }
            case 8:
            {
                //float tmp = powf(par2, 0.33f);
                // tmp=1.0-(1.0-par2)*(1.0-par2);
                gain = (fabsf(powf(2.0f, (1.0f - par) * 7.0f) - i) > i / 2 + 1 ? 0.0f : 1.0f)
                        * par2 + (1.0f - par2); // bp2
                break;
            }
            case 9:
            {
                //float tmp = powf(par2, 0.33f);
                gain = (fabsf(powf(2.0f, (1.0f - par) * 7.0f) - i) < i / 2 + 1 ? 0.0f : 1.0f)
                        * par2 + (1.0f - par2); // bs2
                break;
            }
            case 10:
            {
                float tmp = powf(5.0f, par2 * 2.0f - 1.0f);
                tmp = powf((i / 32.0f), tmp) * 32.0f;
                if (params->Pfilterpar2 == 64)
                    tmp = i;
                gain = cosf(par * par * HALFPI * tmp); // cos
                gain *= gain;
                break;
            }
            case 11:
            {
                float tmp = powf(5.0f, par2 * 2.0f - 1.0f);
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
                int tmp = (int)powf(2.0f, ((1.0f - par) * 7.2f));
                gain = 1.0f;
                if (i == tmp)
                    gain = powf(2.0f, par2 * par2 * 8.0f);
                break;
            }
        }

        oscilFFTfreqs.s[i] *= gain;
        oscilFFTfreqs.c[i] *= gain;
        float tmp = oscilFFTfreqs.s[i] * oscilFFTfreqs.s[i] + oscilFFTfreqs.c[i] * oscilFFTfreqs.c[i];
        if (max < tmp)
            max = tmp;
    }

    max = sqrtf(max);
    if (max < 1e-10f)
        max = 1.0f;
    float imax = 1.0f / max;
    for (int i = 1; i < synth->halfoscilsize; ++i)
    {
        oscilFFTfreqs.s[i] *= imax;
        oscilFFTfreqs.c[i] *= imax;
    }
}


// Change the base function
void OscilGen::changebasefunction(void)
{
    if (params->Pcurrentbasefunc > OSCILLATOR::wave::hyperSec)
    {
        // User base function
        memcpy(oscilFFTfreqs.c, params->getbasefuncFFTfreqs()->c,
               synth->halfoscilsize * sizeof(float));
        memcpy(oscilFFTfreqs.s, params->getbasefuncFFTfreqs()->s,
               synth->halfoscilsize * sizeof(float));
    }
    else if (params->Pcurrentbasefunc != 0)
    {
        getbasefunction(tmpsmps);
        fft->smps2freqs(tmpsmps, &oscilFFTfreqs);
        oscilFFTfreqs.c[0] = 0.0f;
    }
    else
    {
        for (int i = 0; i < synth->halfoscilsize; ++i)
            oscilFFTfreqs.s[i] = oscilFFTfreqs.c[i] = 0.0f;
        //in this case basefuncFFTfreqs_ are not used
    }
    params->updatebasefuncFFTfreqs(&oscilFFTfreqs, synth->halfoscilsize);

    oldbasefunc = params->Pcurrentbasefunc;
    oldbasepar = params->Pbasefuncpar;
    oldbasefuncmodulation = params->Pbasefuncmodulation;
    oldbasefuncmodulationpar1 = params->Pbasefuncmodulationpar1;
    oldbasefuncmodulationpar2 = params->Pbasefuncmodulationpar2;
    oldbasefuncmodulationpar3 = params->Pbasefuncmodulationpar3;
}


// Waveshape
void OscilGen::waveshape(void)
{
    oldwaveshapingfunction = params->Pwaveshapingfunction;
    oldwaveshaping = params->Pwaveshaping;
    if (params->Pwaveshapingfunction == 0)
        return;

    int eighth_i = synth->oscilsize / 8;
    float eighth_f = synth->oscilsize_f / 8.0f;

    oscilFFTfreqs.c[0] = 0.0f; // remove the DC
    // reduce the amplitude of the freqs near the nyquist
    for (int i = 1; i < eighth_i; ++i)
    {
        float tmp = (float)i / eighth_f;
        oscilFFTfreqs.s[synth->halfoscilsize - i] *= tmp;
        oscilFFTfreqs.c[synth->halfoscilsize- i] *= tmp;
    }
    fft->freqs2smps(&oscilFFTfreqs, tmpsmps);

    // Normalize
    float max = 0.0f;
    for (int i = 0; i < synth->oscilsize; ++i)
        if (max < fabsf(tmpsmps[i]))
            max = fabsf(tmpsmps[i]);
    if (max < 0.00001f)
        max = 1.0f;
    max = 1.0f / max;
    for (int i = 0; i < synth->oscilsize; ++i)
        tmpsmps[i] *= max;

    // Do the waveshaping
    waveShapeSmps(synth->oscilsize, tmpsmps, params->Pwaveshapingfunction, params->Pwaveshaping);

    fft->smps2freqs(tmpsmps, &oscilFFTfreqs); // perform FFT
}


// Do the Frequency Modulation of the Oscil
void OscilGen::modulation(void)
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
            modulationpar1 = (powf(2.0f, modulationpar1 * 7.0f) - 1.0f) / 100.0f;
            modulationpar3 = floorf((powf(2.0f, modulationpar3 * 5.0f) - 1.0f));
            if (modulationpar3 < 0.9999f)
                modulationpar3 = -1.0f;
            break;

        case 2:
            modulationpar1 = (powf(2.0f, modulationpar1 * 7.0f) - 1.0f) / 100.0f;
            modulationpar3 = 1.0f + floorf((powf(2.0f, modulationpar3 * 5.0f) - 1.0f));
            break;

        case 3:
            modulationpar1 = (powf(2.0f, modulationpar1 * 9.0f) - 1.0f) / 100.0f;
            modulationpar3 = 0.01f + (powf(2.0f, modulationpar3 * 16.0f) - 1.0f) / 10.0f;
            break;
    }

    int eighth_i = synth->oscilsize / 8;
    float eighth_f = synth->oscilsize_f / 8.0f;

    oscilFFTfreqs.c[0] = 0.0f; // remove the DC
    // reduce the amplitude of the freqs near the nyquist
    for (int i = 1; i < eighth_i; ++i)
    {
        float tmp = (float)i / eighth_f;
        oscilFFTfreqs.s[synth->halfoscilsize - i] *= tmp;
        oscilFFTfreqs.c[synth->halfoscilsize - i] *= tmp;
    }
    fft->freqs2smps(&oscilFFTfreqs, tmpsmps);
    int extra_points = 2;
    float *in = new float[synth->oscilsize + extra_points];

    // Normalize
    float max = 0.0f;
    for (int i = 0; i < synth->oscilsize; ++i)
    {
        float absx = fabsf(tmpsmps[i]);
        if (max < absx)
            max = absx;
    }
    if (max < 0.00001f)
        max = 1.0f;
    max = 1.0f / max;
    for (int i = 0; i < synth->oscilsize; ++i)
        in[i] = tmpsmps[i] * max;
    for (int i = 0; i < extra_points; ++i)
        in[i + synth->oscilsize] = tmpsmps[i] * max;

    // Do the modulation
    for (int i = 0 ; i < synth->oscilsize; ++i)
    {
        float t = (float)i / synth->oscilsize_f;
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

        t = (t - floorf(t)) * synth->oscilsize_f;

        int poshi = int(t);
        float poslo = t - poshi;

        tmpsmps[i] = in[poshi] * (1.0f - poslo) + in[poshi + 1] * poslo;
    }

    delete [] in;
    fft->smps2freqs(tmpsmps, &oscilFFTfreqs); // perform FFT
}


// Adjust the spectrum
void OscilGen::spectrumadjust(void)
{
    if (params->Psatype == 0)
        return;
    float par = params->Psapar / 127.0f;
    switch (params->Psatype)
    {
        case 1:
            par = 1.0f - par * 2.0f;
            if (par >= 0.0f)
                par = powf(5.0f, par);
            else
                par = powf(8.0f, par);
            break;

        case 2:
            par = powf(10.0f, (1.0f - par) * 3.0f) * 0.25f;
            break;

        case 3:
            par = powf(10.0f, (1.0f - par) * 3.0f) * 0.25f;
            break;
    }

    float max = 0.0f;
    for (int i = 0; i < synth->halfoscilsize; ++i)
    {
        float tmp = powf(oscilFFTfreqs.c[i], 2.0f) + powf(oscilFFTfreqs.s[i], 2.0f);
        if (max < tmp)
            max = tmp;
    }
    max = sqrtf(max) / synth->oscilsize_f * 2.0f;
    if (max < 1e-8f)
        max = 1.0f;

    for (int i = 0; i < synth->halfoscilsize; ++i)
    {
        float mag = sqrtf(powf(oscilFFTfreqs.s[i], 2) + powf(oscilFFTfreqs.c[i], 2.0f)) / max;
        float phase = atan2f(oscilFFTfreqs.s[i], oscilFFTfreqs.c[i]);

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
        oscilFFTfreqs.c[i] = mag * cosf(phase);
        oscilFFTfreqs.s[i] = mag * sinf(phase);
    }
}


void OscilGen::shiftharmonics(void)
{
    if (params->Pharmonicshift == 0)
        return;

    float hc, hs;
    int harmonicshift = -params->Pharmonicshift;

    if (harmonicshift > 0)
    {
        for (int i = synth->halfoscilsize - 2; i >= 0; i--)
        {
            int oldh = i - harmonicshift;
            if (oldh < 0)
                hc = hs = 0.0f;
            else
            {
                hc = oscilFFTfreqs.c[oldh + 1];
                hs = oscilFFTfreqs.s[oldh + 1];
            }
            oscilFFTfreqs.c[i + 1] = hc;
            oscilFFTfreqs.s[i + 1] = hs;
        }
    }
    else
    {
        for (int i = 0; i < synth->halfoscilsize - 1; ++i)
        {
            int oldh = i + abs(harmonicshift);
            if (oldh >= synth->halfoscilsize - 1)
                hc = hs = 0.0f;
            else
            {
                hc = oscilFFTfreqs.c[oldh + 1];
                hs = oscilFFTfreqs.s[oldh + 1];
                if (fabsf(hc) < 0.000001f)
                    hc = 0.0f;
                if (fabsf(hs) < 0.000001f)
                    hs = 0.0f;
            }

            oscilFFTfreqs.c[i + 1] = hc;
            oscilFFTfreqs.s[i + 1] = hs;
        }
    }

    oscilFFTfreqs.c[0]=0.0f;
}


// Prepare the Oscillator
void OscilGen::prepare(void)
{
    // with each NoteON, reseed local rand gen from global PRNG
    // Since NoteON happens at random times, this actually injects entropy,
    // because it is essentially random at which cycle position the global PRNG is just now
    prng.init(synth->randomINT() + INT_MAX/2);

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
    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
        if (params->Phmag[i] == 64)
            hmag[i] = 0.0f;

    for (int i = 0; i < synth->halfoscilsize; ++i)
        oscilFFTfreqs.c[i] = oscilFFTfreqs.s[i] = 0.0f;
    if (params->Pcurrentbasefunc == 0)
    {   // the sine case
        for (int i = 0; i < MAX_AD_HARMONICS; ++i)
        {
            oscilFFTfreqs.c[i + 1] =- hmag[i] * sinf(hphase[i] * (i + 1)) / 2.0f;
            oscilFFTfreqs.s[i + 1] = hmag[i] * cosf(hphase[i] * (i + 1)) / 2.0f;
        }
    }
    else
    {
        for (int j = 0; j < MAX_AD_HARMONICS; ++j)
        {
            if (params->Phmag[j] == 64)
                continue;
            for (int i = 1; i < synth->halfoscilsize; ++i)
            {
                int k = i * (j + 1);
                if (k >= synth->halfoscilsize)
                    break;
                float a = params->getbasefuncFFTfreqs()->c[i];
                float b = params->getbasefuncFFTfreqs()->s[i];
                float c = hmag[j] * cosf(hphase[j] * k);
                float d = hmag[j] * sinf(hphase[j] * k);
                oscilFFTfreqs.c[k] += a * c - b * d;
                oscilFFTfreqs.s[k] += a * d + b * c;
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

    oscilFFTfreqs.c[0] = 0.0f;

    oldhmagtype = params->Phmagtype;
    oldharmonicshift = params->Pharmonicshift + params->Pharmonicshiftfirst * 256;
}


void OscilGen::adaptiveharmonic(FFTFREQS f, float freq)
{
    if ((params->Padaptiveharmonics == 0) /*||(freq<1.0)*/)
        return;
    if (freq < 1.0f)
        freq = 440.0f;

    FFTFREQS inf;
    FFTwrapper::newFFTFREQS(&inf, synth->halfoscilsize);
    for (int i = 0; i < synth->halfoscilsize; ++i)
    {
        inf.s[i] = f.s[i];
        inf.c[i] = f.c[i];
        f.s[i] = f.c[i] = 0.0f;
    }
    inf.c[0] = inf.s[0] = 0.0f;

    float hc = 0.0f;
    float hs = 0.0f;
    float basefreq = 30.0f * powf(10.0f, params->Padaptiveharmonicsbasefreq / 128.0f);
    float power = (params->Padaptiveharmonicspower + 1.0f) / 101.0f;

    float rap = freq / basefreq;

    rap = powf(rap, power);

    bool down = false;
    if (rap > 1.0f)
    {
        rap = 1.0f / rap;
        down = true;
    }

    for (int i = 0; i < synth->halfoscilsize - 2; ++i)
    {
        float h = i * rap;
        int high = int(h);
        float low = fmodf(h, 1.0f);

        if (high >= synth->halfoscilsize - 2)
        {
            break;
        }
        else
        {
            if (down)
            {
                f.c[high] += inf.c[i] * (1.0f - low);
                f.s[high] += inf.s[i] * (1.0f - low);
                f.c[high + 1] += inf.c[i] * low;
                f.s[high + 1] += inf.s[i] * low;
            }
            else
            {
                hc = inf.c[high] * (1.0f - low) + inf.c[high + 1] * low;
                hs = inf.s[high] * (1.0f - low) + inf.s[high + 1] * low;
            }
            if (fabsf(hc) < 0.000001f)
                hc = 0.0f;
            if (fabsf(hs) < 0.000001f)
                hs = 0.0f;
        }

        if (!down)
        {
            if (i == 0)
            {   //corect the aplitude of the first harmonic
                hc *= rap;
                hs *= rap;
            }
            f.c[i] = hc;
            f.s[i] = hs;
        }
    }

    f.c[1] += f.c[0];
    f.s[1] += f.s[0];
    f.c[0] = f.s[0] = 0.0f;
    FFTwrapper::deleteFFTFREQS(&inf);
}


void OscilGen::adaptiveharmonicpostprocess(float *f, int size)
{
    if (params->Padaptiveharmonics <= 1)
        return;
    float *inf = new float[size];
    float par = params->Padaptiveharmonicspar * 0.01f;
    par = 1.0f - powf((1.0f - par), 1.5f);

    for (int i = 0; i < size; ++i)
    {
        inf[i] = f[i] * par;
        f[i] = f[i] * (1.0f - par);
    }

    if (params->Padaptiveharmonics == 2)
    {   // 2n+1
        for (int i = 0; i < size; ++i)
            if ((i % 2) == 0)
                f[i] += inf[i]; // i=0 pt prima armonica,etc.
    }
    else
    {
        // celelalte moduri
        int nh = (params->Padaptiveharmonics - 3) / 2 + 2;
        int sub_vs_add = (params->Padaptiveharmonics - 3) % 2;
        if (sub_vs_add == 0)
        {
            for (int i = 0; i < size; ++i)
            {
                if (((i + 1) % nh) == 0)
                {
                    f[i] += inf[i];
                }
            }
        }
        else
        {
            for (int i = 0; i < size / nh - 1; ++i)
                f[(i + 1) * nh - 1] += inf[i];
        }
    }
    delete [] inf;
}


// Get the oscillator function
void OscilGen::get(float *smps, float freqHz)
{
    this->get(smps, freqHz, 0);
}


// Get the oscillator function
void OscilGen::get(float *smps, float freqHz, int resonance)
{
    int nyquist;

    // randseed was drawn in ADnote::ADnote()
    // see also comment at top of OscilGen::prepare()
    harmonicPrng.init(randseed);

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

    memset(outoscilFFTfreqs.c, 0, synth->halfoscilsize * sizeof(float));
    memset(outoscilFFTfreqs.s, 0, synth->halfoscilsize * sizeof(float));
    nyquist = int(0.5f * synth->samplerate_f / fabsf(freqHz)) + 2;
    if (params->ADvsPAD)
        nyquist = synth->halfoscilsize;
    if (nyquist > synth->halfoscilsize)
        nyquist = synth->halfoscilsize;

    int realnyquist = nyquist;

    if (params->Padaptiveharmonics)
        nyquist = synth->halfoscilsize;
    for (int i = 1; i < nyquist - 1; ++i)
    {
        outoscilFFTfreqs.c[i] = oscilFFTfreqs.c[i];
        outoscilFFTfreqs.s[i] = oscilFFTfreqs.s[i];
    }

    adaptiveharmonic(outoscilFFTfreqs, freqHz);
    adaptiveharmonicpostprocess(&outoscilFFTfreqs.c[1], synth->halfoscilsize - 1);
    adaptiveharmonicpostprocess(&outoscilFFTfreqs.s[1], synth->halfoscilsize - 1);

    nyquist = realnyquist;
    if (params->Padaptiveharmonics)
    {   // do the antialiasing in the case of adaptive harmonics
        for (int i = nyquist; i < synth->halfoscilsize; ++i)
            outoscilFFTfreqs.s[i] = outoscilFFTfreqs.c[i] = 0;
    }

    // Randomness (each harmonic), the block type is computed
    // in ADnote by setting start position according to this setting
    if (params->Prand > 64 && freqHz >= 0.0 && !params->ADvsPAD)
    {
        float rnd, angle, a, b, c, d;
        rnd = PI * powf((params->Prand - 64.0f) / 64.0f, 2.0f);
        for (int i = 1; i < nyquist - 1; ++i)
        {   // to Nyquist only for AntiAliasing
            angle = rnd * i * harmonicPrng.numRandom();
            a = outoscilFFTfreqs.c[i];
            b = outoscilFFTfreqs.s[i];
            c = cosf(angle);
            d = sinf(angle);
            outoscilFFTfreqs.c[i] = a * c - b * d;
            outoscilFFTfreqs.s[i] = a * d + b * c;
        }
    }

    // Harmonic Amplitude Randomness
    if (freqHz > 0.1 && !params->ADvsPAD)
    {
        float power = params->Pamprandpower / 127.0f;
        float normalize = 1.0f / (1.2f - power);
        switch (params->Pamprandtype)
        {
            case 1:
                power = power * 2.0f - 0.5f;
                power = powf(15.0f, power);
                for (int i = 1; i < nyquist - 1; ++i)
                {
                    float amp = powf(harmonicPrng.numRandom(), power) * normalize;
                    outoscilFFTfreqs.c[i] *= amp;
                    outoscilFFTfreqs.s[i] *= amp;
                }
                break;

            case 2:
                power = power * 2.0f - 0.5f;
                power = powf(15.0f, power) * 2.0f;
                float rndfreq = TWOPI * harmonicPrng.numRandom();
                for (int i = 1 ; i < nyquist - 1; ++i)
                {
                    float amp = powf(fabsf(sinf(i * rndfreq)), power) * normalize;
                    outoscilFFTfreqs.c[i] *= amp;
                    outoscilFFTfreqs.s[i] *= amp;
                }
                break;
        }
    }

    if (freqHz > 0.1 && resonance != 0)
        res->applyres(nyquist - 1, outoscilFFTfreqs, freqHz);

    // Full RMS normalize
    float sum = 0;
    for (int j = 1; j < synth->halfoscilsize; ++j)
    {
        float term = outoscilFFTfreqs.c[j] * outoscilFFTfreqs.c[j]
                     + outoscilFFTfreqs.s[j] * outoscilFFTfreqs.s[j];
        sum += term;
    }
    if (sum < 0.000001f)
        sum = 1.0f;
    sum = 1.0f / sqrtf(sum);
    for (int j = 1; j < synth->halfoscilsize; ++j)
    {
        outoscilFFTfreqs.c[j] *= sum;
        outoscilFFTfreqs.s[j] *= sum;
    }

    if (params->ADvsPAD && freqHz > 0.1f)
    {   // in this case the smps will contain the freqs
        for (int i = 1; i < synth->halfoscilsize; ++i)
            smps[i - 1] = sqrtf(outoscilFFTfreqs.c[i] * outoscilFFTfreqs.c[i]
                               + outoscilFFTfreqs.s[i] * outoscilFFTfreqs.s[i]);
    }
    else
    {
        fft->freqs2smps(&outoscilFFTfreqs, smps);
        for (int i = 0; i < synth->oscilsize; ++i)
            smps[i] *= 0.25f; // correct the amplitude
    }
}

int OscilGen::getPhase()
{
    if (params->Prand >= 64)
        return 0;

    int outpos;
    outpos = (prng.numRandom() * 2.0f - 1.0f) * synth->oscilsize_f * (params->Prand - 64.0f) / 64.0f;
    outpos = (outpos + 2 * synth->oscilsize) % synth->oscilsize;
    return outpos;
}


// Get the spectrum of the oscillator for the UI
void OscilGen::getspectrum(int n, float *spc, int what)
{
    if (n > synth->halfoscilsize)
        n = synth->halfoscilsize;

    for (int i = 1; i < n; ++i)
    {
        if (what == 0)
        {
            spc[i - 1] = sqrtf(oscilFFTfreqs.c[i] * oscilFFTfreqs.c[i]
                               + oscilFFTfreqs.s[i] * oscilFFTfreqs.s[i]);
        }
        else
        {
            if (params->Pcurrentbasefunc == 0)
                spc[i - 1] = (i == 1) ? 1.0f : 0.0f;
            else
                spc[i - 1] = sqrtf(params->getbasefuncFFTfreqs()->c[i] * params->getbasefuncFFTfreqs()->c[i]
                                   + params->getbasefuncFFTfreqs()->s[i] * params->getbasefuncFFTfreqs()->s[i]);
        }
    }

    if (!what)
    {
        for (int i = 0; i < n; ++i)
            outoscilFFTfreqs.s[i] = outoscilFFTfreqs.c[i] = spc[i];
        for (int i = n; i < synth->halfoscilsize; ++i)
            outoscilFFTfreqs.s[i] = outoscilFFTfreqs.c[i] = 0.0f;
        adaptiveharmonic(outoscilFFTfreqs, 0.0);
        for (int i = 0; i < n; ++i)
            spc[i] = outoscilFFTfreqs.s[i];
        adaptiveharmonicpostprocess(spc, n - 1);
    }
}


// Convert the oscillator as base function
void OscilGen::useasbase(void)
{
    params->updatebasefuncFFTfreqs(&oscilFFTfreqs, synth->halfoscilsize);
    oldbasefunc = params->Pcurrentbasefunc = 127;
    prepare();
}


// Get the base function for UI
void OscilGen::getcurrentbasefunction(float *smps)
{
    if (params->Pcurrentbasefunc)
    {
        fft->freqs2smps(params->getbasefuncFFTfreqs(), smps);
    }
    else
        getbasefunction(smps); // the sine case
}
