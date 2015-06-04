/*
    Distorsion.h - Distorsion Effect

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

#ifndef DISTORSION_H
#define DISTORSION_H

#include "globals.h"
#include "DSP/AnalogFilter.h"
#include "Effects/Effect.h"
#include "Effects/Fader.h"

// Waveshaping(called by Distorsion effect and waveshape from OscilGen)
void waveshapesmps(int n, float *smps, unsigned char type, unsigned char drive);

class Distorsion : public Effect
{
    public:
        Distorsion(bool insertion, float *efxoutl_, float *efxoutr_);
        ~Distorsion();
        void out(float *smpsl, float *smpr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar) const;
        void cleanup(void);
        void applyfilters(float *efxoutl, float *efxoutr);

    private:
        // Parametrii
        unsigned char Pvolume;       // Volumul or E/R
        unsigned char Ppanning;      // Panning
        unsigned char Plrcross;      // L/R Mixing
        unsigned char Pdrive;        // the input amplification
        unsigned char Plevel;        // the output amplification
        unsigned char Ptype;         // Distorsion type
        unsigned char Pnegate;       // if the input is negated
        unsigned char Plpf;          // lowpass filter
        unsigned char Phpf;          // highpass filter
        unsigned char Pstereo;       // 0=mono,1=stereo
        unsigned char Pprefiltering; // if you want to do the filtering before the distorsion

        void setvolume(unsigned char _volume);
        void setpanning(unsigned char _panning);
        void setlrcross(unsigned char _lrcross);
        void setlpf(unsigned char _lpf);
        void sethpf(unsigned char _hpf);

        // Real Parameters
        float panning;
        float lrcross;
        AnalogFilter *lpfl;
        AnalogFilter *lpfr;
        AnalogFilter *hpfl;
        AnalogFilter *hpfr;
        Fader *fader6db;
};

#endif
