/*
    XMLwrapper.cpp - XML wrapper

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2018, Will Godfrey

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

    This file is derivative of original ZynAddSubFX code.

    Modified March 2018
*/

#include <zlib.h>
#include <sstream>
#include <iostream>

#include "Misc/Config.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"

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


XMLwrapper::XMLwrapper(SynthEngine *_synth, bool _isYoshi) :
    stackpos(0),
    isYoshi(_isYoshi),
    synth(_synth)
{
    minimal = 1 - synth->getRuntime().xmlmax;
    information.PADsynth_used = 0;
    information.ADDsynth_used = 0;
    information.SUBsynth_used = 0;
    memset(&parentstack, 0, sizeof(parentstack));
    tree = mxmlNewElement(MXML_NO_PARENT, "?xml version=\"1.0\" encoding=\"UTF-8\"?");
    mxml_node_t *doctype = mxmlNewElement(tree, "!DOCTYPE");

    if (isYoshi)
    {
        //cout << "Our doctype" << endl;
        mxmlElementSetAttr(doctype, "Yoshimi-data", NULL);
        root = mxmlNewElement(tree, "Yoshimi-data");
        information.yoshiType = 1;
    }
    else
    {
        //cout << "Zyn doctype" << endl;
        mxmlElementSetAttr(doctype, "ZynAddSubFX-data", NULL);
        root = mxmlNewElement(tree, "ZynAddSubFX-data");
        mxmlElementSetAttr(root, "version-major", "3");
        mxmlElementSetAttr(root, "version-minor", "0");
        mxmlElementSetAttr(root, "ZynAddSubFX-author", "Nasca Octavian Paul");
        information.yoshiType = 0;
    }

    node = root;
    mxmlElementSetAttr(root, "Yoshimi-author", "Alan Ernest Calvert");
    string tmp = YOSHIMI_VERSION;
    string::size_type pos1 = tmp.find('.'); // != string::npos
    string::size_type pos2 = tmp.find('.',pos1+1);
    mxmlElementSetAttr(root, "Yoshimi-major", tmp.substr(0, pos1).c_str());
    mxmlElementSetAttr(root, "Yoshimi-minor", tmp.substr(pos1+1, pos2-pos1-1).c_str());

    info = addparams0("INFORMATION"); // specifications

    if (synth->getRuntime().xmlType <= XML_CONFIG)
    {
        if(synth->getRuntime().xmlType != XML_STATE && synth->getRuntime().xmlType != XML_CONFIG)
        {
            beginbranch("BASE_PARAMETERS");
                addpar("max_midi_parts", NUM_MIDI_CHANNELS);
                addpar("max_kit_items_per_instrument", NUM_KIT_ITEMS);
                addpar("max_system_effects", NUM_SYS_EFX);
                addpar("max_insertion_effects", NUM_INS_EFX);
                addpar("max_instrument_effects", NUM_PART_EFX);
                addpar("max_addsynth_voices", NUM_VOICES);
            endbranch();
        }
        else if (synth->getUniqueId() == 0)
        {
            beginbranch("BASE_PARAMETERS");
                addpar("sample_rate", synth->getRuntime().Samplerate);
                addpar("sound_buffer_size", synth->getRuntime().Buffersize);
                addpar("oscil_size", synth->getRuntime().Oscilsize);
                addpar("gzip_compression", synth->getRuntime().GzipCompression);
                addparbool("enable_gui", synth->getRuntime().showGui);
                addparbool("enable_splash", synth->getRuntime().showSplash);
                addparbool("enable_CLI", synth->getRuntime().showCLI);
            endbranch();
        }
    }
}


XMLwrapper::~XMLwrapper()
{
    if (tree)
        mxmlDelete(tree);
}


