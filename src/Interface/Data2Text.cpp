/*
    Data2Text.cpp - conversion of commandBlock entries to text

    Copyright 2021 - 2023, Will Godfrey
    Copyright 2024, Will Godfrey, Kristian Amlie

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
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

/*                            **** WARNING ****
 *
 * Text2Data tracks many of these conversions - principally to be able to interpret
 * MIDI-learn files.
 *
 * If you change any of the text you must check whether Text2Data uses them, and if
 * it does, ensure that it carries *both* the old and new versions.
 */

#include "Interface/Data2Text.h"
#include "Interface/TextLists.h"
#include "Misc/SynthEngine.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/FormatFuncs.h"
#include "Misc/NumericFuncs.h"

using std::string;
using std::to_string;

using func::string2int;
using func::stringCaps;
using func::bpm2text;

DataText::DataText()
    : showValue{false}
    , yesno{false}
    , textMsgBuffer{TextMsgBuffer::instance()}
    { }

string DataText::withValue(string resolved, uchar type, bool showValue, bool addValue, float value)
{
    if (!addValue)
        return resolved;

    if (yesno)
    {
        if (value)
            resolved += " - on";
        else
            resolved += " - off";
        return resolved;
    }

    if (showValue)
    {
        resolved += " Value ";
        if (type & TOPLEVEL::type::Integer)
            resolved += to_string(lrint(value));
        else
            resolved += to_string(value);
        return resolved;
    }

    return resolved;
}


