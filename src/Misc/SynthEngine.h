/*
    SynthEngine.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others

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

    Modified May 2019
*/

#ifndef SYNTHENGINE_H
#define SYNTHENGINE_H

#include <sys/types.h>
#include <limits.h>
#include <cstdlib>
#include <semaphore.h>
#include <jack/ringbuffer.h>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/RandomGen.h"
#include "Misc/SynthHelper.h"
#include "Misc/Microtonal.h"
#include "Misc/Bank.h"
#include "Misc/SynthHelper.h"
#include "Interface/InterChange.h"
#include "Interface/MidiLearn.h"
#include "Interface/MidiDecode.h"
#include "Interface/FileMgr.h"
#include "Misc/Config.h"
#include "Params/PresetsStore.h"
#include "Params/UnifiedPresets.h"

class EffectMgr;
class Part;
class XMLwrapper;
class Controller;

#ifdef GUI_FLTK
class MasterUI;
#endif

class SynthEngine : private SynthHelper, MiscFuncs, FileMgr
{
    private:
        unsigned int uniqueId;
        bool isLV2Plugin;
        bool needsSaving;
    public:
        Bank bank;
        InterChange interchange;
        MidiLearn midilearn;
        MidiDecode mididecode;
        UnifiedPresets unifiedpresets;
    private:
        Config Runtime;
        PresetsStore presetsstore;
    public:
        SynthEngine(int argc, char **argv, bool _isLV2Plugin = false, unsigned int forceId = 0);
        ~SynthEngine();
        bool Init(unsigned int audiosrate, int audiobufsize);

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
        bool installBanks(void);
        bool saveBanks(void);
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
        void SetController(unsigned char chan, int CCtype, short int par);
        void SetZynControls(bool in_place);
        int setRootBank(int root, int bank, bool notinplace = true);
        int setProgramByName(CommandBlock *getData);
        int setProgramFromBank(CommandBlock *getData, bool notinplace = true);
        bool setProgram(string fname, int npart);
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
        void ListVectors(list<string>& msg_buf);
        bool SingleVector(list<string>& msg_buf, int chan);
        void ListSettings(list<string>& msg_buf);
        int SetSystemValue(int type, int value);
        bool vectorInit(int dHigh, unsigned char chan, int par);
        void vectorSet(int dHigh, unsigned char chan, int par);
        void ClearNRPNs(void);
        void resetAll(bool andML);
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
        bool masterMono;

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
        unsigned char  syseffnum;
        unsigned char  inseffnum;
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
                float partsR[NUM_MIDI_PARTS];
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
        SynthEngine *getSynthFromId(unsigned int uniqueId);
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

        string getWindowTitle() {return windowTitle;}
        void setWindowTitle(string _windowTitle = "");
        void setNeedsSaving(bool ns) { needsSaving = ns; }
        bool getNeedsSaving() { return needsSaving; }
    private:
        int muted;
        float volume;
        float sysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        float sysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];

        int keyshift;

    public:
#ifdef GUI_FLTK
        MasterUI *guiMaster; // need to read this in InterChange::returns
        MasterUI *getGuiMaster(bool createGui = true);
#endif
    private:
        void( *guiClosedCallback)(void*);
        void *guiCallbackArg;

        int LFOtime; // used by Pcontinous
        string windowTitle;
        //MusicClient *musicClient;

        RandomGen prng;
    public:
        float numRandom()   { return prng.numRandom(); }
        uint32_t randomINT(){ return prng.randomINT(); }   // random number in the range 0...INT_MAX
};

#endif
