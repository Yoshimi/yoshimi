/*
    UnifiedPresets.h - Presets and Clipboard management

    Copyright 2018, 2023, Will Godfrey

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
*/

#ifndef U_PRESETS_H
#define U_PRESETS_H

#include <string>
#include <vector>

#include "globals.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Interface/TextLists.h"

using std::to_string;
using std::vector;

class SynthEngine;

class UnifiedPresets
{
    private:
        vector<std::string> presetList;
        SynthEngine *synth;

    public:
        string section(SynthEngine *synth, CommandBlock *getData);
        string findPresetType(CommandBlock *getData);
        void list(string dirname, string& name);
        string findXML(XMLwrapper *xml,CommandBlock *getData, bool isLoad);
        string resonanceXML(XMLwrapper *xml,CommandBlock *getData, bool isLoad);
        string oscilXML(XMLwrapper *xml,CommandBlock *getData, bool isLoad);
        string filterXML(XMLwrapper *xml,CommandBlock *getData, bool isLoad);
        string lfoXML(XMLwrapper *xml,CommandBlock *getData, bool isLoad);
        string envelopeXML(XMLwrapper *xml,CommandBlock *getData, bool isLoad);
        bool saveUnif(CommandBlock *getData);
        bool load(CommandBlock *getData);
};

#endif
