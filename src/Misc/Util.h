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

using namespace std;

#include "globals.h"
#include "Misc/Microtonal.h"
#include "DSP/FFTwrapper.h"
#include "Misc/Config.h"

// Velocity Sensing function
extern float VelF(float velocity, unsigned char scaling);

bool isRegFile(string chkpath);
bool isDirectory(string chkpath);
bool isFifo(string chkpath);

#define N_DETUNE_TYPES 4 // the number of detune types
extern float getdetune(unsigned char type, unsigned short int coarsedetune,
                       unsigned short int finedetune);

void set_realtime(void);

string asString(const float& number);
string asString(const int& number);
string asString(const unsigned int& number);
float string2float(const string& str);
int string2int(const string& str);

void legit_filename(string& fname);  // make a filename legal

// is like i = (int)(floor(f))
#ifdef ASM_F2I_YES
#define F2I(f,i) __asm__ __volatile__ ("fistpl %0" : "=m" (i) : "t" (f-0.49999999) : "st") ;
#else
#define F2I(f,i) (i)=((f>0) ? ( (int)(f) ) :( (int)(f-1.0) ));
#endif

#endif
