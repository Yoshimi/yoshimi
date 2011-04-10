/*
    SynthEngine.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified March 2011
*/

#ifndef SYNTHENGINE_H
#define SYNTHENGINE_H

#include <limits.h>
#include <cstdlib>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/SynthHelper.h"
#include "Misc/Microtonal.h"
#include "Misc/Bank.h"
#include "Misc/SynthHelper.h"

typedef enum { init, trylock, lock, unlock, lockmute, destroy } lockset;

class EffectMgr;
class Part;
class XMLwrapper;
class Controller;

class SynthEngine : private SynthHelper, MiscFuncs
{
    public:
        SynthEngine();
        ~SynthEngine();
        bool Init(unsigned int audiosrate, int audiobufsize);
        bool actionLock(lockset request);
        bool vupeakLock(lockset request);

        bool saveXML(string filename);
        void add2XML(XMLwrapper *xml);
        void defaults(void);

        bool loadXML(string filename);
        void applyparameters(void);

        bool getfromXML(XMLwrapper *xml);

        int getalldata(char **data);
        void putalldata(char *data, int size);

        void NoteOn(unsigned char chan, unsigned char note, unsigned char velocity);
        void NoteOff(unsigned char chan, unsigned char note);
        void SetController(unsigned char chan, unsigned int type, short int par);
        float numRandom(void);
        unsigned int random(void);
        void ShutUp(void);
        void MasterAudio(float *outl, float *outr);
        void partonoff(int npart, int what);
        void Mute(void) { __sync_or_and_fetch(&muted, 0xFF); }
        void Unmute(void) { __sync_and_and_fetch(&muted, 0); }
        bool isMuted(void) { return (__sync_add_and_fetch(&muted, 0) != 0); }

        Part *part[NUM_MIDI_PARTS];
        bool shutup;

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

        unsigned char Pvolume;
        int           Pkeyshift;
        unsigned char Psysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        unsigned char Psysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];

        // parameters control
        void setPvolume(char value);
        void setPkeyshift(int Pkeyshift_);
        void setPsysefxvol(int Ppart, int Pefx, char Pvol);
        void setPsysefxsend(int Pefxfrom, int Pefxto, char Pvol);

        // effects
        EffectMgr *sysefx[NUM_SYS_EFX]; // system
        EffectMgr *insefx[NUM_INS_EFX]; // insertion

        // part that's apply the insertion effect; -1 to disable
        short int Pinsparts[NUM_INS_EFX];

        // peaks for part VU-meters
        float vuoutpeakpart[NUM_MIDI_PARTS];
        unsigned char fakepeakpart[NUM_MIDI_PARTS]; // this is used to compute the
                                                    // "peak" when the part is disabled
        // others ...
        Controller *ctl;
        Microtonal microtonal;
        Bank bank;
        FFTwrapper *fft;

        // peaks for VU-meters
        void vuresetpeaks(void);
        float vuOutPeakL;
        float vuOutPeakR;
        float vuMaxOutPeakL;
        float vuMaxOutPeakR;
        float vuRmsPeakL;
        float vuRmsPeakR;
        bool vuClippedL;
        bool vuClippedR;


    private:
        int muted;
        float volume;
        float sysefxvol[NUM_SYS_EFX][NUM_MIDI_PARTS];
        float sysefxsend[NUM_SYS_EFX][NUM_SYS_EFX];
        float *tmpmixl; // Temporary mixing samples for part samples
        float *tmpmixr; // which are sent to system effect
        int keyshift;

        float vuoutpeakl;
        float vuoutpeakr;
        float vumaxoutpeakl;
        float vumaxoutpeakr;
        float vurmspeakl;
        float vurmspeakr;
        bool clippedL;
        bool clippedR;

        pthread_mutex_t  processMutex;
        pthread_mutex_t *processLock;
        pthread_mutex_t  meterMutex;
        pthread_mutex_t *meterLock;

        XMLwrapper *stateXMLtree;

        static char random_state[];
        static struct random_data random_buf;
        int32_t random_result;
        float random_0_1;
};

extern SynthEngine *synth;

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
