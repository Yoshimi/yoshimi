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

class SynthEngine;

class InterChange : private MiscFuncs
{
    public:
        InterChange(SynthEngine *_synth);
        ~InterChange();

        union CommandBlock{
            struct{
                float value;
                unsigned char type;
                unsigned char control;
                unsigned char part;
                unsigned char kit;
                unsigned char engine;
                unsigned char insert;
                unsigned char parameter;
            } data;
            unsigned char bytes [sizeof(data)];
        };
        CommandBlock commandData;
        size_t commandSize = sizeof(commandData);

        jack_ringbuffer_t *sendbuf;

        void commandFetch(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char insertParam = 0xff);

        void mediate();
        void commandSend(CommandBlock *getData);

    private:
        void commandVector(CommandBlock *getData);
        void commandMain(CommandBlock *getData);
        void commandPart(CommandBlock *getData);
        void commandAdd(CommandBlock *getData);
        void commandAddVoice(CommandBlock *getData);
        void commandSub(CommandBlock *getData);
        void commandPad(CommandBlock *getData);
        void commandOscillator(CommandBlock *getData);
        void commandResonance(CommandBlock *getData);
        void commandLFO(CommandBlock *getData);
        void commandFilter(CommandBlock *getData);
        float filterReadWrite(CommandBlock *getData, FilterParams *pars, unsigned char *velsnsamp, unsigned char *velsns);
        void commandEnvelope(CommandBlock *getData);
        void commandSysIns(CommandBlock *getData);
        void commandEffects(CommandBlock *getData);

        SynthEngine *synth;
};

#endif
