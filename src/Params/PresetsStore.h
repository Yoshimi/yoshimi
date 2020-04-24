/*
    PresetsStore.h - Presets and Clipboard store

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2018-2019 Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original
*/

#ifndef PRESETSSTORE_H
#define PRESETSSTORE_H

#include "Misc/Config.h"

#include <string>

using std::string;

#define MAX_PRESETTYPE_SIZE 30

class XMLwrapper;
class PresetsStore;
class SynthEngine;


class PresetsStore
{
    public:
        PresetsStore(SynthEngine *_synth);
        ~PresetsStore();

        // Clipboard stuff
        void copyclipboard(XMLwrapper *xml, const string& type);
        bool pasteclipboard(XMLwrapper *xml);
        bool checkclipboardtype(const string& type);

        // presets stuff
        void copypreset(XMLwrapper *xml, const string& type, const string& name);
        bool pastepreset(XMLwrapper *xml, int npreset);
        void deletepreset(int npreset);

        struct presetstruct {
            string file;
            string name;
        };
        presetstruct presets[MAX_PRESETS];

        void rescanforpresets(const string& type, int root);

    private:
        void clearpresets(void);

        struct _clipboard{
            char *data;
            string type;
        };
        static _clipboard clipboard;

        const string preset_extension;

        SynthEngine *synth;
};

#endif //PRESETSSTORE_H
