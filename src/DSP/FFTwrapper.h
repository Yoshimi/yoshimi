/*
    FFTwrapper.h  -  A wrapper for Fast Fourier Transforms

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/

#ifndef FFT_WRAPPER_H
#define FFT_WRAPPER_H

#include <fftw3.h>

typedef struct {
    float *s; // sine and cosine components
    float *c;
} FFTFREQS;


class FFTwrapper
{
    public:
        FFTwrapper(int fftsize_);
        ~FFTwrapper();
        void smps2freqs(float *smps,FFTFREQS freqs);
        void freqs2smps(FFTFREQS freqs, float *smps);
        static void newFFTFREQS(FFTFREQS &f, int size);
        static void deleteFFTFREQS(FFTFREQS &f);
        static int fftw_threads;

    private:
        int fftsize;
        double *data1;
        double *data2;
        fftw_plan planBasic;
        fftw_plan planInv;
};
#endif
