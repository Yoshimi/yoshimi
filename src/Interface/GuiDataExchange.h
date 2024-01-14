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

//#include "Misc/FormatFuncs.h"

#include <functional>
//#include <string>
//#include <array>
#include <utility>


/**
 * A communication protocol to exchange blocks of data with the GUI.
 * Based on a publish-subscribe model with "push" from the core, but in the
 * GUI the message blocks are retrieved by "pull" by the command handling hook.
 */
class GuiDataExchange
{
    using PublishFun = std::function<void(CommandBlock const&)>;
    
    PublishFun publish;
    
    
    // must not be copied nor moved
    GuiDataExchange(GuiDataExchange &&)                =delete;
    GuiDataExchange(GuiDataExchange const&)            =delete;
    GuiDataExchange& operator=(GuiDataExchange &&)     =delete;
    GuiDataExchange& operator=(GuiDataExchange const&) =delete;
    
public:
    /**
     * Create a protocol/mediator for data connection Core -> GUI
     * @param how_to_publish a function allowing to push a CommandBlock
     *        into some communication channel
     */
    template<typename FUN>
    GuiDataExchange(FUN&& how_to_publish)
        : publish(std::forward<FUN> (how_to_publish))
        { }
    
    
    /**
     * Connection-handle and front-End for clients,
     * allowing to push data into the GUI asynchronously
     */
    class Connection
    {
        GuiDataExchange* hub;
        
    public:
        Connection(GuiDataExchange& dataExchangeLink)
            : hub(&dataExchangeLink)
            { }
        
        // standard copy operations acceptable
        
        template<typename DAT>
        void publish(DAT const& data)
        {
            throw std::logic_error("unimplemented");
        }
        
        
        friend bool operator==(Connection const& lc, Connection const& rc)
        {
            throw std::logic_error("unimplemented");
        }
        
        friend bool operator!=(Connection const& lc, Connection const& rc)
        {
            return not (lc == rc);
        }
    };
    
    
    /**
     * Create an unique new connection handle
     * configured to transport data of type \a DAT
     */
    template<typename DAT>
    Connection createConnection()
    {
        return Connection(*this);
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
};

/////////////////////////////////////////////////////////////////////////////////////////////////WIP Prototype 1/24 - throw away when done!!!!!
void run_GuiDataExchangeTest();
/////////////////////////////////////////////////////////////////////////////////////////////////WIP Prototype 1/24 - throw away when done!!!!!
#endif /*GUI_DATA_EXCHANGE_H*/
