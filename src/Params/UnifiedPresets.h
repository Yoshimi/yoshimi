/*
    UnifiedPresets.h - Presets and Clipboard management

    Copyright 2018, Will Godfrey

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

    Created November 2018
*/

#ifndef U_PRESETS_H
#define U_PRESETS_H

#include "globals.h"
#include "Misc/XMLwrapper.h"

class SynthEngine;

class UnifiedPresets
{
    public:
        string findSectionName(CommandBlock *getData);
        string findleafExtension(CommandBlock *getData);
};

#endif
