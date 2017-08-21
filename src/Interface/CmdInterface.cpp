/*
    CmdInterface.cpp

    Copyright 2015-2017, Will Godfrey & others.

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

    Modified August 2017
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

using namespace std;

#include "Misc/SynthEngine.h"
#include "Misc/MiscFuncs.h"
#include "Misc/Bank.h"

#include "Interface/InterChange.h"
#include "Interface/CmdInterface.h"

using namespace std;


static int currentInstance = 0;

string basics[] = {
    "?  Help",                  "show commands",
    "STop",                     "all sound off",
    "RESet",                    "return to start-up conditions (if 'y')",
    "EXit",                     "tidy up and close Yoshimi (if 'y')",
    "..",                       "step back one level",
    "/",                        "step back to top level",
    "end"
};

string toplist [] = {
    "ADD",                      "add paths and files",
    "  Root <s>",               "root path to list",
    "  Bank <s>",               "bank to current root",
    "REMove",                   "remove paths, files and entries",
    "  Root <n>",               "de-list root path ID",
    "  Bank <n>",               "delete bank ID (and all contents) from current root",
    "  MLearn <s> [n]",         "delete midi learned 'ALL' whole list, or '@'(n) line",
    "Set / Read",               "set or read all main parameters",
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
    "  SOlo [s] [n]",           "channel 'solo' switcher (Row, Column, Loop, CC, {other} Disable)",
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
    "Log [s]",                  "incomming MIDI CCs (Enable {other})",
    "SHow [s]",                 "GUI MIDI learn editor (Enable {other})",
    "end"
};

string partlist [] = {
    "ENable",                   "enables the part",
    "DIsable",                  "disables the part",
    "Volume <n2>",              "volume",
    "Pan <n2>",                 "panning",
    "VElocity <n2>",            "velocity sensing sensitivity",
    "OFfset <n2>",              "velocity sense offset",
    "POrtamento <s>",           "portamento (Enable {other})",
    "Mode <s>",                 "key mode (Poly, Mono, Legato)",
    "Note <n2>",                "note polyphony",
    "SHift <n2>",               "key shift semitones (0 no shift)",
    "MIn <n2>",                 "minimum MIDI note value",
    "MAx <n2>",                 "maximum MIDI note value",
    "EFfects [n2]",             "enter effects context level",
    "  Type <s>",               "the effect type",
    "  PREset <n3>",            "set numbered effect preset to n3",
    "  Send <n3> <n4>",         "send part to system effect n3 at volume n4",
    "PRogram <n2>",             "loads instrument ID",
    "NAme <s>",                 "sets the display name the part can be saved with",
    "Channel <n2>",             "MIDI channel (> 32 disables, > 16 note off only)",
    "Destination <s2>",         "jack audio destination (Main, Part, Both)",
    "end"
};

string learnlist [] = {
    "MUte <s>",                 "Enable/Disable this line (Enable, {other})",
    "7Bit",                     "Set incoming NRPNs as 7 bit (Enable, {other})",
    "CC <n2>",                  "Set incoming controler value",
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
    "Not available"
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
    level = 0;
    chan = 0;
    axis = 0;
    mline = 0;
    npart = 0;
    nFX = 0;
    nFXtype = 0;
    nFXpreset = 0;
    isRead = false;
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

    int listnum = 0;

    if (point[0] != 0)
    { // 1 & 2 reserved for syseff & inseff
        if (matchnMove(1, point, "part"))
            listnum = 3;
        else if (matchnMove(1, point, "vector"))
            listnum = 4;
        else if (matchnMove(1, point, "scale"))
            listnum = 5;
        else if (matchnMove(1, point, "load"))
            listnum = 6;
        else if (matchnMove(1, point, "save"))
            listnum = 7;
        else if (matchnMove(1, point, "list"))
            listnum = 8;
        else if (matchnMove(1, point, "config"))
            listnum = 9;
        else if (matchnMove(1, point, "mlearn"))
            listnum = 10;
    }
    else
    {
        if (bitTest(local, part_lev))
            listnum = 3;
        else if (bitTest(local, vect_lev))
            listnum = 4;
        else if (bitTest(local, scale_lev))
            listnum = 5;
        else if (bitTest(local, conf_lev))
            listnum = 9;
        else if (bitTest(local, learn_lev))
            listnum = 10;
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
        case 3:
            msg.push_back("Part: [n1] = part number");
            helpLoop(msg, partlist, 2);
            break;
        case 4:
            msg.push_back("Vector: [n1] = base channel:");
            helpLoop(msg, vectlist, 2);
            break;
        case 5:
            msg.push_back("Scale:");
            helpLoop(msg, scalelist, 2);
            break;
        case 6:
            msg.push_back("Load:");
            helpLoop(msg, loadlist, 2);
            break;
        case 7:
            msg.push_back("Save:");
            helpLoop(msg, savelist, 2);
            break;
        case 8:
            msg.push_back("List:");
            helpLoop(msg, listlist, 2);
            break;
        case 9:
            msg.push_back("Config:");
            helpLoop(msg, configlist, 2);
            msg.push_back("'*' entries need to be saved and Yoshimi restarted to activate");
            break;
        case 10:
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
    int start = 2;
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
                case 2:
                    msg.push_back("Recent Patch Sets:");
                    break;
                case 3:
                    msg.push_back("Recent Scales:");
                    break;
                case 4:
                    msg.push_back("Recent States:");
                    break;
                case 5:
                    msg.push_back("Recent Vectors:");
                    break;
                case 6:
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

    if (bitTest(level, all_fx) && presets == true)
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


int CmdInterface::effects()
{
    Config &Runtime = synth->getRuntime();
    int reply = done_msg;
    int nFXavail;
    int category;
    int par;
    int value;
    string dest = "";
    bool flag;

    nFXpreset = 0; // changing effect always sets the default preset.

    if (bitTest(level, part_lev))
    {
        nFXavail = NUM_PART_EFX;
    }
    else if (bitTest(level, ins_fx))
    {
        nFXavail = NUM_INS_EFX;
    }
    else
    {
        nFXavail = NUM_SYS_EFX;
    }
    if (point[0] == 0)
    {
        if (bitTest(level, part_lev))
        {
            synth->SetEffects(2, 1, nFX, nFXtype, 0, 0);
        }
        else if (bitTest(level, ins_fx))
        {
            synth->SetEffects(1, 1, nFX, nFXtype, 0, 0);
        }
        else
        {
            synth->SetEffects(0, 1, nFX, nFXtype, 0, 0);
        }

        if (isRead)
            Runtime.Log("Current efx number is " + asString(nFX + 1));
        return done_msg;
    }

    value = string2int(point);
    if (value > 0)
    {
        value -= 1;
        point = skipChars(point);
        if (value >= nFXavail)
            return range_msg;

        if (value != nFX)
        { // dummy 'SetEffects' calls to update GUI
            nFX = value;
            if (bitTest(level, part_lev))
            {
                nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
                synth->SetEffects(0, 2, nFX, nFXtype, 0, 0);
            }
            else if (bitTest(level, ins_fx))
            {
                nFXtype = synth->insefx[nFX]->geteffect();
                synth->SetEffects(0, 1, nFX, nFXtype, 0, 0);
            }
            else
            {
                nFXtype = synth->sysefx[nFX]->geteffect();
                synth->SetEffects(0, 0, nFX, nFXtype, 0, 0);
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
        if (isRead)
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

        Runtime.Log("efx type set to " + fx_list[nFXtype]);
        //Runtime.Log("Presets -" + fx_presets[nFXtype].substr(fx_presets[nFXtype].find(',') + 1));
        if (bitTest(level, part_lev))
            category = 2;
        else if (bitTest(level, ins_fx))
            category = 1;
        else
            category = 0;
        synth->SetEffects(category, 1, nFX, nFXtype, 0, 0);

        return done_msg;
    }
    else if (matchnMove(2, point, "send"))
    {
        if (point[0] == 0)
            return parameter_msg;

        if (bitTest(level, ins_fx))
        {
            if (matchnMove(1, point, "master"))
            {
                par = -2;
                dest = "master";
            }
            else if (matchnMove(1, point, "off"))
            {
                par = -1;
                dest = "off";
            }
            else
            {
                par = string2int(point) - 1;
                if (par >= Runtime.NumAvailableParts || par < 0)
                    return range_msg;
                dest = "part " + asString(par + 1);
                // done this way in case there is rubbish on the end
            }
            value = 0;
        }
        else
        {

            par = string2int(point) - 1;
            point = skipChars(point);
            if (point[0] == 0)
                return value_msg;
            value = string2int127(point);
        }
        if (bitTest(level, part_lev))
        {
            category = 2;
            dest = "part " + asString(npart + 1) + " efx sent to system "
                 + asString(par + 1) + " at " + asString(value);
        }
        else if (bitTest(level, ins_fx))
        {
            category = 1;
            dest = "insert efx " + asString(nFX + 1) + " sent to " + dest;
        }
        else
        {
            if (par <= nFX)
                return range_msg;
            category = 0;
            dest = "system efx " + asString(nFX + 1) + " sent to "
                 + asString(par + 1) + " at " + asString(value);
        }

        synth->SetEffects(category, 4, nFX, nFXtype, par, value);
        Runtime.Log(dest);
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
        par = string2int(fx_presets [nFXtype].substr(0, fx_presets [nFXtype].find(',')));
        if (par == 1)
            return available_msg;
        value = string2int127(point) - 1;
        if (value >= par || value < 0)
            return range_msg;
        if (bitTest(level, part_lev))
        {
            category = 2;
            dest = "part " + asString(npart + 1);
        }
        else if (bitTest(level, ins_fx))
        {
            category = 1;
            dest = "insert";
        }
        else
        {
            category = 0;
            dest = "system";
        }
        nFXpreset = value;
        synth->SetEffects(category, 8, nFX, nFXtype, 0, nFXpreset);
        Runtime.Log(dest + " efx preset set to number " + asString(nFXpreset + 1));
    }
    return reply;
}


int CmdInterface::keyShift(int part)
{
    int cmdType = 0;
    if (!isRead)
        cmdType = 64;
    if (!matchnMove(2, point, "shift"))
        return todo_msg;
    if (!isRead && point[0] == 0)
            return value_msg;
    int value = string2int(point);
    if (value < MIN_KEY_SHIFT)
        value = MIN_KEY_SHIFT;
    else if(value > MAX_KEY_SHIFT)
        value = MAX_KEY_SHIFT;
    sendDirect(value, cmdType, 35, part);
    return done_msg;
}


int CmdInterface::volPanVel()
{
    int reply = todo_msg;
    int cmdType = 0;
    if (!isRead)
        cmdType = 64;
    int cmd = -1;

    if (matchnMove(1, point, "volume"))
        cmd = 0;
    else if(matchnMove(1, point, "pan"))
        cmd = 2;
    else if (matchnMove(2, point, "velocity"))
        cmd = 1;
    else if (matchnMove(2, point, "offset"))
        cmd = 4;
    switch (cmd)
    {
        case 0:
        case 1:
        case 2:
        case 4:
            if (!isRead && point[0] == 0)
                return value_msg;
            sendDirect(string2float(point), cmdType, cmd, npart);
            reply = done_msg;
    }
    return reply;
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
                    synth->SetSystemValue(107, -(tmp - 1));
                    /*
                    * we use negative values to detail a single line
                    * because positive ones are used for bulk line count
                    */
                else
                    reply = value_msg;
            }
        else
            synth->SetSystemValue(107, LINES);
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
        else if (matchnMove(1, point, "patchsets"))
            historyList(2);
        else if (matchnMove(2, point, "scales"))
            historyList(3);
        else if (matchnMove(2, point, "states"))
            historyList(4);
        else if (matchnMove(1, point, "vectors"))
            historyList(5);
        else if (matchnMove(2, point, "mlearn"))
            historyList(6);
        else
            reply = todo_msg;
    }

    else if (matchnMove(1, point, "effects") || matchnMove(1, point, "efx"))
        reply = effectsList();
    else if (matchnMove(3, point, "presets"))
        reply = effectsList(true);

    return reply;
}


