/*
    OscilGen.cpp - Waveform generator for ADnote

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011 Alan Calvert
    Copyright 2009 James Morris

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

    This file is a derivative of a ZynAddSubFX original, modified March 2011
*/

#include <cmath>

using namespace std;

#include "Effects/Distorsion.h"
#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "Synth/OscilGen.h"

//char OscilGen::random_state[256];
//struct random_data OscilGen::random_buf;
//char OscilGen::harmonic_random_state[256];
//struct random_data OscilGen::harmonic_random_buf;

OscilGen::OscilGen(FFTwrapper *fft_, Resonance *res_, SynthEngine *_synth) :
    Presets(_synth),
    ADvsPAD(false),
    tmpsmps((float*)fftwf_malloc(_synth->oscilsize * sizeof(float))),
    fft(fft_),
    res(res_),
    randseed(1)
{
    setpresettype("Poscilgen");
    FFTwrapper::newFFTFREQS(&outoscilFFTfreqs, synth->halfoscilsize);
    if (!tmpsmps)
        synth->getRuntime().Log("Very bad error, failed to allocate OscilGen::tmpsmps");
    else
        memset(tmpsmps, 0, synth->oscilsize * sizeof(float));
    FFTwrapper::newFFTFREQS(&oscilFFTfreqs, synth->halfoscilsize);
    FFTwrapper::newFFTFREQS(&basefuncFFTfreqs, synth->halfoscilsize);
    defaults();
}

OscilGen::~OscilGen()
{
    FFTwrapper::deleteFFTFREQS(&basefuncFFTfreqs);
    FFTwrapper::deleteFFTFREQS(&oscilFFTfreqs);
    if (tmpsmps)
    {
        fftwf_free(tmpsmps);
        FFTwrapper::deleteFFTFREQS(&outoscilFFTfreqs);
    }
}


void OscilGen::defaults(void)
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
    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
    {
        Phmag[i] = 64;
        Phphase[i] = 64;
    }
    Phmag[0] = 127;
    Phmagtype = 0;
    if (ADvsPAD)
        Prand = 127; // max phase randomness (usefull if the oscil will be
                     // imported to a ADsynth from a PADsynth
    else
        Prand = 64; // no randomness

    Pcurrentbasefunc = 0;
    Pbasefuncpar = 64;

    Pbasefuncmodulation = 0;
    Pbasefuncmodulationpar1 = 64;
    Pbasefuncmodulationpar2 = 64;
    Pbasefuncmodulationpar3 = 32;

    Pmodulation = 0;
    Pmodulationpar1 = 64;
    Pmodulationpar2 = 64;
    Pmodulationpar3 = 32;

    Pwaveshapingfunction = 0;
    Pwaveshaping = 64;
    Pfiltertype = 0;
    Pfilterpar1 = 64;
    Pfilterpar2 = 64;
    Pfilterbeforews = 0;
    Psatype = 0;
    Psapar = 64;

    Pamprandpower = 64;
    Pamprandtype = 0;

    Pharmonicshift = 0;
    Pharmonicshiftfirst = 0;

    Padaptiveharmonics = 0;
    Padaptiveharmonicspower = 100;
    Padaptiveharmonicsbasefreq = 128;
    Padaptiveharmonicspar = 50;

    memset(oscilFFTfreqs.s, 0, synth->halfoscilsize * sizeof(float));
    memset(oscilFFTfreqs.c, 0, synth->halfoscilsize * sizeof(float));
    memset(basefuncFFTfreqs.s, 0, synth->halfoscilsize * sizeof(float));
    memset(basefuncFFTfreqs.c, 0, synth->halfoscilsize * sizeof(float));

    oscilprepared = 0;
    oldfilterpars = 0;
    oldsapars = 0;
    prepare();
}


