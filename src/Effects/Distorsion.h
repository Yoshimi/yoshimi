/*
    Distorsion.h - Distorsion Effect

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

#ifndef DISTORSION_H
#define DISTORSION_H

#include "DSP/AnalogFilter.h"
#include "Effects/Effect.h"

// Waveshaping(called by Distorsion effect and waveshape from OscilGen)
void waveshapesmps(int n, float *smps, unsigned char type, unsigned char drive);

class Distorsion : public Effect
{
    public:
        Distorsion(bool insertion, float *efxoutl_, float *efxoutr_);
        ~Distorsion();
        void out(float *smpsl, float *smpr);
        void setPreset(unsigned char npreset);
        void changePar(int npar, unsigned char value);
        unsigned char getPar(int npar) const;
        void Cleanup(void);
        void applyFilters(float *efxoutl, float *efxoutr);

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

        void setVolume(unsigned char _volume);
        void setPanning(unsigned char _panning);
        void setLrCross(unsigned char _lrcross);
        void setLpf(unsigned char _lpf);
        void setHpf(unsigned char _hpf);

        // Real Parameters
        float panning;
        float lrcross;
        AnalogFilter *lpfl;
        AnalogFilter *lpfr;
        AnalogFilter *hpfl;
        AnalogFilter *hpfr;
};

#endif
