/*
    ParamCheck.cpp - Checks control changes and updates respective parameters

    Copyright 2018-2023 Kristian Amlie, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code.
*/
#include <cstring>
#include <iostream>

#include "Misc/SynthEngine.h"
#include "Params/ParamCheck.h"


ParamBase::ParamBase(SynthEngine *_synth) :
    nelement(-1),
    synth(_synth),
    updatedAt(0)
{
    ;
}
