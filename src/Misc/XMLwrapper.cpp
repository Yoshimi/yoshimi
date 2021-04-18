/*
    XMLwrapper.cpp - XML wrapper

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2021, Will Godfrey

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

*/

#include <sys/types.h>
#include <zlib.h>
#include <sstream>
#include <iostream>
#include <string>

#include "Misc/Config.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/FormatFuncs.h"

using file::saveText;
using file::loadGzipped;
using file::saveGzipped;
using file::findExtension;
using func::string2int;
using func::string2float;
using func::asLongString;
using func::asString;


const char *XMLwrapper_whitespace_callback(mxml_node_t *node, int where)
{
    const char *name = mxmlGetElement(node);

    if (where == MXML_WS_BEFORE_OPEN && !strncmp(name, "?xml", 4))
        return NULL;
    if (where == MXML_WS_BEFORE_CLOSE && !strncmp(name, "string", 6))
        return NULL;

    if (where == MXML_WS_BEFORE_OPEN || where == MXML_WS_BEFORE_CLOSE)
        return "\n";
    return NULL;
}


XMLwrapper::XMLwrapper(SynthEngine *_synth, bool _isYoshi, bool includeBase) :
    stackpos(0),
    xml_k(0),
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

    if (!includeBase)
        return;

    if (isYoshi)
    {
        //std::cout << "Our doctype" << std::endl;
        mxmlElementSetAttr(doctype, "Yoshimi-data", NULL);
        root = mxmlNewElement(tree, "Yoshimi-data");
        information.yoshiType = 1;
    }
    else
    {
        //std::cout << "Zyn doctype" << std::endl;
        mxmlElementSetAttr(doctype, "ZynAddSubFX-data", NULL);
        root = mxmlNewElement(tree, "ZynAddSubFX-data");
        mxmlElementSetAttr(root, "version-major", "2");
        mxmlElementSetAttr(root, "version-minor", "4");
        mxmlElementSetAttr(root, "ZynAddSubFX-author", "Nasca Octavian Paul");
        information.yoshiType = 0;
    }

    node = root;
    mxmlElementSetAttr(root, "Yoshimi-author", "Alan Ernest Calvert");
    std::string tmp = YOSHIMI_VERSION;
    std::string::size_type pos1 = tmp.find('.'); // != string::npos
    std::string::size_type pos2 = tmp.find('.',pos1+1);
    mxmlElementSetAttr(root, "Yoshimi-major", tmp.substr(0, pos1).c_str());
    mxmlElementSetAttr(root, "Yoshimi-minor", tmp.substr(pos1+1, pos2-pos1-1).c_str());

    info = addparams0("INFORMATION"); // specifications

    if (synth->getRuntime().xmlType == TOPLEVEL::XML::MasterConfig)
    {
        beginbranch("BASE_PARAMETERS");
            addparbool("enable_gui", synth->getRuntime().showGui);
            addparbool("enable_splash", synth->getRuntime().showSplash);
            addparbool("enable_CLI", synth->getRuntime().showCli);
            addparbool("enable_single_master", synth->getRuntime().singlePath);
            addparbool("banks_checked", synth->getRuntime().banksChecked);
            addparbool("enable_auto_instance", synth->getRuntime().autoInstance);
            addparU("active_instances", synth->getRuntime().activeInstance);
            addpar("show_CLI_context", synth->getRuntime().showCLIcontext);
            addpar("gzip_compression", synth->getRuntime().GzipCompression);

            for (int i = 0; i < MAX_PRESET_DIRS; ++i)
            {
                if (synth->getRuntime().presetsDirlist[i].size())
                {
                    beginbranch("PRESETSROOT",i);
                    addparstr("presets_root", synth->getRuntime().presetsDirlist[i]);
                    endbranch();
                }
            }

        endbranch();
        return;
    }

    if (synth->getRuntime().xmlType <= TOPLEVEL::XML::Scale)
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
}


