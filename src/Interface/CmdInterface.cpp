/*
    CmdInterface.cpp

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

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <cstdio>
#include <cerrno>
#include <cfloat>
#include <sys/types.h>
#include <ncurses.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <algorithm>
#include <iterator>
#include <map>
#include <list>
#include <sstream>
#include <sys/time.h>

using namespace std;

#include "Misc/SynthEngine.h"
#include "Misc/MiscFuncs.h"
#include "Misc/Bank.h"

#include "Interface/InterChange.h"
#include "Interface/CmdInterface.h"

using namespace std;

extern SynthEngine *firstSynth;
static unsigned int currentInstance = 0;

namespace LISTS {
    enum {
    all = 0,
    syseff, // not yet
    inseff, // not yet
    part,
    common,
    addsynth,
    subsynth,
    padsynth,
    vector,
    scale,
    load,
    save,
    list,
    config,
    mlearn
    };
}

string basics[] = {
    "?  Help",                  "show commands",
    "STop",                     "all sound off",
    "RESet [s]",                "return to start-up conditions, 'ALL' clear MIDI-learn (if 'y')",
    "EXit",                     "tidy up and close Yoshimi (if 'y')",
    "..",                       "step back one level",
    "/",                        "step back to top level",
    "end"
};

string toplist [] = {
    "ADD",                      "add paths and files",
    "  Root <s>",               "root path to list",
    "  Bank <s>",               "make new bank in current root",
    "  YOshimi [n]",            "new Yoshimi instance ID",
    "IMPort [s <n1>] <n2> <s>", "import named directory to slot n2 of current root, (or 'Root' n1)",
    "EXPort [s <n1>] <n2> <s>", "export bank at slot n2 of current root, (or 'Root' n1) to named directory",
    "REMove",                   "remove paths, files and entries",
    "  Root <n>",               "de-list root path ID",
    "  Bank [s <n1>] <n2>",     "delete bank ID n2 (and all instruments) from current root (or 'Root' n1)",
    "  YOshimi <n>",            "close instance ID",
    "  MLearn <s> [n]",         "delete midi learned 'ALL' whole list, or '@'(n) line",
    "Set/Read/MLearn",          "manage all main parameters",
    "MINimum/MAXimum/DEFault",  "find ranges",
    "  Part",                   "enter context level",
    "  VEctor",                 "enter context level",
    "  SCale",                  "enter context level",
    "  MLearn",                 "enter context level",
    "  COnfig",                 "enter context level",
    "  Root <n>",               "current root path to ID",
    "  Bank <n>",               "current bank to ID",
    "  SYStem effects [n]",     "enter effects context level",
    "    Type <s>",             "the effect type",
    "    PREset <n2>",          "set numbered effect preset to n2",
    "    SEnd <n2> <n3>",       "send system effect to effect n2 at volume n3",
    "  INSert effects [n1]",    "enter effects context level",
    "    Type <s>",             "the effect type",
    "    PREset <n2>",          "set numbered effect preset to n2",
    "    SEnd <s>/<n2>",        "set where (Master, Off or part number)",
    "  AVailable <n>",          "available parts (16, 32, 64)",
    "  Volume <n>",             "master volume",
    "  SHift <n>",              "master key shift semitones (0 no shift)",
    "  DEtune <n>",             "master fine detune",
    "  SOlo [s] [n]",           "channel 'solo' switcher (Row, Column, Loop, Twoway, CC, {other} Disable)",
    "      CC <n>",             "Incoming 'solo' CC number (type must be set first)",
    "end"
};

string configlist [] = {
    "Oscillator <n>",           "* Add/Pad size (power 2 256-16384)",
    "BUffer <n>",               "* internal size (power 2 16-4096)",
    "PAdsynth [s]",             "interpolation type (Linear, other = cubic)",
    "Virtual <n>",              "keyboard (0 = QWERTY, 1 = Dvorak, 2 = QWERTZ, 3 = AZERTY)",
    "Xml <n>",                  "compression (0-9)",
    "REports [s]",              "destination (Stdout, other = console)",
    "SAved [s]",                "Saved instrument type (Legacy {.xiz}, Yoshimi {.xiy}, Both)",

    "STate [s]",                "* autoload default at start (Enable {other})",
    "Hide [s]",                 "non-fatal errors (Enable {other})",
    "Display [s]",              "GUI splash screen (Enable {other})",
    "Time [s]",                 "add to instrument load message (Enable {other})",
    "Include [s]",              "XML headers on file load(Enable {other})",
    "Keep [s]",                 "include inactive data on all file saves (Enable {other})",
    "Gui [s]",                  "* Run with GUI (Enable, Disable)",
    "Cli [s]",                  "* Run with CLI (Enable, Disable)",

    "MIdi <s>",                 "* connection type (Jack, Alsa)",
    "AUdio <s>",                "* connection type (Jack, Alsa)",
    "ALsa Midi <s>",            "* name of source",
    "ALsa Audio <s>",           "* name of hardware device",
    "ALsa Sample <n>",          "* rate (0 = 192000, 1 = 96000, 2 = 48000, 3 = 44100)",
    "Jack Midi <s>",            "* name of source",
    "Jack Server <s>",          "* name",
    "Jack Auto <s>",            "* connect jack on start (Enable {other})",

    "ROot [n]",                 "root CC (0 - 119, other disables)",
    "BAnk [n]",                 "bank CC (0, 32, other disables)",
    "PRogram [s]",              "program change (Enable {other})",
    "ACtivate [s]",             "program change activates part (Enable {other})",
    "Extended [s]",             "extended program change (Enable {other})",
    "Quiet [s]",                "ignore 'reset all controllers' (Enable {other})",
    "Nrpn [s]",                 "incoming NRPN (Enable {other})",
    "Log [s]",                  "incoming MIDI CCs (Enable {other})",
    "SHow [s]",                 "GUI MIDI learn editor (Enable {other})",
    "end"
};

string partlist [] = {
    "OFfset <n2>",              "velocity sense offset",
    "Breath <s>",               "breath control (Enable {other})",
    "POrtamento <s>",           "portamento (Enable {other})",
    "Mode <s>",                 "key mode (Poly, Mono, Legato)",
    "Note <n2>",                "note polyphony",
    "SHift <n2>",               "key shift semitones (0 no shift)",
    "EFfects [n2]",             "enter effects context level",
    "  Type <s>",               "the effect type",
    "  PREset <n3>",            "set numbered effect preset to n3",
    "  Send <n3> <n4>",         "send part to system effect n3 at volume n4",
    "KMode <s>",                "set part to kit mode (Enable, {other})",
    "  KItem <n>",              "select kit item number (1-16)",
    "    MUte <s>",             "silence this item (Enable, {other})",
    "    KEffect <n>",          "select effect for this item (0-none, 1-3)",
    "  DRum <s>",               "set kit to drum mode (Enable, {other})",
    "PRogram <[n2]/[s]>",       "loads instrument ID / CLear sets default",
    "NAme <s>",                 "sets the display name the part can be saved with",
    "Channel <n2>",             "MIDI channel (> 32 disables, > 16 note off only)",
    "Destination <s2>",         "jack audio destination (Main, Part, Both)",
    "ADDsynth ...",             "Enter AddSynth context",
    "SUBsynth ...",             "Enter SubSynth context",
    "PADsynth ...",             "Enter PadSynth context",
    "? COMmon",                 "List controls common to most part contexts",
    "end"
};

string commonlist [] = {
    "ENable @",                 "enables the part/kit/engine etc,",
    "DIsable @",                "disables",
    "Volume <n> @",             "volume",
    "Pan <n2> @",               "panning",
    "VElocity <n> @",           "velocity sensing sensitivity",
    "MIn <n> +",                "minimum MIDI note value",
    "MAx <n> +",                "maximum MIDI note value",
    "DEtune Fine <n> *",        "fine frequency",
    "DEtune Coarse <n> *",      "coarse stepped frequency",
    "DEtune Type <n> *",        "type of coarse stepping",
    "OCTave <n> *",             "shift ovatces up or down",
    "STEreo <s> *-voice",       "ENable/ON/YES, {other}",
    " "," ",
    "@",                        "Exists in all part contexts",
    "+",                        "Part and kit mode controls",
    "*",                        "Add, Sub, Pad and AddVoice controls",
    "*-pad",                    "Not PadSynth",
    "*-voice",                  "Not AddVoice",
    "end"
};

string subsynthlist [] = {
    "HArmonic <n1> Amp <n2>", "set harmonic {n1} to {n2} intensity",
    "HArmonic <n1> Band <n2>", "set harmonic {n1} to {n2} width",
    "end"
};

string learnlist [] = {
    "MUte <s>",                 "Enable/Disable this line (Enable, {other})",
    "7Bit",                     "Set incoming NRPNs as 7 bit (Enable, {other})",
    "CC <n2>",                  "Set incoming controller value",
    "CHan <n2>",                "Set incoming channel number",
    "MIn <n2>",                 "Set minimm percentage",
    "MAx <n2>",                 "set maximum percentage",
    "LImit <s>",                "Limit instead of compress (Enable, {other})",
    "BLock <s>",                "Inhibit others on this CC/Chan pair (Enable, {other})",
    "end"
};

string vectlist [] = {
    "[X/Y] CC <n2>",            "CC n2 is used for X or Y axis sweep",
    "[X/Y] Features <n2> [s]",   "sets X or Y features 1-4 (Enable, Reverse, {other} Disable)",
    "[X] PRogram <l/r> <n2>",   "X program change ID for LEFT or RIGHT part",
    "[Y] PRogram <d/u> <n2>",   "Y program change ID for DOWN or UP part",
    "[X/Y] Control <n2> <n3>",  "sets n3 CC to use for X or Y feature n2 (2-4)",
    "Off",                      "disable vector for this channel",
    "Name <s>",                 "Text name for this complete vector",
    "end"
};

string scalelist [] = {
    "FRequency <n>",            "'A' note actual frequency",
    "NOte <n>",                 "'A' note number",
    "Invert [s]",               "Invert entire scale (Enable, {other})",
    "CEnter <n>",               "Note number of key center",
    "SHift <n>",                "Shift entire scale up or down",
    "SCale [s]",                "Activate microtonal scale (Enable, {other})",
    "MApping [s]",              "Activate keyboard mapping (Enable, {other})",
    "FIrst <n>",                "First note number to map",
    "MIddle <n>",               "Middle note number to map",
    "Last <n>",                 "Last note number to map",
    "Tuning <s> [s2]",          "CSV tuning values (n1.n1 or n1/n1 ,  n2.n2 or n2/n2 , etc.)",
    " ",                        "s2 = 'IMPort' from named file",
    "Keymap <s> [s2]",          "CSV keymap (n1, n2, n3, etc.)",
    " ",                        "s2 = 'IMPort' from named file",
    "NAme <s>",                 "Internal name for this scale",
    "DEscription <s>",          "Description of this scale",
    "CLEar",                    "Clear all settings and revert to standard scale",
    "end"
};

string loadlist [] = {
    "Instrument <s>",           "instrument to current part from named file",
    "SCale <s>",                "scale settings from named file",
    "VEctor [n] <s>",           "vector to channel n (or saved) from named file",
    "Patchset <s>",             "complete set of instruments from named file",
    "MLearn <s>",               "midi learned list from named file",
    "STate <s>",                "all system settings and patch sets from named file",
    "end"
};

string savelist [] = {
    "Instrument <s>",           "current part to named file",
    "SCale <s>",                "current scale settings to named file",
    "VEctor <n> <s>",           "vector on channel n to named file",
    "Patchset <s>",             "complete set of instruments to named file",
    "MLearn <s>",               "midi learned list to named file",
    "STate <s>",                "all system settings and patch sets to named file",
    "Config",                   "current configuration",
    "end",
};

string listlist [] = {
    "Roots",                    "all available root paths",
    "Banks [n]",                "banks in root ID or current",
    "Instruments [n]",          "instruments in bank ID or current",
    "Parts",                    "parts with instruments installed",
    "Vectors",                  "settings for all enabled vectors",
    "Tuning",                   "Microtonal scale tunings",
    "Keymap",                   "Microtonal scale keyboard map",
    "Config",                   "current configuration",
    "MLearn [s <n>]",           "midi learned controls ('@' n for full details on one line)",
    "History [s]",              "recent files (Patchsets, SCales, STates, Vectors, MLearn)",
    "Effects [s]",              "effect types ('all' include preset numbers and names)",
    "PREsets",                  "all the presets for the currently selected effect",
    "end"
};

string replies [] = {
    "OK",
    "Done",
    "Value?",
    "Name?",
    "Which Operation?",
    " what?",
    "Out of range",
    "Too low",
    "Too high",
    "Unrecognised",
    "Parameter?",
    "Not at this level",
    "Not available",
    "Unable to complete"
};

string fx_list [] = {
    "OFf",
    "REverb",
    "ECho",
    "CHorus",
    "PHaser",
    "ALienwah",
    "DIstortion",
    "EQ",
    "DYnfilter"
};


string fx_presets [] = {
    "1, off",
    "13, cathedral 1, cathedral 2, cathedral 3, hall 1, hall 2, room 1, room 2, basement, tunnel, echoed 1, echoed 2, very long 1, very long 2",
    "8, echo 1, echo 2, simple echo, canyon, panning echo 1, panning echo 2, panning echo 3, feedback echo",
    "10, chorus 1, chorus 2, chorus 3, celeste 1, celeste 2, flange 1, flange 2, flange 3, flange 4, flange 5",
    "12, phaser 1, phaser 2, phaser 3, phaser 4, phaser 5, phaser 6, aphaser 1, aphaser 2, aphaser 3, aphaser 4, aphaser 5, aphaser 6",
    "4, alienwah 1, alienwah 2, alienwah 3, alienwah 4 ",
    "6, overdrive 1, overdrive 2, exciter 1, exciter 2, guitar amp, quantisize",
    "1, not available",
    "4, wahwah, autowah, vocal morph 1, vocal morph 2"
};


void CmdInterface::defaults()
{
    context = LEVEL::Top;
    chan = 0;
    axis = 0;
    mline = 0;
    npart = 0;
    nFX = 0;
    nFXtype = 0;
    nFXpreset = 0;
    kitmode = 0;
    kitnumber = 0;
    voiceNumber = 0;
}


bool CmdInterface::query(string text, bool priority)
{
    char *line = NULL;
    string suffix;
    char result;
    char test;

    priority = !priority; // so calls make more sense

    if (priority)
    {
        suffix = " N/y? ";
        test = 'n';
    }
    else
    {
        suffix = " Y/n? ";
        test = 'y';
    }
    result = test;
    text += suffix;
    line = readline(text.c_str());
    if (line)
    {
        if (line[0] != 0)
            result = line[0];
        free(line);
        line = NULL;
    }
    return (((result | 32) == test) ^ priority);
}


void CmdInterface::helpLoop(list<string>& msg, string *commands, int indent)
{
    int word = 0;
    int spaces = 30 - indent;
    string left;
    string right;
    string dent;
    string blanks;

    while (commands[word] != "end")
    {
        left = commands[word];
        msg.push_back(dent.assign(indent, ' ') + left + blanks.assign(spaces - left.length(), ' ') + "- " + commands[word + 1]);
        word += 2;
    }
}


bool CmdInterface::helpList(unsigned int local)
{
    if (!matchnMove(1, point, "help") && !matchnMove(1, point, "?"))
        return false;

    int listnum = LISTS::all;

    if (point[0] != 0)
    { // 1 & 2 reserved for syseff & inseff
        if (matchnMove(1, point, "part"))
            listnum = LISTS::part;
        else if (matchnMove(3, point, "common"))
            listnum = LISTS::common;
        else if (matchnMove(3, point, "subsynth"))
            listnum = LISTS::subsynth;
        else if (matchnMove(1, point, "vector"))
            listnum = LISTS::vector;
        else if (matchnMove(1, point, "scale"))
            listnum = LISTS::scale;
        else if (matchnMove(1, point, "load"))
            listnum = LISTS::load;
        else if (matchnMove(1, point, "save"))
            listnum = LISTS::save;
        else if (matchnMove(1, point, "list"))
            listnum = LISTS::list;
        else if (matchnMove(1, point, "config"))
            listnum = LISTS::config;
        else if (matchnMove(1, point, "mlearn"))
            listnum = LISTS::mlearn;
    }
    else
    {
        if (bitTest(local, LEVEL::SubSynth))
            listnum = LISTS::subsynth;
        else if (bitTest(local, LEVEL::Part))
            listnum = LISTS::part;
        else if (bitTest(local, LEVEL::Vector))
            listnum = LISTS::vector;
        else if (bitTest(local, LEVEL::Scale))
            listnum = LISTS::scale;
        else if (bitTest(local, LEVEL::Config))
            listnum = LISTS::config;
        else if (bitTest(local, LEVEL::Learn))
            listnum = LISTS::mlearn;
    }

    list<string>msg;
    msg.push_back("Commands:");
    helpLoop(msg, basics, 2);
    switch(listnum)
    {
        case 0:
            msg.push_back(" ");
            msg.push_back("  Part [n1]   ...             - part operations");
            msg.push_back("  VEctor [n1] ...             - vector operations");
            msg.push_back("  SCale       ...             - scale (microtonal) operations");
            msg.push_back("  MLearn [n1] ...             - MIDI learn operations");
            msg.push_back("  COnfig      ...             - configuration settings");
            msg.push_back("  LIst        ...             - various available parameters");
            msg.push_back("  LOad        ...             - load various files");
            msg.push_back("  SAve        ...             - save various files");

            msg.push_back(" ");
            break;
        case LISTS::part:
            msg.push_back("Part: [n1] = part number");
            helpLoop(msg, partlist, 2);
            break;
        case LISTS::common:
            msg.push_back("Part Common:");
            helpLoop(msg, commonlist, 2);
            break;
        case LISTS::subsynth:
            msg.push_back("Part SubSynth:");
            helpLoop(msg, subsynthlist, 2);
            break;
        case LISTS::vector:
            msg.push_back("Vector: [n1] = base channel:");
            helpLoop(msg, vectlist, 2);
            break;
        case LISTS::scale:
            msg.push_back("Scale:");
            helpLoop(msg, scalelist, 2);
            break;
        case LISTS::load:
            msg.push_back("Load:");
            helpLoop(msg, loadlist, 2);
            break;
        case LISTS::save:
            msg.push_back("Save:");
            helpLoop(msg, savelist, 2);
            break;
        case LISTS::list:
            msg.push_back("List:");
            helpLoop(msg, listlist, 2);
            break;
        case LISTS::config:
            msg.push_back("Config:");
            helpLoop(msg, configlist, 2);
            msg.push_back("'*' entries need to be saved and Yoshimi restarted to activate");
            break;
        case LISTS::mlearn:
            msg.push_back("Mlearn: [n1] = line number");
            helpLoop(msg, learnlist, 2);
            break;
    }

    if (listnum == 0)
    {
        helpLoop(msg, toplist, 2);
        msg.push_back("'...' help sub-menu");
    }

    if (synth->getRuntime().toConsole)
        // we need this in case someone is working headless
        cout << "\nSet CONfig REPorts [s] - set report destination (gui/stderr)\n\n";

    synth->cliOutput(msg, LINES);
    return true;
}


void CmdInterface::historyList(int listnum)
{
    list<string>msg;
    int start = 1;
    int end = 6;
    bool found = false;

    if (listnum != 0)
    {
        start = listnum;
        end = listnum;
    }
    for (int type = start; type <= end; ++type)
    {
        vector<string> listType = *synth->getHistory(type);
        if (listType.size() > 0)
        {
            msg.push_back(" ");
            switch (type)
            {
                case XML_INSTRUMENT:
                    msg.push_back("Recent Instruments:");
                    break;
                case XML_PARAMETERS:
                    msg.push_back("Recent Patch Sets:");
                    break;
                case XML_MICROTONAL:
                    msg.push_back("Recent Scales:");
                    break;
                case XML_STATE:
                    msg.push_back("Recent States:");
                    break;
                case XML_VECTOR:
                    msg.push_back("Recent Vectors:");
                    break;
                case XML_MIDILEARN:
                    msg.push_back("Recent MIDI learned:");
                    break;
            }
            int itemNo = 0;
            for (vector<string>::iterator it = listType.begin(); it != listType.end(); ++it, ++ itemNo)
                msg.push_back(to_string(itemNo + 1) + "  " + *it);
            found = true;
        }
    }
    if (!found)
        msg.push_back("\nNo Saved History");

    synth->cliOutput(msg, LINES);
}


string CmdInterface::historySelect(int listnum, int selection)
{
    vector<string> listType = *synth->getHistory(listnum);
    if (listType.size()== 0)
    {
        synth->getRuntime().Log("No saved entries");
        return "";
    }
    else
    {
        vector<string>::iterator it = listType.begin();
        int itemNo = 0;
        while (it != listType.end() && itemNo != selection)
        {
            ++ it;
            ++ itemNo;
        }
        if (it != listType.end())
            return *it;
    }
    synth->getRuntime().Log("No such entry");
    return "";
}


int CmdInterface::effectsList(bool presets)
{
    list<string>msg;

    size_t presetsPos;
    size_t presetsLast;
    int presetsCount;
    string blanks;
    string left;
    bool all;

    if (bitTest(context, LEVEL::AllFX) && presets == true)
    {
         synth->getRuntime().Log("Type " + fx_list[nFXtype] + "\nPresets -" + fx_presets[nFXtype].substr(fx_presets[nFXtype].find(',') + 1));
         return done_msg;
    }
    else if (presets)
    {
        synth->getRuntime().Log("No effect selected");
        return done_msg;
    }
    else
        all = matchnMove(1, point, "all");
    if (!all)
        msg.push_back("  effect     presets");
    for (int i = 0; i < 9; ++ i)
    {
        presetsPos = 1;
        presetsLast = fx_presets [i].find(',') + 1; // skip over count
        presetsCount = 0;
        if (all)
        {
            msg.push_back("  " + fx_list[i]);
            msg.push_back("    presets");
            while (presetsPos != string::npos)
            {
                presetsPos = fx_presets [i].find(',', presetsLast);
                msg.push_back("      " + asString(presetsCount + 1) + " =" + fx_presets [i].substr(presetsLast, presetsPos - presetsLast));
                presetsLast = presetsPos + 1;
                ++ presetsCount;
            }
        }
        else
        {
            left = fx_list[i];
            msg.push_back("    " + left + blanks.assign(12 - left.length(), ' ') + fx_presets [i].substr(0, presetsLast - 1));
        }
    }

    synth->cliOutput(msg, LINES);
    return done_msg;
}


int CmdInterface::effects(unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    int reply = done_msg;
    int nFXavail;
    int par = nFX;
    int value;
    string dest = "";
    bool flag;

    if (bitTest(context, LEVEL::Part))
    {
        nFXavail = NUM_PART_EFX;
        nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
    }
    else if (bitTest(context, LEVEL::InsFX))
    {
        nFXavail = NUM_INS_EFX;
        nFXtype = synth->insefx[nFX]->geteffect();
    }
    else
    {
        nFXavail = NUM_SYS_EFX;
        nFXtype = synth->sysefx[nFX]->geteffect();
    }

    if (point[0] == 0)
        return done_msg;

    value = string2int(point);
    if (value > 0)
    {
        value -= 1;
        point = skipChars(point);
        if (value >= nFXavail)
            return range_msg;

        if (value != nFX)
        { // calls to update GUI
            nFX = value;
            if (bitTest(context, LEVEL::Part))
            {
                nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
                sendDirect(nFXtype, TOPLEVEL::type::Write, PART::control::effectType, npart, UNUSED, nFX, TOPLEVEL::insert::partEffectSelect);
            }
            else if (bitTest(context, LEVEL::InsFX))
            {
                nFXtype = synth->insefx[nFX]->geteffect();
                sendDirect(nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::insertEffects, UNUSED, nFX);
            }
            else
            {
                nFXtype = synth->sysefx[nFX]->geteffect();
                sendDirect(nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::systemEffects, UNUSED, nFX);
            }
        }
        if (point[0] == 0)
        {
            Runtime.Log("efx number set to " + asString(nFX + 1));
            return done_msg;
        }
    }

    if (matchnMove(1, point, "type"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            Runtime.Log("Current efx type is " + fx_list[nFXtype]);
            return done_msg;
        }
        flag = true;
        for (int i = 0; i < 9; ++ i)
        {
            //Runtime.Log("command " + (string) point + "  list " + fx_list[i]);
            if (matchnMove(2, point, fx_list[i].c_str()))
            {
                nFXtype = i;
                flag = false;
                break;
            }
        }
        if (flag)
            return unrecognised_msg;
        nFXpreset = 0; // always set this on type change
        Runtime.Log("efx type set to " + fx_list[nFXtype]);
        //Runtime.Log("Presets -" + fx_presets[nFXtype].substr(fx_presets[nFXtype].find(',') + 1));
        if (bitTest(context, LEVEL::Part))
            sendDirect(nFXtype, TOPLEVEL::type::Write, PART::control::effectType, npart, UNUSED, nFX);
        else if (bitTest(context, LEVEL::InsFX))
            sendDirect(nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::insertEffects, UNUSED, nFX);
        else
            sendDirect(nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::systemEffects, UNUSED, nFX);
        return done_msg;
    }

    else if (matchnMove(2, point, "send"))
    {
        if (point[0] == 0)
            return parameter_msg;

        if (bitTest(context, LEVEL::InsFX))
        {
            if (matchnMove(1, point, "master"))
            {
                value = -2;
                dest = "master";
            }
            else if (matchnMove(1, point, "off"))
            {
                value = -1;
                dest = "off";
            }
            else
            {
                value = string2int(point) - 1;
                if (value >= Runtime.NumAvailableParts || value < 0)
                    return range_msg;
                dest = "part " + asString(value + 1);
                // done this way in case there is rubbish on the end
            }
        }
        else
        {
            par = string2int(point) - 1;
            point = skipChars(point);
            if (point[0] == 0)
                return value_msg;
            value = string2int127(point);
        }

        int control;
        int partno;
        int engine = nFX;
        int insert = UNUSED;

        if (bitTest(context, LEVEL::Part))
        {
            partno = npart;
            control = 40 + par;
            engine = UNUSED;

            dest = "part " + asString(npart + 1) + " efx sent to system "
                 + asString(par + 1) + " at " + asString(value);
        }
        else if (bitTest(context, LEVEL::InsFX))
        {
            partno = TOPLEVEL::section::insertEffects;
            control = 2;
            dest = "insert efx " + asString(nFX + 1) + " sent to " + dest;
        }
        else
        {
            if (par <= nFX)
                return range_msg;
            partno = TOPLEVEL::section::systemEffects;
            control = par;
            engine = nFX;
            insert = TOPLEVEL::insert::systemEffectSend;
            dest = "system efx " + asString(nFX + 1) + " sent to "
                 + asString(par + 1) + " at " + asString(value);
        }
        sendDirect(value, TOPLEVEL::type::Write,control, partno, UNUSED, engine, insert);
        Runtime.Log(dest);
        return done_msg;
    }

    else if (matchnMove(3, point, "preset"))
    {
        /*
         * Using constant strings and bedding the number into the list
         * of presets provides a very simple way to keep track of a
         * moving target with minimal code and data space.
         * However, all of this should really be in src/Effects
         * not here *and* in the gui code!
         */
        int partno;
        par = string2int(fx_presets [nFXtype].substr(0, fx_presets [nFXtype].find(',')));
        if (par == 1)
            return available_msg;
        value = string2int127(point) - 1;
        if (value >= par || value < 0)
            return range_msg;
        nFXpreset = value;
        if (bitTest(context, LEVEL::Part))
        {
            partno = npart;
            dest = "part " + asString(npart + 1);
        }
        else if (bitTest(context, LEVEL::InsFX))
        {
            partno = TOPLEVEL::section::insertEffects;
            dest = "insert";
        }
        else
        {
            partno = TOPLEVEL::section::systemEffects;
            dest = "system";
        }
        sendDirect(nFXpreset, TOPLEVEL::type::Write,16, partno, nFXtype + EFFECT::type::none, nFX); // TODO shouldn't need this offset
        Runtime.Log(dest + " efx preset set to number " + asString(value + 1));
    }
    return reply;
}