string DataText::resolveAll(SynthEngine& synth, CommandBlock& cmd, bool addValue)
{
    float value   = cmd.data.value;
    uchar type    = cmd.data.type;
    //   (source)
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar effSend = cmd.data.kit;       // (note: also the kit)
    uchar engine  = cmd.data.engine;
    uchar insert  = cmd.data.insert;
    //   (parameter)
    //   (offset)
    //   (miscmsg)

    if (control == TOPLEVEL::control::textMessage) // special case for simple messages
    {
        synth.getRuntime().Log(textMsgBuffer.fetch(lrint(value)));
        synth.getRuntime().finishedCLI = true;
        return "";
    }

    showValue = true;
    yesno = false;
    string commandName;

    if (npart == TOPLEVEL::section::vector)
    {
        commandName = resolveVector(cmd, addValue);
        return withValue(commandName, type, showValue, addValue, value);
    }
    if (npart == TOPLEVEL::section::scales)
    {
        commandName = resolveMicrotonal(cmd, addValue);
        return withValue(commandName, type, showValue, addValue, value);
    }
    if (npart == TOPLEVEL::section::config)
    {
        commandName = resolveConfig(synth, cmd, addValue);
        return withValue(commandName, type, showValue, addValue, value);
    }
    if (npart == TOPLEVEL::section::bank)
    {
        commandName = resolveBank(cmd, addValue);
        return withValue(commandName, type, showValue, addValue, value);
    }
    if (npart == TOPLEVEL::section::midiIn || npart == TOPLEVEL::section::main)
    {
        commandName = resolveMain(cmd, addValue);
        return withValue(commandName, type, showValue, addValue, value);
    }

    if (npart == TOPLEVEL::section::systemEffects || npart == TOPLEVEL::section::insertEffects)
    {
        commandName = resolveEffects(cmd, addValue);
        return withValue(commandName, type, showValue, addValue, value);
    }

    if (npart == TOPLEVEL::section::undoMark)
    {
        if (control == MAIN::undo)
            return "Nothing to undo!";
        else if (control == MAIN::redo)
            return "Nothing to redo!";
    }

    if ((effSend >= EFFECT::type::none && effSend < EFFECT::type::count) || (control >= PART::control::effectNumber && control <= PART::control::effectBypass && effSend == UNUSED))
    {
        commandName = resolveEffects(cmd, addValue);
        return withValue(commandName, type, showValue, addValue, value);
    }

    if (npart >= NUM_MIDI_PARTS)
        return "Invalid part " + to_string(int(npart) + 1);

    if (kititem >= NUM_KIT_ITEMS && kititem < UNUSED)
        return "Invalid kit " + to_string(int(kititem) + 1);

    if (kititem == UNUSED || insert == TOPLEVEL::insert::kitGroup)
    {
        commandName = resolvePart(cmd, addValue);
        return withValue(commandName, type, showValue, addValue, value);
    }

    if (engine == PART::engine::padSynth)
    {
        switch(insert)
        {
            case UNUSED:
                commandName = resolvePad(synth, cmd, addValue);
                break;
            case TOPLEVEL::insert::LFOgroup:
                commandName = resolveLFO(cmd, addValue);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandName = resolveFilter(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopePointAdd:
            case TOPLEVEL::insert::envelopePointDelete:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::oscillatorGroup:
                commandName = resolveOscillator(synth, cmd, addValue);
                break;
            case TOPLEVEL::insert::harmonicAmplitude:
                commandName = resolveOscillator(synth, cmd, addValue);
                break;
            case TOPLEVEL::insert::harmonicPhase:
                commandName = resolveOscillator(synth, cmd, addValue);
                break;
            case TOPLEVEL::insert::resonanceGroup:
                commandName = resolveResonance(synth, cmd, addValue);
                break;
            case TOPLEVEL::insert::resonanceGraphInsert:
                commandName = resolveResonance(synth, cmd, addValue);
                break;
        }
        return withValue(commandName, type, showValue, addValue, value);
    }

    if (engine == PART::engine::subSynth)
    {
        switch (insert)
        {
            case UNUSED:
                commandName = resolveSub(cmd, addValue);
                break;
            case TOPLEVEL::insert::harmonicAmplitude:
                commandName = resolveSub(cmd, addValue);
                break;
            case TOPLEVEL::insert::harmonicBandwidth:
                commandName = resolveSub(cmd, addValue);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandName = resolveFilter(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopePointAdd:
            case TOPLEVEL::insert::envelopePointDelete:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandName = resolveEnvelope(cmd, addValue);
                break;
        }
        return withValue(commandName, type, showValue, addValue, value);
    }

    if (engine >= PART::engine::addVoice1)
    {
        switch (insert)
        {
            case UNUSED:
                commandName = resolveAddVoice(cmd, addValue);
                break;
            case TOPLEVEL::insert::LFOgroup:
                commandName = resolveLFO(cmd, addValue);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandName = resolveFilter(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopePointAdd:
            case TOPLEVEL::insert::envelopePointDelete:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::oscillatorGroup:
                commandName = resolveOscillator(synth, cmd, addValue);
                break;
            case TOPLEVEL::insert::harmonicAmplitude:
                commandName = resolveOscillator(synth, cmd, addValue);
                break;
            case TOPLEVEL::insert::harmonicPhase:
                commandName = resolveOscillator(synth, cmd, addValue);
                break;
        }
        return withValue(commandName, type, showValue, addValue, value);
    }

    if (engine == PART::engine::addSynth)
    {
        switch (insert)
        {
            case UNUSED:
                commandName = resolveAdd(cmd, addValue);
                break;
            case TOPLEVEL::insert::LFOgroup:
                commandName = resolveLFO(cmd, addValue);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandName = resolveFilter(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopePointAdd:
            case TOPLEVEL::insert::envelopePointDelete:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandName = resolveEnvelope(cmd, addValue);
                break;
            case TOPLEVEL::insert::resonanceGroup:
                commandName = resolveResonance(synth, cmd, addValue);
                break;
            case TOPLEVEL::insert::resonanceGraphInsert:
                commandName = resolveResonance(synth, cmd, addValue);
                break;
        }
    }
    return withValue(commandName, type, showValue, addValue, value);
}


string DataText::resolveVector(CommandBlock& cmd, bool addValue)
{
    int value_int = lrint(cmd.data.value);
    uchar control = cmd.data.control;
    uint  chan    = cmd.data.parameter;

    bool isFeature{false};
    string contstr;
    switch (control)
    {
        case VECTOR::control::name:
            showValue = false;
            contstr = "Name " + textMsgBuffer.fetch(value_int);
            break;

        case VECTOR::control::Xcontroller:
            contstr = "Controller";
            break;
        case VECTOR::control::XleftInstrument:
            contstr = "Left Instrument";
            break;
        case VECTOR::control::XrightInstrument:
            contstr = "Right Instrument";
            break;
        case VECTOR::control::Xfeature0:
        case VECTOR::control::Yfeature0:
            contstr = "Volume";
            isFeature = true;
            break;
        case VECTOR::control::Xfeature1:
        case VECTOR::control::Yfeature1:
            contstr = "Panning";
            isFeature = true;
            break;
        case VECTOR::control::Xfeature2:
        case VECTOR::control::Yfeature2:
            contstr = "Filter";
            isFeature = true;
            break;
        case VECTOR::control::Xfeature3:
        case VECTOR::control::Yfeature3:
            contstr = "Modulation";
            isFeature = true;
            break;

        case VECTOR::control::Ycontroller:
            contstr = "Controller";
            break;
        case VECTOR::control::YupInstrument:
            contstr = "Up Instrument";
            break;
        case VECTOR::control::YdownInstrument:
            contstr = "Down Instrument";
            break;

        case VECTOR::control::erase:
            showValue = false;
            if (chan > NUM_MIDI_CHANNELS)
                contstr = "all channels";
            else
                contstr = "chan " + to_string(chan + 1);
            if (addValue)
                return("Vector cleared on " + contstr);
            break;

        case 127:
            break;

        default:
            showValue = false;
            contstr = "Unrecognised Vector";
            break;
    }

    if (control == VECTOR::control::undefined)
    {
        showValue = false;
        return("Vector " + contstr + " set to " + to_string(chan + 1));
    }
    string name = "Vector Chan " + to_string(chan + 1) + " ";
    if (control == 127)
        name += " all ";
    else if (control >= VECTOR::control::Ycontroller)
        name += "Y ";
    else if (control >= VECTOR::control::Xcontroller)
        name += "X ";

    if (isFeature)
    {
        showValue = false;
        switch (value_int)
        {
            case 0:
                contstr += " off";
                break;
            case 1:
                contstr += " on";
                break;
            case 2:
                contstr += " reverse";
                break;
        }
    }

    return (name + contstr);
}


string DataText::resolveMicrotonal(CommandBlock& cmd, bool addValue)
{
    int value       = cmd.data.value;
    uchar control   = cmd.data.control;
    uchar parameter = cmd.data.parameter;

    string contstr;
    switch (control)
    {
        case SCALES::control::refFrequency:
            if (addValue)
            {
                if (parameter >= 21 && parameter <= 84)
                    contstr = noteslist[parameter - 21];
                else
                    contstr = to_string(parameter);
            }
            contstr += " Frequency";
            break;
        case SCALES::control::refNote:
            showValue = false;
            contstr = "Ref note ";
            if (addValue)
            {
                contstr += to_string(value);
                if (value >= 21 && value <= 84)
                    contstr += " " + noteslist[value - 21];
            }
            break;
        case SCALES::control::invertScale:
            contstr = "Invert Keys";
            yesno = true;
            break;
        case SCALES::control::invertedScaleCenter:
            contstr = "Key Center";
            break;
        case SCALES::control::scaleShift:
            contstr = "Scale Shift";
            break;
        case SCALES::control::enableMicrotonal:
            contstr = "Enable Microtonal";
            yesno = true;
            break;

        case SCALES::control::enableKeyboardMap:
            contstr = "Enable Keyboard Mapping";
            yesno = true;
            break;
        case SCALES::control::lowKey:
            contstr = "Keyboard First Note";
            break;
        case SCALES::control::middleKey:
            contstr = "Keyboard Middle Note";
            break;
        case SCALES::control::highKey:
            contstr = "Keyboard Last Note";
            break;

        case SCALES::control::tuning:
            contstr = "Tuning ";
            showValue = false;
            break;
        case SCALES::control::keyboardMap:
            contstr = "Keymap ";
            showValue = false;
            break;
        case SCALES::control::keymapSize:
            contstr = "Keymap Size ";
            break;
        case SCALES::control::importScl:
            contstr = "Tuning Import ";
            showValue = false;
            break;
        case SCALES::control::importKbm:
            contstr = "Keymap Import ";
            showValue = false;
            break;

        case SCALES::control::exportScl:
            contstr = "Tuning Export ";
            showValue = false;
            break;
        case SCALES::control::exportKbm:
            contstr = "Keymap Export ";
            showValue = false;
            break;

        case SCALES::control::name:
            contstr = "Name: ";
            if (addValue)
                contstr += textMsgBuffer.fetch(cmd.data.miscmsg, false);
            showValue = false;
            break;
        case SCALES::control::comment:
            contstr = "Description: ";
            if (addValue)
                contstr += textMsgBuffer.fetch(cmd.data.miscmsg, false);
            showValue = false;
            break;

        case SCALES::control::clearAll:
            contstr = "Clear all settings";
            showValue = false;
            break;

        default:
            showValue = false;
            contstr = "Unrecognised Microtonal";
            break;

    }

    if (value < 1 and
        (  control == SCALES::control::tuning
        or control == SCALES::control::keyboardMap
        or control == SCALES::control::importScl
        or control == SCALES::control::importKbm
        ))
    // errors :@(
    contstr += scale_errors[0-value];
    return ("Scales " + contstr);
}

string DataText::resolveConfig(SynthEngine& synth, CommandBlock& cmd, bool addValue)
{
    float value     = cmd.data.value;
    int value_int   = lrint(value);
    bool value_bool = _SYS_::F2B(value);
    uchar control   = cmd.data.control;
    uchar kititem   = cmd.data.kit;
    uchar parameter = cmd.data.parameter;
    bool  write     = cmd.data.type & TOPLEVEL::type::Write;

    string contstr;
    switch (control)
    {
        case CONFIG::control::oscillatorSize:
            contstr = "AddSynth oscillator size";
            break;
        case CONFIG::control::bufferSize:
            contstr = "Internal buffer size";
            break;
        case CONFIG::control::padSynthInterpolation:
            contstr = "PadSynth interpolation ";
            if (addValue)
            {
                if (value_bool)
                    contstr += "cubic";
                else
                    contstr += "linear";
            }
            showValue = false;
            break;
        case CONFIG::control::handlePadSynthBuild:
            contstr = "PADSynth wavetable build ";
            if (addValue)
            {
                switch (value_int)
                {
                    case 0:
                        contstr += "Muted";
                        break;
                    case 1:
                        contstr += "Background";
                        break;
                    case 2:
                        contstr += "AutoApply";
                        break;
                }
            }
            showValue = false;
            break;
        case CONFIG::control::virtualKeyboardLayout:
            contstr = "Virtual keyboard ";
            if (addValue)
            {
                switch (value_int)
                {
                    case 0:
                        contstr += "QWERTY";
                        break;
                    case 1:
                        contstr += "Dvorak";
                        break;
                    case 2:
                        contstr += "QWERTZ";
                        break;
                    case 3:
                        contstr += "AZERTY";
                        break;
                }
            }
            showValue = false;
            break;
        case CONFIG::control::XMLcompressionLevel:
            contstr = "XML compression";
            break;
        case CONFIG::control::enablePartReports:
            contstr = "part_changed_reports";
            break;
        case CONFIG::control::reportsDestination:
            contstr = "Reports to ";
            if (addValue)
            {
                if (value_bool)
                    contstr += "Console window";
                else
                    contstr += "stdout";
            }
            showValue = false;
            break;
        case CONFIG::control::logTextSize:
            contstr = "Console text size";
            break;
        case CONFIG::control::savedInstrumentFormat:
            contstr = "Saved instrument format ";
            if (addValue)
            {
                switch (value_int)
                {
                    case 1:
                        contstr += "Legacy (.xiz)";
                        break;
                    case 2:
                        contstr += "Yoshimi (.xiy)";
                        break;
                    case 3:
                        contstr += "Both";
                        break;
                }
            }
            showValue = false;
            break;
        case CONFIG::control::defaultStateStart:
            contstr += "Autoload default state";
            yesno = true;
            break;
        case CONFIG::control::enableSinglePath:
            contstr += "Single master instance";
            yesno = true;
            break;
        case CONFIG::control::hideNonFatalErrors:
            contstr += "Hide non-fatal errors";
            yesno = true;
            break;
        case CONFIG::control::showSplash:
            contstr += "Show splash screen";
            yesno = true;
            break;
        case CONFIG::control::logInstrumentLoadTimes:
            contstr += "Log instrument load times";
            yesno = true;
            break;
        case CONFIG::control::logXMLheaders:
            contstr += "Log XML headers";
            yesno = true;
            break;
        case CONFIG::control::saveAllXMLdata:
            contstr += "Save ALL XML data";
            yesno = true;
            break;
        case CONFIG::control::enableGUI:
            contstr += "Enable GUI";
            yesno = true;
            break;
        case CONFIG::control::enableCLI:
            contstr += "Enable CLI";
            yesno = true;
            break;
        case CONFIG::control::enableAutoInstance:
            contstr += "Enable auto instance";
            yesno = true;
            break;
        case CONFIG::control::enableHighlight:
            contstr += "Enable bank highlight";
            yesno = true;
            break;
        case CONFIG::control::historyLock:
        {
            string group[] = {"Instrument", "Patchset", "Scale", "State", "Vector", "Mlearn"};
            contstr = "History lock " + group[kititem];
            yesno = true;
            break;
        }
        case CONFIG::control::exposeStatus:
            showValue = false;
            contstr += "Show CLI context ";
            if (addValue)
            {
                switch (value_int)
                {
                    case 0:
                        contstr += "off";
                        break;
                    case 1:
                        contstr += "on";
                        break;
                    case 2:
                        contstr += "prompt";
                        break;
                    default:
                        contstr += "unrecognised";
                        break;
                }
            }
            break;

        case CONFIG::control::readAudio:
            contstr += "Audio Destination ";
            if (addValue)
            {
                switch (value_int)
                {
                    case 1:
                        contstr += "JACK";
                        break;
                    case 2:
                        contstr += "ALSA";
                        break;
                    default:
                        contstr += "None";
                }
                showValue = false;
            }
            break;

        case CONFIG::control::readMIDI:
            contstr += "MIDI Source ";
            if (addValue)
            {
                switch (value_int)
                {
                    case 1:
                        contstr += "JACK";
                        break;
                    case 2:
                        contstr += "ALSA";
                        break;
                    default:
                        contstr += "None";
                }
                showValue = false;
            }
            break;
        case CONFIG::control::jackMidiSource:
            contstr += "JACK MIDI source: ";
            if (addValue)
                contstr += textMsgBuffer.fetch(value_int);
            showValue = false;
            break;
        case CONFIG::control::jackPreferredMidi:
            contstr += "Start with JACK MIDI";
            yesno = true;
            break;
        case CONFIG::control::jackServer:
            contstr += "JACK server: ";
            if (addValue)
                contstr += textMsgBuffer.fetch(value_int);
            showValue = false;
            break;
        case CONFIG::control::jackPreferredAudio:
            contstr += "Start with JACK audio";
            yesno = true;
            break;
        case CONFIG::control::jackAutoConnectAudio:
            contstr += "Auto-connect to JACK server";
            yesno = true;
            break;

        case CONFIG::control::alsaMidiSource:
            contstr += "ALSA MIDI source: ";
            if (addValue)
                contstr += textMsgBuffer.fetch(value_int);
            showValue = false;
            break;
        case CONFIG::control::alsaPreferredMidi:
            contstr += "Start with ALSA MIDI";
            yesno = true;
            break;
        case CONFIG::control::alsaMidiType:
            contstr += "ALSA MIDI connection type ";
            switch (value_int)
            {
                case 0:
                    contstr += "Fixed";
                    break;
                case 1:
                    contstr += "Search";
                    break;
                default:
                    contstr += "External";
                    break;
            }
            showValue = false;
            break;
        case CONFIG::control::alsaAudioDevice:
            contstr += "ALSA audio device: ";
            if (addValue)
                contstr += textMsgBuffer.fetch(value_int);
            showValue = false;
            break;
        case CONFIG::control::alsaPreferredAudio:
            contstr += "Start with ALSA audio";
            yesno = true;
            break;
        case CONFIG::control::alsaSampleRate:
            contstr += "ALSA sample rate: ";
            if (addValue)
            {
                switch (value_int)
                { // this is a hack :(
                    case 0:
                    case 192000:
                        contstr += "0 (192000)";
                        break;
                    case 1:
                    case 96000:
                        contstr += "1 (96000)";
                        break;
                    case 2:
                    case 48000:
                        contstr += "2 (48000)";
                        break;
                    case 3:
                    case 44100:
                        contstr += "3 (44100)";
                        break;
                }
            }
            showValue = false;
            break;

        case CONFIG::control::addPresetRootDir:
            contstr += "Preset root add";
            if (addValue)
                contstr += textMsgBuffer.fetch(value_int);
            showValue = false;
            break;
        case CONFIG::control::removePresetRootDir:
            contstr += "Preset root unlinked ";
            if (addValue)
                contstr += textMsgBuffer.fetch(value_int);
            showValue = false;
            break;
        case CONFIG::control::currentPresetRoot:
            contstr += "Current preset root ";
            if (addValue)
                contstr += textMsgBuffer.fetch(value_int);
            showValue = false;
            break;

        case CONFIG::control::bankRootCC:
            contstr += "Bank root CC ";
            if (addValue)
            {
                if (parameter != UNUSED)
                    contstr += textMsgBuffer.fetch(parameter);
                else
                {
                    switch (value_int)
                    {
                        case 0:
                            contstr += "MSB";
                            break;
                        case 32:
                            contstr += "LSB";
                            break;
                        default:
                            contstr += "OFF";
                    }
                }
            }
            showValue = false;
            break;

        case CONFIG::control::bankCC:
            contstr += "Bank CC ";
            if (addValue)
            {
                if (parameter != UNUSED)
                    contstr += textMsgBuffer.fetch(parameter);
                else
                {
                    switch (value_int)
                    {
                        case 0:
                            contstr += "MSB";
                            break;
                        case 32:
                            contstr += "LSB";
                            break;
                        default:
                            contstr += "OFF";
                    }
                }
            }
            showValue = false;
            break;
        case CONFIG::control::enableProgramChange:
            contstr += "Enable program change";
            yesno = true;
            break;
        case CONFIG::control::extendedProgramChangeCC:
            if (addValue)
            {
            if (parameter != UNUSED)
            {
                string test = textMsgBuffer.fetch(parameter);
                contstr += ("Extended program change CC in use by "  + test);
            }
            else if (value == 128)
            {
                contstr += ("Extended program change disabled");
            }
            else
                contstr += "CC for extended program change ";
            contstr += to_string(value_int);
            }
            showValue = false;
            break;
        case CONFIG::control::ignoreResetAllCCs:
            contstr += "Ignore 'reset all CCs'";
            yesno = true;
            break;
        case CONFIG::control::logIncomingCCs:
            contstr += "Log incoming CCs";
            yesno = true;
            break;
        case CONFIG::control::showLearnEditor:
            contstr += "Auto-open GUI MIDI-learn editor";
            yesno = true;
            break;

        case CONFIG::control::enableOmni:
            contstr += "Enable Omni Mode Change";
            yesno = true;
            break;

        case CONFIG::control::enableNRPNs:
            contstr += "Enable NRPN";
            yesno = true;
            break;

        case CONFIG::control::saveCurrentConfig:
        {
            string name = textMsgBuffer.fetch(value_int);
            if (write)
                contstr += ("save" + name);
            else
            {
                contstr += "Condition - ";
                 if (synth.getRuntime().configChanged)
                     contstr += "DIRTY";
                 else
                     contstr += "CLEAN";
            }
            showValue = false;
            break;
        }
        default:
            contstr = "Unrecognised Config";
            break;
    }

    return ("Config " + contstr);
}


string DataText::resolveBank(CommandBlock& cmd, bool)
{
    int value_int = lrint(cmd.data.value);
    int control   = cmd.data.control;
    int kititem   = cmd.data.kit;
    int engine    = cmd.data.engine;
    int insert    = cmd.data.insert;
    string name{textMsgBuffer.fetch(value_int)};
    string contstr;
    showValue = false;
    switch(control)
    {
        case BANK::control::renameInstrument:
        {
            contstr = "Instrument Rename" + name;
            break;
        }
        case BANK::control::saveInstrument:
        {
            contstr = "Instrument Save to slot " + name;
            break;
        }
        case BANK::control::deleteInstrument:
            contstr = "Instrument delete" + name;
            break;
        case BANK::control::selectFirstInstrumentToSwap:
            contstr = "Set Instrument ID " + to_string(insert + 1) + "  Bank ID " + to_string(kititem) + "  Root ID " + to_string(engine) + " for swap";
            break;
        case BANK::control::selectSecondInstrumentAndSwap:
            if (name == "")
                name = "ped with Instrument ID " + to_string(insert + 1) + "  Bank ID " + to_string(kititem) + "  Root ID " + to_string(engine);
            contstr = "Swap" + name;
            break;

        case BANK::control::selectBank:
            contstr = name;
            break;
        case BANK::control::renameBank:
            contstr = name;
            break;
        case BANK::control::createBank:
            contstr = name;
            break;
        case BANK::control::findBankSize:
            if (value_int == UNUSED)
                contstr = " Bank " + to_string(kititem) + " does not exist.";
            else if (value_int == 0)
                contstr = " Bank " + to_string(kititem) + " is empty.";
            else
                contstr = " Bank " + to_string(kititem) + " contains " + to_string(value_int) + " instruments";
            showValue = false;
            break;

        case BANK::control::selectFirstBankToSwap:
            contstr = "Set Bank ID " + to_string(kititem) + "  Root ID " + to_string(engine) + " for swap";
            break;
        case BANK::control::selectSecondBankAndSwap:
            if (name == "")
                name = "ped with Bank ID " + to_string(kititem) + "  Root ID " + to_string(engine);
            contstr = "Swap" + name;
            break;
        case BANK::control::selectRoot:
            contstr = name;
            break;

        case BANK::control::changeRootId:
            contstr = "Root ID changed " + to_string(engine) + " > " + to_string(value_int);
            break;

        case BANK::control::addNamedRoot:
            if (value_int == UNUSED)
                contstr = name;
            else if (kititem != UNUSED)
                contstr = "Created Bank Root " + name;
            else
                contstr = "Link Bank Root " + name;
            break;

        case BANK::control::deselectRoot:
            if (value_int == UNUSED)
                contstr = "Bank Root " + to_string(kititem) + " does not exist";
            else
                contstr = "Unlinked Bank Root " + to_string(kititem);
            showValue = false;
            break;

        default:
            contstr = "Unrecognised Bank";
            break;
    }
    return ("Bank " + contstr);
}

string DataText::resolveMain(CommandBlock& cmd, bool addValue)
{
    float value   = cmd.data.value;
    int value_int = lrint(value);

    uchar control = cmd.data.control;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;

    string name;
    string contstr;
    if (cmd.data.part == TOPLEVEL::section::midiIn)
    {
        switch (control)
        {
            case MIDI::control::noteOn:
                showValue = false;
                break;
            case MIDI::control::noteOff:
                showValue = false;
                break;
            case MIDI::control::controller:
                contstr = "CC " + to_string(int(engine)) + " ";
                break;
            case MIDI::control::bankChange:
                showValue = false;
                contstr = textMsgBuffer.fetch(value_int);
                break;
        }
        return contstr;
    }

    switch (control)
    {
        case MAIN::control::volume:
            contstr = "Volume";
            break;

        case MAIN::control::partNumber:
            showValue = false;
            contstr = "Part Number " + to_string(value_int + 1);
            break;
        case MAIN::control::availableParts:
            contstr = "Available Parts";
            break;

        case MAIN::control::panLawType:
            contstr = "Panning Law ";
            if (addValue)
            {
                switch (value_int)
                {
                    case MAIN::panningType::cut:
                        contstr += "cut";
                        break;
                    case MAIN::panningType::normal:
                        contstr += "default";
                        break;
                    case MAIN::panningType::boost:
                        contstr += "boost";
                        break;
                    default:
                        contstr += "unrecognised";
                }
            }
            showValue = false;
            break;
        case MAIN::control::detune:
            contstr = "Detune";
            break;
        case MAIN::control::keyShift:
            contstr = "Key Shift";
            break;
        case MAIN::control::bpmFallback:
            contstr = "Fallback BPM";
            break;
        case MAIN::control::mono:
            contstr = "Master Mono/Stereo ";
            showValue = false;
            if (addValue)
            {
                if (value_int)
                    contstr += "Mono";
                else
                    contstr += "Stereo";
            }
            break;

        case MAIN::control::reseed:
            showValue = false;
            contstr += "reseeded to " + to_string(value_int);
            break;

        case MAIN::control::soloType:
            showValue = false;
            contstr = "Chan 'solo' Switch ";
            if (addValue)
            {
                switch (value_int)
                {
                    case MIDI::SoloType::Disabled:
                        contstr += "Off";
                        break;
                    case MIDI::SoloType::Row:
                        contstr += "Row";
                        break;
                    case MIDI::SoloType::Column:
                        contstr += "Column";
                        break;
                    case MIDI::SoloType::Loop:
                        contstr += "Loop";
                        break;
                    case MIDI::SoloType::TwoWay:
                        contstr += "Twoway";
                        break;
                    case MIDI::SoloType::Channel:
                        contstr += "Channel";
                        break;
                }
            }
            break;
        case MAIN::control::soloCC:
            showValue = false;
            contstr = "Chan 'solo' Switch CC ";
            if (addValue)
            {
                if (value_int > 127)
                    contstr += "undefined - set type first";
                else
                    contstr += to_string(value_int);
            }
            break;

        case MAIN::control::exportBank:
            showValue = false;
            contstr = "Bank Export" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::importBank:
            showValue = false;
            contstr = "Bank Import" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::deleteBank:
            showValue = false;
            contstr = "Bank delete" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::loadInstrumentFromBank:
            showValue = false;
            contstr = "Part " + to_string (int(kititem + 1)) + " load" + textMsgBuffer.fetch(value_int);
            break;
        case MAIN::control::loadInstrumentByName:
            showValue = false;
            contstr = "Part " + to_string (int(kititem + 1)) + " load" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::saveNamedInstrument:
            showValue = false;
            contstr = "Instrument Save" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::loadNamedPatchset:
            showValue = false;
            contstr = "Patchset Load" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::saveNamedPatchset:
            showValue = false;
            contstr = "Patchset Save" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::loadNamedVector:
            showValue = false;
            name = textMsgBuffer.fetch(value_int);
            contstr = "Vector Load" + name;
            break;

        case MAIN::control::saveNamedVector:
            showValue = false;
            name = textMsgBuffer.fetch(value_int);
            contstr = "Vector Save" + name;
            break;

        case MAIN::control::loadNamedScale:
            showValue = false;
            name = textMsgBuffer.fetch(value_int);
            contstr = "Scale Load" + name;
            break;

        case MAIN::control::saveNamedScale:
            showValue = false;
            name = textMsgBuffer.fetch(value_int);
            contstr = "Scale Save" + name;
            break;

        case MAIN::control::loadNamedState:
            showValue = false;
            name = textMsgBuffer.fetch(value_int);
            contstr = "State Load" + name;
            break;

        case MAIN::control::saveNamedState:
            showValue = false;
            contstr = "State Save" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::loadFileFromList:
            showValue = false;
            contstr = "Load Recent" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::defaultPart:
            showValue = false;
            contstr = "Part " + to_string(value_int + 1) + " completely cleared";
            break;

        case MAIN::control::defaultInstrument:
            showValue = false;
            contstr = "Part " + to_string(value_int + 1) + " instrument cleared";
            break;

        case MAIN::control::exportPadSynthSamples:
            showValue = false;
            contstr = "PadSynth Samples Save" + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::masterReset:
            showValue = false;
            contstr = "Reset All";
            break;
        case MAIN::control::masterResetAndMlearn:
            showValue = false;
            contstr = "Reset All including MIDI-learn";
            break;
        case TOPLEVEL::control::dataExchange:
            showValue = false;
            contstr = "Engine initialised";
            break;

        case MAIN::control::openManual:
            showValue = false;
            contstr = "Open manual in reader " + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::startInstance:
            showValue = false;
            contstr = "Start new instance " + to_string(value_int);
            break;
        case MAIN::control::stopInstance:
            showValue = false;
            contstr = "Close instance - " + textMsgBuffer.fetch(value_int);
            break;

        case MAIN::control::stopSound:
            showValue = false;
            contstr = "Sound Stopped";
            break;

        case MAIN::control::readPartPeak:
            showValue = false;
            if (engine == 1)
                contstr = "Part R";
            else
                contstr = "Part L";
            contstr += to_string(int(kititem));
            if (value < 0.0f)
                contstr += " silent ";
            contstr += (" peak level " + to_string(value));
            break;

        case MAIN::control::readMainLRpeak:
            showValue = false;
            if (kititem == 1)
                contstr = "Right";
            else
                contstr = "Left";
            contstr += (" peak level " + to_string(value));
            break;

        case MAIN::control::readMainLRrms:
            showValue = false;
            if (kititem == 1)
                contstr = "Right";
            else
                contstr = "Left";
            contstr += (" RMS level " + to_string(value));
            break;

        default:
            showValue = false;
            contstr = "Unrecognised Main";
            break;
    }

    return ("Main " + contstr);
}


string DataText::resolveAftertouch(bool type, int value, bool addValue)
{
    string contstr;
    if (type)
        contstr = "ChannelAT";
    else
        contstr = "KeyAT";
    if (!addValue)
        return contstr;

    if (value == PART::aftertouchType::off)
        contstr += " Off";
    else
    {
        if (value & PART::aftertouchType::filterCutoff)
        {
            contstr += "\n Filter Cutoff";
            if (value & PART::aftertouchType::filterCutoffDown)
                contstr += " Down";
        }
        if (value & PART::aftertouchType::filterQ)
        {
            contstr += "\n Peak";
            if (value & PART::aftertouchType::filterQdown)
                contstr += " Down";
        }
        if (value & PART::aftertouchType::pitchBend)
        {
            contstr += "\n Bend";
            if (value & PART::aftertouchType::pitchBendDown)
                contstr += " Down";
        }
        if (value & PART::aftertouchType::volume)
            contstr += "\n Volume";
        if (value & PART::aftertouchType::modulation)
            contstr += "\n Modulation";
    }
    return contstr;
}


string DataText::resolvePart(CommandBlock& cmd, bool addValue)
{
    float value     = cmd.data.value;
    uchar control   = cmd.data.control;
    uchar npart     = cmd.data.part;
    uchar kititem   = cmd.data.kit;
    uchar engine    = cmd.data.engine;
    uchar effNum    = engine;                  // note
    uchar insert    = cmd.data.insert;
    uchar parameter = cmd.data.parameter;

    bool kitType = (insert == TOPLEVEL::insert::kitGroup);
    int value_int = lrint(value);
    bool value_bool = _SYS_::F2B(value);

    if (control == UNUSED)
        return "Number of parts";

    string kitnum;
    if (kitType)
        kitnum = " Kit " + to_string(kititem + 1) + " ";
    else
        kitnum = " ";

    string group = "";

    if (kititem != UNUSED)
    {
        switch (engine)
        {
            case PART::engine::addSynth:
                group = "AddSynth ";
                break;
            case PART::engine::subSynth:
                group = "SubSynth ";
                break;
            case PART::engine::padSynth:
                group = "PadSynth ";
                break;
        }
    }

    string contstr = "";
    switch (control)
    {
        case PART::control::enable:
            contstr += " Enable";
            yesno = true;
            break;
        case PART::control::enableAdd:
            contstr += "AddSynth Enable";
            yesno = true;
            break;
        case PART::control::enableSub:
            contstr += "SubSynth Enable";
            yesno = true;
            break;
        case PART::control::enablePad:
            contstr += "PadSynth Enable";
            yesno = true;
            break;
        case PART::control::enableKitLine:
            contstr += " Enable";
            yesno = true;
            break;

        case PART::control::volume:
            contstr = "Volume";
            break;
        case PART::control::velocitySense:
            contstr = "Velocity Sense";
            break;
        case PART::control::panning:
            contstr = "Panning";
            break;
        case PART::control::velocityOffset:
            contstr = "Velocity Offset";
            break;
        case PART::control::midiChannel:
            showValue = false;
            contstr = "Midi CH ";
            if (addValue)
            {
                contstr += to_string(value_int + 1);
                if (value_int >= NUM_MIDI_CHANNELS * 2)
                    contstr += " Midi ignored";
                else if (value_int >= NUM_MIDI_CHANNELS)
                    contstr = contstr + " Note off only from CH " + to_string(value_int + 1 - NUM_MIDI_CHANNELS);
            }
            break;
        case PART::control::omni:
            contstr = "Omni Mode";
            yesno = true;
            break;
        case PART::control::keyMode:
            showValue = false;
            contstr = "Mode ";
            if (addValue)
            {
                if (value_int == 0)
                    contstr += "Poly";
                else if (value_int == 1)
                    contstr += "Mono";
                else if (value_int >= 2)
                    contstr += "Legato";
            }
            break;
        case PART::control::channelATset:
            showValue = false;
            contstr = resolveAftertouch(true, value_int, addValue);
            if (parameter != UNUSED)
                contstr = contstr + "\n" + resolveAftertouch(false, parameter, addValue);
            break;
        case PART::control::keyATset:
            showValue = false;
            contstr = resolveAftertouch(false, value_int, addValue);
            if (parameter != UNUSED)
                contstr = contstr + "\n" + resolveAftertouch(true, parameter, addValue);
            break;
        case PART::control::portamento:
            contstr = "Portamento Enable";
            yesno = true;
            break;

        case PART::control::kitItemMute:
            if (kitType)
            {
                contstr = "Mute";
                yesno = true;
            }
            break;

        case PART::control::minNote:
            contstr = "Min Note";
            break;
        case PART::control::maxNote:
            contstr = "Max Note";
            break;
        case PART::control::minToLastKey: // always return actual value
            contstr = "Min To Last";
            break;
        case PART::control::maxToLastKey: // always return actual value
            contstr = "Max To Last";
            break;
        case PART::control::resetMinMaxKey:
            contstr = "Full Key Range";
            showValue = false;
            break;

        case PART::control::kitEffectNum:
            if (value_int == 0)
                contstr = "Effect Off";
            else
                contstr = "Effect Number " + to_string(value_int);
            showValue = false;
            break;

        case PART::control::maxNotes:
            contstr = "Key Limit";
            break;
        case PART::control::keyShift:
            contstr = "Key Shift";
            break;

        case PART::control::partToSystemEffect1:
            contstr = "Effect Send 1";
            break;
        case PART::control::partToSystemEffect2:
            contstr = "Effect Send 2";
            break;
        case PART::control::partToSystemEffect3:
            contstr = "Effect Send 3";
            break;
        case PART::control::partToSystemEffect4:
            contstr = "Effect Send 4";
            break;

        case PART::control::humanise:
            contstr = "Humanise Pitch";
            break;

        case PART::control::humanvelocity:
            contstr = "Humanise Velocity";
            break;

        case PART::control::drumMode:
            contstr = "Drum Mode";
            yesno = true;
            break;
        case PART::control::kitMode:
            contstr = "Kit Mode ";
            showValue = false;
            if (addValue)
            {
                switch(value_int)
                {
                    case 0:
                        contstr += "off";
                        break;
                    case 1:
                        contstr += "multi";
                        break;
                    case 2:
                        contstr += "single";
                        break;
                    case 3:
                        contstr += "crossfade";
                        break;
                }
            }
            break;

        case PART::control::effectNumber:
            contstr = "Effect Number " + to_string(value_int);
            showValue = false;
            break;
        case PART::control::effectType:
            contstr = "Effect " + to_string(effNum + 1) + " Type";
            break;
        case PART::control::effectDestination:
            contstr = "Effect " + to_string(effNum + 1) + " Destination";
            break;

        case PART::control::audioDestination:
            contstr = "Audio destination ";
            showValue = false;
            if (addValue)
            {
                switch(value_int)
                {
                    case 3:
                        contstr += "both";
                        break;
                    case 2:
                        contstr += "part";
                        break;
                    case 1:
                        contstr += "main";
                        break;
                    default:
                        contstr += "main";
                        break;
                }
            }
            break;

        case PART::control::instrumentCopyright:
            showValue = false;
            contstr = "Copyright: " + textMsgBuffer.fetch(value_int);
            break;
        case PART::control::instrumentComments:
            showValue = false;
            contstr = "Comment: " + textMsgBuffer.fetch(value_int);
            break;
        case PART::control::instrumentName:
            showValue = false;
            contstr = "Name is: " + textMsgBuffer.fetch(value_int);
            break;
        case PART::control::instrumentType:
            showValue = false;
            contstr = "Type is: " + type_list[value_int];
            break;
        case PART::control::defaultInstrumentCopyright:
            showValue = false;
            contstr = "Copyright ";
            if (parameter == 0)
                contstr += "load:\n";
            else
                contstr += "save:\n";
            contstr += textMsgBuffer.fetch(value_int);
            break;
        case PART::control::resetAllControllers:
            showValue = false;
            contstr = "Cleared controllers";
            break;

        case TOPLEVEL::control::partBusy:
            showValue = false;
            if (value_bool)
                contstr = "is busy";
            else
                contstr = "is free";
            break;

    }
    if (!contstr.empty())
        return ("Part " + to_string(npart + 1) + kitnum + group + contstr);

    switch (control)
    {
            case PART::control::volumeRange:
            contstr = "Vol Range"; // not the *actual* volume
            break;
        case PART::control::volumeEnable:
            contstr = "Vol Enable";
            yesno = true;
            break;
        case PART::control::panningWidth:
            contstr = "Pan Width";
            break;
        case PART::control::modWheelDepth:
            contstr = "Mod Wheel Range";
            break;
        case PART::control::exponentialModWheel:
            contstr = "Exponent Mod Wheel";
            yesno = true;
            break;
        case PART::control::bandwidthDepth:
            contstr = "Bandwidth range";
            break;
        case PART::control::exponentialBandwidth:
            contstr = "Exponent Bandwidth";
            yesno = true;
            break;
        case PART::control::expressionEnable:
            contstr = "Expression Enable";
            yesno = true;
            break;
        case PART::control::FMamplitudeEnable:
            contstr = "FM Amp Enable";
            yesno = true;
            break;
        case PART::control::sustainPedalEnable:
            contstr = "Sustain Ped Enable";
            yesno = true;
            break;
        case PART::control::pitchWheelRange:
            contstr = "Pitch Wheel Range";
            break;
        case PART::control::filterQdepth:
            contstr = "Filter Q Range";
            break;
        case PART::control::filterCutoffDepth:
            contstr = "Filter Cutoff Range";
            break;
        case PART::control::breathControlEnable:
            yesno = true;
            contstr = "Breath Control";
            yesno = true;
            break;

        case PART::control::resonanceCenterFrequencyDepth:
            contstr = "Res Cent Freq Range";
            break;
        case PART::control::resonanceBandwidthDepth:
            contstr = "Res Band Range";
            break;

        case PART::control::portamentoTime:
            contstr = "Time";
            break;
        case PART::control::portamentoTimeStretch:
            contstr = "Time Stretch";
            break;
        case PART::control::portamentoThreshold:
            contstr = "Threshold Gate";
            break;
        case PART::control::portamentoThresholdType:
            contstr = "Threshold Gate Type ";
            showValue = false;
            if (value_int == 0)
                contstr += ">= start";
            else
                contstr += "< end";
            break;
        case PART::control::enableProportionalPortamento:
            contstr = "Prop Enable";
            yesno = true;
            break;
        case PART::control::proportionalPortamentoRate:
            contstr = "Prop Rate";
            break;
        case PART::control::proportionalPortamentoDepth:
            contstr = "Prop depth";
            break;
        case PART::control::receivePortamento:
            contstr = "Receive";
            yesno = true;
            break;
    }
    if (!contstr.empty())
        return ("Part " + to_string(npart + 1) + kitnum + "Controller " + contstr);

    string name = "MIDI ";
    switch (control)
    {
            case PART::control::midiModWheel:
            contstr = "Modulation";
            break;
        case PART::control::midiBreath:
            ; // not yet
            break;
        case PART::control::midiExpression:
            contstr = "Expression";
            break;
        case PART::control::midiSustain:
            ; // not yet
            break;
        case PART::control::midiPortamento:
            ; // not yet
            break;
        case PART::control::midiFilterQ:
            contstr = "Filter Q";
            break;
        case PART::control::midiFilterCutoff:
            contstr = "Filter Cutoff";
            break;
        case PART::control::midiBandwidth:
            contstr = "Bandwidth";
            break;
        case PART::control::midiFMamp:
            contstr = "FM Amp";
            break;
        case PART::control::midiResonanceCenter:
            contstr = "Resonance Cent";
            break;
        case PART::control::midiResonanceBandwidth:
            contstr = "Resonance Band";
            break;

        default:
            showValue = false;
            name = "";
            contstr = "Unrecognised Part";
            break;
    }
    return ("Part " + to_string(npart + 1) + kitnum + name + contstr);
}


string DataText::resolveAdd(CommandBlock& cmd, bool addValue)
{
    float value   = cmd.data.value;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;

    string contstr;

    switch (control)
    {
        case ADDSYNTH::control::volume:
            contstr = "Volume";
            break;
        case ADDSYNTH::control::velocitySense:
            contstr = "Velocity Sense";
            break;

        case ADDSYNTH::control::panning:
            contstr = "Panning";
            break;
        case ADDSYNTH::control::enableRandomPan:
            contstr = "Random Pan";
            yesno = true;
            break;
        case ADDSYNTH::control::randomWidth:
            contstr = "Random Width";
            break;

        case ADDSYNTH::control::detuneFrequency:
            contstr = "Detune";
            break;

        case ADDSYNTH::control::octave:
            contstr = "Octave";
            break;
        case ADDSYNTH::control::detuneType:
            contstr = "Detune Type ";
            showValue = false;
            if (addValue)
                contstr += detuneType [int(value)];
            break;
        case ADDSYNTH::control::coarseDetune:
            contstr = "Coarse Det";
            break;
        case ADDSYNTH::control::relativeBandwidth:
            contstr = "Relative Bandwidth";
            break;

        case ADDSYNTH::control::stereo:
            contstr = "Stereo";
            yesno = true;
            break;
        case ADDSYNTH::control::randomGroup:
            contstr = "Rnd Grp";
            yesno = true;
            break;

        case ADDSYNTH::control::dePop:
            contstr = "De Pop";
            break;
        case ADDSYNTH::control::punchStrength:
            contstr = "Punch Strength";
            break;
        case ADDSYNTH::control::punchDuration:
            contstr = "Punch Time";
            break;
        case ADDSYNTH::control::punchStretch:
            contstr = "Punch Stretch";
            break;
        case ADDSYNTH::control::punchVelocity:
            contstr = "Punch Velocity";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised AddSynth";
            break;
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " AddSynth " + contstr);
}


string DataText::resolveAddVoice(CommandBlock& cmd, bool addValue)
{
    float value   = cmd.data.value;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;

    int value_int = lrint(value);
    int nvoice;
    if (engine >= PART::engine::addMod1)
        nvoice = engine - PART::engine::addMod1;
    else
        nvoice = engine - PART::engine::addVoice1;

    string contstr = "";

    switch (control)
    {
        case ADDVOICE::control::volume:
            contstr = "Volume";
            break;
        case ADDVOICE::control::velocitySense:
            contstr = "Velocity Sense";
            break;
        case ADDVOICE::control::panning:
            contstr = "Panning";
            break;
        case ADDVOICE::control::enableRandomPan:
            contstr = "Random Pan";
            yesno = true;
            break;
        case ADDVOICE::control::randomWidth:
            contstr = "Random Width";
            break;

        case ADDVOICE::control::invertPhase:
            contstr = "Minus";
            yesno = true;
            break;
        case ADDVOICE::control::enableAmplitudeEnvelope:
            contstr = "Amp Enable Env";
            yesno = true;
            break;
        case ADDVOICE::control::enableAmplitudeLFO:
            contstr = "Amp Enable LFO";
            yesno = true;
            break;

        case ADDVOICE::control::modulatorType:
            contstr = "Modulator Type ";
            if (addValue)
            {
                showValue = false;
                contstr += addmodnameslist[value_int];
            }
            break;
        case ADDVOICE::control::externalModulator:
            if (addValue)
            {
                showValue = false;
                if (value_int < 0)
                    contstr = "Local";
                else
                    contstr = "Modulator Source Voice " + to_string(value_int + 1);
            }
            break;

        case ADDVOICE::control::externalOscillator:
            if (addValue)
            {
                showValue = false;
                if (value_int < 0)
                    contstr = "Local";
                else
                    contstr = "Source " + to_string(value_int + 1);
            }
            break;

        case ADDVOICE::control::detuneFrequency:
            contstr = "Detune";
            break;
        case ADDVOICE::control::equalTemperVariation:
            contstr = "Equal Temper";
            break;
        case ADDVOICE::control::baseFrequencyAs440Hz:
            contstr = "440Hz";
            yesno = true;
            break;
        case ADDVOICE::control::octave:
            contstr = "Octave";
            break;
        case ADDVOICE::control::detuneType:
            contstr = "Detune Type ";
            showValue = false;
            if (addValue)
                contstr += stringCaps(detuneType [int(value)], 1);
            break;
        case ADDVOICE::control::coarseDetune:
            contstr = "Coarse Detune";
            break;
        case ADDVOICE::control::pitchBendAdjustment:
            contstr = "Bend Adj";
            break;
        case ADDVOICE::control::pitchBendOffset:
            contstr = "Offset Hz";
            break;
        case ADDVOICE::control::enableFrequencyEnvelope:
            contstr = "Freq Enable Env";
            yesno = true;
            break;
        case ADDVOICE::control::enableFrequencyLFO:
            contstr = "Freq Enable LFO";
            yesno = true;
            break;

        case ADDVOICE::control::unisonFrequencySpread:
            contstr = "Unison Freq Spread";
            break;
        case ADDVOICE::control::unisonPhaseRandomise:
            contstr = "Unison Phase Rnd";
            break;
        case ADDVOICE::control::unisonStereoSpread:
            contstr = "Unison Stereo";
            break;
        case ADDVOICE::control::unisonVibratoDepth:
            contstr = "Unison Vibrato";
            break;
        case ADDVOICE::control::unisonVibratoSpeed:
            contstr = "Unison Vib Speed";
            break;
        case ADDVOICE::control::unisonSize:
            contstr = "Unison Size";
            break;
        case ADDVOICE::control::unisonPhaseInvert:
            showValue = false;
            contstr = "Unison Invert " + unisonPhase[value_int];
            break;
        case ADDVOICE::control::enableUnison:
            contstr = "Unison Enable";
            yesno = true;
            break;

        case ADDVOICE::control::bypassGlobalFilter:
            contstr = "Filter Bypass Global";
            yesno = true;
            break;
        case ADDVOICE::control::enableFilter:
            contstr = "Filter Enable";
            yesno = true;
            break;
        case ADDVOICE::control::enableFilterEnvelope:
            contstr = "Filter Enable Env";
            yesno = true;
            break;
        case ADDVOICE::control::enableFilterLFO:
            contstr = "Filter Enable LFO";
            yesno = true;
            break;

        case ADDVOICE::control::modulatorAmplitude:
            contstr = "Modulator Volume";
            break;
        case ADDVOICE::control::modulatorVelocitySense:
            contstr = "Modulator Vel Sense";
            break;
        case ADDVOICE::control::modulatorHFdamping:
            contstr = "Modulator HF Damping";
            break;

        case ADDVOICE::control::enableModulatorAmplitudeEnvelope:
            contstr = "Modulator Amp Enable Env";
            yesno = true;
            break;

        case ADDVOICE::control::modulatorDetuneFrequency:
            contstr = "Modulator Detune";
            break;
        case ADDVOICE::control::modulatorFrequencyAs440Hz:
            contstr = "Modulator 440Hz";
            yesno = true;
            break;
        case ADDVOICE::control::modulatorDetuneFromBaseOsc:
            contstr = "Modulator Follow voice";
            yesno = true;
            break;
        case ADDVOICE::control::modulatorOctave:
            contstr = "Modulator Octave";
            break;
        case ADDVOICE::control::modulatorDetuneType:
            contstr = "Modulator Detune Type ";
            showValue = false;
            if (addValue)
                contstr += detuneType [int(value)];
            break;
        case ADDVOICE::control::modulatorCoarseDetune:
            contstr = "Modulator Coarse Detune";
            break;
        case ADDVOICE::control::enableModulatorFrequencyEnvelope: // local, external
            contstr = "Modulator Freq Enable Env";
            yesno = true;
            break;

        case ADDVOICE::control::modulatorOscillatorPhase:
            contstr = "Modulator Osc Phase";
            break;
        case ADDVOICE::control::modulatorOscillatorSource:
            if (addValue)
            {
                showValue = false;
                if (value_int < 0)
                    contstr = "Modulator Internal";
                else
                    contstr = "Modulator Osc from " + to_string(value_int + 1);
            }
            break;

        case ADDVOICE::control::delay:
            contstr = "Delay";
            break;
        case ADDVOICE::control::enableVoice:
            contstr = "Enable";
            yesno = true;
            break;
        case ADDVOICE::control::enableResonance:
            contstr = "Resonance Enable";
            yesno = true;
            break;
        case ADDVOICE::control::voiceOscillatorPhase:
            contstr = "Osc Phase";
            break;
        case ADDVOICE::control::voiceOscillatorSource:
            if (addValue)
            {
                showValue = false;
                if (value_int < 0)
                    contstr = "Internal";
                else
                    contstr = "from " + to_string(value_int + 1);
            }
            break;
        case ADDVOICE::control::soundType:
            contstr = "Sound type";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised AddVoice";
            break;
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " Add Voice " + to_string(nvoice + 1) + " " + contstr);
}


string DataText::resolveSub(CommandBlock& cmd, bool addValue)
{
    float value   = cmd.data.value;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar insert  = cmd.data.insert;

    int value_int = int(value);

    if (insert == TOPLEVEL::insert::harmonicAmplitude || insert == TOPLEVEL::insert::harmonicBandwidth)
    {
        string Htype;
        if (insert == TOPLEVEL::insert::harmonicAmplitude)
            Htype = " Amplitude";
        else
            Htype = " Bandwidth";

        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " SubSynth Harmonic " + to_string(control + 1) + Htype);
    }

    string name = "";

    string contstr = "";
    switch (control)
    {
        case SUBSYNTH::control::volume:
            contstr = "Volume";
            break;
        case SUBSYNTH::control::velocitySense:
            contstr = "Velocity Sense";
            break;
        case SUBSYNTH::control::panning:
            contstr = "Panning";
            break;
        case SUBSYNTH::control::enableRandomPan:
            contstr = "Random Pan";
            yesno = true;
            break;
        case SUBSYNTH::control::randomWidth:
            contstr = "Random Width";
            break;

        case SUBSYNTH::control::bandwidth:
            contstr = "Bandwidth"; // it's the actual bandwidth control
            break;
        case SUBSYNTH::control::bandwidthScale:
            contstr = "Bandwidth Band Scale";
            break;
        case SUBSYNTH::control::enableBandwidthEnvelope:
            contstr = "Bandwidth Env Enab";
            yesno = true;
            break;

        case SUBSYNTH::control::detuneFrequency:
            contstr = "Detune";
            break;
        case SUBSYNTH::control::equalTemperVariation:
            contstr = "Equal Temper";
            break;
        case SUBSYNTH::control::baseFrequencyAs440Hz:
            contstr = "440Hz";
            yesno = true;
            break;
        case SUBSYNTH::control::octave:
            contstr = "Octave";
            break;
        case SUBSYNTH::control::detuneType:
            contstr = "Detune Type ";
            showValue = false;
            if (addValue)
                contstr += detuneType [value_int];
            break;
        case SUBSYNTH::control::coarseDetune:
            contstr = "Coarse Detune";
            break;
        case SUBSYNTH::control::pitchBendAdjustment:
            contstr = "Bend Adj";
            break;
        case SUBSYNTH::control::pitchBendOffset:
            contstr = "Offset Hz";
            break;
        case SUBSYNTH::control::enableFrequencyEnvelope:
            contstr = "Frequency Env Enab";
            yesno = true;
            break;

        case SUBSYNTH::control::overtoneParameter1:
            contstr = "Overtones Par 1";
            break;
        case SUBSYNTH::control::overtoneParameter2:
            contstr = "Overtones Par 2";
            break;
        case SUBSYNTH::control::overtoneForceHarmonics:
            contstr = "Overtones Force H";
            break;
        case SUBSYNTH::control::overtonePosition:
            contstr = "Overtones Position " + subPadPosition[value_int];
            showValue = false;
            break;

        case SUBSYNTH::control::enableFilter:
            contstr = "Filter Enable";
            yesno = true;
            break;

        case SUBSYNTH::control::filterStages:
            contstr = "Filt Stages";
            break;
        case SUBSYNTH::control::magType:
            contstr = "Mag Type " + subMagType [value_int];
            showValue = false;
            break;
        case SUBSYNTH::control::startPosition:
            contstr = "Start ";
            showValue = false;
            switch (value_int)
            {
                case 0:
                    contstr += "Zero";
                    break;
                case 1:
                    contstr += "Random";
                    break;
                case 2:
                    contstr += "Maximum";
                    break;
            }
            break;

        case SUBSYNTH::control::clearHarmonics:
            contstr = "Clear Harmonics";
            showValue = false;
            break;

        case SUBSYNTH::control::stereo:
            contstr = "Stereo";
            yesno = true;
            break;

        default:
            showValue = false;
            contstr = "Unrecognised SubSynth";
            break;
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " SubSynth " + contstr);
}


string DataText::resolvePad(SynthEngine& synth, CommandBlock& cmd, bool addValue)
{
    float value   = cmd.data.value;
    int value_int = int(value);
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    bool write = (type & TOPLEVEL::type::Write) > 0;

    string contstr;

    switch (control)
    {
        case PADSYNTH::control::volume:
            contstr = "Volume";
            break;
        case PADSYNTH::control::velocitySense:
            contstr = "Velocity Sense";
            break;
        case PADSYNTH::control::panning:
            contstr = "Panning";
            break;
        case PADSYNTH::control::enableRandomPan:
            contstr = "Random Pan";
            yesno = true;
            break;
        case PADSYNTH::control::randomWidth:
            contstr = "Random Width";
            break;

        case PADSYNTH::control::detuneFrequency:
            contstr = "Detune";
            break;
        case PADSYNTH::control::equalTemperVariation:
            contstr = "Equal Temper";
            break;
        case PADSYNTH::control::baseFrequencyAs440Hz:
            contstr = "440Hz";
            yesno = true;
            break;
        case PADSYNTH::control::octave:
            contstr = "Octave";
            break;
        case PADSYNTH::control::detuneType:
            contstr = "Detune Type ";
            showValue = false;
            if (addValue)
                contstr += detuneType [int(value)];
            break;
        case PADSYNTH::control::coarseDetune:
            contstr = "Coarse Detune";
            break;

        case PADSYNTH::control::pitchBendAdjustment:
            contstr = "Bend Adjust";
            break;
        case PADSYNTH::control::pitchBendOffset:
            contstr = "Offset Hz";
            break;
        case PADSYNTH::control::stereo:
            contstr = "Stereo";
            yesno = true;
            break;
        case PADSYNTH::control::dePop:
            contstr = "De Pop";
            break;
        case PADSYNTH::control::punchStrength:
            contstr = "Punch Strength";
            break;
        case PADSYNTH::control::punchDuration:
            contstr = "Punch Time";
            break;
        case PADSYNTH::control::punchStretch:
            contstr = "Punch Stretch";
            break;
        case PADSYNTH::control::punchVelocity:
            contstr = "Punch Velocity";
            break;

        case PADSYNTH::control::applyChanges:
            showValue = false;
            contstr = "Changes Applied ";
            if (addValue)
            {
                if (value_int != 0)
                    contstr += "Yes";
                else
                    contstr += "No";
            }
            break;
    }
    if (!contstr.empty())
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " PadSynth " + contstr);

    switch (control)
    {
        case PADSYNTH::control::overtoneParameter1:
            contstr = "Overtones Par 1";
            break;
        case PADSYNTH::control::overtoneParameter2:
            contstr = "Overtones Par 2";
            break;
        case PADSYNTH::control::overtoneForceHarmonics:
            contstr = "Overtones Force H";
            break;
        case PADSYNTH::control::overtonePosition:
            contstr = "Position " + subPadPosition[value_int];
            showValue = false;
            break;

        case PADSYNTH::control::bandwidth:
            contstr = "Bandwidth";
            break;
        case PADSYNTH::control::bandwidthScale:
            contstr = "Bandwidth Scale";
            break;
        case PADSYNTH::control::spectrumMode:
            contstr = "Spectrum Mode";
            break;
        case PADSYNTH::control::xFadeUpdate:
            contstr = "XFade Update";
            break;
        case PADSYNTH::control::rebuildTrigger:
            contstr = "BuildTrigger";
            break;
        case PADSYNTH::control::randWalkDetune:
            contstr = "RWDetune";
            break;
        case PADSYNTH::control::randWalkBandwidth:
            contstr = "RWBandwidth";
            break;
        case PADSYNTH::control::randWalkFilterFreq:
            contstr = "RWFilterFreq";
            break;
        case PADSYNTH::control::randWalkProfileWidth:
            contstr = "RWWidthProfile";
            break;
        case PADSYNTH::control::randWalkProfileStretch:
            contstr = "RWStretchProfile";
            break;
    }
    string padApply{synth.getRuntime().usePadAutoApply()? " - rebuilding PAD"
                                                        : " - Need to Apply"};
    if (!contstr.empty())
    {
        if (write)
            contstr += padApply;
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " PadSynth " + contstr);
    }

    switch (control)
    {
        case PADSYNTH::control::baseWidth:
            contstr = "Width";
            break;
        case PADSYNTH::control::frequencyMultiplier:
            contstr = "Freq Mult";
            break;
        case PADSYNTH::control::modulatorStretch:
            contstr = "Str";
            break;
        case PADSYNTH::control::modulatorFrequency:
            contstr = "Freq";
            break;
        case PADSYNTH::control::size:
            contstr = "Size";
            break;
        case PADSYNTH::control::baseType:
            contstr = "Type";
            break;
        case PADSYNTH::control::harmonicSidebands:
            contstr = "Halves";
            break;
        case PADSYNTH::control::spectralWidth:
            contstr = "Amp Par 1";
            break;
        case PADSYNTH::control::spectralAmplitude:
            contstr = "Amp Par 2";
            break;
        case PADSYNTH::control::amplitudeMultiplier:
            contstr = "Amp Mult";
            break;
        case PADSYNTH::control::amplitudeMode:
            contstr = "Amp Mode";
            break;
        case PADSYNTH::control::autoscale: //
            contstr = "Autoscale";
            yesno = true;
            break;
    }
    if (!contstr.empty())
    {
        contstr = "Harmonic Base " + contstr;
        if (write)
            contstr += padApply;
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " PadSynth " + contstr);
    }

    switch (control)
    {
        case PADSYNTH::control::harmonicBase:
            contstr = "Base";
            break;
        case PADSYNTH::control::samplesPerOctave:
            contstr = "samp/Oct";
            break;
        case PADSYNTH::control::numberOfOctaves:
            contstr = "Num Oct";
            break;
        case PADSYNTH::control::sampleSize:
            break;

        default:
            showValue = false;
            contstr = "Unrecognised PadSynth";
            break;
    }
    if (contstr != "Unrecognised PadSynth")
        contstr = "Harmonic Samples " + contstr;
    if (write && contstr != "Unrecognised PadSynth")
        contstr += padApply;
    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " PadSynth " + contstr);
}