void XMLwrapper::checkfileinformation(const string& filename)
{
    stackpos = 0;
    memset(&parentstack, 0, sizeof(parentstack));
    information.PADsynth_used = 0;
    if (tree)
        mxmlDelete(tree);
    tree = NULL;
    char *xmldata = doloadfile(filename);
    if (!xmldata)
        return;

    char *first = strstr(xmldata, "<!DOCTYPE Yoshimi-data>");
    information.yoshiType = (first!= NULL);
    char *start = strstr(xmldata, "<INFORMATION>");
    char *end = strstr(xmldata, "</INFORMATION>");
    if (!start || !end || start >= end)
    {
        slowinfosearch(xmldata);
        delete [] xmldata;
        return;
    }

    // Andrew: just make it simple
    // Will: but not too simple :)
    char *idx = start;
    unsigned short names = 0;

    /* the following could be in any order. We are checking for
     * the actual exisitence of the fields as well as their value.
     */
    idx = strstr(start, "name=\"ADDsynth_used\"");
    if (idx != NULL)
    {
        names |= 2;
        if(strstr(idx, "name=\"ADDsynth_used\" value=\"yes\""))
            information.ADDsynth_used = 1;
    }

    idx = strstr(start, "name=\"SUBsynth_used\"");
    if (idx != NULL)
    {
        names |= 4;
        if(strstr(idx, "name=\"SUBsynth_used\" value=\"yes\""))
            information.SUBsynth_used = 1;
    }

    idx = strstr(start, "name=\"PADsynth_used\"");
    if (idx != NULL)
    {
        names |= 1;
        if(strstr(idx, "name=\"PADsynth_used\" value=\"yes\""))
            information.PADsynth_used = 1;
    }

    if (names != 7)
        slowinfosearch(xmldata);

    delete [] xmldata;
    return;
}


void XMLwrapper::slowinfosearch(char *idx)
{
    idx = strstr(idx, "<INSTRUMENT_KIT>");
    if (idx == NULL)
        return;

    string mark;
    int max = NUM_KIT_ITEMS;
    /*
     * The following *must* exist, otherwise the file is corrupted.
     * They will always be in this order, which means we only need
     * to scan once through the file.
     * We can stop if we get to a point where ADD, SUB and PAD
     * have all been enabled.
     */
    idx = strstr(idx, "name=\"kit_mode\"");
    if (idx == NULL)
        return;
    if (strncmp(idx + 16 , "value=\"0\"", 9) == 0)
        max = 1;

    for (int kitnum = 0; kitnum < max; ++kitnum)
    {
        mark = "<INSTRUMENT_KIT_ITEM id=\"" + asString(kitnum) + "\">";
        idx = strstr(idx, mark.c_str());
        if (idx == NULL)
            return;

        idx = strstr(idx, "name=\"enabled\"");
        if (idx == NULL)
            return;
        if (!strstr(idx, "name=\"enabled\" value=\"yes\""))
            continue;

        if (!information.ADDsynth_used)
        {
            idx = strstr(idx, "name=\"add_enabled\"");
            if (idx == NULL)
                return;
            if (strncmp(idx + 26 , "yes", 3) == 0)
                information.ADDsynth_used = 1;
        }
        if (!information.SUBsynth_used)
        {
            idx = strstr(idx, "name=\"sub_enabled\"");
            if (idx == NULL)
                return;
            if (strncmp(idx + 26 , "yes", 3) == 0)
                information.SUBsynth_used = 1;
        }
        if (!information.PADsynth_used)
        {
            idx = strstr(idx, "name=\"pad_enabled\"");
            if (idx == NULL)
                return;
            if (strncmp(idx + 26 , "yes", 3) == 0)
                information.PADsynth_used = 1;
        }
        if (information.ADDsynth_used
          & information.SUBsynth_used
          & information.PADsynth_used)
        {
            break;
        }
    }
  return;
}


// SAVE XML members

