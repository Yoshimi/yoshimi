/*
    Envelope.h - Envelope implementation

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

#ifndef ENVELOPE_H
#define ENVELOPE_H

#include <cmath>

#include "globals.h"
#include "Params/EnvelopeParams.h"

class Envelope
{
    public:

        Envelope(EnvelopeParams *envpars, float basefreq);
        ~Envelope() { };
        void relasekey(void);
        float envout(void);
        float envout_dB(void);
        int finished(void) { return envfinish; };

    private:
        int envpoints;
        int envsustain   ;                       // "-1" means disabled
        float envdt[MAX_ENVELOPE_POINTS];  // millisecons
        float envval[MAX_ENVELOPE_POINTS]; // [0.0 .. 1.0]
        float envstretch;
        int linearenvelope;

        int currentpoint;      // current envelope point (starts from 1)
        int forcedrelase;
        char keyreleased;      // if the key was released
        char envfinish;
        float t;         // the time from the last point
        float inct;      // the time increment
        float envoutval; // used to do the forced release
};

#endif