string DataText::resolveOscillator(SynthEngine& synth, CommandBlock& cmd, bool addValue)
{
    float value   = cmd.data.value;
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;
    uchar insert  = cmd.data.insert;
    bool write = (type & TOPLEVEL::type::Write) > 0;
    int value_int = int(value);

    string isPad;
    string eng_name;
    if (engine == PART::engine::padSynth)
    {
        eng_name = " PadSynth";
        if (write)
            isPad = synth.getRuntime().usePadAutoApply()? " - rebuilding PAD"
                                                        : " - Need to Apply";
    }
    else
    {
        int eng;
        if (engine >= PART::engine::addMod1)
            eng = engine - PART::engine::addMod1;
        else
            eng = engine - PART::engine::addVoice1;
        eng_name = " Add Voice " + to_string(eng + 1);
        if (engine >= PART::engine::addMod1)
            eng_name += " Modulator";
    }

    if (insert == TOPLEVEL::insert::harmonicAmplitude)
    {
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + eng_name + " Harmonic " + to_string((int)control + 1) + " Amplitude" + isPad);
    }
    else if (insert == TOPLEVEL::insert::harmonicPhase)
    {
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + eng_name + " Harmonic " + to_string((int)control + 1) + " Phase" + isPad);
    }

    string contstr;
    switch (control)
    {
        case OSCILLATOR::control::phaseRandomness:
            contstr = "Random";
            break;
        case OSCILLATOR::control::magType:
            contstr = "Mag Type";
            break;
        case OSCILLATOR::control::harmonicAmplitudeRandomness:
            contstr = "Harm Rnd";
            break;
        case OSCILLATOR::control::harmonicRandomnessType:
            contstr = "Harm Rnd Type";
            break;

        case OSCILLATOR::control::clearHarmonics:
            contstr = "Clear Harmonics";
            break;
        case OSCILLATOR::control::convertToSine:
            contstr = "Conv To Sine";
            break;
    }
    if (!contstr.empty())
    {
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + eng_name + " Oscillator " + contstr + isPad);
    }

    switch(control)
    {
        case OSCILLATOR::control::baseFunctionParameter:
            contstr = "Par";
            break;
        case OSCILLATOR::control::baseFunctionType:
            contstr = "Type ";
            showValue = false;
            if (addValue)
                contstr += stringCaps(waveformlist[int(value) * 2], 1);
            break;
        case OSCILLATOR::control::baseModulationParameter1:
            contstr = "Mod Par 1";
            break;
        case OSCILLATOR::control::baseModulationParameter2:
            contstr = "Mod Par 2";
            break;
        case OSCILLATOR::control::baseModulationParameter3:
            contstr = "Mod Par 3";
            break;
        case OSCILLATOR::control::baseModulationType:
            contstr = "Mod Type";
            break;

        case OSCILLATOR::control::autoClear: // this is local to the GUI
            break;
    }
    if (!contstr.empty())
    {
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + eng_name + " Base Func " + contstr + isPad);
    }

    switch(control)
    {
        case OSCILLATOR::control::useAsBaseFunction:
            contstr = "Osc As Base";
            break;
        case OSCILLATOR::control::waveshapeParameter:
            contstr = "Waveshape Par";
            break;
        case OSCILLATOR::control::waveshapeType:
            contstr = "Waveshape Type";
            break;
        case OSCILLATOR::control::filterParameter1:
            contstr = "Osc Filt Par 1";
            break;
        case OSCILLATOR::control::filterParameter2:
            contstr = "Osc Filt Par 2";
            break;
        case OSCILLATOR::control::filterBeforeWaveshape:
            contstr = "Osc Filt B4 Waveshape";
            break;
        case OSCILLATOR::control::filterType:
            contstr = "Osc Filt Type ";
            if (addValue)
            {
                showValue = false;
                contstr += filtertype[value_int];
            }
            break;
        case OSCILLATOR::control::modulationParameter1:
            contstr = "Osc Mod Par 1";
            break;
        case OSCILLATOR::control::modulationParameter2:
            contstr = "Osc Mod Par 2";
            break;
        case OSCILLATOR::control::modulationParameter3:
            contstr = "Osc Mod Par 3";
            break;
        case OSCILLATOR::control::modulationType:
            contstr = "Osc Mod Type";
            break;
        case OSCILLATOR::control::spectrumAdjustParameter:
            contstr = "Osc Spect Par";
            break;
        case OSCILLATOR::control::spectrumAdjustType:
            contstr = "Osc Spect Type";
            break;
    }
    if (!contstr.empty())
    {
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + eng_name + " Base Mods " + contstr + isPad);
    }

    switch(control)
    {
        case OSCILLATOR::control::harmonicShift:
            contstr = "Shift";
            break;
        case OSCILLATOR::control::clearHarmonicShift:
            contstr = "Reset";
            break;
        case OSCILLATOR::control::shiftBeforeWaveshapeAndFilter:
            contstr = "B4 Waveshape & Filt";
            break;
        case OSCILLATOR::control::adaptiveHarmonicsParameter:
            contstr = "Adapt Param";
            break;
        case OSCILLATOR::control::adaptiveHarmonicsBase:
            contstr = "Adapt Base Freq";
            break;
        case OSCILLATOR::control::adaptiveHarmonicsPower:
            contstr = "Adapt Power";
            break;
        case OSCILLATOR::control::adaptiveHarmonicsType:
            contstr = "Adapt Type";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised Oscillator";
            break;
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + eng_name + " Harm Mods " + contstr + isPad);
}


