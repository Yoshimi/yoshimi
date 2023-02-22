/*
    main.cpp

    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2021, Will Godfrey & others

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
#include <string>
#include <list>
#include <termios.h>
#include <pthread.h>
#include <atomic>

#include <readline/readline.h>
#include <readline/history.h>

#include "Misc/CmdOptions.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"
#include "CLI/CmdInterface.h"
#include "Misc/FileMgrFuncs.h"

#ifdef GUI_FLTK
    #include "MasterUI.h"
    #include "UI/MiscGui.h"
    #include <FL/Fl.H>
    #include <FL/Fl_Window.H>
    #include <FL/Fl_PNG_Image.H>
    #include "Misc/Splash.h"
#endif

extern map<SynthEngine *, MusicClient *> synthInstances;
extern SynthEngine *firstSynth;

// used by automated test launched via CLI
std::atomic <bool> waitForTest{false};


void mainRegisterAudioPort(SynthEngine *s, int portnum);
int mainCreateNewInstance(unsigned int forceId);
Config *firstRuntime = NULL;
static std::list<std::string> globalAllArgs;
bool isSingleMaster = false;
bool bShowGui = true;
bool bShowCmdLine = true;
bool showSplash = false;
bool splashSet = true;
bool configuring = false;
#ifdef GUI_FLTK
time_t old_father_time, here_and_now;
#endif

//Andrew Deryabin: signal handling moved to main from Config Runtime
//It's only suitable for single instance app support
static struct sigaction yoshimiSigAction;


void newBlock()
{
    for (int i = 1; i < 32; ++i)
    {
        if ((firstRuntime->activeInstance >> i) & 1)
        {
            while (configuring)
                usleep(1000);
            // in case there is still an instance starting from elsewhere
            configuring = true;
            mainCreateNewInstance(i);
            configuring = false;
        }
    }
}


void newInstance()
{
    while (configuring)
        usleep(1000);
    // in case there is still an instance starting from elsewhere
    configuring = true;
    startInstance = 0x81;
}


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
        case SIGUSR2: // start next instance
            if (isSingleMaster)
                newInstance();
            sigaction(SIGUSR2, &yoshimiSigAction, NULL);
            break;
        default:
            break;
    }
}


static void *mainThread(void *arg)
{
#ifndef GUI_FLTK
    bShowGui = false; // just to be sure
#endif
    sem_post((sem_t *)arg);
    map<SynthEngine *, MusicClient *>::iterator it;

#ifdef GUI_FLTK
    const int textHeight = 15;
    const int textY = 10;
    const unsigned char lred = 0xd7;
    const unsigned char lgreen = 0xf7;
    const unsigned char lblue = 0xff;
    int winH=10, winW=50;
    int LbX=2, LbY=2, LbW=2, LbH=2;
    int timeout =3;
    std::string startup = YOSHIMI_VERSION;

    if (bShowGui)
    {
        Fl::lock();

        if (showSplash)
        {
            startup = "V " + startup;
            winH = splashHeight;
            winW = splashWidth;
            LbX = 0;
            LbY = winH - textY - textHeight;
            LbW = winW;
            LbH = textHeight;
            timeout = 5;
        }
        else
        {
            startup = "Yoshimi V " + startup + " is starting";
            winH = 36;
            winW = 300;
            LbX = 2;
            LbY = 2;
            LbW = winW - 4;
            LbH = winH -4;
            timeout = 3;
        }
    }//bShowGui

    Fl_PNG_Image pix("splash_screen_png", splashPngData, splashPngLength);
    Fl_Window winSplash(winW, winH, "yoshimi splash screen");
    Fl_Box box(0, 0, winW,winH);
    Fl_Box boxLb(LbX, LbY, LbW, LbH, startup.c_str());

    if (bShowGui)
    {
        if (showSplash)
        {
            box.image(pix);
            boxLb.box(FL_NO_BOX);
            boxLb.align(FL_ALIGN_CENTER);
            boxLb.labelsize(textHeight);
            boxLb.labeltype(FL_NORMAL_LABEL);
            boxLb.labelcolor(fl_rgb_color(lred, lgreen, lblue));
            boxLb.labelfont(FL_HELVETICA | FL_BOLD);
        }
        else
        {
            boxLb.box(FL_EMBOSSED_FRAME);
            boxLb.labelsize(16);
            boxLb.labelfont(FL_BOLD);
            boxLb.labelcolor(0x0000e100);
        }
        winSplash.border(false);
        if (splashSet && bShowGui)
        {
            winSplash.position((Fl::w() - winSplash.w()) / 2, (Fl::h() - winSplash.h()) / 2);
        }
        else
            splashSet = false;
        do
        {
            winSplash.show();
            usleep(33333);
        }
        while (firstSynth == NULL); // just wait
    }
    else /* not bShowGui */
