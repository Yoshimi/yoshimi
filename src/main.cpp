/*
    main.cpp

    Copyright 2009-2010, Alan Calvert

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

int main(int argc, char *argv[])
{
    if (!Runtime.Setup(argc, argv))
        goto bail_out;

    if (Runtime.showGui)
    {
        guiMaster = new MasterUI();
        if (!guiMaster)
        {
            Runtime.Log("Failed to instantiate guiMaster");
            goto bail_out;
        }
    }
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
        Runtime.Log("Master init failed");
        goto bail_out;
    }
    if (!musicClient->Start())
    {
        Runtime.Log("So sad, failed to start MusicIO");
        goto bail_out;
    }
    if (Runtime.showGui)
    {
        guiMaster->Init();
        Runtime.StartupReport();
    }
    while (Runtime.runSynth)
    {
        Runtime.deadObjects->disposeBodies();
        Runtime.signalCheck();
        if (Runtime.showGui)
        {
            if (!Runtime.LogList.empty())
            { 
                guiMaster->Log(Runtime.LogList.front());
                Runtime.LogList.pop_front();
            }
            Fl::wait(0.04);
        }
        else
            usleep(40000); // where all the action is ...
    }
    musicClient->Close();
    if (Runtime.showGui)
    {
        if (guiMaster)
        {
            delete guiMaster;
            guiMaster = NULL;
        }
    }
    Runtime.flushLog();
    exit(EXIT_SUCCESS);

bail_out:
    Runtime.runSynth = false;
    usleep(33333); // contemplative pause ...
    Runtime.Log("Yoshimi stages a strategic retreat :-(");
    if (guiMaster)
        guiMaster->strategicRetreat();
    Runtime.flushLog();
    exit(EXIT_FAILURE);
}
