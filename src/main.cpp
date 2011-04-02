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

int main(int argc, char *argv[])
{
    if (!Runtime.Setup(argc, argv))
        goto bail_out;

    if (!(synth = new SynthEngine()))
    {
        Runtime.Log("Failed to allocate SynthEngine");
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

    if (Runtime.showGui)
    {
        if (!(guiMaster = new MasterUI()))
        {
            Runtime.Log("Failed to instantiate gui");
            goto bail_out;
        }
        guiMaster->Init();
    }

    Runtime.StartupReport();
    synth->Unmute();
    while (Runtime.runSynth)
    {
        Runtime.signalCheck();
        Runtime.deadObjects->disposeBodies();
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
