/*
    FFTwrapper.cpp  -  A wrapper for Fast Fourier Transforms

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cmath>

#include "DSP/FFTwrapper.h"

FFTwrapper::FFTwrapper(int fftsize_)
{
    fftsize = fftsize_;
    tmpfftdata1 = new fftw_real[fftsize];
    tmpfftdata2 = new fftw_real[fftsize];
    planfftw = fftw_plan_r2r_1d(fftsize, tmpfftdata1, tmpfftdata1, FFTW_R2HC, FFTW_ESTIMATE);
    planfftw_inv = fftw_plan_r2r_1d(fftsize, tmpfftdata2, tmpfftdata2, FFTW_HC2R, FFTW_ESTIMATE);
}

FFTwrapper::~FFTwrapper()
{
    fftw_destroy_plan(planfftw);
    fftw_destroy_plan(planfftw_inv);
    delete [] tmpfftdata1;
    delete [] tmpfftdata2;
}

// do the Fast Fourier Transform
void FFTwrapper::smps2freqs(float *smps, FFTFREQS freqs)
{
    for (int i = 0; i < fftsize; ++i)
        tmpfftdata1[i] = smps[i];
    fftw_execute(planfftw);
    for (int i = 0; i < fftsize / 2; ++i)
    {
        freqs.c[i] = tmpfftdata1[i];
        if (i != 0)
            freqs.s[i] = tmpfftdata1[fftsize - i];
    }
    tmpfftdata2[fftsize / 2] = 0.0;
}

// do the Inverse Fast Fourier Transform
void FFTwrapper::freqs2smps(FFTFREQS freqs, float *smps)
{
    tmpfftdata2[fftsize / 2] = 0.0;
    for (int i = 0; i < fftsize / 2; ++i)
    {
        tmpfftdata2[i] = freqs.c[i];
        if (i != 0)
            tmpfftdata2[fftsize - i] = freqs.s[i];
    }
    fftw_execute(planfftw_inv);
    for (int i = 0; i < fftsize; ++i)
        smps[i] = tmpfftdata2[i];
}

