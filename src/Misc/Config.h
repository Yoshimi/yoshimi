/*
    Config.h - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2023, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of ZynAddSubFX original code.
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <atomic>
#include <csignal>
#include <cstring>
#include <bitset>
#include <deque>
#include <list>

#include "Misc/Alloc.h"
#include "Misc/InstanceManager.h"
#include "MusicIO/MusicClient.h"
#ifdef GUI_FLTK
#include "FL/Fl.H"
#endif
#include "globals.h"

using std::atomic_bool;
using std::bitset;
using std::string;
using std::list;

class XMLwrapper;
class SynthEngine;

class Config
{
    public:
        /** convenience access to the global InstanceManager */
        static InstanceManager& instances() { return InstanceManager::get(); }
        static Config&          primary()   { return instances().accessPrimaryConfig(); }


        Config(SynthEngine *_synth);
       ~Config();
        // shall not be copied or moved or assigned
        Config(Config&&)                 = delete;
        Config(Config const&)            = delete;
        Config& operator=(Config&&)      = delete;
        Config& operator=(Config const&) = delete;

        void init();
        void populateFromPrimary();
        void startupReport(string const& clientName);
        void announce();
        void usage();
        void Log(string const& msg, char tostderr = _SYS_::LogNormal);
        void LogError(string const& msg);
        void flushLog();
        bool loadPresetsList();
        bool savePresetsList();
        bool saveMasterConfig();
        bool saveInstanceConfig();
        void loadConfig();
        bool updateConfig(int control, int value);
        void restoreConfig(SynthEngine*);
        bool saveSessionData(string savefile);
        bool restoreSessionData(string sessionfile);
        bool restoreJsession();
        void setJackSessionSave(int event_type, string const& session_file);
        float getConfigLimits(CommandBlock*);

        string testCCvalue(int cc);
        string masterCCtest(int cc);

        static void sigHandler(int sig);
        void setInterruptActive();
        void setLadi1Active();
        void signalCheck();
        void setRtprio(int prio);
        using ThreadFun = void*(void*);
        bool startThread(pthread_t*, ThreadFun*, void* arg,
                         bool schedfifo, char lowprio, string const& name = "");
        string const& programCmd()     { return programcommand; }

        bool    isLV2;
        bool    isMultiFeed;        // can produce separate audio feeds for each part (Jack or LV2)
        string  defaultStateName;
        string  defaultSession;
        string  ConfigFile;
        string  paramsLoad;
        string  instrumentLoad;
        int     load2part;
        string  midiLearnLoad;
        string  rootDefine;
        uint    build_ID;
        bool    stateChanged;
        string  StateFile;
        string  remoteGuiTheme;
        bool    restoreJackSession;
        string  jackSessionFile;
        int     lastXMLmajor;
        int     lastXMLminor;
        bool    oldConfig;

        static bool        showSplash;
        static bool        singlePath;
        static bool        autoInstance;
        static bitset<32>  activeInstances;
        static int         showCLIcontext;

        uint          guiThemeID;
        string        guiTheme;

        atomic_bool   runSynth;
        bool          isLittleEndian;
        bool          finishedCLI;
        int           VirKeybLayout;

        audio_driver  audioEngine;
        bool          engineChanged;
        midi_driver   midiEngine;
        bool          midiChanged;
        int           alsaMidiType;
        string        audioDevice;
        string        midiDevice;

        string        jackServer;
        string        jackMidiDevice;
        bool          startJack;
        bool          connectJackaudio;
        bool          connectJackChanged;
        string        jackSessionUuid;
        static string globalJackSessionUuid;

        string        alsaAudioDevice;
        string        alsaMidiDevice;
        string        nameTag;

        bool          loadDefaultState;
        int           sessionStage;
        int           Interpolation;
        string        presetsDirlist[MAX_PRESETS];
        list<string>  lastfileseen;
        bool          sessionSeen[TOPLEVEL::XML::ScalaMap + 1];
        bool          historyLock[TOPLEVEL::XML::ScalaMap + 1];
        int           xmlType;
        uchar         instrumentFormat;
        int           EnableProgChange;
        bool          toConsole;
        int           consoleTextSize;
        bool          hideErrors;
        bool          showTimes;
        bool          logXMLheaders;
        bool          xmlmax;
        uint          GzipCompression;
        string        guideVersion;

        uint  Samplerate;
        bool  rateChanged;
        uint  Buffersize;
        bool  bufferChanged;
        uint  Oscilsize;
        bool  oscilChanged;
        bool  showGui;
        bool  storedGui;
        bool  guiChanged;
        bool  showCli;
        bool  storedCli;
        bool  cliChanged;
        bool  banksChecked;
        uchar panLaw;
        bool  configChanged;

        uchar handlePadSynthBuild;
        bool  useLegacyPadBuild() { return handlePadSynthBuild == 0; }
        bool  usePadAutoApply()   { return handlePadSynthBuild == 2; }

        int   rtprio;
        int   midi_bank_root;
        int   midi_bank_C;
        int   midi_upper_voice_C;
        bool  enable_NRPN;
        bool  ignoreResetCCs;
        bool  monitorCCin;
        bool  showLearnedCC;
        uint  NumAvailableParts;
        int   currentPart;
        uint  currentBank;
        uint  currentRoot;
        bool  bankHighlight;
        int   lastBankPart;
        int   presetsRootID;
        int   tempBank;
        int   tempRoot;
        int   noteOnSent; // note test
        int   noteOnSeen;
        int   noteOffSent;
        int   noteOffSeen;
        uint  VUcount;
        uchar channelSwitchType;
        uchar channelSwitchCC;
        uchar channelSwitchValue;
        uchar nrpnL;
        uchar nrpnH;
        uchar dataL;
        uchar dataH;
        bool          nrpnActive;

        struct{
            uchar Xaxis[NUM_MIDI_CHANNELS];
            uchar Yaxis[NUM_MIDI_CHANNELS];
            uchar Xfeatures[NUM_MIDI_CHANNELS];
            uchar Yfeatures[NUM_MIDI_CHANNELS];
            uchar Xcc2[NUM_MIDI_CHANNELS];
            uchar Ycc2[NUM_MIDI_CHANNELS];
            uchar Xcc4[NUM_MIDI_CHANNELS];
            uchar Ycc4[NUM_MIDI_CHANNELS];
            uchar Xcc8[NUM_MIDI_CHANNELS];
            uchar Ycc8[NUM_MIDI_CHANNELS];
            string Name[NUM_MIDI_CHANNELS];
            int Part;
            int Controller;
            bool Enabled[NUM_MIDI_CHANNELS];
        }vectordata;

        list<string> logList;

        /*
         * These replace local memory allocations that
         * were being made every time an add or sub note
         * was processed. Now global so treat with care!
         */
        Samples genTmp1;
        Samples genTmp2;
        Samples genTmp3;
        Samples genTmp4;

        // as above but for part and sys effect
        Samples genMixl;
        Samples genMixr;

    private:
        void findManual();
        static void* _findManual(void*);
        pthread_t  findManualHandle;

        void defaultPresets();
        bool initFromPersistentConfig();
        bool extractBaseParameters(XMLwrapper& xml);
        bool extractConfigData(XMLwrapper& xml);
        void addConfigXML(XMLwrapper& xml);
        void saveJackSession();

        string findHtmlManual();


        int sigIntActive;
        int ladi1IntActive;
        //int sse_level;
        int jsessionSave;
        const string programcommand;
        string jackSessionDir;
        string baseConfig;
        string presetDir;

        SynthEngine* synth;

        friend class YoshimiLV2Plugin;

    public:
        std::string manualFile;
        int exitType;
};

#endif /*CONFIG_H*/

