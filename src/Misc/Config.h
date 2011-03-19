/*
    Config.h - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified January 2011
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <csignal>
#include <cstring>
#include <deque>
#include <list>

using namespace std;

#include "MusicIO/MusicClient.h"
#include "Misc/HistoryListItem.h"
#include "Misc/MiscFuncs.h"

typedef enum { no_audio = 0, jack_audio, alsa_audio, } audio_drivers;
typedef enum { no_midi = 0, jack_midi, alsa_midi, } midi_drivers;

class XMLwrapper;
class BodyDisposal;
class SynthEngine;

class Config : public MiscFuncs
{
    public:
        Config();
        ~Config();
        bool Setup(int argc, char **argv);
        void StartupReport(void);
        void Announce(void);
        void Usage(void);
        void Log(string msg, bool tostderr = false);
        void flushLog(void);
        void clearBankrootDirlist(void);
        void clearPresetsDirlist(void);
        void saveConfig(void);
        void saveState(void) { saveSessionData(StateFile); }
        bool stateRestore(SynthEngine *synth);
        bool restoreJsession(SynthEngine *synth);
        void setJackSessionSave(int event_type, string session_file);

        static void sigHandler(int sig);
        void setInterruptActive(void);
        void setLadi1Active(void);
        void signalCheck(void);
        void setRtprio(int prio);
        bool startThread(pthread_t *pth, void *(*thread_fn)(void*), void *arg,
                         bool schedfifo, bool lowprio);

        string addParamHistory(string file);
        string historyFilename(int index);

        string        ConfigDir;
        string        ConfigFile;
        string        paramsLoad;
        string        instrumentLoad;
        bool          restoreState;
        string        StateFile;
        string        CurrentXMZ;
        bool          restoreJackSession;
        const string  baseCmdLine;
        string        jackSessionFile;

        unsigned int  Samplerate;
        unsigned int  Buffersize;
        unsigned int  Oscilsize;

        bool          runSynth;
        bool          showGui;
        bool          showConsole;
        int           VirKeybLayout;

        audio_drivers audioEngine;
        midi_drivers  midiEngine;
        string        audioDevice;
        string        midiDevice;

        string        jackServer;
        bool          startJack;        // false
        bool          connectJackaudio; // false
        string        jackSessionUuid;

        string        alsaAudioDevice;
        unsigned int  alsaSamplerate;
        int           alsaBuffersize;

        string        alsaMidiDevice;
        string        nameTag;

        int           BankUIAutoClose;
        int           Interpolation;
        string        bankRootDirlist[MAX_BANK_ROOT_DIRS];
        string        currentBankDir;
        string        presetsDirlist[MAX_BANK_ROOT_DIRS];
        int           CheckPADsynth;
        int           rtprio;
        bool          configChanged;

        deque<HistoryListItem> ParamsHistory;
        deque<HistoryListItem>::iterator itx;
        static const unsigned short MaxParamsHistory;
        list<string> LogList;
        BodyDisposal *deadObjects;

    private:
        void loadCmdArgs(int argc, char **argv);
        bool loadConfig(void);
        bool extractConfigData(XMLwrapper *xml);
        bool extractRuntimeData(XMLwrapper *xml);
        void addConfigXML(XMLwrapper *xml);
        void addRuntimeXML(XMLwrapper *xml);
        void saveSessionData(string savefile);
        bool restoreSessionData(SynthEngine *synth, string sessionfile);
        int SSEcapability(void);
        void AntiDenormals(bool set_daz_ftz);
        void saveJackSession(void);

        static unsigned short nextHistoryIndex;
        static struct sigaction sigAction;
        int sigIntActive;
        int ladi1IntActive;
        int sse_level;
        int jsessionSave;
        const string programCmd;
        string jackSessionDir;
};

extern Config Runtime;

#endif