XMLwrapper::~XMLwrapper()
{
    if (tree)
        mxmlDelete(tree);
}


void XMLwrapper::checkfileinformation(const std::string& filename, unsigned int& names, int& type)
{
    stackpos = 0; // we don't seem to be using any of this!
    memset(&parentstack, 0, sizeof(parentstack));
    if (tree)
        mxmlDelete(tree);
    tree = NULL;


    std::string report = "";
    char *xmldata = loadGzipped(filename, &report);
    if (report != "")
        synth->getRuntime().Log(report, 2);
    if (!xmldata)
        return;

    char *first = strstr(xmldata, "<!DOCTYPE Yoshimi-data>");
    information.yoshiType = (first!= NULL);
    char *start = strstr(xmldata, "<INFORMATION>");
    char *end = strstr(xmldata, "</INFORMATION>");
    char *idx = start;
    unsigned int seen = 0;

    if (start && end && start < end)
    {
        // Andrew: just make it simple
        // Will: but not too simple :)

        /*
         * the following could be in any order. We are checking for
        * the actual existence of the fields as well as their value.
        */
        idx = strstr(start, "name=\"ADDsynth_used\"");
        if (idx != NULL)
        {
            seen |= 2;
            if (strstr(idx, "name=\"ADDsynth_used\" value=\"yes\""))
                information.ADDsynth_used = 1;
        }

        idx = strstr(start, "name=\"SUBsynth_used\"");
        if (idx != NULL)
        {
            seen |= 4;
            if (strstr(idx, "name=\"SUBsynth_used\" value=\"yes\""))
                information.SUBsynth_used = 1;
        }

        idx = strstr(start, "name=\"PADsynth_used\"");
        if (idx != NULL)
        {
            seen |= 1;
            if (strstr(idx, "name=\"PADsynth_used\" value=\"yes\""))
                information.PADsynth_used = 1;
        }
    }

    idx = strstr(xmldata, "<INFO>");
    if (idx == NULL)
        return;
    idx = strstr(idx, "par name=\"type\" value=\"");
    if (idx != NULL)
        type = string2int(idx + 23);

    if (seen != 7) // at least one was missing
        slowinfosearch(xmldata);
    delete [] xmldata;
    names = information.ADDsynth_used | (information.SUBsynth_used << 1) | (information.PADsynth_used << 2) | (information.yoshiType << 3);
    return;
}


void XMLwrapper::slowinfosearch(char *idx)
{
    idx = strstr(idx, "<INSTRUMENT_KIT>");
    if (idx == NULL)
        return;

    std::string mark;
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
            return;
        }
    }
  return;
}


// SAVE XML members

bool XMLwrapper::saveXMLfile(std::string _filename, bool useCompression)
{
    std::string filename = _filename;
    char *xmldata = getXMLdata();

    if (!xmldata)
    {
        synth->getRuntime().Log("XML: Failed to allocate xml data space");
        return false;
    }

    unsigned int compression = 0;
    if (useCompression)
        compression = synth->getRuntime().GzipCompression;
    if (compression <= 0)
    {
        if (!saveText(xmldata, filename))
        {
            synth->getRuntime().Log("XML: Failed to save xml file " + filename + " for save", 2);
            return false;
        }
    }
    else
    {
        if (compression > 9)
            compression = 9;
        std::string result = saveGzipped(xmldata, filename, compression);
        if (result > "")
        {
            synth->getRuntime().Log(result,2);
            return false;
        }
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
        case TOPLEVEL::XML::Instrument:
            addparbool("ADDsynth_used", (information.ADDsynth_used != 0));
            addparbool("SUBsynth_used", (information.SUBsynth_used != 0));
            addparbool("PADsynth_used", (information.PADsynth_used != 0));
            break;

        case TOPLEVEL::XML::Patch:
            addparstr("XMLtype", "Parameters");
            break;

        case TOPLEVEL::XML::Scale:
            addparstr("XMLtype", "Scales");
            break;

        case TOPLEVEL::XML::State:
            addparstr("XMLtype", "Session");
            break;

        case TOPLEVEL::XML::Vector:
            addparstr("XMLtype", "Vector Control");
            break;

        case TOPLEVEL::XML::MLearn:
            addparstr("XMLtype", "Midi Learn");
            break;

        case TOPLEVEL::XML::MasterConfig:
            addparstr("XMLtype", "Config Base");
            break;

        case TOPLEVEL::XML::Config:
            addparstr("XMLtype", "Config Instance");
            break;

        case TOPLEVEL::XML::Presets:
            addparstr("XMLtype", "Presets");
            break;

        case TOPLEVEL::XML::Bank:
        {
            addparstr("XMLtype", "Roots and Banks");
            addpar("Banks_Version", synth->bank.readVersion());
            break;
        }

        case TOPLEVEL::XML::History:
            addparstr("XMLtype", "Recent Files");
            break;

        default:
            addparstr("XMLtype", "Unknown");
            break;
    }
    node = oldnode;
    char *xmldata = mxmlSaveAllocString(tree, XMLwrapper_whitespace_callback);
    return xmldata;
}


