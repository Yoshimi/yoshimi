/*
    InterfaceAnchor.h - root context and attachment point for UI communication

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

#ifndef INTERFACE_ANCHOR_H
#define INTERFACE_ANCHOR_H

//#include "globals.h"
#include "Interface/GuiDataExchange.h"
//#include "Interface/RingBuffer.h"
//#include "Misc/Hash.h"

//#include <functional>
//#include <utility>
//#include <memory>

class SynthEngine;


/**
 * Anchor context to bootstrap the communication of Core and GUI.
 * This is a copyable data record that will be published into the GUI
 * through the GuiDataExchange system. Data transported up this way
 * allow to attach further, more fine-grained communication and provides
 * base information required for the GUI to connect to the core.
 */
struct InterfaceAnchor
{
    unsigned int synthID{0};
    SynthEngine* synth{nullptr};
    ///////////////////TODO 1/2024 : retract usage of direct SynthEngine* from UI
    ///////////////////TODO 1/2024 : can transport further generic info up into the UI here


    using Tag = GuiDataExchange::RoutingTag;

    Tag sysEffectParam;
    Tag sysEffectEQ;
    Tag insEffectParam;
    Tag insEffectEQ;
    Tag partEffectParam;
    Tag partEffectEQ;
    //...........more connection tags here....
};


#endif /*INTERFACE_ANCHOR_H*/
