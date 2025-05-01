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
#include <mxml.h>
#include <zlib.h>
#include <cassert>
#include <utility>
#include <algorithm>
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

    const auto XML_HEADER = "?xml version=\"1.0\" encoding=\"UTF-8\"?";
    const auto ROOT_ZYN   = "ZynAddSubFX-data";
    const auto ROOT_YOSHI = "Yoshimi-data";

    auto topElmName(XMLStore::Metadata const& meta)
    {
        return meta.isZynCompat()? ROOT_ZYN : ROOT_YOSHI;
    }

    string renderXmlType(TOPLEVEL::XML type)
    {
        switch (type)
        {
            case TOPLEVEL::XML::Instrument:
                return "Instrument";
            case TOPLEVEL::XML::Patch:
                return "Parameters";
            case TOPLEVEL::XML::Scale:
                return "Scales";
            case TOPLEVEL::XML::State:
                return "Session";
            case TOPLEVEL::XML::Vector:
                return "Vector Control";
            case TOPLEVEL::XML::MLearn:
                return "Midi Learn";
            case TOPLEVEL::XML::MasterConfig:
            case TOPLEVEL::XML::MasterUpdate:
                return "Config Base";
            case TOPLEVEL::XML::Config:
                return "Config Instance";
            case TOPLEVEL::XML::Presets:
                return "Presets";
            case TOPLEVEL::XML::Bank:
                return "Roots and Banks";
            case TOPLEVEL::XML::History:
                return "Recent Files";
            case TOPLEVEL::XML::PresetDirs:
                return "Preset Directories";
            default:
                return "Yoshimi Data";
        }
    }

    TOPLEVEL::XML parseXMLtype(string const& spec)
    {
        if (spec == "Instrument")      return TOPLEVEL::XML::Instrument;
        if (spec == "Parameters")      return TOPLEVEL::XML::Patch;
        if (spec == "Scales")          return TOPLEVEL::XML::Scale;
        if (spec == "Session")         return TOPLEVEL::XML::State;
        if (spec == "Vector Control")  return TOPLEVEL::XML::Vector;
        if (spec == "Midi Learn")      return TOPLEVEL::XML::MLearn;
        if (spec == "Config Base")     return TOPLEVEL::XML::MasterConfig;
        if (spec == "Config Instance") return TOPLEVEL::XML::Config;
        if (spec == "Presets")         return TOPLEVEL::XML::Presets;
        if (spec == "Roots and Banks") return TOPLEVEL::XML::Bank;
        if (spec == "Recent Files")    return TOPLEVEL::XML::History;
        if (spec == "Preset Directories") return TOPLEVEL::XML::PresetDirs;

        return TOPLEVEL::XML::Instrument;
    }
    
    /** @remark our XML files often start with leading whitespace
     *          which is not tolerated by the XML parser */
    const char* withoutLeadingWhitespace(string xml)
    {
        const char * text = xml.c_str();
        while (isspace(*text))
            ++text;
        return text;
    }


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

        OStr(const char* const literal)
            : OStr(string(literal? literal:""))
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

        static mxml_node_t* parse(const char* xml)
        {
            return mxmlLoadString(NULL, xml, MXML_OPAQUE_CALLBACK);  // treat all node content as »opaque« data, i.e. passed-through as-is
        }

        char* render()
        {
            mxmlSetWrapMargin(0);
            // disable automatic line wrapping and control whitespace per callback
            return mxmlSaveAllocString(mxmlElm(), XMLStore_whitespace_callback);
        }
    };
}//(End)internal details


/**
 * Abstracted access point to an MXML tree representation.
 * @remark the »downstream« code using the XML library stores Node*
 *         to represent an abstracted element backed by XML, and can
 *         access and manipulate its content through the Node interface.
 *         Yet the pointer stored there at runtime actually points at
 *         an internal struct defined by MXML. The translation between
 *         both worlds is handled in the Policy baseclass; this allows
 *         us to adapt to different MXML versions.
 */
