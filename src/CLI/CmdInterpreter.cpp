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
#include "Misc/CliFuncs.h"

using func::bitTest;
using func::bitFindHigh;
using func::asString;

using cli::matchnMove;

using cli::readControl;
using cli::contextToEngines;

using std::string;


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
