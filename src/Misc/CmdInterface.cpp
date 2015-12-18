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
static string replyString = "";
static unsigned int level = 0;
static int chan = 0;
static int npart = 0;
static int nFX = 0;
static int nFXtype = 0;

string basics[] = {
    "load patchset <s>",          "load a complete set of instruments from named file",
    "save patchset <s>",          "save a complete set of instruments to named file",
    "save setup",                 "save dynamic settings",
    "paths",                      "display bank root paths",
    "path add <s>",               "add bank root path",
    "path remove <n>",            "remove bank root path ID",
    "list banks [n]",             "list banks in root ID or current",
    "list instruments [n]",       "list instruments in bank ID or current",
    "list current",               "list parts with instruments installed",
    "list vectors",               "list settings for all enabled vectors",
    "list setup",                 "show dynamic settings",
    "list effects",               "show effect types",
    "set reports [n]",            "set report destination (1 GUI console, other stderr)",
    "set root <n>",               "set current root path to ID",
    "set bank <n>",               "set current bank to ID",
    "set system effects [n]",     "set system effects for editing",
    "set insert effects [n1]",    "set insertion effects for editing",
    "  send <s>/<n2>",            "set where: master, off or part number",
    "set part [n1]",              "set part ID operations",
    "  enable",                   "enables the part",
    "  disable",                  "disables the part",
    "  volume <n2>",              "part volume",
    "  pan <n2>",                 "part panning",
    "  shift <n>",                "part key shift semitones (64 no shift)",
    "  effects [n2]",             "set part effects for editing",
    "    type <s>",               "set the effect type",
    "    send <n3> <n4>",         "send part effect to system effect n3 at volume n4",
    "  program <n2>",             "loads instrument ID",
    "  channel <n2>",             "sets MIDI channel (> 15 disables)",
    "  destination <s2>",         "sets audio destination (main, part, both)",
    "set ccroot <n>",             "set CC for root path changes (> 119 disables)",
    "set ccbank <n>",             "set CC for bank changes (0, 32, other disables)",
    "set program <n>",            "set MIDI program change (0 off, other on)",
    "set activate <n>",           "set part activate (0 off, other on)",
    "set extend <n>",             "set CC for extended program change (> 119 disables)",
    "set available <n>",          "set available parts (16, 32, 64)",
    "set volume <n>",             "set master volume",
    "set shift <n>",              "set master key shift semitones (64 no shift)",
    "set defaults",               "Restore all dynamic settings to their defaults",
    "set alsa midi <s>",          "* set name of source",
    "set alsa audio <s>",         "* set name of hardware device",
    "set jack server <s>",        "* set server name",
    "set vector [n1]",            "set vector CHANNEL, operations",
    "  [x/y] cc <n2>",            "CC n2 is used for CHANNEL X or Y axis sweep",
    "  [x/y] features <n2>",      "sets CHANNEL X or Y features",
    "  [x/y] program [l/r] <n2>", "X or Y program change ID for CHANNEL L or R part",
    "  [x/y] control <n2> <n3>",  "sets n3 CC to use for X or Y feature n2 (2, 4, 8)",
    "  off",                      "disable vector for CHANNEL",
    "stop",                       "all sound off",
    "..",                         "step back one level",
    "/",                          "step back to top level",
    "?  help",                   "list commands",
    "exit",                       "tidy up and close Yoshimi",
    "end"
};

string replies [] = {
    "OK",
    "Done",
    "Value?",
    "Which Operation?",
    " what?",
    "Out of range",
    "Unrecognised",
    "Parameter?",
    "Not at this level"
};

string fx_list [] = {
    "off",
    "reverb",
    "echo",
    "chorus",
    "phaser",
    "alienWah",
    "distortion",
    "eq",
    "dynfilter"
};

void CmdInterface::defaults()
{
    level = 0;
    chan = 0;
    npart = 0;
    nFX = 0;
    nFXtype = 0;
}

