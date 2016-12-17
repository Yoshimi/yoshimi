/*
    InterChange.h - General communications

    Copyright 2016 Will Godfrey

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

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Params/LFOParams.h"
#include "Params/FilterParams.h"
#include "Params/EnvelopeParams.h"
#include "Synth/OscilGen.h"
#include "Synth/Resonance.h"

class SynthEngine;

class InterChange : private MiscFuncs
{
    public:
        InterChange(SynthEngine *_synth);
        ~InterChange();

        CommandBlock commandData;
        size_t commandSize = sizeof(commandData);

        jack_ringbuffer_t *fromCLI;
        jack_ringbuffer_t *toCLI;
        jack_ringbuffer_t *fromGUI;
        jack_ringbuffer_t *toGUI;
        jack_ringbuffer_t *fromMIDI;

        void mediate();
        void returns(CommandBlock *getData);
        void setpadparams(int point);
        void commandSend(CommandBlock *getData);
        void resolveReplies(CommandBlock *getData);
        void returnLimits(CommandBlock *getData);

    private:
        void *CLIresolvethread(void);
        static void *_CLIresolvethread(void *arg);
        pthread_t  CLIresolvethreadHandle;

        string resolveVector(CommandBlock *getData);
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
        string resolveSysIns(CommandBlock *getData);
        string resolveEffects(CommandBlock *getData);

        void commandVector(CommandBlock *getData);
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

        SynthEngine *synth;
};

#endif
