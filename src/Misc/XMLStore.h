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
#include "Log.h"

#include <string>
#include <limits>
#include <optional>

using std::string;
using std::optional;

class SynthEngine;


/**
 * Structured data subtree,
 * which can be loaded and stored as XML.
 * @note this is a smart-pointer, delegating to
 *       MXML ref-count based memory management.
 */
class XMLtree
{
    struct Node;
    Node* node{nullptr};

        XMLtree(Node*);

    public:
       ~XMLtree();
        XMLtree()                          = default;
        // shall only be moved
        XMLtree(XMLtree&&);
        XMLtree(XMLtree const&)            = delete;
        XMLtree& operator=(XMLtree&&)      = delete;
        XMLtree& operator=(XMLtree const&) = delete;

        explicit operator bool()  const { return     bool(node); }
        bool empty()              const { return not bool(node); }

        static XMLtree parse(const char*);                       // Factory: create from XML buffer
        char* render();                                          // render XMLtree into new malloc() buffer

        XMLtree addElm(string name);
        XMLtree addElm(string name, uint id);
        XMLtree getElm(string name);
        XMLtree getElm(string name, uint id);

        XMLtree& addAttrib(string name, string val ="");
        string getAttrib(string name);
        uint   getAttrib_uint(string name);

        void addPar_int (string const& name, int val);           // add simple parameter element: with attribute name, value
        void addPar_uint(string const& name, uint val);          // add unsigned integer parameter: name, value
        void addPar_frac(string const& name, float val);         // add value both as integral and as float persisted as exact bitstring
        void addPar_real(string const& name, float val);         // add floating-point both textually and as exact bitstring
        void addPar_bool(string const& name, bool val);
        void addPar_str (string const& name, string const&);     // add string parameter (name and string)

        int  getPar_int (string const& name, int defaultVal, int min, int max);
        int  getPar_127 (string const& name, int defaultVal);    // value limited to [0 ... 127]
        int  getPar_255 (string const& name, int defaultVal);    // value limited to [0 ... 255]
        uint getPar_uint(string const& name, uint defaultVal, uint min = 0, uint max = std::numeric_limits<uint>::max());

        float getPar_frac(string const& name, float defaultVal, float min, float max);  // restore exact fractional value, fall-back to integral(legacy)
        float getPar_real(string const& name, float defaultVal, float min, float max);
        float getPar_real(string const& name, float defaultVal);
        bool  getPar_bool(string const& name, bool defaultVal);
        string getPar_str(string const& name);

    private:
        optional<float> readParCombi(string const&, string const& name);
};



/**
 * Maintain tree-structured data,
 * which can be stored and retrieved from XML
 * @remark this is a lightweight value object,
 *         can be moved and stored in local vars.
 */
class XMLStore
{
    XMLtree root;

    public:
        XMLStore(TOPLEVEL::XML type, bool zynCompat =false);       // can be created empty
        XMLStore(string filename, Logger const& log);              // can be created by loading XML
        XMLStore(const char* xml);                                 // can be created from buffer with XML data

        // can be moved
        XMLStore(XMLStore&&)                 = default;
        // shall not be copied or assigned
        XMLStore(XMLStore const&)            = delete;
        XMLStore& operator=(XMLStore&&)      = delete;
        XMLStore& operator=(XMLStore const&) = delete;

        explicit operator bool()  const { return     bool(root); }
        bool empty()              const { return not bool(root); }


        char* render();                                            // rendered XML into malloc() char buffer (NULL terminated)
        bool saveXMLfile(string filename, Logger const& log        // render XML and store to file, possibly compressed, return true on success
                        ,uint gzipCompressionLevel =0);


        XMLtree accessTop();

        XMLtree addElm(string name){ return accessTop().addElm(name);  }
        XMLtree getElm(string name){ return root? accessTop().getElm(name) : XMLtree{}; }


        struct Metadata
        {
            TOPLEVEL::XML type{TOPLEVEL::XML::Instrument};
            VerInfo yoshimiVer{};
            VerInfo zynVer{};

            bool isZynCompat() const { return bool(zynVer); }
            bool isValid()     const { return bool(zynVer) or bool(yoshimiVer); }
        };
        Metadata meta;

        struct Features;

        // opens a file without parsing the XML; just grep some meta information
        static Features checkfileinformation(std::string const& filename, Logger const& log);

    private:
        void buildXMLRoot();
        Metadata extractMetadata();
        static XMLtree loadFile(string filename, Logger const& log);
};

/** used to classify instruments
 * @see XMLStore::checkfileinformation()
 */
struct XMLStore::Features
{
    int   instType{0};
    bool  yoshiFormat{false};
    uchar ADDsynth_used{false};
    uchar SUBsynth_used{false};
    uchar PADsynth_used{false};
};


/** Helper for diagnostics */
TOPLEVEL::XML parseXMLtype(string const&);
string renderXmlType(TOPLEVEL::XML);


#endif /*XML_STORE_H*/
