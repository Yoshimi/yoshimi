/*
    XMLStore.cpp - Store structured data in XML

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2025, Will Godfrey
    Copyright 2025,      Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
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


#include "Misc/Config.h"
#include "Misc/XMLStore.h"
#include "Misc/SynthEngine.h"                  //////////////////////////////////////////////////////////////TODO 4/25 : why?? should never be linked to Yoshimi core logic
#include "Misc/FileMgrFuncs.h"
#include "Misc/FormatFuncs.h"

#include <sys/types.h>                  ///////////////////////////////////////TODO 4/25 : why?
#include <utility>
#include <cassert>
#include <mxml.h>
#include <zlib.h>
#include <sstream>
#include <string>

using file::saveText;
using file::loadGzipped;
using file::saveGzipped;
using file::findExtension;
using func::string2int;
using func::string2uint;
using func::string2float;
using func::asLongString;
using func::asString;

using std::string;
using std::move;


namespace { // internal details of MXML integration

    /** Helper to fit with MXML's optionally-NULL char* arguments */
    class OStr
    {
        string rendered{};

    public:
        OStr() = default;
        // standard copy operations acceptable

        OStr(string str)
            : rendered{move(str)}
            { }

        template<typename X>
        OStr(X x)
            : rendered{func::asString(x)}
            { }

        operator bool()         const { return rendered.size(); }
        operator const char*()  const { return rendered.size()? rendered.c_str() : nullptr; }
    };


    /** Configuration how to adapt and use the MXML API */
    struct Policy
    {
        mxml_node_t* mxmlElm()
        {
            return reinterpret_cast<mxml_node_t*>(this);
        }

        mxml_node_t* searchChild(OStr elmName    =OStr()
                                ,OStr attribName =OStr()
                                ,OStr attribVal  =OStr())
        {
            return mxmlFindElement(mxmlElm(), mxmlElm(), elmName, attribName, attribVal, MXML_DESCEND_FIRST);
        }
    };
}//(End)internal details


/** Abstracted access point to an MXML tree representation */
struct XMLtree::Node
    : Policy
    {
        static Node* asNode(mxml_node_t* mxmlElm)
        {
            return reinterpret_cast<Node*>(mxmlElm);
        }

        void addRef()
        {
            uint refCnt = mxmlRetain(mxmlElm());
            assert(refCnt > 1);
        }

        void unref()
        {   //  decrement refcount -- possibly delete
            mxmlRelease(mxmlElm());
        }

        Node* addChild(OStr elmName)
        {
            return asNode(mxmlNewElement(mxmlElm(), elmName));
        }

        Node* findChild(OStr elmName, OStr id =OStr())
        {
            OStr optIDAttrib{string{id? "id":nullptr}};
            return asNode(searchChild(elmName, optIDAttrib, id));
        }

        Node& setAttrib(OStr attribName, OStr val)
        {
            mxmlElementSetAttr(mxmlElm(), attribName, val);
            return *this;
        }
        Node& setText(OStr content)
        {
            bool leadingWhitespace{false};
            mxmlNewText(mxmlElm(), leadingWhitespace, content);
            return *this;
        }

        const char * getAttrib(OStr attribName)
        {
            return mxmlElementGetAttr(mxmlElm(), attribName);
        }

        const char * getText()
        {
            mxml_node_t* child = mxmlGetFirstChild(mxmlElm());
            if (child and MXML_OPAQUE == mxmlGetType(child))
                return mxmlGetOpaque(child);
            else
                return nullptr;
        }
    };




/* ===== XMLtree implementation backend ===== */

XMLtree::~XMLtree()
{
    if (node)
        node->unref();
}

XMLtree::XMLtree(Node* treeLocation)
    : node{treeLocation}
{
    if (node)
        node->addRef();
}

XMLtree::XMLtree(XMLtree&& ref)
    : node{nullptr}
{
    std::swap(node, ref.node);
}


