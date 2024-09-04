/*
    main.cpp

    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2024, Will Godfrey & others
    Copyright 2024, Ichthyostega

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
#include <thread>
#include <atomic>

#include <readline/readline.h>
#include <readline/history.h>

#include "Misc/CmdOptions.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"
#include "CLI/CmdInterface.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/TestInvoker.h"

#ifdef GUI_FLTK
    #include "MasterUI.h"
    #include "UI/MiscGui.h"
    #include "UI/Splash.h"
#endif

using std::cout;
using std::endl;
using std::string;
using std::this_thread::sleep_for;
using std::chrono_literals::operator ""us;
using std::chrono_literals::operator ""ms;


namespace { // private global implementation state of Yoshimi main

    bool showSplash = false;
    bool isSingleMaster = false;

    //Andrew Deryabin: signal handling moved to main from Config Runtime
    //It's only suitable for single instance app support
    static struct sigaction yoshimiSigAction;
}


void yoshimiSigHandler(int sig)
{
    switch (sig)
    {
        case SIGINT:
        case SIGHUP:
        case SIGTERM:
        case SIGQUIT:
            Config::primary().setInterruptActive();
            break;

        case SIGUSR1:
            Config::primary().setLadi1Active();
            sigaction(SIGUSR1, &yoshimiSigAction, NULL);
            break;
        case SIGUSR2: // start next instance
            if (isSingleMaster)
                Config::instances().handleNewInstanceSignal();
            sigaction(SIGUSR2, &yoshimiSigAction, NULL);
            break;
        default:
            break;
    }
}

void setupSignalHandler()
{
    memset(&yoshimiSigAction, 0, sizeof(yoshimiSigAction));
    yoshimiSigAction.sa_handler = yoshimiSigHandler;
    if (sigaction(SIGUSR1, &yoshimiSigAction, NULL))
        Config::primary().Log("Setting SIGUSR1 handler failed",_SYS_::LogError);
    if (sigaction(SIGUSR2, &yoshimiSigAction, NULL))
        Config::primary().Log("Setting SIGUSR2 handler failed",_SYS_::LogError);
    if (sigaction(SIGINT, &yoshimiSigAction, NULL))
        Config::primary().Log("Setting SIGINT handler failed",_SYS_::LogError);
    if (sigaction(SIGHUP, &yoshimiSigAction, NULL))
        Config::primary().Log("Setting SIGHUP handler failed",_SYS_::LogError);
    if (sigaction(SIGTERM, &yoshimiSigAction, NULL))
        Config::primary().Log("Setting SIGTERM handler failed",_SYS_::LogError);
    if (sigaction(SIGQUIT, &yoshimiSigAction, NULL))
        Config::primary().Log("Setting SIGQUIT handler failed",_SYS_::LogError);
}




static void* mainThread(void*)
{
    bool showGUI = Config::primary().showGui;
    InstanceManager& instanceManager{Config::instances()};

#ifndef GUI_FLTK
    assert (not showGUI);
#else
    SplashScreen splash;
    if (showGUI)
    {
        if (showSplash)
            splash.showPopup();
        else
            splash.showIndicator();
        Fl::wait(10); // allow to draw the splash
    }
#endif /*GUI_FLTK*/


    instanceManager.triggerRestoreInstances();
    instanceManager.performWhileActive(
    [&](SynthEngine& synth)
        {// Duty-cycle : Event-handling

#ifdef GUI_FLTK
            if (showGUI)
            {// where all the action is ...
                MasterUI *guiMaster = synth.getGuiMaster();
                assert(guiMaster);
                if (guiMaster->masterwindow)
                    guiMaster->checkBuffer();
                Fl::wait(33333); // process GUI events
            }
            else
#endif/*GUI_FLTK*/
            sleep_for(33333us);
        });//(End) Duty-cycle : Event-handling

    if (test::TestInvoker::access().activated)
        // After launching an automated test,
        // get out of the way and leave main thread without persisting state...
        // Test runs single threaded and we do not want to persist test state.
        return NULL;

    instanceManager.performShutdownActions();

    return NULL;
}


void* commandThread(void*)
{
    CmdInterface cli;
    cli.cmdIfaceCommandLoop();
    return 0;
}

string runShellCommand(string command)
{
    string returnLine = "";
    file::cmd2string(command, returnLine);
    return returnLine;
}




/******************************//**
 * Run the Yoshimi Application
 */
