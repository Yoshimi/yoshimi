/*
    CmdInterface.h

    Copyright 2015-2016, Will Godfrey and others.

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
#include <string>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/SynthEngine.h"
#include "Interface/InterChange.h"
#include "Effects/EffectMgr.h"

extern map<SynthEngine *, MusicClient *> synthInstances;

// all_fx and ins_fx MUST be the first two
typedef enum { all_fx = 0, ins_fx, vect_lev, part_lev, } level_bits;

typedef enum { todo_msg = 0, done_msg, value_msg, name_msg, opp_msg, what_msg, range_msg, low_msg, high_msg, unrecognised_msg, parameter_msg, level_msg, available_msg,} error_messages;

class CmdInterface : private MiscFuncs
{
    public:
        void defaults();
        void cmdIfaceCommandLoop();

    private:
        bool query(string text, bool priority);
        void helpLoop(list<string>& msg, string *commands, int indent);
        bool helpList();
        void historyList(int listnum);
        int effectsList();
        int effects();
        int volPanShift();
        int commandVector();
        int commandPart(bool justSet);
        int commandReadnSet();
        int sendDirect(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char par2 = 0xff);
        bool cmdIfaceProcessCommand();
        char *cCmd;
        char *point;
        SynthEngine *synth;
        char welcomeBuffer [128];

        int npart;
        int nFX;
        int nFXtype;
        int nFXpreset;
        int chan;
        int axis;
        unsigned int level;
        string replyString;
        bool isRead;
};

#endif