XMLtree XMLtree::addElm(string name)
{
    assert(node);
    return node->addChild(name);
}

XMLtree XMLtree::getElm(string name)
{
    return XMLtree{node? node->findChild(name) : nullptr};
}

XMLtree XMLtree::getElm(string name, int id)
{
    return XMLtree{node? node->findChild(name, id) : nullptr};
}


// add simple parameter: name, value
void XMLtree::addPar_int(string const& name, int val)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

// add unsigned integer parameter: name, value
void XMLtree::addPar_uint(string const& name, uint val)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

// add float parameter persisted as fixed bitstring: name, value
void XMLtree::addPar_float(string const& name, float val)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

void XMLtree::addPar_real(string const& name, float val)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

void XMLtree::addPar_bool(string const& name, bool val)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

// add string parameter (name and string)
void XMLtree::addPar_str(string const& name, string const& val)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

int  XMLtree::getPar_int(string const& name, int defaultVal, int min, int max)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

// value limited to [0 ... 127]
int  XMLtree::getPar_127(string const& name, int defaultVal)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

// value limited to [0 ... 255]
int  XMLtree::getPar_255(string const& name, int defaultVal)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

uint XMLtree::getPar_uint(string const& name, uint defaultVal, uint min, uint max)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

float XMLtree::getPar_float(string const& name, float defaultVal, float min, float max)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

float XMLtree::getPar_real(string const& name, float defaultVal, float min, float max)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

float XMLtree::getPar_real(string const& name, float defaultVal)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

bool  XMLtree::getPar_bool(string const& name, bool defaultVal)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}

string XMLtree::getPar_str(string const& name)
{
    ///////////////////////////////////////////////////////OOO relocate impl. from XMLStore
}




/* ===== XMLStore implementation ===== */



const char *XMLStore_whitespace_callback(mxml_node_t* node, int where)
{
    const char *name = mxmlGetElement(node);

    if (where == MXML_WS_BEFORE_OPEN && name && !strncmp(name, "?xml", 4))
        return NULL;
    if (where == MXML_WS_BEFORE_CLOSE && name && !strncmp(name, "string", 6))
        return NULL;

    if (where == MXML_WS_BEFORE_OPEN || where == MXML_WS_BEFORE_CLOSE)
        return "\n";
    return NULL;
}


XMLStore::XMLStore(TOPLEVEL::XML type, SynthEngine& _synth, bool yoshiFormat):
    stackpos(0),
    xml_k(0),
    isYoshi(yoshiFormat),
    synth(_synth)
{
    minimal = not synth.getRuntime().xmlmax;
    information.PADsynth_used = 0;
    information.ADDsynth_used = 0;
    information.SUBsynth_used = 0;
    information.yoshiType = true;
    information.type = type;
    memset(&parentstack, 0, sizeof(parentstack));

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////OOO : stateful tree position logic broken. MUST GET RID of this "current position"
    treeX = nullptr;  // boom
    rootX = nullptr;  // BOOOOM
    nodeX = nullptr;  // BOOOOOOM
    infoX = nullptr;  // BOOOOOOOOOM
}


