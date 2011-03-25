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

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"
#include "MasterUI.h"
#include "Synth/BodyDisposal.h"

static void *guithread(void *arg);
bool startGuiThread(void);

int main(int argc, char *argv[])
{
    if (!Runtime.Setup(argc, argv))
        goto bail_out;

    if (!(synth = new SynthEngine()))
    {
        Runtime.Log("Failed to allocate Master");
        goto bail_out;
    }

    if (!(musicClient = MusicClient::newMusicClient()))
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
    if (!startGuiThread())
    {
        Runtime.Log("Failed to start main thread");
        goto bail_out;
    }

    while (Runtime.runSynth)
    {
        Runtime.signalCheck();
        Runtime.deadObjects->disposeBodies();
        if (Runtime.runSynth)
            usleep(20000); // where all the action is ...
    }
    musicClient->Close();
    delete musicClient;
    delete synth;
    Runtime.deadObjects->disposeBodies();
    Runtime.flushLog();
    exit(EXIT_SUCCESS);

bail_out:
    Runtime.runSynth = false;
    Runtime.Log("Yoshimi stages a strategic retreat :-(");
    if (musicClient)
    {
        musicClient->Close();
        delete musicClient;
    }
    Runtime.flushLog();
    exit(EXIT_FAILURE);
}

bool startGuiThread(void)
{
    if (Runtime.showGui)
    {
        if (!(guiMaster = new MasterUI()))
        {
            Runtime.Log("Failed to instantiate guiMaster");
            return false;
        }
        guiMaster->Init();
    }
    Runtime.StartupReport();
    static pthread_t pThread;
    return Runtime.startThread(&pThread, guithread, NULL, false, true);
}
static void *guithread(void *arg)
{
    usleep(10000);
    synth->Unmute();
    while (Runtime.runSynth)
    {
        if (!Runtime.LogList.empty())
        {
            if (Runtime.showGui)
                guiMaster->Log(Runtime.LogList.front());
            Runtime.LogList.pop_front();
        }
        if (Runtime.runSynth)
            Fl::wait(0.1);
    }
    if (guiMaster)
        delete guiMaster;
    return NULL;
}

