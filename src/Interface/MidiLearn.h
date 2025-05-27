/*
    MidiLearn.h

    Copyright 2016-2020, Will Godfrey

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

#ifndef MIDILEARN_H
#define MIDILEARN_H

#include "globals.h"

#include <list>
#include <string>

#include "Interface/InterChange.h"
#include "Interface/Data2Text.h"
#include "Interface/Text2Data.h"

class SynthEngine;
class DataText;
class TextData;
class XMLStore;

using std::string;
using std::list;


class MidiLearn : private DataText, TextData
{
        SynthEngine& synth;

    public:
       ~MidiLearn() = default;
        MidiLearn(SynthEngine&);
        // shall not be copied or moved or assigned
        MidiLearn(MidiLearn&&)                 = delete;
        MidiLearn(MidiLearn const&)            = delete;
        MidiLearn& operator=(MidiLearn&&)      = delete;
        MidiLearn& operator=(MidiLearn const&) = delete;

        //commandData
        CommandBlock data;

        //Control data
        struct LearnBlock{
            ushort CC{0};
            uchar chan{0};
            uchar min_in{0};
            uchar max_in{0};
            uchar status{0};    // up to here must be specified on input
            int min_out{0};     // defined programmatically
            int max_out{0};     // defined programmatically
            CommandBlock frame; // controller to learn
        };
        bool learning;

        void setTransferBlock(CommandBlock& getData);

        bool runMidiLearn(int _value, ushort CC, uchar chan, bool in_place);
        bool writeMidi(CommandBlock& putData, bool in_place);

        int  findSize();
        void listLine(int lineNo);
        void listAll(list<string>& msg_buf);
        bool remove(int itemNumber);
        void generalOperations(CommandBlock& getData);
        void insertMidiListData(XMLStore&);
        bool loadList(const string& name);
        bool extractMidiListData(XMLStore&);
        void updateGui(int opp = 0);


    private:
        list<LearnBlock> midi_list;
        string       learnedName;
        CommandBlock learnTransferBlock;

        int findEntry(list<LearnBlock>&, int lastpos, ushort CC, uchar chan, LearnBlock& block, bool show);
        string findName(LearnBlock&);
        void insertLine(ushort CC, uchar chan);
        bool saveList(string const& name);
        void writeToGui(CommandBlock& putData);
};

#endif /*MIDILEARN_H*/

