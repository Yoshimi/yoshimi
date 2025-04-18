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

#include <mxml.h>                      ////////////////////////////////////////TODO 4/25 : remove from front-end
#include <string>
#include <limits>

// max tree depth
#define STACKSIZE 128                  ////////////////////////////////////////TODO 4/25 : becomes obsolete

using std::string;

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

        void addPar_int(string const& name, int val);     // add simple parameter: name, value
        void addPar_uint(string const& name, uint val);   // add unsigned integer parameter: name, value
        void addPar_float(string const& name, float val); // add float parameter persisted as fixed bitstring: name, value
        void addPar_real(string const& name, float val);
        void addPar_bool(string const& name, bool val);
        void addPar_str(string const& name, string const& val);  // add string parameter (name and string)

        int  getPar_int(string const& name, int defaultVal, int min, int max);
        int  getPar_127(string const& name, int defaultVal);     // value limited to [0 ... 127]
        int  getPar_255(string const& name, int defaultVal);     // value limited to [0 ... 255]
        uint getPar_uint(string const& name, uint defaultVal, uint min = 0, uint max = std::numeric_limits<uint>::max());

        float getPar_float(string const& name, float defaultVal, float min, float max);
        float getPar_real(string const& name, float defaultVal, float min, float max);
        float getPar_real(string const& name, float defaultVal);
        bool  getPar_bool(string const& name, bool defaultVal);
        string getPar_str(string const& name);
};



/** Maintain tree structured data, which can be stored and retrieved from XML */
class XMLStore
{
    XMLtree root;

    public:
       ~XMLStore();
        XMLStore(TOPLEVEL::XML type, SynthEngine& _synth, bool yoshiFormat = true);
        // shall not be copied nor moved
        XMLStore(XMLStore&&)                 = delete;
        XMLStore(XMLStore const&)            = delete;
        XMLStore& operator=(XMLStore&&)      = delete;
        XMLStore& operator=(XMLStore const&) = delete;

        void buildXMLroot();

        // SAVE to XML
        bool saveXMLfile(std::string _filename, bool useCompression = true); // return true if ok, false otherwise

        // returns the new allocated string that contains the XML data (used for clipboard)
        // the string is NULL terminated
        char* getXMLdata();

        XMLtree addElm(string name);
        XMLtree getElm(string name);

        void addparU(std::string const& name, uint val); // add unsigned integer parameter: name, value

        void addpar(std::string const& name, int val); // add simple parameter: name, value

        void addparcombi(std::string const& name, float val); // add float parameter persisted as fixed bitstring: name, value

        void addparreal(std::string const& name, float val);

        void addpardouble(std::string const& name, double val);

        void addparbool(std::string const& name, int val); // 1 => "yes", else "no"         /////////////////TODO 4/25 : change to bool

        // add string parameter (name and string)
        void addparstr(std::string const& name, std::string const& val);

        // add a branch
        void beginbranch(std::string const& name);
        void beginbranch(std::string const& name, int id);

        // this must be called after each branch (nodes that contains child nodes)
        void endbranch();


        // we always save with a blank first line
        const char *removeBlanks(const char *c)
        {while (isspace(*c)) ++c; return c;}

        // LOAD from XML
        bool loadXMLfile(std::string const& filename); // true if loaded ok

        // used by the clipboard
        bool putXMLdata(const char *xmldata);

        // enter into the branch
        // returns 1 if is ok, or 0 otherwise
        bool enterbranch(std::string const& name);

        // enter into the branch with id
        // returns 1 if is ok, or 0 otherwise
        bool enterbranch(std::string const& name, int id);

        // exits from a branch
        void exitbranch() { pop(); }

        // get the the branch_id and limits it between the min and max
        // if min==max==0, it will not limit it
        // if there isn't any id, will return min
        // this must be called only immediately after enterbranch()
        int getbranchid(int min, int max);

        // it returns the parameter and limits it between min and max
        // if min==max==0, it will not limit it
        // if no parameter will be here, the defaultpar will be returned
        uint getparU(std::string const& name, uint defaultpar, uint min = 0, uint max = std::numeric_limits<uint>::max());

        int getpar(std::string const& name, int defaultpar, int min, int max);

        float getparcombi(std::string const& name, float defaultpar, float min, float max);

        // the same as getpar, but the limits are 0 and 127
        int getpar127(std::string const& name, int defaultpar);

         // the same as getpar, but the limits are 0 and 255
        int getpar255(std::string const& name, int defaultpar);

       int getparbool(std::string const& name, int defaultpar);

         std::string getparstr(std::string const& name);

        float getparreal(std::string const& name, float defaultpar);
        float getparreal(std::string const& name, float defaultpar,
                         float min, float max);

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