string DataText::resolveResonance(SynthEngine& synth, CommandBlock& cmd, bool addValue)
{
    int value = int(cmd.data.value + 0.5f);

    uchar type      = cmd.data.type;
    uchar control   = cmd.data.control;
    uchar npart     = cmd.data.part;
    uchar kititem   = cmd.data.kit;
    uchar engine    = cmd.data.engine;
    uchar insert    = cmd.data.insert;
    uchar parameter = cmd.data.parameter;
    bool write = (type & TOPLEVEL::type::Write) > 0;

    string name;
    string isPad;
    if (engine == PART::engine::padSynth && control != PADSYNTH::control::applyChanges)
    {
        name = " PadSynth";
        if (write)
            isPad = synth.getRuntime().usePadAutoApply()? " - rebuilding PAD"
                                                        : " - Need to Apply";
    }
    else
        name = " AddSynth";

    if (insert == TOPLEVEL::insert::resonanceGraphInsert)
    {
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + name + " Resonance Point " + to_string(parameter + 1) + isPad);
    }
    string contstr;
    switch (control)
    {
        case RESONANCE::control::maxDb:
            contstr = "Max dB";
            break;
        case RESONANCE::control::centerFrequency:
            contstr = "Center Freq";
            break;
        case RESONANCE::control::octaves:
            contstr = "Octaves";
            break;

        case RESONANCE::control::enableResonance:
            contstr = "Enable";
            yesno = true;
            break;

        case RESONANCE::control::randomType:
            contstr = "Random";
            showValue = false;
            if (addValue)
            {
                if (value == 0)
                    contstr += " - coarse";
                else if (value == 1)
                    contstr += " - medium";
                else
                    contstr += " - fine";
            }
            break;

        case RESONANCE::control::interpolatePeaks:
            contstr = "Interpolate Peaks";
            showValue = false;
            if (addValue && value == 0)
                contstr += " - smooth";
            else
                contstr += " - linear";
            break;
        case RESONANCE::control::protectFundamental:
            contstr = "Protect Fundamental";
            yesno = true;
            break;

        case RESONANCE::control::clearGraph:
            showValue = false;
            contstr = "Clear";
            break;
        case RESONANCE::control::smoothGraph:
            showValue = false;
            contstr = "Smooth";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised Resonance";
            break;
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + name + " Resonance " + contstr + isPad);
}


