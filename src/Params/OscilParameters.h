/*
    OscilGen.h - Waveform generator for ADnote

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019 Will Godfrey & others.
    Copyright 2019 Kristian Amlie

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

#ifndef OSCIL_PARAMETERS_H
#define OSCIL_PARAMETERS_H

#include "ParamCheck.h"
#include "DSP/FFTwrapper.h"

class OscilParameters : public ParamBase
{
    public:
        OscilParameters(fft::Calc const& fft, SynthEngine&);

        void defaults()  override;

        void add2XML(XMLwrapper& xml);
        void getfromXML(XMLwrapper& xml);
        float getLimits(CommandBlock *getData);

        void updatebasefuncSpectrum(fft::Spectrum const& src);
        fft::Spectrum const& getbasefuncSpectrum() const { return basefuncSpectrum; }

    public:
        /**
         * The hmag and hphase starts counting from 0, so the first harmonic(1) has the index 0,
         * 2-nd harmonic has index 1, ..the 128 harminic has index 127
         */
        uchar Phmag[MAX_AD_HARMONICS], Phphase[MAX_AD_HARMONICS];
        // the MIDI parameters for mag. and phases

        uchar Phmagtype;                  // 0 - Linear, 1 - dB scale (-40), 2 - dB scale (-60)
                                          // 3 - dB scale (-80), 4 - dB scale (-100)
        uchar Pcurrentbasefunc;           // The base function used - 0=sin, 1=...
        uchar Pbasefuncpar;               // the parameter of the base function

        uchar Pbasefuncmodulation;        // what modulation is applied to the
                                          // basefunc
        uchar Pbasefuncmodulationpar1;
        uchar Pbasefuncmodulationpar2;
        uchar Pbasefuncmodulationpar3;    // the parameter of the base
                                          // function modulation

        uchar Prand;                      // 64 = no randomness
                                          // 63..0 - block type randomness - 0 is maximum
                                          // 65..127 - each harmonic randomness - 127 is maximum
        uchar Pwaveshaping, Pwaveshapingfunction;
        uchar Pfiltertype, Pfilterpar1, Pfilterpar2;
        uchar Pfilterbeforews;
        uchar Psatype, Psapar;            // spectrum adjust

        uchar Pamprandpower, Pamprandtype;// amplitude randomness
        int Pharmonicshift;               // how the harmonics are shifted
        int Pharmonicshiftfirst;          // if the harmonic shift is done before waveshaping and filter

        uchar Padaptiveharmonics;         // the adaptive harmonics status
                                          // (off=0,on=1,etc..)
        uchar Padaptiveharmonicsbasefreq; // the base frequency of the
                                          // adaptive harmonic (30..3000Hz)
        uchar Padaptiveharmonicspower;    // the strength of the effect (0=off,100=full)
        uchar Padaptiveharmonicspar;      // the parameters in 2,3,4.. modes of adaptive harmonics

        uchar Pmodulation; // what modulation is applied to the oscil
        uchar Pmodulationpar1;
        uchar Pmodulationpar2;
        uchar Pmodulationpar3; // the parameter of the parameters

    private:
        fft::Spectrum basefuncSpectrum; // Base Function Frequencies
};

#endif // OSCIL_PARAMETERS_H
