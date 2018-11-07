/*
    XMLwrapper.cpp - XML wrapper

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

#include <zlib.h>
#include <iostream>

using namespace std;

#include "Misc/XMLwrapper.h"
#include "Misc/Util.h"

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
    {
        /*	const char *tmp=node->value.element.name;
        	if (tmp!=NULL) {
        	    if ((strstr(tmp,"par")!=tmp)&&(strstr(tmp,"string")!=tmp)) {
        		printf("%s ",tmp);
        		if (where==MXML_WS_BEFORE_OPEN) xml_k++;
        		if (where==MXML_WS_BEFORE_CLOSE) xml_k--;
        		if (xml_k>=STACKSIZE) xml_k=STACKSIZE-1;
        		if (xml_k<0) xml_k=0;
        		printf("%d\n",xml_k);
        		printf("\n");
        	    }

        	}
        	int i=0;
        	for (i=1;i<xml_k;i++) tabs[i]='\t';
        	tabs[0]='\n';tabs[i+1]='\0';
        	if (where==MXML_WS_BEFORE_OPEN) return(tabs);
        	    else return("\n");
        */
        return "\n";
    }
    return NULL;
}


XMLwrapper::XMLwrapper()
{
    ZERO(&parentstack, (int)sizeof(parentstack));
    ZERO(&values, (int)sizeof(values));

    minimal = true;
    stackpos = 0;

    information.PADsynth_used = false;

    tree = mxmlNewElement(MXML_NO_PARENT, "?xml version=\"1.0\" encoding=\"UTF-8\"?");
    mxml_node_t *doctype = mxmlNewElement(tree, "!DOCTYPE");
    mxmlElementSetAttr(doctype, "ZynAddSubFX-data", NULL);

    node=root = mxmlNewElement(tree, "ZynAddSubFX-data");

    mxmlElementSetAttr(root, "version-major", "1");
    mxmlElementSetAttr(root, "version-minor", "1");
    mxmlElementSetAttr(root, "ZynAddSubFX-author", "Nasca Octavian Paul");

    // make the empty branch that will contain the information parameters
    info = addparams0("INFORMATION");

    // save zynaddsubfx specifications
    beginbranch("BASE_PARAMETERS");
    addpar("max_midi_parts", NUM_MIDI_PARTS);
    addpar("max_kit_items_per_instrument", NUM_KIT_ITEMS);

    addpar("max_system_effects", NUM_SYS_EFX);
    addpar("max_insertion_effects", NUM_INS_EFX);
    addpar("max_instrument_effects", NUM_PART_EFX);

    addpar("max_addsynth_voices", NUM_VOICES);
    endbranch();
}

XMLwrapper::~XMLwrapper()
{
    if (tree != NULL)
        mxmlDelete(tree);
}

bool XMLwrapper::checkfileinformation(string filename)
{
    stackpos = 0;
    ZERO(&parentstack, (int)sizeof(parentstack));
    information.PADsynth_used = false;
    if (tree != NULL)
        mxmlDelete(tree);
    tree = NULL;
    char *xmldata = doloadfile(filename);
    if (xmldata == NULL)
        return -1; // the file could not be loaded or uncompressed
    char *start = strstr(xmldata, "<INFORMATION>");
    char *end = strstr(xmldata, "</INFORMATION>");
    if (start == NULL || end == NULL || start > end)
    {
        delete [] xmldata;
        return false;
    }
    end += strlen("</INFORMATION>");
    end[0] = '\0';
    tree = mxmlNewElement(MXML_NO_PARENT, "?xml");
    node = root = mxmlLoadString(tree, xmldata, MXML_OPAQUE_CALLBACK);
    if (root == NULL)
    {
        delete [] xmldata;
        mxmlDelete(tree);
        node = root = tree = NULL;
        return false;
    }
    root = mxmlFindElement(tree, tree, "INFORMATION", NULL, NULL, MXML_DESCEND);
    push(root);
    if (root == NULL)
    {
        delete [] xmldata;
        mxmlDelete(tree);
        node = root = tree = NULL;
        return false;
    }
    information.PADsynth_used = getparbool("PADsynth_used", false);
    exitbranch();
    if (tree != NULL)
        mxmlDelete(tree);
    delete [] xmldata;
    node = root = tree = NULL;
    return true;
}


// SAVE XML members

bool XMLwrapper::saveXMLfile(string filename)
{
    char *xmldata = getXMLdata();
    if (xmldata == NULL)
    {
        cerr << "Error, getXMLdata() == NULL" << endl;
        return -2;
    }
    int compression = runtime.settings.GzipCompression;
    bool result = dosavefile(filename, compression, xmldata);
    free(xmldata);
    return result;
}


