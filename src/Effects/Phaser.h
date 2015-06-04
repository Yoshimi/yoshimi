/*
    Phaser.h - Phaser effect

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

#ifndef PHASER_H
#define PHASER_H

#include "Effects/Effect.h"
#include "Effects/EffectLFO.h"

#define MAX_PHASER_STAGES 12

class Phaser : public Effect
{
    public:
        Phaser(bool insertion_, float *efxoutl_, float *efxoutr_);
        ~Phaser();
        void out(float *smpsl, float *smpsr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar) const;
        void cleanup(void);
        void setdryonly(void);
    
    private:
        // Parametrii Phaser
        EffectLFO lfo;           // <lfo-ul Phaser
        unsigned char Pvolume;
        unsigned char Ppanning;
        unsigned char Pdepth;    // <depth of Phaser
        unsigned char Pfb;       // <feedback
        unsigned char Plrcross;  // <feedback
        unsigned char Pstages;
        unsigned char Poutsub;   // <substract the output instead of adding it
        unsigned char Pphase;
    
        // Control Parametrii
        void setvolume(unsigned char _volume);
        void setpanning(unsigned char _panning);
        void setdepth(unsigned char _depth);
        void setfb(unsigned char _fb);
        void setlrcross(unsigned char _lrcross);
        void setstages(unsigned char _stages);
        void setphase(unsigned char _phase);
    
        // Internal Values
        // int insertion; // inherited from Effect
        float   panning;
        float   fb;
        float   depth;
        float   lrcross;
        float   fbl;
        float   fbr;
        float   phase;

        float  *oldl;
        float  *oldr;
        float   oldlgain;
        float   oldrgain;
};

#endif