int CmdInterface::partCommonControls(unsigned char controlType)
{
    int cmd = -1;
    int engine = UNUSED;
    int insert = UNUSED;
    int kit = UNUSED;

    if (bitFindHigh(context) == LEVEL::AddSynth)
        engine = 0;
    else if (bitFindHigh(context) == LEVEL::SubSynth)
        engine = 1;
    else if (bitFindHigh(context) == LEVEL::PadSynth)
        engine = 2;
    if (bitFindHigh(context) == LEVEL::AddVoice)
        engine = PART::engine::addVoice1 + voiceNumber;
        // voice numbers are 0 to 7

    if (kitmode)
        kit = kitnumber;

    if (bitFindHigh(context) != LEVEL::Part)
    {
        // these are all common to Add, Sub, Pad, Voice
        int value = 0;

        if (matchnMove(2, point, "detune"))
        {
            if (matchnMove(1, point, "fine"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = ADDSYNTH::control::detuneFrequency;
            }
            else if (matchnMove(1, point, "coarse"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = ADDSYNTH::control::coarseDetune;
            }
            else if (matchnMove(1, point, "type"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = ADDSYNTH::control::detuneType;
            }
        }
        else if (matchnMove(3, point, "octave"))
        {
            if (lineEnd(controlType))
                return value_msg;
            value = string2int(point);
            cmd = ADDSYNTH::control::octave;
        }
        else if (matchnMove(3, point, "stereo") && bitFindHigh(context) != LEVEL::AddVoice)
        {
            cmd = ADDSYNTH::control::stereo;
            value = (toggle() == 1);
        }
        if (cmd > -1)
        {
            sendNormal(value, controlType, cmd, npart, kitnumber, engine);
            return done_msg;
        }
    }

    int value = toggle();
    if (value >= 0)
    {
        if (kit == 0 && bitFindHigh(context) == LEVEL::Part)
        {
            synth->getRuntime().Log("Kit item 1 always enabled.");
            return done_msg;
        }
        else
            cmd = PART::control::enable;
    }
    if (cmd == -1 && bitFindHigh(context) == LEVEL::Part)
    { // the following can only be done at part/kit level
        if (matchnMove(2, point, "min"))
        {
            if(controlType == TOPLEVEL::type::Write)
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                if (value > synth->part[npart]->Pmaxkey)
                    return high_msg;
            }
            cmd = PART::control::minNote;
        }
        else if (matchnMove(2, point, "max"))
        {
            if(controlType == TOPLEVEL::type::Write)
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                if (value < synth->part[npart]->Pminkey)
                    return low_msg;
            }
            cmd = PART::control::maxNote;
        }
    }
    if (cmd != -1)
    {
        if (kitmode)
            insert = TOPLEVEL::insert::kitGroup;

        //cout << ">> kit cmd " << int(cmd) << "  part " << int(npart) << "  kit " << int(kitnumber) << "  engine " << int(engine) << "  insert " << int(insert) << endl;
        sendNormal(value, controlType, cmd, npart, kit, engine, insert);
        return done_msg;
    }

    if (matchnMove(1, point, "volume"))
        cmd = PART::control::volume;
    else if(matchnMove(1, point, "pan"))
        cmd = PART::control::panning;
    else if (matchnMove(2, point, "velocity"))
        cmd = PART::control::velocitySense;

    if (cmd == -1)
        return todo_msg;
    if (lineEnd(controlType))
        return value_msg;

    if (bitFindHigh(context) == LEVEL::Part)
        kit = UNUSED;
    else
        kit = kitnumber;
    //cout << ">> base cmd " << int(cmd) << "  part " << int(npart) << "  kit " << int(kit) << "  engine " << int(engine) << "  insert " << int(insert) << endl;

    sendNormal(string2float(point), controlType, cmd, npart, kit, engine);
    return done_msg;
}


int CmdInterface::commandList()
{
    Config &Runtime = synth->getRuntime();
    int ID;
    int tmp;
    list<string> msg;
    int reply = done_msg;

    if (matchnMove(1, point, "instruments") || matchnMove(2, point, "programs"))
    {
        if (point[0] == 0)
            ID = 128;
        else
            ID = string2int(point);
        synth->ListInstruments(ID, msg);
        synth->cliOutput(msg, LINES);
    }

    else if (matchnMove(1, point, "banks"))
    {
        if (point[0] == 0)
            ID = 128;
        else
            ID = string2int(point);
        synth->ListBanks(ID, msg);
        synth->cliOutput(msg, LINES);
    }

    else if (matchnMove(1, point, "roots"))
    {
        synth->ListPaths(msg);
        synth->cliOutput(msg, LINES);
    }

    else if (matchnMove(1, point, "vectors"))
    {
        synth->ListVectors(msg);
        synth->cliOutput(msg, LINES);
    }

    else if (matchnMove(1, point, "parts"))
    {
        synth->ListCurrentParts(msg);
        synth->cliOutput(msg, LINES);
    }

    else if (matchnMove(1, point, "config"))
    {
        synth->ListSettings(msg);
        synth->cliOutput(msg, LINES);
    }

    else if (matchnMove(2, point, "mlearn"))
    {
        if (point[0] == '@')
        {
            point += 1;
            point = skipSpace(point);
            tmp = string2int(point);
            if (tmp > 0)
                synth->midilearn.listLine(tmp - 1);
            else
                reply = value_msg;
        }
        else
        {
            synth->midilearn.listAll(msg);
            synth->cliOutput(msg, LINES);
        }
    }

    else if (matchnMove(1, point, "tuning"))
        Runtime.Log("Tuning:\n" + synth->microtonal.tuningtotext());
    else if (matchnMove(1, point, "keymap"))
        Runtime.Log("Keymap:\n" + synth->microtonal.keymaptotext());

    else if (matchnMove(1, point, "history"))
    {
        reply = done_msg;
        if (point[0] == 0)
            historyList(0);
        else if (matchnMove(1, point, "instruments") || matchnMove(2, point, "program") )
            historyList(XML_INSTRUMENT);
        else if (matchnMove(1, point, "patchsets"))
            historyList(XML_PARAMETERS);
        else if (matchnMove(2, point, "scales"))
            historyList(XML_MICROTONAL);
        else if (matchnMove(2, point, "states"))
            historyList(XML_STATE);
        else if (matchnMove(1, point, "vectors"))
            historyList(XML_VECTOR);
        else if (matchnMove(2, point, "mlearn"))
            historyList(XML_MIDILEARN);
        else
            reply = todo_msg;
    }

    else if (matchnMove(1, point, "effects") || matchnMove(1, point, "efx"))
        reply = effectsList();
    else if (matchnMove(3, point, "presets"))
        reply = effectsList(true);

    return reply;
}


string CmdInterface::findStatus(bool show)
{
    string text = " ";
    int kit = UNUSED;
    int insert = UNUSED;
    if (bitTest(context, LEVEL::Part))
    {
        text += "part ";
        text += to_string(int(npart) + 1);
        if (readControl(PART::control::enable, npart))
            text += " on";
        kitmode = readControl(PART::control::kitMode, npart);
        if (kitmode != PART::kitType::Off)
        {
            kit = kitnumber;
            insert = TOPLEVEL::insert::kitGroup;
            text += ", kit ";
            text += to_string(kitnumber + 1);
            if (readControl(PART::control::enable, npart, kitnumber, UNUSED, insert))
                text += " on";
            text += ", ";
            switch (kitmode)
            {
                case PART::kitType::Multi:
                    text += "multi";
                    break;
                case PART::kitType::Single:
                    text += "single";
                    break;
                case PART::kitType::CrossFade:
                    text += "cross";
                    break;
                default:
                    break;
            }
        }
        else
            kitnumber = 0;
        if (!show)
            return "";

        if (bitFindHigh(context) == LEVEL::AddSynth)
        {
            text += ", add";
            if (readControl(PART::control::enable, npart, kit, PART::engine::addSynth, insert))
                text += " on";
        }
        else if (bitFindHigh(context) == LEVEL::SubSynth)
        {
            text += ", sub";
                if (readControl(PART::control::enable, npart, kit, PART::engine::subSynth, insert))
                    text += " on";
        }
        else if (bitFindHigh(context) == LEVEL::PadSynth)
        {
            text += ", pad";
            if (readControl(PART::control::enable, npart, kit, PART::engine::padSynth, insert))
                text += " on";
        }
    }
    else if (bitTest(context, LEVEL::Scale))
        text += " Scale ";
    else if (bitTest(context, LEVEL::Config))
        text += " Config ";
    else if (bitTest(context, LEVEL::Vector))
    {
        text += (" Vect Ch " + asString(chan + 1) + " ");
        if (axis == 0)
            text += "X";
        else
            text += "Y";
    }
    else if (bitTest(context, LEVEL::Learn))
        text += (" MLearn line " + asString(mline + 1) + " ");

    return text;
}


int CmdInterface::toggle()
{
    if (matchnMove(2, point, "enable") || matchnMove(2, point, "on") || matchnMove(3, point, "yes"))
        return 1;
    if (matchnMove(2, point, "disable") || matchnMove(3, point, "off") || matchnMove(2, point, "no") )
        return 0;
    return -1;
    /*
     * this allows you to specify enable or other, disable or other or must be those specifics
     */
}


bool CmdInterface::lineEnd(unsigned char controlType)
{
    return (point[0] == 0 && controlType == TOPLEVEL::type::Write);
}


int CmdInterface::commandMlearn(unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    list<string> msg;
    bitSet(context, LEVEL::Learn);

    if (controlType != TOPLEVEL::type::Write)
    {
        Runtime.Log("Write only");
        return done_msg; // will eventually be readable
    }

    if (isdigit(point[0]) || point[0] == '-') // negative should never happen!
    {
        int lineNo = string2int(point);
        point = skipChars(point);
        if (lineNo <= 0)
            return value_msg;
        else
            mline = lineNo -1;
    }
    int tmp = synth->midilearn.findSize();
    if (tmp == 0 || tmp <= mline)
    {
        if (tmp == 0)
            Runtime.Log("No learned lines");
        else
            Runtime.Log("Line " + to_string(mline + 1) + " Not found");
        mline = 0;
        return (done_msg);
    }
    if (point[0] == 0)
        return done_msg;
    {
        unsigned char type = 0;
        unsigned char control = 0;
        unsigned char kit = UNUSED;
        unsigned char engine = UNUSED;
        unsigned char insert = UNUSED;
        unsigned char parameter = UNUSED;

        if (matchnMove(2, point, "cc"))
        {
            if (!isdigit(point[0]))
                return value_msg;
            kit = string2int(point);
            if (kit > 129)
            {
                Runtime.Log("Max CC value is 129");
                return done_msg;
            }
            control =16;
            Runtime.Log("Lines may be re-ordered");
        }
        else if (matchnMove(2, point, "channel"))
        {
            engine = string2int(point) - 1;
            if (engine > 16)
                engine = 16;
            control = 16;
            Runtime.Log("Lines may be re-ordered");;
        }
        else if (matchnMove(2, point, "minimum"))
        {
            insert = int((string2float(point)* 2.0f) + 0.5f);
            if (insert > 200)
                return value_msg;
            control = 5;
        }
        else if (matchnMove(2, point, "maximum"))
        {
            parameter = int((string2float(point)* 2.0f) + 0.5f);
            if (parameter > 200)
                return value_msg;
            control = 6;
        }
        else if (matchnMove(2, point, "mute"))
        {
            type = (toggle() == 1) * 4;
            control = 2;
        }
        else if (matchnMove(2, point, "limit"))
        {
            type = (toggle() == 1) * 2;
            control = 1;
        }
        else if (matchnMove(2, point, "block"))
        {
            type = (toggle() == 1);
            control = 0;
        }
        else if (matchnMove(2, point, "char"))
        {
            type = (toggle() == 1) * 16;
            control = 4;
        }
        sendNormal(mline, type, control, TOPLEVEL::section::midiLearn, kit, engine, insert, parameter);
        return done_msg;
    }
    return opp_msg;
}


int CmdInterface::commandVector(unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    list<string> msg;
    int reply = todo_msg;
    int tmp;
    bitSet(context, LEVEL::Vector);
    if (controlType != TOPLEVEL::type::Write)
    {
        if (synth->SingleVector(msg, chan))
            synth->cliOutput(msg, LINES);
        else
            Runtime.Log("No vector on channel " + asString(chan + 1));
        return done_msg;
    }
    if (point[0] == 0)
    {
        if (!Runtime.vectordata.Enabled[chan])
            Runtime.Log("No vector on channel " + asString(chan + 1));
        return done_msg;
    }

    unsigned char ch = string2int127(point);
    if (ch > 0)
    {
        ch -= 1;
        if (ch >= NUM_MIDI_CHANNELS)
            return range_msg;
        point = skipChars(point);
        if (chan != ch)
        {
            chan = ch;
            axis = 0;
        }

        Runtime.Log("Vector channel set to " + asString(chan + 1));
    }

    if (matchWord(1, point, "off"))
    {
        sendDirect(0,controlType,VECTOR::control::erase, TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
        axis = 0;
        bitClear(context, LEVEL::Vector);
        return done_msg;
    }
    if (matchnMove(1, point, "xaxis"))
        axis = 0;
    else if (matchnMove(1, point, "yaxis"))
    {
        if (!Runtime.vectordata.Enabled[chan])
        {
            Runtime.Log("Vector X must be set first");
            return done_msg;
        }
        axis = 1;
    }

    if (point[0] == 0)
        return done_msg;

    if (matchnMove(2, point, "cc"))
    {
        if (point[0] == 0)
            return value_msg;

        tmp = string2int(point);
        if (axis == 0)
        {
            sendDirect(tmp, controlType, VECTOR::control::Xcontroller, TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
            bitSet(context, LEVEL::Vector);
            return done_msg;
        }
        if (Runtime.vectordata.Enabled[chan])
        {
            sendDirect(tmp, controlType, VECTOR::control::Ycontroller, TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
            return done_msg;
        }
    }

    if (!Runtime.vectordata.Enabled[chan])
    {
        Runtime.Log("Vector X CC must be set first");
        return done_msg;
    }

    if (axis == 1 && (Runtime.vectordata.Yaxis[chan] > 0x7f))
    {
        Runtime.Log("Vector Y CC must be set first");
        return done_msg;
    }

    if (matchnMove(1, point, "name"))
    {
        string name = "!";
        if (controlType == TOPLEVEL::type::Write)
        {
            name = string(point);
            if (name <= "!")
                return value_msg;
        }
        sendDirect(0, controlType, VECTOR::control::name, TOPLEVEL::section::vector, UNUSED, UNUSED, chan, TOPLEVEL::route::lowPriority, miscMsgPush(name));
        return done_msg;
    }

    if (matchnMove(1, point, "features"))
    {
        if (point[0] == 0)
            return value_msg;
        int feat = string2int(point) - 1;
        if (feat < 0 || feat > 3)
            return range_msg;
        point = skipChars(point);
        int enable = 0;
        if (toggle() == 1)
            enable = 1;
        else if (feat > 1 && matchnMove(1, point, "reverse"))
            enable = 2;
        sendDirect(enable, controlType, VECTOR::control::Xfeature0 + (axis * (VECTOR::control::Ycontroller - VECTOR::control::Xcontroller)) + feat , TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
        return done_msg;
    }

    if (matchnMove(2, point, "program") || matchnMove(1, point, "instrument"))
    {
        int hand = point[0] | 32;
        point = skipChars(point); // in case they type the entire word
        if ((axis == 0 && (hand == 'd' || hand == 'u')) || (axis == 1 && (hand == 'l' || hand == 'r')))
        {
            Runtime.Log("Bad direction for this axis");
            return done_msg;
        }
        if (hand == 'l' || hand == 'd')
            hand = 0;
        else if (hand == 'r' || hand == 'u')
            hand = 1;
        else
            return opp_msg;
        tmp = string2int(point);
        sendDirect(tmp, controlType, VECTOR::control::XleftInstrument + hand + (axis * (VECTOR::control::Ycontroller - VECTOR::control::Xcontroller)), TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
        return done_msg;
    }

    // this disabled for now - needs a lot of work.
    /*if (!matchnMove(1, point, "control"))
        return opp_msg;
    if(isdigit(point[0]))
    {
        int cmd = string2int(point);
        if (cmd < 2 || cmd > 4)
            return range_msg;
        point = skipChars(point);
        if (point[0] == 0)
            return value_msg;
        tmp = string2int(point);
        if (!synth->vectorInit(axis * 3 + cmd + 6, chan, tmp))
        {
            synth->vectorSet(axis * 3 + cmd + 6, chan, tmp);
            reply = done_msg;
        }
        else
            reply = value_msg;
    }*/

    return reply;
}


int CmdInterface::commandConfig(unsigned char controlType)
{
    /*if (point[0] == 0)
    {
        if (controlType != TOPLEVEL::type::Write)
            sendDirect(0, 0, 80, TOPLEVEL::section::main); // status
        return done_msg;
    }*/
    float value = 0;
    unsigned char command = UNUSED;
    unsigned char par = UNUSED;
    unsigned char par2 = UNUSED;

    if (matchnMove(1, point, "oscillator"))
    {
        command = CONFIG::control::oscillatorSize;
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(2, point, "buffer"))
    {
        command = CONFIG::control::bufferSize;
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(2, point, "padsynth"))
    {
        command = CONFIG::control::padSynthInterpolation;
        value = !matchnMove(1, point, "linear");
    }
    else if (matchnMove(1, point, "virtual"))
    {
        command = CONFIG::control::virtualKeyboardLayout;
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(1, point, "xml"))
    {
        command = CONFIG::control::XMLcompressionLevel;
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(2, point, "reports"))
    {
        command = CONFIG::control::reportsDestination;
        value = !matchnMove(1, point, "stdout");
    }
    else if (matchnMove(2, point, "saved"))
    {
        command = CONFIG::control::savedInstrumentFormat;
        if (matchnMove(1, point, "legacy"))
            value = 1;
        else if (matchnMove(1, point, "yoshimi"))
            value = 2;
        else if (matchnMove(1, point, "both"))
            value = 3;
        else if (controlType == TOPLEVEL::type::Write)
            return value_msg;
    }

    else if (matchnMove(2, point, "state"))
    {
        command = CONFIG::control::defaultStateStart;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "hide"))
    {
        command = CONFIG::control::hideNonFatalErrors;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "display"))
    {
        command = CONFIG::control::showSplash;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "time"))
    {
        command = CONFIG::control::logInstrumentLoadTimes;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "include"))
    {
        command = CONFIG::control::logXMLheaders;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "keep"))
    {
        command = CONFIG::control::saveAllXMLdata;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "gui"))
    {
        command = CONFIG::control::enableGUI;
        if (matchnMove(1, point, "enable"))
            value = 1;
        else if (matchnMove(1, point, "disable"))
            value = 0;
        else
            return value_msg;
    }
    else if (matchnMove(1, point, "cli"))
    {
        command = CONFIG::control::enableCLI;
        if (matchnMove(1, point, "enable"))
            value = 1;
        else if (matchnMove(1, point, "disable"))
            value = 0;
        else
            return value_msg;
    }

    else if (matchnMove(1, point, "jack"))
    {
        if (matchnMove(1, point, "midi"))
        {
            command = CONFIG::control::jackMidiSource;
            par = TOPLEVEL::route::lowPriority;
            if (controlType != TOPLEVEL::type::Write || point[0] != 0)
            {
                if (controlType == TOPLEVEL::type::Write)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "server"))
        {
            command = CONFIG::control::jackServer;
            par = TOPLEVEL::route::lowPriority;
            if (controlType != TOPLEVEL::type::Write || point[0] != 0)
            {
                if (controlType == TOPLEVEL::type::Write)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "auto"))
        {
            command = CONFIG::control::jackAutoConnectAudio;
            value = (toggle() == 1);
        }
        else
            return opp_msg;
    }

    else if (matchnMove(2, point, "alsa"))
    {
        if (matchnMove(1, point, "midi"))
        {
            command = CONFIG::control::alsaMidiSource;
            par = TOPLEVEL::route::lowPriority;
            if (controlType != TOPLEVEL::type::Write || point[0] != 0)
            {
                if (controlType == TOPLEVEL::type::Write)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "audio"))
        {
            command = CONFIG::control::alsaAudioDevice;
            par = TOPLEVEL::route::lowPriority;
            if (controlType != TOPLEVEL::type::Write || point[0] != 0)
            {
                if (controlType == TOPLEVEL::type::Write)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "s"))
        {
            command = CONFIG::control::alsaSampleRate;
            if (controlType == TOPLEVEL::type::Write)
            {
                if (point[0] == 0)
                    return value_msg;
                value = string2int(point);
                if (value < 0 || value > 3)
                    return range_msg;
            }
        }
        else
            return opp_msg;
    }

    else if (matchnMove(2, point, "midi"))
    {
        value = 1;
        if (matchnMove(1, point, "alsa"))
            command = CONFIG::control::alsaPreferredMidi;
        else if (controlType != TOPLEVEL::type::Write || matchnMove(1, point, "jack"))
            command = CONFIG::control::jackPreferredMidi;
        else
            return value_msg;
    }

    else if (matchnMove(2, point, "audio"))
    {
        value = 1;
        if (matchnMove(1, point, "alsa"))
            command = CONFIG::control::alsaPreferredAudio;
        else if (controlType != TOPLEVEL::type::Write || matchnMove(1, point, "jack"))
            command = CONFIG::control::jackPreferredAudio;
        else
            return value_msg;
    }

    else if (matchnMove(2, point, "root"))
    {
        command = CONFIG::control::bankRootCC;
        if (controlType != TOPLEVEL::type::Write)
            value = 128; // ignored by range check
        else if (point[0] == 0)
            return value_msg;
        else
            value = string2int(point);
    }
    else if (matchnMove(2, point, "bank"))
    {
        command = CONFIG::control::bankCC;
        if (controlType != TOPLEVEL::type::Write)
            value = 128; // ignored by range check
        else if (point[0] == 0)
            return value_msg;
        else
            value = string2int(point);
    }
    else if (matchnMove(2, point, "program") || matchnMove(2, point, "instrument"))
    {
        command = CONFIG::control::enableProgramChange;
        value = (toggle() == 1);
    }
    else if (matchnMove(2, point, "activate"))
    {
        command = CONFIG::control::programChangeEnablesPart;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "extend"))
    {
        command = CONFIG::control::extendedProgramChangeCC;
        if (controlType != TOPLEVEL::type::Write)
            value = 128; // ignored by range check
        else if (point[0] == 0)
            return value_msg;
        else
            value = string2int(point);
    }
    else if (matchnMove(1, point, "Quiet"))
    {
        command = CONFIG::control::ignoreResetAllCCs;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "log"))
    {
        command = CONFIG::control::logIncomingCCs;
        value = (toggle() == 1);
    }
    else if (matchnMove(2, point, "show"))
    {
        command = CONFIG::control::showLearnEditor;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "nrpn"))
    {
        command = CONFIG::control::enableNRPNs;
        value = (toggle() == 1);
    }

    else
        return todo_msg; // may be picked up later

    sendDirect(value, controlType, command, TOPLEVEL::section::config, UNUSED, UNUSED, UNUSED, par, par2);
    return done_msg;
}


