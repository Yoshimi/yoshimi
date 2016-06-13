/*
    Effect.cpp - inherited by the all effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified April 2011
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
    float t = (Ppanning > 0) ? (float)(Ppanning - 1) / 126.0f : 0.0f;
    pangainL = cosf(t * HALFPI);
    pangainR = cosf((1.0f - t) * HALFPI);
}


void Effect::setlrcross(char Plrcross_)
{
    Plrcross = Plrcross_;
    lrcross = (float)Plrcross / 127.0f;
}
