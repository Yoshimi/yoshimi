/*
    Effect.cpp - inherited by the all effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2011, Alan Calvert
    Copyright 2020, Will Godfrey

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
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

#include "Misc/SynthEngine.h"
#include "Effects/Effect.h"
#include "Misc/NumericFuncs.h"

using func::setAllPan;

namespace {
    const float PAN_NORMAL_CENTRE = cosf(0.5f * HALFPI);
}

Effect::Effect(bool insertion_, float *efxoutl_, float *efxoutr_,
               FilterParams *filterpars_, uchar Ppreset_,
               SynthEngine& synth_)
    : Ppreset{Ppreset_}
    , efxoutl{efxoutl_}
    , efxoutr{efxoutr_}
    , outvolume{0.5f, synth_.samplerate}
    , volume{0.5f, synth_.samplerate}
    , filterpars{filterpars_}
    , insertion{insertion_}
    , pangainL{PAN_NORMAL_CENTRE, synth_.samplerate}
    , pangainR{PAN_NORMAL_CENTRE, synth_.samplerate}
    , lrcross{40.0f/127, synth_.samplerate}
    , synth{synth_}
{
    setpanning(64);
    setlrcross(40);
}


// base implementation: force to clean reproducible state.
// Note: be sure to invoke that from overridden implementations.
void Effect::cleanup()
{
    outvolume.pushToTarget();
    volume   .pushToTarget();
    pangainL.pushToTarget();
    pangainR.pushToTarget();
    lrcross.pushToTarget();
}


void Effect::setpanning(char Ppanning_)
{
    Ppanning = Ppanning_;
    float left = 0;
    float right = 0;
    setAllPan(Ppanning, left, right, MAIN::panningType::normal);
    /*
     * we fix panning as 'normal' for effects, because this has
     * never been any different, and we don't know what subtle
     * changes might take place in the final sound if this
     * followed user panning type changes.
     */
    pangainL.setTargetValue(left);
    pangainR.setTargetValue(right);
}


void Effect::setlrcross(char Plrcross_)
{
    Plrcross = Plrcross_;
    lrcross.setTargetValue(Plrcross / 127.0f);
}


/**
 * Base implementation: fetch through `virtual getpar(n)`
 * Only the EQ uses the high number of (band) parameters.
 */
void Effect::getAllPar(EffectParArray& target) const
{
    const uint LIM = EFFECT::control::bpmStart;
    for (uint i=0; i<=LIM; ++i)
        target[i] = this->getpar(i);
}

