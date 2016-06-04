/*
    CmdInterface.cpp

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

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <cstdio>
#include <cerrno>
#include <sys/types.h>
#include <ncurses.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <algorithm>
#include <iterator>
#include <map>
#include <list>
#include <sstream>
#include <Misc/SynthEngine.h>
#include <Misc/MiscFuncs.h>
#include <Misc/Bank.h>

#include <Misc/CmdInterface.h>

using namespace std;


static int currentInstance = 0;

string basics[] = {
    "?  Help",                      "show commands",
    "STop",                         "all sound off",
    "RESet",                        "return to start-up conditions (if 'y')",
    "EXit",                         "tidy up and close Yoshimi (if 'y')",
    "..",                           "step back one level",
    "/",                            "step back to top level",
    "List",                         "various available parameters",
    "  Roots",                      "all available root paths",
    "  Banks [n]",                  "banks in root ID or current",
    "  Instruments [n]",            "instruments in bank ID or current",
    "  Parts",                      "parts with instruments installed",
    "  Vectors",                    "settings for all enabled vectors",
    "  Settings",                   "dynamic settings",
    "  History [s]",                "recent files (Patchsets, SCales, STates, Vectors)",
    "  Effects [s]",                "effect types ('all' include preset numbers and names)",
    "LOad",                         "load patch files",
    "  Instrument <s>",             "instrument to current part from named file",
    "  Patchset <s>",               "complete set of instruments from named file",
    "  STate <s>",                  "all system settings and patch sets from named file",
    "  SCale <s>",                  "scale settings from named file",
    "  VEctor [{Channel}n] <s>",    "vector on channel n from named file",
    "SAve",                         "save various files",
    "  Instrument <s>",             "current part to named file",
    "  Patchset <s>",               "complete set of instruments to named file",
    "  STate <s>",                  "all system settings and patch sets to named file",
    "  SCale <s>",                  "current scale settings to named file",
    "  VEctor <{Channel}n> <s>",    "vector on channel n to named file",
    "  Setup",                      "dynamic settings",
    "ADD",                          "add paths and files",
    "  Root <s>",                   "root path to list",
    "  Bank <s>",                   "bank to current root",
    "REMove",                       "remove paths and files",
    "  Root <n>",                   "de-list root path ID",
    "  Bank <n>",                   "delete bank ID (and all contents) from current root",
    "Set / Read",                   "set or read all main parameters",
    "  REPorts [s]",                "destination (Gui/Stderr)",
    "  ",                           "  non-fatal (SHow/Hide)",
    "  Root <n>",                   "current root path to ID",
    "  Bank <n>",                   "current bank to ID",
    "end"
};

string toplist [] = {
    "SYStem effects [n]",         "system effects for editing",
    "- Send <n2> <n3>",           "send system effect to effect n2 at volume n3",
    "- preset <n2>",              "set effect preset to number n2",
    "INSert effects [n1]",        "insertion effects for editing",
    "- Send <s>/<n2>",            "set where (Master, Off or part number)",
    "- PREset <n2>",              "set numbered effect preset to n2",
    "PRogram <n>",                "MIDI program change enabled (0 off, other on)",
    "ACtivate <n>",               "MIDI program change activates part (0 off, other on)",
    "CCRoot <n>",                 "CC for root path changes (> 119 disables)",
    "CCBank <n>",                 "CC for bank changes (0, 32, other disables)",
    "EXtend <n>",                 "CC for extended MIDI program change (> 119 disables)",
    "AVailable <n>",              "available parts (16, 32, 64)",
    "Volume <n>",                 "master volume",
    "SHift <n>",                  "master key shift semitones (0 no shift)",
    "PREferred Midi <s>",         "* MIDI connection type (Jack, Alsa)",
    "PREferred Audio <s>",        "* audio connection type (Jack, Alsa)",
    "Alsa Midi <s>",              "* name of alsa MIDI source",
    "Alsa Audio <s>",             "* name of alsa hardware device",
    "Jack Midi <s>",              "* name of jack MIDI source",
    "Jack Server <s>",            "* jack server name",
    "Jack AUto <s>",              "* (0 off, other on)",
    "end"
};

string vectlist [] = {
    "[X/Y] CC <n2>",            "CC n2 is used for CHANNEL X or Y axis sweep",
    "[X/Y] Features <n2> <s>",   "sets CHANNEL X or Y features 1-4 (Enable, Reverse, {other} off)",
    "[X] PRogram <l/r> <n2>",   "X program change ID for CHANNEL LEFT or RIGHT part",
    "[Y] PRogram <d/u> <n2>",   "Y program change ID for CHANNEL DOWN or UP part",
    "[X/Y] Control <n2> <n3>",  "sets n3 CC to use for X or Y feature n2 (2-4)",
    "Off",                      "disable vector for CHANNEL",
    "end"
};


string partlist [] = {
    "ENable",                   "enables the part",
    "DIsable",                  "disables the part",
    "Volume <n2>",              "volume",
    "Pan <n2>",                 "panning",
    "VElocity <n2>",            "velocity sensing sensitivity",
    "OFfset <n2>",              "velocity sense offest",
    "POrtamento <s>",           "portamento (Enable, other - disable",
    "Mode <s>",                 "key mode (Poly, Mono, Legato)",
    "Note <n2>",                "note polyphony",
    "SHift <n2>",               "key shift semitones (0 no shift)",
    "MIn <n2>",                 "minimum MIDI note value",
    "MAx <n2>",                 "maximum MIDI note value",
    "EFfects [n2]",             "effects for editing",
    "- Type <s>",               "the effect type",
    "- PREset <n3>",            "set numbered effect preset to n3",
    "- Send <n3> <n4>",         "send part to system effect n3 at volume n4",
    "PRogram <n2>",             "loads instrument ID",
    "NAme <s>",                 "sets the display name the part can be saved with",
    "Channel <n2>",             "MIDI channel (> 31 disables, > 15 note off only)",
    "Destination <s2>",         "jack audio destination (Main, Part, Both)",
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


bool CmdInterface::helpList()
{
    if (!matchnMove(1, point, "help") && !matchnMove(1, point, "?"))
        return false;
    list<string>msg;
    msg.push_back("Commands:");
    helpLoop(msg, basics, 2);

    if (!bitTest(level, vect_lev))
        msg.push_back("    Part [n1]                 - set part ID operations");
    if (bitTest(level, part_lev))
        helpLoop(msg, partlist, 6);
    else
        msg.push_back("    VEctor [n1]               - vector CHANNEL, operations");

    if (bitTest(level, vect_lev))
        helpLoop(msg, vectlist, 6);

    if (level <= 3)
    {
        helpLoop(msg, toplist, 4);
        msg.push_back("'*' entries need to be saved and Yoshimi restarted to activate");
    }

    if (synth->getRuntime().toConsole)
        // we need this in case someone is working headless
        cout << "\nSet REPorts [s] - set report destination (gui/stderr)\n\n";

    synth->cliOutput(msg, LINES);
    return true;
}


void CmdInterface::historyList(int listnum)
{
    list<string>msg;
    int start = 2;
    int end = 5;
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
            }
            for (vector<string>::iterator it = listType.begin(); it != listType.end(); ++it)
                msg.push_back("  " + *it);
            found = true;
        }
    }
    if (!found)
        msg.push_back("\nNo Saved History");

    synth->cliOutput(msg, LINES);
}


int CmdInterface::effectsList()
{
    list<string>msg;

    size_t presetsPos;
    size_t presetsLast;
    int presetsCount;
    string blanks;
    string left;
    bool all;

    if (bitTest(level, part_lev) && bitTest(level, all_fx))
    {
         synth->getRuntime().Log("Type " + fx_list[nFXtype] + "\nPresets -" + fx_presets[nFXtype].substr(fx_presets[nFXtype].find(',') + 1));
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
                msg.push_back("      " + asString(presetsCount) + " =" + fx_presets [i].substr(presetsLast, presetsPos - presetsLast));
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
            Runtime.Log("Current FX number is " + asString(nFX));
        return done_msg;
    }

    if (!isRead && isdigit(point[0]))
    {
        value = string2int(point);
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
            Runtime.Log("FX number set to " + asString(nFX));
            return done_msg;
        }
    }

    if (matchnMove(1, point, "type"))
    {
        if (isRead)
        {
            Runtime.Log("Current FX type is " + fx_list[nFXtype]);
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

        Runtime.Log("FX type set to " + fx_list[nFXtype]);
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
    else if (matchnMove(1, point, "send"))
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
                par = string2int(point);
                if (par >= Runtime.NumAvailableParts)
                    return range_msg;
                dest = "part " + asString(par);
                // done this way in case there is rubbish on the end
            }
            value = 0;
        }
        else
        {

            par = string2int(point);
            point = skipChars(point);
            if (point[0] == 0)
                return value_msg;
            value = string2int127(point);
        }
        if (bitTest(level, part_lev))
        {
            category = 2;
            dest = "part " + asString(npart) + " fx sent to system "
                 + asString(par) + " at " + asString(value);
        }
        else if (bitTest(level, ins_fx))
        {
            category = 1;
            dest = "insert fx " + asString(nFX) + " sent to " + dest;
        }
        else
        {
            if (par <= nFX)
                return range_msg;
            category = 0;
            dest = "system fx " + asString(nFX) + " sent to "
                 + asString(par) + " at " + asString(value);
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
        if (point[0] == 0)
            return value_msg;
        value = string2int127(point);
        if (value >= par)
            return range_msg;
        if (bitTest(level, part_lev))
        {
            category = 2;
            dest = "part " + asString(npart);
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
        Runtime.Log(dest + " fx preset set to number " + asString(nFXpreset));
    }
    return reply;
}


int CmdInterface::volPanShift()
{
    Config &Runtime = synth->getRuntime();
    int reply = todo_msg;
    int value;
    bool panelFlag = false;
    bool partFlag = false;

    if (matchnMove(1, point, "volume"))
    {
        if (point[0] == 0)
            return value_msg;
        value = string2int127(point);
        if(bitTest(level, part_lev))
        {
                synth->part[npart]->SetController(7, value);
                Runtime.Log("Volume set to " + asString(value));
                panelFlag = true;
        }
        else
                synth->SetSystemValue(7, value);
        reply = done_msg;
    }
    else if(bitTest(level, part_lev) && matchnMove(1, point, "pan"))
    {
        if (point[0] == 0)
            return value_msg;
        value = string2int127(point);
        synth->part[npart]->SetController(10, value);
        reply = done_msg;
        Runtime.Log("Panning set to " + asString(value));
        panelFlag = true;
    }
    else if (matchnMove(2, point, "shift"))
    {
        if (point[0] == 0)
            return value_msg;
        value = string2int(point);
        if (value < MIN_KEY_SHIFT)
            value = MIN_KEY_SHIFT;
        else if(value > MAX_KEY_SHIFT)
            value = MAX_KEY_SHIFT;
        if(bitTest(level, part_lev))
            synth->SetPartShift(npart, value + 64);
        else
                synth->SetSystemValue(2, value + 64);
        reply = done_msg;
    }
    else if (matchnMove(2, point, "velocity"))
    {
        if (point[0] == 0)
            return value_msg;
        value = string2int127(point);
        if (bitTest(level, part_lev))
        {
            synth->part[npart]->Pvelsns = value;
            Runtime.Log("Velocity sense set to " + asString(value));
            partFlag = true;
        }
        reply = done_msg;
    }
    else if (bitTest(level, part_lev) && matchnMove(2, point, "offset"))
    {
        if (point[0] == 0)
            return value_msg;
        value = string2int127(point);
        synth->part[npart]->Pveloffs = value;
        Runtime.Log("Velocity offset set to " + asString(value));
        partFlag = true;
        reply = done_msg;
    }

    if (panelFlag) // currently only volume and pan
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePanelItem, npart);
    if (partFlag)
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePart, 0);
    return reply;
}


int CmdInterface::commandVector()
{
    Config &Runtime = synth->getRuntime();
    list<string> msg;
    int reply = todo_msg;
    int tmp;

    if (isRead)
    {
        if (synth->SingleVector(msg, chan))
            synth->cliOutput(msg, LINES);
        else
            Runtime.Log("No vector on channel " + asString(chan));
        return done_msg;
    }
    if (point[0] == 0)
    {
        if (Runtime.nrpndata.vectorEnabled[chan])
            bitSet(level, vect_lev);
        else
            Runtime.Log("No vector on channel " + asString(chan));
        return done_msg;
    }

    if (isdigit(point[0]))
    {
        tmp = string2int127(point);
        if (tmp >= NUM_MIDI_CHANNELS)
            return range_msg;
        point = skipChars(point);
        if (chan != tmp)
        {
            chan = tmp;
            axis = 0;
        }

        Runtime.Log("Vector channel set to " + asString(chan));
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
        if (!Runtime.nrpndata.vectorEnabled[chan])
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
        if(Runtime.nrpndata.vectorEnabled[chan])
            bitSet(level, vect_lev);
        return done_msg;
    }
    if (!Runtime.nrpndata.vectorEnabled[chan])
    {
        Runtime.Log("Vector X CC must be set first");
        return done_msg;
    }

    if (axis == 1 && (Runtime.nrpndata.vectorYaxis[chan] > 0x7f))
    {
        Runtime.Log("Vector Y CC must be set first");
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
                vecfeat = Runtime.nrpndata.vectorXfeatures[chan];
            else
                vecfeat = Runtime.nrpndata.vectorYfeatures[chan];
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
        if (isdigit(point[0]))
        {
            tmp = string2int127(point);
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
                Runtime.Log("Part number set to " + asString(npart));
                return done_msg;
            }
        }
    }
    if (matchnMove(2, point, "effects"))
    {
        level = 1; // clear out any higher levels
        bitSet(level, part_lev);
        return effects();
    }
    tmp = volPanShift();
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
            synth->SetProgram(npart | 0x80, string2int(point));
            reply = done_msg;
        }
        else
            reply = value_msg;
    }
    else if (matchnMove(1, point, "channel"))
    {
        if (isRead || point[0] != 0)
        {
            if (isRead)
                tmp = synth->part[npart]->Prcvchn;
            else
            {
                tmp = string2int127(point);
                synth->SetPartChan(npart, tmp);
            }
            string name = "";
            if (tmp >= NUM_MIDI_CHANNELS * 2)
                name = " (no MIDI)";
            else if (tmp >= NUM_MIDI_CHANNELS)
                name = " (" + asString (tmp % NUM_MIDI_CHANNELS) + " note off only)";
            Runtime.Log("Part " + asString(npart) + " set to channel " + asString(tmp) + name, isRead);
            reply = done_msg;
        }
        else
            reply = value_msg;
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
        string name = "Part name set to ";
        if (isRead)
        {
            name += synth->part[npart]->Pname;
        }
        else
        {
            if (strlen(point) < 3)
                name = "Name too short";
            else
            {
                name += (string) point;
                synth->part[npart]->Pname = point;
                partFlag = true;
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

    else if (matchnMove(3, point, "reports"))
    {
        if (isRead)
        {
            if (Runtime.hideErrors)
                name = "Non-fatal reports";
            else
                name = "All reports";
            name += " sent to ";
            if (Runtime.toConsole)
                name += "console window";
            else
                name += "stderr";
            Runtime.Log(name, 1);
            return done_msg;
        }
        if (matchnMove(1, point, "gui"))
            synth->SetSystemValue(100, 127);
        else if (matchnMove(1, point, "stderr"))
            synth->SetSystemValue(100, 0);
        else if (matchnMove(2, point, "show"))
        {
            Runtime.hideErrors = false;
            Runtime.Log("Showing all errors");
        }
        else if (matchnMove(1, point, "hide"))
        {
            Runtime.hideErrors = true;
            Runtime.Log("Hiding non-fatal errors");
        }
        else
        {
            synth->SetSystemValue(100, 0);
            Runtime.hideErrors = false;
            Runtime.Log("Showing all errors");
        }
        reply = done_msg;
        Runtime.configChanged = true;
    }

    else if (matchnMove(1, point, "root"))
    {
        if (isRead)
        {
            Runtime.Log("Root is ID " + asString(synth->ReadBankRoot()), 1);
            return done_msg;
        }
        if (point[0] != 0)
        {
            synth->SetBankRoot(string2int(point));
            reply = done_msg;
        }
        else
            reply = value_msg;
    }
    else if (matchnMove(1, point, "bank"))
    {
        if (isRead)
        {
            Runtime.Log("Bank is ID " + asString(synth->ReadBank()), 1);
            return done_msg;
        }
        if (point[0] != 0)
        {
            synth->SetBank(string2int(point));
            reply = done_msg;
        }
        else
            reply = value_msg;
    }

    else if (bitTest(level, part_lev))
        reply = commandPart(false);
    else if (bitTest(level, vect_lev))
        reply = commandVector();
    if (reply > todo_msg)
        return reply;

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
    if (level < 4 && matchnMove(3, point, "system"))
    {
        level = 1;
        nFX = 0; // effects number limit changed
        matchnMove(2, point, "effects"); // clear it if given
        nFXtype = synth->sysefx[nFX]->geteffect();
        return effects();
    }
    if (level < 4 && matchnMove(3, point, "insert"))
    {
        level = 3;
        nFX = 0; // effects number limit changed
        matchnMove(2, point, "effects"); // clear it if given
        nFXtype = synth->insefx[nFX]->geteffect();
        return effects();
    }
    if (bitTest(level, all_fx))
        return effects();

    tmp = volPanShift();
    if(tmp > todo_msg)
        return tmp;

    if (matchnMove(2, point, "program") || matchnMove(4, point, "instrument"))
    {
        if (isRead)
        {
            string name = "MIDI program change ";
            if (Runtime.EnableProgChange)
                name += "enabled";
            else
                name += "disabled";
            Runtime.Log(name, 1);
            return done_msg;
        }
        if (point[0] == '0')
            synth->SetSystemValue(115, 0);
        else
            synth->SetSystemValue(115, 127);
        Runtime.configChanged = true;
        return done_msg;
    }
    else if (matchnMove(2, point, "activate"))
    {
        if (isRead)
        {
            string name = "Program change ";
            if (Runtime.enable_part_on_voice_load)
                name += "activates";
            else
                name += "ignores";
            Runtime.Log(name + " part", 1);
            return done_msg;
        }
        if (point[0] == '0')
            synth->SetSystemValue(116, 0);
        else
            synth->SetSystemValue(116, 127);
        Runtime.configChanged = true;
        return done_msg;
    }
    if (matchnMove(3, point, "ccroot"))
    {
        if (isRead)
        {
            Runtime.Log("Root CC is " + asString(Runtime.midi_bank_root));
            return done_msg;
        }
        if (point[0] != 0)
        {
            synth->SetSystemValue(113, string2int(point));
            reply = done_msg;
            Runtime.configChanged = true;
        }
        else
            reply = value_msg;
    }
    else if (matchnMove(3, point, "ccbank"))
    {
        if (isRead)
        {
            Runtime.Log("Bank CC is " + asString(Runtime.midi_bank_C));
            return done_msg;
        }
        if (point[0] != 0)
        {
            synth->SetSystemValue(114, string2int(point));
            reply = done_msg;
            Runtime.configChanged = true;
        }
        else
            reply = value_msg;
    }
    else if (matchnMove(1, point, "extend"))
    {
         if (isRead)
        {
            string name = "Extended program change ";
            tmp = Runtime.midi_upper_voice_C;
            if (tmp <= 119)
                name += "CC " + asString(tmp);
            else
                name += "disabled";
            Runtime.Log(name, 1);
            return done_msg;
        }
       if (point[0] != 0)
        {
            synth->SetSystemValue(117, string2int(point));
            reply = done_msg;
            Runtime.configChanged = true;
        }
        else
            reply = value_msg;
    }
    else if (matchnMove(2, point, "available")) // 16, 32, 64
    {
        if (isRead)
        {
            Runtime.Log(asString(Runtime.NumAvailableParts) + " available parts", 1);
            return done_msg;
        }
        if (point[0] != 0)
        {
            synth->SetSystemValue(118, string2int(point));
            reply = done_msg;
            Runtime.configChanged = true;
        }
        else
            reply = value_msg;
    }
    else if (matchnMove(3, point, "preferred"))
    {
        name = " set to ";
        if (matchnMove(1, point, "midi"))
        {
            name = "midi" + name;
            if (isRead)
            {
                tmp = (int) Runtime.midiEngine;
                if (tmp == 2)
                    name += "alsa";
                else if (tmp == 1)
                    name += "jack";
                else
                    name += "NULL";
            }
            else
            {
                if (matchnMove(1, point, "alsa"))
                {
                    Runtime.midiEngine = (midi_drivers) 2;
                    name += "alsa";
                }
                else if (matchnMove(1, point, "jack"))
                {
                    Runtime.midiEngine = (midi_drivers) 1;
                    name += "jack";
                }
                else
                    return value_msg;
            }
        }
        else if (matchnMove(1, point, "audio"))
        {
            name = "audio" + name;
            if (isRead)
            {
                tmp = (int) Runtime.audioEngine;
                if (tmp == 2)
                    name += "alsa";
                else if (tmp == 1)
                    name += "jack";
                else
                    name += "NULL";
            }
            else
            {
                if (matchnMove(1, point, "alsa"))
                {
                    Runtime.audioEngine = (audio_drivers) 2;
                    name += "alsa";
                }
                else if (matchnMove(1, point, "jack"))
                {
                    Runtime.audioEngine = (audio_drivers) 1;
                    name += "jack";
                }
                else
                    return value_msg;
            }
        }
        else
            return opp_msg;
        Runtime.Log("Preferred " + name, isRead);
        if (!isRead)
            Runtime.configChanged = true;
        return done_msg;
    }
    else if (matchnMove(1, point, "alsa"))
    {
        if (matchnMove(1, point, "midi"))
        {
            if (isRead || point[0] != 0)
            {
                if (!isRead)
                {
                    Runtime.alsaMidiDevice = (string) point;
                    Runtime.configChanged = true;
                }
                Runtime.Log("* ALSA MIDI set to " + Runtime.alsaMidiDevice, isRead);
            }
            else
                reply = value_msg;
        }
        else if (matchnMove(1, point, "audio"))
        {
            if (isRead || point[0] != 0)
            {
                if (!isRead)
                {
                    Runtime.alsaAudioDevice = (string) point;
                    Runtime.configChanged = true;
                }
                Runtime.Log("* ALSA AUDIO set to " + Runtime.alsaAudioDevice, isRead);
            }
            else
                reply = value_msg;
        }
        else
            reply = opp_msg;
        if (!isRead && reply == todo_msg)
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateConfig, 3);
    }
    else if (matchnMove(1, point, "jack"))
    {
        if (matchnMove(1, point, "midi"))
        {
            if (isRead || point[0] != 0)
            {
                if (!isRead)
                {
                    Runtime.jackMidiDevice = (string) point;
                    Runtime.configChanged = true;
                }
                Runtime.Log("* jack MIDI set to " + Runtime.jackMidiDevice, isRead);
            }
            else
                reply = value_msg;
        }
        else if (matchnMove(1, point, "server"))
        {
            if (isRead || point[0] != 0)
            {
                if (!isRead)
                {
                    Runtime.jackServer = (string) point;
                    Runtime.configChanged = true;
                }
                Runtime.Log("* Jack server set to " + Runtime.jackServer, isRead);
            }
            else
                reply = value_msg;
        }
        else if (matchnMove(2, point, "auto"))
        {
            name = "Jack autoconnect ";
            if (point[0] == '1')
            {
                Runtime.connectJackaudio = 1;
                name = "on";
            }
            else
            {
                Runtime.connectJackaudio = 0;
                name = "off";
            }
            Runtime.Log("Jack autoconnect " + name);
            Runtime.configChanged = true;
        }
        else
            reply = opp_msg;
        if (!isRead && reply == todo_msg)
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateConfig, 2);
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
    int ID;
    int reply = todo_msg;
    int tmp;
    point = cCmd;
    point = skipSpace(point); // just to be sure
    list<string> msg;

    if (matchnMove(2, point, "exit"))
    {
        if (Runtime.configChanged)
            replyString = "System config has been changed. Still exit";
        else
            replyString = "All data will be lost. Still exit";
        if (query(replyString, false))
        {
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
        {
            defaults();
            synth->resetAll();
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateMaster, 0);
        }
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
    if (helpList())
        return false;
    if (matchnMove(2, point, "stop"))
        synth->allStop();
    else if (matchnMove(1, point, "list"))
    {
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
        else if (matchnMove(1, point, "settings"))
        {
            synth->ListSettings(msg);
            synth->cliOutput(msg, LINES);
        }
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
            else
            {
                replyString = "list history";
                reply = what_msg;
            }
        }
        else if (matchnMove(1, point, "effects"))
            reply = effectsList();
        else
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
            replyString = "remove";
            reply = what_msg;
        }
    }

    else if (matchnMove(2, point, "load"))
    {
        if(matchnMove(2, point, "vector"))
        {
            string loadChan;
            if(matchnMove(1, point, "channel"))
            {
                if (isdigit(point[0]))
                {
                    tmp = string2int127(point);
                    point = skipChars(point);
                    chan = tmp;
                }
                else
                    tmp = chan;
                loadChan = "channel " + asString(chan);
            }
            else
            {
                tmp = 255;
                loadChan = "source channel";
            }
            if (tmp != 255 && tmp >= NUM_MIDI_CHANNELS)
                reply = range_msg;
            else if (point[0] == 0)
                reply = name_msg;
            else
            {
                if(synth->loadVector(tmp, (string) point, true))
                    Runtime.Log("Loaded Vector " + (string) point + " to " + loadChan);
                reply = done_msg;
            }
        }
        else if(matchnMove(2, point, "state"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else if (Runtime.loadState(point))
            {
                Runtime.Log("Loaded " + (string) point + ".state");
                GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateMaster, 0);
                reply = done_msg;
            }
        }
        else if (matchnMove(2, point, "scale"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                synth->microtonal.loadXML((string) point);
                reply = done_msg;
            }
        }
        else if (matchnMove(1, point, "patchset"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                int loadResult = synth->loadPatchSetAndUpdate((string) point);
                if (loadResult == 3)
                    Runtime.Log("At least one instrument is named 'Simple Sound'. This should be changed before resave");
                else if  (loadResult == 1)
                    Runtime.Log((string) point + " loaded");
                reply = done_msg;
            }
        }
        else if (matchnMove(1, point, "instrument"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else if (synth->SetProgramToPart(npart, -1, (string) point))
                reply = done_msg;
        }
        else
        {
            replyString = "load";
            reply = what_msg;
        }
    }

    else if (matchnMove(2, point, "save"))
        if(matchnMove(2, point, "vector"))
        {
            tmp = chan;
            if(matchnMove(1, point, "channel"))
            {
                tmp = string2int127(point);
                point = skipChars(point);
            }
            if (tmp >= NUM_MIDI_CHANNELS)
                reply = range_msg;
            else if (point[0] == 0)
                reply = name_msg;
            else
            {
                chan = tmp;
                if(synth->saveVector(chan, (string) point, true))
                    Runtime.Log("Saved channel " + asString(chan) + " Vector to " + (string) point);
                reply = done_msg;
            }
        }
        else if(matchnMove(2, point, "state"))
            if (point[0] == 0)
                reply = value_msg;
            else
            {
                Runtime.saveState(point);
                reply = done_msg;
            }
        else if(matchnMove(1, point, "setup"))
            synth->SetSystemValue(119, 255);
        else if (matchnMove(2, point, "scale"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                synth->microtonal.saveXML((string) point);
                reply = done_msg;
            }
        }else if (matchnMove(1, point, "patchset"))
        {
            if (point[0] == 0)
                reply = name_msg;
            else
            {
                replyString = setExtension((string) point, "xmz");
                tmp = synth->saveXML(replyString);
                if (!tmp)
                    Runtime.Log("Could not save " + (string) point);
                else
                    Runtime.Log("Saved " + replyString);
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
                replyString = setExtension((string) point, "xiz");
                synth->actionLock(lockmute);
                tmp = synth->part[npart]->saveXML(replyString);
                synth->actionLock(unlock);
                if (tmp)
                    Runtime.Log("Saved part " + asString(npart) + "  instrument " + (string) synth->part[npart]->Pname + "  as " +replyString);
                else
                    Runtime.Log("Failed to save " + replyString);
                reply = done_msg;
            }
        }
        else
        {
            replyString = "save";
            reply = what_msg;
        }
    else
      reply = unrecognised_msg;

    if (reply == what_msg)
        Runtime.Log(replyString + replies[what_msg]);
    else if (reply > done_msg)
        Runtime.Log(replies[reply]);
    return false;
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
                prompt += (" part " + asString(npart));
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
                prompt += (" FX " + asString(nFX) + " " + fx_list[nFXtype].substr(0, 5));
                if (nFXtype > 0)
                    prompt += ("-" + asString(nFXpreset));
            }
            if (bitTest(level, vect_lev))
            {
                prompt += (" Vect Ch " + asString(chan) + " ");
                if (axis == 0)
                    prompt += "X";
                else
                    prompt += "Y";
            }
            prompt += " > ";
            sprintf(welcomeBuffer,"%s",prompt.c_str());
        }
        else
            usleep(20000);
    }
    if (write_history(hist_filename.c_str()) != 0) // writing of history file failed
    {
        perror(hist_filename.c_str());
    }
}
