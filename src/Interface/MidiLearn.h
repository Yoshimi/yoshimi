/*
    MidiLearn.h

    Copyright 2016-2019 Will Godfrey

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

    Modified April 2019
*/

#ifndef MIDILEARN_H
#define MIDILEARN_H

#include <jack/ringbuffer.h>
#include <list>
#include <string>

#include "Interface/FileMgr.h"
#include "Interface/InterChange.h"
#include "Interface/Data2Text.h"

class XMLwrapper;
class SynthEngine;
class DataText;

using std::string;

class MidiLearn : private FileMgr, DataText
{
    public:
        MidiLearn(SynthEngine *_synth);
        ~MidiLearn();
        bool saveXML(string filename); // true for load ok, otherwise false
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
        } data;

        struct LearnBlock{
            unsigned int CC;
            unsigned char chan;
            unsigned char min_in;
            unsigned char max_in;
            unsigned char status; // up to here must be specified on input
            int min_out; // defined programmatically
            int max_out; // defined programmatically
            Control data; // controller to learn
            string name; // derived from controller text
        };
        bool learning;

        void setTransferBlock(CommandBlock *getData, string name);

        bool runMidiLearn(int _value, unsigned int CC, unsigned char chan, unsigned char category);
        bool writeMidi(CommandBlock *putData, bool in_place);
        int findEntry(std::list<LearnBlock> &midi_list, int lastpos, unsigned int CC, unsigned char chan, LearnBlock *block, bool show);
        int findSize();
        void listLine(int lineNo);
        void listAll(std::list<string>& msg_buf);
        bool remove(int itemNumber);
        void generalOperations(CommandBlock *getData);
        bool saveList(string name);
        bool insertMidiListData(bool full,  XMLwrapper *xml);
        bool loadList(string name);
        bool extractMidiListData(bool full,  XMLwrapper *xml);
        void updateGui(int opp = 0);


    private:
        std::list<LearnBlock> midi_list;
        string learnedName;
        CommandBlock learnTransferBlock;

        void insert(unsigned int CC, unsigned char chan);
        SynthEngine *synth;
        void writeToGui(CommandBlock *putData);
};

#endif