char *XMLwrapper::getXMLdata()
{
    xml_k = 0;
    ZERO(tabs, STACKSIZE + 2);
    mxml_node_t *oldnode=node;
    node = info;
    // Info storing
    addparbool("PADsynth_used",information.PADsynth_used);
    node = oldnode;
    char *xmldata = mxmlSaveAllocString(tree, XMLwrapper_whitespace_callback);
    return xmldata;
}


bool XMLwrapper::dosavefile(string filename, int compression, const char *xmldata)
{
    if (compression == 0)
    {
        FILE *file;
        file = fopen(filename.c_str(), "w");
        if (file == NULL)
        {
            cerr << "Error, failed to open xml file " << filename << endl;
            return false;
        }
        fputs(xmldata, file);
        fclose(file);
    }
    else
    {
        if (compression > 9)
            compression = 9;
        if (compression < 1)
            compression = 1;
        char options[10];
        snprintf(options, 10, "wb%d", compression);

        gzFile gzfile;
        gzfile = gzopen(filename.c_str(), options);
        if (gzfile == NULL)
        {
            cerr << "Error, gzopen() == NULL" << endl;
            return false;
        }
        gzputs(gzfile, xmldata);
        gzclose(gzfile);
    }
    return true;
}


void XMLwrapper::addpar(const string &name, int val)
{
    addparams2("par", "name", name.c_str(), "value", asString(val));
}


void XMLwrapper::addparreal(const string &name, float val)
{
    //addparams2("par_real","name",name.c_str(),"value", real2str(val));
    addparams2("par_real","name", name.c_str(), "value", asString(val));
}

void XMLwrapper::addparbool(const string &name, int val)
{
    if (val != 0)
        addparams2("par_bool", "name", name.c_str(), "value", "yes");
    else
        addparams2("par_bool", "name", name.c_str(), "value", "no");
}

void XMLwrapper::addparstr(const string &name, const string &val)
{
    mxml_node_t *element = mxmlNewElement(node, "string");
    mxmlElementSetAttr(element, "name", name.c_str());
    mxmlNewText(element, 0, val.c_str());
}


void XMLwrapper::beginbranch(const string &name)
{
    push(node);
    node = addparams0(name.c_str());
}

void XMLwrapper::beginbranch(const string &name,int id)
{
    push(node);
    //node = addparams1(name.c_str(), "id", int2str(id));
    node = addparams1(name.c_str(), "id", asString(id));
}

void XMLwrapper::endbranch()
{
    node = pop();
}


// LOAD XML members

bool XMLwrapper::loadXMLfile(string filename)
{
    if (tree != NULL)
        mxmlDelete(tree);
    tree = NULL;
    ZERO(&parentstack, (int)sizeof(parentstack));
    ZERO(&values, (int)sizeof(values));
    stackpos = 0;
    const char *xmldata = doloadfile(filename);
    if (xmldata == NULL)
    {
        runtime.settings.verbose && cerr << "Error, xml file " << filename << " could not be loaded or uncompressed" << endl;
         return false;
    }
    root = tree = mxmlLoadString(NULL, xmldata, MXML_OPAQUE_CALLBACK);
    delete [] xmldata;
    if (tree == NULL)
    {
        runtime.settings.verbose && cerr << "Error, xml file " << filename << " is not XML" << endl;
        return false;
    }
    node = root = mxmlFindElement(tree, tree, "ZynAddSubFX-data", NULL, NULL, MXML_DESCEND);
    if (root == NULL)
    {
        runtime.settings.verbose && cerr << "Error, xml file " << filename << " doesnt embbed zynaddsubfx data" << endl;
        return false;
    }
    push(root);
    values.xml_version.major = string2int(mxmlElementGetAttr(root, "version-major"));
    values.xml_version.minor = string2int(mxmlElementGetAttr(root, "version-minor"));
    return true;
}


char *XMLwrapper::doloadfile(string filename)
{
    char * xmldata = NULL;
    gzFile gzfile  = gzopen(filename.c_str(), "rb");
    if (gzfile != NULL)
    {
        const int bufSize = 512;
        char fetchBuf[bufSize + 2];
        int bytes_read;
        int read_size = 0;
        string strbuf = string();
        while(!gzeof(gzfile))
        {
            memset(fetchBuf, 0, sizeof(char) * (bufSize + 2));
            if ((bytes_read = gzread(gzfile, fetchBuf, bufSize)) > 0)
            {
                read_size += bytes_read;
                strbuf += string(fetchBuf);
            }
        }
        gzclose(gzfile);
        xmldata = new char[strbuf.size() + 1];
        strcpy(xmldata, strbuf.c_str());
    }
    else
        cerr << "gzopen failed, errno " << errno << ", " << strerror(errno) << endl;

    return xmldata;
}

