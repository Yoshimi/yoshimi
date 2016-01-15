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
    "?  help",                      "list commands",
    "stop",                         "all sound off",
    "reset",                        "return to start-up conditions (if 'y')",
    "exit",                         "tidy up and close Yoshimi (if 'y')",
    "..",                           "step back one level",
    "/",                            "step back to top level",
    "list",                         "various available parameters",
    "  banks [n]",                  "banks in root ID or current",
    "  instruments [n]",            "instruments in bank ID or current",
    "  current",                    "parts with instruments installed",
    "  vectors",                    "settings for all enabled vectors",
    "  setup",                      "dynamic settings",
    "  effects [s]",                "effect types ('all' include preset numbers and names)",
    "load instrument <s>",          "load an instrument to current part from named file",
    "save instrument <s>",          "save current part to named file",
    "load patchset <s>",            "load a complete set of instruments from named file",
    "save patchset <s>",            "save a complete set of instruments to named file",
    "save setup",                   "save dynamic settings",
    "paths",                        "display bank root paths",
    "path add <s>",                 "add bank root path",
    "path remove <n>",              "remove bank root path ID",
    "set",                          "set all main parameters",
    "  reports [n]",                "report destination (1 GUI console, other stderr)",
    "  root <n>",                   "current root path to ID",
    "  bank <n>",                   "current bank to ID",
    "end"
};

string toplist [] = {
    "system effects [n]",         "system effects for editing",
    "- send <n2> <n3>",           "send system effect to effect n2 at volume n3",
    "- preset <n2>",              "set effect preset to number n2",
    "insert effects [n1]",        "insertion effects for editing",
    "- send <s>/<n2>",            "set where: master, off or part number",
    "- preset <n2>",              "set numbered effect preset to n2",
    "program <n>",                "MIDI program change enabled (0 off, other on)",
    "activate <n>",               "MIDI program change activates part (0 off, other on)",
    "ccroot <n>",                 "CC for root path changes (> 119 disables)",
    "ccbank <n>",                 "CC for bank changes (0, 32, other disables)",
    "extend <n>",                 "CC for extended MIDI program change (> 119 disables)",
    "available <n>",              "available parts (16, 32, 64)",
    "volume <n>",                 "master volume",
    "shift <n>",                  "master key shift semitones (64 no shift)",
    "preferred midi <s>",         "* MIDI connection type jack/alsa",
    "preferred audio <s>",        "* audio connection type jack/alsa",
    "alsa midi <s>",              "* name of alsa MIDI source",
    "alsa audio <s>",             "* name of alsa hardware device",
    "jack midi <s>",              "* name of jack MIDI source",
    "jack server <s>",            "* jack server name",
    "end"
};

string vectlist [] = {
    "[x/y] cc <n2>",            "CC n2 is used for CHANNEL X or Y axis sweep",
    "[x/y] features <n2>",      "sets CHANNEL X or Y features",
    "[x] program <l/r> <n2>",   "X program change ID for CHANNEL LEFT or RIGHT part",
    "[y] program <d/u> <n2>",   "Y program change ID for CHANNEL DOWN or UP part",
    "[x/y] control <n2> <n3>",  "sets n3 CC to use for X or Y feature n2 (2, 4, 8)",
    "off",                      "disable vector for CHANNEL",
    "end"
};


string partlist [] = {
    "enable",                   "enables the part",
    "disable",                  "disables the part",
    "volume <n2>",              "volume",
    "pan <n2>",                 "panning",
    "velocity <n2>",            "velocity sensing sensitivity",
    "offset <n2>",              "velocity sense offest",
    "portamento <s>",           "portamento (en - enable, other - disable",
    "mode <s>",                 "key mode (poly, mono, legato)",
    "note <n2>",                "note polyphony",
    "shift <n2>",               "key shift semitones (64 no shift)",
    "min <n2>",                 "minimum MIDI note value",
    "max <n2>",                 "maximum MIDI note value",
    "effects [n2]",             "effects for editing",
    "- type <s>",               "the effect type",
    "- preset <n3>",            "set numbered effect preset to n3",
    "- send <n3> <n4>",         "send part to system effect n3 at volume n4",    "program <n2>",             "loads instrument ID",
    "name <s>",                 "sets the display name the part can be saved with",
    "channel <n2>",             "MIDI channel (> 15 disables)",
    "destination <s2>",         "jack audio destination (main, part, both)",
    "end"
};

