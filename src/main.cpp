/*
    main.cpp

    Copyright 2009-2011, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"
#include "MasterUI.h"
#include "Synth/BodyDisposal.h"

//global synth engine for app instance;
SynthEngine *synth = NULL;

//danvd: signal handling moved to main from Runtime
//It's only suitable for single instance app support
static struct sigaction yoshimiSigAction;

void yoshimiSigHandler(int sig)
{
    switch (sig)
    {
        case SIGINT:
        case SIGHUP:
        case SIGTERM:
        case SIGQUIT:
            Runtime.setInterruptActive();
            break;

        case SIGUSR1:
            Runtime.setLadi1Active();
            sigaction(SIGUSR1, &yoshimiSigAction, NULL);
            break;

        default:
            break;
    }
}

int main(int argc, char *argv[])
{

    synth = new SynthEngine();
    if (!synth->getRuntime().Setup(argc, argv))
        goto bail_out;

    if (!synth)
    {
        Runtime.Log("Failed to allocate SynthEngine");
        goto bail_out;
    }

    memset(&yoshimiSigAction, 0, sizeof(yoshimiSigAction));
    yoshimiSigAction.sa_handler = yoshimiSigHandler;
    if (sigaction(SIGUSR1, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGUSR1 handler failed");
    if (sigaction(SIGINT, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGINT handler failed");
    if (sigaction(SIGHUP, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGHUP handler failed");
    if (sigaction(SIGTERM, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGTERM handler failed");
    if (sigaction(SIGQUIT, &yoshimiSigAction, NULL))
        synth->getRuntime().Log("Setting SIGQUIT handler failed");

    if (!(musicClient = MusicClient::newMusicClient(synth)))
    {
        Runtime.Log("Failed to instantiate MusicClient");
        goto bail_out;
    }

    if (!(musicClient->Open()))
    {
        Runtime.Log("Failed to open MusicClient");
        goto bail_out;
    }

    if (!synth->Init(musicClient->getSamplerate(), musicClient->getBuffersize()))
    {
        Runtime.Log("SynthEngine init failed");
        goto bail_out;
    }

    if (!musicClient->Start())
    {
        Runtime.Log("Failed to start MusicIO");
        goto bail_out;
    }

    if (Runtime.showGui)
    {
        if (!(guiMaster = new MasterUI(synth)))
        {
            Runtime.Log("Failed to instantiate gui");
            goto bail_out;
        }
        guiMaster->Init();
    }

    Runtime.StartupReport();
    synth->Unmute();
    cout << "Yay! We're up and running :-)\n";
    while (Runtime.runSynth)
    {
        synth->getRuntime().signalCheck();
        synth->getRuntime().deadObjects->disposeBodies();
        if (Runtime.showGui)
        {
            for (int i = 0; !Runtime.LogList.empty() && i < 5; ++i)
            {
                guiMaster->Log(Runtime.LogList.front());
                Runtime.LogList.pop_front();
            }
            if (Runtime.runSynth)
                Fl::wait(0.033333);
        }
        else if (Runtime.runSynth)
            usleep(33333); // where all the action is ...
    }
    musicClient->Close();
    delete musicClient;
    delete synth;
    Runtime.deadObjects->disposeBodies();
    Runtime.flushLog();
    if (guiMaster)
        delete guiMaster;
    cout << "Goodbye - Play again soon?\n";
    exit(EXIT_SUCCESS);

bail_out:
    Runtime.runSynth = false;
    Runtime.Log("Yoshimi stages a strategic retreat :-(");
    if (musicClient)
    {
        musicClient->Close();
        delete musicClient;
    }
    if (synth)
        delete synth;
    if (guiMaster)
        delete guiMaster;
    Runtime.flushLog();
    exit(EXIT_FAILURE);
}
