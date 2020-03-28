/*
    Effect.cpp - inherited by the all effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2011, Alan Calvert
    Copyright 2020, Will Godfrey

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
#include "Misc/NumericFuncs.h"

#define DEFAULT_PARAM_INTERPOLATION_LENGTH_MSECS 10.0f

using func::setAllPan;

float InterpolatedParameter::sampleRate = 0;

InterpolatedParameter::InterpolatedParameter() :
    targetValue(0.5f),
    currentValue(0.5f),
    samplesLeft(0)
{
    setInterpolationLength(DEFAULT_PARAM_INTERPOLATION_LENGTH_MSECS);
}

void InterpolatedParameter::setSampleRate(float sampleRate)
{
    InterpolatedParameter::sampleRate = sampleRate;
}

void InterpolatedParameter::setInterpolationLength(float msecs)
{
    float samples = msecs / 1000.0f * sampleRate;
    // Round up so we are as smooth as possible.
    samplesToInterpolate = ceilf(samples);
}

void InterpolatedParameter::setTargetValue(float value)
{
    targetValue = value;
    samplesLeft = samplesToInterpolate;
}

float InterpolatedParameter::getAndAdvanceValue()
{
    float ret = currentValue;
    advanceValue();
    return ret;
}

void InterpolatedParameter::advanceValue()
{
    if (samplesLeft > 1) {
        currentValue = currentValue + (targetValue - currentValue) / samplesLeft;
        samplesLeft--;
    } else {
        currentValue = targetValue;
        samplesLeft = 0;
    }
}

void InterpolatedParameter::advanceValue(int samples)
{
    if (samplesLeft > 1 && samples < samplesLeft) {
        currentValue = currentValue + (targetValue - currentValue) / samplesLeft * (float)samples;
        samplesLeft -= samples;
    } else {
        currentValue = targetValue;
        samplesLeft = 0;
    }
}

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
