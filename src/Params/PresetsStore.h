/*
    PresetsStore.h - Presets and Clipboard store

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

#include "Misc/XMLwrapper.h"
#include "Misc/Config.h"

#define MAX_PRESETTYPE_SIZE 30
#define MAX_PRESETS 1000

class PresetsStore;
extern PresetsStore presetsstore;

class PresetsStore
{
public:
    PresetsStore();
    ~PresetsStore();

    // Clipboard stuff
    void copyclipboard(XMLwrapper *xml, string type);
    bool pasteclipboard(XMLwrapper *xml);
    bool checkclipboardtype(string type);

    // presets stuff
    void copypreset(XMLwrapper *xml, string type, string name);
    bool pastepreset(XMLwrapper *xml, int npreset);
    void deletepreset(int npreset);

    struct presetstruct {
        string file;
        string name;
    };
    presetstruct presets[MAX_PRESETS];

    void rescanforpresets(string type);

private:
    void clearpresets(void);

    struct {
        char *data;
        string type;
    } clipboard;

    const string preset_extension;
};
