/*
    MidiDecode.h

    Copyright 2017-2020, Will Godfrey

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

*/

#ifndef MIDIDECODE_H
#define MIDIDECODE_H


#include "Interface/InterChange.h"

#include <list>

class SynthEngine;

class MidiDecode
{
    public:
       ~MidiDecode() = default;
        MidiDecode(SynthEngine *_synth);
        void midiProcess(uchar par0, uchar par1, uchar par2, bool in_place, bool inSync = false);
        void setMidiBankOrRootDir(uint bank_or_root_num, bool in_place = false, bool setRootDir = false);
        void setMidiProgram(uchar ch, int prg, bool in_place = false);

    private:
        void setMidiController(uchar ch, int ctrl, int param, bool in_place = false, bool inSync = false);
        void sendMidiCC(bool inSync, uchar chan, int type, short par);
        bool nrpnDecode(uchar ch, int ctrl, int param, bool in_place);
        bool nrpnRunVector(uchar ch, int ctrl, int param, bool inSync);
        void nrpnProcessData(uchar chan, int type, int par, bool in_place);
        bool nrpnProcessHistory(uchar nLow, uchar dHigh, uchar dLow, bool in_place);
        void nrpnDirectPart(int dHigh, int par);
        void nrpnSetVector(int dHigh, uchar chan,  int par);

        SynthEngine *synth;
};

#endif
