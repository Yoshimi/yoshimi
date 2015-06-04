/*
    PresetsStore.h - Presets and Clipboard store

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Misc/XMLwrapper.h"
#include "Misc/Config.h"

#define MAX_PRESETTYPE_SIZE 30
#define MAX_PRESETS 1000

class PresetsStore
{
public:
    PresetsStore();
    ~PresetsStore();

    // Clipboard stuff
    void copyclipboard(XMLwrapper *xml,char *type);
    bool pasteclipboard(XMLwrapper *xml);
    bool checkclipboardtype(char *type);

    // presets stuff
    void copypreset(XMLwrapper *xml,char *type, const char *name);
    bool pastepreset(XMLwrapper *xml, int npreset);
    void deletepreset(int npreset);

    struct presetstruct {
        char *file;
        char *name;
    };
    presetstruct presets[MAX_PRESETS];

    void rescanforpresets(char *type);

private:
    struct {
        char *data;
        char type[MAX_PRESETTYPE_SIZE];
    } clipboard;

    void clearpresets();

};

extern PresetsStore presetsstore;

