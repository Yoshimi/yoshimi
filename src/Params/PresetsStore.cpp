/*
    PresetsStore.cpp - Presets and Clipboard store

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/

#include <dirent.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>

#include "Misc/Util.h"
#include "Params/PresetsStore.h"

PresetsStore presetsstore;

PresetsStore::PresetsStore() :
    preset_extension(".xpz")
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
        free(clipboard.data);
}

// Clipboard management

void PresetsStore::copyClipboard(XMLwrapper *xml, string type)
{
    clipboard.type = type;
    if (clipboard.data != NULL)
        free(clipboard.data);
    clipboard.data = xml->getXMLdata();
}

bool PresetsStore::pasteClipboard(XMLwrapper *xml)
{
    if (clipboard.data != NULL)
    {
        xml->putXMLdata(clipboard.data);
        return true;
    }
    return false;
}

bool PresetsStore::checkClipboardType(string type)
{
    //makes LFO's compatible
    if (type.find("Plfo") != string::npos
        && clipboard.type.find("Plfo") != string::npos)
        return true;
    return (!type.compare(clipboard.type));
}


/**
// a helper function that compares 2 presets[]
int Presets_compar(const void *a, const void *b)
{
    struct PresetsStore::presetstruct *p1 = (PresetsStore::presetstruct *)a;
    struct PresetsStore::presetstruct *p2 = (PresetsStore::presetstruct *)b;
    if (p1->name.empty() || p2->name.empty())
        return 0;
    return (strcasecmp(p1->name.c_str(), p2->name.c_str()) < 0);
}
**/

void PresetsStore::rescanPresets(string type)
{
    for (int i = 0; i < MAX_PRESETS; ++i)
    {
        presets[i].file.clear();
        presets[i].name.clear();
    }
    int presetk = 0;
    string ftype = "." + type + preset_extension;

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
    {
        if (Runtime.settings.presetsDirlist[i].empty())
            continue;
        string dirname = Runtime.settings.presetsDirlist[i];
        DIR *dir = opendir(dirname.c_str());
        if (dir == NULL)
            continue;
        struct dirent *fn;
        while ((fn = readdir(dir)))
        {
            string filename = string(fn->d_name);
            if (filename.find(ftype) == string::npos)
                continue;
            if (dirname.at(dirname.size() - 1) != '/')
                dirname += "/";
            presets[presetk].file = dirname + filename;
            if (filename.find_last_of(ftype) != string::npos)
                presets[presetk].name = filename;
            else
                presets[presetk].name =
                    filename.substr(0, filename.find_last_of(ftype));
            presetk++;
            if (presetk >= MAX_PRESETS)
                return;
        }
        closedir(dir);
    }
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

void PresetsStore::copyPreset(XMLwrapper *xml, string type, string name)
{
    if (Runtime.settings.presetsDirlist[0].empty())
        return;
    string filename;
    string tmpfilename = name;
    legit_filename(tmpfilename);
    string dirname = Runtime.settings.presetsDirlist[0];
    if (dirname.find_last_of("/") != (dirname.size() - 1))
        dirname += "/";
    filename = dirname + "." + type + preset_extension;
    xml->saveXMLfile(filename);
}

bool PresetsStore::pastePreset(XMLwrapper *xml, int npreset)
{
    if (npreset >= MAX_PRESETS || npreset < 1)
        return false;
    npreset--;
    if (presets[npreset].file.empty())
        return false;
    return xml->loadXMLfile(presets[npreset].file);
}

void PresetsStore::deletePreset(int npreset)
{
    if (npreset >= MAX_PRESETS || npreset < 1)
        return;
    npreset--;
    if (!presets[npreset].file.empty())
        remove(presets[npreset].file.c_str());
}