int CmdInterface::commandMlearn()
{
    Config &Runtime = synth->getRuntime();
    list<string> msg;
    int reply = todo_msg;
    int lineNo;
    int tmp = 0;
    float value;
    bitSet(level, learn_lev);

    if (isRead)
    {
        Runtime.Log("Write only");
        return done_msg; // will eventually be readable
    }

    if (!isdigit(point[0]))
        value = mline;
    else
    {
        lineNo = string2int(point) - 1;
        point = skipChars(point);
        if (lineNo < 0)
            return value_msg;
        else
        {
            value = lineNo;
            mline = lineNo;
        }
    }
    tmp = synth->midilearn.findSize();
    if (tmp == 0 || tmp <= mline)
    {
        if (tmp == 0)
            Runtime.Log("No learned lines");
        else
            Runtime.Log("Line " + to_string(mline + 1) + " Not found");
        mline = 0;
        return (done_msg);
    }

    tmp = 0;
    if (matchnMove(2, point, "cc"))
    {
        if (!isdigit(point[0]))
            return value_msg;
        tmp = string2int(point);
        if (tmp > 129)
        {
            Runtime.Log("Max CC value is 129");
            return done_msg;
        }
        sendDirect(value, 0xff, 0x10, 0xd8, tmp);
        Runtime.Log("Lines may be re-ordered");
        reply = done_msg;
    }
    else if (matchnMove(2, point, "channel"))
    {
        tmp = string2int(point) - 1;
        if (tmp < 0 || tmp > 16)
            tmp = 16;
        sendDirect(value, 0xff, 0x10, 0xd8, 0xff, tmp);
        Runtime.Log("Lines may be re-ordered");
        reply = done_msg;
    }
    else if (matchnMove(2, point, "minimum"))
    {
        int percent = int((string2float(point)* 2.0f) + 0.5f);
        if (percent < 0 || percent > 200)
            return value_msg;
        sendDirect(value, 0xff, 5, 0xd8, 0xff, 0xff, percent);
        reply = done_msg;
    }
    else if (matchnMove(2, point, "maximum"))
    {
        int percent = int((string2float(point)* 2.0f) + 0.5f);
        if (percent < 0 || percent > 200)
            return value_msg;
        sendDirect(value, 0xff, 6, 0xd8, 0xff, 0xff, 0xff, percent);
    }
    else if (matchnMove(2, point, "mute"))
    {
        if (matchnMove(1, point, "enable"))
            tmp = 4;
        sendDirect(value, tmp, 2, 0xd8);
    }
    else if (matchnMove(2, point, "limit"))
    {
        if (matchnMove(1, point, "enable"))
            tmp = 2;
        sendDirect(value, tmp, 1, 0xd8);
    }
    else if (matchnMove(2, point, "block"))
    {
        if (matchnMove(1, point, "enable"))
            tmp = 1;
        sendDirect(value, tmp, 0, 0xd8);
    }
    else if (matchnMove(2, point, "7bit"))
    {
        if (matchnMove(1, point, "enable"))
            tmp = 16;
        sendDirect(value, tmp, 4, 0xd8);
    }
    else
        reply = opp_msg;
    return reply;
}


