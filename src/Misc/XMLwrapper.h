/*
    XML.h - XML wrapper

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2024, Will Godfrey

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

#ifndef XML_WRAPPER_H
#define XML_WRAPPER_H

#include "globals.h"

#include <mxml.h>
#include <string>
#include <limits>

// max tree depth
#define STACKSIZE 128

class SynthEngine;

class XMLwrapper
{
    public:
       ~XMLwrapper();
        XMLwrapper(SynthEngine& _synth, bool _isYoshi = false, bool includeBase = true);
        // shall not be copied nor moved
        XMLwrapper(XMLwrapper&&)                 = delete;
        XMLwrapper(XMLwrapper const&)            = delete;
        XMLwrapper& operator=(XMLwrapper&&)      = delete;
        XMLwrapper& operator=(XMLwrapper const&) = delete;

        // SAVE to XML
        bool saveXMLfile(std::string _filename, bool useCompression = true); // return true if ok, false otherwise

        // returns the new allocated string that contains the XML data (used for clipboard)
        // the string is NULL terminated
        char* getXMLdata();


        void addparU(std::string const& name, uint val); // add unsigned uinteger parameter: name, value

        void addpar(std::string const& name, int val); // add simple parameter: name, value

        void addparcombi(std::string const& name, float val); // add hybrid float/int parameter: name, value

        void addparreal(std::string const& name, float val);

        void addpardouble(std::string const& name, double val);

        void addparbool(std::string const& name, int val); // 1 => "yes", else "no"

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
        mxml_node_t *tree;
        mxml_node_t *root;
        mxml_node_t *node;
        mxml_node_t *info;

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

#endif
