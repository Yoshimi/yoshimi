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

using std::vector;
using std::string;

inline string listpos(int count, int human)
{
    return presetgroups[count * 2 + human];
}


class SynthEngine;

class UnifiedPresets
{
        SynthEngine& synth;
        CommandBlock& cmd;

        vector<string> presetList;
        int human;  // used to select the extension or the friendly name in listing

    public:
        UnifiedPresets(SynthEngine& synthInstance, CommandBlock& cmdData)
            : synth{synthInstance}
            , cmd{cmdData}
            , presetList{}
            , human{0}
            { };

        // shall not be copied nor moved
        UnifiedPresets(UnifiedPresets&&)                 = delete;
        UnifiedPresets(UnifiedPresets const&)            = delete;
        UnifiedPresets& operator=(UnifiedPresets&&)      = delete;
        UnifiedPresets& operator=(UnifiedPresets const&) = delete;

        string handleStoreLoad();

    private:
        void save();
        void load();
        void remove();
        void list(string dirname, string& name);
        string findPresetType();
        string accessXML   (XMLwrapper&, bool isLoad);
        string resonanceXML(XMLwrapper&, bool isLoad);
        string oscilXML    (XMLwrapper&, bool isLoad);
        string filterXML   (XMLwrapper&, bool isLoad);
        string lfoXML      (XMLwrapper&, bool isLoad);
        string envelopeXML (XMLwrapper&, bool isLoad);

        string listpos(int count) const;
};

#endif
