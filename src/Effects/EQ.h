/*
  EQ.h - EQ Effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of a ZynAddSubFX original, modified October 2010
*/

#ifndef EQ_H
#define EQ_H

#include "Misc/MiscFuncs.h"
#include "DSP/AnalogFilter.h"
#include "Effects/Effect.h"

class SynthEngine;

class EQ : public Effect, private MiscFuncs
{
    public:
        EQ(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth);
        ~EQ() { };
        void out(float *smpsl, float *smpr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar);
        void cleanup(void);
        float getfreqresponse(float freq);

    private:
        // Parameters
        unsigned char Pvolume;
        void setvolume(unsigned char Pvolume_);
        struct {
            unsigned char Ptype, Pfreq, Pgain, Pq, Pstages; // parameters
            AnalogFilter *l, *r; // internal values
        } filter[MAX_EQ_BANDS];

        SynthEngine *synth;
};

#endif


