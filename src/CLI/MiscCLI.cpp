/*
    MiscCLI.cpp

    Copyright 2019, Will Godfrey.

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


#include "Misc/SynthEngine.h"
#include "Misc/MiscFuncs.h"
#include "CLI/MiscCLI.h"


float MiscCli::readControl(SynthEngine *synth, unsigned char action, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset, unsigned char miscmsg)
{
    float value;
    CommandBlock putData;

    putData.data.value.F = 0;
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


std::string MiscCli::readControlText(SynthEngine *synth, unsigned char action, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset)
{
    float value;
    CommandBlock putData;

    putData.data.value.F = 0;
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
    return miscMsgPop(value);
}


void MiscCli::readLimits(SynthEngine *synth, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char miscmsg)
{
    CommandBlock putData;

    putData.data.value.F = value;
    putData.data.type = type;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.miscmsg = miscmsg;

    value = synth->interchange.readAllData(&putData);
    std::string name;
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


int MiscCli::sendNormal(SynthEngine *synth, unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset, unsigned char miscmsg)
{
    if ((type & TOPLEVEL::type::Limits) && part != TOPLEVEL::section::midiLearn)
    {
        readLimits(synth, value, type, control, part, kit, engine, insert, parameter, miscmsg);
        return REPLY::done_msg;
    }
    action |= TOPLEVEL::action::fromCLI;

    CommandBlock putData;

    putData.data.value.F = value;
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
                putData.data.value.F = newValue;
                synth->getRuntime().Log("Range adjusted");
            }
        }
        action |= TOPLEVEL::action::fromCLI;
    }
    putData.data.source = action;
    putData.data.type = type;
    if (synth->interchange.fromCLI->write(putData.bytes))
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


int MiscCli::sendDirect(SynthEngine *synth, unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset, unsigned char miscmsg, unsigned char request)
{
    if (action == TOPLEVEL::action::fromMIDI && part != TOPLEVEL::section::midiLearn)
        request = type & TOPLEVEL::type::Default;
    CommandBlock putData;

    putData.data.value.F = value;
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
        std::string name;
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
        std::string name;
        switch (control)
        {
            case MAIN::control::readPartPeak:
                name = "part " + std::to_string(int(kit)) + " peak ";
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
        synth->getRuntime().Log("In use by " + miscMsgPop(putData.data.miscmsg) );
        return 0;
    }

    if (parameter != UNUSED && (parameter & TOPLEVEL::action::lowPrio))
        action |= (parameter & TOPLEVEL::action::muteAndLoop); // transfer low prio and loopback
    putData.data.source = action;

    if (synth->interchange.fromCLI->write(putData.bytes))
    {
        synth->getRuntime().finishedCLI = false;
    }
    else
        synth->getRuntime().Log("Unable to write to fromCLI buffer");
    return 0; // no function for this yet
}

