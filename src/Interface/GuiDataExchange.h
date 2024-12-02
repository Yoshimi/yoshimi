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
#include "Interface/RingBuffer.h"
#include "Misc/Hash.h"

#include <functional>
#include <cassert>
#include <utility>
#include <memory>


/**
 * A communication protocol to exchange blocks of data with the GUI.
 * Based on a publish-subscribe model with "push" from the core, but in the
 * GUI the message blocks are retrieved by "pull" by the command handling hook.
 *
 * GuiDataExchange can handle several distinct _communication channels,_ each
 * allowing to publish some _arbitrary_ yet _specifically typed_ data blocks
 * to several "listeners" / "subscribers".
 * - A new channel is opened by [creating a connection](\ref GuiDataExchange::createConnection)
 *   Note that the type `Connection<DAT>` is templated to a specific data type to transport
 * - Connection objects are handles and freely copyable. All equivalent handles represent
 *   the same connection, and can be used to operate on that connection
 * - A receiver (typically in the GUI) must be created from such a connection handle;
 *   it must be a subclass of GuiDataExchange::Subscription and implement the single
 *   pure virtual method Subscription::pushUpdate
 * - Registration and de-registration of Subscriptions is managed automatically
 *   (by the constructor / the destructor)
 * - to publish new data, invoke Connection<DAT>::publish(data)
 * - this causes a _copy_ of that data to be stored into an internal data ringbuffer;
 *   moreover, a notification-message is sent through the Yoshimi CommandBlock system.
 * - The code handling GUI updates in the »main thread« will receive this notification
 *   and has then to invoke GuiDataExchange::dispatchUpdates(commandBlock), which will
 *   use the internal registry of Subscribers to push an update to each active receiver.
 */
class GuiDataExchange
{
    class DataManager;
    using PManager  = std::unique_ptr<DataManager>;
    using EmplaceFun = std::function<void(void*)>;
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
        bool verifyType()  const
        {
            return typehash == func::getTypeHash<DAT>();
        }

        static size_t getHash(RoutingTag const&);
        bool operator==(RoutingTag const& otag) const;
        bool operator!=(RoutingTag const& otag) const;
    };


    class Subscription;
    using DetachHook = std::function<void(Subscription const&)>;

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
        Connection(GuiDataExchange& link, RoutingTag id)
            : hub{&link}
            , tag{id}
            { }

        // standard copy operations acceptable (but only for same DAT)

        operator RoutingTag const&()  const { return tag; }

        void publish(DAT const& data);
        size_t emplace(DAT const& data);
        DetachHook attach(Subscription&);

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
        return Connection<DAT>(*this, generateNewTag<DAT>());
    }

    /**
     * Establish a connection with a routing tag retrieved from
     * a designated data slot currently present in the buffer.
     * Typically used to bootstrap a client-side end point.
     */
    template<typename DAT>
    Connection<DAT> bootstrapConnection(size_t slotIdx)
    {
        RoutingTag routingTag = fetchTag(slotIdx);
        assert (routingTag.verifyType<DAT>());
        return Connection<DAT>(*this, routingTag);
    }


    /**Interface used to mark and track all receivers of data push-updates */
    class Subscription
    {
        // must not be copied nor moved
        Subscription(Subscription &&)                =delete;
        Subscription(Subscription const&)            =delete;
        Subscription& operator=(Subscription &&)     =delete;
        Subscription& operator=(Subscription const&) =delete;

    protected:
        template<typename DAT>
        Subscription(Connection<DAT>& connection)
            : detach{connection.attach(*this)}
            { }

        virtual ~Subscription(); ///< detaches automatically

        Subscription() = default;

        template<typename DAT>
        void activate(Connection<DAT>& connection)
        {
            if (detach)
                throw std::logic_error("Subscription already activated; "
                                       "can only attach once.");
            detach = connection.attach(*this);
        }

    public:
        Subscription* next{nullptr};
        virtual void pushUpdate(RoutingTag const&, void* data) =0;
    private:
        DetachHook detach;
    };


    /**
     * Dispatch a notification regarding data updates -> GUI.
     * The given CommandBlock contains a data handle(index); routing info an
     * actual data is fetched from the DataBlockBuff and pushed synchronously to all
     * MirrorData receivers currently enrolled actively within the GUI and marked
     * with the same RoutingTag as found in the index table.
     */
    void dispatchUpdates(CommandBlock const& notification);

    /** performs the actual push-dispatch
     * @param idx valid "slot" holding data to publish
     */
    void pushUpdates(size_t idx);


private:
    template<typename DAT>
    RoutingTag generateNewTag()
    {
        return RoutingTag{generateUniqueID()
                         ,func::getTypeHash<DAT>()
                         };
    }

    DetachHook attachReceiver(RoutingTag const&, Subscription&);
    size_t claimNextSlot(RoutingTag const&, size_t, EmplaceFun);
    void   publishSlot(size_t idx);
    RoutingTag fetchTag(size_t idx);
};



template<typename DAT>
inline size_t GuiDataExchange::Connection<DAT>::emplace(DAT const& data)
{
    const size_t dataSiz = sizeof(DAT);
    return hub->claimNextSlot(this->tag
                             ,dataSiz
                             ,[&data](void* buffer)
                                  {// copy-construct the data into the buffer
                                      new(buffer) DAT{data};
                                  });
}

template<typename DAT>
inline void GuiDataExchange::Connection<DAT>::publish(DAT const& data)
{
    size_t idx = emplace(data);
    hub->publishSlot(idx);
}


template<typename DAT>
inline GuiDataExchange::DetachHook GuiDataExchange::Connection<DAT>::attach(Subscription& client)
{
    return hub->attachReceiver(tag, client);
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


inline size_t GuiDataExchange::RoutingTag::getHash(RoutingTag const& tag)
{
    size_t hash{0};
    func::hash_combine(hash, tag.identity);
    func::hash_combine(hash, tag.typehash);
    return hash;
}

inline bool GuiDataExchange::RoutingTag::operator==(RoutingTag const& otag)  const
{
    return this->identity == otag.identity
       and this->typehash == otag.typehash;
}
inline bool GuiDataExchange::RoutingTag::operator!=(RoutingTag const& otag)  const
{
    return not (*this == otag);
}


#endif /*GUI_DATA_EXCHANGE_H*/