bool CmdInterface::helpList(char *point, string *commands, SynthEngine *synth)
{
    int messagelist = 0;
    switch (messagelist)
    {
        case 0:
            commands = basics;
            break;
 /*       case 0x11:
            commands = subsynth;
            break;
        case 0x21:
            commands = subsynth;
            break;
        case 0x41:
            commands = subsynth;
            break;*/
        }
        
    if (!matchnMove(1, point, "help") && !matchnMove(1, point, "?"))
        return false;

    int word = 0;
    string left;
    string right;
    string blanks;
    list<string>msg;
    msg.push_back("Commands:");
    while (commands[word] != "end")
    {
        left = commands[word];
        msg.push_back("  " + left + blanks.assign<int>(30 - left.length(), ' ') + "- " + commands[word + 1]);
        word += 2;
    }

    msg.push_back("'*' entries need to be saved and Yoshimi restarted to activate");
    if (synth->getRuntime().consoleMenuItem)
        // we need this in case someone is working headless
        cout << "\nset reports [n] - set report destination (1 GUI console, other stderr)\n\n";
 
    synth->cliOutput(msg, LINES);
    return true;
}


int CmdInterface::effects(char *point, SynthEngine *synth, int level)
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
            value = string2char(point);
        }
        if (bitTest(level, part_lev))
        {
            category = 2;
            dest = "part " + asString(npart) + " fx sent to system "
                 + asString(par) + " at " + asString((int) value);
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
                 + asString(par) + " at " + asString((int) value);
        }

        synth->SetEffects(category, 4, nFX, nFXtype, par, value);
        Runtime.Log(dest);
    }
    return reply;
}


int CmdInterface::volPanShift(char *point, SynthEngine *synth, int level)
{
    Config &Runtime = synth->getRuntime();
    int reply = ok_msg;
    int value;
    int type = 0;
    
//    synth->getRuntime().Log("Level " + asString(level));
    
    if (matchnMove(1, point, "volume"))
    {
        if (point[0] == 0)
            return value_msg;
        type = 7;
        value = string2char(point);
        switch (level)
        {
            case part_lev:
                synth->part[npart]->SetController(type, value);
                Runtime.Log("Volume set to " + asString((int)value));
                break;
            default:
                synth->SetSystemValue(type, value);
                break;
        }
        reply = done_msg;
    }
    else if (level >= part_lev && matchnMove(1, point, "pan"))
    {
        if (point[0] == 0)
            return value_msg;
       type = 10;
       value = string2char(point);
        switch (level)
        {
            case part_lev:
                synth->part[npart]->SetController(type, value);
                break;
            default:
                synth->SetSystemValue(type, value);
                break;
        }        
        reply = done_msg;
        Runtime.Log("Panning set to " + asString((int)value));
    }
    else if (matchnMove(1, point, "shift"))
    {
        if (point[0] == 0)
            return value_msg;
        type = 1;
        value = string2int(point);
        if (value < 40)
            value = 40;
        else if(value > 88)
            value = 88;
        switch (level)
        {
            case part_lev:
                synth->part[npart]->Pkeyshift = value;
                GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePart, 0);
                Runtime.Log("Key Shift set to " + asString(value) + "  (" + asString(value - 64) + ")");
                break;
            default:
                synth->SetSystemValue(2, value);
                break;
        }
        reply = done_msg;
    }

    if (level == part_lev && (type == 7 || type == 10)) // currently only volume and pan
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePanelItem, npart);
    return reply;
}


