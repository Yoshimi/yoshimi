/*
    Util.cpp - Miscellaneous functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

using namespace std;

#include "Misc/Util.h"

// Transform the velocity according the scaling parameter (velocity sensing)
float VelF(float velocity, unsigned char scaling)
{
    float x = powf(VELOCITY_MAX_SCALE, (64.0 - scaling) / 64.0);
    if (scaling == 127 || velocity > 0.99)
        return 1.0;
    else
        return powf(velocity, x);

//      return (scaling != 127 && velocity <= 0.99) ? powf(velocity, x) : 1.0;
}

// Get the detune in cents
float getdetune(unsigned char type, unsigned short int coarsedetune,
                      unsigned short int finedetune)
{
    float det = 0.0;
    float octdet = 0.0;
    float cdet = 0.0;
    float findet = 0.0;
    // Get Octave
    int octave = coarsedetune / 1024;
    if (octave >= 8)
        octave -= 16;
    octdet = octave * 1200.0;

    // Coarse and fine detune
    int cdetune = coarsedetune % 1024;
    if (cdetune > 512)
        cdetune -= 1024;

    int fdetune = finedetune - 8192;

    switch (type)
    {
    //	case 1: is used for the default (see below)
        case 2:
            cdet = fabsf(cdetune * 10.0);
            findet = fabsf(fdetune / 8192.0) * 10.0;
            break;
        case 3:
            cdet = fabsf(cdetune * 100);
            findet = powf(10, fabsf(fdetune / 8192.0) * 3.0) / 10.0 - 0.1;
            break;
        case 4:
            cdet = fabsf(cdetune * 701.95500087); // perfect fifth
            findet = (powf(2, fabsf(fdetune / 8192.0) * 12.0) - 1.0) / 4095 * 1200;
            break;
            // case ...: need to update N_DETUNE_TYPES, if you'll add more
        default:
            cdet = fabsf(cdetune * 50.0);
            findet = fabsf(fdetune / 8192.0) * 35.0; // almost like "Paul's Sound Designer 2"
            break;
    }
    if (finedetune < 8192)
        findet = -findet;
    if (cdetune < 0)
        cdet = -cdet;

    det = octdet + cdet + findet;
    return det;
}

bool fileexists(const char *filename)
{
    struct stat tmp;
    int result = stat(filename, &tmp);
    if (result >= 0)
        return true;

    return false;
}

void newFFTFREQS(FFTFREQS *f, int size)
{
    f->c = new float[size];
    f->s = new float[size];
    memset(f->c, 0, size * sizeof(float));
    memset(f->s, 0, size * sizeof(float));
}

void deleteFFTFREQS(FFTFREQS *f)
{
    delete[] f->c;
    delete[] f->s;
    f->c = f->s = NULL;
}

// try to get dreamtime priority
void set_realtime(void)
{
    sched_param sc;
    sc.sched_priority =
        (thread_priority >= 0 && thread_priority <= 75) ? thread_priority : 60;
    sched_setscheduler(0, SCHED_FIFO, &sc);
}

string asString(const float& number)
{
   ostringstream oss;
   oss << number;
   return oss.str();
}

string asString(const int& number)
{
   ostringstream oss;
   oss << number;
   return oss.str();
}

string asString(const unsigned int& number)
{
   ostringstream oss;
   oss << number;
   return oss.str();
}
