/*
    OscilGen.h - Waveform generator for ADnote

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of original ZynAddSubFX code, modified January 2011
*/

#ifndef OSCIL_GEN_H
#define OSCIL_GEN_H

#include <limits.h>

#include "Misc/WaveShapeSamples.h"
#include "Misc/XMLwrapper.h"
#include "DSP/FFTwrapper.h"
#include "Params/Presets.h"
#include "Synth/Resonance.h"

class SynthEngine;

class OscilGen : public Presets, private WaveShapeSamples
{
    public:
        OscilGen(FFTwrapper *fft_,Resonance *res_, SynthEngine *_synth);
        ~OscilGen();

        void prepare();

        int get(float *smps, float freqHz);
        // returns where should I start getting samples, used in block type randomness

        int get(float *smps, float freqHz, int resonance);
        // if freqHz is smaller than 0, return the "un-randomized" sample for UI

        void getbasefunction(float *smps);

        // called by UI
        void getspectrum(int n, float *spc, int what); // what=0 pt. oscil,1 pt. basefunc
        void getcurrentbasefunction(float *smps);
        void useasbase(void);

        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);

        void convert2sine(int magtype);

        // Make a new random seed for Amplitude Randomness -
        //   should be called every noteon event
        inline void newrandseed(void) { randseed = (unsigned int)random(); }

        // Parameters

        /**
         * The hmag and hphase starts counting from 0, so the first harmonic(1) has the index 0,
         * 2-nd harmonic has index 1, ..the 128 harminic has index 127
         */
        unsigned char Phmag[MAX_AD_HARMONICS], Phphase[MAX_AD_HARMONICS];
        // the MIDI parameters for mag. and phases

        unsigned char Phmagtype; // 0 - Linear, 1 - dB scale (-40), 2 - dB scale (-60)
                                 // 3 - dB scale (-80), 4 - dB scale (-100)
        unsigned char Pcurrentbasefunc; // The base function used - 0=sin, 1=...
        unsigned char Pbasefuncpar; // the parameter of the base function

        unsigned char Pbasefuncmodulation; // what modulation is applied to the
                                           // basefunc
        unsigned char Pbasefuncmodulationpar1,
                      Pbasefuncmodulationpar2,
                      Pbasefuncmodulationpar3; // the parameter of the base
                                               // function modulation

        unsigned char Prand; // 64 = no randomness
                             // 63..0 - block type randomness - 0 is maximum
                             // 65..127 - each harmonic randomness - 127 is maximum
        unsigned char Pwaveshaping, Pwaveshapingfunction;
        unsigned char Pfiltertype, Pfilterpar1, Pfilterpar2;
        unsigned char Pfilterbeforews;
        unsigned char Psatype, Psapar; // spectrum adjust

        unsigned char Pamprandpower, Pamprandtype; // amplitude randomness
        int Pharmonicshift; // how the harmonics are shifted
        int Pharmonicshiftfirst; // if the harmonic shift is done before
                                 // waveshaping and filter

        unsigned char Padaptiveharmonics; // the adaptive harmonics status
                                          // (off=0,on=1,etc..)
        unsigned char Padaptiveharmonicsbasefreq; // the base frequency of the
                                                  // adaptive harmonic (30..3000Hz)
        unsigned char Padaptiveharmonicspower; // the strength of the effect
                                               // (0=off,100=full)
        unsigned char Padaptiveharmonicspar; // the parameters in 2,3,4.. modes
                                             // of adaptive harmonics

        unsigned char Pmodulation; // what modulation is applied to the oscil
        unsigned char Pmodulationpar1,
                      Pmodulationpar2,
                      Pmodulationpar3; // the parameter of the parameters

        bool ADvsPAD; // if it is used by ADsynth or by PADsynth

        float numRandom(void);
        unsigned int random(void);

    private:
        float *tmpsmps;
        FFTFREQS outoscilFFTfreqs;

        float hmag[MAX_AD_HARMONICS], hphase[MAX_AD_HARMONICS];
        // the magnituides and the phases of the sine/nonsine harmonics

        FFTwrapper *fft;

        // computes the basefunction and make the FFT; newbasefunc<0  = same basefunc
        void changebasefunction(void);

        void waveshape(void); // Waveshaping (no kidding!)

        void oscilfilter(); // Filter the oscillator accotding to Pfiltertype and Pfilterpar

        void spectrumadjust(void); // Adjust the spectrum

        void shiftharmonics(void); // Shift the harmonics

        void modulation(void); // Do the oscil modulation stuff

        void adaptiveharmonic(FFTFREQS f, float freq);
        // Do the adaptive harmonic stuff

        // Do the adaptive harmonic postprocessing (2n+1,2xS,2xA,etc..)
        // this function is called even for the user interface
        // this can be called for the sine and components, and for the spectrum
        // (that's why the sine and cosine components should be processed with
        // a separate call)
        void adaptiveharmonicpostprocess(float *f, int size);

        // Basic/base functions (Functiile De Baza)
        float basefunc_pulse(float x, float a);
        float basefunc_saw(float x, float a);
        float basefunc_triangle(float x, float a);
        float basefunc_power(float x, float a);
        float basefunc_gauss(float x, float a);
        float basefunc_diode(float x, float a);
        float basefunc_abssine(float x, float a);
        float basefunc_pulsesine(float x, float a);
        float basefunc_stretchsine(float x, float a);
        float basefunc_chirp(float x, float a);
        float basefunc_absstretchsine(float x, float a);
        float basefunc_chebyshev(float x, float a);
        float basefunc_sqr(float x, float a);
        float basefunc_spike(float x, float a);
        float basefunc_circle(float x, float a);

        float harmonicRandom(void);

        // Internal Data
        unsigned char oldbasefunc,
                      oldbasepar,
                      oldhmagtype,
                      oldwaveshapingfunction,
                      oldwaveshaping;

        int oldfilterpars,
            oldsapars,
            oldbasefuncmodulation,
            oldbasefuncmodulationpar1,
            oldbasefuncmodulationpar2,
            oldbasefuncmodulationpar3,
            oldharmonicshift;

        int oldmodulation,
            oldmodulationpar1,
            oldmodulationpar2,
            oldmodulationpar3;

        FFTFREQS basefuncFFTfreqs; // Base Function Frequencies
        FFTFREQS oscilFFTfreqs; // Oscillator Frequencies - this is different
                                // than the hamonics set-up by the user, it may
                                // contain time-domain data if the antialiasing
                                // is turned off
        int oscilprepared; // 1 if the oscil is prepared, 0 if it is not
                           // prepared and is need to call ::prepare() before
                           // ::get()

        Resonance *res;

        unsigned int randseed;

        float random_0_1;
        int32_t random_result;
        struct random_data random_buf;
        char random_state[256];

        float harmonic_random_0_1;
        int32_t harmonic_random_result;
        struct random_data harmonic_random_buf;
        char harmonic_random_state[256];
};


