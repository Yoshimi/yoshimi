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
#include <cmath>

#include "globals.h"
#include "Interface/TextLists.h"

using std::vector;
using std::string;

inline string listpos(int count, int human)
{
    return presetgroups[count * 2 + human];
}


class SynthEngine;
class XMLStore;
class XMLtree;


class UnifiedPresets
{
        SynthEngine& synth;

        const uchar type;
        const uchar npart;
        const uchar kitItem;
        const uchar engineType;
        const uchar parameter;
        const uchar offset;
        const uchar insert;
        const uchar mesgID;

        int listFunction;  // used to select the extension or the friendly name in listing
        vector<string> presetList;

    public:
        UnifiedPresets(SynthEngine& synthInstance, CommandBlock& cmd)
            : synth{synthInstance}
            , type{cmd.data.type}
            , npart{cmd.data.part}
            , kitItem{cmd.data.kit}
            , engineType{cmd.data.engine}
            , parameter{cmd.data.parameter}
            , offset{cmd.data.offset}
            , insert{cmd.data.insert}
            , mesgID{cmd.data.miscmsg}
            , listFunction(lrint(cmd.data.value))
            , presetList{}
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
        string accessXML   (XMLStore&, bool isLoad);
        string synthXML    (XMLtree&, bool isLoad);
        string effectXML   (XMLtree&, bool isLoad);
        string resonanceXML(XMLtree&, bool isLoad);
        string oscilXML    (XMLtree&, bool isLoad);
        string filterXML   (XMLtree&, bool isLoad);
        string lfoXML      (XMLtree&, bool isLoad);
        string envelopeXML (XMLtree&, bool isLoad);

        string listpos(int count) const;
};

#endif
