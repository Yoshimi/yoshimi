/*
    Presets.cpp - Presets and Clipboard management

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2020 Will Godfrey

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
#include <cstring>
#include <iostream>

#include "Misc/SynthEngine.h"
#include "Params/Presets.h"

extern SynthEngine *firstSynth;

Presets::Presets(SynthEngine *_synth) :
    nelement(-1),
    synth(_synth),
    updatedAt(0)
{
    type[0] = 0;
}


void Presets::setpresettype(const char *type_)
{
    strcpy(type, type_);
}


void Presets::copy(const char *name)
{
    XMLwrapper *xml = new XMLwrapper(synth);
    // used only for the clipboard
    if (name == NULL)
        xml->minimal = false;

    char type[MAX_PRESETTYPE_SIZE];
    strcpy(type, this->type);
    if (nelement != -1)
        strcat(type, "n");
    if (name == NULL)
    {
        if (strstr(type, "Plfo") != NULL)
            strcpy(type, "Plfo");
    }

    xml->beginbranch(type);
    if (nelement == -1)
        add2XML(xml);
    else
        add2XMLsection(xml, nelement);
    xml->endbranch();

    if (name == NULL)
        synth->getPresetsStore().copyclipboard(xml, type);
    else
        firstSynth->getPresetsStore().copypreset(xml, type,name);

    delete(xml);
    nelement = -1;
}


void Presets::paste(int npreset)
{
    char type[MAX_PRESETTYPE_SIZE];
    strcpy(type, this->type);
    if (nelement != -1)
        strcat(type, "n");
    if (npreset == 0)
    {
        if (strstr(type, "Plfo") != NULL)
            strcpy(type, "Plfo");
    }

    XMLwrapper *xml = new XMLwrapper(synth);
    if (npreset == 0)
    {
        if (!checkclipboardtype())
        {
            nelement = -1;
            delete(xml);
            return;
        }
        if (!firstSynth->getPresetsStore().pasteclipboard(xml))
        {
            delete(xml);
            nelement = -1;
            return;
        }
    } else {
        if (!firstSynth->getPresetsStore().pastepreset(xml, npreset))
        {
            delete(xml);
            nelement = -1;
            return;
        }
    }

    string altType = "";
    if (string(type) == "Padsyth")
        altType = "ADnoteParameters";
    else if (string(type) == "Padsythn")
        altType = "ADnoteParametersn";
    else if (string(type) == "Psubsyth")
        altType = "SUBnoteParameters";
    else if (string(type) == "Ppadsyth")
        altType = "PADnoteParameters";

    if (xml->enterbranch(type) == 0)
        if (altType.empty() || xml->enterbranch(altType) == 0)
    {
        nelement = -1;
        delete(xml);
        return;
    }

    if (nelement == -1)
    {
        defaults();
        getfromXML(xml);
    }
    else
    {
        defaults(nelement);
        getfromXMLsection(xml, nelement);
    }
    xml->exitbranch();

    delete(xml);
    nelement = -1;
}


bool Presets::checkclipboardtype(void)
{
    char type[MAX_PRESETTYPE_SIZE];
    strcpy(type, this->type);
    if (nelement != -1)
        strcat(type, "n");

    return synth->getPresetsStore().checkclipboardtype(type);
}


void Presets::setelement(int n)
{
    nelement = n;
}


void Presets::rescanforpresets(int root)
{
    char type[MAX_PRESETTYPE_SIZE];
    strcpy(type, this->type);
    if (nelement != -1)
        strcat(type, "n");
    firstSynth->getPresetsStore().rescanforpresets(type, root);
}


void Presets::deletepreset(int npreset)
{
    firstSynth->getPresetsStore().deletepreset(npreset);
}