string DataText::resolveLFO(CommandBlock& cmd, bool addValue)
{
    float value   = cmd.data.value;
    int value_int = int(value);
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;
    uchar insertParam = cmd.data.parameter;

    string name;
    string lfo;

    if (engine == PART::engine::addSynth)
        name = " AddSynth";
    else if (engine == PART::engine::padSynth)
        name = " PadSynth";
    else if (engine >= PART::engine::addVoice1)
    {
        int nvoice = engine - PART::engine::addVoice1;
        name = " Add Voice " + to_string(nvoice + 1);
    }

    switch (insertParam)
    {
        case TOPLEVEL::insertType::amplitude:
            lfo = " Amp";
            break;
        case TOPLEVEL::insertType::frequency:
            lfo = " Freq";
            break;
        case TOPLEVEL::insertType::filter:
            lfo = " Filt";
            break;
    }

    string contstr;
    switch (control)
    {
        case LFOINSERT::control::speed:
            if (cmd.data.offset == 1 && addValue == true)
            {
                contstr += bpm2text(value);
                showValue = false;
            }
            else
                contstr = "Freq";
            break;
        case LFOINSERT::control::depth:
            contstr = "Depth";
            break;
        case LFOINSERT::control::delay:
            contstr = "Delay";
            break;
        case LFOINSERT::control::start:
            contstr = "Start";
            break;
        case LFOINSERT::control::amplitudeRandomness:
            contstr = "Amp Rand";
            break;
        case LFOINSERT::control::type:
        {
            contstr = "Type ";
            showValue = false;
            if (addValue)
                contstr += stringCaps(LFOtype[value_int], 1);
            break;
        }
        case LFOINSERT::control::continuous:
            contstr = "Cont";
            yesno = true;
            break;
        case LFOINSERT::control::bpm:
            contstr = "BPM";
            yesno = true;
            break;
        case LFOINSERT::control::frequencyRandomness:
            contstr = "Freq Rand";
            break;
        case LFOINSERT::control::stretch:
            contstr = "Stretch";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised LFO";
            break;
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + name + lfo + " LFO " + contstr);
}


