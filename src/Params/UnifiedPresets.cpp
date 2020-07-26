/*
    UnifiedPresets.cpp - Presets and Clipboard management

    Copyright 2018-2019 Will Godfrey

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

#include <string>

#include "Misc/SynthEngine.h"

string UnifiedPresets::findSectionName(CommandBlock *getData)
{
    unsigned char value = int (getData->data.value + 0.5f);
    int engine = getData->data.engine;
    int insert = getData->data.insert;
    string name = "unrecognised";

    if (getData->data.part != TOPLEVEL::section::copyPaste || value >= NUM_MIDI_PARTS)
        return name;

    if (insert != UNUSED) // temp!
        return name;

    if (engine >= PART::engine::addVoice1 && engine <= PART::engine::addVoice8)
    {
        string name = "VOICE id=\"" + std::to_string(int(engine - PART::engine::addVoice1)) + "\"";
        return name;
    }
    switch (engine)
    {
        case PART::engine::addSynth:
            return "ADD_SYNTH_PARAMETERS";
            break;
        case PART::engine::subSynth:
            return "SUB_SYNTH_PARAMETERS";
            break;
        case PART::engine::padSynth:
            return "PAD_SYNTH_PARAMETERS";
            break;

    }
    return name;
}


string UnifiedPresets::findleafExtension(CommandBlock *getData)
{
    int engine = getData->data.engine;
    int insert = getData->data.insert;
    string name = "unrecognised";

    if (insert != UNUSED) // temp!
        return name;

    if (engine >= PART::engine::addVoice1 && engine <= PART::engine::addVoice8)
        return "addsythn"; // all voices have the same extension
    switch (engine)
    {
        case PART::engine::addSynth:
            return "addsyth";
            break;
        case PART::engine::subSynth:
            return "subsyth";
            break;
        case PART::engine::padSynth:
            return "padsyth";
            break;
    }
    return name;
}


