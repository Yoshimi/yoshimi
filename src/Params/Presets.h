/*
    Presets.h - Presets and Clipboard management

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018-2019, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code.
*/

#ifndef PRESETS_H
#define PRESETS_H

#include "Misc/XMLwrapper.h"
#include "Params/PresetsStore.h"

class SynthEngine;

class Presets
{
    public:
        Presets(SynthEngine *_synth);
        virtual ~Presets() { }

        void copy(const char *name); // <if name == NULL, the clipboard is used
        void paste(int npreset);     // npreset == 0 for clipboard
        bool checkclipboardtype(void);
        void deletepreset(int npreset);
        void setelement(int n);
        void rescanforpresets(int root);

        SynthEngine *getSynthEngine() {return synth;}

        char type[MAX_PRESETTYPE_SIZE];

    protected:
        void setpresettype(const char *type);

    private:
        virtual void add2XML(XMLwrapper * /* xml */) = 0;
        virtual void getfromXML(XMLwrapper * /* xml */) = 0;
        virtual void defaults(void) = 0;
        virtual void add2XMLsection(XMLwrapper * /* xml */, int /* n */) { }
        virtual void getfromXMLsection(XMLwrapper * /* xml */, int /* n */) { }
        virtual void defaults(int /* n */) { }
        int nelement;

    protected:
        SynthEngine *synth;

    private:
        int updatedAt; // Monotonically increasing counter that tracks last
                       // change.  Users of the parameters compare their last
                       // update to this counter. This can overflow, what's
                       // important is that it's different.

    public:
        class PresetsUpdate
        {
            public:
                PresetsUpdate(const Presets *presets_) :
                    presets(presets_),
                    lastUpdated(presets->updatedAt)
                {}

                // Checks if presets have been updated and resets counter.
                bool checkUpdated()
                {
                    bool result = presets->updatedAt != lastUpdated;
                    lastUpdated = presets->updatedAt;
                    return result;
                }

                void forceUpdate()
                {
                    lastUpdated = presets->updatedAt - 1;
                }

                void changePresets(const Presets *presets_)
                {
                    if (presets != presets_)
                    {
                        presets = presets_;
                        forceUpdate();
                    }
                }

            private:
                const Presets *presets;
                int lastUpdated;
        };

        void presetsUpdated()
        {
            updatedAt++;
        }
};

#endif
