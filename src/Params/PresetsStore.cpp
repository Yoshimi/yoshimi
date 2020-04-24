/*
    PresetsStore.cpp - Presets and Clipboard store

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2017-2019 Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original.
*/

#include <dirent.h>
//#include <iostream>

#include "Misc/XMLwrapper.h"
#include "Params/PresetsStore.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/SynthEngine.h"

using file::make_legit_filename;


extern SynthEngine *firstSynth;

PresetsStore::_clipboard PresetsStore::clipboard;


PresetsStore::PresetsStore(SynthEngine *_synth) :
    preset_extension(".xpz"),
    synth(_synth)
{
    clipboard.data = NULL;
    clipboard.type.clear();

    for (int i = 0; i < MAX_PRESETS; ++i)
    {
        presets[i].file.clear();
        presets[i].name.clear();
    }
}


PresetsStore::~PresetsStore()
{
    if (clipboard.data != NULL)
    {
        char *_data = __sync_fetch_and_and(&clipboard.data, 0);
        free(_data);

    }
    clearpresets();
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
            synth->getRuntime().effectChange |= 0xff0000; // temporary fix
        return true;
    }
    synth->getRuntime().effectChange = UNUSED; // temporary fix
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


void PresetsStore::clearpresets(void)
{
    for (int i = 0; i < MAX_PRESETS; ++i)
    {
        presets[i].file.clear();
        presets[i].name.clear();
    }
}


void PresetsStore::rescanforpresets(const string& type, int root)
{
    for (int i = 0; i < MAX_PRESETS; ++i)
    {
        presets[i].file.clear();
        presets[i].name.clear();
    }
    int presetk = 0;
    string ftype = "." + type + preset_extension;

    //std::cout << "type " << type << std::endl;
    string altType = "";
    if (type == "Padsyth")
        altType = ".ADnoteParameters" + preset_extension;
    else if (type == "Padsythn")
        altType = ".ADnoteParametersn" + preset_extension;
    else if (type == "Psubsyth")
        altType = ".SUBnoteParameters" + preset_extension;
    else if (type == "Ppadsyth")
        altType = ".PADnoteParameters" + preset_extension;
    string dirname = firstSynth->getRuntime().presetsDirlist[root];
    if (dirname.empty())
        return;
    //std::cout << "Preset root " << dirname << std::endl;
    DIR *dir = opendir(dirname.c_str());
    if (dir == NULL)
        return;

    struct dirent *fn;
    while ((fn = readdir(dir)))
    {
        string filename = string(fn->d_name);
        //std::cout << "file " << filename << std::endl;
        if (filename.find(ftype) == string::npos)
        {
            if (altType.empty() || filename.find(altType) == string::npos)
                continue;
        }
        if (dirname.at(dirname.size() - 1) != '/')
            dirname += "/";
        presets[presetk].file = dirname + filename;

        size_t endpos = filename.find(ftype);
        if (endpos == string::npos)
            if (!altType.empty())
                endpos = filename.find(altType);
        presets[presetk].name = filename.substr(0, endpos);
//        std::cout << "Preset name " << presets[presetk].name << std::endl;
        presetk++;
        if (presetk >= MAX_PRESETS)
            return;
    }
    closedir(dir);

    // sort the presets
    bool check = true;
    while (check)
    {
        check = false;
        for (int j = 0; j < MAX_PRESETS - 1; ++j)
        {
            for (int i = j + 1; i < MAX_PRESETS; ++i)
            {
                if (presets[i].name.empty() || presets[j].name.empty())
                    continue;
                if (strcasecmp(presets[i].name.c_str(), presets[j].name.c_str()) < 0)
                {
                    presets[i].file.swap(presets[j].file);
                    presets[i].name.swap(presets[j].name);
                    check = true;
                }
            }
        }
    }
}


void PresetsStore::copypreset(XMLwrapper *xml, const string& type, const string& name)
{
    if (firstSynth->getRuntime().presetsDirlist[0].empty())
        return;
    synth->getRuntime().xmlType = TOPLEVEL::XML::Presets;
    synth->getRuntime().Log(name);
    string tmpfilename = name;
    make_legit_filename(tmpfilename);
    string dirname = firstSynth->getRuntime().presetsDirlist[synth->getRuntime().currentPreset];
    if (dirname.find_last_of("/") != (dirname.size() - 1))
        dirname += "/";
    xml->saveXMLfile(dirname + tmpfilename + "." + type + preset_extension);
}


bool PresetsStore::pastepreset(XMLwrapper *xml, int npreset)
{
    if (npreset >= MAX_PRESETS || npreset < 1)
        return false;
    npreset--;
    if (presets[npreset].file.empty())
        return false;
    if (synth->getRuntime().effectChange != UNUSED)
        synth->getRuntime().effectChange |= 0xff0000; // temporary fix
    return xml->loadXMLfile(presets[npreset].file);
}


void PresetsStore::deletepreset(int npreset)
{
    if (npreset >= MAX_PRESETS || npreset < 1)
        return;
    npreset--;
    if (!presets[npreset].file.empty())
        remove(presets[npreset].file.c_str());
}
