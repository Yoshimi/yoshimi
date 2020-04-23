/*
    MidiLearn.h

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

#ifndef MIDILEARN_H
#define MIDILEARN_H

#include <list>
#include <string>

#include "Interface/InterChange.h"
#include "Interface/Data2Text.h"

class XMLwrapper;
class SynthEngine;
class DataText;

using std::string;
using std::list;

class MidiLearn : private DataText
{
    public:
        MidiLearn(SynthEngine *_synth);
        ~MidiLearn();
        void add2XML(XMLwrapper *xml);
        void getfromXML(XMLwrapper *xml);
        CommandBlock commandData;

        struct Control{
            unsigned char type;
            unsigned char control;
            unsigned char part;
            unsigned char kit;
            unsigned char engine;
            unsigned char insert;
            unsigned char parameter;
            unsigned char miscmsg;
        };

        Control data;

        struct LearnBlock{
            unsigned short int CC;
            unsigned char chan;
            unsigned char min_in;
            unsigned char max_in;
            unsigned char status; // up to here must be specified on input
            int min_out; // defined programmatically
            int max_out; // defined programmatically
            Control data; // controller to learn
        };
        bool learning;

        void setTransferBlock(CommandBlock *getData);

        bool runMidiLearn(int _value, unsigned short int CC, unsigned char chan, bool in_place);
        bool writeMidi(CommandBlock *putData, bool in_place);

        int findSize();
        void listLine(int lineNo);
        void listAll(list<string>& msg_buf);
        bool remove(int itemNumber);
        void generalOperations(CommandBlock *getData);
        bool insertMidiListData(XMLwrapper *xml);
        bool loadList(const string& name);
        bool extractMidiListData(bool full,  XMLwrapper *xml);
        void updateGui(int opp = 0);


    private:
        list<LearnBlock> midi_list;
        string learnedName;
        CommandBlock learnTransferBlock;
        int findEntry(list<LearnBlock> &midi_list, int lastpos, unsigned short int CC, unsigned char chan, LearnBlock *block, bool show);
        string findName(list<LearnBlock>::iterator it);
        void insertLine(unsigned short int CC, unsigned char chan);
        bool saveList(const string& name);
        SynthEngine *synth;
        void writeToGui(CommandBlock *putData);
};

#endif
