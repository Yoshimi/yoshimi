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
//#include "Misc/FormatFuncs.h"

//#include <functional>
//#include <string>
//#include <array>


// emit VTable for the interface here....
GuiDataExchange::Subscription::~Subscription() { }


/**
 */
size_t GuiDataExchange::generateUniqueID()
{
    /////TODO lala
}
