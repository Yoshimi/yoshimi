/*
    main.cpp

    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2017, Will Godfrey & others

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

    Modified December 2017
*/

// approx timeout in seconds.
#define SPLASH_TIME 3

#include <sys/mman.h>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>


using namespace std;

#include "Misc/Config.h"
#include "Misc/Splash.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"
#include "MasterUI.h"
#include "UI/MiscGui.h"
#include <map>
#include <list>
#include <pthread.h>
#include <semaphore.h>
#include <cstdio>
#include <unistd.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_PNG_Image.H>

#include <readline/readline.h>
#include <readline/history.h>
#include <Interface/CmdInterface.h>

CmdInterface commandInt;

void mainRegisterAudioPort(SynthEngine *s, int portnum);

map<SynthEngine *, MusicClient *> synthInstances;
list<string> splashMessages;

static SynthEngine *firstSynth = NULL;
static Config *firstRuntime = NULL;
static int globalArgc = 0;
static char **globalArgv = NULL;
bool bShowGui = true;
bool bShowCmdLine = true;
bool splashSet = true;
time_t old_father_time, here_and_now;

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

static void *mainGuiThread(void *arg)
{
    Fl::lock();

    sem_post((sem_t *)arg);

    map<SynthEngine *, MusicClient *>::iterator it;

    const int textHeight = 15;
    const int textY = 10;
    const unsigned char lred = 0xd7;
    const unsigned char lgreen = 0xf7;
    const unsigned char lblue = 0xff;

    Fl_PNG_Image pix("splash_screen_png", splashPngData, splashPngLength);
    Fl_Window winSplash(splashWidth, splashHeight, "yoshimi splash screen");
    Fl_Box box(0, 0, splashWidth,splashHeight);
    box.image(pix);
    string startup = YOSHIMI_VERSION;
    startup = "V " + startup;
    Fl_Box boxLb(0, splashHeight - textY - textHeight, splashWidth, textHeight, startup.c_str());
    boxLb.box(FL_NO_BOX);
    boxLb.align(FL_ALIGN_CENTER);
    boxLb.labelsize(textHeight);
    boxLb.labeltype(FL_NORMAL_LABEL);
    boxLb.labelcolor(fl_rgb_color(lred, lgreen, lblue));
    boxLb.labelfont(FL_HELVETICA | FL_BOLD);
    winSplash.border(false);
    if (splashSet && bShowGui && firstRuntime->showSplash)
    {
        winSplash.position((Fl::w() - winSplash.w()) / 2, (Fl::h() - winSplash.h()) / 2);
    }
    else
        splashSet = false;
    do
    {
            usleep(33333);
    }
    while (firstSynth == NULL); // just wait

    GuiThreadMsg::sendMessage(firstSynth, GuiThreadMsg::NewSynthEngine, 0);

    while (firstSynth->getRuntime().runSynth)
    {
        if (firstSynth->getUniqueId() == 0)
        {
            firstSynth->getRuntime().signalCheck();
        }

        for (it = synthInstances.begin(); it != synthInstances.end(); ++it)
        {
            SynthEngine *_synth = it->first;
            MusicClient *_client = it->second;
            if (!_synth->getRuntime().runSynth && _synth->getUniqueId() > 0)
            {
                if (_synth->getRuntime().configChanged)
                {
                    size_t tmpRoot = _synth->ReadBankRoot();
                    size_t tmpBank = _synth->ReadBank();
                    _synth->getRuntime().loadConfig(); // restore old settings
                    _synth->RootBank(tmpRoot, tmpBank); // but keep current root and bank
                }
                _synth->getRuntime().saveConfig();
                int tmpID =  _synth->getUniqueId();
                if (_client)
                {
                    _client->Close();
                    delete _client;
                }

                if (_synth)
                {
                    _synth->saveBanks(tmpID);
                    _synth->getRuntime().flushLog();
                    delete _synth;
                }

                synthInstances.erase(it);
                cout << "\nStopped " << tmpID << "\n";
                break;
            }
            if (bShowGui)
            {
                for (int i = 0; !_synth->getRuntime().LogList.empty() && i < 5; ++i)
                {
                    MasterUI *guiMaster = _synth->getGuiMaster(false);
                    if (guiMaster)
                    { guiMaster->Log(_synth->getRuntime().LogList.front());
                        _synth->getRuntime().LogList.pop_front();
                    }
                }
            }
        }

        // where all the action is ...
        if (bShowGui)
        {
            if (splashSet)
            {
                winSplash.show();
                usleep(1000);
                if(time(&here_and_now) < 0) // no time?
                    here_and_now = old_father_time + SPLASH_TIME;
                if ((here_and_now - old_father_time) >= SPLASH_TIME)
                {
                    splashSet = false;
                    winSplash.hide();
                }
            }
            Fl::wait(0.033333);
            GuiThreadMsg::processGuiMessages();
        }
        else
            usleep(33333);
    }
    if (firstSynth->getRuntime().configChanged && (bShowGui | bShowCmdLine)) // don't want this if no cli or gui
    {
        size_t tmpRoot = firstSynth->ReadBankRoot();
        size_t tmpBank = firstSynth->ReadBank();
        firstSynth->getRuntime().loadConfig(); // restore old settings
        firstSynth->RootBank(tmpRoot, tmpBank); // but keep current root and bank
    }
    firstSynth->getRuntime().saveConfig();
    firstSynth->saveHistory();
    firstSynth->saveBanks(0);
    return NULL;
}

