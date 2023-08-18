/*
    CmdInterpreter.h

    Copyright 2019 - 2022, Will Godfrey and others.

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

#ifndef CMDINTERPRETER_H
#define CMDINTERPRETER_H

#include <string>
#include <memory>
#include <list>

#include "Misc/SynthEngine.h"
#include "Interface/TextLists.h"

using std::string;
using std::list;

class TextMsgBuffer;

namespace test {
    class TestInvoker;
}

namespace cli {


struct Reply
{
    // note these are immutable
    const int code;
    const string msg;

    Reply(int c, string m ="") :
        code{c},
        msg{m}
    { }

    // reassignment prohibited
    Reply& operator=(const Reply&)  = delete;


    // some frequently used shortcuts
    static Reply DONE;

    static Reply what(string question)
    {
        return Reply{REPLY::what_msg, question};
    }
};

class Parser;


class CmdInterpreter
{
    public:
        CmdInterpreter();
       ~CmdInterpreter();

        string buildStatus(bool showPartDetails);
        Reply cmdIfaceProcessCommand(Parser& input);

        unsigned int currentInstance;
        SynthEngine *synth;

    private:
        string buildAllFXStatus();
        string buildPartStatus(bool showPartDetails);
        string buildTestStatus();

        void defaults();
        void resetInstance(unsigned int newInstance);
        bool query(string text, bool priority);
        void helpLoop(list<string>& msg, string *commands, int indent, bool single = false);
        char helpList(Parser& input, unsigned int local);
        void copypasteList(string name);
        string historySelect(int listnum, size_t selection);
        void historyList(int listnum);
        void listCurrentParts(Parser& input, list<string>& msg_buf);
        int effectsList(Parser& input, bool presets = false);
        int effects(Parser& input, unsigned char controlType);
        int midiControllers(Parser& input, unsigned char controlType);
        int LFOselect(Parser& input, unsigned char controlType);
        float freqBPMset(Parser& input, bool isBPM, int max = 127);
        int filterSelect(Parser& input, unsigned char controlType);
        int envelopeSelect(Parser& input, unsigned char controlType);
        int commandGroup(Parser& input);
        int commandList(Parser& input);
        int commandMlearn(Parser& input, unsigned char controlType);
        int commandVector(Parser& input, unsigned char controlType);
        int commandBank(Parser& input, unsigned char controlType, bool justEntered = false);
        int commandConfig(Parser& input, unsigned char controlType);
        int commandScale(Parser& input, unsigned char controlType);
        int commandImportScale(Parser& input);
        int commandExportScale(Parser& input);
        int addSynth(Parser& input, unsigned char controlType);
        int subSynth(Parser& input, unsigned char controlType);
        int padSynth(Parser& input, unsigned char controlType);
        int resonance(Parser& input, unsigned char controlType);
        int addVoice(Parser& input, unsigned char controlType);
        int modulator(Parser& input, unsigned char controlType);
        int waveform(Parser& input, unsigned char controlType);
        int commandPart(Parser& input, unsigned char controlType);
        int commandTest(Parser& input, unsigned char controlType);
        int commandReadnSet(Parser& input, unsigned char controlType);
        void presetsControl(float value, unsigned char type, unsigned char section, unsigned char kitNumber, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset = UNUSED,unsigned char miscmsg = UNUSED);
        Reply processSrcriptFile(const string& filename, bool toplevel = true);

    private:
        list<string>  instrumentGroup;
        TextMsgBuffer& textMsgBuffer;

        std::unique_ptr<test::TestInvoker> testInvoker;
        test::TestInvoker& getTestInvoker();



        /* == state fields == */  // all these are used by findStatus()

        // the following are used pervasively
        unsigned int context;
        int section;
        int npart;
        int kitMode;
        int kitNumber;
        bool inKitEditor;
        int engine;
        int voiceNumber;
        int insertGroup;
        int insertType;
        int nFXtype;
        int nFX;

        // the remaining ones are only used at some places
        int nFXpreset;
        int nFXeqBand;

        int filterSequenceSize;
        int filterVowelNumber;
        int filterNumberOfFormants;
        int filterFormantNumber;

        int chan;
        int axis;
        int mline;
};

}//(End)namespace cli
#endif /*CMDINTERPRETER_H*/
