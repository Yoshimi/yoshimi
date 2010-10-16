/*
    EffectMgr.h - Effect manager, an interface betwen the program and effects

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

#ifndef EFFECTMGR_H
#define EFFECTMGR_H

#include "Effects/Effect.h"
#include "Effects/Reverb.h"
#include "Effects/Echo.h"
#include "Effects/Chorus.h"
#include "Effects/Phaser.h"
#include "Effects/Alienwah.h"
#include "Effects/Distorsion.h"
#include "Effects/EQ.h"
#include "Effects/DynamicFilter.h"
#include "Misc/XMLwrapper.h"
#include "Params/FilterParams.h"
#include "Params/Presets.h"

class EffectMgr : public Presets
{
    public:
        EffectMgr(const bool insertion_);
        ~EffectMgr();

        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);

        void out(float *smpsl, float *smpsr);

        void setdryonly(bool value);

        float sysefxgetvolume(void);

        void cleanup(void);

        void changeeffect(int nefx_);
        int geteffect(void);
        void changepreset(unsigned char npreset);
        void changepreset_nolock(unsigned char npreset);
        unsigned char getpreset(void);
        void seteffectpar(int npar, unsigned char value);
        void seteffectpar_nolock(int npar, unsigned char value);
        unsigned char geteffectpar(int npar);

        float *efxoutl, *efxoutr;
        bool insertion; // the effect is connected as insertion effect (or not)

        // used by UI
        float getEQfreqresponse(float freq);

        FilterParams *filterpars;

    private:
        int nefx;
        Effect *efx;
        bool dryonly;
};

#endif

