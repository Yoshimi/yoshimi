/*
    FFTwrapper.cpp  -  A wrapper for Fast Fourier Transforms

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2010, Alan Calvert

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

    This file is derivative of original ZynAddSubFX code, modified December 2010
*/

#include <cstring>
#include <sys/sysinfo.h>

using namespace std;

#include "Misc/Config.h"
#include "DSP/FFTwrapper.h"

FFTwrapper::FFTwrapper(int fftsize_) :
    fftsize(fftsize_),
    half_fftsize(fftsize_ / 2)
{
    data1 = (float*)fftwf_malloc(fftsize * sizeof(float));
    data2 =  (float*)fftwf_malloc(fftsize * sizeof(float));
    planBasic = fftwf_plan_r2r_1d(fftsize, data1, data1, FFTW_R2HC, FFTW_ESTIMATE);
    planInv = fftwf_plan_r2r_1d(fftsize, data2, data2, FFTW_HC2R, FFTW_ESTIMATE);
}


FFTwrapper::~FFTwrapper()
{
    fftwf_destroy_plan(planBasic);
    fftwf_destroy_plan(planInv);
    fftwf_free(data1);
    fftwf_free(data2);
}


// Fast Fourier Transform
void FFTwrapper::smps2freqs(float *smps, FFTFREQS *freqs)
{
    memcpy(data1, smps, fftsize * sizeof(float));
    fftwf_execute(planBasic);
    memcpy(freqs->c, data1, half_fftsize * sizeof(float));
    for (int i = half_fftsize - 1; i > 0; --i)
        freqs->s[i] = data1[fftsize - i];
    data2[half_fftsize] = 0.0f;
}


// Inverse Fast Fourier Transform
void FFTwrapper::freqs2smps(FFTFREQS *freqs, float *smps)
{
    memcpy(data2, freqs->c, half_fftsize * sizeof(float));
    data2[half_fftsize] = 0.0;
    for (int i = 1; i < half_fftsize; ++i)
        data2[fftsize - i] = freqs->s[i];
    fftwf_execute(planInv);
    memcpy(smps, data2, fftsize * sizeof(float));
}


void FFTwrapper::newFFTFREQS(FFTFREQS& f, int size)
{
    f.c = (float*)fftwf_malloc(size * sizeof(float));
    memset(f.c, 0, size * sizeof(float));
    f.s = (float*)fftwf_malloc(size * sizeof(float));
    memset(f.s, 0, size * sizeof(float));
}


void FFTwrapper::deleteFFTFREQS(FFTFREQS& f)
{
    if (f.s)
        fftwf_free(f.s);
    if (f.c)
        fftwf_free(f.c);
    f.s = f.c = NULL;
}
