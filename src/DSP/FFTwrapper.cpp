/*
    FFTwrapper.cpp  -  A wrapper for Fast Fourier Transforms

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul

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

#include <cmath>
#include <cstring>
#include <sys/sysinfo.h>

using namespace std;

#include "Misc/Util.h"
#include "Misc/Config.h"
#include "DSP/FFTwrapper.h"

FFTwrapper::FFTwrapper(int fftsize_)
{
    fftsize = fftsize_;
    data1 = (double*)fftw_malloc(sizeof(double) * fftsize);
    data2 = (double*)fftw_malloc(sizeof(double) * fftsize);
    planBasic = fftw_plan_r2r_1d(fftsize, data1, data1, FFTW_R2HC, FFTW_ESTIMATE);
    planInv = fftw_plan_r2r_1d(fftsize, data2, data2, FFTW_HC2R, FFTW_ESTIMATE);
}


FFTwrapper::~FFTwrapper()
{
    fftw_destroy_plan(planBasic);
    fftw_destroy_plan(planInv);
    fftw_free(data1);
    fftw_free(data2);
}


// do the Fast Fourier Transform
void FFTwrapper::smps2freqs(float *smps, FFTFREQS freqs)
{
    for (int i = 0; i < fftsize; ++i)
        data1[i] = smps[i];
    fftw_execute(planBasic);
    int half_fftsize = fftsize / 2;
    for (int i = 0; i < half_fftsize; ++i)
    {
        freqs.c[i] = data1[i];
        if (i != 0)
            freqs.s[i] = data1[fftsize - i];
    }
    data2[half_fftsize] = 0.0;
}


// do the Inverse Fast Fourier Transform
void FFTwrapper::freqs2smps(FFTFREQS freqs, float *smps)
{
    int half_fftsize = fftsize / 2;
    data2[half_fftsize] = 0.0;
    for (int i = 0; i < half_fftsize; ++i)
    {
        data2[i] = freqs.c[i];
        if (i != 0)
            data2[fftsize - i] = freqs.s[i];
    }
    fftw_execute(planInv);
    for (int i = 0; i < fftsize; ++i)
        smps[i] = data2[i];
}


void FFTwrapper::newFFTFREQS(FFTFREQS &f, int size)
{
    f.c = new float[size];
    if (NULL != f.c)
        memset(f.c, 0, sizeof(float) * size);
    f.s = new float[size];
    if (NULL != f.s)
        memset(f.s, 0, sizeof(float) * size);
}


void FFTwrapper::deleteFFTFREQS(FFTFREQS &f)
{
    if (NULL != f.c)
        delete [] f.c;
    if (NULL != f.s)
        delete [] f.s;
    f.c = f.s = NULL;
}
