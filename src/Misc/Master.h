/*
    Master.h - It sends Midi Messages to Parts, receives samples from parts,
               process them with system/insertion effects and mix them

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

    This file is part of zynminus, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    zynminus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with zynminus.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MASTER_H
#define MASTER_H

#include "Effects/EffectMgr.h"
#include "Effects/Fader.h"
#include "Misc/Part.h"
#include "Misc/Microtonal.h"
#include "Misc/Bank.h"
#include "Misc/XMLwrapper.h"
#include "Misc/Util.h"

typedef enum { init, trylock, lock, unlock } lockset;

extern bool Pexitprogram;  // if the UI sets this true, the program will exit
extern float *denormalkillbuf;

class Master;
extern Master *zynMaster;

class Master {
    public:
        Master();
        ~Master();
        bool Init(unsigned int sample_rate, int buffer_size, int oscil_size);
        bool actionLock(lockset request);

        int saveXML(const char *filename);
        void add2XML(XMLwrapper *xml);
        void defaults(void);

        int loadXML(const char *filename);
        void applyparameters(void);

        void getfromXML(XMLwrapper *xml);

        int getalldata(char **data);
        void putalldata(char *data, int size);

        // midi in
        void NoteOn(unsigned char chan, unsigned char note, unsigned char velocity);
        void NoteOff(unsigned char chan, unsigned char note);
        void SetController(unsigned char chan, unsigned int type, short int par);
        // void NRPN...

        void ShutUp(void);

        bool MasterAudio(float *outl, float *outr, bool lockrequired);

        void partOnOff(int npart, int what);

        Part *part[NUM_MIDI_PARTS];

        int shutup;

        // parameters
        unsigned char Pvolume;
        unsigned char Pkeyshift;
        unsigned char Psysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        unsigned char Psysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];

        // parameters control
        void setPvolume(char value);
        void setPkeyshift(char Pkeyshift_);
        void setPsysefxvol(int Ppart, int Pefx, char Pvol);
        void setPsysefxsend(int Pefxfrom, int Pefxto, char Pvol);

        // effects
        EffectMgr *sysefx[NUM_SYS_EFX]; // system
        EffectMgr *insefx[NUM_INS_EFX]; // insertion

        // part that's apply the insertion effect; -1 to disable
        short int Pinsparts[NUM_INS_EFX];

        // peaks for VU-meter
        void vuresetpeaks(void);
        float vuoutpeakl, vuoutpeakr,
                 vumaxoutpeakl, vumaxoutpeakr,
                 vurmspeakl, vurmspeakr;
        int vuclipped;

        // peaks for part VU-meters
        float vuoutpeakpart[NUM_MIDI_PARTS];
        unsigned char fakepeakpart[NUM_MIDI_PARTS]; // this is used to compute the
                                                    // "peak" when the part is disabled
        // others ...
        Controller ctl;
        Microtonal microtonal;
        Bank bank;
        FFTwrapper *fft;

        unsigned int getSamplerate(void) { return samplerate; };
        int getBuffersize(void) { return buffersize; };
        int getOscilsize(void) { return oscilsize; };

    private:
        unsigned int samplerate;
        int buffersize;
        int oscilsize;

        pthread_mutex_t mutex;
        pthread_mutex_t *processLock;

        float volume;
        float sysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        float sysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];
        float *tmpmixl; // Temporary mixing samples for part samples
        float *tmpmixr; // which are sent to system effect
        int keyshift;
        Fader *volControl;
};

#endif