void XMLStore::buildXMLroot()
{
    treeX = mxmlNewElement(MXML_NO_PARENT, "?xml version=\"1.0\" encoding=\"UTF-8\"?");
    mxml_node_t *doctype = mxmlNewElement(treeX, "!DOCTYPE");

    if (isYoshi)
    {
        mxmlElementSetAttr(doctype, "Yoshimi-data", NULL);
        rootX = mxmlNewElement(treeX, "Yoshimi-data");
        information.yoshiType = 1;
    }
    else
    {
        mxmlElementSetAttr(doctype, "ZynAddSubFX-data", NULL);
        rootX = mxmlNewElement(treeX, "ZynAddSubFX-data");
        mxmlElementSetAttr(rootX, "version-major", "2");
        mxmlElementSetAttr(rootX, "version-minor", "4");
        mxmlElementSetAttr(rootX, "version-revision", "1");
        mxmlElementSetAttr(rootX, "ZynAddSubFX-author", "Nasca Octavian Paul");
        information.yoshiType = 0;
    }

    nodeX = rootX;
    mxmlElementSetAttr(rootX, "Yoshimi-author", "Alan Ernest Calvert");
    string version{YOSHIMI_VERSION};
    size_t pos = version.find(' ');
    if (pos != string::npos)
        version = version.substr(0, pos); // might be an rc or M version.

    string major    = "2";
    string minor    = "0";
    string revision = "0";

    pos = version.find('.');
    if (pos == string::npos)
        major = version;
    else
    {
        major = version.substr(0, pos);
        version = version.substr(pos + 1, version.length());

        pos = version.find('.');
        if (pos == string::npos)
            minor = version;
        else
        {
            minor = version.substr(0, pos);
            version = version.substr(pos + 1, version.length());

            pos = version.find('.');
            if (pos == string::npos)
                revision = version;
            else
                revision = version.substr(0, pos);
        }
    }
    mxmlElementSetAttr(rootX, "Yoshimi-major", major.c_str());
    mxmlElementSetAttr(rootX, "Yoshimi-minor", minor.c_str());
    mxmlElementSetAttr(rootX, "Yoshimi-revision", revision.c_str());

    infoX = addparams0("INFORMATION"); // specifications

    if (synth.getRuntime().xmlType == TOPLEVEL::XML::MasterConfig)
    {
        beginbranch("BASE_PARAMETERS");
            addparbool("enable_gui", synth.getRuntime().storedGui);
            addparbool("enable_splash", synth.getRuntime().showSplash);
            addparbool("enable_CLI", synth.getRuntime().storedCli);
            addpar("show_CLI_context", synth.getRuntime().showCLIcontext);
            addparbool("enable_single_master", synth.getRuntime().singlePath);
            addparbool("enable_auto_instance", synth.getRuntime().autoInstance);
            addparU("handle_padsynth_build", synth.getRuntime().handlePadSynthBuild);
            addpar("gzip_compression", synth.getRuntime().gzipCompression);
            addparbool("banks_checked", synth.getRuntime().banksChecked);
            addparU("active_instances", synth.getRuntime().activeInstances.to_ulong());
            addparstr("guide_version", synth.getRuntime().guideVersion);
            addparstr("manual", synth.getRuntime().manualFile);
        endbranch();
        return;
    }

    if (synth.getRuntime().xmlType <= TOPLEVEL::XML::Scale)
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


XMLStore::~XMLStore()
{
    if (treeX)
        mxmlDelete(treeX);
}


void XMLStore::checkfileinformation(string const& filename, uint& names, int& type)
{
    stackpos = 0; // we don't seem to be using any of this!
    memset(&parentstack, 0, sizeof(parentstack));
    if (treeX)
        mxmlDelete(treeX);
    treeX = NULL;


    string report;
    char *xmldata = loadGzipped(filename, &report);
    if (not report.empty())
        synth.getRuntime().Log(report, _SYS_::LogNotSerious);
    if (!xmldata)
        return;
    char* first = strstr(xmldata, "<!DOCTYPE Yoshimi-data>");
    information.yoshiType = (first!= NULL);
    char* start = strstr(xmldata, "<INFORMATION>");
    char* end = strstr(xmldata, "</INFORMATION>");
    char* idx = start;
    uint seen = 0;

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


void XMLStore::slowinfosearch(char *idx)
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
            return;
        }
    }
  return;
}


// SAVE XML members