int CmdInterface::commandScale(unsigned char controlType)
{
    if (point[0] == 0)
        return done_msg;
    Config &Runtime = synth->getRuntime();
    int reply = done_msg;
    float value = 0;
    unsigned char command = UNUSED;
    unsigned char par = UNUSED;
    unsigned char par2 = UNUSED;
    if (controlType != TOPLEVEL::type::Write)
        reply = done_msg;

    string name;

    if (matchnMove(1, point, "tuning"))
        command = SCALES::control::tuning;
    else if (matchnMove(1, point, "keymap"))
        command = SCALES::control::keyboardMap;
    else if (matchnMove(2, point, "name"))
        command = SCALES::control::name;
    else if (matchnMove(2, point, "description"))
        command = SCALES::control::comment;

    if (command >= SCALES::control::tuning && command <= SCALES::control::comment)
    {
        if (controlType != TOPLEVEL::type::Write && command <= SCALES::control::importKbm)
        {
            Runtime.Log("Write only - use list");
            return done_msg;
        }
        if (command <= SCALES::control::keyboardMap)
        {
            if (matchnMove(3, point, "import"))
                command += (SCALES::control::importKbm - SCALES::control::keyboardMap);
        }
        name = (string)point;
        if (name == "")
            return value_msg;
        par = TOPLEVEL::route::lowPriority;
        par2 = miscMsgPush(name);
    }
    else
    {
        int min = 0;
        int max = 127;
        if (matchnMove(2, point, "frequency"))
        {
            command = SCALES::control::Afrequency;
            min = 1;
            max = 20000;
            controlType &= ~TOPLEVEL::type::Integer; // float
        }
        else if(matchnMove(2, point, "note"))
            command = SCALES::control::Anote;
        else if(matchnMove(1, point, "invert"))
        {
            command = SCALES::control::invertScale;
            max = 1;
        }
        else if(matchnMove(2, point, "center"))
            command = SCALES::control::invertedScaleCenter;
        else if(matchnMove(2, point, "shift"))
        {
            command = SCALES::control::scaleShift;
            min = -63;
            max = 64;
        }
        else if(matchnMove(2, point, "scale"))
        {
            command = SCALES::control::enableMicrotonal;
            max = 1;
        }
        else if(matchnMove(2, point, "mapping"))
        {
            command = SCALES::control::enableKeyboardMap;
            max = 1;
        }
        else if(matchnMove(2, point, "first"))
            command = SCALES::control::lowKey;
        else if(matchnMove(2, point, "middle"))
            command = SCALES::control::middleKey;
        else if(matchnMove(1, point, "last"))
            command = SCALES::control::highKey;
        else if(matchnMove(3, point, "CLEar"))
        {
            point -=1; // sneaky way to force a zero :)
            command = SCALES::control::clearAll;
        }
        else
            return todo_msg;

        if (controlType == TOPLEVEL::type::Write)
        {
            if (point[0] == 0)
                return value_msg;
            if ((toggle() == 1))
                value = 1;
            else//if (isdigit(point[0]))
            {
                value = string2float(point);
                if (value < min || value > max)
                    return value_msg;
            }
        }
    }
//cout << "par " << int(par) << endl;
    sendDirect(value, controlType, command, TOPLEVEL::section::scales, UNUSED, UNUSED, UNUSED, par, par2);
    return reply;
}