void OscilGen::convert2sine(int magtype)
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

        Phmag[i] = (int)(newmag * 64.0f) + 64;

        Phphase[i] = 64 - (int)(64.0f * newphase / PI);
        if (Phphase[i] > 127)
            Phphase[i] = 127;

        if (Phmag[i] == 64)
            Phphase[i] = 64;
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
        x =- 1.0f;
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
    else if (x > 0.5f)
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

    if(x < 0.5)
    {
        if(x < (0.5 - (b / 2.0)))
            return 0.0;
        else
	{
            x = (x + (b / 2) - 0.5) * (2.0 / b); // shift to zero, and expand to range from 0 to 1
            return x * (2.0 / b); // this is the slope: 1 / (b / 2)
        }
    }
    else
    {
        if(x > (0.5 + (b / 2.0)))
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

    if(x < 2)
    {
        x = x - 1; // x goes from -1 to 1
        if((x < -b) || (x > b))
            y = 0;
        else
            y = sqrt(1 - (pow(x, 2) / pow(b, 2)));  // normally * a^2, but a stays 1
    }
    else
    {
        x = x - 3; // x goes from -1 to 1 as well
        if((x < -b) || (x > b))
            y = 0;
        else
            y = -sqrt(1 - (pow(x, 2) / pow(b, 2)));
    }
    return y;
}
// Base Functions - END


// Get the base function
void OscilGen::getbasefunction(float *smps)
{
    float par = (Pbasefuncpar + 0.5f) / 128.0f;
    if (Pbasefuncpar == 64)
        par = 0.5f;

    float basefuncmodulationpar1 = Pbasefuncmodulationpar1 / 127.0f;
    float basefuncmodulationpar2 = Pbasefuncmodulationpar2 / 127.0f;
    float basefuncmodulationpar3 = Pbasefuncmodulationpar3 / 127.0f;

    switch (Pbasefuncmodulation)
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

        switch (Pbasefuncmodulation)
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

            switch (Pcurrentbasefunc)
            {
                case 1:
                    smps[i] = basefunc_triangle(t, par);
                    break;

                case 2:
                    smps[i] = basefunc_pulse(t, par);
                    break;

                case 3:
                    smps[i] = basefunc_saw(t, par);
                    break;

                case 4:
                    smps[i] = basefunc_power(t, par);
                    break;

                case 5:
                    smps[i] = basefunc_gauss(t, par);
                    break;

                case 6:
                    smps[i] = basefunc_diode(t, par);
                    break;

                case 7:
                    smps[i] = basefunc_abssine(t, par);
                    break;

                case 8:
                    smps[i] = basefunc_pulsesine(t, par);
                    break;

                case 9:
                    smps[i] = basefunc_stretchsine(t, par);
                    break;

                case 10:
                    smps[i] = basefunc_chirp(t, par);
                    break;

                case 11:
                    smps[i] = basefunc_absstretchsine(t, par);
                    break;

                case 12:
                    smps[i] = basefunc_chebyshev(t, par);
                    break;

                case 13:
                    smps[i] = basefunc_sqr(t, par);
                    break;

                case 14:
                    smps[i] = basefunc_spike(t, par);
                    break;

                case 15:
                    smps[i] = basefunc_circle(t, par);
                    break;

                default:
                    smps[i] = -sinf(TWOPI * (float)i / synth->oscilsize_f);
        }
    }
}