bool XMLwrapper::saveXMLfile(const string& filename)
{
    char *xmldata = getXMLdata();

    if (!xmldata)
    {
        synth->getRuntime().Log("XML: Failed to allocate xml data space");
        return false;
    }
    unsigned int compression = synth->getRuntime().GzipCompression;
    if (compression == 0)
    {
        FILE *xmlfile = fopen(filename.c_str(), "w");
        if (!xmlfile)
        {
            synth->getRuntime().Log("XML: Failed to open xml file " + filename + " for save", 2);
            return false;
        }
        fputs(xmldata, xmlfile);
        fclose(xmlfile);
    }
    else
    {
        if (compression > 9)
            compression = 9;
        char options[10];
        snprintf(options, 10, "wb%d", compression);

        gzFile gzfile;
        gzfile = gzopen(filename.c_str(), options);
        if (gzfile == NULL)
        {
            synth->getRuntime().Log("XML: gzopen() == NULL");
            return false;
        }
        gzputs(gzfile, xmldata);
        gzclose(gzfile);
    }
    free(xmldata);
    return true;
}


char *XMLwrapper::getXMLdata()
{
    xml_k = 0;
    memset(tabs, 0, STACKSIZE + 2);
    mxml_node_t *oldnode=node;
    node = info;

    switch (synth->getRuntime().xmlType)
    {
        case 0:
            addparstr("XMLtype", "Invalid");
            break;

        case XML_INSTRUMENT:
            addparbool("ADDsynth_used", information.ADDsynth_used);
            addparbool("SUBsynth_used", information.SUBsynth_used);
            addparbool("PADsynth_used", information.PADsynth_used);
            break;

        case XML_PARAMETERS:
            addparstr("XMLtype", "Parameters");
            break;

        case XML_MICROTONAL:
            addparstr("XMLtype", "Scales");
            break;

        case XML_PRESETS:
            addparstr("XMLtype", "Presets");
            break;

        case XML_STATE:
            addparstr("XMLtype", "Session");
            break;

        case XML_CONFIG:
            addparstr("XMLtype", "Config");
            break;

        case XML_BANK:
            addparstr("XMLtype", "Roots and Banks");
            break;

        case XML_HISTORY:
            addparstr("XMLtype", "Recent Files");
            break;

        case XML_VECTOR:
            addparstr("XMLtype", "Vector Control");
            break;

        case XML_MIDILEARN:
            addparstr("XMLtype", "Midi Learn");
            break;

        default:
            addparstr("XMLtype", "Unknown");
            break;
    }
    node = oldnode;
    char *xmldata = mxmlSaveAllocString(tree, XMLwrapper_whitespace_callback);
    return xmldata;
}


void XMLwrapper::addpar(const string& name, int val)
{
    addparams2("par", "name", name.c_str(), "value", asString(val));
}


void XMLwrapper::addparreal(const string& name, float val)
{
    union { float in; uint32_t out; } convert;
    char buf[11];
    convert.in = val;
    sprintf(buf, "0x%8X", convert.out);
    addparams3("par_real", "name", name.c_str(), "value", asLongString(val), "exact_value", buf);
}


void XMLwrapper::addpardouble(const string& name, double val)
{
    addparams2("par_real","name", name.c_str(), "value", asLongString(val));
}


void XMLwrapper::addparbool(const string& name, int val)
{
    if (val != 0)
        addparams2("par_bool", "name", name.c_str(), "value", "yes");
    else
        addparams2("par_bool", "name", name.c_str(), "value", "no");
}


void XMLwrapper::addparstr(const string& name, const string& val)
{
    mxml_node_t *element = mxmlNewElement(node, "string");
    mxmlElementSetAttr(element, "name", name.c_str());
    mxmlNewText(element, 0, val.c_str());
}


void XMLwrapper::beginbranch(const string& name)
{
    push(node);
    node = addparams0(name.c_str());
}


void XMLwrapper::beginbranch(const string& name, int id)
{
    push(node);
    node = addparams1(name.c_str(), "id", asString(id));
}


void XMLwrapper::endbranch()
{
    node = pop();
}


