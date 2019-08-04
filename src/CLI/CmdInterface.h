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
#include <string>
#include <list>

#include "Interface/FileMgr.h"
#include "CLI/MiscCLI.h"
#include "Interface/InterChange.h"
#include "Effects/EffectMgr.h"

class CmdInterface : private  MiscCli, FileMgr
{
    public:
        void defaults();
        void cmdIfaceCommandLoop();

    private:
        list<string>  instrumentGroup;
        void helpLoop(list<std::string>& msg, std::string *commands, int indent, bool single = false);
        char helpList(unsigned int local);
        std::string historySelect(int listnum, int selection);
        void historyList(int listnum);
        void listCurrentParts(list<std::string>& msg_buf);
        int effectsList(bool presets = false);
        int effects(unsigned char controlType);
        int midiControllers(unsigned char controlType);
        int partCommonControls(unsigned char controlType);
        int LFOselect(unsigned char controlType);
        int filterSelect(unsigned char controlType);
        int envelopeSelect(unsigned char controlType);
        int commandGroup();
        int commandList();
        int commandMlearn(unsigned char controlType);
        int commandVector(unsigned char controlType);
        int commandConfig(unsigned char controlType);
        int commandScale(unsigned char controlType);
        int addSynth(unsigned char controlType);
        int subSynth(unsigned char controlType);
        int padSynth(unsigned char controlType);
        int resonance(unsigned char controlType);
        int addVoice(unsigned char controlType);
        int modulator(unsigned char controlType);
        int waveform(unsigned char controlType);
        int commandPart(unsigned char controlType);
        int commandReadnSet(unsigned char controlType);
        int cmdIfaceProcessCommand(char *cCmd);
        char *cCmd;
        char *point;
        SynthEngine *synth;
        char welcomeBuffer [128];
        int reply;
        std::string replyString;
};

#endif