string replies [] = {
    "OK",
    "Done",
    "Value?",
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
    "off",
    "reverb",
    "echo",
    "chorus",
    "phaser",
    "alienwah",
    "distortion",
    "eq",
    "dynfilter"
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
        msg.push_back(dent.assign<int>(indent, ' ') + left + blanks.assign<int>(spaces - left.length(), ' ') + "- " + commands[word + 1]);
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
        msg.push_back("    part [n1]                 - set part ID operations");
    if (bitTest(level, part_lev))
        helpLoop(msg, partlist, 6);
    else
        msg.push_back("    vector [n1]               - vector CHANNEL, operations");
    
    if (bitTest(level, vect_lev))
        helpLoop(msg, vectlist, 6);        

    if (level <= 3)
    {
        helpLoop(msg, toplist, 4);
        msg.push_back("'*' entries need to be saved and Yoshimi restarted to activate");
    }
    
    if (synth->getRuntime().consoleMenuItem)
        // we need this in case someone is working headless
        cout << "\nset reports [n] - set report destination (1 GUI console, other stderr)\n\n";
 
    synth->cliOutput(msg, LINES);
    return true;
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
            msg.push_back("    " + left + blanks.assign<int>(12 - left.length(), ' ') + fx_presets [i].substr(0, presetsLast - 1));
        }
    }
    
    synth->cliOutput(msg, LINES);
    return done_msg;
}


