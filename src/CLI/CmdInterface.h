/*
    CmdInterface.h

    Copyright 2015-2019, Will Godfrey & others.

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef CMDINTERFACE_H
#define CMDINTERFACE_H

#include "CLI/CmdInterpreter.h"



class CmdInterface
{
    public:
        CmdInterface();

        void cmdIfaceCommandLoop();

    private:
        cli::CmdInterpreter interpreter;

        Config& getRuntime();
        void Log(const string& , char tostderr =0);
};

#endif
