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
//#include "Misc/FormatFuncs.h"

#include <functional>
//#include <string>
//#include <array>


/**
 * A communication protocol to exchange blocks of data with the GUI.
 * Based on a publish-subscribe model with "push" from the core, but in the
 * GUI the message blocks are retrieved by "pull" by the command handling hook.
 */
class GuiDataExchange
{
    using HandlerFun = std::function<void()>;

public:
};

/////////////////////////////////////////////////////////////////////////////////////////////////WIP Prototype 1/24 - throw away when done!!!!!
void run_GuiDataExchangeTest();
/////////////////////////////////////////////////////////////////////////////////////////////////WIP Prototype 1/24 - throw away when done!!!!!
#endif /*GUI_DATA_EXCHANGE_H*/
