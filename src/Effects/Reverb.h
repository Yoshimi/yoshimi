/*
    Reverb.h - Reverberation effect

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

#ifndef REVERB_H
#define REVERB_H


#include "globals.h"
#include "DSP/AnalogFilter.h"
#include "Effects/Effect.h"
#include "Effects/Fader.h"

#define REV_COMBS 8
#define REV_APS 4

class Reverb : public Effect
{
    public:
        Reverb(bool insertion_, float *efxoutl_, float *efxoutr_);
        ~Reverb();
        void out(float *smps_l, float *smps_r);
        void cleanup(void);
    
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar) const;
    
    private:
        // Parametrii
        unsigned char Pvolume;
        unsigned char Ppan;
        unsigned char Ptime;
        unsigned char Pidelay;
        unsigned char Pidelayfb;
        unsigned char Prdelay;
        unsigned char Perbalance;
        unsigned char Plpf;
        unsigned char Phpf;
              // todo 0..63 lpf,64=off,65..127=hpf(TODO)
        unsigned char Plohidamp;
        unsigned char Ptype;
        unsigned char Proomsize;

        // parameter control
        void setvolume(unsigned char &Pvolume);
        void setpan(unsigned char &Ppan);
        void settime(unsigned char &Ptime);
        void setlohidamp(unsigned char Plohidamp);
        void setidelay(unsigned char &Pidelay);
        void setidelayfb(unsigned char &Pidelayfb);
        void sethpf(unsigned char &Phpf);
        void setlpf(unsigned char &Plpf);
        void settype( unsigned char Ptype);
        void setroomsize(unsigned char &Proomsize);
    
        float pan, erbalance;
        // Parametrii 2
        int lohidamptype; // <0=disable,1=highdamp(lowpass),2=lowdamp(highpass)
        int idelaylen, rdelaylen;
        int idelayk;
        float lohifb;
        float idelayfb;
        float roomsize;
        float rs; // rs is used to "normalise" the volume according to the roomsize
        int comblen[REV_COMBS * 2];
        int aplen[REV_APS * 2];
    
        // Internal Variables
        float *comb[REV_COMBS * 2];
        int combk[REV_COMBS * 2];
        float combfb[REV_COMBS * 2];// <feedback-ul fiecarui filtru "comb"
        float lpcomb[REV_COMBS * 2];  // <pentru Filtrul LowPass
    
        float *ap[REV_APS * 2];
    
        int apk[REV_APS * 2];
    
        float *idelay;
        AnalogFilter *lpf, *hpf; // filters
        float *inputbuf;
    
        void processmono(int ch, float *output);

        int buffersize;
        Fader *volControl;
};

#endif