int CmdInterface::effects(int level)
{
    Config &Runtime = synth->getRuntime();
    int reply = done_msg;
    int nFXavail;
    
    int category;
    int par;
    int value;
    
    string dest = "";
    bool flag;

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
    if (nFX >= nFXavail)
        nFX = nFXavail - 1; // we may have changed effects base
    if (point[0] == 0)
        return done_msg;
   
    if (isdigit(point[0]))
    {
        value = string2int(point);
        point = skipChars(point);
        if (value >= nFXavail)
            return range_msg;

        if (value != nFX)
        {
            nFX = value;
            
        }
        if (point[0] == 0)
        {
            Runtime.Log("FX number set to " + asString(nFX));
            return done_msg;
        }
    }

    if (matchnMove(1, point, "type"))
    {
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
         * of presets provies a very simple way to keep track of a
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
        synth->SetEffects(category, 8, nFX, nFXtype, 0, value);
        Runtime.Log(dest + " fx preset set to number " + asString(value));
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
    else if (matchnMove(1, point, "shift"))
    {
        if (point[0] == 0)
            return value_msg;
        value = string2int(point);
        if (value < 40)
            value = 40;
        else if(value > 88)
            value = 88;
        if(bitTest(level, part_lev))
        {
            synth->part[npart]->Pkeyshift = value;
            Runtime.Log("Key Shift set to " + asString(value) + "  (" + asString(value - 64) + ")");
            partFlag = true;
        }
        else
                synth->SetSystemValue(2, value);
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
    int reply = todo_msg;
    int tmp;
    
    if (point[0] == 0)
        return done_msg;

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
        return done_msg;
    }
    if (matchnMove(1, point, "xaxis"))
        axis = 0;
    else if (matchnMove(1, point, "yaxis"))
    {
        if (Runtime.nrpndata.vectorXaxis[chan] > 0x7f)
        {
            Runtime.Log("Vector X must be set first");
            return done_msg;
        }
        axis = 1;
    }
    if (point[0] == 0)
        return done_msg;
    
    if (matchnMove(1, point, "cc"))
    {
        if (point[0] == 0)
            return value_msg;

        tmp = string2int(point);
        if (!synth->vectorInit(axis, chan, tmp))
            synth->vectorSet(axis, chan, tmp);
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
        if (point[0] == 0)
            reply = value_msg;
        else
        {
            tmp = string2int(point);
            if (!synth->vectorInit(axis + 2, chan, tmp))
                synth->vectorSet(axis + 2, chan, tmp);
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
            int cmd = string2int(point) >> 1;
            if (cmd == 4)
                cmd = 3; // can't remember how to do this bit-wise :(
            if (cmd < 1 || cmd > 3)
                return range_msg;
            point = skipChars(point);
            if (point[0] == 0)
                return value_msg;
            tmp = string2int(point);
            if (!synth->vectorInit(axis * 2 + cmd + 7, chan, tmp))
            synth->vectorSet(axis * 2 + cmd + 7, chan, tmp);
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
        return effects(level);
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
        return effects(level);
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
        if (point[0] != 0)
        {
            tmp = string2int127(point);
            synth->SetPartChan(npart, tmp);
            if (tmp < NUM_MIDI_CHANNELS)
                Runtime.Log("Part " + asString(npart) + " set to channel " + asString(tmp));
            else
                Runtime.Log("Part " + asString(npart) + " set to no MIDI"); 
            reply = done_msg;
        }
        else
            reply = value_msg;
    }
    else if (matchnMove(1, point, "destination"))
    {
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
        if (point[0] == 0)
            return value_msg;
        tmp = string2int(point);
        if (tmp < 1 || (tmp > POLIPHONY - 20))
            return range_msg;
        else
        {
            synth->part[npart]->setkeylimit(tmp);
            Runtime.Log("Note limit set to " + asString(tmp));
            partFlag = true;
        }
        reply = done_msg;
    }
    else if (matchnMove(2, point, "min"))
    {
        if (point[0] == 0)
            return value_msg;
        tmp = string2int127(point);
        if (tmp > synth->part[npart]->Pmaxkey)
            return high_msg;
        else
        {
            synth->part[npart]->Pminkey = tmp;
            Runtime.Log("Min key set to " + asString(tmp));
            partFlag = true;
        }
        reply = done_msg;
    }
    else if (matchnMove(2, point, "max"))
    {
        if (point[0] == 0)
            return value_msg;
        tmp = string2int127(point);
        if (tmp < synth->part[npart]->Pminkey)
            return low_msg;
        else
        {
            synth->part[npart]->Pmaxkey = tmp;
            Runtime.Log("Max key set to " + asString(tmp));
            partFlag = true;
        }
        reply = done_msg;
    }
    else if (matchnMove(1, point, "mode"))
    {
        if (point[0] == 0)
            return value_msg;
        if (matchnMove(1, point, "poly"))
        {
             synth->part[npart]->Ppolymode = 1;
             synth->part[npart]->Plegatomode = 0;
             Runtime.Log("mode set to 'poly'");
        }
        else 
            if (matchnMove(1, point, "mono"))
        {
            synth->part[npart]->Ppolymode = 0;
            synth->part[npart]->Plegatomode = 0;
            Runtime.Log("mode set to 'mono'");
        }
        else if (matchnMove(1, point, "legato"))
        {
            synth->part[npart]->Ppolymode = 0;
            synth->part[npart]->Plegatomode = 1;
            Runtime.Log("mode set to 'legato'");
        }
        else
            return value_msg;
        partFlag = true;
        reply = done_msg;
    }
    else if (matchnMove(2, point, "portamento"))
    {
        if (point[0] == 0)
            return value_msg;
        if (matchnMove(1, point, "enable"))
        {
           synth->SetPartPortamento(npart, 1);
           Runtime.Log("Portamento enabled");
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
        synth->part[npart]->Pname = point;
        reply = done_msg;
        partFlag = true;
    }
    else
        reply = opp_msg;
    if (partFlag)
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePart, 0);
    return reply;
}


int CmdInterface::commandSet()
{
    Config &Runtime = synth->getRuntime();
    int reply = todo_msg;
    int tmp;
    string name;

    if (matchnMove(4, point, "yoshimi"))
    {
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
        
    else if (matchnMove(1, point, "reports"))
    {
        if (point[0] == '1')
            synth->SetSystemValue(100, 127);
        else
            synth->SetSystemValue(100, 0);
        reply = done_msg;
        Runtime.configChanged = true;
    }
    
    else if (matchnMove(1, point, "root"))
    {
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
        level = 0; // clear all first
        bitSet(level, part_lev);
        nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
        return commandPart(true);
    }
    if (matchnMove(2, point, "vector"))
    {
        level = 0; // clear all first
        bitSet(level, vect_lev);
        return commandVector();
    }
    if (level < 4 && matchnMove(3, point, "system"))
    {
        level = 1;
        matchnMove(2, point, "effects"); // clear it if given
        nFXtype = synth->sysefx[nFX]->geteffect();
        return effects(level);
    }
    if (level < 4 && matchnMove(3, point, "insert"))
    {
        level = 3;
        matchnMove(2, point, "effects"); // clear it if given
        nFXtype = synth->insefx[nFX]->geteffect();
        return effects(level);
    }
    if (bitTest(level, all_fx))
        return effects(level);
    
    tmp = volPanShift();
    if(tmp > todo_msg)
        return tmp;
    
    if (matchnMove(2, point, "program") || matchnMove(4, point, "instrument"))
    {
        if (point[0] == '0')
            synth->SetSystemValue(115, 0);
        else
            synth->SetSystemValue(115, 127);
        Runtime.configChanged = true;
        return done_msg;
    }
    else if (matchnMove(2, point, "activate"))
    {
        if (point[0] == '0')
            synth->SetSystemValue(116, 0);
        else
            synth->SetSystemValue(116, 127);
        Runtime.configChanged = true;
        return done_msg;
    }
    if (matchnMove(3, point, "ccroot"))
    {
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
        else if (matchnMove(1, point, "audio"))
        {
            name = "audio" + name;
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
        else
            return opp_msg;
        Runtime.Log("Preferred " + name);
        Runtime.configChanged = true;
        return done_msg;
    }
    else if (matchnMove(1, point, "alsa"))
    {
        if (matchnMove(1, point, "midi"))
        {
            if (point[0] != 0)
            {
                Runtime.alsaMidiDevice = (string) point;
                Runtime.Log("* ALSA MIDI set to " + Runtime.alsaMidiDevice);
                Runtime.configChanged = true;
            }
            else
                reply = value_msg;
        }
        else if (matchnMove(1, point, "audio"))
        {
            if (point[0] != 0)
            {
                Runtime.alsaAudioDevice = (string) point;
                Runtime.Log("* ALSA AUDIO set to " + Runtime.alsaAudioDevice);
                Runtime.configChanged = true;
            }
            else
                reply = value_msg;
        }
        else
            reply = opp_msg;
        if (reply == todo_msg)
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateConfig, 3);
            
    }
    else if (matchnMove(1, point, "jack"))
    {
        if (matchnMove(1, point, "midi"))
        {
            if (point[0] != 0)
            {
                Runtime.jackMidiDevice = (string) point;
                Runtime.Log("* jack MIDI set to " + Runtime.jackMidiDevice);
                Runtime.configChanged = true;
            }
            else
                reply = value_msg;
        }
        else if (matchnMove(1, point, "server"))
        {
            if (point[0] != 0)
            {
                Runtime.jackServer = (string) point;
                Runtime.Log("* Jack server set to " + Runtime.jackServer);
                Runtime.configChanged = true;
            }
            else
                reply = value_msg;
        }
        else
            reply = opp_msg;
        if (reply == todo_msg)
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
        if (matchnMove(1, point, "instruments") || matchnMove(1, point, "programs"))
        {
            if (point[0] == 0)
                ID = 255;
            else
                ID = string2int(point);
            synth->ListInstruments(ID, msg);
            synth->cliOutput(msg, LINES);
        }
        else if (matchnMove(1, point, "banks"))
        {
            if (point[0] == 0)
                ID = 255;
            else
                ID = string2int(point);
            synth->ListBanks(ID, msg);
            synth->cliOutput(msg, LINES);
        }
        else if (matchnMove(1, point, "vectors"))
        {
            synth->ListVectors(msg);
            synth->cliOutput(msg, LINES);
        }
        else if (matchnMove(1, point, "current"))
        {
            synth->ListCurrentParts(msg);
            synth->cliOutput(msg, LINES);
        }
        else if (matchnMove(1, point, "settings"))
        {
            synth->ListSettings(msg);
            synth->cliOutput(msg, LINES);
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
            reply = commandSet();
        else
        {
            replyString = "set";
            reply = what_msg;
        }
    }
    
    else if (matchnMove(2, point, "path"))
    {
        if (matchnMove(1, point, "add"))
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
                Runtime.configChanged = true;
            }
            reply = done_msg;
        }
        else if (matchnMove(2, point, "remove"))
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
                    Runtime.Log("Removed " + rootname);
                    Runtime.configChanged = true;
                }
                reply = done_msg;
            }
            else
                reply = value_msg;
        }
        else
        {
            synth->ListPaths(msg);
            synth->cliOutput(msg, LINES);
        }
    }

    else if (matchnMove(2, point, "load"))
    {
        if (matchnMove(1, point, "patchset"))
        {
            if (point[0] == 0)
                reply = value_msg;
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
                reply = value_msg;
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
        if(matchnMove(1, point, "setup"))
            synth->SetSystemValue(119, 255);
        else if (matchnMove(1, point, "patchset"))
        {
            if (point[0] == 0)
                reply = value_msg;
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
                reply = value_msg;
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
                prompt += (" part " + asString(npart));
            if (bitTest(level, all_fx))
            {
                if (!bitTest(level, part_lev))
                {
                    if (bitTest(level, ins_fx))
                        prompt += " Ins";
                    else
                        prompt += " Sys";
                }
                prompt += (" FX " + asString(nFX) + " " + fx_list[nFXtype].substr(0, 5));
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
