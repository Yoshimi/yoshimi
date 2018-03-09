/*
    SynthEngine.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2018, Will Godfrey & others

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

    This file is derivative of ZynAddSubFX original code.

    Modified February 2018
*/

#ifndef SYNTHENGINE_H
#define SYNTHENGINE_H

#include <limits.h>
#include <cstdlib>
#include <semaphore.h>
#include <jack/ringbuffer.h>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/SynthHelper.h"
#include "Misc/Microtonal.h"
#include "Misc/Bank.h"
#include "Misc/SynthHelper.h"
#include "Interface/InterChange.h"
#include "Interface/MidiLearn.h"
#include "Interface/MidiDecode.h"
#include "Misc/Config.h"
#include "Params/PresetsStore.h"

typedef enum { init, lockType, unlockType, destroy } lockset;

class EffectMgr;
class Part;
class XMLwrapper;
class Controller;

class MasterUI;

class SynthEngine : private SynthHelper, MiscFuncs
{
    private:
        unsigned int uniqueId;
        bool isLV2Plugin;
        bool needsSaving;
        Bank bank;
    public:
        InterChange interchange;
        MidiLearn midilearn;
        MidiDecode mididecode;
    private:
        Config Runtime;
        PresetsStore presetsstore;
    public:
        SynthEngine(int argc, char **argv, bool _isLV2Plugin = false, unsigned int forceId = 0);
        ~SynthEngine();
        bool Init(unsigned int audiosrate, int audiobufsize);
        bool actionLock(lockset request);

        bool savePatchesXML(string filename);
        void add2XML(XMLwrapper *xml);
        string manualname();
        void defaults(void);

        bool loadXML(string filename);
        bool loadStateAndUpdate(string filename);
        bool saveState(string filename);
        bool loadPatchSetAndUpdate(string filename);
        bool loadMicrotonal(string fname);
        bool saveMicrotonal(string fname);
        bool installBanks(int instance);
        bool saveBanks(int instance);
        void newHistory(string name, int group);
        void addHistory(string name, int group);
        vector<string> *getHistory(int group);
        string lastItemSeen(int group);
        void setLastfileAdded(int group, string name);
        string getLastfileAdded(int group);
        bool loadHistory(void);
        bool saveHistory(void);
        unsigned char loadVectorAndUpdate(unsigned char baseChan, string name);
        unsigned char loadVector(unsigned char baseChan, string name, bool full);
        unsigned char extractVectorData(unsigned char baseChan, XMLwrapper *xml, string name);
        unsigned char saveVector(unsigned char baseChan, string name, bool full);
        bool insertVectorData(unsigned char baseChan, bool full, XMLwrapper *xml, string name);

        bool getfromXML(XMLwrapper *xml);

        int getalldata(char **data);
        void putalldata(const char *data, int size);

        void NoteOn(unsigned char chan, unsigned char note, unsigned char velocity);
        void NoteOff(unsigned char chan, unsigned char note);
        int RunChannelSwitch(int value);
        void SetController(unsigned char chan, int type, short int par);
        void SetZynControls(bool in_place);
        int RootBank(int rootnum, int banknum);
        int SetRBP(CommandBlock *getData, bool notinplace = true);
        int ReadBankRoot(void);
        int ReadBank(void);
        void SetPartChan(unsigned char npart, unsigned char nchan);
        void SetPartPortamento(int npart, bool state);
        bool ReadPartPortamento(int npart);
        void SetPartKeyMode(int npart, int mode);
        int  ReadPartKeyMode(int npart);
        void cliOutput(list<string>& msg_buf, unsigned int lines);
        void ListPaths(list<string>& msg_buf);
        void ListBanks(int rootNum, list<string>& msg_buf);
        void ListInstruments(int bankNum, list<string>& msg_buf);
        void ListCurrentParts(list<string>& msg_buf);
        void ListVectors(list<string>& msg_buf);
        bool SingleVector(list<string>& msg_buf, int chan);
        void ListSettings(list<string>& msg_buf);
        int SetSystemValue(int type, int value);
        bool vectorInit(int dHigh, unsigned char chan, int par);
        void vectorSet(int dHigh, unsigned char chan, int par);
        void ClearNRPNs(void);
        void resetAll(bool andML);
        float numRandom(void);
        unsigned int randomSE(void);
        void ShutUp(void);
        void allStop(unsigned int stopType);
        int MasterAudio(float *outl [NUM_MIDI_PARTS + 1], float *outr [NUM_MIDI_PARTS + 1], int to_process = 0);
        void partonoffLock(int npart, int what);
        void partonoffWrite(int npart, int what);
        char partonoffRead(int npart);
        sem_t partlock;
        unsigned char legatoPart;
        void setPartMap(int npart);
        void setAllPartMaps(void);

        void SetMuteAndWait(void);
        void Unmute(void);
        void Mute(void);
        void mutewrite(int what);
        bool isMuted(void);
        sem_t mutelock;

