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
#include <map>

#ifdef YOSHIMI_CMDLINE_INTERFACE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <string>
#include <sstream>
static int cmdLineFifoFD = 0;
#endif

std::map<SynthEngine *, MusicClient *> synthInstances;

static SynthEngine *firstSynth = NULL;
static Config *firstRuntime = NULL;
static int globalArgc = 0;
static char **globalArgv = NULL;

//Andrew Deryabin: signal handling moved to main from Config Runtime
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
            firstRuntime->setInterruptActive();
            break;

        case SIGUSR1:
            firstRuntime->setLadi1Active();
            sigaction(SIGUSR1, &yoshimiSigAction, NULL);
            break;

        default:
            break;
    }
}

bool mainCreateNewInstance()
{
    MasterUI *guiMaster = NULL;
    MusicClient *musicClient = NULL;
    SynthEngine *synth = new SynthEngine(globalArgc, globalArgv);
    if (!synth->getRuntime().isRuntimeSetupCompleted())
        goto bail_out;

    if (!synth)
    {
        std::cerr << "Failed to allocate SynthEngine" << std::endl;
        goto bail_out;
    }

    if (!(musicClient = MusicClient::newMusicClient(synth)))
    {
        synth->getRuntime().Log("Failed to instantiate MusicClient");
        goto bail_out;
    }

    if (!(musicClient->Open()))
    {
        synth->getRuntime().Log("Failed to open MusicClient");
        goto bail_out;
    }

    if (!synth->Init(musicClient->getSamplerate(), musicClient->getBuffersize()))
    {
        synth->getRuntime().Log("SynthEngine init failed");
        goto bail_out;
    }

    if (!musicClient->Start())
    {
        synth->getRuntime().Log("Failed to start MusicIO");
        goto bail_out;
    }

    if (synth->getRuntime().showGui)
    {
        guiMaster = synth->getGuiMaster();
        if (guiMaster == NULL)
        {
            synth->getRuntime().Log("Failed to instantiate gui");
            goto bail_out;
        }
        guiMaster->Init(musicClient->midiClientName().c_str());
    }

    synth->getRuntime().StartupReport(musicClient);
    synth->Unmute();
    if (synth->getUniqueId() == 0)
        cout << "\nYay! We're up and running :-)\n";
    else
        cout << "\nStarted "<< synth->getUniqueId() << "\n";
    synthInstances.insert(std::make_pair<SynthEngine *, MusicClient *>(synth, musicClient));
    return true;

bail_out:
    synth->getRuntime().runSynth = false;
    synth->getRuntime().Log("Yoshimi stages a strategic retreat :-(");
    if (musicClient)
    {
        musicClient->Close();
        delete musicClient;
    }
    if (synth)
    {
        synth->getRuntime().flushLog();
        delete synth;
    }

    return false;
}

#ifdef YOSHIMI_CMDLINE_INTERFACE
void *cmdLineThread(void *arg)
{
    std::map<SynthEngine *, MusicClient *>::iterator it;
    if(mkfifo(YOSHIMI_CMDLINE_FIFO_NAME, 0666) != 0)
    {
        firstRuntime->Log("Can't create fifo file for cmdline access", true);
        goto bail_out;
    }
    cmdLineFifoFD = open(YOSHIMI_CMDLINE_FIFO_NAME, O_RDWR);
    if(cmdLineFifoFD == -1)
    {
        firstRuntime->Log("Can't open fifo file for cmdline access", true);
        goto bail_out;
    }

    while(firstRuntime->runSynth)
    {
        char buf[4096];
        memset(buf, 0, sizeof(buf));
        read(cmdLineFifoFD, buf, sizeof(buf));
        printf("Received: %s\n", buf);
        istringstream iss(buf);
        vector<string> vals;
        copy(istream_iterator<string>(iss),
             istream_iterator<string>(),
             back_inserter(vals));
        size_t idx = 0;
        size_t sz = vals.size();
        while(idx < sz)
        {
            if(vals.at(idx) == "noteon")
            {
                ++idx;
                if(idx < sz)
                {
                    int instnum = atoi(vals.at(idx).c_str());
                    ++idx;
                    if(idx < sz)
                    {
                        int ch = atoi(vals.at(idx).c_str());
                        ++idx;
                        if(idx < sz)
                        {
                            int note = atoi(vals.at(idx).c_str());
                            ++idx;
                            if(idx < sz)
                            {

                                int vel = atoi(vals.at(idx).c_str());
                                ++idx;
                                for(it = synthInstances.begin(); it != synthInstances.end(); ++it)
                                {
                                    SynthEngine *_synth = it->first;
                                    if(_synth->getUniqueId() == (size_t)instnum)
                                    {
                                        _synth->NoteOn(ch, note, vel);
                                        break;
                                    }
                                }
                            }

                        }
                    }
                }
            }
            else if(vals.at(idx) == "noteoff")
            {
                ++idx;
                if(idx < sz)
                {
                    int instnum = atoi(vals.at(idx).c_str());
                    ++idx;
                    if(idx < sz)
                    {
                        int ch = atoi(vals.at(idx).c_str());
                        ++idx;
                        if(idx < sz)
                        {
                            int note = atoi(vals.at(idx).c_str());
                            ++idx;
                            for(it = synthInstances.begin(); it != synthInstances.end(); ++it)
                            {
                                SynthEngine *_synth = it->first;
                                if(_synth->getUniqueId() == (size_t)instnum)
                                {
                                    _synth->NoteOff(ch, note);
                                    break;
                                }
                            }
                        }
                    }
                }

            }
            else
                ++idx;
        }
        if(strcmp(buf, "close")); //any message to unblock read() call


    }
bail_out:
    if(cmdLineFifoFD != -1)
        close(cmdLineFifoFD);
    unlink(YOSHIMI_CMDLINE_FIFO_NAME);

    return NULL;

}
#endif

