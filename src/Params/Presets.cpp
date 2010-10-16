/*
    Presets.cpp - Presets and Clipboard management

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

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

    This file is a derivative of a ZynAddSubFX original, modified October 2010
*/
#include <cstring>

#include "Misc/SynthEngine.h"
#include "Params/Presets.h"

Presets::Presets() : nelement(-1)
{
    type[0] = 0;
}


void Presets::setpresettype(const char *type)
{
    strcpy(this->type, type);
}


void Presets::copy(const char *name)
{
    XMLwrapper *xml = new XMLwrapper();

    //used only for the clipboard
    if (name == NULL)
        xml->minimal = false;

    char type[MAX_PRESETTYPE_SIZE];
    strcpy(type,this->type);
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
        presetsstore.copyclipboard(xml, type);
    else
        presetsstore.copypreset(xml, type,name);

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

    XMLwrapper *xml = new XMLwrapper();
    if (npreset == 0)
    {
        if (!checkclipboardtype())
        {
            nelement = -1;
            delete(xml);
            return;
        }
        if (!presetsstore.pasteclipboard(xml))
        {
            delete(xml);
            nelement = -1;
            return;
        }
    } else {
        if (!presetsstore.pastepreset(xml, npreset))
        {
            delete(xml);
            nelement = -1;
            return;
        }
    }

    if (xml->enterbranch(type) == 0)
    {
        nelement = -1;
        return;
    }
    if (nelement == -1)
    {
        defaults();
        getfromXML(xml);
    } else {
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

    return presetsstore.checkclipboardtype(type);
}


void Presets::setelement(int n)
{
    nelement = n;
}


void Presets::rescanforpresets(void)
{
    presetsstore.rescanforpresets(type);
}


void Presets::deletepreset(int npreset)
{
    presetsstore.deletepreset(npreset);
}
