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
#include "Misc/FormatFuncs.h"

#include <unordered_map>
#include <climits>
#include <chrono>
#include <atomic>
#include <mutex>


                                //////////////// !! NOTE important : add all relevant types here which shall be published via GuiDataExchange !!
#include "Interface/InterfaceAnchor.h"
#include "Effects/EffectMgr.h"

namespace {
    const size_t SIZ = MaxSize<Types<InterfaceAnchor
                                    ,EffectDTO
                                    ,EqGraphDTO
                                    /////////////////////////////////////////TODO 1/24 : add more actual types here
                                    >>::value;

    const size_t CAP = 64;                       ///< (fixed) number of slots (each with size SIZ) to pre-allocate
    const size_t INITIAL_REGISTRY_BUCKETS = 64;  ///< initial size for the hashtable used for lookup of data receivers

    std::atomic_size_t dataExchangeID{1};

    /** when to consider an asynchronous data message still "on time" */
    inline bool isTimely(std::chrono::milliseconds millis)
    {
        return 0ms <= millis and millis < 500ms;
    }
}

using Guard = const std::lock_guard<std::mutex>;
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
    std::mutex mtx;

    using Storage = DataBlockBuff<RoutingTag, CAP, SIZ>;
    Storage storage;

    using Registry = std::unordered_map<RoutingTag, Subscription*, size_t(&)(RoutingTag const&)>;
    Registry registry;

    DataManager()
        : mtx{}
        , storage{}
        , registry{INITIAL_REGISTRY_BUCKETS, RoutingTag::getHash}
        { }
};




// destructor needs the definition of ProtocolManager
GuiDataExchange::~GuiDataExchange() { }

GuiDataExchange::Subscription::~Subscription()
{
    if (detach)
        detach(*this);
}


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
 * @param dataSize size of the actual data to be copied into the buffer; could be used
 *        to select from a differentiated storage pool (sanity check only as of 1/2024)
 * @param storeIntoBuffer a function with signature `void(void*)` to _drop off_
 *        the actual payload into the buffer slot.
 * @return the indexNr of the claimed slot
 * @note
 *  - using information encoded into the tag to ensure the buffer
 *    size is sufficient to hold a copy of the data to be published
 *  - note this function also constitutes a _memory synchronisation bracket_
 *    to ensure the changes to the buffer structure are visible to other threads
 */
size_t GuiDataExchange::claimNextSlot(RoutingTag const& tag, size_t dataSize, EmplaceFun storeIntoBuffer)
{
    if (dataSize > SIZ)
        throw std::logic_error("Insufficient preconfigured buffer size "
                               "to hold an object of size="
                              + func::asString(dataSize));
    Guard lock(manager->mtx);
    // protect against concurrent data corruption and ensure visibility of published data

    size_t slotIdx = manager->storage.claimNextBuffer(tag);
    void* rawStorageBuff = manager->storage.accessRawStorage(slotIdx);
    storeIntoBuffer(rawStorageBuff);
    return slotIdx;
}



/**
 * This function is called automatically whenever a Subscription (=data receiver) is created.
 * The Subscription is associated with the RoutingTag and gets a callback for detaching on destruction
 */
GuiDataExchange::DetachHook GuiDataExchange::attachReceiver(RoutingTag const& tag, Subscription& client)
{
    DataManager::Registry& reg{manager->registry};
    std::mutex& mtx = manager->mtx;
    Guard lock(mtx);
    // prepend to single-linked list in Registry
    client.next = reg[tag];
    reg[tag] = &client;
    return [tag,&reg,&mtx](Subscription const& entry)
            {// will be called from the Subscription's destructor....
                bool found{false};
                Guard lock(mtx);
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


RoutingTag GuiDataExchange::fetchTag(size_t idx)
{
    return manager->storage.getRoutingTag(idx);
}


bool GuiDataExchange::isValidPushMsg(CommandBlock const& notification)
{
    size_t slotIDX = notification.data.offset;
    return notification.data.control == TOPLEVEL::control::dataExchange
       and notification.data.part    == TOPLEVEL::section::message
       and isTimely(manager->storage.entryAge(slotIDX));
}


void GuiDataExchange::dispatchUpdates(CommandBlock const& notification)
{
    if (notification.data.control != TOPLEVEL::control::dataExchange)
        return;
    pushUpdates(notification.data.offset);
}

void GuiDataExchange::pushUpdates(size_t idx)
{
    if (idx >= CAP)
        throw std::logic_error("GuiDataExchange: invalid data slot index "+func::asString(idx));

    Guard lock(manager->mtx); // sync barrier to ensure visibility of data published by other thread

    if (not isTimely(manager->storage.entryAge(idx)))
        return;
    RoutingTag tag = fetchTag(idx);
    void* rawData = manager->storage.accessRawStorage(idx);
    DataManager::Registry& reg{manager->registry};
    auto entry = reg.find(tag);
    if (entry == reg.end())
        return; // no(longer any) subscribers for this conversation channel
    for (Subscription* p = entry->second; p; p=p->next)
        p->pushUpdate(tag, rawData);
}

