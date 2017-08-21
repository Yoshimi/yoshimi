/*
    InterChange.h - General communications

    Copyright 2016-2017 Will Godfrey

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

    Modified July 2017
*/

#ifndef INTERCH_H
#define INTERCH_H

#include <jack/ringbuffer.h>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Params/LFOParams.h"
#include "Params/FilterParams.h"
#include "Params/EnvelopeParams.h"
#include "Synth/OscilGen.h"
#include "Synth/Resonance.h"

//#include "Interface/MidiDecode.h"
class SynthEngine;
//class MidiDecode;

class InterChange : private MiscFuncs
{
    public:
        InterChange(SynthEngine *_synth);
        ~InterChange();
        bool Init(SynthEngine *_synth);

        CommandBlock commandData;
        size_t commandSize = sizeof(commandData);

        jack_ringbuffer_t *fromCLI;
        jack_ringbuffer_t *toCLI;
        jack_ringbuffer_t *fromGUI;
        jack_ringbuffer_t *toGUI;
        jack_ringbuffer_t *fromMIDI;
        jack_ringbuffer_t *returnsLoopback;

        void mediate(void);
        void returnsDirect(int altData);
        void returns(CommandBlock *getData);
        void setpadparams(int point);
        void doClearPart(int npart);
        bool commandSend(CommandBlock *getData);
        void resolveReplies(CommandBlock *getData);
        void testLimits(CommandBlock *getData);
        void returnLimits(CommandBlock *getData);

        void flagsWrite(unsigned int val){__sync_and_and_fetch(&flagsValue, val);}

    private:
        unsigned int flagsValue;
        unsigned int flagsRead(){return __sync_add_and_fetch(&flagsValue, 0);}
        unsigned int flagsReadClear(){ return __sync_fetch_and_or(&flagsValue, 0xffffffff);}

        void *sortResultsThread(void);
        static void *_sortResultsThread(void *arg);
        pthread_t  sortResultsThreadHandle;
        void transfertext(CommandBlock *getData);
        string formatScales(string text);
        string resolveVector(CommandBlock *getData);
        string resolveMicrotonal(CommandBlock *getData);
        string resolveConfig(CommandBlock *getData);
        string resolveMain(CommandBlock *getData);
        string resolvePart(CommandBlock *getData);
        string resolveAdd(CommandBlock *getData);
        string resolveAddVoice(CommandBlock *getData);
        string resolveSub(CommandBlock *getData);
        string resolvePad(CommandBlock *getData);
        string resolveOscillator(CommandBlock *getData);
        string resolveResonance(CommandBlock *getData);
        string resolveLFO(CommandBlock *getData);
        string resolveFilter(CommandBlock *getData);
        string resolveEnvelope(CommandBlock *getData);
        string resolveEffects(CommandBlock *getData);
        bool showValue;

        void commandVector(CommandBlock *getData);
        void commandMicrotonal(CommandBlock *getData);
        void commandConfig(CommandBlock *getData);
        void commandMain(CommandBlock *getData);
        void commandPart(CommandBlock *getData);
        void commandAdd(CommandBlock *getData);
        void commandAddVoice(CommandBlock *getData);
        void commandSub(CommandBlock *getData);
        void commandPad(CommandBlock *getData);
        void commandOscillator(CommandBlock *getData, OscilGen *oscil);
        void commandResonance(CommandBlock *getData, Resonance *respar);
        void commandLFO(CommandBlock *getData);
        void lfoReadWrite(CommandBlock *getData, LFOParams *pars);
        void commandFilter(CommandBlock *getData);
        void filterReadWrite(CommandBlock *getData, FilterParams *pars, unsigned char *velsnsamp, unsigned char *velsns);
        void commandEnvelope(CommandBlock *getData);
        void envelopeReadWrite(CommandBlock *getData, EnvelopeParams *pars);
        void commandSysIns(CommandBlock *getData);
        void commandEffects(CommandBlock *getData);

        bool commandSendReal(CommandBlock *getData);

        SynthEngine *synth;
};

#endif