string DataText::resolveFilter(CommandBlock& cmd, bool addValue)
{
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;

    string name;

    if (engine == PART::engine::addSynth)
        name = " AddSynth";
    else if (engine == PART::engine::subSynth)
        name = " SubSynth";
    else if (engine == PART::engine::padSynth)
        name = " PadSynth";
    else if (engine >= PART::engine::addVoice1)
        name = " Adsynth Voice " + to_string((engine - PART::engine::addVoice1) + 1);
    string contstr = filterControl(cmd, addValue);

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + name + " Filter " + contstr);
}


string DataText::filterControl(CommandBlock& cmd, bool addValue)
{
    int value_int = int(cmd.data.value);
    uchar control = cmd.data.control;

    int nformant = cmd.data.parameter;
    int nseqpos  = cmd.data.parameter;
    int nvowel   = cmd.data.offset;

    string contstr;
    switch (control)
    {
        case FILTERINSERT::control::centerFrequency:
            contstr = "Cent Freq";
            break;
        case FILTERINSERT::control::Q:
            contstr = "Q";
            break;
        case FILTERINSERT::control::frequencyTracking:
            contstr = "Freq Track";
            break;
        case FILTERINSERT::control::velocitySensitivity:
            contstr = "Velocity Sense";
            break;
        case FILTERINSERT::control::velocityCurve:
            contstr = "Velocity Sense Curve";
            break;
        case FILTERINSERT::control::gain:
            contstr = "Gain";
            break;
        case FILTERINSERT::control::stages:
            showValue = false;
            contstr = "Stages " + to_string(value_int + 1);
            break;
        case FILTERINSERT::control::baseType:
            contstr = "Filt Cat ";
            showValue = false;
            switch (value_int)
            {
                case 0:
                    contstr += "Analog";
                    break;
                case 1:
                    contstr += "Form";
                    break;
                case 2:
                    contstr += "StVar";
                    break;
                default:
                    contstr += "Unrecognised Filter Base";
                    break;
            }
            break;
        case FILTERINSERT::control::analogType:
        {
            contstr = "An Type ";
            showValue = false;
            int idx = 0;
            if (addValue)
            {
                while (filterlist [idx] != "l1")
                    idx += 2;
                contstr += filterlist [idx + (value_int * 2)];
            }
            break;
        }
        case FILTERINSERT::control::stateVariableType:
        {
            contstr = "SV Type";
            int idx = 0;
            if (addValue)
            {
                while (filterlist [idx] != "low")
                    idx += 2;
                contstr += filterlist [idx + (value_int * 2)];
            }
            break;
        }
        case FILTERINSERT::control::frequencyTrackingRange:
            contstr = "Freq Track Offs";
            yesno = true;
            break;
        case FILTERINSERT::control::formantSlowness:
            contstr = "Form Morph";
            break;
        case FILTERINSERT::control::formantClearness:
            contstr = "Form Lucidity";
            break;
        case FILTERINSERT::control::formantFrequency:
            contstr = "Form Freq";
            break;
        case FILTERINSERT::control::formantQ:
            contstr = "Form Q";
            break;
        case FILTERINSERT::control::formantAmplitude:
            contstr = "Form Amp";
            break;
        case FILTERINSERT::control::formantStretch:
            contstr = "Form Stretch";
            break;
        case FILTERINSERT::control::formantCenter:
            contstr = "Form Cent Freq";
            break;
        case FILTERINSERT::control::formantOctave:
            contstr = "Form Octave";
            break;

        case FILTERINSERT::control::numberOfFormants:
            contstr = "Formants";
            break;
        case FILTERINSERT::control::vowelNumber:
            contstr = "Vowel Num";
            if (addValue)
            {
                contstr += " Value " + to_string(value_int + 1);
            }
            showValue = false;
            break;
        case FILTERINSERT::control::formantNumber:
            contstr = "Formant Num";
            break;
        case FILTERINSERT::control::sequenceSize:
            contstr = "Seq Size";
            break;
        case FILTERINSERT::control::sequencePosition:
            contstr = "Seq Pos";
            break;
        case FILTERINSERT::control::vowelPositionInSequence:
            contstr = "Vowel";
            break;
        case FILTERINSERT::control::negateInput:
            contstr = "Neg Input";
            yesno = true;
            break;

        default:
            showValue = false;
            contstr = "Unrecognised Filter";
            break;
    }
    if (control >= FILTERINSERT::control::formantFrequency && control <= FILTERINSERT::control::formantAmplitude)
    {
        contstr = "Vowel " + to_string(nvowel + 1) + " Formant " + to_string(nformant + 1) + " "+ contstr;
    }
    else if (control == FILTERINSERT::control::sequencePosition)
    {
        if (addValue)
        {
            contstr += " Value " + to_string(value_int + 1);
        }
        showValue = false;
    }
    else if (control == FILTERINSERT::control::vowelPositionInSequence)
    {
        contstr = "Seq Pos " + to_string(nseqpos + 1) + " " + contstr;
        if (addValue)
        {
            contstr += " Value " + to_string(value_int + 1);
        }
        showValue = false;
    }
    return contstr;
}