// Filter the oscillator
void OscilGen::oscilfilter(void)
{
    if (Pfiltertype == 0)
        return;
    float par = 1.0f - Pfilterpar1 / 128.0f;
    float par2 = Pfilterpar2 / 127.0f;
    float max = 0.0f;
    float tmp = 0.0f;
    float p2;
    float x;

    for (int i = 1; i < synth->halfoscilsize; ++i)
    {
        float gain = 1.0f;
        switch (Pfiltertype)
        {
            case 1:
                gain = powf((1.0f - par * par * par * 0.99f), i); // lp
                tmp = par2 * par2 * par2 * par2 * 0.5f + 0.0001f;
                if (gain < tmp)
                    gain = powf(gain, 10.0f) / powf(tmp, 9.0f);
                break;

            case 2:
                gain = 1.0f - powf((1.0f - par * par), (float)(i + 1)); // hp1
                gain = powf(gain, (par2 * 2.0f + 0.1f));
                break;

            case 3:
                if (par < 0.2f)
                    par = par * 0.25f + 0.15f;
                gain = 1.0f - powf(1.0f - par * par * 0.999f + 0.001f,
                                 i * 0.05f * i + 1.0f); // hp1b
                tmp = powf(5.0f, (par2 * 2.0f));
                gain = powf(gain, tmp);
                break;

            case 4:
                gain = (i + 1) - powf(2.0f, ((1.0f - par) * 7.5f)); // bp1
                gain = 1.0f / (1.0f + gain * gain / (i + 1.0f));
                tmp = powf(5.0f, (par2 * 2.0f));
                gain = powf(gain, tmp);
                if (gain < 1e-5f)
                    gain = 1e-5f;
                break;

            case 5:
                gain = i + 1 - powf(2.0f, (1.0f - par) * 7.5f); // bs1
                gain = powf(atanf(gain / (i / 10.0f + 1.0f)) / 1.57f, 6.0f);
                gain = powf(gain, (par2 * par2 * 3.9f + 0.1f));
                break;

            case 6:
                tmp = powf(par2, 0.33f);
                gain = (i + 1 > powf(2.0f, (1.0f - par) * 10.0f) ? 0.0f : 1.0f)
                            * par2 + (1.0f - par2); // lp2
                break;

            case 7:
                tmp = powf(par2, 0.33f);
                // tmp=1.0-(1.0-par2)*(1.0-par2);
                gain = (i + 1 > powf(2.0f, (1.0f - par) * 7.0f) ? 1.0f : 0.0f)
                        * par2 + (1.0f - par2); // hp2
                if (Pfilterpar1 == 0)
                    gain = 1.0f;
                break;

            case 8:
                tmp = powf(par2, 0.33f);
                // tmp=1.0-(1.0-par2)*(1.0-par2);
                gain = (fabsf(powf(2.0f, (1.0f - par) * 7.0f) - i) > i / 2 + 1 ? 0.0f : 1.0f)
                        * par2 + (1.0f - par2); // bp2
                break;

            case 9:
                tmp = powf(par2, 0.33f);
                gain = (fabsf(powf(2.0f, (1.0f - par) * 7.0f) - i) < i / 2 + 1 ? 0.0f : 1.0f)
                        * par2 + (1.0f - par2); // bs2
                break;

            case 10:
                tmp = powf(5.0f, par2 * 2.0f - 1.0f);
                tmp = powf((i / 32.0f), tmp) * 32.0f;
                if (Pfilterpar2 == 64)
                    tmp = i;
                gain = cosf(par * par * HALFPI * tmp); // cos
                gain *= gain;
                break;

            case 11:
                tmp = powf(5.0f, par2 * 2.0f - 1.0f);
                tmp = powf((i / 32.0f), tmp) * 32.0f;
                if (Pfilterpar2 == 64)
                    tmp = i;
                gain = sinf(par * par * HALFPI * tmp); // sin
                gain *= gain;
                break;

            case 12:
                p2 = 1.0f - par + 0.2f;
                x = i / (64.0f * p2 * p2);
                x = (x > 1.0f) ? 1.0f : x;
                tmp = powf(1.0f - par2, 2.0f);
                gain = cosf(x * PI) * (1.0f - tmp) + 1.01f + tmp; // low shelf
                break;

            case 13:
                tmp = (int)truncf(powf(2.0f, ((1.0f - par) * 7.2f)));
                gain = 1.0f;
                if (i == tmp)
                    gain = powf(2.0f, par2 * par2 * 8.0f);
                break;
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
    if (Pcurrentbasefunc != 0)
    {
        getbasefunction(tmpsmps);
        fft->smps2freqs(tmpsmps, &basefuncFFTfreqs);
        basefuncFFTfreqs.c[0] = 0.0f;
    }
    else
    {
        for (int i = 0; i < synth->halfoscilsize; ++i)
            basefuncFFTfreqs.s[i] = basefuncFFTfreqs.c[i] = 0.0f;
        //in this case basefuncFFTfreqs_ are not used
    }
    oscilprepared = 0;
    oldbasefunc = Pcurrentbasefunc;
    oldbasepar = Pbasefuncpar;
    oldbasefuncmodulation = Pbasefuncmodulation;
    oldbasefuncmodulationpar1 = Pbasefuncmodulationpar1;
    oldbasefuncmodulationpar2 = Pbasefuncmodulationpar2;
    oldbasefuncmodulationpar3 = Pbasefuncmodulationpar3;
}


// Waveshape
void OscilGen::waveshape(void)
{
    oldwaveshapingfunction = Pwaveshapingfunction;
    oldwaveshaping = Pwaveshaping;
    if (Pwaveshapingfunction == 0)
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
    waveShapeSmps(synth->oscilsize, tmpsmps, Pwaveshapingfunction, Pwaveshaping);

    fft->smps2freqs(tmpsmps, &oscilFFTfreqs); // perform FFT
}


// Do the Frequency Modulation of the Oscil
void OscilGen::modulation(void)
{
    oldmodulation = Pmodulation;
    oldmodulationpar1 = Pmodulationpar1;
    oldmodulationpar2 = Pmodulationpar2;
    oldmodulationpar3 = Pmodulationpar3;
    if (Pmodulation == 0)
        return;

    float modulationpar1 = Pmodulationpar1 / 127.0f;
    float modulationpar2 = 0.5 - Pmodulationpar2 / 127.0f;
    float modulationpar3 = Pmodulationpar3 / 127.0f;

    switch (Pmodulation)
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
        switch (Pmodulation)
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

        int poshi = (int)truncf(t);
        float poslo = t - floorf(t);

        tmpsmps[i] = in[poshi] * (1.0f - poslo) + in[poshi + 1] * poslo;
    }

    delete [] in;
    fft->smps2freqs(tmpsmps, &oscilFFTfreqs); // perform FFT
}


// Adjust the spectrum
void OscilGen::spectrumadjust(void)
{
    if (Psatype == 0)
        return;
    float par = Psapar / 127.0f;
    switch (Psatype)
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

        switch (Psatype)
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
    if (Pharmonicshift == 0)
        return;

    float hc, hs;
    int harmonicshift = -Pharmonicshift;

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
    //int i, j, k;
    float a, b, c, d, hmagnew;
    memset(random_state, 0, sizeof(random_state));
    memset(&random_buf, 0, sizeof(random_buf));
    if (initstate_r(synth->random(), random_state,
                    sizeof(random_state), &random_buf))
        synth->getRuntime().Log("OscilGen failed to init general randomness");
    if (oldbasepar != Pbasefuncpar
        || oldbasefunc != Pcurrentbasefunc
        || oldbasefuncmodulation != Pbasefuncmodulation
        || oldbasefuncmodulationpar1 != Pbasefuncmodulationpar1
        || oldbasefuncmodulationpar2 != Pbasefuncmodulationpar2
        || oldbasefuncmodulationpar3 != Pbasefuncmodulationpar3)
        changebasefunction();

    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
        hphase[i] = (Phphase[i] - 64.0f) / 64.0f * PI / (i + 1);

    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
    {
        hmagnew = 1.0f - fabsf(Phmag[i] / 64.0f - 1.0f);
        switch (Phmagtype)
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

        if (Phmag[i] < 64)
            hmag[i] =- hmag[i];
    }

    // remove the harmonics where Phmag[i]==64
    for (int i = 0; i < MAX_AD_HARMONICS; ++i)
        if (Phmag[i] == 64)
            hmag[i] = 0.0f;

    for (int i = 0; i < synth->halfoscilsize; ++i)
        oscilFFTfreqs.c[i] = oscilFFTfreqs.s[i] = 0.0f;
    if (Pcurrentbasefunc == 0)
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
            if (Phmag[j] == 64)
                continue;
            for (int i = 1; i < synth->halfoscilsize; ++i)
            {
                int k = i * (j + 1);
                if (k >= synth->halfoscilsize)
                    break;
                a = basefuncFFTfreqs.c[i];
                b = basefuncFFTfreqs.s[i];
                c = hmag[j] * cosf(hphase[j] * k);
                d = hmag[j] * sinf(hphase[j] * k);
                oscilFFTfreqs.c[k] += a * c - b * d;
                oscilFFTfreqs.s[k] += a * d + b * c;
            }
        }
    }

    if (Pharmonicshiftfirst)
        shiftharmonics();

    if (Pfilterbeforews == 0)
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
    if (!Pharmonicshiftfirst)
        shiftharmonics();

    oscilFFTfreqs.c[0] = 0.0f;

    oldhmagtype = Phmagtype;
    oldharmonicshift = Pharmonicshift + Pharmonicshiftfirst * 256;

    oscilprepared = 1;
}


