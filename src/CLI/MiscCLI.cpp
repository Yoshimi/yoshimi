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
#include <readline/readline.h>

#include "CLI/MiscCLI.h"
#include "Misc/TextMsgBuffer.h"


bool MiscCli::lineEnd(char * point, unsigned char controlType)
{
    return (point[0] == 0 && controlType == TOPLEVEL::type::Write);
    // so all other controls aren't tested
    // e.g. you don't need to send a value when you're reading it!
}


int MiscCli::toggle(char *point)
{
    if (matchnMove(2, point, "enable") || matchnMove(2, point, "on") || matchnMove(3, point, "yes"))
        return 1;
    if (matchnMove(2, point, "disable") || matchnMove(3, point, "off") || matchnMove(2, point, "no") )
        return 0;
    return -1;
    /*
     * this allows you to specify enable or other, disable or other or must be those specifics
     */
}


int MiscCli::contextToEngines(int context)
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


bool MiscCli::query(std::string text, bool priority)
{
    char *line = NULL;
    std::string suffix;
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
    return TextMsgBuffer::instance().miscMsgPop(value);
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
        synth->getRuntime().Log("In use by " + TextMsgBuffer::instance().miscMsgPop(putData.data.miscmsg) );
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

std::string MiscCli::findStatus(SynthEngine *synth, int context, bool show)
{
    std::string text = "";
    int kit = UNUSED;
    int insert = UNUSED;

    if (bitTest(context, LEVEL::AllFX))
    {
        int section;
        int ctl = EFFECT::sysIns::effectType;
        if (bitTest(context, LEVEL::Part))
        {
            text = " p" + std::to_string(int(npart) + 1);
            if (readControl(synth, 0, PART::control::enable, npart))
                text += "+";
            ctl = PART::control::effectType;
            section = npart;
        }
        else if (bitTest(context, LEVEL::InsFX))
        {
            text += " Ins";
            section = TOPLEVEL::section::insertEffects;
        }
        else
        {
            text += " Sys";
            section = TOPLEVEL::section::systemEffects;
        }
        nFXtype = readControl(synth, 0, ctl, section, UNUSED, nFX);
        text += (" eff " + asString(nFX + 1) + " " + fx_list[nFXtype].substr(0, 6));
        nFXpreset = readControl(synth, 0, EFFECT::control::preset, section,  EFFECT::type::none + nFXtype, nFX);

        if (bitTest(context, LEVEL::InsFX) && readControl(synth, 0, EFFECT::sysIns::effectDestination, TOPLEVEL::section::systemEffects, UNUSED, nFX) == -1)
            text += " Unrouted";
        else if (nFXtype > 0 && nFXtype != 7)
        {
            text += ("-" + asString(nFXpreset + 1));
            if (readControl(synth, 0, EFFECT::control::changed, section,  EFFECT::type::none + nFXtype, nFX))
                text += "?";
        }
        return text;
    }

    if (bitTest(context, LEVEL::Part))
    {
        bool justPart = false;
        text = " p";
        kitMode = readControl(synth, 0, PART::control::kitMode, npart);
        if (bitFindHigh(context) == LEVEL::Part)
        {
            justPart = true;
            if (kitMode == PART::kitType::Off)
                text = " Part ";
        }
        text += std::to_string(int(npart) + 1);
        if (readControl(synth, 0, PART::control::enable, npart))
            text += "+";
        if (kitMode != PART::kitType::Off)
        {
            kit = kitNumber;
            insert = TOPLEVEL::insert::kitGroup;
            text += ", ";
            std::string front = "";
            std::string back = " ";
            if (!inKitEditor)
            {
                front = "(";
                back = ")";
            }
            switch (kitMode)
            {
                case PART::kitType::Multi:
                    if (justPart)
                        text += (front + "Multi" + back);
                    else
                        text += "M";
                    break;
                case PART::kitType::Single:
                    if (justPart)
                        text += (front + "Single" + back);
                    else
                        text += "S";
                    break;
                case PART::kitType::CrossFade:
                    if (justPart)
                        text += (front + "Crossfade" + back);
                    else
                        text += "C";
                    break;
                default:
                    break;
            }
            if (inKitEditor)
            {
                text += std::to_string(kitNumber + 1);
                if (readControl(synth, 0, PART::control::enable, npart, kitNumber, UNUSED, insert))
                    text += "+";
            }
        }
        else
            kitNumber = 0;
        if (!show)
            return "";

        if (bitFindHigh(context) == LEVEL::MControl)
            return text +" Midi controllers";

        int engine = contextToEngines(context);
        switch (engine)
        {
            case PART::engine::addSynth:
                if (bitFindHigh(context) == LEVEL::AddSynth)
                    text += ", Add";
                else
                    text += ", A";
                if (readControl(synth, 0, ADDSYNTH::control::enable, npart, kit, PART::engine::addSynth, insert))
                    text += "+";
                break;
            case PART::engine::subSynth:
                if (bitFindHigh(context) == LEVEL::SubSynth)
                    text += ", Sub";
                else
                    text += ", S";
                if (readControl(synth, 0, SUBSYNTH::control::enable, npart, kit, PART::engine::subSynth, insert))
                    text += "+";
                break;
            case PART::engine::padSynth:
                if (bitFindHigh(context) == LEVEL::PadSynth)
                    text += ", Pad";
                else
                    text += ", P";
                if (readControl(synth, 0, PADSYNTH::control::enable, npart, kit, PART::engine::padSynth, insert))
                    text += "+";
                break;
            case PART::engine::addVoice1: // intentional drop through
            case PART::engine::addMod1:
            {
                text += ", A";
                if (readControl(synth, 0, ADDSYNTH::control::enable, npart, kit, PART::engine::addSynth, insert))
                    text += "+";

                if (bitFindHigh(context) == LEVEL::AddVoice)
                    text += ", Voice ";
                else
                    text += ", V";
                text += std::to_string(voiceNumber + 1);
                voiceFromNumber = readControl(synth, 0, ADDVOICE::control::voiceOscillatorSource, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
                if (voiceFromNumber > -1)
                    text += (">" +std::to_string(voiceFromNumber + 1));
                voiceFromNumber = readControl(synth, 0, ADDVOICE::control::externalOscillator, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
                if (voiceFromNumber > -1)
                    text += (">V" +std::to_string(voiceFromNumber + 1));
                if (readControl(synth, 0, ADDVOICE::control::enableVoice, npart, kitNumber, PART::engine::addVoice1 + voiceNumber))
                    text += "+";

                if (bitTest(context, LEVEL::AddMod))
                {
                    text += ", ";
                    int tmp = readControl(synth, 0, ADDVOICE::control::modulatorType, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
                    if (tmp > 0)
                    {
                        std::string word = "";
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
                            text += (word + " Mod ");
                        else
                            text += word.substr(0, 2);

                        modulatorFromVoiceNumber = readControl(synth, 0, ADDVOICE::control::externalModulator, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
                        if (modulatorFromVoiceNumber > -1)
                            text += (">V" + std::to_string(modulatorFromVoiceNumber + 1));
                        else
                        {
                            modulatorFromNumber = readControl(synth, 0, ADDVOICE::control::modulatorOscillatorSource, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
                            if (modulatorFromNumber > -1)
                                text += (">" + std::to_string(modulatorFromNumber + 1));
                        }
                    }
                    else
                        text += "Modulator";
                }
                break;
            }
        }
        if (bitFindHigh(context) == LEVEL::Resonance)
        {
            text += ", Resonance";
            if (readControl(synth, 0, RESONANCE::control::enableResonance, npart, kitNumber, engine, TOPLEVEL::insert::resonanceGroup))
            text += "+";
        }
        else if (bitTest(context, LEVEL::Oscillator))
            text += (" " + waveshape[(int)readControl(synth, 0, OSCILLATOR::control::baseFunctionType, npart, kitNumber, engine + voiceNumber, TOPLEVEL::insert::oscillatorGroup)]);

        if (bitTest(context, LEVEL::LFO))
        {
            text += ", LFO ";
            int cmd = -1;
            switch (insertType)
            {
                case TOPLEVEL::insertType::amplitude:
                    cmd = ADDVOICE::control::enableAmplitudeLFO;
                    text += "amp";
                    break;
                case TOPLEVEL::insertType::frequency:
                    cmd = ADDVOICE::control::enableFrequencyLFO;
                    text += "freq";
                    break;
                case TOPLEVEL::insertType::filter:
                    cmd = ADDVOICE::control::enableFilterLFO;
                    text += "filt";
                    break;
            }

            if (engine == PART::engine::addVoice1)
            {
                if (readControl(synth, 0, cmd, npart, kitNumber, engine + voiceNumber))
                    text += "+";
            }
            else
                text += "+";
        }
        else if (bitTest(context, LEVEL::Filter))
        {
            int baseType = readControl(synth, 0, FILTERINSERT::control::baseType, npart, kitNumber, engine, TOPLEVEL::insert::filterGroup);
            text += ", Filter ";
            switch (baseType)
            {
                case 0:
                    text += "analog";
                    break;
                case 1:
                    text += "formant V";
                    text += std::to_string(filterVowelNumber);
                    text += " F";
                    text += std::to_string(filterFormantNumber);
                    break;
                case 2:
                    text += "state var";
                    break;
            }
            if (engine == PART::engine::subSynth)
            {
                if (readControl(synth, 0, SUBSYNTH::control::enableFilter, npart, kitNumber, engine))
                    text += "+";
            }
            else if (engine == PART::engine::addVoice1)
            {
                if (readControl(synth, 0, ADDVOICE::control::enableFilter, npart, kitNumber, engine + voiceNumber))
                    text += "+";
            }
            else
                text += "+";
        }
        else if (bitTest(context, LEVEL::Envelope))
        {
            text += ", Envel ";
            int cmd = -1;
            switch (insertType)
            {
                case TOPLEVEL::insertType::amplitude:
                    if(engine == PART::engine::addMod1)
                        cmd = ADDVOICE::control::enableModulatorAmplitudeEnvelope;
                    else
                        cmd = ADDVOICE::control::enableAmplitudeEnvelope;
                    text += "amp";
                    break;
                case TOPLEVEL::insertType::frequency:
                    if(engine == PART::engine::addMod1)
                        cmd = ADDVOICE::control::enableModulatorFrequencyEnvelope;
                    else
                        cmd = ADDVOICE::control::enableFrequencyEnvelope;
                    text += "freq";
                    break;
                case TOPLEVEL::insertType::filter:
                    cmd = ADDVOICE::control::enableFilterEnvelope;
                    text += "filt";
                    break;
                case TOPLEVEL::insertType::bandwidth:
                    cmd = SUBSYNTH::control::enableBandwidthEnvelope;
                    text += "band";
                    break;
            }

            if (readControl(synth, 0, ENVELOPEINSERT::control::enableFreeMode, npart, kitNumber, engine, TOPLEVEL::insert::envelopeGroup, insertType))
                text += " free";
            if (engine == PART::engine::addVoice1  || engine == PART::engine::addMod1 || (engine == PART::engine::subSynth && cmd != ADDVOICE::control::enableAmplitudeEnvelope && cmd != ADDVOICE::control::enableFilterEnvelope))
            {
                if (readControl(synth, 0, cmd, npart, kitNumber, engine + voiceNumber))
                    text += "+";
            }
            else
                text += "+";
        }
    }
    else if (bitTest(context, LEVEL::Scale))
        text += " Scale ";
    else if (bitTest(context, LEVEL::Config))
        text += " Config ";
    else if (bitTest(context, LEVEL::Vector))
    {
        text += (" Vect Ch " + asString(chan + 1) + " ");
        if (axis == 0)
            text += "X";
        else
            text += "Y";
    }
    else if (bitTest(context, LEVEL::Learn))
        text += (" MLearn line " + asString(mline + 1) + " ");

    return text;
}



