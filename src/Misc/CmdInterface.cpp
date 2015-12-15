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
static string errorString = "";
static unsigned int level = 0;
static int chan = 0;
static int partnum = 0;

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
    "set reports [n]",            "set report destination (1 GUI console, other stderr)",
    "set root <n>",               "set current root path to ID",
    "set bank <n>",               "set current bank to ID",
    "set part [n1]",              "set part ID operations",
    "  enable",                   "enables the part",
    "  disable",                  "disables the part",
    "  volume <n2>",              "part volume",
    "  pan <n2>",                 "part panning",
    "  shift <n>",                "part key shift semitones (64 no shift)",
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

string errors [] = {
    "OK",
    "Done",
    "Value?",
    "Which Operation",
    " what?",
    "Out of range",
    "Unrecognised",
    "Not at this level"
};

void CmdInterface::defaults()
{
    level = 0;
    chan = 0;
    partnum = 0;
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


int CmdInterface::volPanShift(char *point, SynthEngine *synth, int level)
{
    int error = ok_msg;
    int value;
    int type = 0;
    
//    synth->getRuntime().Log("Level " + asString(level));
    
    if (matchnMove(1, point, "volume"))
    {
        if (point[0] == 0)
            return value_msg;
        type = 7;
        value = string2int(point);
        switch (level)
        {
            case part_lev:
                synth->part[partnum]->SetController(type, value);
                break;
            default:
                synth->SetSystemValue(type, value);
                break;
        }
        error = done_msg;
    }
    else if (level >= part_lev && matchnMove(1, point, "pan"))
    {
        if (point[0] == 0)
            return value_msg;
       type = 10;
       value = string2int(point);
        switch (level)
        {
            case part_lev:
                synth->part[partnum]->SetController(type, value);
                break;
            default:
                synth->SetSystemValue(type, value);
                break;
        }        
        error = done_msg;
    }
    else if (matchnMove(1, point, "shift"))
    {
        if (point[0] == 0)
            return value_msg;
        type = 1;
        value = string2int(point);
        switch (level)
        {
            case part_lev:
                synth->part[partnum]->Pkeyshift = value;
                GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePart, 0);
                break;
            default:
                synth->SetSystemValue(2, value);
                break;
        }
        error = done_msg;
    }

    if (level == part_lev && (type == 7 || type == 10)) // currently only volume and pan
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePanelItem, partnum);
    return error;
}


int CmdInterface::commandVector(char *point, SynthEngine *synth)
{
    static int axis;
    int error = ok_msg;
    int tmp;
    
    if (isdigit(point[0]))
    {
        tmp = string2int(point);
        if (tmp >= NUM_MIDI_CHANNELS)
            return range_msg;
        point = skipChars(point);
        if (chan != tmp)
        {
            chan = tmp;
            synth->getRuntime().currentChannel = chan;
        }
        synth->getRuntime().Log("Vector channel set to " + asString(chan));
    }
    else
        chan = synth->getRuntime().currentChannel;

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
            error = value_msg;
        else
        {
            tmp = string2int(point);
            if (!synth->vectorInit(axis, chan, tmp))
                synth->vectorSet(axis, chan, tmp);
            error = done_msg;
        } 
    }
    else if (matchnMove(1, point, "features"))
    {
        if (point[0] == 0)
            error = value_msg;
        else
        {
            tmp = string2int(point);
            if (!synth->vectorInit(axis + 2, chan, tmp))
                synth->vectorSet(axis + 2, chan, tmp);
            error = done_msg;
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
        error = done_msg;
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
            error = done_msg;
        }
        else
            error = value_msg;
    }
    return error;
}


int CmdInterface::commandPart(char *point, SynthEngine *synth, bool justSet)
{
    int error = ok_msg;
    int tmp;

    if (point[0] == 0)
        return done_msg;
    if (justSet || isdigit(point[0]))
    {
        if (isdigit(point[0]))
        {
            tmp = string2int(point);
            if (tmp >= synth->getRuntime().NumAvailableParts)
            {
                synth->getRuntime().Log("Part number too high");
                return done_msg;
            }
            point = skipChars(point);
            if (partnum != tmp)
            {
                partnum = tmp;
                synth->getRuntime().currentPart = partnum;
                GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateMaster, 0);
            }
            if (point[0] == 0)
            {
                synth->getRuntime().Log("Part number set to " + asString(partnum));
                return done_msg;
            }
        }
    }
    tmp = volPanShift(point, synth, part_lev);
    if(tmp != ok_msg)
        return tmp;
    if (matchnMove(1, point, "enable"))
    {
        synth->partonoff(partnum, 1);
        synth->getRuntime().Log("Part enabled");
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePanelItem, partnum);
        error = done_msg;
    }
    else if (matchnMove(1, point, "disable"))
    {
        synth->partonoff(partnum, 0);
        synth->getRuntime().Log("Part disabled");
        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePanelItem, partnum);
        error = done_msg;
    }
    else if (matchnMove(1, point, "program"))
    {
        if (point[0] != 0) // force part not channel number
        {
            synth->SetProgram(partnum | 0x80, string2int(point));
            error = done_msg;
        }
        else
            error = value_msg;
    }
    else if (matchnMove(1, point, "channel"))
    {
        if (point[0] != 0)
        {
            tmp = string2int(point);
            synth->SetPartChan(partnum, tmp);
            if (tmp < NUM_MIDI_CHANNELS)
                synth->getRuntime().Log("Part " + asString((int) partnum) + " set to channel " + asString(tmp));
            else
                synth->getRuntime().Log("Part " + asString((int) partnum) + " set to no MIDI"); 
            error = done_msg;
        }
        else
            error = value_msg;
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
            synth->partonoff(partnum, 1);
            synth->SetPartDestination(partnum, dest);
            error = done_msg;
        }
        else
            error = range_msg;
    }
    else
        error = opp_msg;
    
    return error;
}


