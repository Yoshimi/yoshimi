#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
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
    "list vector [n]",            "list settings for vector CHANNEL",
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
    "mode <s>",                   "change to different menus, (addsynth, subsynth, padsynth)",
    "? / help",                   "list commands for current mode",
    "exit",                       "tidy up and close Yoshimi",
    "end"
};

string subsynth [] = {
    "volume",                     "Not yet!",
    "pan",                        "Not yet!",
    "mode <s>",                   "change to different menus, (opt1, opt2, opt3) (.. / back) (top)",
    "end"
};

string errors [] = {
    "OK",
    "Value?",
    "Which Operation",
    " what?",
    "Out of range"
};


bool matchMove(char *&pnt, const char *word)
{
 bool found = miscFuncs.matchWord(pnt, word);
 if (found)
     pnt = miscFuncs.skipChars(pnt);
 return found;
}

// developed from ideas of F. Silvain
void output(list<string>& msg_buf, unsigned int windowHeight)
{
    list<string>::iterator it;
    if (msg_buf.size() < windowHeight) // Output will fit the screen
    {
        for (it = msg_buf.begin(); it != msg_buf.end(); ++it)
            cout << *it << endl;
    }

    else // Output is too long, page it
    {
        // JBS: make that a class member variable
        string page_filename = "yoshimi-pager-" + miscFuncs.asString(getpid()) + ".log";
        ofstream fout(page_filename.c_str(),(ios_base::out | ios_base::trunc));
        for (it = msg_buf.begin(); it != msg_buf.end(); ++it)
            fout << *it << endl;
        fout.close();
        //int le = lt + windowHeight;
        string cmd = "less -X -i -M -PM\"q=quit /=search PgUp/PgDown=scroll (line %lt of %L)\" " + page_filename;
        system(cmd.c_str());
        unlink(page_filename.c_str());
    }
    msg_buf.clear();
}


bool helpList(int mode, char *point, string *commands, SynthEngine *synth)
{
    Config &Runtime = synth->getRuntime();
        
    switch (mode)
    {
        case 0:
            commands = basics;
            break;
        case 0x11:
            commands = subsynth;
            break;
        case 0x21:
            commands = subsynth;
            break;
        case 0x41:
            commands = subsynth;
            break;
        }
        
    if (!matchMove(point, "hel") && !matchMove(point, "?"))
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
    if (mode == 0)
        msg.push_back("'*' entries need to be saved and Yoshimi restarted to activate");
    output(msg, LINES);
    return true;
}


bool cmdIfaceProcessCommand(char *buffer)
{
    map<SynthEngine *, MusicClient *>::iterator itSynth = synthInstances.begin();
    for(int i = 0; i < currentInstance; i++, ++itSynth);
    SynthEngine *synth = itSynth->first;
    Config &Runtime = synth->getRuntime();
    
    static int mode;
    int ID;
    int listType = 0;
    int error = 0;
    string *commands = NULL;
    char *point = buffer;
    point = miscFuncs.skipSpace(point); // just to be sure
    list<string> msg;

    if (helpList(mode, point, commands, synth))
        return false;
    if (matchMove(point, "lis") || listType >= 2)
    {
        if (miscFuncs.matchWord(point, "ban") || miscFuncs.matchWord(point, "ins") || listType >= 2 )
        {
            if (matchMove(point, "ban"))
                listType = 2;
            else if(matchMove(point, "ins"))
                listType = 3;
                    
            if (point[0] == 0)
                ID = 255;
            else
                ID = miscFuncs.string2int(point);
            if (listType == 2)
                synth->ListBanks(ID, msg);
            else
                synth->ListInstruments(ID, msg);
            output(msg, LINES);
            return false;
        }

        if (matchMove(point, "vec"))
        {
            int chan;
            if (point[0] == 0)
                chan = Runtime.currentChannel;
            else
            {
                chan = miscFuncs.string2int(point);
                if (chan < NUM_MIDI_CHANNELS)
                    Runtime.currentChannel = chan;
                else
                    error = 4;
            }
            if (error != 4)
                synth->SetSystemValue(108, chan);
        }
        else if (matchMove(point, "set"))
        {
            synth->SetSystemValue(109, 255);
            Runtime.Log("ALSA MIDI " + Runtime.alsaMidiDevice);
            Runtime.Log("ALSA AUDIO " + Runtime.alsaAudioDevice);
            Runtime.Log("Jack server " + Runtime.jackServer);
        }
        else
        {
            sprintf(buffer, "list");
            error = 3;
        }
    }
    
    else if (matchMove(point, "sto"))
        synth->allStop();
    else if (matchMove(point, "pat"))
    {
        if (matchMove(point, "add"))
        {
            int found = synth->getBankRef().addRootDir(point);
            if (!found)
            {
                Runtime.Log("Can't find path " + (string) point);
            }
            else
                Runtime.Log("Added new root ID " + miscFuncs.asString(found) + " as " + (string) point);
        }
        else if (matchMove(point, "rem"))
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
                    Runtime.Log("Removed " + rootname);
                }
            }
            else
                error = 1;
        }
        else
            synth->SetSystemValue(110, 255);
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
    
    else if (matchMove(point, "loa"))
    {
        if (matchMove(point, "pat") )
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
    else if (matchMove(point, "sav"))
        if(matchMove(point, "set"))
            synth->SetSystemValue(119, 255);
        else if (matchMove(point, "pat"))
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
    else if (matchMove(point, "exi"))
    {
        Runtime.runSynth = false;
        return true;
    }
    else
    {
        int back = miscFuncs.matchWord(point, "..");
        if (back != 0 || matchMove(point, "mod"))
        {
            if (back != 0 || (mode > 0 && ( matchMove(point, "bac") || matchMove(point, ".."))))
            {
                if (mode & 0x0f)
                    mode = 0;
                else
                    -- mode;
            }
            else if (matchMove(point, "top"))
                mode = 0;
            else if (matchMove(point, "add"))
                mode = 0x11;
            else if (matchMove(point, "sub"))
                mode = 0x21;
            else if (matchMove(point, "pad"))
                mode = 0x41;
            string extension;
            switch (mode)
            {
                case 0:
                    extension = "";
                    break;
                case 0x11:
                    extension = "addsynth> ";
                    break;
                case 0x21:
                    extension = "subsynth> ";
                    break;
                case 0x41:
                    extension = "padsynth> ";
                    break;
            }
            sprintf(welcomeBuffer + 9, extension.c_str());
        }
    }

    if (error == 3)
        Runtime.Log((string) buffer + errors[3]);
    else if (error)
        Runtime.Log(errors[error]);
        
//    Runtime.Log("Mode " + miscFuncs.asString(mode));
    return false;
}


void cmdIfaceCommandLoop()
{
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
}
