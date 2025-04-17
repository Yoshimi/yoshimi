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

    CHECK(xml.enterbranch("BASE_PARAMETERS"));
    bool guiParam = xml.getparbool("enable_gui", true);
    uint compParam = xml.getpar("gzip_compression", 5, 0, 9);
    string guideVersion{xml.getparstr("guide_version")};

    cout << "enable_gui:"<<guiParam
         << "\ngzip_compression:"<<compParam
         << "\nguide_version:"<<guideVersion
         << endl;

    xml.endbranch();
    xml.addparreal("Heffalump", (1+sqrtf(5))/2);

    const string TESTFILE{"heffalump.xml"};
    CHECK(xml.saveXMLfile(TESTFILE, false))

    cout << "Bye Cruel World..." <<endl;
}
