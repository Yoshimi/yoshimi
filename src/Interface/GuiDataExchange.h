/*
    GuiDataExchange.h - threadsafe and asynchronous data exchange into the GUI

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

#ifndef GUI_DATA_EXCHANGE_H
#define GUI_DATA_EXCHANGE_H

#include "globals.h"
#include "Interface/InterChange.h"
#include "Interface/RingBuffer.h"
#include "Misc/Hash.h"
//#include "Misc/FormatFuncs.h"

#include <functional>
//#include <string>
//#include <array>
#include <utility>
#include <memory>


/**
 * A communication protocol to exchange blocks of data with the GUI.
 * Based on a publish-subscribe model with "push" from the core, but in the
 * GUI the message blocks are retrieved by "pull" by the command handling hook.
 */
class GuiDataExchange
{
    class DataManager;
    using PManager  = std::unique_ptr<DataManager>;
    using PublishFun = std::function<void(CommandBlock const&)>;

    PublishFun publish;
    PManager manager;

    static size_t generateUniqueID();

    // must not be copied nor moved
    GuiDataExchange(GuiDataExchange &&)                =delete;
    GuiDataExchange(GuiDataExchange const&)            =delete;
    GuiDataExchange& operator=(GuiDataExchange &&)     =delete;
    GuiDataExchange& operator=(GuiDataExchange const&) =delete;

public:
   ~GuiDataExchange();

    GuiDataExchange(PublishFun how_to_publish);



    /* ========== Types used to implement the communication protocol ========== */

    /** @internal tag to organise routing */
    struct RoutingTag
    {
        size_t identity{0};
        size_t typehash{0};

        template<typename DAT>
        bool verifyType()
        {
            return typehash == func::getTypeHash<DAT>();
        }

        size_t getHash()
        {
            size_t hash{0};
            func::hash_combine(hash, identity);
            func::hash_combine(hash, typehash);
            return hash;
        }
    };

    class Subscription
    {
    public:
        virtual ~Subscription();  ///< this is an interface

        Subscription* next{nullptr};
        virtual void pushUpdate(RoutingTag)   =0;
    };


    /**
     * Connection-handle and front-End for clients,
     * allowing to push data into the GUI asynchronously
     */
    template<typename DAT>
    class Connection
    {
        GuiDataExchange* hub;
        RoutingTag       tag;

    public:
        Connection(GuiDataExchange& dataExchangeLink)
            : hub{&dataExchangeLink}
            , tag{hub->generateNewTag<DAT>()}
            { }

        // standard copy operations acceptable

        void publish(DAT const& data);

        // Equality: Connections to the same routing tag are equivalent...
        template<typename DX, typename DY>
        friend bool operator==(Connection<DX> const&, Connection<DY> const&);
    };


    /**
     * Create an unique new connection handle
     * configured to transport data of type \a DAT
     */
    template<typename DAT>
    Connection<DAT> createConnection()
    {
        return Connection<DAT>(*this);
    }

    /**
     * Dispatch a notification regarding data updates -> GUI.
     * The given CommandBlock contains a data handle and destination designation;
     * actual data is fetched from the DataBlockBuff and pushed synchronously to all
     * MirrorData receivers currently enrolled actively within the GUI.
     */
    void dispatchUpdates(CommandBlock const& notification)
    {
        throw std::logic_error("unimplemented");
    }


private:
    template<typename DAT>
    RoutingTag generateNewTag()
    {
        return RoutingTag{generateUniqueID()
                         ,func::getTypeHash<DAT>()
                         };
    }

    template<typename DAT>
    auto claimSlot(RoutingTag const& tag)
    {
        size_t idx = claimBuffer(tag);
        return [this,idx](DAT const& data)
                        {   // copy-construct the data into the buffer
                            new(getRawStorageBuff(idx)) DAT{data};
                            publishSlot(idx);
                        };
    }

    size_t claimBuffer(RoutingTag const& tag);
    void*  getRawStorageBuff(size_t idx);
    void   publishSlot(size_t idx);
};



template<typename DAT>
void GuiDataExchange::Connection<DAT>::publish(DAT const& data)
{
    auto copy_and_publish = hub->claimSlot<DAT>(tag);
    copy_and_publish(data);
}



template<typename DX, typename DY>
inline bool operator==(GuiDataExchange::Connection<DX> const& con1
                      ,GuiDataExchange::Connection<DY> const& con2)
{
    return con1.tag.identity == con2.tag.identity;
}

template<typename DX, typename DY>
inline bool operator!=(GuiDataExchange::Connection<DX> const& con1
                      ,GuiDataExchange::Connection<DY> const& con2)
{
    return not (con1 == con2);
}



/////////////////////////////////////////////////////////////////////////////////////////////////WIP Prototype 1/24 - throw away when done!!!!!
void run_GuiDataExchangeTest();
/////////////////////////////////////////////////////////////////////////////////////////////////WIP Prototype 1/24 - throw away when done!!!!!
#endif /*GUI_DATA_EXCHANGE_H*/
