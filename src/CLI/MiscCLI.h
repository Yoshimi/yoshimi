/*
    MiscCLI.h

    Copyright 2019, Will Godfrey.

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

#ifndef MISCCLI_H
#define MISCCLI_H



#include "Misc/MiscFuncs.h"
#include "Interface/FileMgr.h"
#include "Misc/SynthEngine.h"
#include "Interface/TextLists.h"

class MiscCli : public MiscFuncs
{
    public:
        float readControl(SynthEngine *synth, unsigned char action, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char offset = 0xff, unsigned char miscmsg = 0xff);
        std::string readControlText(SynthEngine *synth, unsigned char action, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char offset = 0xff);
        void readLimits(SynthEngine *synth, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char miscmsg);
        int sendNormal(SynthEngine *synth, unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char offset = 0xff, unsigned char miscmsg = 0xff);
        int sendDirect(SynthEngine *synth, unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char offset = 0xff, unsigned char miscmsg = 0xff, unsigned char request = 0xff);


};

#endif