int CmdInterface::commandVector(char *point, SynthEngine *synth)
{
    static int axis;
    int reply = ok_msg;
    int tmp;
    
    if (isdigit(point[0]))
    {
        tmp = string2int(point);
        if (tmp >= NUM_MIDI_CHANNELS)
            return range_msg;
        point = skipChars(point);
        if (chan != tmp)
            chan = tmp;
        synth->getRuntime().Log("Vector channel set to " + asString(chan));
    }

    if (point[0] == 0)
        return done_msg;
    
    if (matchWord(1, point, "off"))
    {
        synth->vectorSet(127, chan, 0);
        return done_msg;
    }
    tmp = point[0] | 32; // trap upper case    
    if (tmp == 'x' || tmp == 'y')
    {
        axis = tmp - 'x';
        ++ point;
        point = skipSpace(point); // can manage with or without a space
    }
    if (matchnMove(1, point, "cc"))
    {
        if (point[0] == 0)
            reply = value_msg;
        else
        {
            tmp = string2int(point);
            if (!synth->vectorInit(axis, chan, tmp))
                synth->vectorSet(axis, chan, tmp);
            reply = done_msg;
        } 
    }
    else if (matchnMove(1, point, "features"))
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
    else if (matchnMove(1, point, "program"))
    {
        int hand = point[0] | 32;
        if (point[0] == 'l')
            hand = 0;
        else if (point[0] == 'r')
            hand = 1;
        else
            return opp_msg;
        point = skipChars(point);
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


int CmdInterface::commandPart(char *point, SynthEngine *synth, bool justSet)
{
    Config &Runtime = synth->getRuntime();
    int reply = ok_msg;
    int tmp;

    if (point[0] == 0)
        return done_msg;
    if (bitTest(level, all_fx))
        return effects(point, synth, level);
    if (justSet || isdigit(point[0]))
    {
        if (isdigit(point[0]))
        {
            tmp = string2int(point);
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
        return effects(point, synth, level);
    }
    tmp = volPanShift(point, synth, part_lev);
    if(tmp != ok_msg)
        return tmp;
    if (matchnMove(1, point, "enable"))
    {
        synth->partonoff(npart, 1);
        Runtime.Log("Part enabled");
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePanelItem, npart);
        reply = done_msg;
    }
    else if (matchnMove(1, point, "disable"))
    {
        synth->partonoff(npart, 0);
        Runtime.Log("Part disabled");
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePanelItem, npart);
        reply = done_msg;
    }
    else if (matchnMove(1, point, "program"))
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
            tmp = string2int(point);
            synth->SetPartChan(npart, tmp);
            if (tmp < NUM_MIDI_CHANNELS)
                Runtime.Log("Part " + asString((int) npart) + " set to channel " + asString(tmp));
            else
                Runtime.Log("Part " + asString((int) npart) + " set to no MIDI"); 
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
            synth->partonoff(npart, 1);
            synth->SetPartDestination(npart, dest);
            reply = done_msg;
        }
        else
            reply = range_msg;
    }
    else
        reply = opp_msg;
    
    return reply;
}


int CmdInterface::commandSet(char *point, SynthEngine *synth)
{
    Config &Runtime = synth->getRuntime();
    int reply = ok_msg;
    int tmp;

    if (matchnMove(1, point, "yoshimi"))
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
        
    if (matchnMove(3, point, "ccroot"))
    {
        if (point[0] != 0)
        {
            synth->SetSystemValue(113, string2int(point));
            reply = done_msg;
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
        }
        else
            reply = value_msg;
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
        reply = commandPart(point, synth, false);
    else if (bitTest(level, vect_lev))
        reply = commandVector(point, synth);
    if (reply > ok_msg)
        return reply;

    if (matchnMove(1, point, "part"))
    {
        level = 0; // clear all first
        bitSet(level, part_lev);
        return commandPart(point, synth, true);
    }
    if (matchnMove(2, point, "vector"))
    {
        level = 0; // clear all first
        bitSet(level, vect_lev);
        return commandVector(point, synth);
    }
    if (level < 4 && matchnMove(3, point, "sys"))
    {
        level = 1;
        matchnMove(2, point, "effects"); // clear it if given
        return effects(point, synth, level);
    }
    if (level < 4 && matchnMove(3, point, "ins"))
    {
        level = 3;
        matchnMove(2, point, "effects"); // clear it if given
        return effects(point, synth, level);
    }
    if (bitTest(level, all_fx))
        return effects(point, synth, level);
    
    tmp = volPanShift(point, synth, 0);
    if(tmp > ok_msg)
        return tmp;
    
    if (matchnMove(1, point, "program"))
    {
        if (point[0] == '0')
            synth->SetSystemValue(115, 0);
        else
            synth->SetSystemValue(115, 127);
    }
    else if (matchnMove(1, point, "activate"))
    {
        if (point[0] == '0')
            synth->SetSystemValue(116, 0);
        else
            synth->SetSystemValue(116, 127);
    }
    else if (matchnMove(1, point, "extend"))
    {
        if (point[0] != 0)
            synth->SetSystemValue(117, string2int(point));
        else
            reply = value_msg;
    }
    else if (matchnMove(1, point, "available"))
    {
        if (point[0] != 0)
            synth->SetSystemValue(118, string2int(point));
        else
            reply = value_msg;
    }
    else if (matchnMove(1, point, "reports"))
    {
        if (point[0] == '1')
            synth->SetSystemValue(100, 127);
        else
            synth->SetSystemValue(100, 0);
    }
    else if (matchWord(1, point, "defaults"))
    {
        level = 0;
        currentInstance = 0;
        synth->resetAll();
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateMaster, 0);

    }
    
    else if (matchnMove(1, point, "alsa"))
    {
        if (matchnMove(1, point, "midi"))
        {
            if (point[0] != 0)
            {
                Runtime.alsaMidiDevice = (string) point;
                Runtime.Log("* ALSA MIDI set to " + Runtime.alsaMidiDevice);
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
            }
            else
                reply = value_msg;
        }
        else
            reply = opp_msg;
        if (reply == ok_msg)
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
            }
            else
                reply = value_msg;
        }
        else
            reply = opp_msg;
        if (reply == ok_msg)
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateConfig, 2);
    }
    else
        reply = opp_msg;
    return reply; 
}