int CmdInterface::commandVector()
{
    Config &Runtime = synth->getRuntime();
    list<string> msg;
    int reply = todo_msg;
    int tmp;
    bitSet(level, vect_lev);
    if (isRead)
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
        synth->vectorSet(127, chan, 0);
        axis = 0;
        bitClear(level, vect_lev);
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
        if (!synth->vectorInit(axis, chan, tmp))
            synth->vectorSet(axis, chan, tmp);
        if(Runtime.vectordata.Enabled[chan])
            bitSet(level, vect_lev);
        return done_msg;
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
        unsigned char type = 64;
        if (isRead)
            type = 0;
        else
        {
            name = string(point);
            if (name <= "!")
                return value_msg;
        }
        sendDirect(0, type, 8, 0xc0, 0xff, 0xff, chan, 0x80, miscMsgPush(name));
        return done_msg;
    }
    if (matchnMove(1, point, "features"))
    {
        unsigned int vecfeat;
        if (point[0] == 0)
            reply = value_msg;
        else
        {
            if (axis == 0)
                vecfeat = Runtime.vectordata.Xfeatures[chan];
            else
                vecfeat = Runtime.vectordata.Yfeatures[chan];
            tmp = string2int(point);
            if (tmp < 1 || tmp > 4)
                return range_msg;
            point = skipChars(point);
            if (matchnMove(1, point, "enable"))
            {
                bitSet(vecfeat, tmp - 1);
                if (tmp > 1) // volume is not reversible
                    bitClear(vecfeat, (tmp + 2)); // disable reverse
            }
            else if(matchnMove(1, point, "reverse"))
            {
                bitSet(vecfeat, tmp - 1);
                if (tmp > 1)
                    bitSet(vecfeat, (tmp + 2));
            }
            else
            {
                bitClear(vecfeat, tmp - 1);
                if (tmp > 1)
                    bitClear(vecfeat, (tmp + 2));
            }
            if (!synth->vectorInit(axis + 2, chan, vecfeat))
                synth->vectorSet(axis + 2, chan, vecfeat);
            reply = done_msg;
        }
    }
    else if (matchnMove(2, point, "program") || matchnMove(1, point, "instrument"))
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
        if (!synth->vectorInit(axis * 2 + hand + 4, chan, tmp))
            synth->vectorSet(axis * 2 + hand + 4, chan, tmp);
        reply = done_msg;
    }
    else
    {
        if (!matchnMove(1, point, "control"))
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
            synth->vectorSet(axis * 3 + cmd + 6, chan, tmp);
            reply = done_msg;
        }
        else
            reply = value_msg;
    }
    return reply;
}


