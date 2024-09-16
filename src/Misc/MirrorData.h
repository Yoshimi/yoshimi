/*
    MirrorData.h - Component to store and provide data for the GUI mirrored from Core

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

#ifndef MIRROR_DATA_H
#define MIRROR_DATA_H

#include "globals.h"
#include "Interface/GuiDataExchange.h"

#include <cassert>
#include <utility>
#include <functional>

using RoutingTag = GuiDataExchange::RoutingTag;


/**
 * A »data mirror« component for the GUI.
 * As part of the GuiDataExchange protocol, this component is attached to some
 * GUI window or control and will then receive data updates pushed by the Core.
 * Optionally a callback hook can be installed to be henceforth invoked on »push«.
 */
template<class DAT>
class MirrorData
    : public GuiDataExchange::Subscription
{
    DAT data;
    std::function<void(DAT&)> updateHook{};

    void pushUpdate(RoutingTag const& tag, void* buffer)  override
    {
        assert(tag.verifyType<DAT>());  (void)tag;
        assert(buffer);
        data.~DAT(); // copy-construct into data storage
        new(&data) DAT{* reinterpret_cast<DAT*>(buffer)};
        if (updateHook)
            updateHook(data);
    }

public:
    MirrorData() = default;

    MirrorData(GuiDataExchange::Connection<DAT> con)
        : Subscription{con}
        , data{}
        { }

    MirrorData(GuiDataExchange& hub, RoutingTag tag)
        : MirrorData{GuiDataExchange::Connection<DAT>{hub,tag}}
        { }

    void activate(GuiDataExchange::Connection<DAT> con)
    {
        RoutingTag const& tag(con);
        if (not tag.verifyType<DAT>()) // is the template parameter DAT correct? did you use the proper ConnectionTag?
            throw std::logic_error{"Connection type mismatch"};
        GuiDataExchange::Subscription::activate(con);
    }

    /** install a hook to be invoked on each push update */
    template<typename FUN>
    void onUpdate(FUN&& callback)
    {
        updateHook = std::forward<FUN>(callback);
    }


    DAT& get()
    {
        return data;
    }
};

#endif /*MIRROR_DATA_H*/