struct XMLtree::Node
    : Policy
    {
        static Node* asNode(mxml_node_t* mxmlElm)
        {
            return reinterpret_cast<Node*>(mxmlElm);
        }

        static Node* newTree()
        {
            return asNode(mxmlNewElement(MXML_NO_PARENT, XML_HEADER));
        }

        static Node* parse(const char* xml)
        {
            assert (xml);
            return asNode(Policy::parse(xml));
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
            OStr optIDAttrib{id? "id":nullptr};
            return findChild(elmName, optIDAttrib, id);
        }

        Node* findChild(OStr elmName, OStr attribName, OStr attribVal)
        {
            return asNode(searchChild(elmName, attribName, attribVal));
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

    static_assert(1 == sizeof(XMLtree::Node)
                 ,"Node acts as placeholder for a MXML datatype "
                  "and must not define any data fields on its own."
                 );
}

XMLtree::XMLtree(XMLtree&& ref)
    : node{nullptr}
{
    std::swap(node, ref.node);
}


/** Factory: create from XML buffer.
 * @remark buffer is owned by caller and will only be read
 * @return new XMLtree handle, which can be empty in case of parsing failure.
 */
XMLtree XMLtree::parse(const char* xml)
{
    return xml? Node::parse(xml) : nullptr;
}

/** render XMLtree into new malloc() buffer
 * @note caller must deallocate returned buffer with `free()`
 */
char* XMLtree::render()
{
    return node? node->render() : nullptr;
}


XMLtree XMLtree::addElm(string name)
{
    if (!node)
        node = Node::newTree();
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

string XMLtree::getAttrib(string name)
{
    const char* valStr = node? node->getAttrib(name) : nullptr;
    return valStr? string{valStr} : string{};
}

uint XMLtree::getAttrib_uint(string name)
{
    const char* valStr = node? node->getAttrib(name) : nullptr;
    return valStr? string2uint(valStr) : 0;
}

XMLtree& XMLtree::addAttrib(string name, string val)
{
    assert(node);
    node->setAttrib(name,val);
    return *this;
}

/** add simple parameter element: with attribute name, value */
void XMLtree::addPar_int(string const& name, int val)
{
    assert(node);
    node->addChild("par")
            ->setAttrib("name",name)
             .setAttrib("value",val);
}

void XMLtree::addPar_uint(string const& name, uint val)
{
    assert(node);
    node->addChild("parU")
            ->setAttrib("name",name)
             .setAttrib("value",val);
}

/** add value both as integral number and as float persisted as exact bitstring */
void XMLtree::addPar_frac(string const& name, float val)
{
    assert(node);
    node->addChild("par")
            ->setAttrib("name",  name)
             .setAttrib("value", lrint(val)) // NOTE: rounded to integer
             .setAttrib("exact_value", func::asExactBitstring(val));
}

/** add floating-point both textually in decimal-format and as exact bitstring */
void XMLtree::addPar_real(string const& name, float val)
{
    assert(node);
    node->addChild("par_real")
            ->setAttrib("name",  name)
             .setAttrib("value", asLongString(val)) // decimal floating-point form
             .setAttrib("exact_value", func::asExactBitstring(val));
}

void XMLtree::addPar_bool(string const& name, bool yes)
{
    assert(node);
    node->addChild("par_bool")
            ->setAttrib("name",name)
             .setAttrib("value",yes? "yes":"no");
}

/** add string parameter: the name as attribute and the text as content */
void XMLtree::addPar_str(string const& name, string const& text)
{
    assert(node);
    node->addChild("string")
            ->setAttrib("name",name)
             .setText(text);
}


/**
 * Retrieve numeric value from a nested parameter element.
 * - if present, the stored representation will be converted to an int ∈[min ... max]
 * - otherwise, defaultVal is returned
 */
int XMLtree::getPar_int(string const& name, int defaultVal, int min, int max)
{
    if (node)
    {
        Node* paramElm = node->findChild("par","name",name);
        if (paramElm)
        {
            const char* valStr = paramElm->getAttrib("value");
            if (valStr)
                return std::clamp(string2int(valStr), min, max);
        }
    }
    // parameter entry not retrieved
    return defaultVal;
}

uint XMLtree::getPar_uint(string const& name, uint defaultVal, uint min, uint max)
{
    if (node)
    {
        Node* paramElm = node->findChild("parU","name",name);
        if (paramElm)                 //  ^^^^
        {
            const char* valStr = paramElm->getAttrib("value");
            if (valStr)
                return std::clamp(string2uint(valStr), min, max);
        }                             // ^^^^
    }
    // parameter entry not retrieved
    return defaultVal;
}

/** @internal attempt to retrieve a float value,
 *            preferably using the exact IEEE 754 bitstring stored in an attribute "exact_value";
 *            For legacy format, fall back to the "value" attribute, which can either be a decimal
 *            floating-point (for `<par_real...`) or even just an integer (for the 0...127 char params)
 */
optional<float> XMLtree::readParCombi(string const& elmID, string const& name)
{
    if (node)
    {
        Node* paramElm = node->findChild(elmID,"name",name);
        if (paramElm)
        {
            const char* valStr = paramElm->getAttrib("exact_value");
            if (valStr)
                return func::bitstring2float(valStr);

            // fall-back to legacy format
            valStr = paramElm->getAttrib("value");
            if (valStr)
                return string2float(valStr);
        }
    }
    // parameter entry not retrieved
    return std::nullopt;
}

float XMLtree::getPar_frac(string const& name, float defaultVal, float min, float max)
{
    auto val = readParCombi("par",name);
    return std::clamp(val? *val:defaultVal, min, max);
}

float XMLtree::getPar_real(string const& name, float defaultVal)
{
    auto val = readParCombi("par_real",name);
    return val? *val:defaultVal;
}

float XMLtree::getPar_real(string const& name, float defaultVal, float min, float max)
{
    return std::clamp(getPar_real(name,defaultVal), min, max);
}

/** value limited to [0 ... 127] */
int XMLtree::getPar_127(string const& name, int defaultVal)
{
    return getPar_int(name, defaultVal, 0, 127);
}

/** value limited to [0 ... 255] */
int XMLtree::getPar_255(string const& name, int defaultVal)
{
    return getPar_int(name, defaultVal, 0, 255);
}

bool XMLtree::getPar_bool(string const& name, bool defaultVal)
{
    if (node)
    {
        Node* paramElm = node->findChild("par_bool","name",name);
        if (paramElm)
        {
            const char* valStr = paramElm->getAttrib("value");
            if (valStr)
                return func::string2bool(valStr);
        }
    }
    // parameter entry not retrieved
    return defaultVal;
}

string XMLtree::getPar_str(string const& name)
{
    if (node)
    {
        Node* paramElm = node->findChild("string","name",name);
        if (paramElm)
        {
            const char* text = paramElm->getText();
            if (text)
                return string{text};
        }
    }
    // parameter entry not retrieved
    return string{};
}




/* ===== XMLStore implementation ===== */



XMLStore::XMLStore(TOPLEVEL::XML type, SynthEngine& OBSOLETE, bool zynCompat)
    : meta{type
          ,Config::VER_YOSHI_CURR
          ,zynCompat? Config::VER_ZYN_COMPAT : VerInfo()
          }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////TODO 5/25 : stateful tree position logic broken. MUST GET RID of this "current position"
    , minimal(not synth.getRuntime().xmlmax)   ///////////////////////////////////OOO does this ever represent some state independent from the global config???
    , treeX{nullptr}
    , rootX{nullptr}
    , nodeX{nullptr}
    , infoX{nullptr}
    , stackpos(0)
    , xml_k(0)
    , synth(OBSOLETE)
{
    information.PADsynth_used = 0;
    information.ADDsynth_used = 0;
    information.SUBsynth_used = 0;
    information.yoshiType = true;
    information.type = type;
    memset(&parentstack, 0, sizeof(parentstack));

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////TODO 5/25 : stateful tree position logic broken. MUST GET RID of this "current position"
    treeX = nullptr;  // boom
    rootX = nullptr;  // BOOOOM
    nodeX = nullptr;  // BOOOOOOM
    infoX = nullptr;  // BOOOOOOOOOM
}

XMLStore::XMLStore(string filename, Logger const& log, SynthEngine& OBSOLETE)
    : root{loadFile(filename,log)}
    , meta{extractMetadata()}
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////TODO 5/25 : stateful tree position logic broken. MUST GET RID of this "current position"
    , minimal(not synth.getRuntime().xmlmax)
    , treeX{nullptr}
    , rootX{nullptr}
    , nodeX{nullptr}
    , infoX{nullptr}
    , stackpos(0)
    , xml_k(0)
    , synth(OBSOLETE)
    { }

XMLStore::XMLStore(const char* xml, SynthEngine& OBSOLETE)
    : root{XMLtree::parse(xml)}
    , meta{extractMetadata()}
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////TODO 5/25 : stateful tree position logic broken. MUST GET RID of this "current position"
    , minimal(not synth.getRuntime().xmlmax)
    , treeX{nullptr}
    , rootX{nullptr}
    , nodeX{nullptr}
    , infoX{nullptr}
    , stackpos(0)
    , xml_k(0)
    , synth(OBSOLETE)
    { }


void XMLStore::buildXMLRoot()
{
    if (root)
        return;

    assert (meta.isValid());

    if (meta.isZynCompat())
    {
        root.addElm("!DOCTYPE").addAttrib(topElmName(meta));
        root.addElm(topElmName(meta))
            .addAttrib("version-major",     asString(meta.zynVer.maj))
            .addAttrib("version-minor",     asString(meta.zynVer.min))
            .addAttrib("version-revision",  asString(meta.zynVer.rev))
            .addAttrib("Yoshimi-major",     asString(meta.yoshimiVer.maj))
            .addAttrib("Yoshimi-minor",     asString(meta.yoshimiVer.min))
            .addAttrib("Yoshimi-revision",  asString(meta.yoshimiVer.rev))
            .addAttrib("ZynAddSubFX-author","Nasca Octavian Paul")
            .addAttrib("Yoshimi-author",    "Alan Ernest Calvert")
            ;
    }
    else
    {// Yoshimi native format
        root.addElm("!DOCTYPE").addAttrib(topElmName(meta));
        root.addElm(topElmName(meta))
            .addAttrib("Yoshimi-major",   asString(meta.yoshimiVer.maj))
            .addAttrib("Yoshimi-minor",   asString(meta.yoshimiVer.min))
            .addAttrib("Yoshimi-revision",asString(meta.yoshimiVer.rev))
            .addAttrib("Yoshimi-author",  "Alan Ernest Calvert")
            ;
    }
    assert(root);
    root.addElm("INFORMATION")
        .addAttrib("XMLtype", renderXmlType(meta.type));
}


XMLStore::Metadata XMLStore::extractMetadata()
{
    if (XMLtree top = root.getElm(ROOT_YOSHI))
        return Metadata{parseXMLtype(top.getElm("INFORMATION").getPar_str("XMLtype"))
                       ,VerInfo{top.getAttrib_uint("Yoshimi-major")
                               ,top.getAttrib_uint("Yoshimi-minor")
                               ,top.getAttrib_uint("Yoshimi-revision")
                               }
                       ,Config::ZYNADDSUBFX_VER
                       };
    else
    if (XMLtree top = root.getElm(ROOT_ZYN))
        return Metadata{parseXMLtype(top.getElm("INFORMATION").getPar_str("XMLtype"))
                       ,VerInfo{top.getAttrib_uint("Yoshimi-major")
                               ,top.getAttrib_uint("Yoshimi-minor")
                               ,top.getAttrib_uint("Yoshimi-revision")
                               }
                       ,VerInfo{top.getAttrib_uint("version-major")
                               ,top.getAttrib_uint("version-minor")
                               ,top.getAttrib_uint("version-revision")
                               }
                       };
    else
        return Metadata{}; // marked as invalid
}
    ///////////////////////////////////////////////////////////////////////TODO 4/2025 : old code initialised base parameters in ctor -- must do that explicitly now --> Config::initData(xml)

XMLtree XMLStore::accessTop()
{
    buildXMLRoot();
    assert (root);
    return root.getElm(topElmName(meta));
}



XMLStore::~XMLStore()  ////////////////////////////////////////////////////TODO 4/2025 : must go away in the end, since all memory management becomes automatic!!
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
    string xml = loadGzipped(filename, report);
    char* xmldata = & xml[0];
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

/**
 * Render tree contents into XML format.
 * @return NULL if empty, otherwise pointer to a char buffer, allocated with malloc()
 * @warning user must free the returned buffer
 */
char* XMLStore::render()
{
    return root.render();
}


/**
 * Render tree contents into XML format and write it into a file,
 * possibly gzip compressed (0 means no compression)
 * @return true on success
 */
bool XMLStore::saveXMLfile(string filename, Logger const& log, uint gzipCompressionLevel)
{
    bool success{false};
    if (not root)
        log("XML: empty tree -- nothing to save", _SYS_::LogNotSerious);
    else
    {
        char* xmldata = render();
        if (not xmldata)
            log("XML: Failed to allocate storage for rendered XML");
        else
        {
            gzipCompressionLevel = std::clamp(gzipCompressionLevel, 0u, 9u);
            if (gzipCompressionLevel == 0)
            {
                if (not saveText(xmldata, filename))
                    log("XML: Failed to save xml file " + filename + "(uncompressed)", _SYS_::LogNotSerious);
                else
                    success = true;
            }
            else
            {
                string result = saveGzipped(xmldata, filename, gzipCompressionLevel);
                if (not result.empty())
                    log(result, _SYS_::LogNotSerious);
                else
                    success = true;
            }
            free(xmldata);
        }
    }
    return success;
}


XMLtree XMLStore::loadFile(string filename, Logger const& log)
{
    string report{};
    string xmldata = loadGzipped(filename, report);
    if (not report.empty())
        log(report, _SYS_::LogNotSerious);

    XMLtree content{XMLtree::parse(withoutLeadingWhitespace(xmldata))};
    if (not content)
        log("XML: File \""+filename+"\" can not be parsed as XML", _SYS_::LogNotSerious);
    if (xmldata.empty())
        log("XML: Could not load xml file: " + filename, _SYS_::LogNotSerious);

    return content;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////TODO 4/25 : all old code below is obsolete !!!!

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