inline float OscilGen::numRandom(void)
{
    if (!random_r(&random_buf, &random_result))
    {
        random_0_1 = (float)random_result / (float)INT_MAX;
        if (isgreater(random_0_1, 1.0f))
            random_0_1 = 1.0f;
        else if (isless(random_0_1, 0.0f))
            random_0_1 = 0.0f;
        return random_0_1;
    }
    return 0.05f;
}


/**
inline float OscilGen::numRandom(void)
{
    if (!random_r(&random_buf, &random_result))
    {
        random_0_1 = (float)random_result / (float)INT_MAX;
        random_0_1 = (random_0_1 > 1.0f) ? 1.0f : random_0_1;
        random_0_1 = (random_0_1 < 0.0f) ? 0.0f : random_0_1;
        return random_0_1;
    }
    return 0.05f;
}

**/


inline float OscilGen::harmonicRandom(void)
{
    if (!random_r(&harmonic_random_buf, &harmonic_random_result))
    {
        harmonic_random_0_1 = (float)harmonic_random_result / (float)INT_MAX;
        harmonic_random_0_1 = (harmonic_random_0_1 > 1.0f) ? 1.0f : harmonic_random_0_1;
        harmonic_random_0_1 = (harmonic_random_0_1 < 0.0f) ? 0.0f : harmonic_random_0_1;
        return harmonic_random_0_1;
    }
    return 0.05f;
}


inline unsigned int OscilGen::random(void)
{
    if (!random_r(&random_buf, &random_result))
        return random_result + INT_MAX / 2;
    return INT_MAX;
}

#endif
