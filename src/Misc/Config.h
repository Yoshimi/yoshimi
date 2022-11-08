/*
    Config.h - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2022, Will Godfrey & others

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

#include <csignal>
#include <cstring>
#include <deque>
#include <list>

#include "Misc/Alloc.h"
#include "MusicIO/MusicClient.h"
#ifdef GUI_FLTK
#include "FL/Fl.H"
#endif
#include "globals.h"

using std::string;
using std::list;

class XMLwrapper;
class SynthEngine;

class Config
{
    public:
        Config(SynthEngine *_synth, list<string>& allArgs, bool isLV2Plugin);
       ~Config();
        // shall not be copied or moved or assigned
        Config(Config&&)                 = delete;
        Config(Config const&)            = delete;
        Config& operator=(Config&&)      = delete;
        Config& operator=(Config const&) = delete;

        bool Setup(void);
        void StartupReport(const string& clientName);
        void Announce(void);
        void Usage(void);
        void Log(const string& msg, char tostderr = _SYS_::LogNormal);
    void LogError(const string& msg);
        void flushLog(void);
        bool loadPresetsList(void);
        bool savePresetsList(void);
        bool saveConfig(bool master = false);
        bool loadConfig(void);
        void restoreConfig(SynthEngine *_synth);
        bool saveSessionData(string savefile);
        bool restoreSessionData(string sessionfile);
        bool restoreJsession();
        void setJackSessionSave(int event_type, const string& session_file);

        string testCCvalue(int cc);
        string masterCCtest(int cc);

        static void sigHandler(int sig);
        void setInterruptActive(void);
        void setLadi1Active(void);
        void signalCheck(void);
        void setRtprio(int prio);
        bool startThread(pthread_t *pth, void *(*thread_fn)(void*), void *arg,
                         bool schedfifo, char lowprio, const string& name = "");
        const string& programCmd(void) { return programcommand; }

        bool isRuntimeSetupCompleted() {return bRuntimeSetupCompleted;}

        string        defaultStateName;
        string        defaultSession;
        string        ConfigFile;
        string        paramsLoad;
        string        instrumentLoad;
        int           load2part;
        string        midiLearnLoad;
        string        rootDefine;
        bool          stateChanged;
        string        StateFile;
        bool          restoreJackSession;
        string        jackSessionFile;
        int           lastXMLmajor;
        int           lastXMLminor;
        bool          oldConfig;

        static bool          showSplash;
        static bool          autoInstance;
        static unsigned int  activeInstance;
        static int           showCLIcontext;

        bool          runSynth;
        bool          isLittleEndian;
        bool          finishedCLI;
        int           VirKeybLayout;

        audio_drivers audioEngine;
        bool          engineChanged;
        midi_drivers  midiEngine;
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

        string        alsaAudioDevice;
        string        alsaMidiDevice;
        string        nameTag;

        bool          loadDefaultState;
        int           sessionStage;
        int           Interpolation;
        string        presetsDirlist[MAX_PRESETS];
        list<string> lastfileseen;
        bool          sessionSeen[TOPLEVEL::XML::ScalaMap + 1];
        bool          historyLock[TOPLEVEL::XML::ScalaMap + 1];
        int           xmlType;
        unsigned char instrumentFormat;
        int           EnableProgChange;
        bool          toConsole;
        int           consoleTextSize;
        bool          hideErrors;
        bool          showTimes;
        bool          logXMLheaders;
        bool          xmlmax;
        unsigned int  GzipCompression;

        unsigned int  Samplerate;
        bool          rateChanged;
        unsigned int  Buffersize;
        bool          bufferChanged;
        unsigned int  Oscilsize;
        bool          oscilChanged;
        bool          showGui;
        bool          guiChanged;
        bool          showCli;
        bool          cliChanged;
        bool          singlePath;
        bool          banksChecked;
        unsigned char panLaw;
        bool          configChanged;

        unsigned char handlePadSynthBuild;
        bool useLegacyPadBuild() { return handlePadSynthBuild == 0; }
        bool usePadAutoApply()   { return handlePadSynthBuild == 2; }

        int           rtprio;
        int           midi_bank_root;
        int           midi_bank_C;
        int           midi_upper_voice_C;
        bool          enable_NRPN;
        bool          ignoreResetCCs;
        bool          monitorCCin;
        bool          showLearnedCC;
        int           single_row_panel;
        int           NumAvailableParts;
        int           currentPart;
        unsigned int  currentBank;
        unsigned int  currentRoot;
        bool          bankHighlight;
        int           lastBankPart;
        int           presetsRootID;
        int           tempBank;
        int           tempRoot;
        int           noteOnSent; // note test
        int           noteOnSeen;
        int           noteOffSent;
        int           noteOffSeen;
        unsigned int  VUcount;
        unsigned char channelSwitchType;
        unsigned char channelSwitchCC;
        unsigned char channelSwitchValue;
        unsigned char nrpnL;
        unsigned char nrpnH;
        unsigned char dataL;
        unsigned char dataH;
        bool          nrpnActive;
        int           effectChange; // temporary fix

        struct{
            unsigned char Xaxis[NUM_MIDI_CHANNELS];
            unsigned char Yaxis[NUM_MIDI_CHANNELS];
            unsigned char Xfeatures[NUM_MIDI_CHANNELS];
            unsigned char Yfeatures[NUM_MIDI_CHANNELS];
            unsigned char Xcc2[NUM_MIDI_CHANNELS];
            unsigned char Ycc2[NUM_MIDI_CHANNELS];
            unsigned char Xcc4[NUM_MIDI_CHANNELS];
            unsigned char Ycc4[NUM_MIDI_CHANNELS];
            unsigned char Xcc8[NUM_MIDI_CHANNELS];
            unsigned char Ycc8[NUM_MIDI_CHANNELS];
            string Name[NUM_MIDI_CHANNELS];
            int Part;
            int Controller;
            bool Enabled[NUM_MIDI_CHANNELS];
        }vectordata;

        list<string> LogList;

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
        void applyOptions(Config*settings, list<string>& allArgs);
        void defaultPresets(void);
        bool extractBaseParameters(XMLwrapper *xml);
        bool extractConfigData(XMLwrapper *xml);
        void addConfigXML(XMLwrapper *xml);
        int SSEcapability(void);
        void AntiDenormals(bool set_daz_ftz);
        void saveJackSession(void);

        int sigIntActive;
        int ladi1IntActive;
        int sse_level;
        int jsessionSave;
        const string programcommand;
        string jackSessionDir;
        string baseConfig;
        string presetDir;

        SynthEngine *synth;
        bool bRuntimeSetupCompleted;

        friend class YoshimiLV2Plugin;

    public:
        int exitType;
};


//struct GuiThreadMsg must be allocated by caller via `new` and is freed by receiver via `delete`
class GuiThreadMsg
{
private:
    GuiThreadMsg()
    {
        data = NULL;
        length = 0;
        type = GuiThreadMsg::UNDEFINED;
    }
public:
    enum
    {
        NewSynthEngine,
        UNDEFINED = 9999
    };
    void *data; //custom data, must be static or handled by called, does nod freed by receiver
    unsigned long length; //length of data member (determined by type member, can be set to 0, if data is known struct/class)
    unsigned int index; // if there is integer data, it can be passed through index (to remove additional receiver logic)
    unsigned int type; // type of gui message (see enum above)
    static void sendMessage(void *_data, unsigned int _type, unsigned int _index)
    {
        GuiThreadMsg *msg = new GuiThreadMsg;
        msg->data = _data;
        msg->type = _type;
        msg->index = _index;
#ifdef GUI_FLTK
        Fl::awake((void *)msg); // we probably need to review all of this :(
#endif
    }
    static void processGuiMessages();
};

#endif