bool CmdInterface::cmdIfaceProcessCommand(char *buffer)
{
    map<SynthEngine *, MusicClient *>::iterator itSynth = synthInstances.begin();
    if (currentInstance >= (int)synthInstances.size())
    {
        currentInstance = 0;
        defaults();
    }
    for(int i = 0; i < currentInstance; i++, ++itSynth);
    SynthEngine *synth = itSynth->first;
    Config &Runtime = synth->getRuntime();

    replyString = "";
    npart = Runtime.currentPart;
    int ID;
    int reply = ok_msg;
    int tmp;
    string *commands = NULL;
    char *point = buffer;
    point = skipSpace(point); // just to be sure
    list<string> msg;

    if (matchnMove(2, point, "exit"))
    {
        Runtime.runSynth = false;
        return true;
    }
    if (point[0] == '/')
    {
        ++ point;
        point = skipSpace(point);
        level = 0;
        if (point[0] == 0)
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
    if (helpList(point, commands, synth))
        return false;
    if (matchnMove(2, point, "stop"))
        synth->allStop();
    else if (matchnMove(1, point, "list"))
    {
        if (matchnMove(1, point, "instrument"))
        {
            if (point[0] == 0)
                ID = 255;
            else
                ID = string2int(point);
            synth->ListInstruments(ID, msg);
            synth->cliOutput(msg, LINES);
        }
        else if (matchnMove(1, point, "bank"))
        {
            if (point[0] == 0)
                ID = 255;
            else
                ID = string2int(point);
            synth->ListBanks(ID, msg);
            synth->cliOutput(msg, LINES);
        }
        else if (matchnMove(1, point, "vector"))
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
        {
            for (int i = 0; i < 9; ++ i)
                Runtime.Log("  " + fx_list[i]);
        }
        else
        {
            replyString = "list";
            reply = what_msg;
        }
    }
    
    else if (matchnMove(1, point, "set"))
    {
        if (point[0] != 0)
            reply = commandSet(point, synth);
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
            }
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
                }
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
        if (matchnMove(1, point, "patchset") )
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
            }
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
                int saveResult = synth->saveXML((string) point);
                if (!saveResult)
                    Runtime.Log("Could not save " + (string) point);
                else
                    Runtime.Log((string) point + " saved");
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
    char *cCmd = NULL;
    bool exit = false;
//    string prompt = "yoshimi";
    sprintf(welcomeBuffer, "yoshimi> ");
    while(!exit)
    {
        cCmd = readline(welcomeBuffer);
        if (cCmd)
        {
            if(cCmd[0] != 0)
            {
                exit = cmdIfaceProcessCommand(cCmd);
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
                prompt += (" FX " + asString(nFX));
            }
            if (bitTest(level, vect_lev))
                prompt += (" vect " + asString(chan));
            prompt += "> ";
            sprintf(welcomeBuffer, prompt.c_str());
        }
        else
            usleep(20000);
    }
    if (write_history(hist_filename.c_str()) != 0) // writing of history file failed
    {
        perror(hist_filename.c_str());
    }
}