// LOAD XML members
bool XMLwrapper::loadXMLfile(const string& filename)
{
    bool zynfile = true;
    bool yoshitoo = false;

    if (tree)
        mxmlDelete(tree);
    tree = NULL;
    memset(&parentstack, 0, sizeof(parentstack));
    stackpos = 0;
    const char *xmldata = doloadfile(filename);
    if (xmldata == NULL)
    {
        synth->getRuntime().Log("XML: Could not load xml file: " + filename, 2);
         return false;
    }
    root = tree = mxmlLoadString(NULL, xmldata, MXML_OPAQUE_CALLBACK);
    delete [] xmldata;
    if (!tree)
    {
        synth->getRuntime().Log("XML: File " + filename + " is not XML", 2);
        return false;
    }
    root = mxmlFindElement(tree, tree, "ZynAddSubFX-data", NULL, NULL, MXML_DESCEND);
    if (!root)
    {
        zynfile = false;
        root = mxmlFindElement(tree, tree, "Yoshimi-data", NULL, NULL, MXML_DESCEND);
    }

    if (!root)
    {
        synth->getRuntime().Log("XML: File " + filename + " doesn't contain valid data in this context", 2);
        return false;
    }
    node = root;
    push(root);
    if (zynfile)
    {
        xml_version.major = string2int(mxmlElementGetAttr(root, "version-major"));
        xml_version.minor = string2int(mxmlElementGetAttr(root, "version-minor"));
    }
    if (mxmlElementGetAttr(root, "Yoshimi-major"))
    {
        xml_version.y_major = string2int(mxmlElementGetAttr(root, "Yoshimi-major"));
        yoshitoo = true;

//        synth->getRuntime().Log("XML: Yoshimi " + asString(xml_version.y_major));
    }
    else
        synth->getRuntime().lastXMLmajor = 0;

    if (mxmlElementGetAttr(root, "Yoshimi-minor"))
    {
        xml_version.y_minor = string2int(mxmlElementGetAttr(root, "Yoshimi-minor"));

//        synth->getRuntime().Log("XML: Yoshimi " + asString(xml_version.y_minor));
    }
    else
        synth->getRuntime().lastXMLminor = 0;

    if (synth->getRuntime().logXMLheaders)
    {
        if (zynfile)
            synth->getRuntime().Log("ZynAddSubFX version major " + asString(xml_version.major) + "   minor " + asString(xml_version.minor));
        if (yoshitoo)
            synth->getRuntime().Log("Yoshimi version major " + asString(xml_version.y_major) + "   minor " + asString(xml_version.y_minor));
    }
    return true;
}


char *XMLwrapper::doloadfile(const string& filename)
{
    char *xmldata = NULL;
    gzFile gzf  = gzopen(filename.c_str(), "rb");
    if (!gzf)
    {
        synth->getRuntime().Log("XML: Failed to open xml file " + filename + " for load, errno: "
                    + asString(errno) + "  " + string(strerror(errno)), 2);
        return NULL;
    }
    const int bufSize = 4096;
    char fetchBuf[4097];
    int this_read;
    int total_bytes = 0;
    stringstream readStream;
    for (bool quit = false; !quit;)
    {
        memset(fetchBuf, 0, sizeof(fetchBuf) * sizeof(char));
        this_read = gzread(gzf, fetchBuf, bufSize);
        if (this_read > 0)
        {
            readStream << fetchBuf;
            total_bytes += this_read;
        }
        else if (this_read < 0)
        {
            int errnum;
            synth->getRuntime().Log("XML: Read error in zlib: " + string(gzerror(gzf, &errnum)), 2);
            if (errnum == Z_ERRNO)
                synth->getRuntime().Log("XML: Filesystem error: " + string(strerror(errno)), 2);
            quit = true;
        }
        else if (total_bytes > 0)
        {
            xmldata = new char[total_bytes + 1];
            if (xmldata)
            {
                memset(xmldata, 0, total_bytes + 1);
                memcpy(xmldata, readStream.str().c_str(), total_bytes);
            }
            quit = true;
        }
    }
    gzclose(gzf);
    return xmldata;
}