bool XMLStore::saveXMLfile(string _filename, bool useCompression)
{
    string filename{_filename};
    char* xmldata = getXMLdata();

    if (!xmldata)
    {
        synth.getRuntime().Log("XML: Failed to allocate xml data space");
        return false;
    }

    uint compression = 0;
    if (useCompression)
        compression = synth.getRuntime().gzipCompression;
    if (compression <= 0)
    {
        if (!saveText(xmldata, filename))
        {
            synth.getRuntime().Log("XML: Failed to save xml file " + filename + " for save", _SYS_::LogNotSerious);
            return false;
        }
    }
    else
    {
        if (compression > 9)
            compression = 9;
        string result = saveGzipped(xmldata, filename, compression);
        if (result > "")
        {
            synth.getRuntime().Log(result, _SYS_::LogNotSerious);
            return false;
        }
    }
    free(xmldata);
    return true;
}


char *XMLStore::getXMLdata()
{
    xml_k = 0;
    memset(tabs, 0, STACKSIZE + 2);
    mxml_node_t *oldnode=nodeX;
    nodeX = infoX;

    switch (synth.getRuntime().xmlType)
    {
        case TOPLEVEL::XML::Instrument:
        {
            addparbool("ADDsynth_used", (information.ADDsynth_used != 0));
            addparbool("SUBsynth_used", (information.SUBsynth_used != 0));
            addparbool("PADsynth_used", (information.PADsynth_used != 0));
            break;
        }
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
        case TOPLEVEL::XML::MasterUpdate:
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
            addpar("Banks_Version", synth.bank.readVersion());
            break;
        }

        case TOPLEVEL::XML::History:
            addparstr("XMLtype", "Recent Files");
            break;

        case TOPLEVEL::XML::PresetDirs:
            addparstr("XMLtype", "Preset Directories");
            break;

        default:
            addparstr("XMLtype", "Unknown");
            break;
    }
    nodeX = oldnode;
    char *xmldata = mxmlSaveAllocString(treeX, XMLStore_whitespace_callback);
    return xmldata;
}


XMLtree XMLStore::addElm(string name)
{
/////////////////////////////////////////////////OOO silently handle root element and then delegate to the XMLtree API
}

XMLtree XMLStore::getElm(string name)
{
/////////////////////////////////////////////////OOO silently tolerate empty root, otherwise navigate and then delegate to XMLtree API
}


void XMLStore::addparU(string const& name, uint val)
{
    addparams2("parU", "name", name.c_str(), "value", asString(val));
}


void XMLStore::addpar(string const& name, int val)
{
    addparams2("par", "name", name.c_str(), "value", asString(val));
}


void XMLStore::addparcombi(string const& name, float val)
{
    union { float in; uint32_t out; } convert;
    char buf[11];
    convert.in = val;
    sprintf(buf, "0x%8X", convert.out);
    addparams3("par", "name", name.c_str(), "value", asString(lrintf(val)), "exact_value", buf);
}


void XMLStore::addparreal(string const& name, float val)
{
    union { float in; uint32_t out; } convert;
    char buf[11];
    convert.in = val;
    sprintf(buf, "0x%8X", convert.out);
    addparams3("par_real", "name", name.c_str(), "value", asLongString(val), "exact_value", buf);
}


void XMLStore::addpardouble(string const& name, double val)
{
    addparams2("par_real","name", name.c_str(), "value", asLongString(val));
}


void XMLStore::addparbool(string const& name, int val)
{
    if (val != 0)
        addparams2("par_bool", "name", name.c_str(), "value", "yes");
    else
        addparams2("par_bool", "name", name.c_str(), "value", "no");
}


void XMLStore::addparstr(string const& name, string const& val)
{
    mxml_node_t *element = mxmlNewElement(nodeX, "string");
    mxmlElementSetAttr(element, "name", name.c_str());
    mxmlNewText(element, 0, val.c_str());
}


void XMLStore::beginbranch(string const& name)
{
    push(nodeX);
    nodeX = addparams0(name.c_str());
}


void XMLStore::beginbranch(string const& name, int id)
{
    push(nodeX);
    nodeX = addparams1(name.c_str(), "id", asString(id));
}


