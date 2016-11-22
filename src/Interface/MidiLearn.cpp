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
#include <unistd.h>

using namespace std;

#include "Interface/MidiLearn.h"
#include "Interface/InterChange.h"
#include "Misc/MiscFuncs.h"
#include "Misc/SynthEngine.h"

MidiLearn::MidiLearn(SynthEngine *_synth) :
    learning(false),
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
    synth->getRuntime().Log("Learning");
}


bool MidiLearn::runMidiLearn(float value, unsigned char CC, unsigned char chan, bool in_place)
{
    if (learning)
    {
        insert(CC, chan);
        return true; // block while learning
    }
    //cout << "here" << endl;

    int lastpos = -1;
    LearnBlock foundEntry;
    while (lastpos != -2)
    {
        lastpos = findEntry(midi_list, lastpos, CC, chan, &foundEntry, false);
        //cout << "found " << lastpos << endl;
        int status = foundEntry.status;
        if (status & 4)
            continue;
        //cout << "CC " << (int)foundEntry.CC << "  chan " << (int)foundEntry.chan << endl;
        //cout << "Min " << (int)foundEntry.min_in << "  Max " << (int)foundEntry.max_in << endl;
        if (lastpos != -2)
        {
            int minIn = foundEntry.min_in;
            int maxIn = foundEntry.max_in;
            if (minIn > maxIn)
            {
                value = 127 - value;
                swap(minIn, maxIn);
            }

            if ((maxIn - minIn) != 127)
            {
                if (status & 2) // compress
                {
                    int range = maxIn - minIn;
                    value = (value * range / 127) + minIn;
                }
                else // limit
                {
                    if (value < minIn)
                        value = minIn;
                    else if (value > maxIn)
                        value = maxIn;
                }
            }


            CommandBlock putData;
            unsigned int writesize = sizeof(putData);
            putData.data.value = value;
            putData.data.type = 0x48;// write command from midi
            putData.data.control = foundEntry.data.control;
            putData.data.part = foundEntry.data.part;
            putData.data.kit = foundEntry.data.kit;
            putData.data.engine = foundEntry.data.engine;
            putData.data.insert = foundEntry.data.insert;
            putData.data.parameter = foundEntry.data.parameter;
            putData.data.par2 = foundEntry.data.par2;
            char *point = (char*)&putData;
            unsigned int towrite = writesize;
            unsigned int wrote = 0;
            unsigned int found;
            unsigned int tries = 0;

            if (jack_ringbuffer_write_space(synth->interchange.fromMIDI) >= writesize)
            {
                while (towrite && tries < 3)
                {
                    found = jack_ringbuffer_write(synth->interchange.fromMIDI, point, towrite);
                wrote += found;
                point += found;
                towrite -= found;
                ++tries;
                }
                if (towrite)
                    synth->getRuntime().Log("Unable to write data to fromMidi buffer", 2);
            }
            else
                synth->getRuntime().Log("fromMidi buffer full!", 2);
            if (lastpos == -1)
                return true;
        }
    }
    return false;
}

/*
 * This will only be called by incoming midi. It is the only function that
 * needs to be quick
 */
int MidiLearn::findEntry(list<LearnBlock> &midi_list, int lastpos, unsigned char CC, unsigned char chan, LearnBlock *block, bool show)
{
    int newpos = 0; // last comes in at -1 for the first call
    list<LearnBlock>::iterator it = midi_list.begin();

    while (newpos <= lastpos && it != midi_list.end())
    {
        ++ it;
        ++ newpos;
    }
    if (it == midi_list.end())
        return -2;

    while (CC >= it->CC && it != midi_list.end())
    {
        if ((it->chan >= 16 || chan == it->chan) && CC == it->CC)
        {
            if (show)
                synth->getRuntime().Log("Found line " + it->name + "  at " + to_string(newpos)); // a test!
            block->chan = it->chan;
            block->CC = it->CC;
            block->min_in = it->min_in;
            block->max_in = it->max_in;
            block->status = it->status;
            block->min_out = it->min_out;
            block->max_out = it->max_out;
            block->data = it->data;
            if (it->status & 1)
                return -1; // don't allow any more of this CC and channel;
            return newpos;
        }
        ++ it;
        ++ newpos;
    }
    return -2;
}


void MidiLearn::listAll()
{
    list<LearnBlock>::iterator it = midi_list.begin();
    int lineNo = 0;
    synth->getRuntime().Log("Midi learned:");
    while (it != midi_list.end())
    {
        synth->getRuntime().Log("Line " + to_string(lineNo) + "  CC " + to_string((int)it->CC) + "  Chan " + to_string((int)it->chan) + "  " + it->name);
        ++ it;
        ++ lineNo;
    }
}


bool MidiLearn::remove(int itemNumber)
{
    list<LearnBlock>::iterator it = midi_list.begin();
    int found = 0;
    while (found < itemNumber && it != midi_list.end())
    {
        ++ found;
        ++ it;
    }
    if (it != midi_list.end())
    {
        midi_list.erase(it);
        return true;
    }
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

    synth->getRuntime().Log("Learned ");
    synth->getRuntime().Log("CC " + to_string((int)entry.CC) + "  Chan " + to_string((int)entry.chan) + "  " + entry.name);
    learning = false;
}

