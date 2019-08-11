/*
    CliFuncs.h

    Copyright 2010, Alan Calvert
    Copyright 2014-2019, Will Godfrey

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

    Modified August 2019
*/

#ifndef CLIFUNCS_H
#define CLIFUNCS_H

#include <cmath>
#include <string>
#include <cstring>


/*
    CmdInterpreter.cpp

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
#include <readline/readline.h>
#include <cassert>

#include "CLI/CmdInterpreter.h"
#include "CLI/Parser.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/NumericFuncs.h"
#include "Misc/FormatFuncs.h"

using func::bitTest;
using func::bitFindHigh;

using func::asString;
using cli::matchnMove;

using std::string;


namespace cli {

int CmdInterpreter::contextToEngines(int context)
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


bool CmdInterpreter::query(string text, bool priority)
{
    char *line = NULL;
    string suffix;
    char result;
    char test;

    priority = !priority; // so calls make more sense

    if (priority)
    {
        suffix = " N/y? ";
        test = 'n';
    }
    else
    {
        suffix = " Y/n? ";
        test = 'y';
    }
    result = test;
    text += suffix;
    line = readline(text.c_str());
    if (line)
    {
        if (line[0] != 0)
            result = line[0];
        free(line);
        line = NULL;
    }
    return (((result | 32) == test) ^ priority);
}


float CmdInterpreter::readControl(SynthEngine *synth, unsigned char action, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset, unsigned char miscmsg)
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


string CmdInterpreter::readControlText(SynthEngine *synth, unsigned char action, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset)
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
    return TextMsgBuffer::instance().fetch(value);
}


void CmdInterpreter::readLimits(SynthEngine *synth, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char miscmsg)
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


int CmdInterpreter::sendNormal(SynthEngine *synth, unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset, unsigned char miscmsg)
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


int CmdInterpreter::sendDirect(SynthEngine *synth, unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset, unsigned char miscmsg, unsigned char request)
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
        synth->getRuntime().Log("In use by " + TextMsgBuffer::instance().fetch(putData.data.miscmsg) );
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



string CmdInterpreter::buildStatus(SynthEngine *synth, int context, bool showPartDetails)
{
    if (bitTest(context, LEVEL::AllFX))
    {
        return buildAllFXStatus(synth, context);
    }
    if (bitTest(context, LEVEL::Part))
    {
        return buildPartStatus(synth, context, showPartDetails);
    }

    string result = "";

    if (bitTest(context, LEVEL::Scale))
        result += " Scale ";
    else if (bitTest(context, LEVEL::Config))
        result += " Config ";
    else if (bitTest(context, LEVEL::Vector))
    {
        result += (" Vect Ch " + asString(chan + 1) + " ");
        if (axis == 0)
            result += "X";
        else
            result += "Y";
    }
    else if (bitTest(context, LEVEL::Learn))
        result += (" MLearn line " + asString(mline + 1) + " ");

    return result;
}



string CmdInterpreter::buildAllFXStatus(SynthEngine *synth, int context)
{
    assert(bitTest(context, LEVEL::AllFX));

    string result = "";
    int section;
    int ctl = EFFECT::sysIns::effectType;
    if (bitTest(context, LEVEL::Part))
    {
        result = " p" + std::to_string(int(npart) + 1);
        if (readControl(synth, 0, PART::control::enable, npart))
            result += "+";
        ctl = PART::control::effectType;
        section = npart;
    }
    else if (bitTest(context, LEVEL::InsFX))
    {
        result += " Ins";
        section = TOPLEVEL::section::insertEffects;
    }
    else
    {
        result += " Sys";
        section = TOPLEVEL::section::systemEffects;
    }
    nFXtype = readControl(synth, 0, ctl, section, UNUSED, nFX);
    result += (" eff " + asString(nFX + 1) + " " + fx_list[nFXtype].substr(0, 6));
    nFXpreset = readControl(synth, 0, EFFECT::control::preset, section,  EFFECT::type::none + nFXtype, nFX);

    if (bitTest(context, LEVEL::InsFX) && readControl(synth, 0, EFFECT::sysIns::effectDestination, TOPLEVEL::section::systemEffects, UNUSED, nFX) == -1)
        result += " Unrouted";
    else if (nFXtype > 0 && nFXtype != 7)
    {
        result += ("-" + asString(nFXpreset + 1));
        if (readControl(synth, 0, EFFECT::control::changed, section,  EFFECT::type::none + nFXtype, nFX))
            result += "?";
    }
    return result;
}


string CmdInterpreter::buildPartStatus(SynthEngine *synth, int context, bool showPartDetails)
{
    assert(bitTest(context, LEVEL::Part));

    int kit = UNUSED;
    int insert = UNUSED;
    bool justPart = false;
    string result = " p";

    kitMode = readControl(synth, 0, PART::control::kitMode, npart);
    if (bitFindHigh(context) == LEVEL::Part)
    {
        justPart = true;
        if (kitMode == PART::kitType::Off)
            result = " Part ";
    }
    result += std::to_string(int(npart) + 1);
    if (readControl(synth, 0, PART::control::enable, npart))
        result += "+";
    if (kitMode != PART::kitType::Off)
    {
        kit = kitNumber;
        insert = TOPLEVEL::insert::kitGroup;
        result += ", ";
        string front = "";
        string back = " ";
        if (!inKitEditor)
        {
            front = "(";
            back = ")";
        }
        switch (kitMode)
        {
            case PART::kitType::Multi:
                if (justPart)
                    result += (front + "Multi" + back);
                else
                    result += "M";
                break;
            case PART::kitType::Single:
                if (justPart)
                    result += (front + "Single" + back);
                else
                    result += "S";
                break;
            case PART::kitType::CrossFade:
                if (justPart)
                    result += (front + "Crossfade" + back);
                else
                    result += "C";
                break;
            default:
                break;
        }
        if (inKitEditor)
        {
            result += std::to_string(kitNumber + 1);
            if (readControl(synth, 0, PART::control::enable, npart, kitNumber, UNUSED, insert))
                result += "+";
        }
    }
    else
        kitNumber = 0;
    if (!showPartDetails)
        return "";

    if (bitFindHigh(context) == LEVEL::MControl)
        return result +" Midi controllers";

    int engine = contextToEngines(context);
    switch (engine)
    {
        case PART::engine::addSynth:
            if (bitFindHigh(context) == LEVEL::AddSynth)
                result += ", Add";
            else
                result += ", A";
            if (readControl(synth, 0, ADDSYNTH::control::enable, npart, kit, PART::engine::addSynth, insert))
                result += "+";
            break;
        case PART::engine::subSynth:
            if (bitFindHigh(context) == LEVEL::SubSynth)
                result += ", Sub";
            else
                result += ", S";
            if (readControl(synth, 0, SUBSYNTH::control::enable, npart, kit, PART::engine::subSynth, insert))
                result += "+";
            break;
        case PART::engine::padSynth:
            if (bitFindHigh(context) == LEVEL::PadSynth)
                result += ", Pad";
            else
                result += ", P";
            if (readControl(synth, 0, PADSYNTH::control::enable, npart, kit, PART::engine::padSynth, insert))
                result += "+";
            break;
        case PART::engine::addVoice1: // intentional drop through
        case PART::engine::addMod1:
        {
            result += ", A";
            if (readControl(synth, 0, ADDSYNTH::control::enable, npart, kit, PART::engine::addSynth, insert))
                result += "+";

            if (bitFindHigh(context) == LEVEL::AddVoice)
                result += ", Voice ";
            else
                result += ", V";
            result += std::to_string(voiceNumber + 1);
            int voiceFromNumber = readControl(synth, 0, ADDVOICE::control::voiceOscillatorSource, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
            if (voiceFromNumber > -1)
                result += (">" +std::to_string(voiceFromNumber + 1));
            voiceFromNumber = readControl(synth, 0, ADDVOICE::control::externalOscillator, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
            if (voiceFromNumber > -1)
                result += (">V" +std::to_string(voiceFromNumber + 1));
            if (readControl(synth, 0, ADDVOICE::control::enableVoice, npart, kitNumber, PART::engine::addVoice1 + voiceNumber))
                result += "+";

            if (bitTest(context, LEVEL::AddMod))
            {
                result += ", ";
                int tmp = readControl(synth, 0, ADDVOICE::control::modulatorType, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
                if (tmp > 0)
                {
                    string word = "";
                    switch (tmp)
                    {
                        case 1:
                            word = "Morph";
                            break;
                        case 2:
                            word = "Ring";
                            break;
                        case 3:
                            word = "Phase";
                            break;
                        case 4:
                            word = "Freq";
                            break;
                        case 5:
                            word = "Pulse";
                            break;
                    }

                    if (bitFindHigh(context) == LEVEL::AddMod)
                        result += (word + " Mod ");
                    else
                        result += word.substr(0, 2);

                    int modulatorFromVoiceNumber = readControl(synth, 0, ADDVOICE::control::externalModulator, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
                    if (modulatorFromVoiceNumber > -1)
                        result += (">V" + std::to_string(modulatorFromVoiceNumber + 1));
                    else
                    {
                        int modulatorFromNumber = readControl(synth, 0, ADDVOICE::control::modulatorOscillatorSource, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
                        if (modulatorFromNumber > -1)
                            result += (">" + std::to_string(modulatorFromNumber + 1));
                    }
                }
                else
                    result += "Modulator";
            }
            break;
        }
    }
    if (bitFindHigh(context) == LEVEL::Resonance)
    {
        result += ", Resonance";
        if (readControl(synth, 0, RESONANCE::control::enableResonance, npart, kitNumber, engine, TOPLEVEL::insert::resonanceGroup))
        result += "+";
    }
    else if (bitTest(context, LEVEL::Oscillator))
        result += (" " + waveshape[(int)readControl(synth, 0, OSCILLATOR::control::baseFunctionType, npart, kitNumber, engine + voiceNumber, TOPLEVEL::insert::oscillatorGroup)]);

    if (bitTest(context, LEVEL::LFO))
    {
        result += ", LFO ";
        int cmd = -1;
        switch (insertType)
        {
            case TOPLEVEL::insertType::amplitude:
                cmd = ADDVOICE::control::enableAmplitudeLFO;
                result += "amp";
                break;
            case TOPLEVEL::insertType::frequency:
                cmd = ADDVOICE::control::enableFrequencyLFO;
                result += "freq";
                break;
            case TOPLEVEL::insertType::filter:
                cmd = ADDVOICE::control::enableFilterLFO;
                result += "filt";
                break;
        }

        if (engine == PART::engine::addVoice1)
        {
            if (readControl(synth, 0, cmd, npart, kitNumber, engine + voiceNumber))
                result += "+";
        }
        else
            result += "+";
    }
    else if (bitTest(context, LEVEL::Filter))
    {
        int baseType = readControl(synth, 0, FILTERINSERT::control::baseType, npart, kitNumber, engine, TOPLEVEL::insert::filterGroup);
        result += ", Filter ";
        switch (baseType)
        {
            case 0:
                result += "analog";
                break;
            case 1:
                result += "formant V";
                result += std::to_string(filterVowelNumber);
                result += " F";
                result += std::to_string(filterFormantNumber);
                break;
            case 2:
                result += "state var";
                break;
        }
        if (engine == PART::engine::subSynth)
        {
            if (readControl(synth, 0, SUBSYNTH::control::enableFilter, npart, kitNumber, engine))
                result += "+";
        }
        else if (engine == PART::engine::addVoice1)
        {
            if (readControl(synth, 0, ADDVOICE::control::enableFilter, npart, kitNumber, engine + voiceNumber))
                result += "+";
        }
        else
            result += "+";
    }
    else if (bitTest(context, LEVEL::Envelope))
    {
        result += ", Envel ";
        int cmd = -1;
        switch (insertType)
        {
            case TOPLEVEL::insertType::amplitude:
                if(engine == PART::engine::addMod1)
                    cmd = ADDVOICE::control::enableModulatorAmplitudeEnvelope;
                else
                    cmd = ADDVOICE::control::enableAmplitudeEnvelope;
                result += "amp";
                break;
            case TOPLEVEL::insertType::frequency:
                if(engine == PART::engine::addMod1)
                    cmd = ADDVOICE::control::enableModulatorFrequencyEnvelope;
                else
                    cmd = ADDVOICE::control::enableFrequencyEnvelope;
                result += "freq";
                break;
            case TOPLEVEL::insertType::filter:
                cmd = ADDVOICE::control::enableFilterEnvelope;
                result += "filt";
                break;
            case TOPLEVEL::insertType::bandwidth:
                cmd = SUBSYNTH::control::enableBandwidthEnvelope;
                result += "band";
                break;
        }

        if (readControl(synth, 0, ENVELOPEINSERT::control::enableFreeMode, npart, kitNumber, engine, TOPLEVEL::insert::envelopeGroup, insertType))
            result += " free";
        if (engine == PART::engine::addVoice1  || engine == PART::engine::addMod1 || (engine == PART::engine::subSynth && cmd != ADDVOICE::control::enableAmplitudeEnvelope && cmd != ADDVOICE::control::enableFilterEnvelope))
        {
            if (readControl(synth, 0, cmd, npart, kitNumber, engine + voiceNumber))
                result += "+";
        }
        else
            result += "+";
    }

    return result;
}


}//(End)namespace cli
#endif /*CLIFUNCS_H*/
