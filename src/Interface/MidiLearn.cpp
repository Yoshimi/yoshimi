/*
    MidiLearn.cpp

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

#include <iostream>
#include <bitset>
#include <unistd.h>
#include <list>
#include <string>

using namespace std;

#include "Interface/MidiLearn.h"
#include "Interface/InterChange.h"
#include "Misc/MiscFuncs.h"
#include "Misc/SynthEngine.h"

MidiLearn::MidiLearn(SynthEngine *_synth) :
    learning(true),
    synth(_synth)
{
 //init

}


MidiLearn::~MidiLearn()
{
    //close
}

void MidiLearn::setTransferBlock(unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2, string name)
{
    learnTransferBlock.type = type;
    learnTransferBlock.control = control;
    learnTransferBlock.part = part;
    learnTransferBlock.kit = kit;
    learnTransferBlock.engine = engine;
    learnTransferBlock.insert = insert;
    learnTransferBlock.parameter = parameter;
    learnTransferBlock.par2 = par2;
    learnedName = name;
    learning = true;
    cout << "Learning" << endl;
}

bool MidiLearn::runMidiLearn(float value, unsigned char CC, unsigned char chan, bool in_place)
{
    if (learning)
    {
        insert(CC, chan);
        return true;
    }
    //cout << "here" << endl;
    return false;
}

void MidiLearn::insert(unsigned char CC, unsigned char chan)
{
    list<LearnBlock>::iterator it;
    LearnBlock entry;
    entry.chan = chan;
    entry.CC = CC;
    entry.min_in = 0;
    entry.max_in = 127;
    entry.status = 0;
    entry.min_out = 0;
    entry.max_out = 127;
    entry.name = learnedName;

    entry.data = learnTransferBlock;

    it = midi_list.begin();

    if (midi_list.size() > 0)
    {
        while(CC >= it->CC && it != midi_list.end()) // CC is priority
            ++it;
        while(CC == it->CC && chan >= it->chan && it != midi_list.end())
            ++it;
    }
    if (it == midi_list.end())
        midi_list.push_back(entry);
    else
        midi_list.insert(it, entry);

    cout << "Learned " <<endl;
    it = midi_list.begin();
    while (it != midi_list.end())
    {
        cout << " CC " << (int)it->CC << "  Chan " << (int)it->chan << "  "<< it->name << endl;
        ++ it;
    }
    learning = false;
}
