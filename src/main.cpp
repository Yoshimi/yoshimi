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

#ifdef GUI_FLTK
    #include "MasterUI.h"
    #include "UI/MiscGui.h"
    #include <FL/Fl.H>
    #include <FL/Fl_Window.H>
    #include <FL/Fl_PNG_Image.H>
    #include "UI/Splash.h"
#endif

using std::this_thread::sleep_for;
using std::chrono_literals::operator ""us;
using std::chrono_literals::operator ""ms;



// used by automated test launched via CLI
std::atomic <bool> waitForTest{false};

bool showSplash = false;
bool isSingleMaster = false;
bool splashSet = true;

#ifdef GUI_FLTK
time_t old_father_time, here_and_now;
#endif

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




static void *mainThread(void*)
{
    bool showGUI = Config::primary().showGui;
    InstanceManager& instanceManager{Config::instances()};

#ifndef GUI_FLTK
    assert (not showGUI);
#else
    const int textHeight = 15;
    const int textY = 10;
    const unsigned char lred = 0xd7;
    const unsigned char lgreen = 0xf7;
    const unsigned char lblue = 0xff;
    int winH=10, winW=50;
    int LbX=2, LbY=2, LbW=2, LbH=2;
    int timeout =3;
    std::string startup = YOSHIMI_VERSION;

    if (showGUI)
    {
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
    }

    Fl_PNG_Image pix("splash_screen_png", splashPngData, splashPngLength);
    Fl_Window winSplash(winW, winH, "yoshimi splash screen");
    Fl_Box box(0, 0, winW,winH);
    Fl_Box boxLb(LbX, LbY, LbW, LbH, startup.c_str());

    if (showGUI)
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
        if (splashSet && showGUI)
        {
            winSplash.position((Fl::w() - winSplash.w()) / 2, (Fl::h() - winSplash.h()) / 2);
        }
        else
            splashSet = false;

        winSplash.show();
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
            }// if(showGUI)
            else
#endif/*GUI_FLTK*/
            sleep_for(33333us);
        });//(End) Duty-cycle : Event-handling

    if (waitForTest)
        // After launching an automated test,
        // get out of the way and leave main thread without persisting state...
        // Test runs single threaded and we do not want to persist test state.
        return NULL;

    instanceManager.performShutdownActions();

    return NULL;
}


void *commandThread(void *)
{
    CmdInterface cli;
    cli.cmdIfaceCommandLoop();
    return 0;
}

std::string runShellCommand(std::string command)
{
    string returnLine = "";
    file::cmd2string(command, returnLine);
    //std::cout << returnLine << std::endl;
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
    std::string Home = getenv("HOME");
    std::string Config = file::loadText(Home + "/.config/yoshimi/yoshimi.config");
    if (Config.empty())
    {
        std::cout << "Config not there" << std::endl;
#ifdef GUI_FLTK
        showSplash = true;
#endif
    }
    else
    {
        int count = 0;
        int found = 0;
        while (!Config.empty() && count < 16 && found < 2)
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

        std::string firstText = runShellCommand("pgrep -o -x yoshimi");
        int firstpid = std::stoi(firstText);
        int firstTime = std::stoi(runShellCommand("ps -o etimes= -p " + firstText));
        int secondTime = std::stoi(runShellCommand("ps -o etimes= -p " + std::to_string(getpid())));
        if ((firstTime - secondTime) > 0)
        {
                kill(firstpid, SIGUSR2); // send message to 1st instance
                return 0; // exit quietly
        }
    }

    std::cout << YOSHIMI<< " " << YOSHIMI_VERSION << " is starting" << std::endl; // guaranteed start message


#ifdef GUI_FLTK
    time(&old_father_time);
    here_and_now = old_father_time;
#endif

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

        std::cout << "\nExisting config older than " << MIN_CONFIG_MAJOR << "." << MIN_CONFIG_MINOR << "\nCheck settings.\n"<< std::endl;
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
        std::cout << "Yoshimi can't start main loop!" << std::endl;
        goto bail_out;
    }

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

    if (waitForTest && threadCmd)
    {   // CLI about to launch TestRunner for automated test
        pthread_join(threadCmd, &res);
        if (res == (void *)1)
        {
            goto bail_out;
        }
    }
    bExitSuccess = true;

bail_out:
    if (Config::primary().showCli)
        tcsetattr(0, TCSANOW, &oldTerm);

    if (bExitSuccess)
    {
        int exitType = Config::primary().exitType;
        if (exitType == FORCED_EXIT)
            std::cout << "\nExit was forced :(" << std::endl;
        else
            std::cout << "\nGoodbye - Play again soon?"<< std::endl;
        exit(exitType);
    }
    else
    {
        Config::primary().Log("Those evil-natured robots are programmed to destroy us...", _SYS_::LogError);
        exit(EXIT_FAILURE);
    }
}
