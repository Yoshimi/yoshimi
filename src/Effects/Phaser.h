/*
    Phaser.h - Phaser effect

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
        void setPreset(unsigned char npreset);
        void changePar(int npar, unsigned char value);
        unsigned char getPar(int npar) const;
        void Cleanup(void);
        void setDryonly(void);
    
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
        void setVolume(unsigned char _volume);
        void setPanning(unsigned char _panning);
        void setDepth(unsigned char _depth);
        void setFb(unsigned char _fb);
        void setLrCross(unsigned char _lrcross);
        void setStages(unsigned char _stages);
        void setPhase(unsigned char _phase);
    
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
