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


class SynthEngine;

class InterChange : private MiscFuncs
{
    public:
        InterChange(SynthEngine *_synth);
        ~InterChange();

        void commandFetch(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char insertParam = 0xff);

        void commandSend(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char insertParam);

        void mediate();

        jack_ringbuffer_t *sendbuf;

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

    private:
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
        void commandEffects(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit);



        SynthEngine *synth;
};

#endif