int CmdInterface::commandConfig()
{
    if (point[0] == 0)
    {
        if (isRead)
            sendDirect(0, 0, 80, 248); // status
        return done_msg;
    }
    float value = 0;
    unsigned char type;
    unsigned char command = 0xff;
    unsigned char par = 255;
    unsigned char par2 = 255;
    if (isRead)
        type = 0x80;
    else
        type = 0xc0; // write, integer

    if (matchnMove(1, point, "oscillator"))
    {
        command = 0;
        if (!isRead && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(2, point, "buffer"))
    {
        command = 1;
        if (!isRead && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(2, point, "padsynth"))
    {
        command = 2;
        value = !matchnMove(1, point, "linear");
    }
    else if (matchnMove(1, point, "virtual"))
    {
        command = 3;
        if (!isRead && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(1, point, "xml"))
    {
        command = 4;
        if (!isRead && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(2, point, "reports"))
    {
        command = 5;
        value = !matchnMove(1, point, "stdout");
    }

    else if (matchnMove(2, point, "state"))
    {
        command = 16;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(1, point, "hide"))
    {
        command = 17;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(1, point, "display"))
    {
        command = 18;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(1, point, "time"))
    {
        command = 19;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(1, point, "include"))
    {
        command = 20;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(1, point, "keep"))
    {
        command = 21;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(1, point, "gui"))
    {
        command = 22;
        if (matchnMove(1, point, "enable"))
            value = 1;
        else if (matchnMove(1, point, "disable"))
            value = 0;
        else
            return value_msg;
    }
    else if (matchnMove(1, point, "cli"))
    {
        command = 23;
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
            command = 32;
            par = 128;
            if (isRead || point[0] != 0)
            {
                if (!isRead)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "server"))
        {
            command = 34;
            par = 128;
            if (isRead || point[0] != 0)
            {
                if (!isRead)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "auto"))
        {
            command = 36;
            value = (matchnMove(1, point, "enable"));
        }
        else
            return opp_msg;
    }

    else if (matchnMove(2, point, "alsa"))
    {
        if (matchnMove(1, point, "midi"))
        {
            command = 48;
            par = 128;
            if (isRead || point[0] != 0)
            {
                if (!isRead)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "audio"))
        {
            command = 50;
            par = 128;
            if (isRead || point[0] != 0)
            {
                if (!isRead)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "s"))
        {
            command = 52;
            if (!isRead)
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
            command = 49;
        else if (isRead == true || matchnMove(1, point, "jack"))
            command = 33;
        else
            return value_msg;
    }

    else if (matchnMove(2, point, "audio"))
    {
        value = 1;
        if (matchnMove(1, point, "alsa"))
            command = 51;
        else if (isRead == true || matchnMove(1, point, "jack"))
            command = 35;
        else
            return value_msg;
    }

    else if (matchnMove(2, point, "root"))
    {
        command = 65;
        if (isRead)
            value = 128; // ignored by range check
        else if (point[0] == 0)
            return value_msg;
        else
            value = string2int(point);
    }
    else if (matchnMove(2, point, "bank"))
    {
        command = 67;
        if (isRead)
            value = 128; // ignored by range check
        else if (point[0] == 0)
            return value_msg;
        else
            value = string2int(point);
    }
    else if (matchnMove(2, point, "program") || matchnMove(2, point, "instrument"))
    {
        command = 68;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(2, point, "activate"))
    {
        command = 69;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(1, point, "extend"))
    {
        command = 71;
        if (isRead)
            value = 128; // ignored by range check
        else if (point[0] == 0)
            return value_msg;
        else
            value = string2int(point);
    }
    else if (matchnMove(1, point, "Quiet"))
    {
        command = 72;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(1, point, "log"))
    {
        command = 73;
        value = matchnMove(1, point, "enable");
    }
    else if (matchnMove(2, point, "show"))
    {
        command = 74;
        value = matchnMove(1, point, "enable");
    }

    else
        return todo_msg; // may be picked up later

    sendDirect(value, type, command, 0xf8, 0xff, 0xff, 0xff, par, par2);
    return done_msg;
}


int CmdInterface::commandScale()
{
    if (point[0] == 0)
        return done_msg;
    Config &Runtime = synth->getRuntime();
    int reply = done_msg;
    float value = 0;
    unsigned char type;
    unsigned char command = 0xff;
    unsigned char par = 255;
    unsigned char par2 = 255;
    if (isRead)
    {
        type = 0x80;
        reply = done_msg;
    }
    else
        type = 0xc0; // write, integer

    string name;

    if (matchnMove(1, point, "tuning"))
        command = 0x20;
    else if (matchnMove(1, point, "keymap"))
        command = 0x21;
    else if (matchnMove(2, point, "name"))
        command = 0x40;
    else if (matchnMove(2, point, "description"))
        command = 0x41;

    if (command >= 0x20 && command <= 0x41)
    {
        if (isRead && command < 0x40)
        {
            Runtime.Log("Write only - use list");
            return done_msg;
        }
        else if (command <= 0x21)
        {
            if (matchnMove(3, point, "import"))
                command += 0x10;
            name = (string)point;
            if (name == "")
                return value_msg;
            par = 0x80;
            par2 = miscMsgPush(name);
        }
    }
    else
    {
        int min = 0;
        int max = 127;
        if (matchnMove(2, point, "frequency"))
        {
            command = 0;
            min = 1;
            max = 20000;
            type &= 0x7f; // float
        }
        else if(matchnMove(2, point, "note"))
            command = 1;
        else if(matchnMove(1, point, "invert"))
        {
            command = 2;
            max = 1;
        }
        else if(matchnMove(2, point, "center"))
            command = 3;
        else if(matchnMove(2, point, "shift"))
        {
            command = 4;
            min = -63;
            max = 64;
        }
        else if(matchnMove(2, point, "scale"))
        {
            command = 8;
            max = 1;
        }
        else if(matchnMove(2, point, "mapping"))
        {
            command = 16;
            max = 1;
        }
        else if(matchnMove(2, point, "first"))
            command = 17;
        else if(matchnMove(2, point, "middle"))
            command = 18;
        else if(matchnMove(1, point, "last"))
            command = 19;
        else if(matchnMove(3, point, "CLEar"))
        {
            point -=1; // sneaky way to force a zero :)
            command = 96;
        }
        else
            return todo_msg;

        if (!isRead)
        {
            if (point[0] == 0)
                return value_msg;
            if (matchnMove(1, point, "enable"))
                value = 1;
            else//if (isdigit(point[0]))
            {
                value = string2float(point);
                if (value < min || value > max)
                    return value_msg;
            }
        }
    }

    sendDirect(value, type, command, 0xe8, 0xff, 0xff, 0xff, par, par2);
    return reply;
}


int CmdInterface::commandPart(bool justSet)
{
    Config &Runtime = synth->getRuntime();
    int reply = todo_msg;
    int tmp;
    bool partFlag = false;

    if (point[0] == 0)
        return done_msg;
    if (bitTest(level, all_fx))
        return effects();
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
                Runtime.currentPart = npart;
                GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateMaster, 0);
            }
            if (point[0] == 0)
            {
                Runtime.Log("Part number set to " + asString(npart + 1));
                return done_msg;
            }
        }
    }
    if (matchnMove(2, point, "effects") || matchnMove(2, point, "efx"))
    {
        level = 1; // clear out any higher levels
        bitSet(level, part_lev);
        return effects();
    }
    tmp = keyShift(npart);
    if (tmp != todo_msg)
        return tmp;
    tmp = volPanVel();
    if(tmp != todo_msg)
        return tmp;
    if (matchnMove(2, point, "enable"))
    {
        synth->partonoffLock(npart, 1);
        Runtime.Log("Part enabled");
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePanelItem, npart);
        reply = done_msg;
    }
    else if (matchnMove(2, point, "disable"))
    {
        synth->partonoffLock(npart, 0);
        Runtime.Log("Part disabled");
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePanelItem, npart);
        reply = done_msg;
    }
    else if (matchnMove(2, point, "program") || matchnMove(1, point, "instrument"))
    {
        if (isRead)
        {
            Runtime.Log("Part name is " + synth->part[npart]->Pname);
            return done_msg;
        }
        if (point[0] != 0) // force part not channel number
        {
            tmp = string2int(point) - 1;
            if (tmp < 0 || tmp > 159)
                return range_msg;
            Runtime.finishedCLI = false;
            if (tmp < 128)
                synth->writeRBP(3, npart | 0x80, tmp); // lower set
            else
                synth->writeRBP(4, npart | 0x80, tmp - 128); // upper set
            reply = done_msg;
        }
        else
            reply = value_msg;
    }
    else if (matchnMove(1, point, "channel"))
    {
        tmp = string2int127(point);
        if(!isRead && tmp < 1)
            return value_msg;
        tmp -= 1;
        if (isRead)
            tmp = synth->part[npart]->Prcvchn;
        else
            synth->SetPartChan(npart, tmp);
        string name = "";
        if (tmp >= NUM_MIDI_CHANNELS * 2)
            name = " (no MIDI)";
        else if (tmp >= NUM_MIDI_CHANNELS)
            name = " (" + asString((tmp % NUM_MIDI_CHANNELS) + 1) + " note off only)";
        Runtime.Log("Part " + asString(npart + 1) + " set to channel " + asString(tmp + 1) + name, isRead);
        reply = done_msg;
    }
    else if (matchnMove(1, point, "destination"))
    {
        if (isRead)
        {
            string name;
            switch (synth->part[npart]->Paudiodest)
            {
                case 2:
                    name = "part";
                    break;
                case 3:
                    name = "both";
                    break;
                case 1:
                default:
                    name = "main";
                    break;
            }
            Runtime.Log("Jack audio to " + name, 1);
            return done_msg;
        }
        int dest = 0;

        if (matchnMove(1, point, "main"))
            dest = 1;
        else if (matchnMove(1, point, "part"))
            dest = 2;
        else if (matchnMove(1, point, "both"))
            dest = 3;
        if (dest > 0)
        {
            synth->partonoffWrite(npart, 1);
            synth->SetPartDestination(npart, dest);
            reply = done_msg;
        }
        else
            reply = range_msg;
    }
    else if (matchnMove(1, point, "note"))
    {
        string name = "Note limit set to ";
        if (isRead)
        {
            Runtime.Log(name + asString((int)synth->part[npart]->Pkeylimit), 1);
            return done_msg;
        }
        if (point[0] == 0)
            return value_msg;
        tmp = string2int(point);
        if (tmp < 1 || (tmp > POLIPHONY - 20))
            return range_msg;
        else
        {
            synth->part[npart]->setkeylimit(tmp);
            Runtime.Log(name + asString(tmp));
            partFlag = true;
        }
        reply = done_msg;
    }
    else if (matchnMove(2, point, "min"))
    {
        string name = "Min key set to ";
        if (isRead)
        {
            Runtime.Log(name + asString((int)synth->part[npart]->Pminkey));
            return done_msg;
        }
        if (point[0] == 0)
            return value_msg;
        tmp = string2int127(point);
        if (tmp > synth->part[npart]->Pmaxkey)
            return high_msg;
        else
        {
            synth->part[npart]->Pminkey = tmp;
            Runtime.Log(name + asString(tmp));
            partFlag = true;
        }
        reply = done_msg;
    }
    else if (matchnMove(2, point, "max"))
    {
        string name = "Max key set to ";
        if (isRead)
        {
            Runtime.Log(name + asString((int)synth->part[npart]->Pmaxkey), 1);
            return done_msg;
        }
       if (point[0] == 0)
            return value_msg;
        tmp = string2int127(point);
        if (tmp < synth->part[npart]->Pminkey)
            return low_msg;
        else
        {
            synth->part[npart]->Pmaxkey = tmp;
            Runtime.Log(name + asString(tmp));
            partFlag = true;
        }
        reply = done_msg;
    }
    else if (matchnMove(1, point, "mode"))
    {
        if (isRead)
        {
            string name;
            tmp = synth->ReadPartKeyMode(npart);
            switch (tmp)
            {
                case 2:
                    name = "'legato'";
                    break;
                case 1:
                    name = "'mono'";
                    break;
                case 0:
                default:
                    name = "'poly'";
                    break;
            }
            Runtime.Log("Key mode set to " + name, 1);
            return done_msg;
        }
        if (point[0] == 0)
            return value_msg;
        if (matchnMove(1, point, "poly"))
             synth->SetPartKeyMode(npart, 0);
        else if (matchnMove(1, point, "mono"))
            synth->SetPartKeyMode(npart, 1);
        else if (matchnMove(1, point, "legato"))
            synth->SetPartKeyMode(npart, 2);
        else
            return value_msg;
        partFlag = true;
        reply = done_msg;
    }
    else if (matchnMove(2, point, "portamento"))
    {
        if (isRead)
        {
            string name = "Portamento ";
            if (synth->ReadPartPortamento(npart))
                name += "enabled";
            else
                name += "disabled";
            Runtime.Log(name, 1);
            return done_msg;
        }
        if (point[0] == 0)
            return value_msg;
        if (matchnMove(1, point, "enable"))
        {
           synth->SetPartPortamento(npart, 1);
           Runtime.Log("Portamento enabled", isRead);
        }
        else
        {
           synth->SetPartPortamento(npart, 0);
           Runtime.Log("Portamento disabled");
        }
        reply = done_msg;
        partFlag = true;
    }
    else if (matchnMove(2, point, "name"))
    {
        string name;
        if (isRead)
        {
            name = "Part name set to " + synth->part[npart]->Pname;
        }
        else
        {
            name = (string) point;
            if (name.size() < 3)
                name = "Name too short";
            else if ( name == "Simple Sound")
                name = "Cant use name of default sound";
            else
            {
                sendDirect(0, 64, 222, npart, 255, 255, 255, 255, miscMsgPush(name));
                return done_msg;
            }
        }
        Runtime.Log(name);
        reply = done_msg;
    }
    else
        reply = opp_msg;
    if (partFlag)
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePart, 0);
    return reply;
}


