/*
    DynamicFilter.h - "WahWah" effect and others

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

#ifndef DYNAMICFILTER_H
#define DYNAMICFILTER_H

#include "globals.h"
#include "DSP/Filter.h"
#include "Effects/EffectLFO.h"
#include "Effects/Effect.h"

class DynamicFilter : public Effect
{
    public:
        DynamicFilter(bool insertion_, float *efxoutl_, float *efxoutr_);
        ~DynamicFilter();
        void out(float *smpsl, float *smpsr);

        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar) const;
        void cleanup(void);

    //	void setdryonly();

    private:
        // Parametrii DynamicFilter
        EffectLFO lfo; // lfo-ul DynamicFilter
        unsigned char Pvolume;
        unsigned char Ppanning;
        unsigned char Pdepth;
        unsigned char Pampsns;
        unsigned char Pampsnsinv; // if the filter freq is lowered if the input amplitude rises
        unsigned char Pampsmooth; // how smooth the input amplitude changes the filter

        // Parameter Control
        void setvolume(unsigned char _volume);
        void setpanning(unsigned char _panning);
        void setdepth(unsigned char _depth);
        void setampsns(unsigned char _ampsns);
        void reinitfilter(void);

        // Internal Values
        float panning, depth, ampsns, ampsmooth;

        Filter *filterl, *filterr;
        float ms1, ms2, ms3, ms4; // mean squares
};

#endif

