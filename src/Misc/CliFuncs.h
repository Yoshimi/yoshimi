/*
    CliFuncs.h

    Copyright 2019, Will Godfrey.

    Copyright 2021, Rainer Hans Liffers

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef CLIFUNCS_H
#define CLIFUNCS_H

#include <cmath>
#include <string>
#include <cstring>

#include <readline/readline.h>
#include <cassert>

#include "CLI/Parser.h"
#include "Interface/TextLists.h"
#include "Misc/SynthEngine.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/NumericFuncs.h"
#include "Misc/FormatFuncs.h"


namespace cli {

using func::bitTest;
using func::bitFindHigh;

using func::asString;

using std::string;


inline int contextToEngines(int context)
{
    int engine = UNUSED;
    if (bitTest(context, LEVEL::SubSynth))
        engine = PART::engine::subSynth;
    else if (bitTest(context, LEVEL::PadSynth))
        engine = PART::engine::padSynth;
    else if (bitTest(context, LEVEL::AddMod))
        engine = PART::engine::addMod1;
    else if (bitTest(context, LEVEL::AddVoice))
        engine = PART::engine::addVoice1;
    else if (bitTest(context, LEVEL::AddSynth))
        engine = PART::engine::addSynth;
    return engine;
}


inline float readControl(SynthEngine *synth,
                         unsigned char action, unsigned char control, unsigned char part,
                         unsigned char kit = UNUSED,
                         unsigned char engine = UNUSED,
                         unsigned char insert = UNUSED,
                         unsigned char parameter = UNUSED,
                         unsigned char offset = UNUSED,
                         unsigned char miscmsg = NO_MSG)
{
    float value;
    CommandBlock putData;

    putData.data.value = 0;
    putData.data.type = 0;
    putData.data.source = action;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.offset = offset;
    putData.data.miscmsg = miscmsg;
    value = synth->interchange.readAllData(&putData);
    //if (putData.data.type & TOPLEVEL::type::Error)
        //return 0xfffff;
        //std::cout << "err" << std::endl;
    return value;
}


inline string readControlText(SynthEngine *synth,
                              unsigned char action, unsigned char control, unsigned char part,
                              unsigned char kit = UNUSED,
                              unsigned char engine = UNUSED,
                              unsigned char insert = UNUSED,
                              unsigned char parameter = UNUSED,
                              unsigned char offset = UNUSED)
{
    float value;
    CommandBlock putData;

    putData.data.value = 0;
    putData.data.type = 0;
    putData.data.source = action;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.offset = offset;
    putData.data.miscmsg = UNUSED;
    value = synth->interchange.readAllData(&putData);
    return TextMsgBuffer::instance().fetch(value);
}


inline void readLimits(SynthEngine *synth,
                       float value, unsigned char type, unsigned char control, unsigned char part,
                       unsigned char kit, unsigned char engine, unsigned char insert,
                       unsigned char parameter, unsigned char miscmsg)
{
    CommandBlock putData;

    putData.data.value = value;
    putData.data.type = type;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.miscmsg = miscmsg;

    value = synth->interchange.readAllData(&putData);
    string name;
    switch (type & 3)
    {
        case TOPLEVEL::type::Minimum:
            name = "Min ";
            break;
        case TOPLEVEL::type::Maximum:
            name = "Max ";
            break;
        default:
            name = "Default ";
            break;
    }
    type = putData.data.type;
    if ((type & TOPLEVEL::type::Integer) == 0)
        name += std::to_string(value);
    else if (value < 0)
        name += std::to_string(int(value - 0.5f));
    else
        name += std::to_string(int(value + 0.5f));
    if (type & TOPLEVEL::type::Error)
        name += " - error";
    else if (type & TOPLEVEL::type::Learnable)
        name += " - learnable";
    synth->getRuntime().Log(name);
}


inline int sendNormal(SynthEngine *synth,
                      unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part,
                      unsigned char kit = UNUSED,
                      unsigned char engine = UNUSED,
                      unsigned char insert = UNUSED,
                      unsigned char parameter = UNUSED,
                      unsigned char offset = UNUSED,
                      unsigned char miscmsg = NO_MSG)
{
    if ((type & TOPLEVEL::type::Limits) && part != TOPLEVEL::section::midiLearn)
    {
        readLimits(synth, value, type, control, part, kit, engine, insert, parameter, miscmsg);
        return REPLY::done_msg;
    }
    action |= TOPLEVEL::action::fromCLI;

    CommandBlock putData;

    putData.data.value = value;
    putData.data.type = type;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.offset = offset;
    putData.data.miscmsg = miscmsg;

    /*
     * MIDI learn settings are synced by the audio thread
     * but not passed on to any of the normal controls.
     * The type field is used for a different purpose.
     */

    if (part != TOPLEVEL::section::midiLearn)
    {
        putData.data.type |= TOPLEVEL::type::Limits;
        float newValue = synth->interchange.readAllData(&putData);
        if (type & TOPLEVEL::type::LearnRequest)
        {
            if ((putData.data.type & TOPLEVEL::type::Learnable) == 0)
            {
            synth->getRuntime().Log("Can't learn this control");
            return REPLY::failed_msg;
            }
        }
        else
        {
            if (putData.data.type & TOPLEVEL::type::Error)
                return REPLY::available_msg;
            if (newValue != value && (type & TOPLEVEL::type::Write))
            { // checking the original type not the reported one
                putData.data.value = newValue;
                synth->getRuntime().Log("Range adjusted");
            }
        }
        action |= TOPLEVEL::action::fromCLI;
    }
    putData.data.source = action;
    putData.data.type = type;
    if (synth->interchange.fromCLI.write(putData.bytes))
    {
        synth->getRuntime().finishedCLI = false;
    }
    else
    {
        synth->getRuntime().Log("Unable to write to fromCLI buffer");
        return REPLY::failed_msg;
    }
    return REPLY::done_msg;
}