int CmdInterface::addVoice(unsigned char controlType)
{
    cout << "addvoice" << endl;
    if (point[0] == 0)
        return done_msg;
    int result = partCommonControls(controlType);
    if (result != todo_msg)
        return result;
    return available_msg;
}


int CmdInterface::addSynth(unsigned char controlType)
{

    if (matchnMove(1, point, "voice"))
    {
        bitSet(context, LEVEL::AddVoice);
        return addVoice(controlType);
    }
    cout << "addsynth" << endl;
    if (point[0] == 0)
        return done_msg;
    int result = partCommonControls(controlType);
    if (result != todo_msg)
        return result;
    return available_msg;
}


int CmdInterface::subSynth(unsigned char controlType)
{
    float value;
    int control = -1;
    unsigned char insert = UNUSED;
    bool set = false;

    cout << "subsynth" << endl;
    if (point[0] == 0)
        return done_msg;
    int result = partCommonControls(controlType);
    if (result != todo_msg)
        return result;
    if (matchnMove(2, point, "harmonic"))
    {
        if (lineEnd(controlType))
            return parameter_msg;
        control = string2int(point) - 1;
        point = skipChars(point);
        if (matchnMove(1, point, "amplitude"))
            insert = TOPLEVEL::insert::harmonicAmplitude;
        else if (matchnMove(1, point, "bandwidth"))
            insert = TOPLEVEL::insert::harmonicPhaseBandwidth;
        if (lineEnd(controlType))
            return value_msg;
        value = string2int(point);
        set = true;
    }

    if (set)
    {

        //cout << "control " << int(control) << "  part " << int(npart) << "  kit " << int(kitnumber) << "  engine " << int(PART::engine::subSynth) << "  insert " << int(insert) << endl;

        return sendNormal(value, controlType, control, npart, kitnumber, PART::engine::subSynth, insert);
    }
    return available_msg;
}


