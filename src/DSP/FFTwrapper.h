/*
    FFTwrapper.h  -  A wrapper for Fast Fourier Transforms

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

#ifndef FFT_WRAPPER_H
#define FFT_WRAPPER_H

#include "globals.h"

#include <fftw3.h>

#define fftw_real double
#define rfftw_plan fftw_plan

class FFTwrapper
{
    public:
        FFTwrapper(int fftsize_);
        ~FFTwrapper();
        void smps2freqs(float *smps,FFTFREQS freqs);
        void freqs2smps(FFTFREQS freqs,float *smps);
    private:
        int fftsize;
        fftw_real *tmpfftdata1, *tmpfftdata2;
        rfftw_plan planfftw, planfftw_inv;
};
#endif