int main(int argc, char *argv[])
{
    globalArgc = argc;
    globalArgv = argv;
    bool bExitSuccess = false;
    bool bGuiWait = false;
#ifdef YOSHIMI_CMDLINE_INTERFACE
    pthread_t pCmdLine = 0;
#endif
    //MasterUI *guiMaster = NULL;
    //MusicClient *musicClient = NULL;    
    std::map<SynthEngine *, MusicClient *>::iterator it;

    if (!mainCreateNewInstance())
    {
        goto bail_out;
    }
    it = synthInstances.begin();
    firstSynth = it->first;
    //musicClient = it->second;
    //guiMaster = synth->getGuiMaster();

    firstRuntime = &firstSynth->getRuntime();

    memset(&yoshimiSigAction, 0, sizeof(yoshimiSigAction));
    yoshimiSigAction.sa_handler = yoshimiSigHandler;
    if (sigaction(SIGUSR1, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGUSR1 handler failed");
    if (sigaction(SIGINT, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGINT handler failed");
    if (sigaction(SIGHUP, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGHUP handler failed");
    if (sigaction(SIGTERM, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGTERM handler failed");
    if (sigaction(SIGQUIT, &yoshimiSigAction, NULL))
        firstSynth->getRuntime().Log("Setting SIGQUIT handler failed");

    bGuiWait = firstRuntime->showGui;
#ifdef YOSHIMI_CMDLINE_INTERFACE
    if(!firstRuntime->startThread(&pCmdLine, cmdLineThread, 0, false, 0, false))
    {
        pCmdLine = 0;
        firstRuntime->Log("Can't start cmd line thread!", true);
    }
#endif

    while (firstSynth->getRuntime().runSynth)
    {
        if(firstSynth->getUniqueId() == 0)
        {
            firstSynth->getRuntime().signalCheck();
        }

        for(it = synthInstances.begin(); it != synthInstances.end(); ++it)
        {
            SynthEngine *_synth = it->first;
            MusicClient *_client = it->second;
            _synth->getRuntime().deadObjects->disposeBodies();
            if(!_synth->getRuntime().runSynth && _synth->getUniqueId() > 0)
            {
                if (_client)
                {
                    _client->Close();
                    delete _client;
                }

                if (_synth)
                {
                    _synth->getRuntime().deadObjects->disposeBodies();
                    _synth->getRuntime().flushLog();
                    delete _synth;
                }

                synthInstances.erase(it);
                cout << "\nStopped " << _synth->getUniqueId() << "\n";
                break;
            }
            if (bGuiWait)
            {
                for (int i = 0; !_synth->getRuntime().LogList.empty() && i < 5; ++i)
                {
                    _synth->getGuiMaster()->Log(_synth->getRuntime().LogList.front());
                    _synth->getRuntime().LogList.pop_front();
                }
            }
        }

        // where all the action is ...
        if(bGuiWait)
            Fl::wait(0.033333);
        else
            usleep(33333);
    }

    cout << "\nGoodbye - Play again soon?\n";
    bExitSuccess = true;
#ifdef YOSHIMI_CMDLINE_INTERFACE
    if(pCmdLine != 0)
    {
        void *vRet = NULL;
        if(cmdLineFifoFD != -1)
            write(cmdLineFifoFD, "close", 6);
        pthread_join(pCmdLine, &vRet);
    }
#endif

bail_out:    
    for(it = synthInstances.begin(); it != synthInstances.end(); ++it)
    {
        SynthEngine *_synth = it->first;
        MusicClient *_client = it->second;
        _synth->getRuntime().runSynth = false;
        if(!bExitSuccess)
        {
            _synth->getRuntime().Log("Yoshimi stages a strategic retreat :-(");
        }

        if (_client)
        {
            _client->Close();
            delete _client;
        }

        if (_synth)
        {
            _synth->getRuntime().deadObjects->disposeBodies();
            _synth->getRuntime().flushLog();
            delete _synth;
        }
    }
    if(bExitSuccess)
        exit(EXIT_SUCCESS);
    else
        exit(EXIT_FAILURE);
}
