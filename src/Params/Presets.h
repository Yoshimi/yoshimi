/*
    Presets.h - Presets and Clipboard management

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

#ifndef PRESETS_H
#define PRESETS_H

#include "Misc/XMLwrapper.h"
#include "Params/PresetsStore.h"

class Presets
{
    public:
        Presets();
        virtual ~Presets();
        void copy(const char *name); // <if name==NULL, the clipboard is used
        void paste(int npreset);     // npreset==0 for clipboard
        bool checkclipboardtype();
        void deletepreset(int npreset);

        char type[MAX_PRESETTYPE_SIZE];
        void setelement(int n);
        void rescanforpresets();
        unsigned int getSamplerate(void) { return samplerate; };
        int getBuffersize(void) { return buffersize; };
        int getOscilsize(void) { return oscilsize; };

    protected:
        void setpresettype(const char *type);
        unsigned int samplerate;
        int buffersize;
        int oscilsize;
        int half_oscilsize;

    private:
        virtual void add2XML(XMLwrapper *xml) = 0;
        virtual void getfromXML(XMLwrapper *xml) = 0;
        virtual void defaults() = 0;
        virtual void add2XMLsection(XMLwrapper *xml, int n) { };
        virtual void getfromXMLsection(XMLwrapper *xml, int n) { };
        virtual void defaults(int n) { };
        int nelement;
};

#endif