int CmdInterface::padSynth(unsigned char controlType)
{
    cout << "padsynth" << endl;
    if (point[0] == 0)
        return done_msg;
    int result = partCommonControls(controlType);
    if (result != todo_msg)
        return result;
    return available_msg;
}


int CmdInterface::commandPart(bool justSet, unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    int reply = todo_msg;
    int tmp;

    if (point[0] == 0)
        return done_msg;
    if (justSet || isdigit(point[0]))
    {
        tmp = string2int127(point);
        if (tmp > 0)
        {
            tmp -= 1;
            if (tmp >= Runtime.NumAvailableParts)
            {
                Runtime.Log("Part number too high");
                return done_msg;
            }
            point = skipChars(point);
            if (npart != tmp)
            {
                npart = tmp;
                if (controlType == TOPLEVEL::type::Write)
                {
                    context = LEVEL::Top;
                    bitSet(context, LEVEL::Part);
                    kitmode = PART::kitType::Off;
                    kitnumber = 0;
                    sendDirect(npart, TOPLEVEL::type::Write, MAIN::control::partNumber, TOPLEVEL::section::main);
                }
            }
            if (point[0] == 0)
                return done_msg;
        }
    }

    if (bitTest(context, LEVEL::AllFX))
        return effects(controlType);

    if (matchnMove(3, point, "addsynth"))
    {
        bitSet(context, LEVEL::AddSynth);
        return addSynth(controlType);
    }

    if (matchnMove(3, point, "subsynth"))
    {
        bitSet(context, LEVEL::SubSynth);
        return subSynth(controlType);
    }

    if (matchnMove(3, point, "padsynth"))
    {
        bitSet(context, LEVEL::PadSynth);
        return padSynth(controlType);
    }

    tmp = partCommonControls(controlType);
    if (tmp != todo_msg)
        return tmp;

    if (matchnMove(2, point, "effects") || matchnMove(2, point, "efx"))
    {
        context = LEVEL::Top;
        bitSet(context, LEVEL::AllFX);
        bitSet(context, LEVEL::Part);
        return effects(controlType);
    }

    if (matchnMove(2, point, "kmode"))
    {
        if (matchnMove(2, point, "off"))
            kitmode = PART::kitType::Off;
        else if(matchnMove(2, point, "multi"))
            kitmode = PART::kitType::Multi;
        else if(matchnMove(2, point, "single"))
            kitmode = PART::kitType::Single;
        else if(matchnMove(2, point, "crossfade"))
            kitmode = PART::kitType::CrossFade;
        else
            return value_msg;
        sendDirect(kitmode, controlType, PART::control::kitMode, npart);
        kitnumber = 0;
        return done_msg;
    }
    if (kitmode == PART::kitType::Off)
        kitnumber = UNUSED; // always clear it if not kit mode
    else
    {
        if (matchnMove(2, point, "kitem"))
        {
            if (controlType == TOPLEVEL::type::Write)
            {
                if (point[0] == 0)
                    return value_msg;
                int tmp = string2int(point);
                if (tmp < 1 || tmp > NUM_KIT_ITEMS)
                    return range_msg;
                kitnumber = tmp - 1;
            }
            Runtime.Log("Kit item number " + to_string(kitnumber + 1));
            return done_msg;
        }
    }
    if (kitmode)
    {
        int value;
        if (matchnMove(2, point, "drum"))
        {
            value = toggle();
            sendDirect((value == 1), controlType, PART::control::drumMode, npart);
            return done_msg;
        }
        if (matchnMove(2, point, "mute"))
        {
            value = toggle();
            sendDirect((value == 1), controlType, PART::control::kitItemMute, npart, kitnumber, UNUSED, TOPLEVEL::insert::kitGroup);
            return done_msg;
        }
        if (matchnMove(2, point,"keffect"))
        {
            if (controlType == TOPLEVEL::type::Write && point[0] == 0)
                return value_msg;
            value = string2int(point);
            if (value < 0 || value > 3)
                return range_msg;
            sendDirect(value, controlType, PART::control::kitEffectNum, npart, kitnumber, UNUSED, TOPLEVEL::insert::kitGroup);
            return done_msg;
        }
    }

    if (matchnMove(2, point, "shift"))
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        int value = string2int(point);
        if (value < MIN_KEY_SHIFT)
            value = MIN_KEY_SHIFT;
        else if(value > MAX_KEY_SHIFT)
            value = MAX_KEY_SHIFT;
        sendDirect(value, controlType, PART::control::keyShift, npart, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
        return done_msg;
    }

    if (matchnMove(2, point, "offset"))
    {
        tmp = string2int127(point);
        if(controlType == TOPLEVEL::type::Write && tmp < 1)
            return value_msg;
        sendDirect(tmp, controlType, PART::control::velocityOffset, npart);
    }

    if (matchnMove(2, point, "program") || matchnMove(1, point, "instrument"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            Runtime.Log("Part name is " + synth->part[npart]->Pname);
            return done_msg;
        }
        if (matchnMove(2, point, "clear"))
        {
            sendDirect(0, controlType, PART::control::defaultInstrument, npart, UNUSED, UNUSED, UNUSED, UNUSED, tmp);
            return done_msg;
        }
        if (point[0] != 0) // force part not channel number
        {
            tmp = string2int(point) - 1;
            if (tmp < 0 || tmp > 159)
                return range_msg;
            sendDirect(npart, controlType, MAIN::control::loadInstrument, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, tmp);
            reply = done_msg;
        }
        else
            reply = value_msg;
    }
    else if (matchnMove(1, point, "channel"))
    {
        tmp = string2int127(point);
        if(controlType == TOPLEVEL::type::Write && tmp < 1)
            return value_msg;
        tmp -= 1;
        sendDirect(tmp, controlType, PART::control::midiChannel, npart);
        reply = done_msg;
    }
    else if (matchnMove(1, point, "destination"))
    {
        int dest = 0;
        if (controlType == TOPLEVEL::type::Write)
        {
            if (matchnMove(1, point, "main"))
                dest = 1;
            else if (matchnMove(1, point, "part"))
                dest = 2;
            else if (matchnMove(1, point, "both"))
                dest = 3;
            if (dest == 0)
                reply = range_msg;
        }
        sendDirect(dest, controlType, PART::control::audioDestination, npart, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::adjustAndLoopback);
        reply = done_msg;
    }
    else if (matchnMove(1, point, "breath"))
    {
        sendDirect((toggle() == 1), controlType, PART::control::breathControlEnable, npart);
        return done_msg;
    }
    else if (matchnMove(1, point, "note"))
    {
        int value = 0;
        if(controlType == TOPLEVEL::type::Write)
        {
            if (point[0] == 0)
                return value_msg;
            value = string2int(point);
            if (value < 1 || (value > POLIPHONY - 20))
                return range_msg;
        }
        sendDirect(value, controlType, 33, npart);
        return done_msg;
    }

    else if (matchnMove(1, point, "mode"))
    {
        int value = 0;
        if(controlType == TOPLEVEL::type::Write)
        {
            if (matchnMove(1, point, "poly"))
                value = 0;
            else if (matchnMove(1, point, "mono"))
                value = 1;
            else if (matchnMove(1, point, "legato"))
                value = 2;
            else
                return name_msg;
        }
        sendDirect(value, controlType, 6, npart);
        return done_msg;
    }
    else if (matchnMove(2, point, "portamento"))
    {
        sendDirect((toggle() == 1), controlType, PART::control::portamento, npart);
        return done_msg;
    }
    else if (matchnMove(2, point, "name"))
    {
        string name;
        unsigned char par2 = NO_MSG;
        if (controlType == TOPLEVEL::type::Write)
        {
            name = (string) point;
            if (name.size() < 3)
            {
                Runtime.Log("Name too short");
                return done_msg;
            }
            else if ( name == "Simple Sound")
            {
                Runtime.Log("Cant use name of default sound");
                return done_msg;
            }
            else
                par2 = miscMsgPush(name);
        }
        sendDirect(0, controlType, PART::control::instrumentName, npart, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, par2, UNUSED);
        reply = done_msg;
    }
    else
        reply = opp_msg;
    return reply;
}