void XMLStore::endbranch()
{
    nodeX = pop();
}

// LOAD XML members
bool XMLStore::loadXMLfile(string const& filename)
{
    bool zynfile = true;
    bool yoshitoo = false;

    if (treeX)
        mxmlDelete(treeX);
    treeX = NULL;
    nodeX = NULL;
    infoX = NULL;
    memset(&parentstack, 0, sizeof(parentstack));
    stackpos = 0;
    string report = "";
    char* xmldata = loadGzipped(filename, &report);
    if (report != "")
        synth.getRuntime().Log(report, _SYS_::LogNotSerious);
    if (xmldata == NULL)
    {
        synth.getRuntime().Log("XML: Could not load xml file: " + filename, _SYS_::LogNotSerious);
         return false;
    }
    rootX = treeX = mxmlLoadString(NULL, removeBlanks(xmldata), MXML_OPAQUE_CALLBACK);
    delete [] xmldata;
    if (!treeX)
    {
        synth.getRuntime().Log("XML: File " + filename + " is not XML", _SYS_::LogNotSerious);
        return false;
    }
    rootX = mxmlFindElement(treeX, treeX, "ZynAddSubFX-data", NULL, NULL, MXML_DESCEND);
    if (!rootX)
    {
        zynfile = false;
        rootX = mxmlFindElement(treeX, treeX, "Yoshimi-data", NULL, NULL, MXML_DESCEND);
    }

    if (!rootX)
    {
        synth.getRuntime().Log("XML: File " + filename + " doesn't contain valid data in this context", _SYS_::LogNotSerious);
        return false;
    }
    nodeX = rootX;
    push(rootX);
    infoX = mxmlFindElement(rootX, rootX, "INFORMATION", NULL, NULL, MXML_DESCEND_FIRST);
    if (!infoX)  // container to hold meta-information (xml type)
        infoX = addparams0("INFORMATION");

    synth.fileCompatible = true;
    if (zynfile)
    {
        xml_version.major = string2int(mxmlElementGetAttr(rootX, "version-major"));
        xml_version.minor = string2int(mxmlElementGetAttr(rootX, "version-minor"));
        if(mxmlElementGetAttr(rootX, "version-revision") != NULL)
            xml_version.revision = string2int(mxmlElementGetAttr(rootX, "version-revision"));
        else
            xml_version.revision = 0;
    }
    if (mxmlElementGetAttr(rootX, "Yoshimi-major"))
    {
        xml_version.y_major = string2int(mxmlElementGetAttr(rootX, "Yoshimi-major"));
        yoshitoo = true;
    }
    else
    {
        synth.getRuntime().lastXMLmajor = 0;
        if (xml_version.major > 2)
            synth.fileCompatible = false;
    }
    if (mxmlElementGetAttr(rootX, "Yoshimi-minor"))
    {
        xml_version.y_minor = string2int(mxmlElementGetAttr(rootX, "Yoshimi-minor"));
        if (mxmlElementGetAttr(rootX, "Yoshimi-revision") != NULL)
            xml_version.y_revision = string2int(mxmlElementGetAttr(rootX, "Yoshimi-revision"));
        else
            xml_version.y_revision = 0;
    }
    else
        synth.getRuntime().lastXMLminor = 0;
    string exten = findExtension(filename);
    if (exten.length() != 4 && exten != EXTEN::state)
        return true; // we don't want config stuff

    if (synth.getRuntime().logXMLheaders)
    {
        if (yoshitoo && xml_version.major > 2)
        { // we were giving the wrong value :(
            xml_version.major = 2;
            xml_version.minor = 4;
            xml_version.revision = 1;
        }
        if (zynfile)
        {
            string text = "ZynAddSubFX version major " + asString(xml_version.major) + ", minor " + asString(xml_version.minor);
            if (xml_version.revision > 0)
                text += (", revision " + asString(xml_version.revision));
            synth.getRuntime().Log(text);
        }
        if (yoshitoo)
        {
            string text = "Yoshimi version major " + asString(xml_version.y_major) + ", minor " + asString(xml_version.y_minor);
            if (xml_version.y_revision > 0)
                text += (", revision " + asString(xml_version.y_revision));
            synth.getRuntime().Log(text);
        }
    }
    return true;
}