bool XMLwrapper::putXMLdata(const char *xmldata)
{
    if (tree)
        mxmlDelete(tree);
    tree = NULL;
    memset(&parentstack, 0, sizeof(parentstack));
    stackpos = 0;
    if (xmldata == NULL)
        return false;
    root = tree = mxmlLoadString(NULL, xmldata, MXML_OPAQUE_CALLBACK);
    if (tree == NULL)
        return false;
    root = mxmlFindElement(tree, tree, "ZynAddSubFX-data", NULL, NULL, MXML_DESCEND);
    if (!root)
        root = mxmlFindElement(tree, tree, "Yoshimi-data", NULL, NULL, MXML_DESCEND);
    node = root;
    if (!root)
        return false;
    push(root);
    return true;
}


bool XMLwrapper::enterbranch(const string& name)
{
    node = mxmlFindElement(peek(), peek(), name.c_str(), NULL, NULL,
                           MXML_DESCEND_FIRST);
    if (!node)
        return false;
    push(node);
    if (name == "CONFIGURATION")
    {
        synth->getRuntime().lastXMLmajor = xml_version.y_major;
        synth->getRuntime().lastXMLminor = xml_version.y_minor;
    }
    return true;
}


bool XMLwrapper::enterbranch(const string& name, int id)
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


int XMLwrapper::getpar(const string& name, int defaultpar, int min, int max)
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


int XMLwrapper::getpar127(const string& name, int defaultpar)
{
    return(getpar(name, defaultpar, 0, 127));
}


int XMLwrapper::getpar255(const string& name, int defaultpar)
{
    return(getpar(name, defaultpar, 0, 255));
}


int XMLwrapper::getparbool(const string& name, int defaultpar)
{
    node = mxmlFindElement(peek(), peek(), "par_bool", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!node)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(node, "value");
    if (!strval)
        return defaultpar;
    char tmp = strval[0] | 0x20;
    return (tmp != '0' && tmp != 'n' && tmp != 'f') ? 1 : 0;
}


string XMLwrapper::getparstr(const string& name)
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


float XMLwrapper::getparreal(const string& name, float defaultpar)
{
    node = mxmlFindElement(peek(), peek(), "par_real", "name", name.c_str(),
                           MXML_DESCEND_FIRST);
    if (!node)
        return defaultpar;

    const char *strval = mxmlElementGetAttr(node, "exact_value");
    if (strval != NULL) {
        union { float out; uint32_t in; } convert;
        sscanf(strval+2, "%x", &convert.in);
        return convert.out;
    }

    strval = mxmlElementGetAttr(node, "value");
    if (!strval)
        return defaultpar;
    return string2float(string(strval));
}


float XMLwrapper::getparreal(const string& name, float defaultpar, float min, float max)
{
    float result = getparreal(name, defaultpar);
    if (result < min)
        result = min;
    else if (result > max)
        result = max;
    return result;
}


// Private parts

mxml_node_t *XMLwrapper::addparams0(const string& name)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams1(const string& name, const string& par1, const string& val1)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams2(const string& name, const string& par1, const string& val1,
                                    const string& par2, const string& val2)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    mxmlElementSetAttr(element, par2.c_str(), val2.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams3(const string& name, const string& par1, const string& val1,
                                    const string& par2, const string& val2,
                                    const string& par3, const string& val3)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    mxmlElementSetAttr(element, par2.c_str(), val2.c_str());
    mxmlElementSetAttr(element, par3.c_str(), val3.c_str());
    return element;
}


void XMLwrapper::push(mxml_node_t *node)
{
    if (stackpos >= STACKSIZE - 1)
    {
        synth->getRuntime().Log("XML: Not good, XMLwrapper push on a full parentstack", 2);
        return;
    }
    stackpos++;
    parentstack[stackpos] = node;
}


mxml_node_t *XMLwrapper::pop(void)
{
    if (stackpos <= 0)
    {
        synth->getRuntime().Log("XML: Not good, XMLwrapper pop on empty parentstack", 2);
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
        synth->getRuntime().Log("XML: Not good, XMLwrapper peek on an empty parentstack", 2);
        return root;
    }
    return parentstack[stackpos];
}