#endif /*GUI_FLTK*/
    {
        while (firstSynth == NULL); // just wait
    }


    if (firstRuntime->autoInstance)
        newBlock();
    while (firstRuntime->runSynth)
    {
        firstRuntime->signalCheck();

        for (it = synthInstances.begin(); it != synthInstances.end(); ++it)
        {
            SynthEngine *_synth = it->first;
            MusicClient *_client = it->second;
            if (!_synth->getRuntime().runSynth && _synth->getUniqueId() > 0)
            {
                if (_synth->getRuntime().configChanged)
                    _synth->getRuntime().restoreConfig(_synth);
                _synth->getRuntime().saveConfig();
                unsigned int instanceID =  _synth->getUniqueId();
                if (_client)
                {
                    _client->Close();
                    delete _client;
                }

                if (_synth)
                {
                    int instancebit = (1 << instanceID);
                    if (_synth->getRuntime().activeInstance & instancebit)
                        _synth->getRuntime().activeInstance -= instancebit;
                    _synth->saveBanks();
                    _synth->getRuntime().flushLog();
                    delete _synth;
                }

                synthInstances.erase(it);
                std::cout << "\nStopped " << instanceID << "\n";
                break;
            }
#ifdef GUI_FLTK
            if (bShowGui)
            {
                MasterUI *guiMaster = _synth->getGuiMaster(false);
                if (guiMaster)
                {
                    if (guiMaster->masterwindow)
                    {
                        guiMaster->checkBuffer();
                    }
                }
                else
                    GuiThreadMsg::processGuiMessages();
                Fl::wait(33333);

                if (splashSet)
                {
                    if (showSplash)
                    {
                        winSplash.show(); // keeps it in front;
                    }
                    if (time(&here_and_now) < 0) // no time?
                        here_and_now = old_father_time + timeout;
                    if ((here_and_now - old_father_time) >= timeout)
                    {
                        splashSet = false;
                        winSplash.hide();
                    }
                }

            }
#endif
        }

        // where all the action is ...
        if (startInstance > 0x80)
        {
            int testInstance = startInstance &= 0x7f;
            configuring = true;
            mainCreateNewInstance(testInstance);
            configuring = false;
            startInstance = testInstance; // to prevent repeats!
        }
        if (!bShowGui)
            usleep(33333);
    }

    if (waitForTest)
        // After launching an automated test,
        // get out of the way and leave main thread without persisting state...
        // Test runs single threaded and we do not want to persist test state.
        return NULL;

    if (firstRuntime->configChanged && (bShowGui | bShowCmdLine)) // don't want this if no cli or gui
        firstSynth->getRuntime().restoreConfig(firstSynth);

    firstRuntime->saveConfig(true);
    firstSynth->saveHistory();
    firstSynth->saveBanks();
    return NULL;
}

int mainCreateNewInstance(unsigned int forceId)
{
    MusicClient *musicClient = NULL;
    unsigned int instanceID;
    SynthEngine *synth = new SynthEngine(globalAllArgs, LV2PluginTypeNone, forceId);
    if (!synth->getRuntime().isRuntimeSetupCompleted())
        goto bail_out;
    instanceID = synth->getUniqueId();
    if (!synth)
    { // this has to be a direct call - no synth for log!
        std::cerr << "Failed to allocate SynthEngine" << std::endl;
        goto bail_out;
    }

    if (!(musicClient = MusicClient::newMusicClient(synth)))
    {
        synth->getRuntime().Log("Failed to instantiate MusicClient",_SYS_::LogError);
        goto bail_out;
    }

    if (!synth->Init(musicClient->getSamplerate(), musicClient->getBuffersize()))
    {
        synth->getRuntime().Log("SynthEngine init failed",_SYS_::LogError);
        goto bail_out;
    }

    if (!musicClient->Start())
    {
        synth->getRuntime().Log("Failed to start MusicIO",_SYS_::LogError);
        goto bail_out;
    }
#ifdef GUI_FLTK
    if (synth->getRuntime().showGui)
    {
        synth->setWindowTitle(musicClient->midiClientName());
        if (firstSynth != NULL) //FLTK is not ready yet - send this message later for first synth
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::NewSynthEngine, 0);
    }
    else
        synth->getRuntime().toConsole = false;