bool XMLStore::putXMLdata(const char *xmldata)
{
    if (treeX)
        mxmlDelete(treeX);
    treeX = NULL;
    memset(&parentstack, 0, sizeof(parentstack));
    stackpos = 0;
    if (xmldata == NULL)
        return false;
    rootX = treeX = mxmlLoadString(NULL, xmldata, MXML_OPAQUE_CALLBACK);
    if (treeX == NULL)
        return false;
    rootX = mxmlFindElement(treeX, treeX, "ZynAddSubFX-data", NULL, NULL, MXML_DESCEND);
    if (!rootX)
        rootX = mxmlFindElement(treeX, treeX, "Yoshimi-data", NULL, NULL, MXML_DESCEND);
    nodeX = rootX;
    if (!rootX)
        return false;
    push(rootX);
    return true;
}


bool XMLStore::enterbranch(string const& name)
{
    nodeX = mxmlFindElement(peek(), peek(), name.c_str(), NULL, NULL,
                           MXML_DESCEND_FIRST);
    if (!nodeX)
        return false;
    push(nodeX);
    if (name == "CONFIGURATION")
    {
        synth.getRuntime().lastXMLmajor = xml_version.y_major;
        synth.getRuntime().lastXMLminor = xml_version.y_minor;
    }
    return true;
}


bool XMLStore::enterbranch(string const& name, int id)
{
    nodeX = mxmlFindElement(peek(), peek(), name.c_str(), "id",
                           asString(id).c_str(), MXML_DESCEND_FIRST);
    if (!nodeX)
        return false;
    push(nodeX);
    return true;
}


int XMLStore::getbranchid(int min, int max)
{
    int id = string2int(mxmlElementGetAttr(nodeX, "id"));
    if (min == 0 && max == 0)
        return id;
    if (id < min)
        id = min;
    else if (id > max)
        id = max;
    return id;
}


uint XMLStore::getparU(string const& name, uint defaultpar, uint min, uint max)
{
    nodeX = mxmlFindElement(peek(), peek(), "parU", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!nodeX)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(nodeX, "value");
    if (!strval)
        return defaultpar;
    uint val = string2uint(strval);
    if (val < min)
        val = min;
    else if (val > max)
        val = max;
    return val;
}


int XMLStore::getpar(string const& name, int defaultpar, int min, int max)
{
    nodeX = mxmlFindElement(peek(), peek(), "par", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!nodeX)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(nodeX, "value");
    if (!strval)
        return defaultpar;
    int val = string2int(strval);
    if (val < min)
        val = min;
    else if (val > max)
        val = max;
    return val;
}


float XMLStore::getparcombi(string const& name, float defaultpar, float min, float max)
{
    nodeX = mxmlFindElement(peek(), peek(), "par", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!nodeX)
        return defaultpar;
    float result = 0;
    const char *strval = mxmlElementGetAttr(nodeX, "exact_value");
    if (strval != NULL)
    {
        union { float out; uint32_t in; } convert;
        sscanf(strval+2, "%x", &convert.in);
        result = convert.out;
    }
    else
    {
        strval = mxmlElementGetAttr(nodeX, "value");
        if (!strval)
        return defaultpar;
        result = string2float(string(strval));
    }
    if (result < min)
        result = min;
    else if (result > max)
        result = max;
    return result;
}


int XMLStore::getpar127(string const& name, int defaultpar)
{
    return(getpar(name, defaultpar, 0, 127));
}