int CmdInterface::commandReadnSet()
{
    Config &Runtime = synth->getRuntime();
    int reply = todo_msg;
    int tmp;
    string name;

    if (matchnMove(4, point, "yoshimi"))
    {
        if (isRead)
        {
            Runtime.Log("Instance " + asString(currentInstance), 1);
            return done_msg;
        }
        if (point[0] == 0)
            return value_msg;
        tmp = string2int(point);
        if (tmp >= (int)synthInstances.size())
            reply = range_msg;
        else
        {
            currentInstance = tmp;
            defaults();
        }
        return done_msg;
    }

    if (bitTest(level, conf_lev))
        reply = commandConfig();
    else if (bitTest(level, scale_lev))
        reply = commandScale();
    else if (bitTest(level, part_lev))
        reply = commandPart(false);
    else if (bitTest(level, vect_lev))
        reply = commandVector();
    else if (bitTest(level, learn_lev))
        reply = commandMlearn();
    if (reply > todo_msg)
        return reply;

    if (matchnMove(2, point, "config"))
    {
        level = 0; // clear all first
        bitSet(level, conf_lev);
        return commandConfig();
    }

    if (matchnMove(1, point, "scale"))
    {
        level = 0; // clear all first
        bitSet(level, scale_lev);
        return commandScale();
    }

    if (matchnMove(1, point, "part"))
    {
        nFX = 0; // effects number limit changed
        if (isRead && point[0] == 0)
        {
            if (synth->partonoffRead(npart))
                name = " enabled";
            else
                name = " disabled";
            Runtime.Log("Current part " + asString(npart) + name, 1);
            return done_msg;
        }
        level = 0; // clear all first
        bitSet(level, part_lev);
        nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
        return commandPart(true);
    }

    if (matchnMove(2, point, "vector"))
    {
        level = 0; // clear all first
        return commandVector();
    }

    if (matchnMove(2, point, "mlearn"))
    {
        level = 0; // clear all first
        return commandMlearn();
    }

    if (level < 4 && matchnMove(3, point, "system"))
    {
        level = 1;
        nFX = 0; // effects number limit changed
        matchnMove(2, point, "effects"); // clear it if given
        matchnMove(2, point, "efx");
        nFXtype = synth->sysefx[nFX]->geteffect();
        return effects();
    }
    if (level < 4 && matchnMove(3, point, "insert"))
    {
        level = 3;
        nFX = 0; // effects number limit changed
        matchnMove(2, point, "effects"); // clear it if given
        matchnMove(2, point, "efx");
        nFXtype = synth->insefx[nFX]->geteffect();
        return effects();
    }
    if (bitTest(level, all_fx))
        return effects();

    if (matchnMove(1, point, "root"))
    {
        if (isRead)
        {
            Runtime.Log("Root is ID " + asString(synth->ReadBankRoot()), 1);
            return done_msg;
        }
        if (point[0] != 0)
        {
            synth->SetBankRoot(string2int(point));
            return done_msg;
        }
        else
            return value_msg;
    }

    if (matchnMove(1, point, "bank"))
    {
        if (isRead)
        {
            Runtime.Log("Bank is ID " + asString(synth->ReadBank()), 1);
            return done_msg;
        }
        if (point[0] != 0)
        {
            synth->SetBank(string2int(point));
            return done_msg;
        }
        else
            return value_msg;
    }

    int cmdType = 0;
    if (!isRead)
        cmdType = 64;

    if (matchnMove(1, point, "volume"))
    {
        if (!isRead && point[0] == 0)
            return value_msg;
        sendDirect(string2int127(point), cmdType, 0, 240);
        return done_msg;
    }
    if (matchnMove(2, point, "detune"))
    {
        if (!isRead && point[0] == 0)
            return value_msg;
        sendDirect(string2int127(point), cmdType, 32, 240);
        return done_msg;
    }

    tmp = keyShift(240);
    if (tmp != todo_msg)
        return tmp;

    if (matchnMove(2, point, "solo"))
    {
        int value = 0;
        int command = 48;

        if (matchnMove(2, point, "cc"))
        {
            if (!isRead)
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
            command = 49;
        }
        else if (matchnMove(1, point, "loop"))
            value = 3;
        else if (matchnMove(1, point, "column"))
            value = 2;
        else if (matchnMove(1, point, "row"))
            value = 1;

        sendDirect(value, cmdType, command, 240);
        return done_msg;
    }
    else if (matchnMove(2, point, "available")) // 16, 32, 64
    {
        if (!isRead && point[0] == 0)
            return value_msg;
        int value = string2int(point);
        if (value != 16 && value != 32 && value != 64)
            return range_msg;
        sendDirect(value, cmdType, 15, 240);
        return done_msg;
    }

    else
        reply = opp_msg;
    return reply;
}

