/*
    Text2Data.cpp - conversion of text to commandBlock entries

    Copyright 2021, Will Godfrey

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
#include <stdlib.h>

#include "Interface/Text2Data.h"
#include "Interface/TextLists.h"
#include "Misc/SynthEngine.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/FormatFuncs.h"
#include "Misc/NumericFuncs.h"

using std::string;
using std::cout;
using std::endl;

void TextData::log(std::string text)
{
    oursynth->getRuntime().Log("Error: " + text);
}

void TextData::encodeAll(SynthEngine *_synth, std::string _source, CommandBlock &allData)
{
    oursynth = _synth;
    string source = _source;
    strip (source);
    if (source.empty())
    {
        log("empty Command String");
        return;
    }
    //cout << ">" << source << endl;
    memset(&allData.bytes, 255, sizeof(allData));

    size_t pos = source.find("Main");
    if (pos != string::npos)
    {
        encodeMain(source.substr(pos + 4), allData);
        return;
    }

    pos = source.find("System");
    if (pos != string::npos)
    {
        allData.data.part = (TOPLEVEL::section::systemEffects);
        nextWord(source);
        pos = source.find("Effect");
        if (pos != string::npos)
        {
            nextWord(source);
            encodeEffects(source, allData);
        }
        return;
    }

    pos = source.find("Insert");
    if (pos != string::npos)
    {
        allData.data.part = (TOPLEVEL::section::insertEffects);
        nextWord(source);
        pos = source.find("Effect");
        if (pos != string::npos)
        {
            nextWord(source);
            encodeEffects(source, allData);
        }return;
    }

    pos = source.find("Part");
    if (pos != string::npos)
    {
        encodePart(source.substr(pos + 4), allData);
        return;
    }
    log("bad Command String");
}


void TextData::strip(std::string &line)
{
    size_t pos = line.find_first_not_of(" ");
    if (pos == string::npos)
        line = "";
    else
        line = line.substr(pos);
}


void TextData::nextWord(std::string &line)
{
    size_t pos = line.find_first_of(" ");
    if (pos == string::npos)
    {
        line = "";
        return;
    }
    line = line.substr(pos);
    strip(line);
}


void TextData::encodeMain(std::string source, CommandBlock &allData)
{
    strip (source);
    cout << ">" << source << endl;
    allData.data.part = TOPLEVEL::section::main;
}


void TextData::encodePart(std::string source, CommandBlock &allData)
{
    strip (source);
    unsigned char npart = UNUSED;
    if (isdigit(source[0]))
    {
        npart = stoi(source);
        cout << "part " << int(npart) << endl;
        if (npart >= NUM_MIDI_PARTS)
        {
            log("part number out of range");
            return;
        }
        allData.data.part = (TOPLEVEL::section::part1 + npart);
        nextWord(source);
        size_t pos = source.find("Effect");
        if (pos != string::npos)
        {
            nextWord(source);
            encodeEffects(source, allData);
            return;
        }
    }
    cout << ">" << source << endl;
}


void TextData::encodeEffects(std::string source, CommandBlock &allData)
{
    if (isdigit(source[0]))
    {
        unsigned char effnum = stoi(source); // need to find number ranges
        allData.data.engine = effnum;
        //cout << "effnum " << int(effnum) << endl;
        nextWord(source);

        if (source == "" || source == "Bypass")
            return;

        unsigned char efftype = 0;
        bool found = false;
        //cout << source << endl;
        std::string test;
        do {
            test = func::stringCaps(fx_list [efftype], 1);
            //cout << test << endl;
            if (source.find(test) != string::npos)
            {
                nextWord(source);
                found = true;
            }
            else
                ++ efftype;

        } while (!found && test != "@end");
        if (efftype >= EFFECT::type::count - EFFECT::type::none)
        {
            log("effect type out of range");
            return;
        }
        allData.data.kit = efftype + EFFECT::type::none;
        cout << "effnum " << int(effnum) << "  efftype " << int(efftype + EFFECT::type::none) << endl;
        // now need to do actual control
    }
    cout << ">" << source << endl;
}
