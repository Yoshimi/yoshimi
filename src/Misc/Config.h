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

#include "Misc/Log.h"
#include "Misc/Alloc.h"
#include "Misc/VerInfo.h"
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

class XMLtree;
class XMLStore;
class SynthEngine;


class Config
{
        // each Config instance is hard wired to a specific SynthEngine instance
        SynthEngine& synth;

    public:
        /** convenience access to the global InstanceManager */
        static InstanceManager& instances() { return InstanceManager::get(); }
        static Config&          primary()   { return instances().accessPrimaryConfig(); }


       ~Config() = default;
        Config(SynthEngine&);
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
        void flushLog();
        /** provide a Logger to delegate to this Config / runtime */
        Logger const& getLogger(){ return logHandler; }
        bool loadPresetsList();
        bool savePresetsList();
        void loadConfig();
        bool updateConfig(int control, int value);
        void initBaseConfig(XMLStore&);
        void verifyVersion(XMLStore const&);
        void maybeMigrateConfig();
        bool saveMasterConfig();
        bool saveInstanceConfig();
        bool saveSessionData(string sessionfile);
        int  saveSessionData(char** dataBuffer);
        bool restoreSessionData(string sessionfile);
        bool restoreSessionData(const char* dataBuffer, int size);
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
        uint    build_ID;
        VerInfo loadedConfigVer;
        bool    incompatibleZynFile;

        static const VerInfo VER_YOSHI_CURR, VER_ZYN_COMPAT;
        static bool is_compatible (VerInfo const&);

        static bool        showSplash;
        static bool        singlePath;
        static bool        autoInstance;
        static bitset<32>  activeInstances;
        static int         showCLIcontext;

        atomic_bool   runSynth;
        bool          finishedCLI;
        bool          isLittleEndian;
        int           virKeybLayout;

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
        string        defaultStateName;
        string        defaultSession;
        string        configFile;
        string        paramsLoad;
        string        instrumentLoad;
        uint          load2part;
        string        midiLearnLoad;
        string        rootDefine;
        string        stateFile;
        uint          guiThemeID;
        string        guiTheme;
        string        remoteGuiTheme;
        bool          restoreJackSession;
        string        jackSessionFile;
        int           sessionStage;
        int           Interpolation;
        string        presetsDirlist[MAX_PRESETS];
        list<string>  lastfileseen;
        bool          sessionSeen[TOPLEVEL::XML::ScalaMap + 1];
        bool          historyLock[TOPLEVEL::XML::ScalaMap + 1];
        int           xmlType;
        uchar         instrumentFormat;
        bool          enableProgChange;
        bool          toConsole;
        int           consoleTextSize;
        bool          hideErrors;
        bool          showTimes;
        bool          logXMLheaders;
        bool          xmlmax;
        uint          gzipCompression;
        bool          enablePartReports;
        string        guideVersion;

        uint  samplerate;
        bool  rateChanged;
        uint  buffersize;
        bool  bufferChanged;
        uint  oscilsize;
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
        bool  enableOmni;
        bool  enable_NRPN;
        bool  ignoreResetCCs;
        bool  monitorCCin;
        bool  showLearnedCC;
        uint  numAvailableParts;
        int   currentPart;
        uint  currentBank;
        uint  currentRoot;
        bool  bankHighlight;
        int   lastBankPart;
        int   presetsRootID;
        int   tempBank;
        int   tempRoot;
#ifdef REPORT_NOTES_ON_OFF
        int   noteOnSent; // note test
        int   noteOnSeen;
        int   noteOffSent;
        int   noteOffSeen;
#endif //REPORT_NOTES_ON_OFF
        uint  VUcount;
        uchar channelSwitchType;
        uchar channelSwitchCC;
        uchar channelSwitchValue;
        uchar nrpnL;
        uchar nrpnH;
        uchar dataL;
        uchar dataH;
        bool  nrpnActive;

        struct Vectordata{
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
        };
        Vectordata vectordata;

        list<string> logList;
        string manualFile;
        int exitType;

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
        pthread_t  findManual_Thread;

        void defaultPresets();
        void buildConfigLocation();
        bool initFromPersistentConfig();
        bool extractBaseParameters(XMLStore&);
        bool extractConfigData(XMLStore&);
        void capturePatchState(XMLStore&);
        bool restorePatchState(XMLStore&);
        void addConfigXML(XMLStore& xml);
        void migrateLegacyPresetsList(XMLtree&);
        void saveJackSession();

        string findHtmlManual();


        int sigIntActive;
        int ladi1IntActive;
        int jsessionSave;
        const string programcommand;
        string jackSessionDir;
        string baseConfig;
        string presetList;
        string presetDir;

        Logger logHandler;

        friend class YoshimiLV2Plugin;
};

/** Convenience function to verify Metadata of loaded XML files */
void postLoadCheck(XMLStore const&, SynthEngine&);


#endif /*CONFIG_H*/