#else
    synth->getRuntime().toConsole = false;
#endif
    synth->getRuntime().StartupReport(musicClient->midiClientName());

    if (instanceID == 0)
        std::cout << "\nYay! We're up and running :-)\n";
    else
    {
        std::cout << "\nStarted "<< instanceID << "\n";
        // following copied here for other instances
        synth->installBanks();
    }

    synthInstances.insert(std::make_pair(synth, musicClient));
    //register jack ports for enabled parts
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (synth->partonoffRead(npart))
            mainRegisterAudioPort(synth, npart);
    }
    synth->getRuntime().activeInstance |= (1 << instanceID);
    return instanceID;

bail_out:
    synth->getRuntime().runSynth = false;
    synth->getRuntime().Log("Bail: Yoshimi stages a strategic retreat :-(",_SYS_::LogError);
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

    return -1;
}

void *commandThread(void *) // silence warning (was *arg = NULL)
{
    CmdInterface commandInit;
    commandInit.cmdIfaceCommandLoop();
    return 0;
}

std::string runCommand(std::string command)
{
    string returnLine = "";
    file::cmd2string(command, returnLine);
    //std::cout << returnLine << std::endl;
    return returnLine;
}

int main(int argc, char *argv[])
{
    /*
     * The following is a way to quickly identify and read key config startup values
     * before the synth engine has started, or any of the normal functions have been
     * identified. The base config file is quite small and is always uncompressed
     * (regardless of settings) as it is useful to be able to read and/or manually
     * change settings under fault conditions.
     */
    std::string Home = getenv("HOME");
    std::string Config = file::loadText(Home + "/.config/yoshimi/yoshimi.config");
    if (Config.empty())
    {
        std::cout << "Not there" << std::endl;
#ifdef GUI_FLTK
        showSplash = true;
#endif
    }
    else
    {
        int count = 0;
        int found = 0;
        while (!Config.empty() && count < 16 && found < 3)
        { // no need to count beyond 16 lines!
            std::string line = func::nextLine(Config);
            ++ count;
            if (line.find("enable_splash") != std::string::npos)
            {
                ++ found;
                if (line.find("yes") != std::string::npos)
                    showSplash = true;
            }
            if (line.find("enable_single_master") != std::string::npos)
            {
                ++ found;
                if (line.find("yes") != std::string::npos)
                    isSingleMaster = true;
            }
        }
    }

    if (isSingleMaster)
    {

        std::string firstText = runCommand("pgrep -o -x yoshimi");
        int firstpid = std::stoi(firstText);
        int firstTime = std::stoi(runCommand("ps -o etimes= -p " + firstText));
        int secondTime = std::stoi(runCommand("ps -o etimes= -p " + std::to_string(getpid())));
        if ((firstTime - secondTime) > 0)
        {
                kill(firstpid, SIGUSR2); // send message to 1st instance
                return 0; // exit quietly
        }
    }

    std::list<std::string> allArgs;

    int gui;
    int cmd;
    CmdOptions(argc, argv, allArgs, gui, cmd);
    globalAllArgs = allArgs;
    bool mainThreadStarted = false;

#ifdef GUI_FLTK
    time(&old_father_time);
    here_and_now = old_father_time;
#endif

    struct termios  oldTerm;
    tcgetattr(0, &oldTerm);

    std::cout << "Yoshimi " << YOSHIMI_VERSION << " is starting" << std::endl; // guaranteed start message

    bool bExitSuccess = false;
    map<SynthEngine *, MusicClient *>::iterator it;

    pthread_t threadMainLoop;
    pthread_attr_t pthreadAttr;
    sem_t semMain;

    if (mainCreateNewInstance(0) == -1)
    {
        goto bail_out;
    }

    it = synthInstances.begin();
    firstRuntime = &it->first->getRuntime();
    firstSynth = it->first;
    bShowGui = firstRuntime->showGui;
    bShowCmdLine = firstRuntime->showCli;

    if (firstRuntime->oldConfig)
    {

        std::cout << "\nExisting config older than " << MIN_CONFIG_MAJOR << "." << MIN_CONFIG_MINOR << "\nCheck settings, save and restart.\n"<< std::endl;
    }
    if (sem_init(&semMain, 0, 0) == 0)
    {
        if (pthread_attr_init(&pthreadAttr) == 0)
        {
            if (pthread_create(&threadMainLoop, &pthreadAttr, mainThread, (void *)&semMain) == 0)
                mainThreadStarted = true;
            pthread_attr_destroy(&pthreadAttr);
        }
    }

    if (!mainThreadStarted)
    {
        std::cout << "Yoshimi can't start main loop!" << std::endl;
        goto bail_out;
    }
    sem_wait(&semMain);
    sem_destroy(&semMain);

    memset(&yoshimiSigAction, 0, sizeof(yoshimiSigAction));
    yoshimiSigAction.sa_handler = yoshimiSigHandler;
    if (sigaction(SIGUSR1, &yoshimiSigAction, NULL))
        firstRuntime->Log("Setting SIGUSR1 handler failed",_SYS_::LogError);
    if (sigaction(SIGUSR2, &yoshimiSigAction, NULL))
        firstRuntime->Log("Setting SIGUSR2 handler failed",_SYS_::LogError);
    if (sigaction(SIGINT, &yoshimiSigAction, NULL))
        firstRuntime->Log("Setting SIGINT handler failed",_SYS_::LogError);
    if (sigaction(SIGHUP, &yoshimiSigAction, NULL))
        firstRuntime->Log("Setting SIGHUP handler failed",_SYS_::LogError);
    if (sigaction(SIGTERM, &yoshimiSigAction, NULL))
        firstRuntime->Log("Setting SIGTERM handler failed",_SYS_::LogError);
    if (sigaction(SIGQUIT, &yoshimiSigAction, NULL))
        firstRuntime->Log("Setting SIGQUIT handler failed",_SYS_::LogError);
    // following moved here for faster first synth startup
    firstSynth->loadHistory();
    firstSynth->installBanks();
#ifdef GUI_FLTK
    if (bShowGui)
        GuiThreadMsg::sendMessage(firstSynth, GuiThreadMsg::NewSynthEngine, 0);
#endif

    //create command line processing thread
    pthread_t threadCmd;

    if (bShowCmdLine)
    {
        if (pthread_attr_init(&pthreadAttr) == 0)
        {
            if (pthread_create(&threadCmd, &pthreadAttr, commandThread, (void *)firstSynth) == 0)
            {
                ;
            }
            pthread_attr_destroy(&pthreadAttr);
        }
    }

//firstRuntime->Log("test normal msg");
//firstRuntime->Log("test not serious",_SYS_::LogNotSerious);
//firstRuntime->Log("test error msg",_SYS_::LogError);
//firstRuntime->Log("test not serious error",_SYS_::LogNotSerious | _SYS_::LogError);

    void *res;
    pthread_join(threadMainLoop, &res);
    if (res == (void *)1)
    {
        goto bail_out;
    }
    if (waitForTest && threadCmd)
    {   // CLI about to launch TestRunner for automated test
        MusicClient* music = synthInstances[firstSynth];
        if (music) music->Close(); // causes esp. JackClient to detach

        pthread_join(threadCmd, &res);
        if (res == (void *)1)
        {
            goto bail_out;
        }
    }
    bExitSuccess = true;

bail_out:
    // firstSynth is freed in the for loop below, so save this for later
    int exitType = firstSynth->getRuntime().exitType;
    for (it = synthInstances.begin(); it != synthInstances.end(); ++it)
    {
        SynthEngine *_synth = it->first;
        MusicClient *_client = it->second;
        _synth->getRuntime().runSynth = false;
        if (!bExitSuccess)
        {
            _synth->getRuntime().Log("Bail: Yoshimi stages a strategic retreat :-(",_SYS_::LogError);
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
    if (bShowCmdLine)
        tcsetattr(0, TCSANOW, &oldTerm);
    if (bExitSuccess)
    {
        if (exitType == FORCED_EXIT)
            std::cout << "\nExit was forced :(" << std::endl;
        else
            std::cout << "\nGoodbye - Play again soon?"<< std::endl;
        exit(exitType);
    }
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
