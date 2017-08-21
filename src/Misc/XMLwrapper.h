/*
    XML.h - XML wrapper

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2017, Will Godfrey

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

    Modified June 2017
*/

#ifndef XML_WRAPPER_H
#define XML_WRAPPER_H

#include <mxml.h>
#include <string>

using namespace std;

#include "Misc/MiscFuncs.h"

// max tree depth
#define STACKSIZE 128

class SynthEngine;

class XMLwrapper : private MiscFuncs
{
    public:
        XMLwrapper(SynthEngine *_synth);
        ~XMLwrapper();

        // SAVE to XML
        bool saveXMLfile(const string& filename); // return true if ok, false otherwise

        // returns the new allocated string that contains the XML data (used for clipboard)
        // the string is NULL terminated
        char *getXMLdata(void);


        void addpar(const string& name, int val); // add simple parameter: name, value
        void addparreal(const string& name, float val);

        void addpardouble(const string& name, double val);

        void addparbool(const string& name, int val); // 1 => "yes", else "no"


        // add string parameter (name and string)
        void addparstr(const string& name, const string& val);

        // add a branch
        void beginbranch(const string& name);
        void beginbranch(const string& name, int id);

        // this must be called after each branch (nodes that contains child nodes)
        void endbranch(void);

        // LOAD from XML
        bool loadXMLfile(const string& filename); // true if loaded ok

        // used by the clipboard
        bool putXMLdata(const char *xmldata);

        // enter into the branch
        // returns 1 if is ok, or 0 otherwise
        bool enterbranch(const string& name);

        // enter into the branch with id
        // returns 1 if is ok, or 0 otherwise
        bool enterbranch(const string& name, int id);

        // exits from a branch
        void exitbranch(void) { pop(); }

        // get the the branch_id and limits it between the min and max
        // if min==max==0, it will not limit it
        // if there isn't any id, will return min
        // this must be called only imediately after enterbranch()
        int getbranchid(int min, int max);

        // it returns the parameter and limits it between min and max
        // if min==max==0, it will not limit it
        // if no parameter will be here, the defaultpar will be returned
        int getpar(const string& name, int defaultpar, int min, int max);

        // the same as getpar, but the limits are 0 and 127
        int getpar127(const string& name, int defaultpar);

         // the same as getpar, but the limits are 0 and 255
        int getpar255(const string& name, int defaultpar);

       int getparbool(const string& name, int defaultpar);

         string getparstr(const string& name);

        float getparreal(const string& name, float defaultpar);
        float getparreal(const string& name, float defaultpar,
                         float min, float max);

        bool minimal; // false if all parameters will be stored

        struct {
            unsigned char ADDsynth_used;
            unsigned char SUBsynth_used;
            unsigned char PADsynth_used;
        } information;

        // opens a file and parse only the "information" data on it
        // returns "true" if all went ok or "false" on errors
        bool checkfileinformation(const string& filename);
        bool slowinfosearch(char *xmldata);

    private:
        char *doloadfile(const string& filename);

        mxml_node_t *tree;
        mxml_node_t *root;
        mxml_node_t *node;
        mxml_node_t *info;

        // adds params like this:
        // <name>
        // returns the node
        //mxml_node_t *addparams0(const char *name);
        mxml_node_t *addparams0(const string&  name);

        // adds params like this: <name par1="val1">, returns the node
        mxml_node_t *addparams1(const string& name, const string& par1, const string& val1);

        // adds params like this: <name par1="val1" par2="val2">, returns the node
        mxml_node_t *addparams2(const string& name, const string& par1, const string& val1,
                                const string& par2, const string& val2);

        mxml_node_t *addparams3(const string& name, const string& par1, const string& val1,
                                const string& par2, const string& val2,
                                const string& par3, const string& val3);

        // this is used to store the parents
        mxml_node_t *parentstack[STACKSIZE];
        int stackpos;

        void push(mxml_node_t *node);
        mxml_node_t *pop(void);
        mxml_node_t *peek(void);
        struct {
            int major; // settings format version
            int minor;
            int y_major;
            int y_minor;
        } xml_version;

        SynthEngine *synth;
};

#endif
