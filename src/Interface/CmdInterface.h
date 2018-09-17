/*
    CmdInterface.h

    Copyright 2015-2018, Will Godfrey & others.

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

    Modified August 2018
*/

#ifndef CMDINTERFACE_H
#define CMDINTERFACE_H
#include <string>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/SynthEngine.h"
#include "Interface/InterChange.h"
#include "Effects/EffectMgr.h"

/*
 * These are all handled bit-wise so that you can set several
 * at the same time. e.g. part, addSynth, resonance.
 * There is a function that will clear just the highest bit that
 * is set so you can then step back up the level tree.
 * It is also possible to zero it so that you immediately go to
 * the top level. Therefore, the sequence is important.
 * 16 bits are currently defined out of a possible 32.
 *
 * AllFX, InsFX and Part MUST be the first three
 */
namespace LEVEL{
    enum {
        Top = 0, // set directly as an interger to clear down
        AllFX = 0, // bits from here on
        InsFX,
        Part,
        Config,
        Vector,
        Scale,
        Learn,
        AddSynth,
        SubSynth,
        PadSynth,
        AddVoice,
        Oscillator,
        Resonance,
        LFO, // amp/freq/filt
        Filter, // params only (slightly confused with env)
        Envelope, // amp/freq/filt/ Sub only band
    };
}

typedef enum {exit_msg = -1, todo_msg = 0, done_msg, value_msg, name_msg, opp_msg, what_msg, range_msg, low_msg, high_msg, unrecognised_msg, parameter_msg, level_msg, available_msg, failed_msg,} error_messages;

class CmdInterface : private MiscFuncs
{
    public:
        void defaults();
        void cmdIfaceCommandLoop();

    private:
        bool query(string text, bool priority);
        void helpLoop(list<string>& msg, string *commands, int indent);
        bool helpList(unsigned int local);
        string historySelect(int listnum, int selection);
        void historyList(int listnum);
        int effectsList(bool presets = false);
        int effects(unsigned char controlType);
        int partCommonControls(unsigned char controlType);
        int LFOselect(unsigned char controlType);
        int filterSelect(unsigned char controlType);
        int envelopeSelect(unsigned char controlType);
        int commandList();
        string findStatus(bool show);
        int contextToEngines(void);
        int toggle(void);
        bool lineEnd(unsigned char controlType);
        int commandMlearn(unsigned char controlType);
        int commandVector(unsigned char controlType);
        int commandConfig(unsigned char controlType);
        int commandScale(unsigned char controlType);
        int addSynth(unsigned char controlType);
        int subSynth(unsigned char controlType);
        int padSynth(unsigned char controlType);
        int addVoice(unsigned char controlType);
        int waveform(unsigned char controlType);
        int commandPart(bool justSet, unsigned char controlType);
        int commandReadnSet(unsigned char controlType);
        float readControl(unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char par2 = 0xff);
        void readLimits(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2);
        int sendNormal(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char par2 = 0xff);
        int sendDirect(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char par2 = 0xff, unsigned char request = 0xff);
        int cmdIfaceProcessCommand(char *cCmd);
        char *cCmd;
        char *point;
        SynthEngine *synth;
        char welcomeBuffer [128];
        int reply;
        string replyString;
        int filterVowelNumber;
        int filterFormantNumber;
        int insertType;
        int voiceNumber;
        int kitMode;
        int kitNumber;
        int npart;

        int nFX;
        int nFXtype;
        int nFXpreset;
        int chan;
        int axis;
        int mline;
        unsigned int context;
};

#endif
