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
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

using namespace std;

#include "Misc/XMLwrapper.h"
#include "MusicIO/MusicClient.h"
#include "Misc/HistoryListItem.h"

#define MAX_BANK_ROOT_DIRS 100

class Config;
class Master;

extern Config Runtime;

class Config
{
    public:
        Config();
        ~Config();

        void loadCmdArgs(int argc, char **argv);
        void StartupReport(unsigned int samplerate, int buffersize);
        void Announce(void);
        void Usage(void);
        void clearBankrootDirlist(void);
        void clearPresetsDirlist(void);
        void saveConfig(void);
        void saveState(void);
        bool restoreState(Master *synth);
        bool restoreJackSession(Master *synth);

        string addParamHistory(string file);
        string historyFilename(int index);

        static void sigHandler(int sig);
        void setInterruptActive(int sig);
        void setLadi1Active(int sig);
        void signalCheck(void);

        void setJackSessionDetail(const char *session_dir, const char *client_uuid);
        void saveJackSession(void);
        void setJackSessionSave(void);
        void setJackSessionSaveAndQuit(void);
        void setJackSessionSaveTemplate(void);


        void Log(string msg);
        void flushLog(void);

        string        ConfigFile;
        string        StateFile;
        string        CurrentXMZ;
        string        paramsLoad;
        string        instrumentLoad;
        bool          doRestoreState;
        bool          doRestoreJackSession;

        unsigned int  Samplerate;
        unsigned int  Buffersize;
        unsigned int  Oscilsize;

        bool          showGui;
        bool          showConsole;
        int           VirKeybLayout;

        string        audioDevice;
        audio_drivers audioEngine;
        string        midiDevice;
        midi_drivers  midiEngine;
        string        nameTag;

        string        jackServer;
        bool          startJack;        // default false
        bool          connectJackaudio; // default false
        string        baseCmdLine;      // for jack session
        string        jacksessionDir;
        string        jacksessionUuid;

        string        alsaMidiDevice;
        string        alsaAudioDevice;

        bool          Float32bitWavs;
        string        DefaultRecordDirectory;
        string        CurrentRecordDirectory;

        int           Interpolation;
        int           CheckPADsynth;
        int           BankUIAutoClose;
        string        bankRootDirlist[MAX_BANK_ROOT_DIRS];
        string        currentBankDir;
        string        presetsDirlist[MAX_BANK_ROOT_DIRS];

        deque<HistoryListItem> ParamsHistory;
        deque<HistoryListItem>::iterator itx;
        static const unsigned short MaxParamsHistory;
        list<string> LogList;
        vector <boost::shared_ptr<void> > dead_ptrs;
        vector <boost::shared_array<float> > dead_floats;
        void bringOutYerDead(void);

    private:
        bool loadConfig(void);
        bool extractConfigData(XMLwrapper *xml);
        bool extractRuntimeData(XMLwrapper *xml);
        void addConfigXML(XMLwrapper *xml);
        void addRuntimeXML(XMLwrapper *xml);
        void saveSessionData(string tag, string savefile);
        bool restoreSessionData(Master *synth, string sessionfile);
        int SSEcapability(void);
        void AntiDenormals(bool set_daz_ftz);
        void saveJackSessionTemplate() { };

        string homeDir;
        string yoshimiHome;

        static struct sigaction sigAction;
        static int sigIntActive;
        static int ladi1IntActive;
        int sse_level;

        static unsigned short nextHistoryIndex;

        static int jsessionSave;
        static int jsessionSaveAndQuit;
        static int jsessionSaveTemplate;
};

#endif
