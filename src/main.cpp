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

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"
#include "Misc/BodyDisposal.h"
#include "Sql/ProgramBanks.h"
#include "MasterUI.h"

int main(int argc, char *argv[])
{
    int failure = 0;

    if (!Runtime.Setup(argc, argv))
    {
        failure  = 1;
        goto bail_out;
    }
    if (Runtime.showGui)
    {
        guiMaster = new MasterUI();
        if (!guiMaster)
        {
            Runtime.Log("Failed to instantiate guiMaster");
            goto bail_out;
        }
    }
    if (!(progBanks = new ProgramBanks()))
    {
        Runtime.Log("Failed to instantiate new ProgramBanks", true);
        failure  = 2;
        goto bail_out;
    }
    switch (progBanks->Setup())
    {
        case 0: // is good
            break;
        case 1:
            guiMaster->create_database = true;
            break;
        default:
            Runtime.Log("Failed to establish program bank database", true);
            failure  = 3;
            goto bail_out;
            break;
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
        failure  = 4;
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
    string msg = "Bad things happened, Yoshimi strategically retreats. ";
    switch (failure)
    {
        case 1:
            msg += "Config setup failed";
            break;
        case 2:
        case 3:
            msg += "Serious problems dealing with the instrument database";
            break;
        case 4:
            if (Runtime.audioEngine == jack_audio || Runtime.midiEngine == jack_midi)
                msg += "Is jack running?";
            break;
        default:
            break;            
    }
    Runtime.Log(msg);
    if (guiMaster)
        guiMaster->strategicRetreat(msg);
    Runtime.flushLog();
    exit(EXIT_FAILURE);
}