void OscilGen::adaptiveharmonic(FFTFREQS f, float freq)
{
    if ((Padaptiveharmonics == 0) /*||(freq<1.0)*/)
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
    float basefreq = 30.0f * powf(10.0f, Padaptiveharmonicsbasefreq / 128.0f);
    float power = (Padaptiveharmonicspower + 1.0f) / 101.0f;

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
        int high = (int)truncf(i * rap);
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
    if (Padaptiveharmonics <= 1)
        return;
    float *inf = new float[size];
    float par = Padaptiveharmonicspar * 0.01f;
    par = 1.0f - powf((1.0f - par), 1.5f);

    for (int i = 0; i < size; ++i)
    {
        inf[i] = f[i] * par;
        f[i] = f[i] * (1.0f - par);
    }

    if (Padaptiveharmonics == 2)
    {   // 2n+1
        for (int i = 0; i < size; ++i)
            if ((i % 2) == 0)
                f[i] += inf[i]; // i=0 pt prima armonica,etc.
    }
    else
    {
        // celelalte moduri
        int nh = (Padaptiveharmonics - 3) / 2 + 2;
        int sub_vs_add = (Padaptiveharmonics - 3) % 2;
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
int OscilGen::get(float *smps, float freqHz)
{
    return this->get(smps, freqHz, 0);
}


// Get the oscillator function
int OscilGen::get(float *smps, float freqHz, int resonance)
{
    int nyquist, outpos;

    if (oldbasepar != Pbasefuncpar
        || oldbasefunc != Pcurrentbasefunc
        || oldhmagtype != Phmagtype
        || oldwaveshaping != Pwaveshaping
        || oldwaveshapingfunction != Pwaveshapingfunction)
        oscilprepared = 0;
    if (oldfilterpars != Pfiltertype * 256 + Pfilterpar1 + Pfilterpar2 * 65536
        + Pfilterbeforews * 16777216)
    {
        oscilprepared = 0;
        oldfilterpars =
            Pfiltertype * 256 + Pfilterpar1 + Pfilterpar2 * 65536 + Pfilterbeforews * 16777216;
    }
    if (oldsapars != Psatype * 256 + Psapar)
    {
        oscilprepared = 0;
        oldsapars = Psatype * 256 + Psapar;
    }

    if (oldbasefuncmodulation != Pbasefuncmodulation
        || oldbasefuncmodulationpar1 != Pbasefuncmodulationpar1
        || oldbasefuncmodulationpar2 != Pbasefuncmodulationpar2
        || oldbasefuncmodulationpar3 != Pbasefuncmodulationpar3)
        oscilprepared=0;

    if (oldmodulation != Pmodulation
        || oldmodulationpar1 != Pmodulationpar1
        || oldmodulationpar2 != Pmodulationpar2
        || oldmodulationpar3 != Pmodulationpar3)
        oscilprepared = 0;

    if (oldharmonicshift != Pharmonicshift + Pharmonicshiftfirst * 256)
        oscilprepared = 0;

    if (oscilprepared != 1)
        prepare();

    outpos = (int)truncf((numRandom() * 2.0f - 1.0f) * synth->oscilsize_f * (Prand - 64.0f) / 64.0f);
    outpos = (outpos + 2 * synth->oscilsize) % synth->oscilsize;

    memset(outoscilFFTfreqs.c, 0, synth->halfoscilsize * sizeof(float));
    memset(outoscilFFTfreqs.s, 0, synth->halfoscilsize * sizeof(float));

    nyquist = (int)truncf(0.5f * synth->samplerate_f / fabsf(freqHz)) + 2;
    if (ADvsPAD)
        nyquist = synth->halfoscilsize;
    if (nyquist > synth->halfoscilsize)
        nyquist = synth->halfoscilsize;

    int realnyquist = nyquist;

    if (Padaptiveharmonics)
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
    if (Padaptiveharmonics)
    {   // do the antialiasing in the case of adaptive harmonics
        for (int i = nyquist; i < synth->halfoscilsize; ++i)
            outoscilFFTfreqs.s[i] = outoscilFFTfreqs.c[i] = 0;
    }

    // Randomness (each harmonic), the block type is computed
    // in ADnote by setting start position according to this setting
    if (Prand > 64 && freqHz >= 0.0 && !ADvsPAD)
    {
        float rnd, angle, a, b, c, d;
        rnd = PI * powf((Prand - 64.0f) / 64.0f, 2.0f);
        for (int i = 1; i < nyquist - 1; ++i)
        {   // to Nyquist only for AntiAliasing
            angle = rnd * i * numRandom();
            a = outoscilFFTfreqs.c[i];
            b = outoscilFFTfreqs.s[i];
            c = cosf(angle);
            d = sinf(angle);
            outoscilFFTfreqs.c[i] = a * c - b * d;
            outoscilFFTfreqs.s[i] = a * d + b * c;
        }
    }

    // Harmonic Amplitude Randomness
    if (freqHz > 0.1 && !ADvsPAD)
    {
        // unsigned int realrnd = random();
//        srandom_r(randseed, &harmonic_random_buf);
        memset(harmonic_random_state, 0, sizeof(harmonic_random_state));
        memset(&harmonic_random_buf, 0, sizeof(harmonic_random_buf));
        if (initstate_r(randseed, harmonic_random_state,
                    sizeof(harmonic_random_state), &harmonic_random_buf))
            synth->getRuntime().Log("OscilGen failed to init harmonic amplitude amplitude randomness");
        float power = Pamprandpower / 127.0f;
        float normalize = 1.0f / (1.2f - power);
        switch (Pamprandtype)
        {
            case 1:
                power = power * 2.0f - 0.5f;
                power = powf(15.0f, power);
                for (int i = 1; i < nyquist - 1; ++i)
                {
                    float amp = powf(harmonicRandom(), power) * normalize;
                    outoscilFFTfreqs.c[i] *= amp;
                    outoscilFFTfreqs.s[i] *= amp;
                }
                break;

            case 2:
                power = power * 2.0f - 0.5f;
                power = powf(15.0f, power) * 2.0f;
                float rndfreq = TWOPI * harmonicRandom();
                for (int i = 1 ; i < nyquist - 1; ++i)
                {
                    float amp = powf(fabsf(sinf(i * rndfreq)), power) * normalize;
                    outoscilFFTfreqs.c[i] *= amp;
                    outoscilFFTfreqs.s[i] *= amp;
                }
                break;
        }
        // srandom_r(realrnd + 1, &random_data_buf);
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

    if (ADvsPAD && freqHz > 0.1f)
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

    if (Prand < 64)
        return outpos;
    else
        return 0;
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
            if (Pcurrentbasefunc == 0)
                spc[i - 1] = (i == 1) ? 1.0f : 0.0f;
            else
                spc[i - 1] = sqrtf(basefuncFFTfreqs.c[i] * basefuncFFTfreqs.c[i]
                                   + basefuncFFTfreqs.s[i] * basefuncFFTfreqs.s[i]);
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
    for (int i = 0; i < synth->halfoscilsize; ++i)
    {
        basefuncFFTfreqs.c[i] = oscilFFTfreqs.c[i];
        basefuncFFTfreqs.s[i] = oscilFFTfreqs.s[i];
    }
    oldbasefunc = Pcurrentbasefunc = 127;
    prepare();
}


// Get the base function for UI
void OscilGen::getcurrentbasefunction(float *smps)
{
    if (Pcurrentbasefunc)
    {
        fft->freqs2smps(&basefuncFFTfreqs, smps);
    }
    else
        getbasefunction(smps); // the sine case
}


void OscilGen::add2XML(XMLwrapper *xml)
{
    xml->addpar("harmonic_mag_type", Phmagtype);

    xml->addpar("base_function", Pcurrentbasefunc);
    xml->addpar("base_function_par", Pbasefuncpar);
    xml->addpar("base_function_modulation", Pbasefuncmodulation);
    xml->addpar("base_function_modulation_par1", Pbasefuncmodulationpar1);
    xml->addpar("base_function_modulation_par2", Pbasefuncmodulationpar2);
    xml->addpar("base_function_modulation_par3", Pbasefuncmodulationpar3);

    xml->addpar("modulation", Pmodulation);
    xml->addpar("modulation_par1", Pmodulationpar1);
    xml->addpar("modulation_par2", Pmodulationpar2);
    xml->addpar("modulation_par3", Pmodulationpar3);

    xml->addpar("wave_shaping", Pwaveshaping);
    xml->addpar("wave_shaping_function", Pwaveshapingfunction);

    xml->addpar("filter_type", Pfiltertype);
    xml->addpar("filter_par1", Pfilterpar1);
    xml->addpar("filter_par2", Pfilterpar2);
    xml->addpar("filter_before_wave_shaping", Pfilterbeforews);

    xml->addpar("spectrum_adjust_type", Psatype);
    xml->addpar("spectrum_adjust_par", Psapar);

    xml->addpar("rand", Prand);
    xml->addpar("amp_rand_type", Pamprandtype);
    xml->addpar("amp_rand_power", Pamprandpower);

    xml->addpar("harmonic_shift", Pharmonicshift);
    xml->addparbool("harmonic_shift_first", Pharmonicshiftfirst);

    xml->addpar("adaptive_harmonics", Padaptiveharmonics);
    xml->addpar("adaptive_harmonics_base_frequency", Padaptiveharmonicsbasefreq);
    xml->addpar("adaptive_harmonics_power", Padaptiveharmonicspower);
    xml->addpar("adaptive_harmonics_par", Padaptiveharmonicspar);

    xml->beginbranch("HARMONICS");
        for (int n = 0; n < MAX_AD_HARMONICS; ++n)
        {
            if (Phmag[n] == 64 && Phphase[n] == 64)
                continue;
            xml->beginbranch("HARMONIC", n + 1);
                xml->addpar("mag", Phmag[n]);
                xml->addpar("phase", Phphase[n]);
            xml->endbranch();
        }
    xml->endbranch();

    if (Pcurrentbasefunc == 127)
    {
        float max = 0.0;
        for (int i = 0; i < synth->halfoscilsize; ++i)
        {
            if (max < fabsf(basefuncFFTfreqs.c[i]))
                max = fabsf(basefuncFFTfreqs.c[i]);
            if (max < fabsf(basefuncFFTfreqs.s[i]))
                max = fabsf(basefuncFFTfreqs.s[i]);
        }
        if (max < 0.00000001)
            max = 1.0;

        xml->beginbranch("BASE_FUNCTION");
            for (int i = 1; i < synth->halfoscilsize; ++i)
            {
                float xc = basefuncFFTfreqs.c[i] / max;
                float xs = basefuncFFTfreqs.s[i] / max;
                if (fabsf(xs) > 0.00001 && fabsf(xs) > 0.00001)
                {
                    xml->beginbranch("BF_HARMONIC", i);
                        xml->addparreal("cos", xc);
                        xml->addparreal("sin", xs);
                    xml->endbranch();
                }
            }
        xml->endbranch();
    }
}


void OscilGen::getfromXML(XMLwrapper *xml)
{

    Phmagtype = xml->getpar127("harmonic_mag_type", Phmagtype);

    Pcurrentbasefunc = xml->getpar127("base_function", Pcurrentbasefunc);
    Pbasefuncpar = xml->getpar127("base_function_par", Pbasefuncpar);

    Pbasefuncmodulation = xml->getpar127("base_function_modulation", Pbasefuncmodulation);
    Pbasefuncmodulationpar1 = xml->getpar127("base_function_modulation_par1", Pbasefuncmodulationpar1);
    Pbasefuncmodulationpar2 = xml->getpar127("base_function_modulation_par2", Pbasefuncmodulationpar2);
    Pbasefuncmodulationpar3 = xml->getpar127("base_function_modulation_par3", Pbasefuncmodulationpar3);

    Pmodulation = xml->getpar127("modulation", Pmodulation);
    Pmodulationpar1 = xml->getpar127("modulation_par1", Pmodulationpar1);
    Pmodulationpar2 = xml->getpar127("modulation_par2", Pmodulationpar2);
    Pmodulationpar3 = xml->getpar127("modulation_par3", Pmodulationpar3);

    Pwaveshaping = xml->getpar127("wave_shaping", Pwaveshaping);
    Pwaveshapingfunction = xml->getpar127("wave_shaping_function", Pwaveshapingfunction);

    Pfiltertype = xml->getpar127("filter_type", Pfiltertype);
    Pfilterpar1 = xml->getpar127("filter_par1", Pfilterpar1);
    Pfilterpar2 = xml->getpar127("filter_par2", Pfilterpar2);
    Pfilterbeforews = xml->getpar127("filter_before_wave_shaping", Pfilterbeforews);

    Psatype = xml->getpar127("spectrum_adjust_type", Psatype);
    Psapar = xml->getpar127("spectrum_adjust_par", Psapar);

    Prand = xml->getpar127("rand", Prand);
    Pamprandtype = xml->getpar127("amp_rand_type", Pamprandtype);
    Pamprandpower = xml->getpar127("amp_rand_power", Pamprandpower);

    Pharmonicshift = xml->getpar("harmonic_shift", Pharmonicshift, -64, 64);
    Pharmonicshiftfirst = xml->getparbool("harmonic_shift_first", Pharmonicshiftfirst);

    Padaptiveharmonics = xml->getpar("adaptive_harmonics", Padaptiveharmonics, 0, 127);
    Padaptiveharmonicsbasefreq = xml->getpar("adaptive_harmonics_base_frequency", Padaptiveharmonicsbasefreq,0,255);
    Padaptiveharmonicspower = xml->getpar("adaptive_harmonics_power", Padaptiveharmonicspower, 0, 200);
    Padaptiveharmonicspar = xml->getpar("adaptive_harmonics_par", Padaptiveharmonicspar, 0, 100);

    if (xml->enterbranch("HARMONICS"))
    {
        Phmag[0] = 64;
        Phphase[0] = 64;
        for (int n = 0; n < MAX_AD_HARMONICS; ++n)
        {
            if (xml->enterbranch("HARMONIC",n + 1) == 0)
                continue;
            Phmag[n] = xml->getpar127("mag", 64);
            Phphase[n] = xml->getpar127("phase", 64);

            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if (Pcurrentbasefunc != 0)
        changebasefunction();

    if (xml->enterbranch("BASE_FUNCTION"))
    {
        for (int i = 1; i < synth->halfoscilsize; ++i)
        {
            if (xml->enterbranch("BF_HARMONIC", i))
            {
                basefuncFFTfreqs.c[i] = xml->getparreal("cos", 0.0);
                basefuncFFTfreqs.s[i] = xml->getparreal("sin", 0.0);

                xml->exitbranch();
            }
        }
        xml->exitbranch();

        float max = 0.0;

        basefuncFFTfreqs.c[0] = 0.0;
        for (int i = 0; i < synth->halfoscilsize; ++i)
        {
            if (max < fabsf(basefuncFFTfreqs.c[i]))
                max = fabsf(basefuncFFTfreqs.c[i]);
            if (max < fabsf(basefuncFFTfreqs.s[i]))
                max = fabsf(basefuncFFTfreqs.s[i]);
        }
        if (max < 0.00000001)
            max = 1.0;

        for (int i = 0; i < synth->halfoscilsize; ++i)
        {
            if (basefuncFFTfreqs.c[i])
                basefuncFFTfreqs.c[i] /= max;
            if (basefuncFFTfreqs.s[i])
                basefuncFFTfreqs.s[i] /= max;
        }
    }
}