bool XMLwrapper::putXMLdata(const char *xmldata)
{
    if (tree != NULL)
        mxmlDelete(tree);
    tree = NULL;
    ZERO(&parentstack, (int)sizeof(parentstack));
    ZERO(&values, (int)sizeof(values));
    stackpos = 0;
    if (xmldata == NULL)
        return false;
    root = tree = mxmlLoadString(NULL, xmldata, MXML_OPAQUE_CALLBACK);
    if (tree == NULL)
        return false;
    node = root = mxmlFindElement(tree, tree, "ZynAddSubFX-data", NULL, NULL, MXML_DESCEND);
    if (root == NULL)
        return false;
    push(root);
    return true;
}


bool XMLwrapper::enterbranch(string name)
{
    node = mxmlFindElement(peek(), peek(), name.c_str(), NULL, NULL,
                           MXML_DESCEND_FIRST);
    if (node == NULL)
        return false;
    push(node);
    return true;
}


bool XMLwrapper::enterbranch(string name, int id)
{
    node = mxmlFindElement(peek(), peek(), name.c_str(), "id",
                           asString(id).c_str(), MXML_DESCEND_FIRST);
    if (node == NULL)
        return false;
    push(node);
    return true;
}


void XMLwrapper::exitbranch(void) { pop(); }


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


int XMLwrapper::getpar(string name, int defaultpar, int min, int max)
{
    node = mxmlFindElement(peek(), peek(), "par", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (node == NULL)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(node, "value");
    if (strval == NULL)
        return defaultpar;
    int val = string2int(strval);
    if (val < min)
        val = min;
    else if (val > max)
        val = max;
    return val;
}


int XMLwrapper::getpar127(string name, int defaultpar)
{
    return(getpar(name, defaultpar, 0, 127));
}


int XMLwrapper::getparbool(string name, int defaultpar)
{
    node = mxmlFindElement(peek(), peek(), "par_bool", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (node == NULL)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(node, "value");
    if (strval == NULL)
        return defaultpar;
    return (strval[0] == 'Y' || strval[0] == 'y') ? 1 : 0;
}


string XMLwrapper::getparstr(string name)
{
    node = mxmlFindElement(peek(), peek(), "string", "name", name.c_str(), MXML_DESCEND_FIRST);
    if (node == NULL)
        return string();
    if (node->child == NULL)
        return string();
    if (node->child->type != MXML_OPAQUE)
        return string();
    return string(node->child->value.element.name);
}


float XMLwrapper::getparreal(string name, float defaultpar)
{
    node = mxmlFindElement(peek(), peek(), "par_real", "name", name.c_str(),
                           MXML_DESCEND_FIRST);
    if (node == NULL)
        return defaultpar;
    const char *strval = mxmlElementGetAttr(node, "value");
    if (strval == NULL)
        return defaultpar;
    return string2float(string(strval));
}


float XMLwrapper::getparreal(string name, float defaultpar, float min, float max)
{
    float result = getparreal(name, defaultpar);
    if (result < min)
        result = min;
    else if (result > max)
        result = max;
    return result;
}


// Private parts

mxml_node_t *XMLwrapper::addparams0(string name)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams1(string name, string par1, string val1)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    return element;
}


mxml_node_t *XMLwrapper::addparams2(string name, string par1, string val1,
                                    string par2, string val2)
{
    mxml_node_t *element = mxmlNewElement(node, name.c_str());
    mxmlElementSetAttr(element, par1.c_str(), val1.c_str());
    mxmlElementSetAttr(element, par2.c_str(), val2.c_str());
    return element;
}


void XMLwrapper::push(mxml_node_t *node)
{
    if (stackpos >= STACKSIZE - 1)
    {
        printf("BUG!: XMLwrapper::push() - full parentstack\n");
        return;
    }
    stackpos++;
    parentstack[stackpos] = node;
}


mxml_node_t *XMLwrapper::pop()
{
    if (stackpos <= 0)
    {
        printf("BUG!: XMLwrapper::pop() - empty parentstack\n");
        return root;
    }
    mxml_node_t *node = parentstack[stackpos];
    parentstack[stackpos] = NULL;
    stackpos--;
    return node;
}


mxml_node_t *XMLwrapper::peek()
{
    if (stackpos <= 0)
    {
        cerr << "BUG!: XMLwrapper::peek() - empty parentstack" << endl;
        return root;
    }
    return parentstack[stackpos];
}