int CmdInterface::commandSet(char *point, SynthEngine *synth)
{
    int error = ok_msg;
    int tmp;

    if (matchnMove(1, point, "yoshimi"))
    {
        if (point[0] == 0)
            return value_msg;
        tmp = string2int(point);
        if (tmp >= (int)synthInstances.size())
            error = range_msg;
        else
            currentInstance = tmp;
        return done_msg;
    }
        
    if (matchnMove(3, point, "ccroot"))
    {
        if (point[0] != 0)
        {
            synth->SetSystemValue(113, string2int(point));
            error = done_msg;
        }
        else
            error = value_msg;
    }
    else if (matchnMove(3, point, "ccbank"))
    {
        if (point[0] != 0)
        {
            synth->SetSystemValue(114, string2int(point));
            error = done_msg;
        }
        else
            error = value_msg;
    }
    else if (matchnMove(1, point, "root"))
    {
        if (point[0] != 0)
        {
            synth->SetBankRoot(string2int(point));
            error = done_msg;
        }
        else
            error = value_msg;
    }
    else if (matchnMove(1, point, "bank"))
    {
        if (point[0] != 0)
        {
            synth->SetBank(string2int(point));
            error = done_msg;
        }
        else
            error = value_msg;
    }
   
    else if (matchnMove(1, point, "fx"))
    {
        level = 1;
        if (matchnMove(1, point, "part"))
        {
            bitSet(level, part_lev);
        }
        else if (matchnMove(1, point, "insert"))
        {
            bitSet(level, ins_fx);
        }
        error = done_msg;
    }

    else if (bitTest(level, part_lev))
        error = commandPart(point, synth, false);
    else if (bitTest(level, vect_lev))
        error = commandVector(point, synth);
    else if (matchnMove(2, point, "vector"))
    {
        level = 0; // clear all first
        bitSet(level, vect_lev);
        error = commandVector(point, synth);
    }
    else if (matchnMove(1, point, "part"))
    {
        level = 0; // clear all first
        bitSet(level, part_lev);
        error = commandPart(point, synth, true);
    }
    if (error > ok_msg)
        return error;
    
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
            error = value_msg;
    }
    else if (matchnMove(1, point, "available"))
    {
        if (point[0] != 0)
            synth->SetSystemValue(118, string2int(point));
        else
            error = value_msg;
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
                synth->getRuntime().alsaMidiDevice = (string) point;
                synth->getRuntime().Log("* ALSA MIDI set to " + synth->getRuntime().alsaMidiDevice);
            }
            else
                error = value_msg;
        }
        else if (matchnMove(1, point, "audio"))
        {
            if (point[0] != 0)
            {
                synth->getRuntime().alsaAudioDevice = (string) point;
                synth->getRuntime().Log("* ALSA AUDIO set to " + synth->getRuntime().alsaAudioDevice);
            }
            else
                error = value_msg;
        }
        else
            error = opp_msg;
        if (error == ok_msg)
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateConfig, 3);
            
    }
    else if (matchnMove(1, point, "jack"))
    {
        if (matchnMove(1, point, "midi"))
        {
            if (point[0] != 0)
            {
                synth->getRuntime().jackMidiDevice = (string) point;
                synth->getRuntime().Log("* jack MIDI set to " + synth->getRuntime().jackMidiDevice);
            }
            else
                error = value_msg;
        }
        else if (matchnMove(1, point, "server"))
        {
            if (point[0] != 0)
            {
                synth->getRuntime().jackServer = (string) point;
                synth->getRuntime().Log("* Jack server set to " + synth->getRuntime().jackServer);
            }
            else
                error = value_msg;
        }
        else
            error = opp_msg;
        if (error == ok_msg)
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdateConfig, 2);
    }
    else
        error = opp_msg;
    return error; 
}


bool CmdInterface::cmdIfaceProcessCommand(char *buffer)
{
    map<SynthEngine *, MusicClient *>::iterator itSynth = synthInstances.begin();
    if (currentInstance >= (int)synthInstances.size())
        currentInstance = 0;
    for(int i = 0; i < currentInstance; i++, ++itSynth);
    SynthEngine *synth = itSynth->first;
    Config &Runtime = synth->getRuntime();

    errorString = "";
    partnum = synth->getRuntime().currentPart;
    int ID;
    int error = ok_msg;
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
        else
        {
            errorString = "list";
            error = what_msg;
        }
    }
    
    else if (matchnMove(1, point, "set"))
    {
        if (point[0] != 0)
            error = commandSet(point, synth);
        else
        {
            errorString = "set";
            error = what_msg;
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
                error = value_msg;
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
                error = value_msg;
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
            errorString = "load";
            error = what_msg;
        }
    }
    else if (matchnMove(2, point, "save"))
        if(matchnMove(1, point, "setup"))
            synth->SetSystemValue(119, 255);
        else if (matchnMove(1, point, "patchset"))
        {
            if (point[0] == 0)
                error = value_msg;
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
            errorString = "save";
            error = what_msg;
        }
    else
      error = unrecognised_msg;

    if (error == what_msg)
        Runtime.Log(errorString + errors[what_msg]);
    else if (error > done_msg)
        Runtime.Log(errors[error]);
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
            if (bitTest(level, all_fx))
            {
                prompt += " FX";
                if (!bitTest(level, part_lev))
                {
                    if (bitTest(level, ins_fx))
                        prompt += " Ins";
                    else
                        prompt += " Sys";
                }
            }
            if (bitTest(level, vect_lev))
                prompt += (" vect " + asString(chan));
            if (bitTest(level, part_lev))
                prompt += (" part " + asString(partnum));
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
