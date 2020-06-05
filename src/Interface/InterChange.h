/*
    InterChange.h - General communications

    Copyright 2016-2020 Will Godfrey

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

*/

#ifndef INTERCH_H
#define INTERCH_H

#include <jack/ringbuffer.h>

#include "globals.h"
#include "Interface/Data2Text.h"
#include "Interface/RingBuffer.h"
#include "Params/LFOParams.h"
#include "Params/FilterParams.h"
#include "Params/EnvelopeParams.h"
#include "Params/OscilParameters.h"
#include "Synth/Resonance.h"

class SynthEngine;
class DataText;

// used by main.cpp and SynthEngine.cpp
extern std::string singlePath;
extern int startInstance;


class InterChange : private DataText
{

    private:
        SynthEngine *synth;

    public:
        InterChange(SynthEngine *_synth);
        ~InterChange();
        bool Init();

        CommandBlock commandData;
#ifndef YOSHIMI_LV2_PLUGIN
        ringBuff *fromCLI;
#endif
        ringBuff *decodeLoopback;
        ringBuff *fromGUI;
        ringBuff *toGUI;
        ringBuff *fromMIDI;
        ringBuff *returnsBuffer;
        ringBuff *muteQueue;

        void generateSpecialInstrument(int npart, std::string name);
        void mediate(void);
        void historyActionCheck(CommandBlock *getData);
        void returns(CommandBlock *getData);
        void setpadparams(int npart, int kititem);
        void doClearPart(int npart);
        bool commandSend(CommandBlock *getData);
        float readAllData(CommandBlock *getData);
        void resolveReplies(CommandBlock *getData);
        std::string resolveText(CommandBlock *getData, bool addValue);
        void testLimits(CommandBlock *getData);
        float returnLimits(CommandBlock *getData);
        std::atomic<bool> syncWrite;
        std::atomic<bool> lowPrioWrite;

        unsigned int tick; // needs to be read by synth

    private:
        void *sortResultsThread(void);
        static void *_sortResultsThread(void *arg);
        pthread_t  sortResultsThreadHandle;
        void muteQueueWrite(CommandBlock *getData);
        void indirectTransfers(CommandBlock *getData, bool noForward = false);
        std::string formatScales(std::string text);

        unsigned int lockTime;

        unsigned int swapRoot1;
        unsigned int swapBank1;
        unsigned int swapInstrument1;

        void commandMidi(CommandBlock *getData);
        void vectorClear(int Nvector);
        void commandVector(CommandBlock *getData);
        void commandMicrotonal(CommandBlock *getData);
        void commandConfig(CommandBlock *getData);
        void commandMain(CommandBlock *getData);
        void commandBank(CommandBlock *getData);
        void commandPart(CommandBlock *getData);
        void commandAdd(CommandBlock *getData);
        void commandAddVoice(CommandBlock *getData);
        void commandSub(CommandBlock *getData);
        void commandPad(CommandBlock *getData);
        void commandOscillator(CommandBlock *getData, OscilParameters *oscil);
        void commandResonance(CommandBlock *getData, Resonance *respar);
        void commandLFO(CommandBlock *getData);
        void lfoReadWrite(CommandBlock *getData, LFOParams *pars);
        void commandFilter(CommandBlock *getData);
        void filterReadWrite(CommandBlock *getData, FilterParams *pars, unsigned char *velsnsamp, unsigned char *velsns);
        void commandEnvelope(CommandBlock *getData);
        void envelopeReadWrite(CommandBlock *getData, EnvelopeParams *pars);
        void commandSysIns(CommandBlock *getData);

        /*
         * this is made public specifically so that it can be
         * reached from SynthEngine by jack freewheeling NRPNs.
         * This avoids unnecessary (error prone) duplication.
         */
    public:
        void commandEffects(CommandBlock *getData);

    private:
        bool commandSendReal(CommandBlock *getData);
};

#endif
