/*
    Effect.cpp - inherited by the all effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2011, Alan Calvert

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

    This file is a derivative of the ZynAddSubFX original, modified April 2011
*/

#include "Effects/Effect.h"


Effect::Effect(bool insertion_, float *efxoutl_, float *efxoutr_,
               FilterParams *filterpars_, unsigned char Ppreset_) :
    Ppreset(Ppreset_),
    efxoutl(efxoutl_),
    efxoutr(efxoutr_),
    filterpars(filterpars_),
    insertion(insertion_)
{
    setpanning(64);
    setlrcross(40);
 }


void Effect::setpanning(char Ppanning_)
{
    Ppanning = Ppanning_;
    char pan = Ppanning - 1;
    if (pan < 0) pan = 0;
    float x = (2.0f * (float)pan / 126.0f) - 1.0f;
    pangainL = (1.0f - x) * (0.7f + 0.2f * x);
    pangainR = (1.0f + x ) * (0.7f - 0.2f * x);
}


void Effect::setlrcross(char Plrcross_)
{
    Plrcross = Plrcross_;
    lrcross = (float)Plrcross / 127.0f;
}
