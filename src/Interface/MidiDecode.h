/*
    MidiDecode.h

    Copyright 2017 - 2020 Will Godfrey

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

#ifndef MIDIDECODE_H
#define MIDIDECODE_H

#include <jack/ringbuffer.h>
#include <list>
#include <string>

#include "Interface/InterChange.h"

class SynthEngine;

class MidiDecode
{
    public:
        MidiDecode(SynthEngine *_synth);
        ~MidiDecode();
        void midiProcess(unsigned char par0, unsigned char par1, unsigned char par2, bool in_place, bool inSync = false);
        void setMidiBankOrRootDir(unsigned int bank_or_root_num, bool in_place = false, bool setRootDir = false);
        void setMidiProgram(unsigned char ch, int prg, bool in_place = false);

    private:
        void setMidiController(unsigned char ch, int ctrl, int param, bool in_place = false, bool inSync = false);
        void sendMidiCC(bool inSync, unsigned char chan, int type, short int par);
        bool nrpnDecode(unsigned char ch, int ctrl, int param, bool in_place);
        bool nrpnRunVector(unsigned char ch, int ctrl, int param, bool inSync);
        void nrpnProcessData(unsigned char chan, int type, int par, bool in_place);
        bool nrpnProcessHistory(unsigned char nLow, unsigned char dHigh, unsigned char dLow, bool in_place);
        void nrpnDirectPart(int dHigh, int par);
        void nrpnSetVector(int dHigh, unsigned char chan,  int par);

        SynthEngine *synth;
};

#endif
