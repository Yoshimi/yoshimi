/*
    XMLStore.h - Store structured data in XML

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

    This file is derivative of ZynAddSubFX original code.

*/


#ifndef XML_STORE_H
#define XML_STORE_H

#include "globals.h"
#include "VerInfo.h"

#include <mxml.h>                      ////////////////////////////////////////TODO 4/25 : remove from front-end
#include <string>
#include <limits>
#include <optional>

// max tree depth
#define STACKSIZE 128                  ////////////////////////////////////////TODO 4/25 : becomes obsolete

using std::string;
using std::optional;

class SynthEngine;

/** Structured data subtree, which can be loaded and stored as XML */
class XMLtree
{
    struct Node;
    Node* node{nullptr};

        XMLtree(Node*);     ////////////////////OOO unclear if we need this private constructor...

    public:
       ~XMLtree();
        XMLtree()                          = default;
        // shall only be moved
        XMLtree(XMLtree&&);
        XMLtree(XMLtree const&)            = delete;
        XMLtree& operator=(XMLtree&&)      = delete;
        XMLtree& operator=(XMLtree const&) = delete;

        explicit operator bool()  const
        {
            return bool(node);
        }

        XMLtree addElm(string name);
        XMLtree getElm(string name);
        XMLtree getElm(string name, int id);

        XMLtree& addAttrib(string name, string val ="");

        void addPar_int(string const& name, int val);            // add simple parameter element: with attribute name, value
        void addPar_uint(string const& name, uint val);          // add unsigned integer parameter: name, value
        void addPar_frac(string const& name, float val);         // add value both as integral and as float persisted as exact bitstring
        void addPar_real(string const& name, float val);         // add floating-point both textually and as exact bitstring
        void addPar_bool(string const& name, bool val);
        void addPar_str(string const& name, string const&);      // add string parameter (name and string)

        int  getPar_int(string const& name, int defaultVal, int min, int max);
        int  getPar_127(string const& name, int defaultVal);     // value limited to [0 ... 127]
        int  getPar_255(string const& name, int defaultVal);     // value limited to [0 ... 255]
        uint getPar_uint(string const& name, uint defaultVal, uint min = 0, uint max = std::numeric_limits<uint>::max());

        float getPar_frac(string const& name, float defaultVal, float min, float max);  // restore exact fractional value, fall-back to integral(legacy)
        float getPar_real(string const& name, float defaultVal, float min, float max);
        float getPar_real(string const& name, float defaultVal);
        bool  getPar_bool(string const& name, bool defaultVal);
        string getPar_str(string const& name);

    private:
        optional<float> readParCombi(string const&, string const& name);
};



/** Maintain tree structured data, which can be stored and retrieved from XML */
class XMLStore
{
    XMLtree root;

    public:
       ~XMLStore(); /////////////////////////////////////////////////////////////////////////////////////////TODO 4/25 obsolete -- automatic memory management!

        XMLStore(TOPLEVEL::XML type, SynthEngine& OBSOLETE, bool yoshiFormat = true);

        XMLStore(string filename, uint gzipCompressionLevel, SynthEngine& OBSOLETE);

        XMLStore(string xml, SynthEngine& OBSOLETE);

        // can be moved
        XMLStore(XMLStore&&)                 = default;
        // shall not be copied or assigned
        XMLStore(XMLStore const&)            = delete;
        XMLStore& operator=(XMLStore&&)      = delete;
        XMLStore& operator=(XMLStore const&) = delete;

        void normaliseRoot();

        // SAVE to XML
        bool saveXMLfile(std::string _filename, bool useCompression = true); // return true if ok, false otherwise

        // returns the new allocated string that contains the XML data (used for clipboard)
        // the string is NULL terminated
        char* getXMLdata();

        XMLtree accessTop();

        XMLtree addElm(string name){ return accessTop().addElm(name);  }
        XMLtree getElm(string name){ return root? accessTop().getElm(name) : XMLtree{}; }


        // we always save with a blank first line
        const char *removeBlanks(const char *c)
        {while (isspace(*c)) ++c; return c;}

        // LOAD from XML
        bool loadXMLfile(std::string const& filename); // true if loaded ok

        // used by the clipboard
        bool putXMLdata(const char *xmldata);

        // get the the branch_id and limits it between the min and max
        // if min==max==0, it will not limit it
        // if there isn't any id, will return min
        // this must be called only immediately after enterbranch()
        int getbranchid(int min, int max);         ////////////////////////////////////////////OOO what is this for?


        struct Metadata
        {
            TOPLEVEL::XML type{TOPLEVEL::XML::Instrument};
            VerInfo yoshimiVer{};
            VerInfo zynVer{};

            bool isYoshiFormat() const { return bool(yoshimiVer); }
        };
        Metadata meta;

        bool minimal; // false if all parameters will be stored

        struct {
            int type;
            uchar ADDsynth_used;
            uchar SUBsynth_used;
            uchar PADsynth_used;
            bool yoshiType;
        } information;

        // opens a file and parse only the "information" data on it

        void checkfileinformation(std::string const& filename, uint& names, int& type);
        void slowinfosearch(char *idx);

    private:
        mxml_node_t *treeX;
        mxml_node_t *rootX;
        mxml_node_t *nodeX;
        mxml_node_t *infoX;

        // adds params like this:
        // <name>
        // returns the node
        //mxml_node_t *addparams0(const char *name);
        mxml_node_t *addparams0(std::string const&  name);

        // adds params like this: <name par1="val1">, returns the node
        mxml_node_t *addparams1(std::string const& name, std::string const& par1, std::string const& val1);

        // adds params like this: <name par1="val1" par2="val2">, returns the node
        mxml_node_t *addparams2(std::string const& name, std::string const& par1, std::string const& val1,
                                std::string const& par2, std::string const& val2);

        mxml_node_t *addparams3(std::string const& name, std::string const& par1, std::string const& val1,
                                std::string const& par2, std::string const& val2,
                                std::string const& par3, std::string const& val3);

        // this is used to store the parents
        mxml_node_t *parentstack[STACKSIZE];
        int stackpos;
        int xml_k;
        char tabs[STACKSIZE + 2];

        void push(mxml_node_t *node);
        mxml_node_t* pop();
        mxml_node_t* peek();
        struct {
            int major; // settings format version
            int minor;
            int revision;
            int y_major;
            int y_minor;
            int y_revision;
        } xml_version;

        bool isYoshi;
        SynthEngine& synth;
};

/////////////////////////////////////////////////////////////////////////////////////////////////WIP Prototype 4/25 - throw away when done!!!!!
void run_XMLStoreTest(SynthEngine&);
/////////////////////////////////////////////////////////////////////////////////////////////////WIP Prototype 4/25 - throw away when done!!!!!
#endif /*XML_STORE_H*/
