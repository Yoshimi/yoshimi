/*
  Config.h - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of the ZynAddSubFX original, modified January 2010
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <csignal>
#include <cstring>
#include <deque>
#include <list>

using namespace std;

#include "Misc/XMLwrapper.h"
#include "MusicIO/MusicClient.h"
#include "Misc/HistoryListItem.h"

#define MAX_BANK_ROOT_DIRS 100

class Config
{
    private:

    public:
        Config();
        ~Config();

        void loadCmdArgs(int argc, char **argv);
        void StartupReport(unsigned int samplerate, int buffersize);
        void Announce(void);
        void Usage(void);
        void clearBankrootDirlist(void);
        void clearPresetsDirlist(void);
        void SaveConfig(void);
        void SaveState(void);
        XMLwrapper *RestoreRuntimeState(void);
        string AddParamHistory(string file);
        string HistoryFilename(int index);

        static void sigHandler(int sig);
        void setInterruptActive(int sig);
        void setLadi1Active(int sig);
        void signalCheck(void);

        void Log(string msg);
        void flushLog(void);

        string        ConfigFile;
        bool          restoreState;
        string        StateFile;
        string        CurrentXMZ;
        string        paramsLoad;
        string        instrumentLoad;

        unsigned int  Samplerate;
        unsigned int  Buffersize;
        unsigned int  Oscilsize;

        bool          showGui;
        bool          showConsole;
        int           VirKeybLayout;

        audio_drivers audioEngine;
        string        alsaAudioDevice;
        string        jackServer;
        bool          startJack;  // default false
        bool          connectJackaudio; // default false
        string        audioDevice;

        midi_drivers  midiEngine;
        string        alsaMidiDevice;
        string        midiDevice;
        string        nameTag;

        bool          Float32bitWavs;
        string        DefaultRecordDirectory;
        string        CurrentRecordDirectory;

        int           BankUIAutoClose;
        int           Interpolation;
        string        bankRootDirlist[MAX_BANK_ROOT_DIRS];
        string        currentBankDir;
        string        presetsDirlist[MAX_BANK_ROOT_DIRS];
        int           CheckPADsynth;

        deque<HistoryListItem> ParamsHistory;
        deque<HistoryListItem>::iterator itx;
        static const unsigned short MaxParamsHistory;
        list<string> LogList;

    private:
        bool loadConfig(void);
        bool loadConfigData(XMLwrapper *xml);
        bool loadRuntimeData(XMLwrapper *xml);
        void addConfigXML(XMLwrapper *xml);
        void addRuntimeXML(XMLwrapper *xml);
        int SSEcapability(void);
        void AntiDenormals(bool set_daz_ftz);

        static unsigned short nextHistoryIndex;
        static struct sigaction sigAction;
        static int sigIntActive;
        static int ladi1IntActive;
        int sse_level;
};

extern Config Runtime;

#endif