        float getLimits(CommandBlock *getData);
        float getVectorLimits(CommandBlock *getData);
        float getConfigLimits(CommandBlock *getData);

        Part *part[NUM_MIDI_PARTS];
        unsigned int fadeAll;
        float fadeStep;
        float fadeLevel;

        // parameters
        unsigned int samplerate;
        float samplerate_f;
        float halfsamplerate_f;
        int buffersize;
        float buffersize_f;
        int bufferbytes;
        int oscilsize;
        float oscilsize_f;
        int halfoscilsize;
        float halfoscilsize_f;

        int sent_buffersize; //used for variable length runs
        int sent_bufferbytes; //used for variable length runs
        float sent_buffersize_f; //used for variable length runs
        float sent_all_buffersize_f; //used for variable length runs (mainly for lv2 - calculate envelopes and lfo)
        float         TransVolume;
        float         Pvolume;
        float         ControlStep;
        int           Paudiodest;
        int           Pkeyshift;
        unsigned char Psysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        unsigned char Psysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];

        // parameters control
        void setPvolume(float value);
        void setPkeyshift(int Pkeyshift_);
        void setPsysefxvol(int Ppart, int Pefx, char Pvol);
        void setPsysefxsend(int Pefxfrom, int Pefxto, char Pvol);
        void setPaudiodest(int value);

        // effects
        EffectMgr *sysefx[NUM_SYS_EFX]; // system
        EffectMgr *insefx[NUM_INS_EFX]; // insertion

        // part that's apply the insertion effect; -1 to disable
        short int Pinsparts[NUM_INS_EFX];

        // others ...
        Controller *ctl;
        Microtonal microtonal;
        FFTwrapper *fft;

        // peaks for VU-meters
        union VUtransfer{
            struct{
                float vuOutPeakL;
                float vuOutPeakR;
                float vuRmsPeakL;
                float vuRmsPeakR;
                float parts[NUM_MIDI_PARTS];
                int buffersize;
            } values;
            char bytes [sizeof(values)];
        };
        VUtransfer VUpeak, VUcopy, VUdata;
        unsigned int VUcount;
        bool VUready;
        void fetchMeterData(void);

        inline bool getIsLV2Plugin() {return isLV2Plugin; }
        inline Config &getRuntime() {return Runtime;}
        inline PresetsStore &getPresetsStore() {return presetsstore;}
        unsigned int getUniqueId() {return uniqueId;}
        MasterUI *getGuiMaster(bool createGui = true);
        void guiClosed(bool stopSynth);
        void setGuiClosedCallback(void( *_guiClosedCallback)(void*), void *arg)
        {
            guiClosedCallback = _guiClosedCallback;
            guiCallbackArg = arg;
        }
        void closeGui();
        int getLFOtime() {return LFOtime;}
        string makeUniqueName(string name);

        Bank &getBankRef() {return bank;}
        Bank *getBankPtr() {return &bank;}
        unsigned int exportBank(string exportfile, size_t rootID, unsigned int bankID);
        unsigned int importBank(string inportfile, size_t rootID, unsigned int bankID);
        unsigned int removeBank(unsigned int bankID, size_t rootID);
        string getWindowTitle() {return windowTitle;}
        void setWindowTitle(string _windowTitle = "");
        void setNeedsSaving(bool ns) { needsSaving = ns; }
        bool getNeedsSaving() { return needsSaving; }
    private:
        int muted;
        float volume;
        float sysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        float sysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];
        //float *tmpmixl; // Temporary mixing samples for part samples
        //float *tmpmixr; // which are sent to system effect
        int keyshift;

        pthread_mutex_t  processMutex;
        pthread_mutex_t *processLock;

        //XMLwrapper *stateXMLtree;

        char random_state[256];
        float random_0_1;

#if (HAVE_RANDOM_R)
        struct random_data random_buf;
        int32_t random_result;
#else
        long int random_result;
#endif

    public:
        MasterUI *guiMaster; // need to read this in InterChange::returns
    private:
        void( *guiClosedCallback)(void*);
        void *guiCallbackArg;

        int LFOtime; // used by Pcontinous
        string windowTitle;
        //MusicClient *musicClient;
};

inline float SynthEngine::numRandom(void)
{
    int ret;
#if (HAVE_RANDOM_R)
    ret = random_r(&random_buf, &random_result);
#else
    random_result = random();
    ret = 0;
#endif

    if (!ret)
    {
        random_0_1 = (float)random_result / (float)INT_MAX;
        random_0_1 = (random_0_1 > 1.0f) ? 1.0f : random_0_1;
        random_0_1 = (random_0_1 < 0.0f) ? 0.0f : random_0_1;
        return random_0_1;
    }
    return 0.05f;
}

inline unsigned int SynthEngine::randomSE(void)
{
#if (HAVE_RANDOM_R)
    if (!random_r(&random_buf, &random_result))
        return random_result + INT_MAX / 2;
    return INT_MAX / 2;
#else
    random_result = random();
    return (unsigned int)random_result + INT_MAX / 2;
#endif
}

#endif
