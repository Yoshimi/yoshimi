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

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "MusicIO/MusicClient.h"
#include "GuiThreadUI.h"

int main(int argc, char *argv[])
{
    std::ios::sync_with_stdio(false);
    cerr.precision(2);
    set_DAZ_and_FTZ(1);

    Runtime.loadCmdArgs(argc, argv);
    if (Runtime.showGui)
    {
        guiMaster = new MasterUI();
        if (NULL == guiMaster)
        {
            Runtime.Log("Failed to instantiate guiMaster");
            goto bail_out;
        }
    }
    if (NULL == (zynMaster = new Master()))
    {
        Runtime.Log("Failed to allocate Master");
        goto bail_out;
    }

    if (NULL == (musicClient = MusicClient::newMusicClient()))
    {
        Runtime.Log("Failed to instantiate MusicClient");
        goto bail_out;
    }

    if (!(musicClient->Open()))
    {
        Runtime.Log("Failed to open MusicClient");
        goto bail_out;
    }

    if (!zynMaster->Init())
    {
        Runtime.Log("Master init failed");
        goto bail_out;
    }

    if (!startGuiThread(Runtime.showGui))
    {
        Runtime.Log("Failed to start gui thread");
        goto bail_out;
    }
    if (musicClient->Start())
    {
        if (Runtime.showGui)
            Runtime.StartupReport(musicClient->getSamplerate(),
                                  musicClient->getBuffersize());
        while (!Pexitprogram)
            usleep(16666); // where all the action is ...
        musicClient->Close();
        if (Runtime.showGui)
        {
            if (NULL != guiMaster)
            {
                stopGuiThread();
                delete guiMaster;
                guiMaster = NULL;
            }
        }
        else
            stopGuiThread();
    }
    else
    {
        Runtime.Log("So sad, failed to start MusicIO");
        goto bail_out;
    }
    Runtime.flushLog();
    set_DAZ_and_FTZ(0);
    return 0;

bail_out:
    Runtime.Log("Yoshimi stages a strategic retreat :-(");
    if (NULL != guiMaster)
    {
        guiMaster->strategicRetreat();
    }
    Runtime.flushLog();
    set_DAZ_and_FTZ(0);
    return 1;
}