int CmdInterface::commandReadnSet(unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    int reply = todo_msg;
    string name;

    if (matchnMove(2, point, "yoshimi"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            //Runtime.Log("Instance " + asString(currentInstance), 1);
            Runtime.Log("Instance " + to_string(synth->getUniqueId()));
            return done_msg;
        }
        if (point[0] == 0)
            return value_msg;
        currentInstance = string2int(point);
        synth = firstSynth->getSynthFromId(currentInstance);
        unsigned int newID = synth->getUniqueId();
        if (newID != currentInstance)
        {
            Runtime.Log("Instance " + to_string(currentInstance) + " not found. Set to " + to_string(newID), 1);
            currentInstance = newID;
        }
        defaults();
        return done_msg;
    }

 // these must all be highest (relevant) bit first
    if (bitTest(context, LEVEL::Config))
        reply = commandConfig(controlType);
    else if (bitTest(context, LEVEL::Scale))
        reply = commandScale(controlType);
    else if (bitTest(context, LEVEL::AddVoice))
        reply = addVoice(controlType);
    else if (bitTest(context, LEVEL::AddSynth))
        reply = addSynth(controlType);
    else if (bitTest(context, LEVEL::SubSynth))
        reply = subSynth(controlType);
    else if (bitTest(context, LEVEL::PadSynth))
        reply = padSynth(controlType);
    else if (bitTest(context, LEVEL::Part))
        reply = commandPart(false, controlType);
    else if (bitTest(context, LEVEL::Vector))
        reply = commandVector(controlType);
    else if (bitTest(context, LEVEL::Learn))
        reply = commandMlearn(controlType);
    if (reply > todo_msg)
        return reply;

    if (matchnMove(2, point, "config"))
    {
        context = LEVEL::Top;
        bitSet(context, LEVEL::Config);
        return commandConfig(controlType);
    }

    if (matchnMove(1, point, "scale"))
    {
        context = LEVEL::Top;
        bitSet(context, LEVEL::Scale);
        return commandScale(controlType);
    }

    if (matchnMove(1, point, "part"))
    {
        nFX = 0; // effects number limit changed
        if (controlType != TOPLEVEL::type::Write && point[0] == 0)
        {
            if (synth->partonoffRead(npart))
                name = " enabled";
            else
                name = " disabled";
            Runtime.Log("Current part " + asString(npart) + name, 1);
            return done_msg;
        }
        context = LEVEL::Top;
        bitSet(context, LEVEL::Part);
        nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
        return commandPart(true, controlType);
    }

    if (matchnMove(2, point, "vector"))
    {
        context = LEVEL::Top;
        return commandVector(controlType);
    }

    if (matchnMove(2, point, "mlearn"))
    {
        context = LEVEL::Top;
        return commandMlearn(controlType);
    }

    if ((context == LEVEL::Top || bitTest(context, LEVEL::InsFX)) && matchnMove(3, point, "system"))
    {
        bitSet(context,LEVEL::AllFX);
        bitClear(context, LEVEL::InsFX);
        nFX = 0; // effects number limit changed
        matchnMove(2, point, "effects"); // clear it if given
        matchnMove(2, point, "efx");
        nFXtype = synth->sysefx[nFX]->geteffect();
        return effects(controlType);
    }
    if ((context == LEVEL::Top || bitTest(context, LEVEL::AllFX)) && !bitTest(context, LEVEL::Part) && matchnMove(3, point, "insert"))
    {
        bitSet(context,LEVEL::AllFX);
        bitSet(context,LEVEL::InsFX);
        nFX = 0; // effects number limit changed
        matchnMove(2, point, "effects"); // clear it if given
        matchnMove(2, point, "efx");
        nFXtype = synth->insefx[nFX]->geteffect();
        return effects(controlType);
    }
    if (bitTest(context, LEVEL::AllFX))
        return effects(controlType);

    if (matchnMove(1, point, "root"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            Runtime.Log("Root is ID " + asString(synth->ReadBankRoot()), 1);
            return done_msg;
        }
        if (point[0] != 0)
        {
            sendDirect(255, controlType, 8, TOPLEVEL::section::midiIn, 0, UNUSED, string2int(point), TOPLEVEL::route::adjustAndLoopback);
            return done_msg;
        }
        else
            return value_msg;
    }

    if (matchnMove(1, point, "bank"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            Runtime.Log("Bank is ID " + asString(synth->ReadBank()), 1);
            return done_msg;
        }
        if (point[0] != 0)
        {
            sendDirect(255, TOPLEVEL::type::Write, 8, TOPLEVEL::section::midiIn, 0, string2int(point), UNUSED, TOPLEVEL::route::adjustAndLoopback);
            return done_msg;
        }
        else
            return value_msg;
    }

    if (matchnMove(1, point, "volume"))
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        sendDirect(string2int127(point), controlType, MAIN::control::volume, TOPLEVEL::section::main);
        return done_msg;
    }
    if (matchnMove(2, point, "detune"))
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        sendDirect(string2int127(point), controlType, MAIN::control::detune, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
        return done_msg;
    }

    if (matchnMove(2, point, "shift"))
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        int value = string2int(point);
        if (value < MIN_KEY_SHIFT)
            value = MIN_KEY_SHIFT;
        else if(value > MAX_KEY_SHIFT)
            value = MAX_KEY_SHIFT;
        sendDirect(value, controlType, MAIN::control::keyShift, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
        return done_msg;
    }

    if (matchnMove(2, point, "solo"))
    {
        int value = 0; // disable

        if (matchnMove(2, point, "cc"))
        {
            if (controlType == TOPLEVEL::type::Write)
            {
                if (point[0] == 0)
                    return value_msg;
                value = string2int127(point);
                string reply = Runtime.masterCCtest(value);
                if (reply > "")
                {
                    Runtime.Log("In use for " + reply);
                    return done_msg;
                }
            }
            sendDirect(value, controlType, 49, TOPLEVEL::section::main);
            return done_msg;
        }

        else if (matchnMove(1, point, "row"))
            value = 1;
        else if (matchnMove(1, point, "column"))
            value = 2;
        else if (matchnMove(1, point, "loop"))
            value = 3;
        else if (matchnMove(1, point, "twoway"))
            value = 4;
        sendDirect(value, controlType, 48, TOPLEVEL::section::main);
        return done_msg;
    }
    else if (matchnMove(2, point, "available")) // 16, 32, 64
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        int value = string2int(point);
        if (value != 16 && value != 32 && value != 64)
            return range_msg;
        sendDirect(value, controlType, 15, TOPLEVEL::section::main);
        return done_msg;
    }

    else
        reply = opp_msg;
    return reply;
}


