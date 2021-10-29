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

void TextData::encodeAll(SynthEngine *_synth, std::string _source, CommandBlock &allData)
{
    oursynth = _synth;
    string source = _source;
    strip (source);
    if (source.empty())
    {
        log(source, "empty Command String");
        return;
    }
    //cout << ">" << source << endl;
    memset(&allData.bytes, 255, sizeof(allData));

    if (findAndStep(source, "Main"))
    {
        encodeMain(source, allData);
        return;
    }

    if (findAndStep(source, "System"))
    {
        allData.data.part = (TOPLEVEL::section::systemEffects);
        if (findAndStep(source, "Effect"))
            encodeEffects(source, allData);
        return;
    }

    if (findAndStep(source, "Insert"))
    {
        allData.data.part = (TOPLEVEL::section::insertEffects);
        if (findAndStep(source, "Effect"))
            encodeEffects(source, allData);
        return;
    }

    if (findAndStep(source, "Part"))
    {
        encodePart(source, allData);
        return;
    }
    log(source, "bad Command String");

}


void TextData::log(std::string &line, std::string text)
{
    oursynth->getRuntime().Log("Error: " + text);
    // we may later decide to print the string before emptying it

    line = "";
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

bool TextData::findAndStep(std::string &line, std::string text)
{
    size_t pos = line.find(text);
    if (pos != string::npos)
    {
        nextWord(line);
        return true;
    }
    return false;
}

int TextData::findListEntry(std::string &line, int step, std::string list [])
{
    int count = 0;
    bool found = false;
    std::string test;
    do {
        test = func::stringCaps(list [count], 1);
        size_t split = test.find(" ");
        if (split != string::npos)
            test = test.substr(0, split);
        found = findAndStep(line, test);
        if (!found)
            count += step;

    } while (!found && test != "@end");
    if (count > 0)
        count = count / step; // gives actual list position
    return count;
}


void TextData::encodeMain(std::string &source, CommandBlock &allData)
{
    strip (source);
    //cout << ">" << source << endl;
    allData.data.part = TOPLEVEL::section::main;
    if (findAndStep(source, "Master"))
    {
        if (findAndStep(source, "Mono/Stereo"))
        {
            nextWord(source);
            allData.data.control = MAIN::control::mono;
            return;
        }
    }
    if (findAndStep(source, "Volume"))
    {
        allData.data.control = MAIN::control::volume;
        return;
    }

    cout << "main overflow >" << source << endl;
}


void TextData::encodePart(std::string &source, CommandBlock &allData)
{
    strip (source);
    unsigned char npart = UNUSED;
    if (isdigit(source[0]))
    {
        npart = stoi(source) - 1;
        //cout << "part " << int(npart) << endl;
        if (npart >= NUM_MIDI_PARTS)
        {
            log(source, "part number out of range");
            return;
        }
        allData.data.part = (TOPLEVEL::section::part1 + npart);
        nextWord(source);
        if (findAndStep(source, "Effect"))
        {
            encodeEffects(source, allData);
            return;
        }
    }
    unsigned char kitnum = UNUSED;
    if (findAndStep(source, "Kit"))
    {
        if (isdigit(source[0]))
        {
            kitnum = stoi(source) - 1;
            if (kitnum >= NUM_KIT_ITEMS)
            {
                log(source, "kit number out of range");
                return;
            }
            nextWord(source);
            allData.data.kit = kitnum;
        }
    }

    if (findAndStep(source, "AddSynth"))
    {
        encodeAddSynth(source, allData);
        return;
    }
    if (findAndStep(source, "Add"))
    {
        if (findAndStep(source, "Voice"))
        {
            encodeAddVoice(source, allData);
            return;
        }
    }
    if (findAndStep(source, "SubSynth"))
    {
        encodeSubSynth(source, allData);
        return;
    }
    if (findAndStep(source, "PadSynth"))
    {
        encodePadSynth(source, allData);
        return;
    }
    cout << "part overflow >" << source << endl;
}


void TextData::encodeEffects(std::string &source, CommandBlock &allData)
{
    if (findAndStep(source, "Send"))
    {
        if (isdigit(source[0]))
        {
            unsigned char sendto = stoi(source) - 1;
            allData.data.control = PART::control::partToSystemEffect1 + sendto;
            nextWord(source);
            return;
        }
    }
    if (isdigit(source[0]))
    {
        unsigned char effnum = stoi(source) - 1; // need to find number ranges
        allData.data.engine = effnum;
        //cout << "effnum " << int(effnum) << endl;
        nextWord(source);

        if (allData.data.part < NUM_MIDI_PARTS)
        {
            if (findAndStep(source, "Bypass"))
            {
                allData.data.control = PART::control::effectBypass;
                allData.data.insert = TOPLEVEL::insert::partEffectSelect;
                return;
            }
        }
        if (allData.data.part == TOPLEVEL::section::systemEffects)
        {
            bool test = (source == "");
            if (!test)
            {
                test = (source.find("Enable") != string::npos);
                if (!test)
                    test = isdigit(source[0]);

            }
            if (test)
            {
                if (!isdigit(source[0]))
                    nextWord(source); // a number might be a value for later
                allData.data.control = EFFECT::sysIns::effectEnable;
                return;
            }
        }

        unsigned char efftype = findListEntry(source, 1, fx_list);
        if (efftype >= EFFECT::type::count - EFFECT::type::none)
        {
            log(source, "effect type out of range");
            return;
        }
        int effpos = efftype + EFFECT::type::none;
        allData.data.kit = efftype + EFFECT::type::none;

        // now need to do actual control
        unsigned char result = UNUSED;
        switch (effpos)
        {
            case EFFECT::type::reverb:
                result = findListEntry(source, 2, reverblist);
                if (result > 4) // no 5 or 6
                    result += 2;
                break;
            case EFFECT::type::echo:
                result = findListEntry(source, 2, echolist);
                if (result == 7) // skip unused numbers
                    result = EFFECT::control::bpm;
                break;
            case EFFECT::type::chorus:
                result = findListEntry(source, 2, choruslist);
                if (result >= 11) // skip unused numbers
                    result = result - 11 + EFFECT::control::bpm;
                break;
            case EFFECT::type::phaser:
                result = findListEntry(source, 2, phaserlist);
                if (result >= 15) // skip unused numbers
                    result = result - 15 + EFFECT::control::bpm;
                break;
            case EFFECT::type::alienWah:
                result = findListEntry(source, 2, alienwahlist);
                if (result >= 11) // skip unused numbers
                    result = result - 11 + EFFECT::control::bpm;
                break;
            case EFFECT::type::distortion:
                result = findListEntry(source, 2, distortionlist);
                if (result > 5) // extra line
                    result -= 1;
                break;
            case EFFECT::type::eq:
                result = findListEntry(source, 2, eqlist);
                if (result > 2) // extra line
                    result -= 1;
                break;
            case EFFECT::type::dynFilter:
                result = findListEntry(source, 2, dynfilterlist);
                if (result >= 11) // skip unused numbers
                    result = result - 11 + EFFECT::control::bpm;
                break;
            default:
                log(source, "effect control out of range");
                return;
        }
        //cout << "effnum " << int(effnum) << "  efftype " << int(efftype + EFFECT::type::none) << "  control " << int(result) << endl;
        allData.data.control = result;
        return;
    }
    cout << "effects overflow >" << source << endl;
}


void TextData::encodeAddSynth(std::string &source, CommandBlock &allData)
{

    cout << "addsynth overflow >" << source << endl;
}


void TextData::encodeAddVoice(std::string &source, CommandBlock &allData)
{
    cout << "addvoice overflow >" << source << endl;
}


void TextData::encodeSubSynth(std::string &source, CommandBlock &allData)
{
    cout << "subsynth overflow >" << source << endl;
}


void TextData::encodePadSynth(std::string &source, CommandBlock &allData)
{
    cout << "padsynth overflow >" << source << endl;
}