bool CmdInterface::cmdIfaceProcessCommand()
{
    map<SynthEngine *, MusicClient *>::iterator itSynth = synthInstances.begin();
    if (currentInstance >= (int)synthInstances.size())
    {
        currentInstance = 0;
        defaults();
    }
    for(int i = 0; i < currentInstance; i++, ++itSynth);
    synth = itSynth->first;
    Config &Runtime = synth->getRuntime();

    replyString = "";
    npart = Runtime.currentPart;
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

    if (matchnMove(2, point, "exit"))
    {
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
        level = 0;
        if (point[0] == 0)
            return false;
    }

    if (matchnMove(3, point, "reset"))
    {
        if (query("Restore to basic settings", false))
            sendDirect(0, 64, 96, 240, 255, 255, 255, 192);
        return false;
    }

    else if (point[0] == '.' && point[1] == '.')
    {
        point += 2;
        point = skipSpace(point);
        if (bitTest(level, all_fx)) // clears any effects level
        {
            bitClear(level, all_fx);
            bitClear(level, ins_fx);
        }
        else
        {
            tmp = bitFindHigh(level);
            bitClear(level, tmp);
        }
        if (point[0] == 0)
            return false;
    }
    if (helpList(level))
        return false;
    if (matchnMove(2, point, "stop"))
        sendDirect(0, 64, 128, 240);
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
        {
            isRead = false;
            reply = commandReadnSet();
        }
        else
        {
            replyString = "set";
            reply = what_msg;
        }
    }

    else if (matchnMove(1, point, "read") || matchnMove(1, point, "get"))
    {
        if (point[0] != 0)
        {
            isRead = true;
            reply = commandReadnSet();
        }
        else
        {
            replyString = "read";
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
        else
        {
            replyString = "add";
            reply = what_msg;
        }
    }

    else if (matchnMove(3, point, "remove"))
    {
        if  (matchnMove(1, point, "root"))
        {
            if (isdigit(point[0]))
            {
                int rootID = string2int(point);
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
            else
                reply = value_msg;
        }
        else if (matchnMove(1, point, "bank"))
        {
            if (isdigit(point[0]))
            {
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
                            if (synth->getBankRef().removebank(bankID))
                                Runtime.Log("Removed bank " + replyString);
                            else
                                Runtime.Log("Deleting failed. Some files may still exist");
                            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePaths, 0);
                        }
                    }

                }
            }
            else
                reply = value_msg;
        }
        else
        {
            if (matchnMove(2, point, "mlearn"))
            {
                if (matchnMove(3, point, "all"))
                {
                    sendDirect(0, 0, 0x60, 0xd8);
                    reply = done_msg;
                }
                else if (point[0] == '@')
                {
                    point += 1;
                    point = skipSpace(point);
                    tmp = string2int(point);
                    if (tmp > 0)
                        sendDirect(tmp - 1, 0, 8, 0xd8);
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
                    Runtime.finishedCLI = false;
                    sendDirect(0, 64, 0xf2, 0xd8, 0, 0, 0, 0, tmp - 1);
                    reply = done_msg;
                }
                else
                    reply = value_msg;

            }
            else
            {
                if ((string) point > "")
                {
                    Runtime.finishedCLI = false;
                    sendDirect(0, 64, 0xf1, 0xd8, 0, 0, 0, 0, miscMsgPush((string) point));
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
                ch = 255;
                loadChan = "source channel";
            }
            if (ch != 255 && ch >= NUM_MIDI_CHANNELS)
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
                    Runtime.finishedCLI = false;
                    sendDirect(0, 64, 84, 240, 255, 255, ch, 192, miscMsgPush(name));
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
                    Runtime.finishedCLI = false;
                    sendDirect(0, 64, 92, 0xf0, 0xff, 0xff, 0xff, 0xc0, miscMsgPush(name));
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
                    Runtime.finishedCLI = false;
                    sendDirect(0, 64, 88, 0xf0, 0xff, 0xff, 0xff, 0x80, miscMsgPush(name));
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
                    Runtime.finishedCLI = false;
                    sendDirect(0, 64, 80, 240, 255, 255, 255, 192, miscMsgPush(name));
                    reply = done_msg;
                }
            }
        }
        else if (matchnMove(1, point, "instrument"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                Runtime.finishedCLI = false;
                synth->writeRBP(5, npart, miscMsgPush((string) point));
            }
            reply = done_msg;
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
                Runtime.finishedCLI = false;
                sendDirect(0, 64, 0xf5, 0xd8, 0, 0, 0, 0, miscMsgPush((string) point));
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
                Runtime.finishedCLI = false;
                sendDirect(0, 64, 85, 0xf0, 0xff, 0xff, chan, 0x80, miscMsgPush((string) point));
                Runtime.finishedCLI = true;
                reply = done_msg;
            }
        }
        else if(matchnMove(2, point, "state"))
            if (point[0] == 0)
                reply = value_msg;
            else
            {
                Runtime.finishedCLI = false;
                sendDirect(0, 64, 93, 0xf0, 0xff, 0xff, 0xff, 0x80, miscMsgPush(string(point)));
                reply = done_msg;
            }
        else if(matchnMove(1, point, "config"))
        {
            Runtime.finishedCLI = false;
            sendDirect(0, 64, 80, 248, 0xff, 0xff, 0xff, 0x80, miscMsgPush("DUMMY"));
        }

        else if (matchnMove(2, point, "scale"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                Runtime.finishedCLI = false;
                sendDirect(0, 64, 89, 0xf0, 0xff, 0xff, 0xff, 0x80, miscMsgPush(string(point)));
                reply = done_msg;
            }
        }else if (matchnMove(1, point, "patchset"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                Runtime.finishedCLI = false;
                sendDirect(0, 64, 81, 0xf0, 0xff, 0xff, 0xff, 0x80, miscMsgPush(string(point)));
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
                Runtime.finishedCLI = false;
                replyString = setExtension((string) point, "xiz");
                tmp = synth->part[npart]->saveXML(replyString);
                if (tmp)
                    Runtime.Log("Saved part " + asString(npart + 1) + "  instrument " + (string) synth->part[npart]->Pname + "  as " +replyString);
                else
                    Runtime.Log("Failed to save " + replyString);
                reply = done_msg;
                Runtime.finishedCLI = true;
            }
        }
        else
        {
            replyString = "save";
            reply = what_msg;
        }

    else if (matchnMove(6, point, "direct"))
    {
        float value;
        unsigned char type = 0;
        if (matchnMove(3, point, "limits"))
            value = FLT_MAX;
        else if (matchnMove(3, point, "default"))
        {
            value = FLT_MAX / 1.5f;
            type = 0x40;
        }
        else
        {
            value = string2float(point);
            if (strchr(point, '.') == NULL)
                type |= 0x80; // fix as integer
            point = skipChars(point);
            type |= (string2int127(point) & 0x43); // Allow 'pretend' and MIDI learn
            point = skipChars(point);
        }
        type |= 0x10; // Fix as from CLI
        unsigned char control = string2int(point);
        point = skipChars(point);
        unsigned char part = string2int(point);
        point = skipChars(point);
        unsigned char kit = 0xff;
        unsigned char engine = 0xff;
        unsigned char insert = 0xff;
        unsigned char param = 0xff;
        unsigned char par2 = 0xff;
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
                        if (((control == 80 || control == 88) && part == 240) || ((param & 0x80) && param < 0xff))
                        {
                            string name = string(point);
                            if (name < "!")
                                name = "no file";
                            par2 = miscMsgPush(name);
                            //cout << name << endl;
                        }
                        else if (point[0] != 0)
                            par2 = string2int(point);
                    }
                }
            }
        }
        sendDirect(value, type, control, part, kit, engine, insert, param, par2);
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


