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
/* ================ 4/25 This is a demonstration how to read/write XML data ===================== */

#include "Misc/XMLStore.h"

#include "Misc/FileMgrFuncs.h"
#include "Misc/SynthEngine.h"

#include <iostream>
#include <string>
#include <memory>
#include <limits>
#include <cmath>

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
    XMLStore xmlNew{TOPLEVEL::XML::MasterConfig, true};
    synth.getRuntime().initBaseConfig(xmlNew);
    char* xmldata = xmlNew.render();
    cout << "Loaded XML-Tree:\n"<<string{xmldata}<<endl;
    free(xmldata);


    // Hex formating
    cout <<"Verify Bitstring conversion..." << endl;
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

    cout <<"Verify Version info..." << endl;
    CHECK(VerInfo("").maj           == 0 );
    CHECK(VerInfo("").min           == 0 );
    CHECK(VerInfo("").rev           == 0 );
    CHECK(VerInfo("55555555555").maj== std::numeric_limits<uint>::max() );
    CHECK(VerInfo("55555555555").min== 0 );
    CHECK(VerInfo("55555555555").rev== 0 );
    CHECK(VerInfo("1.2").maj        == 1 );
    CHECK(VerInfo("1.2").min        == 2 );
    CHECK(VerInfo("1.2").rev        == 0 );
    CHECK(VerInfo("1.2.").maj       == 1 );
    CHECK(VerInfo("1.2.").min       == 2 );
    CHECK(VerInfo("1.2.").rev       == 0 );
    CHECK(VerInfo("1.2.3.").maj     == 1 );
    CHECK(VerInfo("1.2.3.").min     == 2 );
    CHECK(VerInfo("1.2.3.").rev     == 3 );
    CHECK(VerInfo("1.2.3.4.5").maj  == 1 );
    CHECK(VerInfo("1.2.3.4.5").min  == 2 );
    CHECK(VerInfo("1.2.3.4.5").rev  == 3 );
    CHECK(VerInfo("x1.2.3.4.5").maj == 1 );
    CHECK(VerInfo("x1.2.3.4.5").min == 2 );
    CHECK(VerInfo("x1.2.3.4.5").rev == 3 );

    CHECK(VerInfo("1.2.3")     == VerInfo(1,2,3))
    CHECK(VerInfo("xx1.2.3uu") == VerInfo(1,2,3))
    CHECK(VerInfo("1.2")       == VerInfo(1,2,0))
    CHECK(VerInfo("6")         == VerInfo(6,0,0))
    CHECK(VerInfo("5") < VerInfo("6"))
    CHECK(VerInfo("5") < VerInfo("5.1"))
    CHECK(VerInfo("5") < VerInfo("5.0.1"))


    // the following code is a simplified version of loading the base config
    string location = file::configDir();
    string baseConfig = location + "/" + YOSHIMI + EXTEN::config;
    CHECK(file::isRegularFile(baseConfig))

    cout << "Loading from: "<<baseConfig<<endl;
    XMLStore xml{baseConfig, synth.getRuntime().getLogger()};
    CHECK(xml);

    xmldata = xml.render();
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
    CHECK(xml.saveXMLfile(TESTFILE,synth.getRuntime().getLogger()))

    cout << "Bye Cruel World..." <<endl;
}
