/*
    Presets.cpp - Presets and Clipboard management

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
#include <string.h>

#include "Misc/Master.h"
#include "Params/Presets.h"

Presets::Presets() :
    samplerate(zynMaster->getSamplerate()),
    buffersize(zynMaster->getBuffersize()),
    oscilsize(zynMaster->getOscilsize()),
    half_oscilsize(zynMaster->getOscilsize() / 2),
    nelement(-1)
{
    type[0] = 0;
}

Presets::~Presets() { }

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

bool Presets::checkclipboardtype()
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

void Presets::rescanforpresets()
{
    presetsstore.rescanforpresets(type);
}


void Presets::deletepreset(int npreset)
{
    presetsstore.deletepreset(npreset);
}
