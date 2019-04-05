/*
    Util.h - Miscellaneous functions

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

#ifndef UTIL_H
#define UTIL_H

#include <cmath>
#include <pthread.h>
#include <string>
#include <csignal>

using namespace std;

#include "Misc/Microtonal.h"
#include "DSP/FFTwrapper.h"
#include "Misc/Config.h"

extern bool Pexitprogram;  // if the UI sets this true, the program will exit

int set_DAZ_and_FTZ(int on); // bool on/off

string asString(const float& number);
string asString(const int& number);
string asString(const unsigned int& number);
float string2float(const string& str);
int string2int(const string& str);
bool isRegFile(string chkpath);
bool isDirectory(string chkpath);
bool isFifo(string chkpath);
void legit_filename(string& fname);  // make a filename legal

// Velocity Sensing function
float VelF(float velocity, unsigned char scaling);

float getdetune(unsigned char type, unsigned short int coarsedetune,
                unsigned short int finedetune);

#if defined(ASM_F2I_YES)
// is like i = (int)(floor(f))
#define F2I(f,i) __asm__ __volatile__ ("fistpl %0" : "=m" (i) : "t" (f-0.49999999) : "st") ;
#else
#define F2I(f,i) (i)=((f>0) ? ( (int)(f) ) :( (int)(f-1.0) ));
#endif

// How the amplitude threshold is computed, AMPLITUDE_INTERPOLATION_THRESHOLD: 0.0001
inline bool AboveAmplitudeThreshold(float a, float b)
{
    return ((2.0f * fabsf(b - a) / fabsf(b + a + 0.0000000001f))
             > 0.0001f);
}

// Interpolate Amplitude
inline float InterpolateAmplitude(float a, float b, int x, int size)
{
    return a + (b - a) * (float)x / (float)size;
}

inline float dB2rap(float dB) { return exp10f((dB) / 20.0f); }

inline float rap2dB(float rap) { return 20.0f * log10f(rap); }

#endif