string DataText::resolveEnvelope(CommandBlock& cmd, bool)
{
    int value = lrint(cmd.data.value);
    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;

    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;
    uchar insert  = cmd.data.insert;
    uchar offset  = cmd.data.offset;
    uchar insertParam = cmd.data.parameter;

    string env;
    string name;
    if (engine == PART::engine::addSynth)
        name = " AddSynth";
    else if (engine == PART::engine::subSynth)
        name = " SubSynth";

    else if (engine == PART::engine::padSynth)
        name = " PadSynth";
    else if (engine >= PART::engine::addVoice1)
    {
        name = " Add Voice ";
        int nvoice;
        if (engine >= PART::engine::addMod1)
            nvoice = engine - PART::engine::addMod1;
        else
            nvoice = engine - PART::engine::addVoice1;
        name += to_string(nvoice + 1);
        if (engine >= PART::engine::addMod1)
            name += " Modulator";
    }

    switch(insertParam)
    {
        case TOPLEVEL::insertType::amplitude:
            env = " Amp";
            break;
        case TOPLEVEL::insertType::frequency:
            env = " Freq";
            break;
        case TOPLEVEL::insertType::filter:
            env = " Filt";
            break;
        case TOPLEVEL::insertType::bandwidth:
            env = " Band";
            break;
    }

    if (insert == TOPLEVEL::insert::envelopePointAdd || insert == TOPLEVEL::insert::envelopePointDelete)
    {
        if (!write)
        {
            return ("Freemode add/remove is write only. Current points " + to_string(value));
        }
        if (insert == TOPLEVEL::insert::envelopePointAdd)
            return ("Part " + to_string(int(npart + 1)) + " Kit " + to_string(int(kititem + 1)) + name  + env + " Env Added Freemode Point " + to_string(int(control & 0x3f)) + " X increment " + to_string(int(offset)) + " Y");
        else
        {
            showValue = false;
            return ("Part " + to_string(int(npart + 1)) + " Kit " + to_string(int(kititem + 1)) + name  + env + " Env Removed Freemode Point " +  to_string(int(control)) + "  Remaining " +  to_string(value));
        }
    }

    if (insert == TOPLEVEL::insert::envelopePointChange)
    {
        return ("Part " + to_string(int(npart + 1)) + " Kit " + to_string(int(kititem + 1)) + name  + env + " Env Freemode Point " +  to_string(int(control)) + " X increment " + to_string(int(offset)) + " Y");
    }

    string contstr;
    switch (control)
    {
        case ENVELOPEINSERT::control::attackLevel:
            contstr = "Attack Level";
            break;
        case ENVELOPEINSERT::control::attackTime:
            contstr = "Attack Time";
            break;
        case ENVELOPEINSERT::control::decayLevel:
            contstr = "Decay Level";
            break;
        case ENVELOPEINSERT::control::decayTime:
            contstr = "Decay Time";
            break;
        case ENVELOPEINSERT::control::sustainLevel:
            contstr = "Sustain Level";
            break;
        case ENVELOPEINSERT::control::releaseLevel:
            contstr = "Release Level";
            break;
        case ENVELOPEINSERT::control::releaseTime:
            contstr = "Release Time";
            break;
        case ENVELOPEINSERT::control::stretch:
            contstr = "Stretch";
            break;

        case ENVELOPEINSERT::control::forcedRelease:
            contstr = "frcR";
            yesno = true;
            break;
        case ENVELOPEINSERT::control::linearEnvelope:
            contstr = "L";
            yesno = true;
            break;

        case ENVELOPEINSERT::control::edit:
            contstr = "Edit";
            break;

        case ENVELOPEINSERT::control::enableFreeMode:
            contstr = "Freemode";
            yesno = true;
            break;
        case ENVELOPEINSERT::control::points:
            contstr = "Points";
            break;
        case ENVELOPEINSERT::control::sustainPoint:
            contstr = "Sust";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised Envelope";
            break;
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(int(kititem + 1)) + name  + env + " Env " + contstr);
}


