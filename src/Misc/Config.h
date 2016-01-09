/*
    Config.h - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2015, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of ZynAddSubFX original code, last modified January 2015
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
#include "FL/Fl.H"

class XMLwrapper;
class BodyDisposal;

class SynthEngine;

class Config : public MiscFuncs
{
    public:
        Config(SynthEngine *_synth, int argc, char **argv);
        ~Config();
        bool Setup(int argc, char **argv);
#ifndef YOSHIMI_LV2_PLUGIN
        void StartupReport(MusicClient *musicClient);
#endif
        void Announce(void);
        void Usage(void);
        void Log(string msg, bool tostderr = false);
        void flushLog(void);

        void clearPresetsDirlist(void);

        string testCCvalue(int cc);
        string masterCCtest(int cc);
        void saveConfig(void);
        void saveState() { saveSessionData(StateFile); }
        void saveState(const string statefile)  { saveSessionData(statefile); }
        bool loadState(const string statefile)
            { return restoreSessionData(statefile); }
        bool stateRestore(void)
            { return restoreSessionData(StateFile); }
        bool restoreJsession(void);
        void setJackSessionSave(int event_type, string session_file);

        static void sigHandler(int sig);
        void setInterruptActive(void);
        void setLadi1Active(void);
        void signalCheck(void);
        void setRtprio(int prio);
        bool startThread(pthread_t *pth, void *(*thread_fn)(void*), void *arg,
                         bool schedfifo, char lowprio, bool create_detached = true);

        string addParamHistory(string file);
        string historyFilename(int index);
        bool showQuestionOrCmdWarning(string guiQuestion, string cmdLineWarning, bool bForceCmdLinePositive);
        string programCmd(void) { return programcommand; }

        bool isRuntimeSetupCompleted() {return bRuntimeSetupCompleted;}

        string        ConfigDir;
        string        ConfigFile;
        string        paramsLoad;
        string        instrumentLoad;
        string        rootDefine;
        bool          restoreState;
        string        StateFile;
        string        CurrentXMZ;
        bool          restoreJackSession;
        string        jackSessionFile;

        unsigned int  Samplerate;
        unsigned int  Buffersize;
        unsigned int  Oscilsize;

        bool          runSynth;
        bool          showGui;
        bool          showCLI;
        int           VirKeybLayout;

        audio_drivers audioEngine;
        midi_drivers  midiEngine;
        string        audioDevice;
        string        midiDevice;

        string        jackServer;
        string        jackMidiDevice;
        bool          startJack;        // false
        bool          connectJackaudio; // false
        string        jackSessionUuid;

        string        alsaAudioDevice;
        string        alsaMidiDevice;
        string        nameTag;

        unsigned int  GzipCompression;
        int           Interpolation;        
        string        presetsDirlist[MAX_PRESETS];
        int           checksynthengines;
        bool          SimpleCheck;
        int           xmlType;
        int           EnableProgChange;
        bool          consoleMenuItem;
        bool          logXMLheaders;
        bool          configChanged;
        int           rtprio;
        int           midi_bank_root;
        int           midi_bank_C;
        int           midi_upper_voice_C;
        int           enable_part_on_voice_load;
        bool          monitorCCin;
        int           single_row_panel;
        int           NumAvailableParts;
        int           currentPart;
        unsigned char nrpnL;
        unsigned char nrpnH;
        unsigned char dataL;
        unsigned char dataH;
        bool          nrpnActive;

        struct IOdata{
            unsigned char vectorXaxis[NUM_MIDI_CHANNELS];
            unsigned char vectorYaxis[NUM_MIDI_CHANNELS];
            unsigned char vectorXfeatures[NUM_MIDI_CHANNELS];
            unsigned char vectorYfeatures[NUM_MIDI_CHANNELS];
            unsigned char vectorXcc2[NUM_MIDI_CHANNELS];
            unsigned char vectorYcc2[NUM_MIDI_CHANNELS];
            unsigned char vectorXcc4[NUM_MIDI_CHANNELS];
            unsigned char vectorYcc4[NUM_MIDI_CHANNELS];
            unsigned char vectorXcc8[NUM_MIDI_CHANNELS];
            unsigned char vectorYcc8[NUM_MIDI_CHANNELS];
            int Part;
            int Controller;
            bool vectorEnabled[NUM_MIDI_CHANNELS];
        };
        
        IOdata nrpndata;        
        
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
        bool restoreSessionData(string sessionfile);
        int SSEcapability(void);
        void AntiDenormals(bool set_daz_ftz);
        void saveJackSession(void);

        unsigned short nextHistoryIndex;
        int sigIntActive;
        int ladi1IntActive;
        int sse_level;
        int jsessionSave;
        const string programcommand;
        string jackSessionDir;

        SynthEngine *synth;
        bool bRuntimeSetupCompleted;

        friend class YoshimiLV2Plugin;
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
        NewSynthEngine = 0,
        UpdateMaster,
        UpdateConfig,
        UpdatePaths,
        UpdatePanel,
        UpdatePart,
        UpdatePanelItem,
        UpdatePartProgram,
        UpdateEffects,
        RegisterAudioPort,
        UpdateBankRootDirs,
        RescanForBanks,
        RefreshCurBank,
        UNDEFINED = 9999
    };
    void *data; //custom data, must be static or handled by called, does nod freed by receiver
    unsigned long length; //length of data member (determined by type member, can be set to 0, if data is known struct/class)
    unsigned int index; // if there is integer data, it can be passed through index (to remove aditional receiver logic)
    unsigned int type; // type of gui message (see enum above)
    static void sendMessage(void *_data, unsigned int _type, unsigned int _index)
    {
        GuiThreadMsg *msg = new GuiThreadMsg;
        msg->data = _data;
        msg->type = _type;
        msg->index = _index;
        Fl::awake((void *)msg);
    }
    static void processGuiMessages();
};

#endif
