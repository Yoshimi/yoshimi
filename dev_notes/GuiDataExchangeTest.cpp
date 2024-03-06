/*
    GuiDataExchangeTest.cpp - TEMPORARY / PROTOTYPE / DEMO

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
/* ================ 1/24 This is a demonstration how GuiDataExchange works  ===================== */ 

#include "Interface/GuiDataExchange.h"
#include "Interface/InterChange.h"
#include "Misc/MirrorData.h"
#include "Misc/FormatFuncs.h"

#include <iostream>

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
    h2 = Heffalump();
    CHECK (h1 != h2);
    
    
    // =============================================== setup (fake) communication infrastructure (for this test)
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
    
    // *Gui-Data-Exchange* : central facility to manage exchange connections
    GuiDataExchange guiDataExchange(sendData);
    
    
    // =============================================== setup a connection-identity
    auto con = guiDataExchange.createConnection<Heffalump>();
    // has unique identity
    CHECK (con != guiDataExchange.createConnection<Heffalump>());
    CHECK (con != guiDataExchange.createConnection<float>());
    // can be copied and assigned
    GuiDataExchange::Connection<Heffalump> c2(con);
    CHECK (con == c2);
    c2 = guiDataExchange.createConnection<Heffalump>();
    CHECK (con != c2);
    // can not be assigned with the wrong data buffer type
//  c2 = guiDataExchange.createConnection<float>();             //////// does not compile due to different template argument DAT=float
    
    
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
    
    
    // =============================================== bootstrap a new receiver from a published data block
    size_t slotIDX = con.emplace(h2);
    
    // the following happens »elsewhere« (e.g. in the GUI)
    GuiDataExchange::Connection<Heffalump> c3 = guiDataExchange.bootstrapConnection<Heffalump>(slotIDX);
    MirrorData<Heffalump> receiver3{c3};
    
    CHECK (h1 != receiver3.get());
    CHECK (h2 != receiver3.get());
    CHECK (h1 == receiver.get());
    
    // cause a push directly from given index
    guiDataExchange.pushUpdates(slotIDX);
    CHECK (h2 == receiver.get());
    CHECK (h2 == receiver3.get());
    
    // the new connection is fully usable for publishing
    c3.publish(h1);
    guiDataExchange.dispatchUpdates(pullData());
    CHECK (h1 == receiver.get());
    CHECK (h1 == receiver3.get());
    
    
    // =============================================== can install a hook to be activated on each push
    string proofMark{};
    receiver3.onUpdate([&](Heffalump const& h)
                        {
                            proofMark = string{h.data()};
                        });
    
    // on next push-update...
    con.publish(h2);
    guiDataExchange.dispatchUpdates(pullData());
    CHECK (proofMark == h2.data());
    
    
    cout << "Bye Bye "<<receiver.get().data() <<endl;
}
