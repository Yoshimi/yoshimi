/*
    GuiDataExchange.cpp - threadsafe and asynchronous data exchange into the GUI

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


#include "Interface/GuiDataExchange.h"
#include "Misc/DataBlockBuff.h"
//#include "Misc/FormatFuncs.h"

//#include <functional>
#include <atomic>
#include <unordered_map>
#include <climits>
//#include <string>
//#include <array>




namespace {
    std::atomic_size_t dataExchangeID{1};

    const size_t SIZ = 512; /////////////////////TODO find a way to derive sizes
    const size_t CAP = 64;

    const size_t INITIAL_REGISTRY_BUCKETS = 64;
}

using RoutingTag = GuiDataExchange::RoutingTag;
using Subscription = GuiDataExchange::Subscription;


/**
 * Generate a new unique ID on each invocation, to be used as _Identity._
 * This allows to keep track of different connections and update receivers.
 */
size_t GuiDataExchange::generateUniqueID()
{          // Note : returning previous value before increment
    return dataExchangeID.fetch_add(+1, std::memory_order_relaxed);
}


/**
 * »PImpl« to maintain the block storage and manage the actual data exchange.
 */
class GuiDataExchange::DataManager
{
    static_assert (CAP <= UCHAR_MAX, "index will be passed via CommandBlock");
public:
    using Storage = DataBlockBuff<RoutingTag, CAP, SIZ>;
    Storage storage;

    using Registry = std::unordered_map<RoutingTag, Subscription*, size_t(&)(RoutingTag const&)>;
    Registry registry;

    DataManager()
        : storage{}
        , registry{INITIAL_REGISTRY_BUCKETS, RoutingTag::getHash}
        { }
};



// destructor needs the definition of ProtocolManager
GuiDataExchange::~GuiDataExchange() { }

/**
 * Create a protocol/mediator for data connection Core -> GUI
 * @param how_to_publish a function allowing to push a CommandBlock
 *        into some communication channel
 */
GuiDataExchange::GuiDataExchange(PublishFun how_to_publish)
    : publish{std::move(how_to_publish)}
    , manager{std::make_unique<DataManager>()}
    { }


/**
 * Open new storage slot by re-using the oldest storage buffer;
 * @param tag connection-ID to mark the new buffer, so it's contents
 *        can later be published to the correct receivers by dispatchUpdates()
 * @note using information encoded into the tag to ensure the buffer is suitable
 *        to hold a copy of the data to be published
 */
size_t GuiDataExchange::claimBuffer(RoutingTag const& tag)
{
    return manager->storage.claimNextBuffer(tag);
}

void* GuiDataExchange::getRawStorageBuff(size_t idx)
{
    return manager->storage.accessRawStorage(idx);
}


/**
 * This function is called automatically whenever a Subscription (=data receiver) is created.
 * The Subscription is associated with the RoutingTag and gets a callback for detaching on destruction
 */
GuiDataExchange::DetachHook GuiDataExchange::attachReceiver(RoutingTag const& tag, Subscription& client)
{
    DataManager::Registry& reg{manager->registry};
    // prepend to single-linked list in Registry
    client.next = reg[tag];
    reg[tag] = &client;
    return [tag,&reg](Subscription const& entry)
            {// will be called from the Subscription's destructor....
                bool found{false};
                for (Subscription** p = & reg[tag]; *p != nullptr; p = & (*p)->next)
                    if (*p == &entry)
                    {// remove entry from registry
                        *p = entry.next;
                        found = true;
                        break;
                    }
                if (not found)
                    throw std::logic_error("GuiDataExchange: registration of push data receivers corrupted.");
                if (reg[tag] == nullptr)
                    reg.erase(tag);
            };
}


void GuiDataExchange::publishSlot(size_t idx)
{
    CommandBlock notifyMsg;

    notifyMsg.data.type    = TOPLEVEL::type::Integer;
    notifyMsg.data.control = TOPLEVEL::control::dataExchange;
    notifyMsg.data.part    = TOPLEVEL::section::message;
    notifyMsg.data.source  = TOPLEVEL::action::lowPrio | TOPLEVEL::action::noAction;
    notifyMsg.data.offset  = static_cast<unsigned char>(idx);
    //
    notifyMsg.data.kit       = UNUSED;
    notifyMsg.data.engine    = UNUSED;
    notifyMsg.data.insert    = UNUSED;
    notifyMsg.data.parameter = UNUSED;
    notifyMsg.data.miscmsg   = UNUSED;
    notifyMsg.data.spare0    = UNUSED;
    notifyMsg.data.spare1    = UNUSED;
    notifyMsg.data.value     = 0;

    // send it via configured messaging channel
    publish(notifyMsg);
}

void GuiDataExchange::dispatchUpdates(CommandBlock const& notification)
{
    throw std::logic_error("unimplemented");
}

