/*
    PresetsStore.cpp - Presets and Clipboard store

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2017-2022 Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original.
*/

#include <dirent.h>

#include "Misc/XMLwrapper.h"
#include "Params/PresetsStore.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/SynthEngine.h"

using file::make_legit_filename;


PresetsStore::_clipboard PresetsStore::clipboard;


PresetsStore::PresetsStore(SynthEngine *_synth) :
    synth(_synth)
{
    clipboard.data = NULL;
    clipboard.type.clear();

    presets.clear();
}


PresetsStore::~PresetsStore()
{
    if (clipboard.data != NULL)
    {
        char *_data = __sync_fetch_and_and(&clipboard.data, 0);
        free(_data);

    }
    presets.clear();
}


// Clipboard management
void PresetsStore::copyclipboard(XMLwrapper *xml, const string& type)
{
    clipboard.type = type;
    if (clipboard.data != NULL)
    {
        char *_data = __sync_fetch_and_and(&clipboard.data, 0);
        free(_data);

    }
    clipboard.data = xml->getXMLdata();
}


bool PresetsStore::pasteclipboard(XMLwrapper *xml)
{
    if (clipboard.data != NULL)
    {
        xml->putXMLdata(clipboard.data);
        if (synth->getRuntime().effectChange != UNUSED)
            synth->getRuntime().effectChange |= 0xff0000; // temporary fix - fills in effect header
        return true;
    }
    return false;
}


bool PresetsStore::checkclipboardtype(const string& type)
{
    // makes LFO's compatible
    if (type.find("Plfo") != string::npos
        && clipboard.type.find("Plfo") != string::npos)
        return true;
    return (!type.compare(clipboard.type));
}


void PresetsStore::rescanforpresets(const string& type)
{
    string dirname = synth->getRuntime().presetsDirlist[synth->getRuntime().presetsRootID];
    if (!dirname.empty())
    {
        file::presetsList(dirname, type, presets);
        if(presets.size() > 1)
        {
            sort(presets.begin(), presets.end());
        }
    }
}


void PresetsStore::copypreset(XMLwrapper *xml, const string& type, const string& name)
{
    if (synth->getRuntime().presetsDirlist[0].empty())
        return;
    synth->getRuntime().xmlType = TOPLEVEL::XML::Presets;
    synth->getRuntime().Log(name);
    string tmpfilename = name;
    make_legit_filename(tmpfilename);
    string dirname = synth->getRuntime().presetsDirlist[synth->getRuntime().presetsRootID];
    if (dirname.find_last_of("/") != (dirname.size() - 1))
        dirname += "/";
    xml->saveXMLfile(dirname + tmpfilename + "." + type + EXTEN::presets);
}


bool PresetsStore::pastepreset(XMLwrapper *xml, size_t npreset)
{
    if (npreset > presets.size() || npreset < 1)
        return false;
    npreset--;
    if (presets[npreset].empty())
        return false;
    if (synth->getRuntime().effectChange != UNUSED)
        synth->getRuntime().effectChange |= 0xff0000; // temporary fix - fills in effect header
    return xml->loadXMLfile(presets[npreset]);
}


void PresetsStore::deletepreset(size_t npreset)
{
    if (npreset >= presets.size() || npreset < 1)
        return;
    npreset--;
    if (!presets[npreset].empty())
        file::deleteFile(presets[npreset]);
}
