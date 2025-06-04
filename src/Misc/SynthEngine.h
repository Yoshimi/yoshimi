/*
    SynthEngine.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2025, Will Godfrey & others

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

#ifndef SYNTHENGINE_H
#define SYNTHENGINE_H

#include <sys/types.h>
#include <limits.h>
#include <cstdlib>
#include <semaphore.h>
#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <list>
#include <map>

#include "Misc/RandomGen.h"
#include "Misc/Microtonal.h"
#include "Misc/Bank.h"
#include "DSP/FFTwrapper.h"
#include "Interface/InterChange.h"
#include "Interface/MidiLearn.h"
#include "Interface/MidiDecode.h"
#include "Interface/Vectors.h"
#include "Misc/Config.h"
#include "globals.h"

class Part;
class EffectMgr;
struct EffectDTO;
struct EqGraphDTO;
class XMLStore;
class Controller;
class TextMsgBuffer;
class InterfaceAnchor;

#ifdef GUI_FLTK
class MasterUI;
#endif

using std::string;
using std::to_string;
using std::unique_ptr;


class SynthEngine
{
        const uint uniqueId;
        Config Runtime;

    public:
        Bank bank;
        InterChange interchange;
        MidiLearn midilearn;
        MidiDecode mididecode;
        Vectors vectorcontrol;

    public:
        Config& getRuntime()     {return Runtime;}
        uint getUniqueId() const {return uniqueId;}

        SynthEngine(uint instanceID);
       ~SynthEngine();
        // shall not be copied nor moved
        SynthEngine(SynthEngine&&)                 = delete;
        SynthEngine(SynthEngine const&)            = delete;
        SynthEngine& operator=(SynthEngine&&)      = delete;
        SynthEngine& operator=(SynthEngine const&) = delete;

        bool Init(uint audiosrate, int audiobufsize);
        InterfaceAnchor buildGuiAnchor();
        void postBootHook(bool);

        bool savePatchesXML(string filename);
        void add2XML(XMLStore&);
        string manualname();
        void defaults();

        bool loadXML(string const& filename);
        bool loadStateAndUpdate(string const& filename);
        bool saveState(string const& filename);
        bool loadPatchSetAndUpdate(string filename);
        bool installBanks();
        bool saveBanks();
        void newHistory(string name, uint group);
        void addHistory(string const& name, uint group);
        std::vector<string>& getHistory(uint group);
        void setHistoryLock(int group, bool status);
        bool getHistoryLock(int group);
        string lastItemSeen(int group);
        bool loadHistory();
        bool saveHistory();

        bool getfromXML(XMLStore&);

        void NoteOn(uchar chan, uchar note, uchar velocity);
        void NoteOff(uchar chan, uchar note);
        int  RunChannelSwitch(uchar chan, int value);
        void SetController(uchar chan, int CCtype, short par);
        void SetZynControls(bool in_place);
        int  setRootBank(int root, int bank, bool inplace = false);
        int  setProgramByName(CommandBlock&);
        int  setProgramFromBank(CommandBlock&, bool inplace = false);
        bool setProgram(string const& fname, int npart);
        int  ReadBankRoot();
        int  ReadBank();
        void SetPartChanForVector(uchar npart, uchar nchan);
        void SetPartPortamento(int npart, bool state);
        bool ReadPartPortamento(int npart);
        void SetPartKeyMode(int npart, int mode);
        int  ReadPartKeyMode(int npart);
        void cliOutput(std::list<string>& msg_buf, uint lines);
        void ListPaths(std::list<string>& msg_buf);
        void ListBanks(int rootNum, std::list<string>& msg_buf);
        void ListInstruments(int bankNum, std::list<string>& msg_buf);
        void ListVectors(std::list<string>& msg_buf);
        bool SingleVector(std::list<string>& msg_buf, int chan);
        void ListSettings(std::list<string>& msg_buf);
        int  SetSystemValue(int type, int value);
        int  LoadNumbered(uchar group, uchar entry);
        bool vectorInit(int dHigh, uchar chan, int par);
        void vectorSet(int dHigh, uchar chan, int par);
        void ClearNRPNs();
        void resetAll(bool andML);
        void ShutUp();
        int  MasterAudio(float *outl [NUM_MIDI_PARTS + 1], float *outr [NUM_MIDI_PARTS + 1], int to_process = 0);
        void partonoffLock(uint npart, int what);
        void partonoffWrite(uint npart, int what);
        char partonoffRead(uint npart);
        void setPartMap(int npart);
        void setAllPartMaps();

        void audioOutStore(uint8_t num);
        std::atomic <uint8_t> audioOut;
        sem_t partlock;
        uchar legatoPart;

        bool masterMono;

        float getLimits(CommandBlock *getData);
        float getVectorLimits(CommandBlock *getData);
        float getConfigLimits(CommandBlock *getData);
        void  CBtest(CommandBlock *candidate, bool miscmsg = false);


        Part *part[NUM_MIDI_PARTS];
        uint  fadeAll;
        // Per sample change in gain calculated whenever samplerate changes
        // (which is currently only on init). fadeStep is used in SynthEngine,
        // while fadeStepShort is used directly by notes, currently only for legato.
        float fadeStep;
        float fadeStepShort;
        float fadeLevel;

        // parameters
        uint  samplerate;
        float samplerate_f;
        float halfsamplerate_f;
        int   buffersize;
        float buffersize_f;
        int   bufferbytes;
        int   oscilsize;
        float oscilsize_f;
        int   halfoscilsize;
        float halfoscilsize_f;
        float oscil_sample_step_f;
        float oscil_norm_factor_pm;
        float oscil_norm_factor_fm;

        // Reference values used for normalization
        static constexpr float samplerate_ref_f = 44100.0f;
        static constexpr float oscilsize_ref_f = float(1024 * 256);

        int   sent_buffersize; //used for variable length runs
        int   sent_bufferbytes; //used for variable length runs
        float sent_buffersize_f; //used for variable length runs
        float fixed_sample_step_f;
        float TransVolume;
        float Pvolume;
        float ControlStep;
        int   Paudiodest;
        int   Pkeyshift;
        float PbpmFallback;
        uchar Psysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        uchar Psysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];

        // parameters control
        void setPvolume(float value);
        void setPkeyshift(int Pkeyshift_);
        void setPsysefxvol(int Ppart, int Pefx, char Pvol);
        void setPsysefxsend(int Pefxfrom, int Pefxto, char Pvol);
        void setPaudiodest(int value);

        // effects
        uchar  syseffnum;
        bool   syseffEnable[NUM_SYS_EFX];
        uchar  inseffnum;
        EffectMgr* sysefx[NUM_SYS_EFX]; // system
        EffectMgr* insefx[NUM_INS_EFX]; // insertion

        // part that's apply the insertion effect; -1 to disable
        int Pinsparts[NUM_INS_EFX];

        // connection to the currently active effect UI
        GuiDataExchange::Connection<EffectDTO> sysEffectUiCon;
        GuiDataExchange::Connection<EffectDTO> insEffectUiCon;
        GuiDataExchange::Connection<EffectDTO> partEffectUiCon;
        GuiDataExchange::Connection<EqGraphDTO> sysEqGraphUiCon;
        GuiDataExchange::Connection<EqGraphDTO> insEqGraphUiCon;
        GuiDataExchange::Connection<EqGraphDTO> partEqGraphUiCon;

        void pushEffectUpdate(uchar partNr);
        void maybePublishEffectsToGui();

        // others ...
        Controller* ctl;
        Microtonal microtonal;
        unique_ptr<fft::Calc> fft;
        TextMsgBuffer& textMsgBuffer;

        // peaks for VU-meters
        union VUtransfer{
            struct{
                float vuOutPeakL{0};
                float vuOutPeakR{0};
                float vuRmsPeakL{0};
                float vuRmsPeakR{0};
                float parts[NUM_MIDI_PARTS];
                float partsR[NUM_MIDI_PARTS];
                int buffersize{0};
            } values;
            char bytes [sizeof(values)];
        };
        VUtransfer VUpeak, VUcopy, VUdata;
        uint VUcount;
        bool VUready;
        void fetchMeterData();

        using CallbackGuiClosed = std::function<void()>;
        void installGuiClosedCallback(CallbackGuiClosed callback)
        {
            callbackGuiClosed = std::move(callback);
        }
        void signalGuiWindowClosed();
        void shutdownGui();
        int64_t getLFOtime()      const { return LFOtime;}
        float getSongBeat()       const { return songBeat;}
        float getMonotonicBeat()  const { return monotonicBeat;}
        float getBPM()            const { return bpm; }
        bool isBPMAccurate()      const { return bpmAccurate; }
        void setBPMAccurate(bool value) { bpmAccurate = value; }
        void setBeatValues(float songBeat, float monotonicBeat, float bpm)
        {
            this->songBeat = songBeat;
            this->monotonicBeat = monotonicBeat;
            this->bpm = bpm;
        }
        string makeUniqueName(string const& name);

        void setWindowTitle(string const& t){ if (!t.empty()) windowTitle = t; }
        string getWindowTitle()             { return windowTitle;}
        void setNeedsSaving(bool ns)        { needsSaving = ns; }
        bool getNeedsSaving()               { return needsSaving; }
    private:
        float volume;
        float sysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        float sysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];

        int keyshift;

    public:
#ifdef GUI_FLTK
        ///////////////////TODO 1/2024 : retract direct usage of direct SynthEngine* from UI
        MasterUI* getGuiMaster();
#endif
    private:
        CallbackGuiClosed callbackGuiClosed;
        string windowTitle;
        bool needsSaving;

        int64_t channelTimer;

        int64_t LFOtime;     // used by Pcontinous without Pbpm
        float songBeat;      // used by Pbpm without Pcontinous
        float monotonicBeat; // used by Pbpm
        float bpm;           // used by Echo Effect
        bool  bpmAccurate;   // Set to false by engines that can't provide an accurate BPM value.


        RandomGen prng;
    public:
        float numRandom()   { return prng.numRandom(); }
        uint32_t randomINT(){ return prng.randomINT(); }   // random number in the range 0...INT_MAX
        void setReproducibleState(int value);
        void swapTestPADtable();
};
#endif /*SYNTHENGINE_H*/
