/*
    Text2Data.cpp - conversion of text to commandBlock entries

    Copyright 2023, Will Godfrey
    Copyright 2024, Kristian Amlie

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
#include <iostream>
#include <stdlib.h>

#include "Interface/Text2Data.h"
#include "Interface/TextLists.h"
#include "Interface/InterChange.h"
#include "Misc/SynthEngine.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/FormatFuncs.h"
#include "Misc/NumericFuncs.h"

using std::string;
using std::to_string;
using std::cout;
using std::endl;

void TextData::encodeAll(SynthEngine *_synth, string &sentCommand, CommandBlock &allData)
{
    memset(&allData.bytes, 255, sizeof(allData));

    oursynth = _synth;
    string source = sentCommand;
    strip (source);
    if (source.empty())
    {
        allData.data.control = TOPLEVEL::control::unrecognised;
        allData.data.source = TOPLEVEL::action::noAction;
        log(source, "empty Command String");
        return;
    }
    encodeLoop(source, allData);

    /*
     * If we later decide to be able to set and read values
     * this is where the code should go in order to catch
     * all of the subroutines.
     *
     * MIDI-learn will not use this
     */

    /*size_t pos = source.find("Value");
    if (pos != string::npos)
    { // done directly - we don't know 'source' is tidy
        source = source.substr(pos);
        nextWord(source);
        if (isdigit(source[0]))
        {
            allData.data.value = string2float(source, NULL);
            // need a ring buffer to write allData CommandBlock
        }
        else
            log (source, "no value to write given");
    }
    else
    {
        // return the supplied allData CommandBlock
        // and/or the supplied string
        allData.data.type = 0;
        allData.data.value = oursynth->interchange.readAllData(&allData);
        sentCommand += (" Value >" + to_string(allData.data.value));
    }*/
}


void TextData::log(string& line, string text)
{
    oursynth->getRuntime().Log("Error: " + text);
    // we may later decide to print the string before emptying it

    line = "";
}


void TextData::strip(string& line)
{
    size_t pos = line.find_first_not_of(" ");
    if (pos == string::npos)
        line = "";
    else
        line = line.substr(pos);
}


void TextData::nextWord(string& line)
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


bool TextData::findCharNum(string& line, uchar& value)
{
    if (!isdigit(line[0]))
        return false;
    value = stoi(line) - 1;
    nextWord(line);
    return true;
}

bool TextData::findAndStep(string& line, string text, bool step)
{
    // now case insensitive
    transform(text.begin(), text.end(), text.begin(), ::tolower);
    string lineCopy{line};
    transform(lineCopy.begin(), lineCopy.end(), lineCopy.begin(), ::tolower);
    size_t pos = lineCopy.find(text);
    if (pos != string::npos && pos < 3) // allow leading spaces
    {
        if (step)
        {
            pos += text.length();
            line = line.substr(pos);
            nextWord(line);
        }
        return true;
    }
    return false;
}