string DataText::resolveEffects(CommandBlock& cmd, bool addValue)
{
    int value = lrint(cmd.data.value);
    uchar control   = cmd.data.control;
    uchar npart     = cmd.data.part;
    uchar effType   = cmd.data.kit;
    uchar effnum    = cmd.data.engine;
    uchar insert    = cmd.data.insert;
    uchar parameter = cmd.data.parameter;
    uchar offset    = cmd.data.offset;

    string name;
    string actual;
    if (npart == TOPLEVEL::section::systemEffects)
        name = "System";
    else if (npart == TOPLEVEL::section::insertEffects)
        name = "Insert";
    else
        name = "Part " + to_string(npart + 1);

    if (effType == EFFECT::type::dynFilter && cmd.data.insert != UNUSED)
    {
        if (npart == TOPLEVEL::section::systemEffects)
            name = "System";
        else if (npart == TOPLEVEL::section::insertEffects)
            name = "Insert";
        else name = "Part " + to_string(npart + 1);
        name += " Effect " + to_string(effnum + 1);
        name += " DynFilter ~ Filter ";
        name += filterControl(cmd, addValue);
        return name;
    }

    name += " Effect " + to_string(effnum + 1);

    string effname = "";
    if (npart < NUM_MIDI_PARTS && (control == PART::control::effectNumber || control == PART::control::effectDestination || control == PART::control::effectBypass))
    {
        if (control == PART::control::effectNumber)
            name = "Set " + name;
        else if (control == PART::control::effectDestination)
        {
            effname = " sent to ";
            if (value == 0)
                effname += "next effect";
            else if (value == 1)
                effname += "part out";
            else if (value == 2)
                effname += "dry out";
        }
        if (control == PART::control::effectBypass)
        {
            effname = " Bypass";
            showValue = false;
            if (addValue)
            {
                if (value)
                    effname += " - on";
                else
                    effname += " - off";
            }
        }
        else
            showValue = false;
        return (name + effname);
    }
    else if (npart >= TOPLEVEL::section::systemEffects && effType == UNUSED)
    {
        string contstr;
        string second = "";
        if (npart == TOPLEVEL::section::systemEffects)
        {
            if (insert == TOPLEVEL::insert::systemEffectSend)
            {
                contstr = " from Effect " + to_string(effnum + 1);
                second = " to Effect " + to_string(control + 1);
                return (name + contstr + second);
            }
            if (control == EFFECT::sysIns::effectEnable)
            {
                contstr += " Enable";
                if (addValue)
                {
                    showValue = false;
                    if (value > 0)
                        contstr += " - on";
                    else
                        contstr += " - off";
                }
                return (name + contstr);
            }

        }
        if (npart == TOPLEVEL::section::insertEffects && control == EFFECT::sysIns::effectDestination)
        {
            contstr = " To ";
            if (value == -2)
                contstr += "Master out";
            else if (value == -1)
                contstr = " - off";
            else
            {
                contstr += "Part ";
                second = to_string(value + 1);
            }
            showValue = false;
            return ("Send " + name + contstr + second);
        }
        if (control == EFFECT::sysIns::effectNumber)
        {
            name = "Set " + name;
            showValue = false;
            return (name + effname);
        }
    }
    string contstr = "";
    if ((npart < NUM_MIDI_PARTS && control == PART::control::effectType) || (npart > TOPLEVEL::section::main && effType == UNUSED && control == EFFECT::sysIns::effectType))
    {
        name += " set to";
        effType = value | EFFECT::type::none; // TODO fix this!
        showValue = false;
    }
    else
        contstr = ""; //" Control " + to_string(control + 1);
    string controlType = "";
    int ref = control; // we frequently modify this#
    bool isBPM = ((ref == 2 && (offset == 1 || offset == 3)) || (ref == 3 && offset == 3));
    switch (effType)
    {
        case EFFECT::type::none:
            effname = " None";
            contstr = " ";
            break;
        case EFFECT::type::reverb:
        {
            ref = mapFromEffectNumber(ref, reverblistmap);
            effname = " Reverb ";
            controlType = reverblist[(ref) * 2];
            if (control == 10 && addValue == true)// && offset > 0)
            {
                showValue = false;
                switch (value)
                {
                    case 0:
                        contstr = " Random ";
                        break;
                    case 1:
                        contstr = " Freeverb ";
                        break;
                    case 2:
                        contstr = " Bandwidth ";
                        break;
                }
            }
            break;
        }
        case EFFECT::type::echo:
            effname = " Echo ";
            ref = mapFromEffectNumber(ref, echolistmap);;
            controlType = echolist[ref * 2];
            if (addValue == true) // && offset > 0)
            {
                if (isBPM)
                {
                    showValue = false;
                    contstr += (" " + bpm2text(float(value) / 127.0f));
                }
                if (control == 7 || control == 17)
                    yesno = true;
            }
            break;
        case EFFECT::type::chorus:
        {
            effname = " Chorus ";
            ref = mapFromEffectNumber(ref, choruslistmap);
            controlType = choruslist[ref * 2];
            if (addValue && offset > 0)
            {
                if (control == 4)
                {
                    showValue = false;
                    if (value)
                        contstr = " Triangle";
                    else
                        contstr = " Sine";
                }
                else if (isBPM)
                {
                    showValue = false;
                    contstr += (" " + bpm2text(float(value) / 127.0f));
                }
                if (control == 11 || control == 17)
                {
                    yesno = true;
                }
            }
            break;
        }
        case EFFECT::type::phaser:
            effname = " Phaser ";
            ref = mapFromEffectNumber(ref, phaserlistmap);
            controlType = phaserlist[ref * 2];
            if (addValue == true) // && offset > 0)
            {
                if (isBPM)
                {
                    showValue = false;
                    contstr += (" " + bpm2text(float(value) / 127.0f));
                }
                switch (control)
                {
                    case 4:
                        showValue = false;
                        if (value)
                            contstr = " Triangle";
                        else
                            contstr = " Sine";
                        break;
                    case 10:
                    case 12:
                    case 14:
                    case 17:
                        yesno = true;
                        break;
                }
            }
            break;
        case EFFECT::type::alienWah:
            effname = " AlienWah ";
            ref = mapFromEffectNumber(ref, alienwahlistmap);
            controlType = alienwahlist[ref * 2];
            if (addValue == true) // && offset > 0)
            {
                if (isBPM)
                {
                    showValue = false;
                    contstr += (" " + bpm2text(float(value) / 127.0f));
                }
                if (control == 4  && offset > 0)
                {
                    showValue = false;
                    if (value)
                        contstr = " Triangle";
                    else
                    contstr = " Sine";
                }
                else if (control == 17)
                    yesno = true;
            }
            break;
        case EFFECT::type::distortion:
        {
            effname = " Distortion ";
            ref = mapFromEffectNumber(ref, distortionlistmap);
            if (addValue == true) // && offset > 0)
            {
                switch (ref)
                {
                    case 5:
                        contstr = " " + stringCaps(effdistypes[value], 1);
                        showValue = false;
                        break;
                    case 11:
                    {
                        contstr = " Pre dist.";
                        yesno = true;
                        break;
                    }
                    case 7:
                    case 10:
                    {
                        yesno = true;
                        break;
                    }
                }
            }
            controlType = distortionlist[ref * 2];
            break;
        }
        case EFFECT::type::eq:
        {
            effname = " EQ ";
            if (control == 1)
            {
                contstr = " " + to_string(int(value) + 1);
                showValue = false;
            }
            else if (control > 1)
            {
                if (offset > 0)
                    effname += "(Band " + to_string(int(parameter) + 1) + ") ";
                ref = mapFromEffectNumber(ref, eqlistmap);
                if (ref < 4 && addValue == true && offset > 0)
                {
                    showValue = false;
                    contstr = " " + stringCaps(eqtypes[value], 1);
                }
            }
            controlType = eqlist[ref * 2];
            break;
        }
        case EFFECT::type::dynFilter:
            effname = " DynFilter ";
            ref = mapFromEffectNumber(ref, dynfilterlistmap);
            controlType = dynfilterlist[ref * 2];
            if (addValue == true)// && offset > 0)
            {
                if (control == 17)
                {
                    contstr = "bpm";
                    yesno = true;
                    return (name + effname + contstr);
                }
                if (control == 4)
                {
                    showValue = false;
                    if (value)
                        contstr = " Triangle";
                    else
                        contstr = " Sine";
                }
                else if (control == 8)
                {
                    yesno = true;
                }
                if (offset == 1 && ref == 2)
                {
                    showValue = false;
                    contstr += (" " + bpm2text(float(value) / 127.0f));
                }
            }
            break;

        default:
            showValue = false;
            contstr = " Unrecognised Effect";
            break;
    }

    if (control == EFFECT::control::preset && effType != EFFECT::type::eq)
    {
        contstr = " Preset " + to_string (value + 1);
        showValue = false;
    }
    else if (offset)
    {
        controlType = controlType.substr(0, controlType.find(' '));
        effname += stringCaps(controlType, 1);
    }

    return (name + effname + contstr);
}


int DataText::mapFromEffectNumber(int effectIndex, const int list [])
{
    for (int index = 0; list[index] >= 0; index++)
    {
        if (list[index] == effectIndex)
        {
            return index;
        }
    }
    // Kind of bad to return a bogus entry, but this function is often called
    // even when the result will not be used, and the index is often out of
    // range then.
    return 0;
}