bool CmdInterface::cmdIfaceProcessCommand()
{
    // in case it's been changed from elsewhere
    synth = firstSynth->getSynthFromId(currentInstance);
    unsigned int newID = synth->getUniqueId();
    if (newID != currentInstance)
    {
        currentInstance = newID;
        defaults();
    }

    Config &Runtime = synth->getRuntime();

    replyString = "";
    int reply = todo_msg;
    int tmp;
    point = cCmd;
    point = skipSpace(point); // just to be sure
    tmp = strlen(cCmd) - 1;
    while (point[tmp] < '!' && tmp > 0)
    {
        point[tmp] = 0; // also trailing spaces
        -- tmp;
    }

    list<string> msg;

    findStatus(false);

#ifdef REPORT_NOTES_ON_OFF
    if (matchnMove(3, point, "report")) // note test
    {
        cout << "note on sent " << Runtime.noteOnSent << endl;
        cout << "note on seen " << Runtime.noteOnSeen << endl;
        cout << "note off sent " << Runtime.noteOffSent << endl;
        cout << "note off seen " << Runtime.noteOffSeen << endl;
        cout << "notes hanging sent " << Runtime.noteOnSent - Runtime.noteOffSent << endl;
        cout << "notes hanging seen " << Runtime.noteOnSeen - Runtime.noteOffSeen << endl;
        return false;
    }
#endif
    if (matchnMove(2, point, "exit"))
    {
        if (currentInstance > 0)
        {
            Runtime.Log("Can only exit from instance 0", 1);
            return false;
        }
        if (Runtime.configChanged)
            replyString = "System config has been changed. Still exit";
        else
            replyString = "All data will be lost. Still exit";
        if (query(replyString, false))
        {
            // this seems backwards but it *always* saves.
            // seeing configChanged makes it reload the old config first.
            Runtime.runSynth = false;
            return true;
        }
        return false;
    }
    if (point[0] == '/')
    {
        ++ point;
        point = skipSpace(point);
        context = LEVEL::Top;
        if (point[0] == 0)
            return false;
    }

    if (matchnMove(3, point, "reset"))
    {
        int control = MAIN::control::masterReset;
        if (matchnMove(3, point, "all"))
            control = MAIN::control::masterResetAndMlearn;
        if (query("Restore to basic settings", false))
            sendDirect(0, TOPLEVEL::type::Write, control, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::adjustAndLoopback);
        return false;
    }

    else if (point[0] == '.' && point[1] == '.')
    {
        point += 2;
        point = skipSpace(point);

        if (bitFindHigh(context) == LEVEL::AllFX || bitFindHigh(context) == LEVEL::InsFX)
            context = LEVEL::Top;
        else if (bitFindHigh(context) == LEVEL::Part && (bitTest(context, LEVEL::AllFX) || bitTest(context, LEVEL::InsFX)))
        {
            context = LEVEL::Top;
            bitSet(context, LEVEL::Part); // restore part level
        }
        else
        {
            bitClearHigh(context);
        }
        if (point[0] == 0)
            return false;
    }

    if (helpList(context))
        return false;
    if (matchnMove(2, point, "stop"))
        sendDirect(0, TOPLEVEL::type::Write,MAIN::control::stopSound, TOPLEVEL::section::main);
    else if (matchnMove(1, point, "list"))
    {
        if (commandList() == todo_msg)
        {
            replyString = "list";
            reply = what_msg;
        }
    }

    else if (matchnMove(1, point, "set"))
    {
        if (point[0] != 0)
            reply = commandReadnSet(TOPLEVEL::type::Write);
        else
        {
            replyString = "set";
            reply = what_msg;
        }
    }

    else if (matchnMove(1, point, "read") || matchnMove(1, point, "get"))
    {
        if (point[0] != 0)
            reply = commandReadnSet(TOPLEVEL::type::Read);
        else
        {
            replyString = "read";
            reply = what_msg;
        }
    }

    else if (matchnMove(3, point, "minimum"))
    {
        if (point[0] != 0)
            reply = commandReadnSet(TOPLEVEL::type::Minimum | TOPLEVEL::type::Limits);
        else
        {
            replyString = "minimum";
            reply = what_msg;
        }
    }

    else if (matchnMove(3, point, "maximum"))
    {
        if (point[0] != 0)
            reply = commandReadnSet(TOPLEVEL::type::Maximum | TOPLEVEL::type::Limits);
        else
        {
            replyString = "maximum";
            reply = what_msg;
        }
    }

    else if (matchnMove(3, point, "default"))
    {
        if (point[0] != 0)
            reply = commandReadnSet(TOPLEVEL::type::Default | TOPLEVEL::type::Limits);
        else
        {
            replyString = "default";
            reply = what_msg;
        }
    }

    else if (matchnMove(2, point, "mlearn"))
    {
        if (point[0] != 0)
            reply = commandReadnSet(TOPLEVEL::type::LearnRequest);
        else
        {
            replyString = "mlearn";
            reply = what_msg;
        }
    }

    else if (matchnMove(3, point, "add"))
    {
        if (matchnMove(1, point, "root"))
        {
            int found = synth->getBankRef().addRootDir(point);
            if (!found)
            {
                Runtime.Log("Can't find path " + (string) point);
            }
            else
            {
                GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePaths, 0);
                Runtime.Log("Added new root ID " + asString(found) + " as " + (string) point);
                synth->saveBanks(currentInstance);
            }
            reply = done_msg;
        }
        else if (matchnMove(1, point, "bank"))
        {
            int slot;
            for (slot = 0; slot < MAX_BANKS_IN_ROOT; ++slot)
            {
                if (synth->getBankRef().getBankName(slot).empty())
                    break;
            }
            if (!synth->getBankRef().newIDbank(point, (unsigned int)slot))
            {
                Runtime.Log("Could not create bank " + (string) point + " for ID " + asString(slot));
            }

            Runtime.Log("Created  new bank " + (string) point + " with ID " + asString(slot));
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePaths, 0);
        }
        else if (matchnMove(2, point, "yoshimi"))
        {
            int forceId = string2int(point);
            if (forceId < 1 || forceId >= 32)
                forceId = 0;
            sendDirect(forceId, TOPLEVEL::type::Write, MAIN::control::startInstance, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
        }
        else
        {
            replyString = "add";
            reply = what_msg;
        }
    }
    else if (matchWord(3, point, "import") || matchWord(3, point, "export") )
    { // need the double test to find which then move along line
        int type = 0;
        if (matchnMove(3, point, "import"))
            type = MAIN::control::importBank;
        else if (matchnMove(3, point, "export"))
            type = MAIN::control::exportBank;
        int root = UNUSED;
        if (matchnMove(1, point, "root"))
        {
            if (isdigit(point[0]))
            {
                root = string2int(point);
                point = skipChars(point);
            }
            else
                root = 200; // force invalid root error
        }
        int value = string2int(point);
        point = skipChars(point);
        string name = string(point);
        if (root < 0 || (root > 127 && root != UNUSED) || value < 0 || value > 127 || name <="!")
        {
            if (type == MAIN::control::importBank)
                replyString = "import";
            else
                replyString = "export";
            reply = value_msg;
        }
        else
        {
            sendDirect(value, TOPLEVEL::type::Write, type, TOPLEVEL::section::main, root, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(name));
            reply = done_msg;
        }
    }

    else if (matchnMove(3, point, "remove"))
    {
        if  (matchnMove(1, point, "root"))
        {
            if (isdigit(point[0]))
            {
                int rootID = string2int(point);
                if (rootID >= MAX_BANK_ROOT_DIRS)
                    reply = range_msg;
                else
                {
                    string rootname = synth->getBankRef().getRootPath(rootID);
                    if (rootname.empty())
                        Runtime.Log("Can't find path " + asString(rootID));
                    else
                    {
                        synth->getBankRef().removeRoot(rootID);
                        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePaths, 0);
                        Runtime.Log("Un-linked " + rootname);
                        synth->saveBanks(currentInstance);
                    }
                    reply = done_msg;
                }
            }
            else
                reply = value_msg;
        }
        else if (matchnMove(1, point, "bank"))
        {
            int rootID = UNUSED;
            if (matchnMove(1, point, "root"))
            {
                if (isdigit(point[0]))
                    rootID = string2int(point);
                if (rootID >= MAX_BANK_ROOT_DIRS)
                    reply = range_msg;
            }
            if (reply != range_msg && isdigit(point[0]))
            {
                point = skipChars(point);
                int bankID = string2int(point);
                if (bankID >= MAX_BANKS_IN_ROOT)
                    reply = range_msg;
                else
                {
                    replyString = synth->getBankRef().getBankName(bankID);
                    if (replyString.empty())
                        Runtime.Log("No bank at this location");
                    else
                    {
                        tmp = synth->getBankRef().getBankSize(bankID);
                        if (tmp)
                        {
                            Runtime.Log("Bank " + replyString + " has " + asString(tmp) + " Instruments");
                            if (query("Delete bank and all of these", false))
                                tmp = 0;
                            else
                                Runtime.Log("Aborted");
                        }
                        if (tmp == 0)
                        {
                            sendDirect(bankID, TOPLEVEL::type::Write, MAIN::control::deleteBank, TOPLEVEL::section::main, rootID, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
                        }
                    }

                }
            }
            else if (reply != range_msg)
                reply = value_msg;
        }
        else if(matchnMove(2, point, "yoshimi"))
        {
            if (point[0] == 0)
            {
                replyString = "remove";
                reply = what_msg;
            }
            else
            {
                int to_close = string2int(point);
                if (to_close == 0)
                    Runtime.Log("Use 'Exit' to close main instance");
                else
                    sendDirect(to_close, TOPLEVEL::type::Write, MAIN::control::stopInstance, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
                reply = done_msg;
            }
        }
        else
        {
            if (matchnMove(2, point, "mlearn"))
            {
                if (matchnMove(3, point, "all"))
                {
                    sendNormal(0, 0, MIDILEARN::control::clearAll, TOPLEVEL::section::midiLearn);
                    reply = done_msg;
                }
                else if (point[0] == '@')
                {
                    point += 1;
                    point = skipSpace(point);
                    tmp = string2int(point);
                    if (tmp > 0)
                        sendNormal(tmp - 1, 0, MIDILEARN::control::deleteLine, TOPLEVEL::section::midiLearn);
                    else
                        reply = value_msg;
                }
                else
                {
                    replyString = "remove";
                    reply = what_msg;
                }
            }
            else
            {
                replyString = "remove";
                reply = what_msg;
            }
        }
    }

    else if (matchnMove(2, point, "load"))
    {
        if(matchnMove(2, point, "mlearn"))
        {
            if (point[0] == '@')
            {
                point += 1;
                tmp = string2int(point);
                if (tmp > 0)
                {
                    sendNormal(0, TOPLEVEL::type::Write, MIDILEARN::control::loadFromRecent, TOPLEVEL::section::midiLearn, 0, 0, 0, 0, tmp - 1);
                    reply = done_msg;
                }
                else
                    reply = value_msg;

            }
            else
            {
                if ((string) point > "")
                {
                    sendNormal(0, TOPLEVEL::type::Write, MIDILEARN::control::loadList, TOPLEVEL::section::midiLearn, 0, 0, 0, 0, miscMsgPush((string) point));
                    reply = done_msg;
                }
                else
                    reply = name_msg;
            }
        }
        else if(matchnMove(2, point, "vector"))
        {
            string loadChan;
            unsigned char ch;
            if(matchnMove(1, point, "channel"))
            {
                ch = string2int127(point);
                if (ch > 0)
                {
                    ch -= 1;
                    point = skipChars(point);
                }
                else
                    ch = chan;
                loadChan = "channel " + asString(ch + 1);
            }
            else
            {
                ch = UNUSED;
                loadChan = "source channel";
            }
            if (ch != UNUSED && ch >= NUM_MIDI_CHANNELS)
                reply = range_msg;
            else if (point[0] == 0)
                reply = name_msg;
            else
            {
                bool ok = true;
                string name;
                if (point[0] == '@')
                {
                    point += 1;
                    point = skipSpace(point);
                    tmp = string2int(point);
                    if (tmp <= 0)
                    {
                        ok = false;
                        reply = value_msg;
                    }
                    name = historySelect(5, tmp - 1);
                    if (name == "")
                    {
                        ok = false;
                        reply = done_msg;
                    }
                }
                else
                {
                    name = (string)point;
                    if (name == "")
                    {
                        ok = false;
                        reply = name_msg;
                    }
                }
                if (ok)
                {
                    sendDirect(0, TOPLEVEL::type::Write, MAIN::control::loadNamedVector, TOPLEVEL::section::main, UNUSED, UNUSED, ch, TOPLEVEL::route::adjustAndLoopback, miscMsgPush(name));
                }
                reply = done_msg;
            }
        }
        else if(matchnMove(2, point, "state"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                bool ok = true;
                string name;
                if (point[0] == '@')
                {
                    point += 1;
                    point = skipSpace(point);
                    tmp = string2int(point);
                    if (tmp <= 0)
                    {
                        ok = false;
                        reply = value_msg;
                    }
                    name = historySelect(4, tmp - 1);
                    if (name == "")
                    {
                        ok = false;
                        reply = done_msg;
                    }
                }
                else
                {
                    name = (string)point;
                    if (name == "")
                    {
                        ok = false;
                        reply = name_msg;
                    }
                }
                if (ok)
                {
                    sendDirect(0, TOPLEVEL::type::Write, MAIN::control::loadNamedState, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::adjustAndLoopback, miscMsgPush(name));
                    reply = done_msg;
                }
            }
        }
        else if (matchnMove(2, point, "scale"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                bool ok = true;
                string name;
                if (point[0] == '@')
                {
                    point += 1;
                    point = skipSpace(point);
                    tmp = string2int(point);
                    if (tmp <= 0)
                    {
                        ok = false;
                        reply = value_msg;
                    }
                    name = historySelect(3, tmp - 1);
                    if (name == "")
                    {
                        ok = false;
                        reply = done_msg;
                    }
                }
                else
                {
                    name = (string)point;
                    if (name == "")
                    {
                        ok = false;
                        reply = name_msg;
                    }
                }
                if (ok)
                {
                    sendDirect(0, TOPLEVEL::type::Write, MAIN::control::loadNamedScale, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(name));
                    reply = done_msg;
                }
            }
        }
        else if (matchnMove(1, point, "patchset"))
        {
            bool ok = true;
            if (point[0] == 0)
            {
                ok = false;
                reply = name_msg;
            }
            else
            {
                string name;
                if (point[0] == '@')
                {
                    point += 1;
                    point = skipSpace(point);
                    tmp = string2int(point);
                    if (tmp <= 0)
                    {
                        ok = false;
                        reply = value_msg;
                    }
                    name = historySelect(2, tmp - 1);
                    if (name == "")
                    {
                        ok = false;
                        reply = done_msg;
                    }
                }
                else
                {
                    name = (string)point;
                    if (name == "")
                    {
                        ok = false;
                        reply = name_msg;
                    }
                }
                if (ok)
                {
                    sendDirect(0, TOPLEVEL::type::Write, MAIN::control::loadNamedPatchset, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::adjustAndLoopback, miscMsgPush(name));
                    reply = done_msg;
                }
            }
        }
        else if (matchnMove(1, point, "instrument"))
        {
            bool ok = true;
            if (point[0] == 0)
            {
                ok = false;
                reply = name_msg;
            }
            else
            {
                string name;
                if (point[0] == '@')
                {
                    point += 1;
                    point = skipSpace(point);
                    tmp = string2int(point);
                    if (tmp <= 0)
                    {
                        ok = false;
                        reply = value_msg;
                    }
                    name = historySelect(1, tmp - 1);
                    if (name == "")
                    {
                        ok = false;
                        reply = done_msg;
                    }
                }
                else
                {
                    name = (string)point;
                    cout << name << endl;
                    if (name == "")
                    {
                        ok = false;
                        reply = name_msg;
                    }
                }
                if (ok)
                {
                    sendDirect(npart, TOPLEVEL::type::Write, MAIN::control::loadNamedInstrument, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, miscMsgPush(name));
                    reply = done_msg;
                }
            }
        }
        else
        {
            replyString = "load";
            reply = what_msg;
        }
    }

    else if (matchnMove(2, point, "save"))
        if(matchnMove(2, point, "mlearn"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                sendNormal(0, TOPLEVEL::type::Write, MIDILEARN::control::saveList, TOPLEVEL::section::midiLearn, 0, 0, 0, 0, miscMsgPush((string) point));
                reply = done_msg;
            }
        }
        else if(matchnMove(2, point, "vector"))
        {
            tmp = chan;
            if(matchnMove(1, point, "channel"))
            {
                tmp = string2int127(point) - 1;
                point = skipChars(point);
            }
            if (tmp >= NUM_MIDI_CHANNELS || tmp < 0)
                reply = range_msg;
            else if (point[0] == 0)
                reply = name_msg;
            else
            {
                chan = tmp;
                sendDirect(0, TOPLEVEL::type::Write, MAIN::control::saveNamedVector, TOPLEVEL::section::main, UNUSED, UNUSED, chan, TOPLEVEL::route::lowPriority, miscMsgPush((string) point));
                reply = done_msg;
            }
        }
        else if(matchnMove(2, point, "state"))
            if (point[0] == 0)
                reply = value_msg;
            else
            {
                sendDirect(0, TOPLEVEL::type::Write, MAIN::control::saveNamedState, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(string(point)));
                reply = done_msg;
            }
        else if(matchnMove(1, point, "config"))
        {
            sendDirect(0, TOPLEVEL::type::Write, CONFIG::control::saveCurrentConfig, TOPLEVEL::section::config, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush("DUMMY"));
        }

        else if (matchnMove(2, point, "scale"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                sendDirect(0, TOPLEVEL::type::Write, MAIN::control::saveNamedScale, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(string(point)));
                reply = done_msg;
            }
        }else if (matchnMove(1, point, "patchset"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                sendDirect(0, TOPLEVEL::type::Write, MAIN::control::saveNamedPatchset, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(string(point)));
                reply = done_msg;
            }
        }
        else if (matchnMove(1, point, "instrument"))
        {
            if (synth->part[npart]->Pname == "Simple Sound")
            {
                Runtime.Log("Nothing to save!");
                reply = done_msg;
            }
            else if (point[0] == 0)
                reply = name_msg;
            else
            {
                sendDirect(npart, TOPLEVEL::type::Write, MAIN::control::saveNamedInstrument, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(string(point)));
                reply = done_msg;
            }
        }
        else
        {
            replyString = "save";
            reply = what_msg;
        }

    else if (matchnMove(6, point, "direct"))
    {
        unsigned char request;
        float value;
        unsigned char type = 0;
        if (matchnMove(3, point, "limits"))
        {
            value = 0;
            type = TOPLEVEL::type::Limits;
            if (matchnMove(3, point, "min"))
                request = TOPLEVEL::type::Minimum;
            else if (matchnMove(3, point, "max"))
                request = TOPLEVEL::type::Maximum;
            else if (matchnMove(3, point, "default"))
                request = TOPLEVEL::type::Default;
            else request = UNUSED;
        }
        else
        {
            request = UNUSED;
            value = string2float(point);
            if (strchr(point, '.') == NULL)
                type |= TOPLEVEL::type::Integer;
            point = skipChars(point);
            type |= (string2int127(point) & 0x43); // Allow 'pretend' and MIDI learn
            point = skipChars(point);
        }
        type |= TOPLEVEL::source::CLI;
        unsigned char control = string2int(point);
        point = skipChars(point);
        unsigned char part = string2int(point);
        point = skipChars(point);
        unsigned char kit = UNUSED;
        unsigned char engine = UNUSED;
        unsigned char insert = UNUSED;
        unsigned char param = UNUSED;
        unsigned char par2 = UNUSED;
        if (point[0] != 0)
        {
            kit = string2int(point);
            point = skipChars(point);
            if (point[0] != 0)
            {
                engine = string2int(point);
                point = skipChars(point);
                if (point[0] != 0)
                {
                    insert = string2int(point);
                    point = skipChars(point);
                    if (point[0] != 0)
                    {
                        param = string2int(point);
                        point = skipChars(point);
                        if ((part == TOPLEVEL::section::main && (control == MAIN::control::loadNamedPatchset || control == MAIN::control::loadNamedScale)) || ((param & TOPLEVEL::lowPriority) && param != UNUSED && insert != TOPLEVEL::insert::resonanceGraphInsert))
                        {
                            string name = string(point);
                            if (name > "!")
                                par2 = miscMsgPush(name);
                            //cout << "name " << name << endl;
                        }
                        else if (point[0] != 0)
                            par2 = string2int(point);
                    }
                }
            }
        }
        sendDirect(value, type, control, part, kit, engine, insert, param, par2, request);
        reply = done_msg;
    }
    else if (matchnMove(2, point, "zread"))
    {
        /*
         * This is a very specific test for reading values and is intended to measure
         * the time these calls take. For that reason the return echos to the CLI and
         * GUI are suppressed, and all results are sent to the CLI only.
         *
         * It is only the selection time we are measuring, and that the correct
         * value is returned.
         *
         * The limit to the number of repeats is INT max. Using high repeat numbers
         * reduces the effect of the processing overhead outside the call loop itself.
         */

        float value, result;
        unsigned char control, part;
        unsigned char kit = UNUSED;
        unsigned char engine = UNUSED;
        unsigned char insert = UNUSED;
        unsigned char parameter = UNUSED;
        unsigned char par2 = UNUSED;
        int repeats;
        if (point[0] == 0)
        {
            reply = value_msg;
            return false;
        }
        value = string2int(point);
        point = skipChars(point);
        if (point[0] == 0)
        {
            reply = value_msg;
            return false;
        }
        repeats = string2int(point);
        if (repeats < 1)
            repeats = 1;
        point = skipChars(point);
        if (point[0] == 0)
        {
            reply = value_msg;
            return false;
        }
        control = string2int(point);
        point = skipChars(point);
        if (point[0] == 0)
        {
            reply = value_msg;
            return false;
        }
        part = string2int(point);
        point = skipChars(point);
        if (point[0] != 0)
        {
            kit = string2int(point);
            point = skipChars(point);
            if (point[0] != 0)
            {
                engine = string2int(point);
                point = skipChars(point);
                if (point[0] != 0)
                {
                    insert = string2int(point);
                    point = skipChars(point);
                    if (point[0] != 0)
                    {
                        parameter = string2int(point);
                        point = skipChars(point);
                        if (point[0] != 0)
                            par2 = string2int(point);
                    }
                }
            }
        }

        CommandBlock putData;
        putData.data.value = value;
        putData.data.control = control;
        putData.data.part = part;
        putData.data.kit = kit;
        putData.data.engine = engine;
        putData.data.insert = insert;
        putData.data.parameter = parameter;
        putData.data.par2 = par2;
        putData.data.type = 0;
        struct timeval tv1, tv2;
        gettimeofday(&tv1, NULL);
        for (int i = 0; i < repeats; ++ i)
            result = synth->interchange.readAllData(&putData);
        gettimeofday(&tv2, NULL);

        if (tv1.tv_usec > tv2.tv_usec)
        {
            tv2.tv_sec--;
            tv2.tv_usec += 1000000;
            }
        float actual = (tv2.tv_sec - tv1.tv_sec) *1000000 + (tv2.tv_usec - tv1.tv_usec);
        cout << "result " << result << endl;
        cout << "Loops " << repeats << "  Total time " << actual << "uS" << "  average call time " << actual/repeats * 1000.0f << "nS" << endl;
        reply = done_msg;
    }
    else
      reply = unrecognised_msg;

    if (reply == what_msg)
        Runtime.Log(replyString + replies[what_msg]);
    else if (reply > done_msg)
        Runtime.Log(replies[reply]);
    return false;
}