void XMLwrapper::addparU(const std::string& name, unsigned int val)
{
    addparams2("parU", "name", name.c_str(), "value", asString(val));
}


void XMLwrapper::addpar(const std::string& name, int val)
{
    addparams2("par", "name", name.c_str(), "value", asString(val));
}


void XMLwrapper::addparreal(const std::string& name, float val)
{
    union { float in; uint32_t out; } convert;
    char buf[11];
    convert.in = val;
    sprintf(buf, "0x%8X", convert.out);
    addparams3("par_real", "name", name.c_str(), "value", asLongString(val), "exact_value", buf);
}


void XMLwrapper::addpardouble(const std::string& name, double val)
{
    addparams2("par_real","name", name.c_str(), "value", asLongString(val));
}


void XMLwrapper::addparbool(const std::string& name, int val)
{
    if (val != 0)
        addparams2("par_bool", "name", name.c_str(), "value", "yes");
    else
        addparams2("par_bool", "name", name.c_str(), "value", "no");
}


void XMLwrapper::addparstr(const std::string& name, const std::string& val)
{
    mxml_node_t *element = mxmlNewElement(node, "string");
    mxmlElementSetAttr(element, "name", name.c_str());
    mxmlNewText(element, 0, val.c_str());
}


void XMLwrapper::beginbranch(const std::string& name)
{
    push(node);
    node = addparams0(name.c_str());
}


void XMLwrapper::beginbranch(const std::string& name, int id)
{
    push(node);
    node = addparams1(name.c_str(), "id", asString(id));
}


void XMLwrapper::endbranch()
{
    node = pop();
}

