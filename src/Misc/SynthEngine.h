/*
    SynthEngine.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2021, Will Godfrey & others

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

*/

#ifndef SYNTHENGINE_H
#define SYNTHENGINE_H

#include <sys/types.h>
#include <limits.h>
#include <cstdlib>
#include <semaphore.h>
#include <jack/ringbuffer.h>
#include <string>
#include <vector>
#include <list>

#include "Misc/RandomGen.h"
#include "Misc/Microtonal.h"
#include "Misc/Bank.h"
#include "Interface/InterChange.h"
#include "Interface/MidiLearn.h"
#include "Interface/MidiDecode.h"
#include "Misc/Config.h"
#include "Params/PresetsStore.h"
#include "Params/UnifiedPresets.h"
#include "globals.h"

class Part;
class EffectMgr;
class XMLwrapper;
class Controller;
class TextMsgBuffer;

#ifdef GUI_FLTK
class MasterUI;
#endif

using std::string;


class SynthEngine
{
    private:
        unsigned int uniqueId;
        bool isLV2Plugin;
        bool needsSaving;
    public:
        std::atomic <uint8_t> audioOut;
        Bank bank;
        InterChange interchange;
        MidiLearn midilearn;
        MidiDecode mididecode;
        UnifiedPresets unifiedpresets;
    private:
        Config Runtime;
        PresetsStore presetsstore;
    public:
        TextMsgBuffer& textMsgBuffer;
        SynthEngine(int argc, char **argv, bool _isLV2Plugin = false, unsigned int forceId = 0);
        ~SynthEngine();
        bool Init(unsigned int audiosrate, int audiobufsize);

        bool savePatchesXML(string filename);
        void add2XML(XMLwrapper *xml);
        string manualname();
        void defaults(void);

        bool loadXML(const string& filename);
        bool loadStateAndUpdate(const string& filename);
        bool saveState(const string& filename);
        bool loadPatchSetAndUpdate(string filename);
        bool loadMicrotonal(const string& fname);
        bool saveMicrotonal(const string& fname);
        bool installBanks(void);
        bool saveBanks(void);
        void newHistory(string name, int group);
        void addHistory(const string& name, int group);
        std::vector<string> *getHistory(int group);
        void setHistoryLock(int group, bool status);
        bool getHistoryLock(int group);
        string lastItemSeen(int group);
        void setLastfileAdded(int group, string name);
        string getLastfileAdded(int group);
        bool loadHistory(void);
        bool saveHistory(void);
        unsigned char loadVectorAndUpdate(unsigned char baseChan, const string& name);
        unsigned char loadVector(unsigned char baseChan, const string& name, bool full);
        unsigned char extractVectorData(unsigned char baseChan, XMLwrapper *xml, const string& name);
        unsigned char saveVector(unsigned char baseChan, const string& name, bool full);
        bool insertVectorData(unsigned char baseChan, bool full, XMLwrapper *xml, const string& name);

        bool getfromXML(XMLwrapper *xml);

        int getalldata(char **data);
        void putalldata(const char *data, int size);

        void NoteOn(unsigned char chan, unsigned char note, unsigned char velocity);
        void NoteOff(unsigned char chan, unsigned char note);
        int RunChannelSwitch(unsigned char chan, int value);
        void SetController(unsigned char chan, int CCtype, short int par);
        void SetZynControls(bool in_place);
        int setRootBank(int root, int bank, bool notinplace = true);
        int setProgramByName(CommandBlock *getData);
        int setProgramFromBank(CommandBlock *getData, bool notinplace = true);
        bool setProgram(const string& fname, int npart);
        int ReadBankRoot(void);
        int ReadBank(void);
        void SetPartChan(unsigned char npart, unsigned char nchan);
        void SetPartPortamento(int npart, bool state);
        bool ReadPartPortamento(int npart);
        void SetPartKeyMode(int npart, int mode);
        int  ReadPartKeyMode(int npart);
        void cliOutput(std::list<string>& msg_buf, unsigned int lines);
        void ListPaths(std::list<string>& msg_buf);
        void ListBanks(int rootNum, std::list<string>& msg_buf);
        void ListInstruments(int bankNum, std::list<string>& msg_buf);
        void ListVectors(std::list<string>& msg_buf);
        bool SingleVector(std::list<string>& msg_buf, int chan);
        void ListSettings(std::list<string>& msg_buf);
        int SetSystemValue(int type, int value);
        int LoadNumbered(unsigned char group, unsigned char entry);
        bool vectorInit(int dHigh, unsigned char chan, int par);
        void vectorSet(int dHigh, unsigned char chan, int par);
        void ClearNRPNs(void);
        void resetAll(bool andML);
        void ShutUp(void);
        int MasterAudio(float *outl [NUM_MIDI_PARTS + 1], float *outr [NUM_MIDI_PARTS + 1], int to_process = 0);
        void partonoffLock(int npart, int what);
        void partonoffWrite(int npart, int what);
        char partonoffRead(int npart);
        sem_t partlock;
        unsigned char legatoPart;
        void setPartMap(int npart);
        void setAllPartMaps(void);

        bool masterMono;
        bool fileCompatible;
        bool usingYoshiType;

        float getLimits(CommandBlock *getData);
        float getVectorLimits(CommandBlock *getData);
        float getConfigLimits(CommandBlock *getData);

        Part *part[NUM_MIDI_PARTS];
        unsigned int fadeAll;
        // Per sample change in gain calculated whenever samplerate changes (which
        // is currently only on init). fadeStep is used in SynthEngine, while
        // fadeStepShort is used directly by notes, currently only for legato.
        float fadeStep;
        float fadeStepShort;
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
        float oscil_sample_step_f;
        float oscil_norm_factor_pm;
        float oscil_norm_factor_fm;

        // Reference values used for normalization
        static constexpr float samplerate_ref_f = 44100.0f;
        static constexpr float oscilsize_ref_f = float(1024 * 256);

        int           sent_buffersize; //used for variable length runs
        int           sent_bufferbytes; //used for variable length runs
        float         sent_buffersize_f; //used for variable length runs
        float         fixed_sample_step_f;
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
        bool syseffEnable[NUM_SYS_EFX];
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
        int meterDelay;
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
        int64_t getLFOtime() {return LFOtime;}
        float getSongBeat() {return songBeat;}
        float getMonotonicBeat() {return monotonicBeat;}
        void setBeatValues(float song, float monotonic) {
            songBeat = song;
            monotonicBeat = monotonic;
        }
        string makeUniqueName(const string& name);

        Bank &getBankRef() {return bank;}
        Bank *getBankPtr() {return &bank;}

        string getWindowTitle() {return windowTitle;}
        void setWindowTitle(const string& _windowTitle = "");
        void setNeedsSaving(bool ns) { needsSaving = ns; }
        bool getNeedsSaving() { return needsSaving; }
    private:
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

        int CHtimer;

        int64_t LFOtime; // used by Pcontinous without Pbpm
        float songBeat; // used by Pbpm without Pcontinous
        float monotonicBeat; // used by Pbpm

        string windowTitle;
        //MusicClient *musicClient;

        RandomGen prng;
    public:
        float numRandom()   { return prng.numRandom(); }
        uint32_t randomINT(){ return prng.randomINT(); }   // random number in the range 0...INT_MAX
        void reseed(int value) { prng.init(value); }
};

#endif
