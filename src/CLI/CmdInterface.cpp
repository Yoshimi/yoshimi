/*
    CmdInterface.cpp

    Copyright 2015-2019, Will Godfrey & others.

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

#include "Misc/Bank.h"

#include "CLI/CmdInterface.h"
#include "CLI/Parser.h"
#include "Interface/TextLists.h"
#include "Misc/FormatFuncs.h"
#include "Misc/CliFuncs.h"

using std::string;
using std::ofstream;

using func::asString;

using cli::readControl;


extern SynthEngine *firstSynth;


CmdInterface::CmdInterface() :
    cCmd(nullptr)
{
}


void CmdInterface::cmdIfaceCommandLoop()
{
    // Initialise the history functionality
    // Set up the history filename
    string hist_filename;

    { // put this in a block to lose the password afterwards
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
    char welcomeBuffer [128];
    sprintf(welcomeBuffer, "yoshimi> ");
    SynthEngine* synth = firstSynth;
    while(!exit)
    {
        cCmd = readline(welcomeBuffer);
        if (cCmd)
        {
            if (strlen(cCmd) >= COMMAND_SIZE)
                std::cout << "*** Error: line too long" << std::endl;
            else if(cCmd[0] != 0)
            {
                replyString = "";
                int reply = cmdIfaceProcessCommand(cCmd);
                exit = (reply == REPLY::exit_msg);

                if (reply == REPLY::what_msg)
                    synth->getRuntime().Log(replyString + replies[REPLY::what_msg]);
                else if (reply > REPLY::done_msg)
                    synth->getRuntime().Log(replies[reply]);
                add_history(cCmd);
            }
            free(cCmd);
            cCmd = NULL;

            if (!exit)
            {
                do
                { // create enough delay for most ops to complete
                    usleep(2000);
                }
                while (synth->getRuntime().runSynth && !synth->getRuntime().finishedCLI);
            }
            if (synth->getRuntime().runSynth)
            {
                string prompt = "yoshimi";
                if (currentInstance > 0)
                    prompt += (":" + asString(currentInstance));
                int expose = readControl(synth, 0, CONFIG::control::exposeStatus, TOPLEVEL::section::config);
                if (expose == 1)
                {
                    string status = buildStatus(synth, true);
                    if (status == "" )
                        status = " Top";
                    synth->getRuntime().Log("@" + status, 1);
                }
                else if (expose == 2)
                    prompt += buildStatus(synth, true);
                prompt += "> ";
                sprintf(welcomeBuffer,"%s",prompt.c_str());
            }
        }
        if (!exit && synth->getRuntime().runSynth)
            usleep(20000);
    }

    if (write_history(hist_filename.c_str()) != 0) // writing of history file failed
    {
        perror(hist_filename.c_str());
    }
}
