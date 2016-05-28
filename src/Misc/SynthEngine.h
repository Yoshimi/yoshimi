/*
    SynthEngine.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2016, Will Godfrey & others

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

    This file is derivative of ZynAddSubFX original code, last modified March 2016
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

#include "Misc/Config.h"
#include "Params/PresetsStore.h"

typedef enum { init, trylock, lock, unlock, lockmute, destroy } lockset;

class EffectMgr;
class Part;
class XMLwrapper;
class Controller;
class CmdInterface;

class MasterUI;

class SynthEngine : private SynthHelper, MiscFuncs
{
    private:
        unsigned int uniqueId;
        bool isLV2Plugin;
        Bank bank;
        Config Runtime;
        PresetsStore presetsstore;
    public:
        SynthEngine(int argc, char **argv, bool _isLV2Plugin = false, unsigned int forceId = 0);
        ~SynthEngine();
        bool Init(unsigned int audiosrate, int audiobufsize);
        bool actionLock(lockset request);

        bool saveXML(string filename);
        void add2XML(XMLwrapper *xml);
        void defaults(void);

        bool loadXML(string filename);
        void applyparameters(void);
        int loadParameters(string filename);
        int loadPatchSetAndUpdate(string filename);
        bool installBanks(int instance);
        bool saveBanks(int instance);
        void addHistory(string name, int group);
        vector<string> * getHistory(int group);
        bool loadHistory(void);
        bool saveHistory(void);
        bool loadVector(unsigned char baseChan, string name, bool full);
        bool saveVector(unsigned char baseChan, string name, bool full);

        bool getfromXML(XMLwrapper *xml);

        int getalldata(char **data);
        void putalldata(const char *data, int size);

        void NoteOn(unsigned char chan, unsigned char note, unsigned char velocity);
        void NoteOff(unsigned char chan, unsigned char note);
        void SetController(unsigned char chan, int type, short int par);
        void SetZynControls();
        void SetEffects(unsigned char category, unsigned char command, unsigned char nFX, unsigned char nType, int nPar, unsigned char value);
        void SetBankRoot(int rootnum);
        void SetBank(int banknum);
        int ReadBankRoot(void);
        int ReadBank(void);

        void commandFetch(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char insertParam = 0xff);
        void commandSend(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char insertParam = 0xff);
        void commandVector(float value, unsigned char type, unsigned char control);
        void commandMain(float value, unsigned char type, unsigned char control);
        void commandPart(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine);
        void commandAdd(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit);
        void commandAddVoice(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine);
        void commandSub(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char insert);
        void commandPad(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit);
        void commandOscillator(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert);
        void commandResonance(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert);
        void commandLFO(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter);
        void commandFilter(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert);
        void commandEnvelope(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter);
        void commandSysIns(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char engine, unsigned char insert);
        void commandEffects(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine);

        void SetProgram(unsigned char chan, unsigned short pgm);
        bool SetProgramToPart(int npart, int pgm, string fname);
        void SetPartChan(unsigned char npart, unsigned char nchan);
        void SetPartDestination(unsigned char npart, unsigned char dest);
        void SetPartShift(unsigned char npart, unsigned char shift);
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
        void SetSystemValue(int type, int value);
        void writeRBP(char type, char data0, char data1);
        bool vectorInit(int dHigh, unsigned char chan, int par);
        void vectorSet(int dHigh, unsigned char chan, int par);
        void ClearNRPNs(void);
        void resetAll(void);
        float numRandom(void);
        unsigned int random(void);
        void ShutUp(void);
        void allStop();
        int MasterAudio(float *outl [NUM_MIDI_PARTS + 1], float *outr [NUM_MIDI_PARTS + 1], int to_process = 0);
        void partonoffLock(int npart, int what);
        void partonoffWrite(int npart, int what);
        bool partonoffRead(int npart);
        sem_t partlock;

        void Mute(void) { __sync_or_and_fetch(&muted, 0xFF); }
        void Unmute(void) { __sync_and_and_fetch(&muted, 0); }
        bool isMuted(void) { return (__sync_add_and_fetch(&muted, 0) != 0); }

        Part *part[NUM_MIDI_PARTS];
        bool shutup;
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

        int p_buffersize; //used for variable length runs
        int p_bufferbytes; //used for variable length runs
        float p_buffersize_f; //used for variable length runs
        float p_all_buffersize_f; //used for variable length runs (mainly for lv2 - calculate envelopes and lfo)

        unsigned char Pvolume;
        int           Paudiodest;
        int           Pkeyshift;
        unsigned char Psysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        unsigned char Psysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];

        // parameters control
        void setPvolume(char value);
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
                int p_buffersize;
            } values;
            char bytes [sizeof(values)];
        };
        union VUtransfer VUpeak, VUdata;

        bool fetchMeterData(VUtransfer *VUdata);

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
        string getWindowTitle() {return windowTitle;}
        void setWindowTitle(string _windowTitle = "");
    private:
        int muted;
        float volume;
        float sysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        float sysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];
        float *tmpmixl; // Temporary mixing samples for part samples
        float *tmpmixr; // which are sent to system effect
        int keyshift;

        pthread_mutex_t  processMutex;
        pthread_mutex_t *processLock;

        jack_ringbuffer_t *vuringbuf;

        jack_ringbuffer_t *RBPringbuf;
        void *RBPthread(void);
        static void *_RBPthread(void *arg);
        pthread_t  RBPthreadHandle;

        struct RBP_data {
            char data[4];
        };

        XMLwrapper *stateXMLtree;

        char random_state[256];
        struct random_data random_buf;
        int32_t random_result;
        float random_0_1;

        MasterUI *guiMaster;
        void( *guiClosedCallback)(void*);
        void *guiCallbackArg;

        int LFOtime; // used by Pcontinous
        string windowTitle;
        MusicClient *musicClient;
};

inline float SynthEngine::numRandom(void)
{
    if (!random_r(&random_buf, &random_result))
    {
        random_0_1 = (float)random_result / (float)INT_MAX;
        random_0_1 = (random_0_1 > 1.0f) ? 1.0f : random_0_1;
        random_0_1 = (random_0_1 < 0.0f) ? 0.0f : random_0_1;
        return random_0_1;
    }
    return 0.05f;
}

inline unsigned int SynthEngine::random(void)
{
    if (!random_r(&random_buf, &random_result))
        return random_result + INT_MAX / 2;
    return INT_MAX / 2;
}

#endif