int CmdInterface::sendNormal(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2)
{
    if (type >= TOPLEVEL::type::Limits && type < TOPLEVEL::source::CLI)
    {
        readLimits(value, type, control, part, kit, engine, insert, parameter, par2);
        return done_msg;
    }

    CommandBlock putData;
    size_t commandSize = sizeof(putData);

    putData.data.value = value;
    putData.data.type = type;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;

    /*
     * MIDI learn settings are synced by the audio thread
     * but not passed on to any of the normal controls.
     * The type field is used for a different purpose.
     */

    if (part != TOPLEVEL::section::midiLearn)
    {
        putData.data.type = TOPLEVEL::type::Limits;
        float newValue = synth->interchange.readAllData(&putData);
        if (type == TOPLEVEL::type::LearnRequest)
        {
            if ((putData.data.type & TOPLEVEL::type::Learnable) == 0)
            {
            synth->getRuntime().Log("Can't learn this control");
            return failed_msg;
            }
        }
        else
        {
            if (putData.data.type & TOPLEVEL::type::Error)
                return available_msg;
            if (newValue != value && (type & TOPLEVEL::type::Write))
            { // checking the original type not the reported one
                putData.data.value = newValue;
                synth->getRuntime().Log("Range adjusted");
            }
        }
        type |= TOPLEVEL::source::CLI;
    }

    putData.data.type = type;
    if (jack_ringbuffer_write_space(synth->interchange.fromCLI) >= commandSize)
    {
        synth->getRuntime().finishedCLI = false;
        jack_ringbuffer_write(synth->interchange.fromCLI, (char*) putData.bytes, commandSize);
    }
    else
    {
        synth->getRuntime().Log("Unable to write to fromCLI buffer");
        return failed_msg;
    }
    return done_msg;
}


int CmdInterface::sendDirect(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2, unsigned char request)
{
    if (type >= TOPLEVEL::type::Limits && type <= TOPLEVEL::source::CLI)
        request = type & TOPLEVEL::type::Default;
    CommandBlock putData;
    size_t commandSize = sizeof(putData);

    putData.data.value = value;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;

    if (type == TOPLEVEL::type::Default)
    {
        putData.data.type |= TOPLEVEL::type::Limits;
        synth->interchange.readAllData(&putData);
        if ((putData.data.type & TOPLEVEL::type::Learnable) == 0)
        {
            synth->getRuntime().Log("Can't learn this control");
            return 0;
        }
    }

    if (part != TOPLEVEL::section::midiLearn)
        type |= TOPLEVEL::source::CLI;
    /*
     * MIDI learn is synced by the audio thread but
     * not passed on to any of the normal controls.
     * The type field is used for a different purpose.
     */

    putData.data.type = type;
    if (request < TOPLEVEL::type::Limits)
    {
        putData.data.type = request | TOPLEVEL::type::Limits;
        value = synth->interchange.readAllData(&putData);
        string name;
        switch (request)
        {
            case TOPLEVEL::type::Minimum:
                name = "Min ";
                break;
            case TOPLEVEL::type::Maximum:
                name = "Max ";
                break;
            default:
                name = "Default ";
                break;
        }
        type = putData.data.type;
        if ((type & TOPLEVEL::type::Integer) == 0)
            name += to_string(value);
        else if (value < 0)
            name += to_string(int(value - 0.5f));
        else
            name += to_string(int(value + 0.5f));
        if (type & TOPLEVEL::type::Error)
            name += " - error";
        else if (type & TOPLEVEL::type::Learnable)
            name += " - learnable";
        synth->getRuntime().Log(name);
        return 0;
    }

    if (part == TOPLEVEL::section::main && (type & TOPLEVEL::type::Write) == 0 && control >= MAIN::control::readPartPeak && control <= MAIN::control::readMainLRrms)
    {
        string name;
        switch (control)
        {
            case MAIN::control::readPartPeak:
                name = "part " + to_string(int(kit)) + " peak ";
                break;
            case MAIN::control::readMainLRpeak:
                name = "main ";
                if (kit == 0)
                    name += "L ";
                else
                    name += "R ";
                name += "peak ";
                break;
            case MAIN::control::readMainLRrms:
                name = "main ";
                if (kit == 0)
                    name += "L ";
                else
                    name += "R ";
                name += "RMS ";
                break;
            }
            value = synth->interchange.readAllData(&putData);
            synth->getRuntime().Log(name + to_string(value));
        return 0;
    }

    if (part == TOPLEVEL::section::config && putData.data.par2 != UNUSED && (control == CONFIG::control::bankRootCC || control == CONFIG::control::bankCC || control == CONFIG::control::extendedProgramChangeCC))
    {
        synth->getRuntime().Log("In use by " + miscMsgPop(putData.data.par2) );
        return 0;
    }

    if (jack_ringbuffer_write_space(synth->interchange.fromCLI) >= commandSize)
    {
        synth->getRuntime().finishedCLI = false;
        jack_ringbuffer_write(synth->interchange.fromCLI, (char*) putData.bytes, commandSize);
    }
    else
        synth->getRuntime().Log("Unable to write to fromCLI buffer");
    return 0; // no function for this yet
}


float CmdInterface::readControl(unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2)
{
    float value;
    CommandBlock putData;

    putData.data.value = 0;
    putData.data.type = 0;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;
    value = synth->interchange.readAllData(&putData);
    if (putData.data.type & TOPLEVEL::type::Error)
        //return 0xfffff;
        cout << "err" << endl;
    return value;
}


void CmdInterface::readLimits(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2)
{
    CommandBlock putData;

    putData.data.value = value;
    putData.data.type = type;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;

        //putData.data.type = request | TOPLEVEL::type::Limits;
    value = synth->interchange.readAllData(&putData);
    string name;
    switch (type & 3)
    {
        case TOPLEVEL::type::Minimum:
            name = "Min ";
            break;
        case TOPLEVEL::type::Maximum:
            name = "Max ";
            break;
        default:
            name = "Default ";
            break;
    }
    type = putData.data.type;
    if ((type & TOPLEVEL::type::Integer) == 0)
        name += to_string(value);
    else if (value < 0)
        name += to_string(int(value - 0.5f));
    else
        name += to_string(int(value + 0.5f));
    if (type & TOPLEVEL::type::Error)
        name += " - error";
    else if (type & TOPLEVEL::type::Learnable)
        name += " - learnable";
    synth->getRuntime().Log(name);
}


void CmdInterface::cmdIfaceCommandLoop()
{
    // Initialise the history functionality
    // Set up the history filename
    string hist_filename;

    { // put this in a block to lose the passwd afterwards
        struct passwd *pw = getpwuid(getuid());
        hist_filename = string(pw->pw_dir) + string("/.yoshimi_history");
    }
    using_history();
    stifle_history(80); // Never more than 80 commands
    if (read_history(hist_filename.c_str()) != 0) // reading failed
    {
        perror(hist_filename.c_str());
        ofstream outfile (hist_filename.c_str()); // create an empty file
    }
    cCmd = NULL;
    bool exit = false;
    sprintf(welcomeBuffer, "yoshimi> ");
    synth = firstSynth;
    while(!exit)
    {
        cCmd = readline(welcomeBuffer);
        if (cCmd)
        {
            if(cCmd[0] != 0)
            {
                exit = cmdIfaceProcessCommand();
                add_history(cCmd);
            }
            free(cCmd);
            cCmd = NULL;

            if (!exit)
            {
                if (synth) // it won't be until Process called
                {
                    do
                    { // create enough delay for most ops to complete
                        usleep(2000);
                    }
                    while (synth->getRuntime().runSynth && !synth->getRuntime().finishedCLI);
                }
            }

            string prompt = "yoshimi";
            if (currentInstance > 0)
                prompt += (":" + asString(currentInstance));

            prompt += findStatus(true);

            if (bitTest(context, LEVEL::AllFX))
            {
                if (!bitTest(context, LEVEL::Part))
                {
                    if (bitTest(context, LEVEL::InsFX))
                    {
                        prompt += " Ins";
                        nFXtype = synth->insefx[nFX]->geteffect();
                    }
                    else
                    {
                        prompt += " Sys";
                        nFXtype = synth->sysefx[nFX]->geteffect();
                    }
                }
                prompt += (" efx " + asString(nFX + 1) + " " + fx_list[nFXtype].substr(0, 5));
                if (nFXtype > 0)
                    prompt += ("-" + asString(nFXpreset + 1));
            }

            prompt += "> ";
            sprintf(welcomeBuffer,"%s",prompt.c_str());
        }
        if (!exit)
            usleep(20000);
    }

    if (write_history(hist_filename.c_str()) != 0) // writing of history file failed
    {
        perror(hist_filename.c_str());
    }
}
