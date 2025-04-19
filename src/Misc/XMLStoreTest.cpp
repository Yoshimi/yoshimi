/*
    XMLStoreTest.cpp - TEMPORARY / PROTOTYPE

    Copyright 2025,  Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the License,
    or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License (version 2
    or later) for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/

/* ============================================================================================== */
/* ==================== TODO : 4/25 This is a Prototype : Throw-away when done ================== */

#include "Misc/XMLStore.h"

#include "Misc/FileMgrFuncs.h"
#include "Misc/SynthEngine.h"
//#include "Misc/FormatFuncs.h"

#include <iostream>
#include <string>
#include <memory>
#include <limits>
#include <cmath>

//#include <functional>
//#include <string>
//#include <array>

using std::string;
using std::cout;
using std::endl;


#define CHECK(COND) \
    if (not (COND)) {\
        cout << "FAIL: Line "<<__LINE__<<": " #COND <<endl; \
        std::terminate();\
    }


void run_XMLStoreTest(SynthEngine& synth)
{
    cout << "+++ Test XML handling................................." << endl;

    // Hex formating
    cout <<"int(0)   "<< func::asHexString(0)<<endl;
    cout <<"int(15)  "<< func::asHexString(15)<<endl;
    cout <<"int(-1)  "<< func::asHexString(-1)<<endl;
    cout <<"uint(-1) "<< func::asHexString(uint(-1))<<endl;
    cout <<"ExactBitstring 0.0           "<< func::asExactBitstring(0.0)<<endl;
    cout <<"ExactBitstring 1.01          "<< func::asExactBitstring(1.01)<<endl;
    cout <<"ExactBitstring -1.01         "<< func::asExactBitstring(-1.01)<<endl;
    cout <<"ExactBitstring float.max     "<< func::asExactBitstring(std::numeric_limits<float>::max())<<endl;
    cout <<"ExactBitstring float.min     "<< func::asExactBitstring(std::numeric_limits<float>::min())<<endl;
    cout <<"ExactBitstring float.lowest  "<< func::asExactBitstring(std::numeric_limits<float>::lowest())<<endl;
    cout <<"ExactBitstring float.epsilon "<< func::asExactBitstring(std::numeric_limits<float>::epsilon())<<endl;
    cout <<"ExactBitstring float +inf    "<< func::asExactBitstring(INFINITY)<<endl;
    cout <<"ExactBitstring float nan     "<< func::asExactBitstring(NAN)<<endl;
    
    cout <<"ExactBitstring 0.0           "<< func::asExactBitstring(0.0)<<endl;
    cout <<"ExactBitstring 1.01          "<< func::asExactBitstring(1.01)<<endl;
    cout <<"ExactBitstring -1.01         "<< func::asExactBitstring(-1.01)<<endl;
    cout <<"ExactBitstring float.max     "<< func::asExactBitstring(std::numeric_limits<float>::max())<<endl;
    cout <<"ExactBitstring float.min     "<< func::asExactBitstring(std::numeric_limits<float>::min())<<endl;
    cout <<"ExactBitstring float.lowest  "<< func::asExactBitstring(std::numeric_limits<float>::lowest())<<endl;
    cout <<"ExactBitstring float.epsilon "<< func::asExactBitstring(std::numeric_limits<float>::epsilon())<<endl;
    cout <<"ExactBitstring float +inf    "<< func::asExactBitstring(INFINITY)<<endl;
    cout <<"ExactBitstring float nan     "<< func::asExactBitstring(NAN)<<endl;
    cout <<"read Bitstring  0x00000000 : "<< func::bitstring2float("0x00000000")<<endl;
    cout <<"read Bitstring  0x3F8147AE : "<< func::bitstring2float("0x3F8147AE")<<endl;
    cout <<"read Bitstring  0xBF8147AE : "<< func::bitstring2float("0xBF8147AE")<<endl;
    cout <<"read Bitstring  0x7F7FFFFF : "<< func::bitstring2float("0x7F7FFFFF")<<endl;
    cout <<"read Bitstring  0x00800000 : "<< func::bitstring2float("0x00800000")<<endl;
    cout <<"read Bitstring  0xFF7FFFFF : "<< func::bitstring2float("0xFF7FFFFF")<<endl;
    cout <<"read Bitstring  0x34000000 : "<< func::bitstring2float("0x34000000")<<endl;
    cout <<"read Bitstring  0x7F800000 : "<< func::bitstring2float("0x7F800000")<<endl;
    cout <<"read Bitstring  0x7FC00000 : "<< func::bitstring2float("0x7FC00000")<<endl;
    cout << endl;

    // the following code is a simplified version of loading the base config
    string location = file::configDir();
    string baseConfig = location + "/" + YOSHIMI + EXTEN::config;
    CHECK(file::isRegularFile(baseConfig))
    cout << "Loading from: "<<baseConfig<<endl;

    synth.getRuntime().xmlType = TOPLEVEL::XML::MasterConfig;

    XMLStore xml{TOPLEVEL::XML::MasterConfig, synth, true};    //////////////////OOO should define other ctor to load file directly!
    synth.getRuntime().initData(xml);
    bool success = xml.loadXMLfile(baseConfig);
    CHECK(success);

    char* xmldata = xml.getXMLdata();
    cout << "Loaded XML-Tree:\n"<<string{xmldata}<<endl;
    free(xmldata);

    XMLtree baseParam = xml.getElm("BASE_PARAMETERS");
    CHECK(baseParam);
    bool guiParam = baseParam.getPar_bool("enable_gui", true);
    uint compParam = baseParam.getPar_int("gzip_compression", 5, 0, 9);
    string guideVersion{baseParam.getPar_str("guide_version")};

    cout << "enable_gui:"<<guiParam
         << "\ngzip_compression:"<<compParam
         << "\nguide_version:"<<guideVersion
         << endl;

    baseParam.addPar_real("Heffalump", (1+sqrtf(5))/2);

    const string TESTFILE{"heffalump.xml"};
    CHECK(xml.saveXMLfile(TESTFILE, false))

    cout << "Bye Cruel World..." <<endl;
}
