/*
    XML.h - XML wrapper

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2020, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code.

*/

#ifndef XML_WRAPPER_H
#define XML_WRAPPER_H

#include <mxml.h>
#include <string>

// max tree depth
#define STACKSIZE 128

class SynthEngine;

class XMLwrapper
{
    public:
        XMLwrapper(SynthEngine *_synth, bool _isYoshi = false, bool includeBase = true);
        ~XMLwrapper();

        // SAVE to XML
        bool saveXMLfile(std::string _filename, bool useCompression = true); // return true if ok, false otherwise

        // returns the new allocated string that contains the XML data (used for clipboard)
        // the string is NULL terminated
        char *getXMLdata(void);


        void addparU(const std::string& name, unsigned int val); // add unsigned uinteger parameter: name, value

        void addpar(const std::string& name, int val); // add simple parameter: name, value
        void addparreal(const std::string& name, float val);

        void addpardouble(const std::string& name, double val);

        void addparbool(const std::string& name, int val); // 1 => "yes", else "no"


        // add string parameter (name and string)
        void addparstr(const std::string& name, const std::string& val);

        // add a branch
        void beginbranch(const std::string& name);
        void beginbranch(const std::string& name, int id);

        // this must be called after each branch (nodes that contains child nodes)
        void endbranch(void);

        // we always save with a blank first line
        const char *removeBlanks(const char *c)
        {while (isspace(*c)) ++c; return c;}

        // LOAD from XML
        bool loadXMLfile(const std::string& filename); // true if loaded ok

        // used by the clipboard
        bool putXMLdata(const char *xmldata);

        // enter into the branch
        // returns 1 if is ok, or 0 otherwise
        bool enterbranch(const std::string& name);

        // enter into the branch with id
        // returns 1 if is ok, or 0 otherwise
        bool enterbranch(const std::string& name, int id);

        // exits from a branch
        void exitbranch(void) { pop(); }

        // get the the branch_id and limits it between the min and max
        // if min==max==0, it will not limit it
        // if there isn't any id, will return min
        // this must be called only immediately after enterbranch()
        int getbranchid(int min, int max);

        // it returns the parameter and limits it between min and max
        // if min==max==0, it will not limit it
        // if no parameter will be here, the defaultpar will be returned
        unsigned int getparU(const std::string& name, unsigned int defaultpar, unsigned int min = 0, unsigned int max = 0xffffffff);

        int getpar(const std::string& name, int defaultpar, int min, int max);

        // the same as getpar, but the limits are 0 and 127
        int getpar127(const std::string& name, int defaultpar);

         // the same as getpar, but the limits are 0 and 255
        int getpar255(const std::string& name, int defaultpar);

       int getparbool(const std::string& name, int defaultpar);

         std::string getparstr(const std::string& name);

        float getparreal(const std::string& name, float defaultpar);
        float getparreal(const std::string& name, float defaultpar,
                         float min, float max);

        bool minimal; // false if all parameters will be stored

        struct {
            int type;
            unsigned char ADDsynth_used;
            unsigned char SUBsynth_used;
            unsigned char PADsynth_used;
            bool yoshiType;
        } information;

        // opens a file and parse only the "information" data on it

        void checkfileinformation(const std::string& filename, unsigned int& names, int& type);
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
        mxml_node_t *addparams0(const std::string&  name);

        // adds params like this: <name par1="val1">, returns the node
        mxml_node_t *addparams1(const std::string& name, const std::string& par1, const std::string& val1);

        // adds params like this: <name par1="val1" par2="val2">, returns the node
        mxml_node_t *addparams2(const std::string& name, const std::string& par1, const std::string& val1,
                                const std::string& par2, const std::string& val2);

        mxml_node_t *addparams3(const std::string& name, const std::string& par1, const std::string& val1,
                                const std::string& par2, const std::string& val2,
                                const std::string& par3, const std::string& val3);

        // this is used to store the parents
        mxml_node_t *parentstack[STACKSIZE];
        int stackpos;
        int xml_k;
        char tabs[STACKSIZE + 2];

        void push(mxml_node_t *node);
        mxml_node_t *pop(void);
        mxml_node_t *peek(void);
        struct {
            int major; // settings format version
            int minor;
            int y_major;
            int y_minor;
        } xml_version;

        bool isYoshi;
        SynthEngine *synth;
};

#endif
