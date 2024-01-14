/*
    GuiDataExchangeTest.cpp - TEMPORARY / PROTOTYPE

    Copyright 2024,  Ichthyostega

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
/* ==================== TODO : 1/24 This is a Prototype : Throw-away when done ================== */ 

#include "Interface/GuiDataExchange.h"
#include "Misc/FormatFuncs.h"

#include <iostream>

//#include <functional>
#include <algorithm>
#include <string>
#include <array>

using std::cout;
using std::endl;
using std::string;

#define CHECK(COND) \
    if (not (COND)) {\
        cout << "FAIL: Line "<<__LINE__<<": " #COND <<endl; \
        std::terminate();\
    }

/** some »strange« test data we want to transport into the GUI */
class Heffalump
    : public std::array<char,20>
{
public:
    Heffalump()
    {
        auto h = "Heffalump.."+func::asHexString(rand());
        std::copy (h.begin(),h.end(), this->begin());
        back() = '\0';
    }
};

/**
 */
class GuiDataExchangeTest
{
public:
    void run()
    {
        
    }
};


void run_GuiDataExchangeTest()
{
    srand(time(0));
    cout << "\n■□■□■□■□■□■□■□■□◆•Gui-Data-Exchange-Test•◆□■□■□■□■□■□■□■□■\n"<<endl;
    
    // verify Heffalump (test data)
    Heffalump h1,h2;
    cout << "Hello " << h1.data() << endl;
    CHECK (sizeof(Heffalump) == 20);
    
    // all Heffalumps are unique (and can be compared)
    CHECK (h1 != h2);
    
    // Heffalumps can be copied and assigned
    h2 = h1;
    CHECK (h1 == h2);
  
    cout << "Bye Bye Cruel World..." <<endl;
}
