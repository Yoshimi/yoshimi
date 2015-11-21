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
 
using namespace std;



extern map<SynthEngine *, MusicClient *> synthInstances;

MiscFuncs miscFuncs;

static int currentInstance = 0;

char welcomeBuffer [128];

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
    "  program <n2>",             "loads instrument ID",
    "  channel <n2>",             "sets MIDI channel (> 15 disables)",
    "  destination <n2>",         "(1 main, 2 part, 3 both)",
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
    "? / help",                   "list commands",
    "exit",                       "tidy up and close Yoshimi",
    "end"
};

string errors [] = {
    "OK",
    "Value?",
    "Which Operation",
    " what?",
    "Out of range",
    "Unrecognised",
    "Not at this level"
};


bool matchMove(char *&pnt, const char *word)
{
 bool found = miscFuncs.matchWord(pnt, word);
 if (found)
     pnt = miscFuncs.skipChars(pnt);
 return found;
}


bool helpList(char *point, string *commands, SynthEngine *synth)
{
    int level = 0;
    switch (level)
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
        
    if (!matchMove(point, "help") && !matchMove(point, "?"))
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


bool cmdIfaceProcessCommand(char *buffer)
{
    map<SynthEngine *, MusicClient *>::iterator itSynth = synthInstances.begin();
    for(int i = 0; i < currentInstance; i++, ++itSynth);
    SynthEngine *synth = itSynth->first;
    Config &Runtime = synth->getRuntime();
    
    int ID;
    int listType = 0;
    int error = 0;
    string *commands = NULL;
    char *point = buffer;
    point = miscFuncs.skipSpace(point); // just to be sure
    list<string> msg;

    if (helpList(point, commands, synth))
        return false;
    if (matchMove(point, "list"))
    {
        if (miscFuncs.matchWord(point, "bank") || miscFuncs.matchWord(point, "instrument"))
        {
            if (matchMove(point, "bank"))
                listType = 2;
            else if (matchMove(point, "instrument")) // yes we do need this second check!
                listType = 3;
                    
            if (point[0] == 0)
                ID = 255;
            else
                ID = miscFuncs.string2int(point);
            if (listType == 2)
            {
                synth->ListBanks(ID, msg);
                synth->cliOutput(msg, LINES);
            }
            else
            {
                synth->ListInstruments(ID, msg);
                synth->cliOutput(msg, LINES);
            }
            return false;
        }

        if (matchMove(point, "vector"))
        {
            synth->ListVectors(msg);
            synth->cliOutput(msg, LINES);
        }
        else if (matchMove(point, "current"))
        {
            synth->ListCurrentParts(msg);
            synth->cliOutput(msg, LINES);
        }
        else if (matchMove(point, "settings"))
        {
            synth->ListSettings(msg);
            synth->cliOutput(msg, LINES);
        }
        else
        {
            sprintf(buffer, "list");
            error = 3;
        }
    }
    
    else if (matchMove(point, "set"))
    {
        if (point[0] != 0)
            error = synth->commandSet(point);
        else
        {
            sprintf(buffer, "set");
            error = 3;
        }
    }
    
    else if (matchMove(point, "stop"))
        synth->allStop();
    else if (matchMove(point, "path"))
    {
        if (matchMove(point, "add"))
        {
            int found = synth->getBankRef().addRootDir(point);
            if (!found)
            {
                Runtime.Log("Can't find path " + (string) point);
            }
            else
            {
                GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePaths, 0);
                Runtime.Log("Added new root ID " + miscFuncs.asString(found) + " as " + (string) point);
            }
        }
        else if (matchMove(point, "remove"))
        {
            if (isdigit(point[0]))
            {
                int rootID = miscFuncs.string2int(point);
                string rootname = synth->getBankRef().getRootPath(rootID);
                if (rootname.empty())
                    Runtime.Log("Can't find path " + miscFuncs.asString(rootID));
                else
                {
                    synth->getBankRef().removeRoot(rootID);
                    GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePaths, 0);
                    Runtime.Log("Removed " + rootname);
                }
            }
            else
                error = 1;
        }
        else
        {
            synth->ListPaths(msg);
            synth->cliOutput(msg, LINES);
        }
    }

    else if (matchMove(point, "load"))
    {
        if (matchMove(point, "patchset") )
        {
            if (point[0] == 0)
                error = 1;
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
            sprintf(buffer, "load");
            error = 3;
        }
    }
    else if (matchMove(point, "save"))
        if(matchMove(point, "setup"))
            synth->SetSystemValue(119, 255);
        else if (matchMove(point, "patchset"))
        {
            if (point[0] == 0)
                error = 1;
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
            sprintf(buffer, "save");
            error = 3;
        }
    else if (matchMove(point, "exit"))
    {
        Runtime.runSynth = false;
        return true;
    }
    else
      error = 5;

    if (error == 3)
        Runtime.Log((string) buffer + errors[3]);
    else if (error)
        Runtime.Log(errors[error]);
    return false;
}


void cmdIfaceCommandLoop()
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
        }
        else
            usleep(20000);
    }
    if (write_history(hist_filename.c_str()) != 0) // writing of history file failed
    {
        perror(hist_filename.c_str());
    }
}