int TextData::findListEntry(string& line, int step, const string list [])
{
    int count = 0;
    bool found = false;
    string test;
    do {
        test = list [count];
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

int TextData::mapToEffectNumber(int textIndex, const int list [])
{
    return list[textIndex];
}

int TextData::findEffectFromText(string &line, int step, const string list [], const int listmap [])
{
    return mapToEffectNumber(findListEntry(line, step, list), listmap);
}

void TextData::encodeLoop(string source, CommandBlock& allData)
{
    /* NOTE
     * subsections must *always* come before local controls!
     */
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

    if (findAndStep(source, "Scales"))
    {
        encodeScale(source, allData);
        return;
    }

    if (findAndStep(source, "Part"))
    {
        encodePart(source, allData);
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    log(source, "bad Command String");
}


void TextData::encodeMain(string& source, CommandBlock& allData)
{
    strip (source);
    allData.data.part = TOPLEVEL::section::main;
    if (findAndStep(source, "Master"))
    {
        if (findAndStep(source, "Mono/Stereo"))
        {
            allData.data.control = MAIN::control::mono;
            return;
        }
    }
    if (findAndStep(source, "Volume"))
    {
        allData.data.control = MAIN::control::volume;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "main overflow >" << source << endl;
}


void TextData::encodeScale(string& source, CommandBlock& allData)
{
    strip (source);
    allData.data.part = TOPLEVEL::section::scales;

    uchar ctl = UNUSED;
    if (findAndStep(source, "Enable"))
    {
        if (findAndStep(source, "Microtonal"))
            ctl = SCALES::control::enableMicrotonal;
        else if (findAndStep(source, "Keyboard Mapping"))
            ctl = SCALES::control::enableKeyboardMap;
    }
    else if (findAndStep(source, "Ref note"))
        ctl = SCALES::control::refNote;
    else if (findAndStep(source, "Invert Keys"))
        ctl = SCALES::control::invertScale;
    else if (findAndStep(source, "Key Center"))
        ctl = SCALES::control::invertedScaleCenter;
    else if (findAndStep(source, "Scale Shift"))
        ctl = SCALES::control::scaleShift;
    else if (findAndStep(source, "Keyboard"))
    {
        if (findAndStep(source, "First Note"))
            ctl = SCALES::control::lowKey;
        else if (findAndStep(source, "Middle Note"))
            ctl = SCALES::control::middleKey;
        else if (findAndStep(source, "Last Note"))
            ctl = SCALES::control::highKey;
    }

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "scale overflow >" << source << endl;
}

void TextData::encodePart(string& source, CommandBlock& allData)
{
    strip (source);
    uchar npart = UNUSED;
    if (findCharNum(source, npart))
    {
        if (npart >= NUM_MIDI_PARTS)
        {
            log(source, "part number out of range");
            return;
        }
        allData.data.part = (TOPLEVEL::section::part1 + npart);
        if (findAndStep(source, "Effect"))
        {
            encodeEffects(source, allData);
            return;
        }
    }
    else
        return; // must have a part number!

    uchar kitnum = UNUSED;
    if (findAndStep(source, "Kit"))
    {
        if (findCharNum(source, kitnum))
        {
            if (kitnum >= NUM_KIT_ITEMS)
            {
                log(source, "kit number out of range");
                return;
            }

            allData.data.kit = kitnum;
        }

        //allData.data.insert = TOPLEVEL::insert::kitGroup;
        uchar kitctl = UNUSED;
        if (findAndStep(source, "Mute"))
            kitctl = PART::control::kitItemMute;
        // we may add other controls later
        if (kitctl < UNUSED)
        {
            allData.data.insert = TOPLEVEL::insert::kitGroup;
            allData.data.control = kitctl;
            return;
        }
    }
    if (findAndStep(source, "Controller"))
    {
        encodeController(source, allData);
        return;
    }
    if (findAndStep(source, "MIDI"))
    {
        encodeMidi(source, allData);
        return;
    }

    if (findAndStep(source, "AddSynth"))
    {
        encodeAddSynth(source, allData);
        return;
    }

    if (findAndStep(source, "Add Voice") || findAndStep(source, "Adsynth Voice") || findAndStep(source, "addvoice"))
    {
        uchar voiceNum = UNUSED;
        if (findCharNum(source, voiceNum))
        {
            if (voiceNum >= NUM_VOICES)
            {
                log(source, "voice number out of range");
                return;
            }
            allData.data.engine = PART::engine::addVoice1+voiceNum;
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

    uchar ctl = UNUSED;
    if (findAndStep(source, "Vel"))
    {
        if (findAndStep(source, "Sens"))
            ctl = PART::control::velocitySense;
        else if (findAndStep(source, "Offset"))
            ctl = PART::control::velocityOffset;
    }
    else if (findAndStep(source, "Panning"))
        ctl = PART::control::panning;
    else if (findAndStep(source, "Volume"))
        ctl = PART::control::volume;
    else if (findAndStep(source, "Humanise"))
    {
        if (findAndStep(source, "Pitch"))
            ctl = PART::control::humanise;
        else if (findAndStep(source, "Velocity"))
            ctl = PART::control::humanvelocity;
        else
            ctl = PART::control::humanise; // old single control version
    }
    else if (findAndStep(source, "Portamento Enable") || findAndStep(source, "Portamento"))
        ctl = PART::control::portamento;
    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }
    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "part overflow >" << source << endl;
}

// ----------------------------

void TextData::encodeController(string& source, CommandBlock& allData)
{
    uchar ctl = UNUSED;
    if (findAndStep(source,"Vol"))
    {
        if (findAndStep(source,"Range"))
            ctl = PART::control::volumeRange;
        else if (findAndStep(source,"Enable"))
            ctl = PART::control::volumeEnable;
    }
    else if (findAndStep(source,"Pan Width"))
    {
        ctl = PART::control::panningWidth;
    }
    else if (findAndStep(source,"Mod Wheel Range") || findAndStep(source,"Mod Wheel Depth"))
    {
        ctl = PART::control::modWheelDepth;
    }
    else if (findAndStep(source,"Exponent"))
    {
        if (findAndStep(source,"Mod Wheel"))
        {
            ctl = PART::control::exponentialModWheel;
        }
        else if (findAndStep(source,"Bandwidth"))
            ctl = PART::control::exponentialBandwidth;
    }
    else if (findAndStep(source,"Bandwidth Range") || findAndStep(source,"Bandwidth depth"))
    {
        ctl = PART::control::bandwidthDepth;
    }
    else if (findAndStep(source,"Expression Enable"))
    {
        ctl = PART::control::expressionEnable;
    }
    else if (findAndStep(source,"FM Amp Enable"))
    {
        ctl = PART::control::FMamplitudeEnable;
    }
    else if (findAndStep(source,"Sustain Ped Enable"))
    {
        ctl = PART::control::sustainPedalEnable;
    }
    else if (findAndStep(source,"Pitch Wheel Range"))
    {
        ctl = PART::control::pitchWheelRange;
    }
    else if (findAndStep(source,"Filter"))
    {
        if (findAndStep(source,"Q Range") || findAndStep(source,"Q Depth"))
        {
            ctl = PART::control::filterQdepth;
        }
        else if (findAndStep(source,"Cutoff Range") || findAndStep(source,"Cutoff Depth"))
        {
            ctl = PART::control::filterCutoffDepth;
        }
    }
    else if (findAndStep(source,"Breath Control"))
    {
        ctl = PART::control::breathControlEnable;
    }
    else if (findAndStep(source,"Res"))
    {
        if (findAndStep(source,"Cent Freq Range"))
        {
            ctl = PART::control::resonanceCenterFrequencyDepth;
        }
        else if (findAndStep(source,"Band Range") || findAndStep(source,"Band Depth"))
        {
            ctl = PART::control::resonanceBandwidthDepth;
        }
    }
    else if (findAndStep(source,"Time"))
    {
        if (findAndStep(source,"Stretch"))
            ctl = PART::control::portamentoTimeStretch;
        else
            ctl = PART::control::portamentoTime;
    }
    else if (findAndStep(source,"Portamento"))
    {
        if (findAndStep(source,"Receive"))
            ctl = PART::control::receivePortamento;
    }
    else if (findAndStep(source,"Threshold Gate"))
    {
        if (findAndStep(source,"Type"))
            ctl = PART::control::portamentoThresholdType;
        else
            ctl = PART::control::portamentoThreshold;
    }
    else if (findAndStep(source,"Prop"))
    {
        if (findAndStep(source,"Enable"))
            ctl = PART::control::enableProportionalPortamento;
        else if (findAndStep(source,"Rate"))
            ctl = PART::control::proportionalPortamentoRate;
        else if (findAndStep(source,"depth"))
            ctl = PART::control::proportionalPortamentoDepth;
    }
    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "controller overflow >" << source << endl;
}


void TextData::encodeMidi(string& source, CommandBlock& allData)
{
    uchar ctl = UNUSED;
    if (findAndStep(source,"Modulation"))
        ctl = PART::control::midiModWheel;
    else if (findAndStep(source,"Expression"))
        ctl = PART::control::midiExpression;
    else if (findAndStep(source,"Filter"))
    {
        if (findAndStep(source,"Q"))
            ctl = PART::control::midiFilterQ;
        else if (findAndStep(source,"Cutoff"))
            ctl = PART::control::midiFilterCutoff;
    }
    else if (findAndStep(source,"Bandwidth"))
        ctl = PART::control::midiBandwidth;

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "midi overflow >" << source << endl;
}


void TextData::encodeEffects(string& source, CommandBlock& allData)
{
    if (findAndStep(source, "Send"))
    {
        uchar sendto = UNUSED;
        if (findCharNum(source, sendto))
        {
            allData.data.control = PART::control::partToSystemEffect1 + sendto;
            return;
        }
    }
    uchar effnum = UNUSED;
    if (findCharNum(source, effnum)) // need to find number ranges
    {
        allData.data.engine = effnum;
        if (findAndStep(source, "DynFilter ~ Filter"))
        {
            allData.data.kit = EFFECT::type::dynFilter;
            encodeFilter(source, allData);
            return;
        }
        if (allData.data.part < NUM_MIDI_PARTS)
        {
            if (findAndStep(source, "Bypass") || findAndStep(source, "bypassed"))
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

        uchar efftype = findListEntry(source, 1, fx_list) + EFFECT::type::none;
        if (efftype >= EFFECT::type::count || efftype <= EFFECT::type::none)
        {
            log(source, "effect type out of range");
            return;
        }
        allData.data.kit = efftype;

        // now need to do actual control
        uchar result = UNUSED;
        switch (efftype)
        {
            case EFFECT::type::reverb:
                result = findEffectFromText(source, 2, reverblist, reverblistmap);
                break;
            case EFFECT::type::echo:
                result = findEffectFromText(source, 2, echolist, echolistmap);
                break;
            case EFFECT::type::chorus:
                result = findEffectFromText(source, 2, choruslist, choruslistmap);
                break;
            case EFFECT::type::phaser:
                result = findEffectFromText(source, 2, phaserlist, phaserlistmap);
                break;
            case EFFECT::type::alienWah:
                result = findEffectFromText(source, 2, alienwahlist, alienwahlistmap);
                break;
            case EFFECT::type::distortion:
                result = findEffectFromText(source, 2, distortionlist, distortionlistmap);
                break;

            case EFFECT::type::eq:
                if (findAndStep(source, "(Band", true))
                {
                    uchar tmp;
                    if (findCharNum(source, tmp))
                    allData.data.parameter = tmp;
                }
                result = findEffectFromText(source, 2, eqlist, eqlistmap);
                if (result > 0)
                {
                    if (findAndStep(source, "(Band", true))
                    {
                        uchar tmp;
                        if (findCharNum(source, tmp))
                            allData.data.parameter = tmp;
                    }
                }

                break;
            case EFFECT::type::dynFilter:
                result = findEffectFromText(source, 2, dynfilterlist, dynfilterlistmap);
                break;
            default:
                log(source, "effect control out of range");
                return;
        }
        //allData.data.kit = EFFECT::type::
        allData.data.control = result;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "effects overflow >" << source << endl;
}

// ----------------------------

void TextData::encodeAddSynth(string& source, CommandBlock& allData)
{
    if (findAndStep(source, "Enable"))
    {
        if (allData.data.kit != UNUSED)
            allData.data.insert = TOPLEVEL::insert::kitGroup;
        allData.data.control = PART::control::enableAdd;
        return;
    }
    allData.data.engine = PART::engine::addSynth;
    uchar ctl = UNUSED;

    if (findAndStep(source, "Resonance"))
    {
        encodeResonance(source, allData);
        return;
    }
    else if (findAndStep(source, "Amp Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::amplitude;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Amp LFO"))
    {
        allData.data.parameter = TOPLEVEL::insertType::amplitude;
        encodeLFO(source, allData);
        return;
    }
    else if (findAndStep(source, "Filt Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::filter;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Filt LFO"))
    {
        allData.data.parameter = TOPLEVEL::insertType::filter;
        encodeLFO(source, allData);
        return;
    }
    else if (findAndStep(source, "Filter"))
    {
        encodeFilter(source, allData);
        return;
    }
    else if (findAndStep(source, "Freq Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::frequency;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Freq LFO"))
    {
        allData.data.parameter = TOPLEVEL::insertType::frequency;
        encodeLFO(source, allData);
        return;
    }
    findAndStep(source, "Amplitude") // we just throw this away
        ;
    if (findAndStep(source, "Volume"))
        ctl = ADDSYNTH::control::volume;
    else if (findAndStep(source, "Velocity Sense") || findAndStep(source, "Vel Sens"))
        ctl = ADDSYNTH::control::velocitySense;
    else if (findAndStep(source, "Panning"))
        ctl = ADDSYNTH::control::panning;
    else if (findAndStep(source, "Random Width"))
        ctl = ADDSYNTH::control::randomWidth;
    else if (findAndStep(source, "Stereo"))
        ctl = ADDSYNTH::control::stereo;
    else if (findAndStep(source, "De Pop"))
        ctl = ADDSYNTH::control::dePop;

    else if (findAndStep(source, "Punch"))
    {
        if (findAndStep(source, "Strength") || findAndStep(source, "Strngth"))
            ctl = ADDSYNTH::control::punchStrength;
        else if (findAndStep(source, "Time"))
            ctl = ADDSYNTH::control::punchDuration;
        else if (findAndStep(source, "Stretch") || findAndStep(source, "Strtch"))
            ctl = ADDSYNTH::control::punchStretch;
        else if (findAndStep(source, "Vel"))
            ctl = ADDSYNTH::control::punchVelocity;
    }

    findAndStep(source, "Frequency"); // throw this away too
    if (findAndStep(source, "Detune"))
        ctl = ADDSYNTH::control::detuneFrequency;
    else if (findAndStep(source, "Octave"))
        ctl = ADDSYNTH::control::octave;
    else if (findAndStep(source, "Relative Bandwidth") ||findAndStep(source, "Rel B Wdth"))
        ctl = ADDSYNTH::control::relativeBandwidth;

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "addsynth overflow >" << source << endl;
}


void TextData::encodeAddVoice(string& source, CommandBlock& allData)
{
    uchar ctl = UNUSED;

    if (findAndStep(source, "Enable"))
        ctl = ADDVOICE::control::enableVoice;

    else if (findAndStep(source, "Resonance"))
    {
        encodeResonance(source, allData);
        return;
    }
    else if (findAndStep(source, "Oscillator", false) || findAndStep(source, "Base", false) || findAndStep(source, "Harm Mods", false) || findAndStep(source, "Harmonic", false))
    {
        encodeWaveform(source, allData);
        return;
    }

    else if (findAndStep(source, "Amp Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::amplitude;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Amp LFO"))
    {
        allData.data.parameter = TOPLEVEL::insertType::amplitude;
        encodeLFO(source, allData);
        return;
    }
    if (findAndStep(source, "Amp"))
    {
        if (findAndStep(source, "Enable Env"))
            ctl = ADDVOICE::control::enableAmplitudeEnvelope;
        else if (findAndStep(source, "Enable LFO"))
            ctl = ADDVOICE::control::enableAmplitudeLFO;
    }
    else if (findAndStep(source, "Filt Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::filter;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Filt LFO"))
    {
        allData.data.parameter = TOPLEVEL::insertType::filter;
        encodeLFO(source, allData);
        return;
    }
    else if (findAndStep(source, "Filter"))
    {
        if (findAndStep(source, "Enable Env"))
            ctl = ADDVOICE::control::enableFilterEnvelope;
        else if (findAndStep(source, "Enable LFO"))
            ctl = ADDVOICE::control::enableFilterLFO;
        else if (findAndStep(source, "Enable"))
            ctl = ADDVOICE::control::enableFilter;
        else
        {
            encodeFilter(source, allData);
            return;
        }
    }

    else if (findAndStep(source, "Modulator"))
    {
        if (findAndStep(source, "Amp Env"))
        {
            allData.data.engine += (PART::engine::addMod1 - PART::engine::addVoice1);
            allData.data.parameter = TOPLEVEL::insertType::amplitude;
            encodeEnvelope(source, allData);
            return;
        }
        if (findAndStep(source, "Freq Env"))
        {
            allData.data.engine += (PART::engine::addMod1 - PART::engine::addVoice1);
            allData.data.parameter = TOPLEVEL::insertType::frequency;
            encodeEnvelope(source, allData);
            return;
        }
        else if (findAndStep(source, "Amp"))
        {
            if (findAndStep(source, "Enable Env"))
                ctl = ADDVOICE::control::enableModulatorAmplitudeEnvelope;
        } // throw it away for the next three controls
        if (findAndStep(source, "Volume"))
            ctl = ADDVOICE::control::modulatorAmplitude;
        else if (findAndStep(source, "Vel Sense") || findAndStep(source, "V Sense"))
            ctl = ADDVOICE::control::modulatorVelocitySense;
        else if (findAndStep(source, "HF Damping") || findAndStep(source, "F Damp"))
            ctl = ADDVOICE::control::modulatorHFdamping;

        if (findAndStep(source, "Freq"))
        {
            if (findAndStep(source, "Enable Env"))
                ctl = ADDVOICE::control::enableModulatorFrequencyEnvelope;
            else
                ctl = ADDVOICE::control::modulatorDetuneFrequency; // old form
        } // throw away for next
        if (findAndStep(source, "Octave"))
            ctl = ADDVOICE::control::modulatorOctave;

        else if (findAndStep(source, "Detune"))
            ctl = ADDVOICE::control::modulatorDetuneFrequency;

        else if (findAndStep(source, "Osc Phase"))
            ctl = ADDVOICE::control::modulatorOscillatorPhase;
    }

    else if (findAndStep(source, "Freq Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::frequency;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Freq LFO"))
    {
        allData.data.parameter = TOPLEVEL::insertType::frequency;
        encodeLFO(source, allData);
        return;
    }
    else if (findAndStep(source, "Freq"))
    {
        if (findAndStep(source, "Enable Env"))
        {
            ctl = ADDVOICE::control::enableFrequencyEnvelope;
            allData.data.control = ctl;
            return;
        }
        else if (findAndStep(source, "Enable LFO"))
        {
            ctl = ADDVOICE::control::enableFrequencyLFO;
            allData.data.control = ctl;
            return;
        }
        // throw away for next few
    }
    if (findAndStep(source, "Bend Adj"))
        ctl = ADDVOICE::control::pitchBendAdjustment;
    else if (findAndStep(source, "Offset Hz"))
        ctl = ADDVOICE::control::pitchBendOffset;
    else if (findAndStep(source, "Equal Temper") || findAndStep(source, "Eq T"))
        ctl = ADDVOICE::control::equalTemperVariation;
    else if (findAndStep(source, "Detune"))
        ctl = ADDVOICE::control::detuneFrequency;
    else if (findAndStep(source, "Octave"))
        ctl = ADDVOICE::control::octave;

    else if (findAndStep(source, "Unison"))
    {
        if (findAndStep(source, "Enable"))
            ctl = ADDVOICE::control::enableUnison;
        else if (findAndStep(source, "Freq Spread"))
            ctl = ADDVOICE::control::unisonFrequencySpread;
        else if (findAndStep(source, "Phase Rnd"))
            ctl = ADDVOICE::control::unisonPhaseRandomise;
        else if (findAndStep(source, "Stereo"))
            ctl = ADDVOICE::control::unisonStereoSpread;
        else if (findAndStep(source, "Vibrato"))
            ctl = ADDVOICE::control::unisonVibratoDepth;
        else if (findAndStep(source, "Vib Speed"))
            ctl = ADDVOICE::control::unisonVibratoSpeed;
    }

    else if (findAndStep(source, "Volume"))
        ctl = ADDVOICE::control::volume;
    else if (findAndStep(source, "Velocity Sense") || findAndStep(source, "Vel Sens"))
        ctl = ADDVOICE::control::velocitySense;
    else if (findAndStep(source, "Panning"))
        ctl = ADDVOICE::control::panning;
    else if (findAndStep(source, "Random Width"))
        ctl = ADDVOICE::control::randomWidth;

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "addvoice overflow >" << source << endl;
}


void TextData::encodeSubSynth(string& source, CommandBlock& allData)
{
    if (findAndStep(source, "Enable"))
    {
        if (allData.data.kit != UNUSED)
            allData.data.insert = TOPLEVEL::insert::kitGroup;
        allData.data.control = PART::control::enableSub;
        return;
    }

    allData.data.engine = PART::engine::subSynth;
    uchar ctl = UNUSED;

    if (findAndStep(source, "Amp Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::amplitude;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Filt Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::filter;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Freq Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::frequency;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Band Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::bandwidth;
        encodeEnvelope(source, allData);
        return;
    }

    if (findAndStep(source, "Filter"))
    {
        if (findAndStep(source, "Enable"))
        {
            ctl = SUBSYNTH::control::enableFilter;
        }
        else
        {
            encodeFilter(source, allData);
            return;
        }
    }
    else if (findAndStep(source, "Stereo"))
        ctl = SUBSYNTH::control::stereo;

    else if (findAndStep(source, "Overtones"))
    {
        if (findAndStep(source, "Par 1"))
            ctl = SUBSYNTH::control::overtoneParameter1;
        else if (findAndStep(source, "Par 2"))
            ctl = SUBSYNTH::control::overtoneParameter2;
        else if (findAndStep(source, "Force H"))
            ctl = SUBSYNTH::control::overtoneForceHarmonics;
    }
    else if (findAndStep(source, "Harmonic"))
    { // has to be before anything starting with Amplitude or Bandwidth
        uchar harmonicNum = UNUSED;
        if (!findCharNum(source, harmonicNum))
        {
            log (source, "no harmonic number");
            return;
        }
        if (findAndStep(source, "Amplitude"))
        {
            allData.data.insert = TOPLEVEL::insert::harmonicAmplitude;
            ctl = harmonicNum;
        }
        else if (findAndStep(source, "Bandwidth"))
        {
            allData.data.insert = TOPLEVEL::insert::harmonicBandwidth;
            ctl = harmonicNum;
        }
        if (ctl < UNUSED)
        {
            allData.data.control = ctl;
            return;
        }
    }
    else if (findAndStep(source, "Bandwidth"))
    {
        if (findAndStep(source, "Env Enab"))
            ctl = SUBSYNTH::control::enableBandwidthEnvelope;
        else if (findAndStep(source, "Band Scale"))
            ctl = SUBSYNTH::control::bandwidthScale;
        else
            ctl = SUBSYNTH::control::bandwidth;
    }
    else if (findAndStep(source, "Frequency"))
    {
        if (findAndStep(source, "Env Enab"))
        {
            ctl = SUBSYNTH::control::enableFrequencyEnvelope;
            allData.data.control = ctl;
            return;
        }
        // throw away for the next few
    }
    else if (findAndStep(source, "Octave"))
        ctl = SUBSYNTH::control::octave;
    else if (findAndStep(source, "Bend Adj"))
        ctl = SUBSYNTH::control::pitchBendAdjustment;
    else if (findAndStep(source, "Offset Hz"))
        ctl = SUBSYNTH::control::pitchBendOffset;
    else if (findAndStep(source, "Equal Temper") || findAndStep(source, "Eq T"))
        ctl = SUBSYNTH::control::equalTemperVariation;
    else if (findAndStep(source, "Detune"))
        ctl = SUBSYNTH::control::detuneFrequency;

    findAndStep(source, "Amplitude"); // throw away for next few
    if (findAndStep(source, "Volume"))
        ctl = SUBSYNTH::control::volume;
    else if (findAndStep(source, "Velocity Sense") || findAndStep(source, "Vel Sens"))
        ctl = SUBSYNTH::control::velocitySense;
    else if (findAndStep(source, "Panning"))
        ctl = SUBSYNTH::control::panning;
    else if (findAndStep(source, "Random Width"))
        ctl = SUBSYNTH::control::randomWidth;
    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "subsynth overflow >" << source << endl;
}


void TextData::encodePadSynth(string& source, CommandBlock& allData)
{
    if (findAndStep(source, "Enable"))
    {
        if (allData.data.kit != UNUSED)
            allData.data.insert = TOPLEVEL::insert::kitGroup;
        allData.data.control = PART::control::enablePad;
        return;
    }


    allData.data.engine = PART::engine::padSynth;
    uchar ctl = UNUSED;

    if (findAndStep(source, "Resonance"))
    {
        encodeResonance(source, allData);
        return;
    }
    else if (findAndStep(source, "Oscillator", false) || findAndStep(source, "Base", false) || findAndStep(source, "Harm Mods", false))
    {
        encodeWaveform(source, allData);
        return;
    }
    else if (findAndStep(source, "Amp Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::amplitude;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Amp LFO"))
    {
        allData.data.parameter = TOPLEVEL::insertType::amplitude;
        encodeLFO(source, allData);
        return;
    }
    else if (findAndStep(source, "Filt Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::filter;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Filt LFO"))
    {
        allData.data.parameter = TOPLEVEL::insertType::filter;
        encodeLFO(source, allData);
        return;
    }
    else if (findAndStep(source, "Filter"))
    {
        encodeFilter(source, allData);
        return;
    }
    else if (findAndStep(source, "Freq Env"))
    {
        allData.data.parameter = TOPLEVEL::insertType::frequency;
        encodeEnvelope(source, allData);
        return;
    }
    else if (findAndStep(source, "Freq LFO"))
    {
        allData.data.parameter = TOPLEVEL::insertType::frequency;
        encodeLFO(source, allData);
        return;
    }
    else if (findAndStep(source, "Harmonic Base"))
    {
        if (findAndStep(source, "Width"))
            ctl =PADSYNTH::control::baseWidth;
        else if (findAndStep(source, "Freq Mult"))
            ctl =PADSYNTH::control::frequencyMultiplier;
        else if (findAndStep(source, "Str"))
            ctl =PADSYNTH::control::modulatorStretch;
        else if (findAndStep(source, "Freq"))
            ctl =PADSYNTH::control::modulatorFrequency;
        else if (findAndStep(source, "Size"))
            ctl =PADSYNTH::control::size;
        else if (findAndStep(source, "Amp Par 1"))
            ctl =PADSYNTH::control::spectralWidth;
        else if (findAndStep(source, "Amp Par 2"))
            ctl =PADSYNTH::control::spectralAmplitude;
    }
    else if (findAndStep(source, "Oscillator", false) || findAndStep(source, "Base", false) || findAndStep(source, "Harm Mods", false) || findAndStep(source, "Harmonic", false))
    { // must come after harmonic base
        encodeWaveform(source, allData);
        return;
    }
    else if (findAndStep(source, "Overtones"))
    {
        findAndStep(source, "Overt"); // throw it away
        if (findAndStep(source, "Par 1"))
            ctl =PADSYNTH::control::overtoneParameter1;
        else if (findAndStep(source, "Par 2"))
            ctl =PADSYNTH::control::overtoneParameter2;
        else if (findAndStep(source, "Force H"))
            ctl =PADSYNTH::control::overtoneForceHarmonics;
    }

    else if (findAndStep(source, "Bandwidth"))
    {
        if (findAndStep(source, "Scale"))
            ; // not yet
        else if(findAndStep(source, "Spectrum Mode")) // old form
            ; // not yet
        else
        {
            findAndStep(source, "Bandwidth"); //throw it away (old form)
            ctl =PADSYNTH::control::bandwidth;
        }
    }
    else if(findAndStep(source, "Spectrum Mode")) // new form
        ; // not yet
    else if(findAndStep(source, "XFade Update"))
        ctl =PADSYNTH::control::xFadeUpdate;
    else if(findAndStep(source, "BuildTrigger"))
        ctl =PADSYNTH::control::rebuildTrigger;
    else if(findAndStep(source, "RWDetune"))
        ctl =PADSYNTH::control::randWalkDetune;
    else if(findAndStep(source, "RWBandwidth"))
        ctl =PADSYNTH::control::randWalkBandwidth;
    else if(findAndStep(source, "RWFilterFreq"))
        ctl =PADSYNTH::control::randWalkFilterFreq;
    else if(findAndStep(source, "RWWidthProfile"))
        ctl =PADSYNTH::control::randWalkProfileWidth;
    else if(findAndStep(source, "RWStretchProfile"))
        ctl =PADSYNTH::control::randWalkProfileStretch;

    else if (findAndStep(source, "Changes Applied"))
        ctl =PADSYNTH::control::applyChanges;

    findAndStep(source, "Amplitude"); // throw it away for the next few
    if (findAndStep(source, "Volume"))
        ctl =PADSYNTH::control::volume;
    else if (findAndStep(source, "Velocity Sense") || findAndStep(source, "Vel Sens"))
        ctl =PADSYNTH::control::velocitySense;
    else if (findAndStep(source, "Panning"))
        ctl =PADSYNTH::control::panning;
    else if (findAndStep(source, "Random Pan"))
        ctl =PADSYNTH::control::enableRandomPan;
    else if (findAndStep(source, "Random Width"))
        ctl =PADSYNTH::control::randomWidth;

    else if (findAndStep(source, "Punch"))
    {
        if (findAndStep(source, "Strength") || findAndStep(source, "Strngth"))
            ctl =PADSYNTH::control::punchStrength;
        else if (findAndStep(source, "Time"))
            ctl =PADSYNTH::control::punchDuration;
        else if (findAndStep(source, "Stretch") || findAndStep(source, "Strtch"))
            ctl =PADSYNTH::control::punchStretch;
        else if (findAndStep(source, "Vel"))
            ctl =PADSYNTH::control::punchVelocity;
    }
    else if (findAndStep(source, "Stereo"))
            ctl =PADSYNTH::control::stereo;
    else if (findAndStep(source, "De Pop"))
            ctl =PADSYNTH::control::dePop;

    findAndStep(source, "Frequency"); // throw it away for the next few
    if (findAndStep(source, "Bend Adj"))
        ctl =PADSYNTH::control::pitchBendAdjustment;
    else if (findAndStep(source, "Offset Hz"))
        ctl =PADSYNTH::control::pitchBendOffset;
    else if (findAndStep(source, "440Hz"))
        ctl =PADSYNTH::control::baseFrequencyAs440Hz;
    else if (findAndStep(source, "Detune"))
        ctl =PADSYNTH::control::detuneFrequency;
    else if (findAndStep(source, "Equal Temper") || findAndStep(source, "Eq T"))
        ctl =PADSYNTH::control::equalTemperVariation;
    else if (findAndStep(source, "Octave"))
        ctl =PADSYNTH::control::octave;

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "padsynth overflow >" << source << endl;
}

// ----------------------------

void TextData::encodeWaveform(string& source, CommandBlock& allData)
{
    uchar ctl = UNUSED;
    allData.data.insert = TOPLEVEL::insert::oscillatorGroup;

    if (findAndStep(source, "Harmonic"))
    {
        if (findCharNum(source, ctl))
            allData.data.control = ctl;
        else
        {
            log(source, " no harmonic number");
            return;
        }

        if (findAndStep(source, "Amplitude"))
            allData.data.insert = TOPLEVEL::insert::harmonicAmplitude;
        else if (findAndStep(source, "Phase"))
            allData.data.insert = TOPLEVEL::insert::harmonicPhase;
        else
            log(source, " no harmonic type");
    }
    else if (findAndStep(source, "Oscillator"))
    {
        if (findAndStep(source, "Random"))
            ctl = OSCILLATOR::control::phaseRandomness;
        else if (findAndStep(source, "Harm Rnd"))
            ctl = OSCILLATOR::control::harmonicAmplitudeRandomness;
    }
    else if (findAndStep(source, "Harm Mods"))
    {
        if (findAndStep(source, "Adapt Param"))
            ctl = OSCILLATOR::control::adaptiveHarmonicsParameter;
        else if (findAndStep(source, "Adapt Base Freq"))
            ctl = OSCILLATOR::control::adaptiveHarmonicsBase;
        else if (findAndStep(source, "Adapt Power"))
            ctl = OSCILLATOR::control::adaptiveHarmonicsPower;
    }
    else if (findAndStep(source, "Base Mods"))
    {
        if (findAndStep(source, "Osc"))
        {
            if (findAndStep(source, "Filt Par 1"))
                ctl = OSCILLATOR::control::filterParameter1;
            else if (findAndStep(source, "Filt Par 2"))
                ctl = OSCILLATOR::control::filterParameter2;
            else if (findAndStep(source, "Mod Par 1"))
                ctl = OSCILLATOR::control::modulationParameter1;
            else if (findAndStep(source, "Mod Par 2"))
                ctl = OSCILLATOR::control::modulationParameter2;
            else if (findAndStep(source, "Mod Par 3"))
                ctl = OSCILLATOR::control::modulationParameter3;
            else if (findAndStep(source, "Spect Par"))
                ctl = OSCILLATOR::control::spectrumAdjustParameter;
        }
        else if (findAndStep(source, "Waveshape Par"))
            ctl = OSCILLATOR::control::waveshapeParameter;
    }
    else if (findAndStep(source, "Base Funct"))
    {
        if (findAndStep(source, "Par"))
            ctl = OSCILLATOR::control::baseFunctionParameter;
        else if (findAndStep(source, "Mod Par 1"))
            ctl = OSCILLATOR::control::baseModulationParameter1;
        else if (findAndStep(source, "Mod Par 2"))
            ctl = OSCILLATOR::control::baseModulationParameter2;
        else if (findAndStep(source, "Mod Par 3"))
            ctl = OSCILLATOR::control::baseModulationParameter3;
    }

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "waveform overflow >" << source << endl;
}


void TextData::encodeResonance(string& source, CommandBlock& allData)
{
    uchar ctl = UNUSED;
    allData.data.insert = TOPLEVEL::insert::resonanceGroup;
     // this might be changed for graph inserts

    if (findAndStep(source, "Max dB"))
        ctl = RESONANCE::control::maxDb;
    if (findAndStep(source, "Center Freq"))
        ctl = RESONANCE::control::centerFrequency;
    if (findAndStep(source, "Octaves"))
        ctl = RESONANCE::control::octaves;
    if (findAndStep(source, "Random"))
        ctl = RESONANCE::control::randomType;

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "resonance overflow >" << source << endl;
}

// ----------------------------

void TextData::encodeLFO(string& source, CommandBlock& allData)
{
    uchar ctl = UNUSED;
    allData.data.insert = TOPLEVEL::insert::LFOgroup;

    if (findAndStep(source, "Freq Random") ||findAndStep(source, "FreqRand")) // must be before Freq
        ctl = LFOINSERT::control::frequencyRandomness;
    else if (findAndStep(source, "Freq"))
        ctl = LFOINSERT::control::speed;
    else if (findAndStep(source, "Depth"))
        ctl = LFOINSERT::control::depth;
    else if (findAndStep(source, "Start"))
        ctl = LFOINSERT::control::start;
    else if (findAndStep(source, "Delay"))
        ctl = LFOINSERT::control::delay;
    else if (findAndStep(source, "Amp Random")||findAndStep(source, "AmpRand"))
        ctl = LFOINSERT::control::amplitudeRandomness;
    else if (findAndStep(source, "Stretch"))
        ctl = LFOINSERT::control::stretch;

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "lfo overflow >" << source << endl;
}


void TextData::encodeEnvelope(string& source, CommandBlock& allData)
{
    uchar ctl = UNUSED;
    allData.data.insert = TOPLEVEL::insert::envelopeGroup;
    // this might be changed for freemode points

    if (findAndStep(source, "Attack Level") || findAndStep(source, "A val"))
        ctl = ENVELOPEINSERT::control::attackLevel;
    else if (findAndStep(source, "Attack Time") || findAndStep(source, "A dt"))
        ctl = ENVELOPEINSERT::control::attackTime;
    else if (findAndStep(source, "Decay Level") || findAndStep(source, "D val"))
        ctl = ENVELOPEINSERT::control::decayLevel;
    else if (findAndStep(source, "Decay Time") || findAndStep(source, "D dt"))
        ctl = ENVELOPEINSERT::control::decayTime;
    else if (findAndStep(source, "Sustain Level") || findAndStep(source, "S val"))
        ctl = ENVELOPEINSERT::control::sustainLevel;
    else if (findAndStep(source, "Release Level") || findAndStep(source, "R val"))
        ctl = ENVELOPEINSERT::control::releaseLevel;
    else if (findAndStep(source, "Release Time") || findAndStep(source, "R dt"))
        ctl =ENVELOPEINSERT::control::releaseTime;
    else if (findAndStep(source, "Stretch"))
        ctl = ENVELOPEINSERT::control::stretch;

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "envelope overflow >" << source << endl;
}


void TextData::encodeFilter(string& source, CommandBlock& allData)
{
    uchar ctl = UNUSED;
    allData.data.insert = TOPLEVEL::insert::filterGroup;

    if (findAndStep(source, "C_Freq") || findAndStep(source, "C Freq") || findAndStep(source, "Cent Freq"))
        ctl = FILTERINSERT::control::centerFrequency;
    else if (findAndStep(source, "Q"))
        ctl = FILTERINSERT::control::Q;
    else if (findAndStep(source, "VsensA") || findAndStep(source, "Velocity Sense"))
        ctl = FILTERINSERT::control::velocitySensitivity;
    else if (findAndStep(source, "Vsens") || findAndStep(source, "Velocity Sense Curve"))
        ctl = FILTERINSERT::control::velocityCurve;
    else if (findAndStep(source, "ain")) // missing G/g deliberate
        ctl = FILTERINSERT::control::gain;
    else if (findAndStep(source, "Freq Track") || findAndStep(source, "FreqTrk"))
        ctl = FILTERINSERT::control::frequencyTracking;

    else if (findAndStep(source, "Form"))
    {
        if (findAndStep(source, "Morph") || findAndStep(source, "Fr Sl"))
            ctl = FILTERINSERT::control::formantSlowness;
        else if (findAndStep(source, "Lucidity") || findAndStep(source, "Vw Cl"))
            ctl = FILTERINSERT::control::formantClearness;
        else if (findAndStep(source, "Stretch"))
            ctl = FILTERINSERT::control::formantStretch;
        else if (findAndStep(source, "Cent Freq"))
            ctl = FILTERINSERT::control::formantCenter;
        else if (findAndStep(source, "Octave"))
            ctl = FILTERINSERT::control::formantOctave;
    }

    else if (findAndStep(source, "Vowel"))
    {
        uchar Vnum = UNUSED - 1; // special cases
        uchar Fnum = UNUSED - 1; // actually have printed zeros
        if (findCharNum(source, Vnum))
            allData.data.offset = Vnum + 1;
        else
        {
            log(source, "no vowel number");
            return;
        }
        if (findAndStep(source, "Formant"))
        {
            if (findCharNum(source, Fnum))
                allData.data.parameter = Fnum + 1;
            else
            {
                log(source, "no formant number");
                return;
            }
            if (findAndStep(source, "Form Freq"))
                ctl = FILTERINSERT::control::formantFrequency;
            else if (findAndStep(source, "Form Q"))
                ctl = FILTERINSERT::control::formantQ;
            else if (findAndStep(source, "Form Amp"))
                ctl = FILTERINSERT::control::formantAmplitude;
            }
    }

    if (ctl < UNUSED)
    {
        allData.data.control = ctl;
        return;
    }

    allData.data.control = TOPLEVEL::control::unrecognised;
    allData.data.source = TOPLEVEL::action::noAction;
    cout << "filter overflow >" << source << endl;
}
