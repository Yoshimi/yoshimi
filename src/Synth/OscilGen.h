/*
    OscilGen.h - Waveform generator for ADnote

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2020 Will Godfrey & others.

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

    This file is derivative of original ZynAddSubFX code.

*/

#ifndef OSCIL_GEN_H
#define OSCIL_GEN_H

#include <sys/types.h>
#include <limits.h>
#include <vector>

#include "Misc/RandomGen.h"
#include "Misc/WaveShapeSamples.h"
#include "Misc/XMLwrapper.h"
#include "DSP/FFTwrapper.h"
#include "Params/OscilParameters.h"
#include "Synth/Resonance.h"

class SynthEngine;

class OscilGen : private WaveShapeSamples
{
    public:
        OscilGen(fft::Calc&,Resonance *res_, SynthEngine *_synth, OscilParameters *params_);
       ~OscilGen() = default;

        // shall not be copied or moved or assigned
        OscilGen(OscilGen&&)                 = delete;
        OscilGen(OscilGen const&)            = delete;
        OscilGen& operator=(OscilGen&&)      = delete;
        OscilGen& operator=(OscilGen const&) = delete;


        void changeParams(OscilParameters *params_);

        void prepare();

        void getWave(fft::Waveform&, float freqHz, bool applyResonance =false, bool forGUI =false);
        std::vector<float> getSpectrumForPAD(float freqHz);

        // Get just the phase of the oscillator.
        int getPhase();

        void getbasefunction(fft::Waveform&);

        // called by UI
        void getOscilSpectrumIntensities(size_t, float*);
        void getBasefuncSpectrumIntensities(size_t, float*);
        void displayBasefuncForGui(fft::Waveform&);
        void displayWaveformForGui(fft::Waveform&);

        // convert the current Oscil settings into a "user base function"
        void useasbase(void);

        void genDefaults(void);
        void defaults(void);
        void convert2sine();

        // Draw a new random seed for randomisation of harmonics - called every noteon event
        void newrandseed() { randseed = basePrng.randomINT() + INT_MAX/2; }
        void resetHarmonicPrng() { harmonicPrng.init(randseed); }
        void reseed(int value);
        void forceUpdate();

    private:
        OscilParameters *params;

        SynthEngine *synth;

        fft::Calc& fft;

        fft::Waveform tmpsmps;

        float hmag[MAX_AD_HARMONICS], hphase[MAX_AD_HARMONICS];
        // the magnituides and the phases of the sine/nonsine harmonics

        // OscilGen core implementation: generate the current Spectrum -> outoscilSpectrum
        void buildSpectrum(float freqHz, bool applyResonance, bool forGUI, bool forPAD);

        // computes the basefunction and make the FFT;
        void changebasefunction(void);

        void waveshape(void); // Waveshaping (no kidding!)

        void oscilfilter(); // Filter the oscillator according to Pfiltertype and Pfilterpar

        void spectrumadjust(void); // Adjust the spectrum

        void shiftharmonics(void); // Shift the harmonics

        void modulation(void); // Do the oscil modulation stuff


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
        float basefunc_hypsec(float x, float a);


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

        fft::Spectrum outoscilSpectrum;

        fft::Spectrum oscilSpectrum; // Oscillator Frequencies - this is different
                                     // than the hamonics set-up by the user, it may
                                     // contain time-domain data if the antialiasing
                                     // is turned off
        Presets::PresetsUpdate oscilupdate;// whether the oscil is prepared, if
                                           // not prepared we need to call
                                           // ::prepare() before ::get()


        Resonance *res;

        uint32_t randseed;

        RandomGen basePrng;
        RandomGen harmonicPrng;
};

// allow to mark this OscilGen as "dirty" to force recalculation of spectrum
// (as of 4/22 only relevant for automated testing, see SynthEngine::setReproducibleState()
inline void OscilGen::forceUpdate()
{
    oscilupdate.forceUpdate();
}

#endif