inline int sendDirect(SynthEngine *synth,
                      unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part,
                      unsigned char kit = UNUSED,
                      unsigned char engine = UNUSED,
                      unsigned char insert = UNUSED,
                      unsigned char parameter = UNUSED,
                      unsigned char offset = UNUSED,
                      unsigned char miscmsg = NO_MSG,
                      unsigned char request = UNUSED)
{
    if (action == TOPLEVEL::action::fromMIDI && part != TOPLEVEL::section::midiLearn)
        request = type & TOPLEVEL::type::Default;
    CommandBlock putData;

    putData.data.value = value;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.offset = offset;
    putData.data.miscmsg = miscmsg;

    if (type == TOPLEVEL::type::Default)
    {
        putData.data.type = TOPLEVEL::type::Limits;
        synth->interchange.readAllData(&putData);
        if ((putData.data.type & TOPLEVEL::type::Learnable) == 0)
        {
            synth->getRuntime().Log("Can't learn this control");
            return 0;
        }
    }

    if (part != TOPLEVEL::section::midiLearn)
        action |= TOPLEVEL::action::fromCLI;
    /*
     * MIDI learn is synced by the audio thread but
     * not passed on to any of the normal controls.
     * The type field is used for a different purpose.
     */
    putData.data.source = action | TOPLEVEL::action::fromCLI;
    putData.data.type = type;
    if (request < TOPLEVEL::type::Limits)
    {
        putData.data.type = request | TOPLEVEL::type::Limits;
        value = synth->interchange.readAllData(&putData);
        string name;
        switch (request)
        {
            case TOPLEVEL::type::Minimum:
                name = "Min ";
                break;
            case TOPLEVEL::type::Maximum:
                name = "Max ";
                break;
            default:
                name = "Default ";
                break;
        }
        type = putData.data.type;
        if ((type & TOPLEVEL::type::Integer) == 0)
            name += std::to_string(value);
        else if (value < 0)
            name += std::to_string(int(value - 0.5f));
        else
            name += std::to_string(int(value + 0.5f));
        if (type & TOPLEVEL::type::Error)
            name += " - error";
        else if (type & TOPLEVEL::type::Learnable)
            name += " - learnable";
        synth->getRuntime().Log(name);
        return 0;
    }

    if (part == TOPLEVEL::section::main && (type & TOPLEVEL::type::Write) == 0 && control >= MAIN::control::readPartPeak && control <= MAIN::control::readMainLRrms)
    {
        string name;
        switch (control)
        {
            case MAIN::control::readPartPeak:
                name = "part " + std::to_string(int(kit));
                if (engine == 0)
                    name += "L ";
                else
                    name += "R ";
                name += "peak ";
                break;
            case MAIN::control::readMainLRpeak:
                name = "main ";
                if (kit == 0)
                    name += "L ";
                else
                    name += "R ";
                name += "peak ";
                break;
            case MAIN::control::readMainLRrms:
                name = "main ";
                if (kit == 0)
                    name += "L ";
                else
                    name += "R ";
                name += "RMS ";
                break;
            }
            value = synth->interchange.readAllData(&putData);
            synth->getRuntime().Log(name + std::to_string(value));
        return 0;
    }

    if (part == TOPLEVEL::section::config && putData.data.miscmsg != UNUSED && (control == CONFIG::control::bankRootCC || control == CONFIG::control::bankCC || control == CONFIG::control::extendedProgramChangeCC))
    {
        synth->getRuntime().Log("In use by " + TextMsgBuffer::instance().fetch(putData.data.miscmsg) );
        return 0;
    }

    if (parameter != UNUSED && (parameter & TOPLEVEL::action::lowPrio))
        action |= (parameter & TOPLEVEL::action::muteAndLoop); // transfer low prio and loopback
    putData.data.source = action;

    if (synth->interchange.fromCLI.write(putData.bytes))
    {
        synth->getRuntime().finishedCLI = false;
    }
    else
        synth->getRuntime().Log("Unable to write to fromCLI buffer");
    return 0; // no function for this yet
}


}//(End)namespace cli
#endif /*CLIFUNCS_H*/
