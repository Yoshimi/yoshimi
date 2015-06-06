/*
    XML.h - XML wrapper

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/

#ifndef XML_WRAPPER_H
#define XML_WRAPPER_H

#include <mxml.h>
#include <string>

using namespace std;

#include "globals.h"

// the maxim tree depth
#define STACKSIZE 100

class XMLwrapper
{
    public:
        XMLwrapper();
        ~XMLwrapper();

        /********************************/
        /*         SAVE to XML          */
        /********************************/

        // returns true if ok, false if the file cannot be saved
        bool saveXMLfile(string filename);

        // returns the new allocated string that contains the XML data (used for clipboard)
        // the string is NULL terminated
        char *getXMLdata();

        // add simple parameter (name and value)
        void addpar(const string &name, int val);
        void addparreal(const string &name, float val);

        // add boolean parameter (name and boolean value)
        // if the value is 0 => "yes", else "no"
        void addparbool(const string &name, int val);

        // add string parameter (name and string)
        void addparstr(const string &name, const string &val);

        // add a branch
        void beginbranch(const string &name);
        void beginbranch(const string &name, int id);

        // this must be called after each branch (nodes that contains child nodes)
        void endbranch();

        /********************************/
        /*        LOAD from XML         */
        /********************************/

        bool loadXMLfile(string filename); // true if loaded ok

        // used by the clipboard
        bool putXMLdata(const char *xmldata);

        // enter into the branch
        // returns 1 if is ok, or 0 otherwise
        bool enterbranch(string name);


        // enter into the branch with id
        // returns 1 if is ok, or 0 otherwise
        bool enterbranch(string name, int id);

        // exits from a branch
        void exitbranch(void);

        // get the the branch_id and limits it between the min and max
        // if min==max==0, it will not limit it
        // if there isn't any id, will return min
        // this must be called only imediately after enterbranch()
        int getbranchid(int min, int max);

        // it returns the parameter and limits it between min and max
        // if min==max==0, it will not limit it
        // if no parameter will be here, the defaultpar will be returned
        int getpar(string name, int defaultpar, int min, int max);

        // the same as getpar, but the limits are 0 and 127
        int getpar127(string name, int defaultpar);

        int getparbool(string name, int defaultpar);

         string getparstr(string name);

        float getparreal(string name, float defaultpar);
        float getparreal(string name, float defaultpar,
                         float min, float max);

        bool minimal; // false if all parameters will be stored (used only for clipboard)

        struct {
            bool PADsynth_used;
        } information;

        // opens a file and parse only the "information" data on it
        // returns "true" if all went ok or "false" on errors
        bool checkfileinformation(string filename);

    private:
        bool dosavefile(string filename, int compression, const char *xmldata);
        char *doloadfile(string filename);

        mxml_node_t *tree; // all xml data
        mxml_node_t *root; // xml data used by zynaddsubfx
        mxml_node_t *node; // current node
        mxml_node_t *info; // this node is used to store the information about the data

        // adds params like this:
        // <name>
        // returns the node
        //mxml_node_t *addparams0(const char *name);
        mxml_node_t *addparams0(string  name);

        // adds params like this:
        // <name par1="val1">
        // returns the node
        //mxml_node_t *addparams1(const char *name, const char *par1, const char *val1);
        mxml_node_t *addparams1(string name, string par1, string val1);

        // adds params like this:
        // <name par1="val1" par2="val2">
        // returns the node
        //mxml_node_t *addparams2(const char *name, const char *par1,
        //                        const char *val1, const char *par2,
        //                        const char *val2);
        mxml_node_t *addparams2(string name, string par1, string val1,
                                string par2, string val2);

        // this is used to store the parents
        mxml_node_t *parentstack[STACKSIZE];
        int stackpos;

        void push(mxml_node_t *node);
        mxml_node_t *pop();
        mxml_node_t *peek();

        // these are used to store the values
        struct {
            struct {
                int major, minor;
            } xml_version;
        } values;
};

#endif
