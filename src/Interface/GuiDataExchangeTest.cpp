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
#include "Misc/MirrorData.h"
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



void run_GuiDataExchangeTest()
{
    srand(time(0));
    cout << "\n■□■□■□■□■□■□■□■□◆•Gui-Data-Exchange-Test•◆□■□■□■□■□■□■□■□■\n"<<endl;
    
    
    // =============================================== verify Heffalump (test data)
    Heffalump h1,h2;
    cout << "Hello " << h1.data() << endl;
    CHECK (sizeof(Heffalump) == 20);
    
    // all Heffalumps are unique (and can be compared)
    CHECK (h1 != h2);
    
    // Heffalumps can be copied and assigned
    h2 = h1;
    CHECK (h1 == h2);
    
    
    // =============================================== setup a connection-identity
    // use a dummy-ringbuffer for this test...
    static constexpr size_t commandBlockSize = sizeof (CommandBlock);
    RingBuffer <10, log2 (commandBlockSize)> simulatedGUI;
    auto sendData = [&simulatedGUI](CommandBlock const& block)
                                    {
                                        simulatedGUI.write(block.bytes);
                                    };
    auto pullData = [&simulatedGUI]() -> CommandBlock
                                    {
                                        CommandBlock getData;
                                        simulatedGUI.read(getData.bytes);
                                        return getData;
                                    };
    
    // Central instance to manage exchange connections
    GuiDataExchange guiDataExchange(sendData);
    
    auto con = guiDataExchange.createConnection<Heffalump>();
    // has unique identity
    CHECK (con != guiDataExchange.createConnection<Heffalump>());
    CHECK (con != guiDataExchange.createConnection<float>());
    // can be copied and assigned
    GuiDataExchange::Connection c2(con);
    CHECK (con == c2);
    c2 = guiDataExchange.createConnection<Heffalump>();
    CHECK (con != c2);
    // can not be assigned with the wrong data buffer type
//  c2 = GuiDataExchange::connection<float>();              //////// throws logic_error
    
    
    // =============================================== setup a receiver
    MirrorData<Heffalump> receiver(con);
    // holds default-constructed data
    Heffalump& receivedData = receiver.get();
    CHECK (receivedData != h1);
    CHECK (receivedData != h2);
    
    
    // =============================================== Core publishes data
    con.publish(h1);
    // not transported to the GUI yet
    CHECK (receivedData != h1);
    
    
    // =============================================== GUI loop pulls and dispatches updates
    guiDataExchange.dispatchUpdates(pullData());
    // buffer contents were push-updated
    CHECK (receivedData == h1);
    
    
    // =============================================== dynamic registration of multiple receivers
    { //nested scope
        MirrorData<Heffalump> receiver2(con);
        CHECK (h1 != receiver2.get());
        CHECK (h1 == receiver.get());
        
        con.publish(h2);
        CHECK (h2 != receiver2.get());
        CHECK (h2 != receiver.get());
        CHECK (h1 == receiver.get());
        
        guiDataExchange.dispatchUpdates(pullData());
        CHECK (h2 == receiver2.get());
        CHECK (h2 == receiver.get());
        
        con.publish(h1);
        CHECK (h2 == receiver2.get());
        CHECK (h2 == receiver.get());
    }//(End)nested scope
    // receiver2 does not exist anymore...
    guiDataExchange.dispatchUpdates(pullData());
    CHECK (h1 == receiver.get());
    
    cout << "Bye Bye "<<receiver.get().data() <<endl;
}
