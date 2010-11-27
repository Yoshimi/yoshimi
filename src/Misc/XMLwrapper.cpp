/*
    XMLwrapper.cpp - XML wrapper

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

#include <zlib.h>
#include <sstream>
#include <iostream>

#include "Misc/Config.h"
#include "Misc/XMLwrapper.h"

int xml_k = 0;
char tabs[STACKSIZE + 2];

const char *XMLwrapper_whitespace_callback(mxml_node_t *node, int where)
{
    const char *name = node->value.element.name;

    if (where == MXML_WS_BEFORE_OPEN && !strcmp(name, "?xml"))
        return NULL;
    if (where == MXML_WS_BEFORE_CLOSE && !strcmp(name, "string"))
        return NULL;

    if (where == MXML_WS_BEFORE_OPEN || where == MXML_WS_BEFORE_CLOSE)
        return "\n";
    return NULL;
}


XMLwrapper::XMLwrapper() :
    minimal(true),
    stackpos(0)
{
    information.PADsynth_used = 0;
    memset(&parentstack, 0, sizeof(parentstack));
    tree = mxmlNewElement(MXML_NO_PARENT, "?xml version=\"1.0\" encoding=\"UTF-8\"?");
    mxml_node_t *doctype = mxmlNewElement(tree, "!DOCTYPE");
    mxmlElementSetAttr(doctype, "ZynAddSubFX-data", NULL);

    node=root = mxmlNewElement(tree, "ZynAddSubFX-data");

    mxmlElementSetAttr(root, "version-major", "1");
    mxmlElementSetAttr(root, "version-minor", "1");

    info = addparams0("INFORMATION");
    beginbranch("BASE_PARAMETERS");
    addpar("max_midi_parts", NUM_MIDI_PARTS);
    addpar("max_kit_items_per_instrument", NUM_KIT_ITEMS);

    addpar("max_system_effects", NUM_SYS_EFX);
    addpar("max_insertion_effects", NUM_INS_EFX);
    addpar("max_instrument_effects", NUM_PART_EFX);

    addpar("max_addsynth_voices", NUM_VOICES);
    endbranch();
}


XMLwrapper::~XMLwrapper()
{
    if (tree)
        mxmlDelete(tree);
}


// SAVE XML members

bool XMLwrapper::saveXMLfile(const string filename)
{
    string xml = getXMLdata();
    FILE *xmlfile = fopen(filename.c_str(), "w");
    if (!xmlfile)
    {
        Runtime.Log("Error, failed to open xml file " + filename + " for save");
        return false;
    }
    fputs(xml.c_str(), xmlfile);
    fclose(xmlfile);
    return true;
}


string XMLwrapper::getXMLdata(void)
{
    xml_k = 0;
    memset(tabs, 0, STACKSIZE + 2);
    mxml_node_t *oldnode = node;
    node = info;
    addpar("PADsynth_used", information.PADsynth_used);
    node = oldnode;
    boost::shared_array<char> xml =
        boost::shared_array<char>(mxmlSaveAllocString(tree, XMLwrapper_whitespace_callback));
    xmlData = string(xml.get());
    return xmlData;
}


void XMLwrapper::addpar(const string name, int val)
{
    addparams2("par", "name", name.c_str(), "value", asString(val));
}


void XMLwrapper::addparreal(const string name, float val)
{
    addparams2("par_real","name", name.c_str(), "value", asString(val));
}


void XMLwrapper::addparbool(const string name, int val)
{
    if (val != 0)
        addparams2("par_bool", "name", name.c_str(), "value", "yes");
    else
        addparams2("par_bool", "name", name.c_str(), "value", "no");
}


void XMLwrapper::addparstr(const string name, const string val)
{
    mxml_node_t *element = mxmlNewElement(node, "string");
    mxmlElementSetAttr(element, "name", name.c_str());
    mxmlNewText(element, 0, val.c_str());
}


void XMLwrapper::beginbranch(const string name)
{
    push(node);
    node = addparams0(name.c_str());
}


void XMLwrapper::beginbranch(const string name, int id)
{
    push(node);
    node = addparams1(name.c_str(), "id", asString(id));
}


void XMLwrapper::endbranch()
{
    node = pop();
}


bool XMLwrapper::loadXML(const string xml)
{
    if (tree)
        mxmlDelete(tree);
    memset(&parentstack, 0, sizeof(parentstack));
    stackpos = 0;
    xmlData.clear();
    if (xml.empty())
    {
        Runtime.Log("Empty xml data through XMLwrapper::loadXML()", true);
        return false;
    }
    root = tree = mxmlLoadString(NULL, xml.c_str(), MXML_OPAQUE_CALLBACK);
    if (!tree)
    {   
        Runtime.Log("Xml string is not XML ===>>>[" + xml + "]<<<===");
        return false;
    }
    node = root = mxmlFindElement(tree, tree, "ZynAddSubFX-data", NULL, NULL, MXML_DESCEND);
    if (!root)
    {
        Runtime.Log("Invalid xml data in this context: " + xml, true);
        return false;
    }
    push(root);
    xml_version.major = string2int(mxmlElementGetAttr(root, "version-major"));
    xml_version.minor = string2int(mxmlElementGetAttr(root, "version-minor"));
    xmlData = xml;
    return true;
}


bool XMLwrapper::loadXMLfile(const string filename)
{
    if (tree)
        mxmlDelete(tree);
    xmlData.clear();
    tree = NULL;
    memset(&parentstack, 0, sizeof(parentstack));
    stackpos = 0;
    if (!doloadfile(filename))
    {
        Runtime.Log("Could not load xml file: " + filename);
         return false;
    }
    root = tree = mxmlLoadString(NULL, xmlData.c_str(), MXML_OPAQUE_CALLBACK);
    if (!tree)
    {
        Runtime.Log("File " + filename + " is not XML");
        return false;
    }
    node = root = mxmlFindElement(tree, tree, "ZynAddSubFX-data", NULL, NULL, MXML_DESCEND);
    if (!root)
    {
        Runtime.Log("File " + filename + " doesn't contain valid data in this context");
        return false;
    }
    push(root);
    xml_version.major = string2int(mxmlElementGetAttr(root, "version-major"));
    xml_version.minor = string2int(mxmlElementGetAttr(root, "version-minor"));
    return true;
}


bool XMLwrapper::doloadfile(const string filename)
{
    bool ok = false;
    gzFile gzf  = gzopen(filename.c_str(), "rb");
    if (!gzf)
    {
        Runtime.Log("Failed to open xml file " + filename + " for load, errno: "
                    + asString(errno) + "  " + string(strerror(errno)));
        return ok;
    }
    xmlData.clear();
    stringstream readStream;
    char fetchBuf[4097];
    int this_read = 0;
    int total_bytes = 0;
    for (bool quit = false; Runtime.runSynth && !quit;)
    {
        memset(fetchBuf, 0, sizeof(fetchBuf) * sizeof(char));
        this_read = gzread(gzf, fetchBuf, (sizeof(fetchBuf) - 1) * sizeof(char));
        if (this_read > 0)
        {
            total_bytes += this_read;
            readStream << fetchBuf;
        }
        else if (this_read < 0)
        {
            int errnum;
            Runtime.Log("zlib read error: " + string(gzerror(gzf, &errnum)));
            if (errnum == Z_ERRNO)
                Runtime.Log("Filesystem error: " + string(strerror(errno)));
            quit = true;
        }
        else if (total_bytes > 0)
        {
            xmlData = readStream.str();
            ok = quit = true;
        }
        Runtime.signalCheck();
    }
    gzclose(gzf);
    return ok;
}


bool XMLwrapper::putXMLdata(string xmldata)
{
    if (tree)
        mxmlDelete(tree);
    tree = NULL;
    memset(&parentstack, 0, sizeof(parentstack));
    stackpos = 0;
    if (xmldata.empty())
        return false;
    root = tree = mxmlLoadString(NULL, xmldata.c_str(), MXML_OPAQUE_CALLBACK);
    if (tree == NULL)
        return false;
    node = root = mxmlFindElement(tree, tree, "ZynAddSubFX-data", NULL, NULL, MXML_DESCEND);
    if (!root)
        return false;
    push(root);
    return true;
}


bool XMLwrapper::enterbranch(const string name)
{
    node = mxmlFindElement(peek(), peek(), name.c_str(), NULL, NULL,
                           MXML_DESCEND_FIRST);
    if (!node)
        return false;
    push(node);
    return true;
}


bool XMLwrapper::enterbranch(const string name, int id)
{
    node = mxmlFindElement(peek(), peek(), name.c_str(), "id",
                           asString(id).c_str(), MXML_DESCEND_FIRST);
    if (!node)
        return false;
    push(node);
    return true;
}


int XMLwrapper::getbranchid(int min, int max)
{
    int id = string2int(mxmlElementGetAttr(node, "id"));
    if (min == 0 && max == 0)
        return id;
    if (id < min)
        id = min;
    else if (id > max)
        id = max;
    return id;
}


int XMLwrapper::getpar(const string name, int defaultpar, int min, int max)
{
    node = mxmlFindElement(peek(), peek(), "par", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!node)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(node, "value");
    if (!strval)
        return defaultpar;
    int val = string2int(strval);
    if (val < min)
        val = min;
    else if (val > max)
        val = max;
    return val;
}


int XMLwrapper::getpar127(const string name, int defaultpar)
{
    return getpar(name, defaultpar, 0, 127);
}


int XMLwrapper::getparbool(const string name, int defaultpar)
{
    node = mxmlFindElement(peek(), peek(), "par_bool", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!node)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(node, "value");
    if (!strval)
        return defaultpar;
    return (strval[0] == 'Y' || strval[0] == 'y') ? 1 : 0;
}


string XMLwrapper::getparstr(const string name)
{
    node = mxmlFindElement(peek(), peek(), "string", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!node)
        return string();
    if (!node->child)
        return string();
    if (node->child->type != MXML_OPAQUE)
        return string();
    return string(node->child->value.element.name);
}


float XMLwrapper::getparreal(const string name, float defaultpar)
{
    node = mxmlFindElement(peek(), peek(), "par_real", "name", name.c_str(),
                           MXML_DESCEND_FIRST);
    if (!node)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(node, "value");
    if (!strval)
        return defaultpar;
    return string2float(string(strval));
}


float XMLwrapper::getparreal(const string name, float defaultpar, float min, float max)
{
    float result = getparreal(name, defaultpar);
    if (result < min)
        result = min;
    else if (result > max)
        result = max;
    return result;
}


// Private parts

mxml_node_t *XMLwrapper::addparams0(const string name)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams1(const string name, const string par1, const string val1)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams2(const string name, const string par1, const string val1,
                                    const string par2, const string val2)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    mxmlElementSetAttr(element, par2.c_str(), val2.c_str());
    return element;
}


void XMLwrapper::push(mxml_node_t *node)
{
    if (stackpos >= STACKSIZE - 1)
    {
        Runtime.Log("Not good, XMLwrapper push on a full parentstack");
        return;
    }
    stackpos++;
    parentstack[stackpos] = node;
}


mxml_node_t *XMLwrapper::pop(void)
{
    if (stackpos <= 0)
    {
        Runtime.Log("Not good, XMLwrapper pop on empty parentstack");
        return root;
    }
    mxml_node_t *node = parentstack[stackpos];
    parentstack[stackpos] = NULL;
    stackpos--;
    return node;
}


mxml_node_t *XMLwrapper::peek(void)
{
    if (stackpos <= 0)
    {
        Runtime.Log("Not good, XMLwrapper peek on an empty parentstack");
        return root;
    }
    return parentstack[stackpos];
}