bool mainCreateNewInstance(unsigned int forceId)
{
    MusicClient *musicClient = NULL;
    SynthEngine *synth = new SynthEngine(globalArgc, globalArgv, false, forceId);
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
        synth->setWindowTitle(musicClient->midiClientName());
        if(firstSynth != NULL) //FLTK is not ready yet - send this message later for first synth
        {
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::NewSynthEngine, 0);
        }
        if (synth->getRuntime().audioEngine < 1)
            fl_alert("Yoshimi can't find an available sound system. Running with no Audio");
        if (synth->getRuntime().midiEngine < 1)
            fl_alert("Yoshimi can't find an input system. Running with no MIDI");
    }

    synth->getRuntime().StartupReport(musicClient);
    synth->Unmute();
    if (synth->getUniqueId() == 0)
        cout << "\nYay! We're up and running :-)\n";
    else
    {
        cout << "\nStarted "<< synth->getUniqueId() << "\n";
        // following copied here for other instances
        synth->installBanks(synth->getUniqueId());
    }
    synthInstances.insert(std::make_pair(synth, musicClient));
    //register jack ports for enabled parts
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (synth->partonoffRead(npart))
            mainRegisterAudioPort(synth, npart);
    }
    return true;

bail_out:
    synth->getRuntime().runSynth = false;
    synth->getRuntime().Log("Bail: Yoshimi stages a strategic retreat :-(");
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

void *commandThread(void *arg = NULL) // silence warning
{
    commandInt.cmdIfaceCommandLoop();
    return 0;
}

int main(int argc, char *argv[])
{
    time(&old_father_time);
    here_and_now = old_father_time;
    struct termios  oldTerm;
    tcgetattr(0, &oldTerm);

    cout << "Yoshimi " << YOSHIMI_VERSION << " is starting" << endl; // guaranteed start message
    globalArgc = argc;
    globalArgv = argv;
    bool bExitSuccess = false;
    map<SynthEngine *, MusicClient *>::iterator it;
    bool guiStarted = false;
    pthread_t thr;
    pthread_attr_t attr;
    sem_t semGui;

    if (!mainCreateNewInstance(0))
    {
        goto bail_out;
    }

    it = synthInstances.begin();
    firstRuntime = &it->first->getRuntime();
    firstSynth = it->first;
    bShowGui = firstRuntime->showGui;
    bShowCmdLine = firstRuntime->showCLI;

    if (firstRuntime->oldConfig)
    {

        cout << "\nExisting config older than " << MIN_CONFIG_MAJOR << "." << MIN_CONFIG_MINOR << "\nCheck settings, save and restart.\n"<< endl;
    }
    if(sem_init(&semGui, 0, 0) == 0)
    {
        if (pthread_attr_init(&attr) == 0)
        {
            if (pthread_create(&thr, &attr, mainGuiThread, (void *)&semGui) == 0)
            {
                guiStarted = true;
            }
            pthread_attr_destroy(&attr);
        }
    }

    if (!guiStarted)
    {
        cout << "Yoshimi can't start main gui loop!" << endl;
        goto bail_out;
    }
    sem_wait(&semGui);
    sem_destroy(&semGui);

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
    // following moved here for faster first synth startup
    firstSynth->loadHistory();
    firstSynth->installBanks(0);
    GuiThreadMsg::sendMessage(firstSynth, GuiThreadMsg::RefreshCurBank, 1);

    //create command line processing thread
    pthread_t cmdThr;
    if(bShowCmdLine)
    {
        if (pthread_attr_init(&attr) == 0)
        {
            if (pthread_create(&cmdThr, &attr, commandThread, (void *)firstSynth) == 0)
            {

            }
            pthread_attr_destroy(&attr);
        }
    }

    void *ret;
    pthread_join(thr, &ret);
    if(ret == (void *)1)
    {
        goto bail_out;
    }
    cout << "\nGoodbye - Play again soon?\n";
    bExitSuccess = true;

bail_out:
    for (it = synthInstances.begin(); it != synthInstances.end(); ++it)
    {
        SynthEngine *_synth = it->first;
        MusicClient *_client = it->second;
        _synth->getRuntime().runSynth = false;
        if(!bExitSuccess)
        {
            _synth->getRuntime().Log("Bail: Yoshimi stages a strategic retreat :-(");
        }

        if (_client)
        {
            _client->Close();
            delete _client;
        }

        if (_synth)
        {
            _synth->getRuntime().flushLog();
            delete _synth;
        }
    }
    if(bShowCmdLine)
        tcsetattr(0, TCSANOW, &oldTerm);
    munlockall(); // just to be sure
    if (bExitSuccess)
        exit(EXIT_SUCCESS);
    else
        exit(EXIT_FAILURE);
}

void mainRegisterAudioPort(SynthEngine *s, int portnum)
{
    if (s && (portnum < NUM_MIDI_PARTS) && (portnum >= 0))
    {
        map<SynthEngine *, MusicClient *>::iterator it = synthInstances.find(s);
        if (it != synthInstances.end())
        {
            it->second->registerAudioPort(portnum);
        }
    }
}