int main(int argc, char *argv[])
{
    /*
     * The following is a way to quickly identify and read key config startup values
     * before the synth engine has started, or any of the normal functions have been
     * identified. The base config file is quite small and is always uncompressed
     * (regardless of settings) as it is useful to be able to read and/or manually
     * change settings under fault conditions.
     */
    string Home = getenv("HOME");
    string baseConfig = file::loadText(Home + "/.config/yoshimi/yoshimi.config");
    if (baseConfig.empty())
    {
        cout << "Missing application start-up configuration." << endl;
#ifdef GUI_FLTK
        showSplash = true;
#endif
    }
    else
    {
        int count = 0;
        int found = 0;
        while (!baseConfig.empty() && count < 16 && found < 2)
        { // no need to count beyond 16 lines!
            string line = func::nextLine(baseConfig);
            ++ count;
            if (line.find("enable_splash") != string::npos)
            {
                ++ found;
                if (line.find("yes") != string::npos)
                    showSplash = true;
            }
            if (line.find("enable_single_master") != string::npos)
            {
                ++ found;
                if (line.find("yes") != string::npos)
                    isSingleMaster = true;
            }
        }
    }

    if (isSingleMaster)
    {

        string firstText = runShellCommand("pgrep -o -x yoshimi");
        int firstpid = std::stoi(firstText);
        int firstTime = std::stoi(runShellCommand("ps -o etimes= -p " + firstText));
        int secondTime = std::stoi(runShellCommand("ps -o etimes= -p " + std::to_string(getpid())));
        if ((firstTime - secondTime) > 0)
        {
                kill(firstpid, SIGUSR2); // send message to 1st instance
                return 0; // exit quietly
        }
    }

    cout << YOSHIMI<< " " << YOSHIMI_VERSION << " is starting...\n" << endl; // guaranteed start message


    struct termios  oldTerm;
    tcgetattr(0, &oldTerm);
    bool bExitSuccess = false;
    bool mainThreadStarted = false;


    if (not Config::instances().bootPrimary(argc,argv))
    {
        goto bail_out;
    }

    if (Config::primary().oldConfig)
    {

        cout << "\nExisting config older than " << MIN_CONFIG_MAJOR << "." << MIN_CONFIG_MINOR << "\nCheck settings.\n"<< endl;
    }

    pthread_t threadMainLoop;
    pthread_attr_t pthreadAttr;
        if (pthread_attr_init(&pthreadAttr) == 0)
        {
            if (pthread_create(&threadMainLoop, &pthreadAttr, mainThread, nullptr) == 0)
                mainThreadStarted = true;
            pthread_attr_destroy(&pthreadAttr);
        }

    if (!mainThreadStarted)
    {
        cout << "Yoshimi can't start main loop!" << endl;
        goto bail_out;
    }

    setupSignalHandler();

    //create command line processing thread
    pthread_t threadCmd;

    if (Config::primary().showCli)
    {
        if (pthread_attr_init(&pthreadAttr) == 0)
        {
            if (pthread_create(&threadCmd, &pthreadAttr, commandThread, nullptr) == 0)
            {
                ;
            }
            pthread_attr_destroy(&pthreadAttr);
        }
    }

//Config::primary().Log("test normal msg");
//Config::primary().Log("test not serious",_SYS_::LogNotSerious);
//Config::primary().Log("test error msg",_SYS_::LogError);
//Config::primary().Log("test not serious error",_SYS_::LogNotSerious | _SYS_::LogError);

    void *res;
    pthread_join(threadMainLoop, &res);
    if (res == (void *)1)
    {
        goto bail_out;
    }

    Config::instances().disconnectAll();

    if (Config::instances().requestedSoundTest())
    {
        pthread_join(threadCmd, &res);
        Config::instances().launchSoundTest();
    }
    bExitSuccess = true;

bail_out:
    if (Config::primary().showCli)
        tcsetattr(0, TCSANOW, &oldTerm);

    if (bExitSuccess)
    {
        int exitType = Config::primary().exitType;
        if (exitType == FORCED_EXIT)
            cout << "\nExit was forced :(" << endl;
        else
            cout << "\nGoodbye - Play again soon?" << endl;
        exit(exitType);
    }
    else
    {
        Config::primary().Log("Those evil-natured robots are programmed to destroy us...", _SYS_::LogError);
        exit(EXIT_FAILURE);
    }
}