int XMLStore::getpar255(string const& name, int defaultpar)
{
    return(getpar(name, defaultpar, 0, 255));
}


int XMLStore::getparbool(string const& name, int defaultpar)
{
    nodeX = mxmlFindElement(peek(), peek(), "par_bool", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!nodeX)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(nodeX, "value");
    if (!strval)
        return defaultpar;
    char tmp = strval[0] | 0x20;
    return (tmp != '0' && tmp != 'n' && tmp != 'f') ? 1 : 0;
}
// case insensitive, anything other than '0', 'no', 'false' is treated as 'true'


string XMLStore::getparstr(string const& name)
{
    nodeX = mxmlFindElement(peek(), peek(), "string", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (!nodeX)
        return string();
    mxml_node_t *child = mxmlGetFirstChild(nodeX);
    if (!child)
        return string();
    if (mxmlGetType(child) != MXML_OPAQUE)
        return string();
    return string(mxmlGetOpaque(child));
}


float XMLStore::getparreal(string const& name, float defaultpar)
{
    nodeX = mxmlFindElement(peek(), peek(), "par_real", "name", name.c_str(),
                           MXML_DESCEND_FIRST);
    if (!nodeX)
        return defaultpar;

    const char *strval = mxmlElementGetAttr(nodeX, "exact_value");
    if (strval != NULL)
    {
        union { float out; uint32_t in; } convert;
        sscanf(strval+2, "%x", &convert.in);
        return convert.out;
    }

    strval = mxmlElementGetAttr(nodeX, "value");
    if (!strval)
        return defaultpar;
    return string2float(string(strval));
}


float XMLStore::getparreal(string const& name, float defaultpar, float min, float max)
{
    float result = getparreal(name, defaultpar);
    if (result < min)
        result = min;
    else if (result > max)
        result = max;
    return result;
}


// Private parts

mxml_node_t *XMLStore::addparams0(string const& name)
{
    mxml_node_t *element = mxmlNewElement(nodeX, name.c_str());
    return element;
}


mxml_node_t *XMLStore::addparams1(string const& name, string const& par1, string const& val1)
{
    mxml_node_t *element = mxmlNewElement(nodeX, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    return element;
}


mxml_node_t *XMLStore::addparams2(string const& name, string const& par1, string const& val1,
                                    string const& par2, string const& val2)
{
    mxml_node_t *element = mxmlNewElement(nodeX, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    mxmlElementSetAttr(element, par2.c_str(), val2.c_str());
    return element;
}


mxml_node_t *XMLStore::addparams3(string const& name, string const& par1, string const& val1,
                                    string const& par2, string const& val2,
                                    string const& par3, string const& val3)
{
    mxml_node_t *element = mxmlNewElement(nodeX, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    mxmlElementSetAttr(element, par2.c_str(), val2.c_str());
    mxmlElementSetAttr(element, par3.c_str(), val3.c_str());
    return element;
}


void XMLStore::push(mxml_node_t *node)
{
    if (stackpos >= STACKSIZE - 1)
    {
        synth.getRuntime().Log("XML: Not good, XMLwrapper push on a full parentstack", _SYS_::LogNotSerious);
        return;
    }
    stackpos++;
    parentstack[stackpos] = node;
}


mxml_node_t *XMLStore::pop()
{
    if (stackpos <= 0)
    {
        synth.getRuntime().Log("XML: Not good, XMLwrapper pop on empty parentstack", _SYS_::LogNotSerious);
        return rootX;
    }
    mxml_node_t *node = parentstack[stackpos];
    parentstack[stackpos] = NULL;
    stackpos--;
    return node;
}


mxml_node_t *XMLStore::peek()
{
    if (stackpos <= 0)
    {
        synth.getRuntime().Log("XML: Not good, XMLwrapper peek on an empty parentstack", _SYS_::LogNotSerious);
        return rootX;
    }
    return parentstack[stackpos];
}