int CmdInterface::sendDirect(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2)
{
    if (part != 0xd8) // MIDI learn
        type |= 0x10; // from command line
    /*
     * MIDI learn is synced by the audio thread but
     * not passed on to any of the normal controls.
     * The type field is used for a different purpose.
     */
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
    if (putData.data.value == FLT_MAX)
    {
        synth->interchange.resolveReplies(&putData);
        string name = miscMsgPop(putData.data.par2) + "\n~ ";
        putData.data.par2 = par2; // restore this
        synth->interchange.returnLimits(&putData);
        unsigned char returntype = putData.data.type;
        short int min = putData.limits.min;
        short int def = putData.limits.def;
        short int max = putData.limits.max;
        if (min > max)
        {
            synth->getRuntime().Log("Text: " + miscMsgPop(def));
            return 0;
        }
        if (min == -1 && def == -10 && max == -1)
        {
            synth->getRuntime().Log("Unrecognised Control");
            return 0;
        }
        string valuetype = "   Type ";
        if (returntype & 0x80)
            valuetype += " integer";
        else
            valuetype += " float";
        if (returntype & 0x40)
            valuetype += " learnable";

        string deftype;
        if (def >= 10 || def <= 0)
            deftype = to_string(lrint(def / 10));
        else
            deftype = to_string(float(def / 10.0f) + 0.000001).substr(0,4);

        synth->getRuntime().Log(name + "Min " + to_string(min)  + "   Def " + deftype + "   Max " + to_string(max) + valuetype);
        return 0;
    }
    // next line screws effects :(
    //synth->interchange.testLimits(&putData);
    if (part == 0xf8 && putData.data.par2 < 0xff && (control == 65 || control == 67 || control == 71))
    {
        synth->getRuntime().Log("In use by " + miscMsgPop(putData.data.par2) );
        return 0;
    }

    if (jack_ringbuffer_write_space(synth->interchange.fromCLI) >= commandSize)
    {
        jack_ringbuffer_write(synth->interchange.fromCLI, (char*) putData.bytes, commandSize);
    }
    return 0; // no function for this yet
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
            string prompt = "yoshimi";
            if (currentInstance > 0)
                prompt += (":" + asString(currentInstance));
            if (bitTest(level, part_lev))
            {
                prompt += (" part " + asString(npart + 1));
                nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
                if (synth->partonoffRead(npart))
                    prompt += " on";
                else
                    prompt += " off";
            }
            if (bitTest(level, all_fx))
            {
                if (!bitTest(level, part_lev))
                {
                    if (bitTest(level, ins_fx))
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
            if (bitTest(level, scale_lev))
                prompt += " Scale ";
            if (bitTest(level, conf_lev))
                prompt += " Config ";
            if (bitTest(level, vect_lev))
            {
                prompt += (" Vect Ch " + asString(chan + 1) + " ");
                if (axis == 0)
                    prompt += "X";
                else
                    prompt += "Y";
            }
            if (bitTest(level, learn_lev))
            {
                prompt += (" MLearn line " + asString(mline + 1) + " ");
            }

            prompt += "> ";
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
                sprintf(welcomeBuffer,"%s",prompt.c_str());
            }
        }
        if (!exit)
            usleep(20000);
    }

    if (write_history(hist_filename.c_str()) != 0) // writing of history file failed
    {
        perror(hist_filename.c_str());
    }
}