// LOAD XML members
bool XMLwrapper::loadXMLfile(const std::string& filename)
{
    bool zynfile = true;
    bool yoshitoo = false;

    if (tree)
        mxmlDelete(tree);
    tree = NULL;
    memset(&parentstack, 0, sizeof(parentstack));
    stackpos = 0;
    std::string report = "";
    char *xmldata = loadGzipped(filename, &report);
    if (report != "")
        synth->getRuntime().Log(report, 2);
    if (xmldata == NULL)
    {
        synth->getRuntime().Log("XML: Could not load xml file: " + filename, 2);
         return false;
    }
    root = tree = mxmlLoadString(NULL, removeBlanks(xmldata), MXML_OPAQUE_CALLBACK);
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
    synth->fileCompatible = true;
    if (zynfile)
    {
        xml_version.major = string2int(mxmlElementGetAttr(root, "version-major"));
        xml_version.minor = string2int(mxmlElementGetAttr(root, "version-minor"));
    }
    if (mxmlElementGetAttr(root, "Yoshimi-major"))
    {
        xml_version.y_major = string2int(mxmlElementGetAttr(root, "Yoshimi-major"));
        yoshitoo = true;

        //synth->getRuntime().Log("XML: Yoshimi " + asString(xml_version.y_major) + "  " + filename);
    }
    else
    {
        synth->getRuntime().lastXMLmajor = 0;
        if (xml_version.major > 2)
            synth->fileCompatible = false;
    }
// std::cout << "major " << int(xml_version.major) << "  Yosh " << int(synth->getRuntime().lastXMLmajor) << std::endl;
    if (mxmlElementGetAttr(root, "Yoshimi-minor"))
    {
        xml_version.y_minor = string2int(mxmlElementGetAttr(root, "Yoshimi-minor"));

//        synth->getRuntime().Log("XML: Yoshimi " + asString(xml_version.y_minor));
    }
    else
        synth->getRuntime().lastXMLminor = 0;
    string exten = findExtension(filename);
    if (exten.length() != 4 && exten != ".state")
        return true; // we don't want config stuff

    if (synth->getRuntime().logXMLheaders)
    {
        if (yoshitoo && xml_version.major > 2)
        { // we were giving the wrong value :(
            xml_version.major = 2;
            xml_version.minor = 4;
        }
        if (zynfile)
            synth->getRuntime().Log("ZynAddSubFX version major " + asString(xml_version.major) + "   minor " + asString(xml_version.minor));
        if (yoshitoo)
            synth->getRuntime().Log("Yoshimi version major " + asString(xml_version.y_major) + "   minor " + asString(xml_version.y_minor));
    }
    return true;
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


bool XMLwrapper::enterbranch(const std::string& name)
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


bool XMLwrapper::enterbranch(const std::string& name, int id)
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


unsigned int XMLwrapper::getparU(const std::string& name, unsigned int defaultpar, unsigned int min, unsigned int max)
{
    node = mxmlFindElement(peek(), peek(), "parU", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!node)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(node, "value");
    if (!strval)
        return defaultpar;
    unsigned int val = string2int(strval);
    if (val < min)
        val = min;
    else if (val > max)
        val = max;
    return val;
}


int XMLwrapper::getpar(const std::string& name, int defaultpar, int min, int max)
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


int XMLwrapper::getpar127(const std::string& name, int defaultpar)
{
    return(getpar(name, defaultpar, 0, 127));
}


int XMLwrapper::getpar255(const std::string& name, int defaultpar)
{
    return(getpar(name, defaultpar, 0, 255));
}


int XMLwrapper::getparbool(const std::string& name, int defaultpar)
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
// case insensitive, anything other than '0', 'no', 'false' is treated as 'true'


std::string XMLwrapper::getparstr(const std::string& name)
{
    node = mxmlFindElement(peek(), peek(), "string", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!node)
        return std::string();
    mxml_node_t *child = mxmlGetFirstChild(node);
    if (!child)
        return std::string();
    if (mxmlGetType(child) != MXML_OPAQUE)
        return std::string();
    return std::string(mxmlGetOpaque(child));
}


float XMLwrapper::getparreal(const std::string& name, float defaultpar)
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
    return string2float(std::string(strval));
}


float XMLwrapper::getparreal(const std::string& name, float defaultpar, float min, float max)
{
    float result = getparreal(name, defaultpar);
    if (result < min)
        result = min;
    else if (result > max)
        result = max;
    return result;
}


// Private parts

mxml_node_t *XMLwrapper::addparams0(const std::string& name)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams1(const std::string& name, const std::string& par1, const std::string& val1)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams2(const std::string& name, const std::string& par1, const std::string& val1,
                                    const std::string& par2, const std::string& val2)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    mxmlElementSetAttr(element, par2.c_str(), val2.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams3(const std::string& name, const std::string& par1, const std::string& val1,
                                    const std::string& par2, const std::string& val2,
                                    const std::string& par3, const std::string& val3)
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
