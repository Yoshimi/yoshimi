/*
    CmdInterpreter.cpp

    Copyright 2019 - 2021, Will Godfrey and others.

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

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <cstdio>
#include <cerrno>
#include <cfloat>
#include <sys/time.h>
#include <sys/types.h>
#include <ncurses.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <algorithm>
#include <iterator>
#include <map>
#include <list>
#include <vector>
#include <sstream>
#include <cassert>

#include "CLI/CmdInterpreter.h"
#include "Effects/EffectMgr.h"
#include "CLI/Parser.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/NumericFuncs.h"
#include "Misc/FormatFuncs.h"
#include "Misc/CliFuncs.h"


// global variable; see SynthEngine.cpp and main.cpp
extern SynthEngine *firstSynth;

// these two are both zero and repesented by an enum entry
const unsigned char type_read = TOPLEVEL::type::Adjust;

namespace cli {

using std::string;
using std::vector;
using std::list;

using file::loadText;

using func::bitSet;
using func::bitTest;
using func::bitClear;
using func::bitClearHigh;
using func::bitFindHigh;

using func::asString;
using func::string2int;
using func::string2int127;
using func::string2float;


/*
 * There are two routes that 'write' commands can take.
 * sendDirect(synth, ) and sendNormal(synth, )
 *
 * sendDirect(synth, ) is the older form and is now mostly used for
 * numerical entry by test calls. It always returns zero.
 *
 * sendNormal(synth, ) performs 'value' range adjustment and also
 * performs some error checks, returning a response.
 *
 *
 * readControl(synth, ) provides a non-buffered way to find the
 * value of any control. It may be temporarily blocked if
 * there is a write command in progress.
 *
 * readControlText(synth, ) provides a non-buffered way to fetch
 * some text items. It is not error checked.
 */


// predefined OK-Reply constant
Reply Reply::DONE{REPLY::done_msg};


CmdInterpreter::CmdInterpreter() :
    currentInstance{0},
    synth{nullptr},
    instrumentGroup{},
    textMsgBuffer{TextMsgBuffer::instance()},

    context{LEVEL::Top},
    npart{0},
    kitMode{PART::kitType::Off},
    kitNumber{0},
    inKitEditor{false},
    voiceNumber{0},
    insertType{0},
    nFXtype{0},
    nFXpreset{0},
    nFXeqBand{0},
    nFX{0},
    filterSequenceSize{1},
    filterVowelNumber{0},
    filterNumberOfFormants{1},
    filterFormantNumber{0},
    chan{0},
    axis{0},
    mline{0}
{
}

void CmdInterpreter::defaults()
{
    context = LEVEL::Top;
    npart = 0;
    kitMode = PART::kitType::Off;
    kitNumber = 0;
    inKitEditor = false;
    voiceNumber = 0;
    insertType = 0;
    nFXtype = 0;
    nFXpreset = 0;
    nFXeqBand = 0;
    nFX = 0;
    filterVowelNumber = 0;
    filterFormantNumber = 0;
    chan = 0;
    axis = 0;
    mline = 0;
}


void CmdInterpreter::resetInstance(unsigned int newInstance)
{
    currentInstance = newInstance;
    synth = firstSynth->getSynthFromId(currentInstance);
    unsigned int newID = synth->getUniqueId();
    if (newID != currentInstance)
    {
        synth->getRuntime().Log("Instance " + std::to_string(currentInstance) + " not found. Set to " + std::to_string(newID), 1);
        currentInstance = newID;
    }
    defaults();
}


string CmdInterpreter::buildStatus(bool showPartDetails)
{
    if (bitTest(context, LEVEL::AllFX))
    {
        return buildAllFXStatus();
    }
    if (bitTest(context, LEVEL::Part))
    {
        return buildPartStatus(showPartDetails);
    }

    string result = "";

    if (bitTest(context, LEVEL::Scale))
        result += " Scale ";
    else if (bitTest(context, LEVEL::Bank))
    {
        result += " Bank " + to_string(int(readControl(synth, 0, BANK::control::selectBank, TOPLEVEL::section::bank))) + " (root " + to_string(int(readControl(synth, 0, BANK::control::selectRoot, TOPLEVEL::section::bank))) + ")";
    }
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



string CmdInterpreter::buildAllFXStatus()
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


string CmdInterpreter::buildPartStatus(bool showPartDetails)
{
    assert(bitTest(context, LEVEL::Part));

    int kit = UNUSED;
    int insert = UNUSED;
    bool justPart = false;
    string result = " p";

    npart = readControl(synth, 0, MAIN::control::partNumber, TOPLEVEL::section::main);

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
            if (readControl(synth, 0, PART::control::enableKitLine, npart, kitNumber, UNUSED, TOPLEVEL::insert::kitGroup))
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
            if (readControl(synth, 0, PART::control::enableAdd, npart, kit, PART::engine::addSynth, insert))
                result += "+";
            break;
        case PART::engine::subSynth:
            if (bitFindHigh(context) == LEVEL::SubSynth)
                result += ", Sub";
            else
                result += ", S";
            if (readControl(synth, 0, PART::control::enableSub, npart, kit, PART::engine::subSynth, insert))
                result += "+";
            break;
        case PART::engine::padSynth:
            if (bitFindHigh(context) == LEVEL::PadSynth)
                result += ", Pad";
            else
                result += ", P";
            if (readControl(synth, 0, PART::control::enablePad, npart, kit, PART::engine::padSynth, insert))
                result += "+";
            break;
        case PART::engine::addVoice1: // intentional drop through
        case PART::engine::addMod1:
        {
            result += ", A";
            if (readControl(synth, 0, PART::control::enableAdd, npart, kit, PART::engine::addSynth, insert))
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
                    string word = addmodnameslist[tmp];

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
    {
        int type = (int)readControl(synth, 0, OSCILLATOR::control::baseFunctionType, npart, kitNumber, engine + voiceNumber, TOPLEVEL::insert::oscillatorGroup);
        if (type > OSCILLATOR::wave::hyperSec)
            result += " user";
        else
            result += (" " + waveshape[type]);
    }

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
        int baseType = readControl(synth, 0, FILTERINSERT::control::baseType, npart, kitNumber, engine + voiceNumber, TOPLEVEL::insert::filterGroup);
        result += ", Filter ";
        switch (baseType)
        {
            case 0:
                result += "analog";
                break;
            case 1:
                filterSequenceSize = readControl(synth, 0, FILTERINSERT::control::sequenceSize, npart, kitNumber, engine + voiceNumber, TOPLEVEL::insert::filterGroup);
                filterNumberOfFormants = readControl(synth, 0, FILTERINSERT::control::numberOfFormants, npart, kitNumber, engine + voiceNumber, TOPLEVEL::insert::filterGroup);
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
                if (engine == PART::engine::addMod1)
                    cmd = ADDVOICE::control::enableModulatorAmplitudeEnvelope;
                else
                    cmd = ADDVOICE::control::enableAmplitudeEnvelope;
                result += "amp";
                break;
            case TOPLEVEL::insertType::frequency:
                if (engine == PART::engine::addMod1)
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
    synth->getRuntime().Log(text);
    // changed this so that all messages go to same destination.
    //line = readline(text.c_str());
    line = readline("");
    if (line)
    {
        if (line[0] != 0)
            result = line[0];
        free(line);
        line = NULL;
    }
    return (((result | 32) == test) ^ priority);
}


void CmdInterpreter::helpLoop(list<string>& msg, string *commands, int indent, bool single)
{
    int word = 0;
    int spaces = 30 - indent;
    string left = "";
    string right = "";
    string dent;
    string blanks;

    while (commands[word] != "@end")
    {
        left = commands[word];
        if (!single)
            right = commands[word + 1];
        if (left == "")
        {
            left = "  " + right;
            right = "";
        }
        if (right > "")
            left = left +(blanks.assign(spaces - left.length(), ' ') + right);
        msg.push_back(dent.assign(indent, ' ') + left);
        word += (2 - single);
    }
}


char CmdInterpreter::helpList(Parser& input, unsigned int local)
{
    if (!input.matchnMove(1, "help") && !input.matchnMove(1, "?"))
        return REPLY::todo_msg;

    int listnum = -1;
    bool named = false;

    if (!input.isAtEnd())
    { // 1 & 2 reserved for syseff & inseff
        if (input.matchnMove(3, "effects"))
            listnum = LISTS::eff;
        else if (input.matchnMove(3, "reverb"))
            listnum = LISTS::reverb;
        else if (input.matchnMove(3, "echo"))
            listnum = LISTS::echo;
        else if (input.matchnMove(3, "chorus"))
            listnum = LISTS::chorus;
        else if (input.matchnMove(3, "phaser"))
            listnum = LISTS::phaser;
        else if (input.matchnMove(3, "alienwah"))
            listnum = LISTS::alienwah;
        else if (input.matchnMove(3, "distortion"))
            listnum = LISTS::distortion;
        else if (input.matchnMove(2, "eq"))
            listnum = LISTS::eq;
        else if (input.matchnMove(3, "dynfilter"))
            listnum = LISTS::dynfilter;

        else if (input.matchnMove(1, "part"))
            listnum = LISTS::part;
        else if (input.matchnMove(2, "mcontrol"))
            listnum = LISTS::mcontrol;
        else if (input.matchnMove(3, "common"))
            listnum = LISTS::common;
        else if (input.matchnMove(3, "addsynth"))
            listnum = LISTS::addsynth;
        else if (input.matchnMove(3, "subsynth"))
            listnum = LISTS::subsynth;
        else if (input.matchnMove(3, "padsynth"))
            listnum = LISTS::padsynth;
        else if (input.matchnMove(3, "resonance"))
            listnum = LISTS::resonance;
        else if (input.matchnMove(3, "voice"))
            listnum = LISTS::addvoice;
        else if (input.matchnMove(3, "modulator"))
            listnum = LISTS::addmod;
        else if (input.matchnMove(3, "waveform"))
            listnum = LISTS::waveform;
        else if (input.matchnMove(3, "lfo"))
            listnum = LISTS::lfo;
        else if (input.matchnMove(3, "filter"))
            listnum = LISTS::filter;
        else if (input.matchnMove(3, "envelope"))
            listnum = LISTS::envelope;

        else if (input.matchnMove(1, "vector"))
            listnum = LISTS::vector;
        else if (input.matchnMove(1, "scale"))
            listnum = LISTS::scale;
        else if (input.matchnMove(1, "load"))
            listnum = LISTS::load;
        else if (input.matchnMove(1, "save"))
            listnum = LISTS::save;
        else if (input.matchnMove(1, "list"))
            listnum = LISTS::list;
        else if (input.matchnMove(1, "config"))
            listnum = LISTS::config;
        else if (input.matchnMove(1, "bank"))
            listnum = LISTS::bank;
        else if (input.matchnMove(1, "mlearn"))
            listnum = LISTS::mlearn;
        if (listnum != -1)
            named = true;
    }
    else
    {
        if (bitTest(local, LEVEL::AllFX))
        {
            switch (nFXtype)
            {
                case 0:
                    listnum = LISTS::eff;
                    break;
                case 1:
                    listnum = LISTS::reverb;
                    break;
                case 2:
                    listnum = LISTS::echo;
                    break;
                case 3:
                    listnum = LISTS::chorus;
                    break;
                case 4:
                    listnum = LISTS::phaser;
                    break;
                case 5:
                    listnum = LISTS::alienwah;
                    break;
                case 6:
                    listnum = LISTS::distortion;
                    break;
                case 7:
                    listnum = LISTS::eq;
                    break;
                case 8:
                    listnum = LISTS::dynfilter;
                    break;
            }
        }
        else if (bitTest(local, LEVEL::Envelope))
            listnum = LISTS::envelope;
        else if (bitTest(local, LEVEL::LFO))
            listnum = LISTS::lfo;
        else if (bitTest(local, LEVEL::Filter))
            listnum = LISTS::filter;
        else if (bitTest(local, LEVEL::Oscillator))
            listnum = LISTS::waveform;
        else if (bitTest(local, LEVEL::AddMod))
            listnum = LISTS::addmod;
        else if (bitTest(local, LEVEL::AddVoice))
            listnum = LISTS::addvoice;
        else if (bitTest(local, LEVEL::Resonance))
            listnum = LISTS::resonance;
        else if (bitTest(local, LEVEL::AddSynth))
            listnum = LISTS::addsynth;
        else if (bitTest(local, LEVEL::SubSynth))
            listnum = LISTS::subsynth;
        else if (bitTest(local, LEVEL::PadSynth))
            listnum = LISTS::padsynth;
        else if (bitTest(local, LEVEL::MControl))
            listnum = LISTS::mcontrol;

        else if (bitTest(local, LEVEL::Part))
            listnum = LISTS::part;
        else if (bitTest(local, LEVEL::Vector))
            listnum = LISTS::vector;
        else if (bitTest(local, LEVEL::Scale))
            listnum = LISTS::scale;
        else if (bitTest(local, LEVEL::Bank))
            listnum = LISTS::bank;
        else if (bitTest(local, LEVEL::Config))
            listnum = LISTS::config;
        else if (bitTest(local, LEVEL::Learn))
            listnum = LISTS::mlearn;
    }
    if (listnum == -1)
        listnum = LISTS::all;
    list<string>msg;
    if (!named)
    {
        msg.push_back("Commands:");
        helpLoop(msg, basics, 2);
    }
    switch(listnum)
    {
        case 0:
            msg.push_back(" ");
            msg.push_back("  Part [n1]   ...             - part operations");
            msg.push_back("  VEctor [n1] ...             - vector operations");
            msg.push_back("  SCale       ...             - scale (microtonal) operations");
            msg.push_back("  MLearn [n1] ...             - MIDI learn operations");
            msg.push_back("  COnfig      ...             - configuration settings");
            msg.push_back("  BAnk        ...             - root and bank settings");
            msg.push_back("  LIst        ...             - various available parameters");
            msg.push_back("  LOad        ...             - load various files");
            msg.push_back("  SAve        ...             - save various files");

            msg.push_back(" ");
            break;
        case LISTS::part:
            msg.push_back("Part: [n1] = part number");
            helpLoop(msg, partlist, 2);
            break;
        case LISTS::mcontrol:
            msg.push_back("Midi Control:");
            helpLoop(msg, mcontrollist, 2);
            break;
        case LISTS::common:
            msg.push_back("Part Common:");
            helpLoop(msg, commonlist, 2);
            break;
        case LISTS::addsynth:
            msg.push_back("Part AddSynth:");
            helpLoop(msg, addsynthlist, 2);
            break;
        case LISTS::subsynth:
            msg.push_back("Part SubSynth:");
            helpLoop(msg, subsynthlist, 2);
            break;
        case LISTS::padsynth:
            msg.push_back("Part PadSynth:");
            helpLoop(msg, padsynthlist, 2);
            break;
        case LISTS::resonance:
            msg.push_back("Resonance:");
            helpLoop(msg, resonancelist, 2);
            break;
        case LISTS::addvoice:
            msg.push_back("Part AddVoice:");
            helpLoop(msg, addvoicelist, 2);
            break;
        case LISTS::addmod:
            msg.push_back("AddVoice Modulator:");
            helpLoop(msg, addmodlist, 2);
            break;
        case LISTS::waveform:
            msg.push_back("Part Waveform:");
            helpLoop(msg, waveformlist, 2);
            break;

        case LISTS::lfo:
            msg.push_back("Engine LFOs:");
            helpLoop(msg, LFOlist, 2);
            break;
        case LISTS::filter:
            msg.push_back("Engine Filters:");
            helpLoop(msg, filterlist, 2);
            break;
        case LISTS::envelope:
            msg.push_back("Engine Envelopes:");
            helpLoop(msg, envelopelist, 2);
            break;

        case LISTS::eff:
            msg.push_back("Effects:");
            helpLoop(msg, fx_list, 2, true);
            break;
        case LISTS::reverb:
            msg.push_back("Reverb:");
            helpLoop(msg, reverblist, 2);
            break;
        case LISTS::echo:
            msg.push_back("Echo:");
            helpLoop(msg, echolist, 2);
            break;
        case LISTS::chorus:
            msg.push_back("Chorus:");
            helpLoop(msg, choruslist, 2);
            break;
        case LISTS::phaser:
            msg.push_back("Phaser:");
            helpLoop(msg, phaserlist, 2);
            break;
        case LISTS::alienwah:
            msg.push_back("Alienwah:");
            helpLoop(msg, alienwahlist, 2);
            break;
        case LISTS::distortion:
            msg.push_back("Distortion:");
            helpLoop(msg, distortionlist, 2);
            break;
        case LISTS::eq:
            msg.push_back("EQ:");
            helpLoop(msg, eqlist, 2);
            break;
        case LISTS::dynfilter:
            msg.push_back("Dynfilter:");
            helpLoop(msg, dynfilterlist, 2);
            break;

        case LISTS::vector:
            msg.push_back("Vector:");
            helpLoop(msg, vectlist, 2);
            break;
        case LISTS::scale:
            msg.push_back("Scale:");
            helpLoop(msg, scalelist, 2);
            break;
        case LISTS::load:
            msg.push_back("Load:");
            helpLoop(msg, loadlist, 2);
            break;
        case LISTS::save:
            msg.push_back("Save:");
            helpLoop(msg, savelist, 2);
            break;
        case LISTS::list:
            msg.push_back("List:");
            helpLoop(msg, listlist, 2);
            break;
        case LISTS::bank:
            msg.push_back("Bank:");
            helpLoop(msg, banklist, 2);
            break;
        case LISTS::config:
            msg.push_back("Config:");
            helpLoop(msg, configlist, 2);
            msg.push_back("'*' entries need to be saved and Yoshimi restarted to activate");
            break;
        case LISTS::mlearn:
            msg.push_back("Mlearn:");
            helpLoop(msg, learnlist, 2);
            break;
    }

    if (listnum == LISTS::all)
    {
        helpLoop(msg, toplist, 2);
        msg.push_back("'...' is a help sub-menu");
    }

    if (synth->getRuntime().toConsole)
        // we need this in case someone is working headless
        std::cout << "\nSet CONfig REPorts [s] - set report destination (gui/stderr)\n";

    synth->cliOutput(msg, LINES);
    return REPLY::exit_msg;
}


void CmdInterpreter::historyList(int listnum)
{
    list<string>msg;
    int start = TOPLEVEL::XML::Instrument;
    int end = TOPLEVEL::XML::MLearn;
    bool found = false;

    if (listnum >= 0) // its a single list we want
    {
        start = listnum;
        end = listnum;
    }
    for (int type = start; type <= end; ++type)
    {
        vector<string> listType = *synth->getHistory(type);
        if (listType.size() > 0)
        {
            msg.push_back(" ");
            switch (type)
            {
                case TOPLEVEL::XML::Instrument:
                    msg.push_back("Recent Instruments:");
                    break;
                case TOPLEVEL::XML::Patch:
                    msg.push_back("Recent Patch Sets:");
                    break;
                case TOPLEVEL::XML::Scale:
                    msg.push_back("Recent Scales:");
                    break;
                case TOPLEVEL::XML::State:
                    msg.push_back("Recent States:");
                    break;
                case TOPLEVEL::XML::Vector:
                    msg.push_back("Recent Vectors:");
                    break;
                case TOPLEVEL::XML::MLearn:
                    msg.push_back("Recent MIDI learned:");
                    break;
            }
            int itemNo = 0;
            for (vector<string>::iterator it = listType.begin(); it != listType.end(); ++it, ++ itemNo)
                msg.push_back(std::to_string(itemNo + 1) + "  " + *it);
            found = true;
        }
    }
    if (!found)
        msg.push_back("\nNo Saved History");

    synth->cliOutput(msg, LINES);
}


string CmdInterpreter::historySelect(int listnum, int selection)
{
    vector<string> listType = *synth->getHistory(listnum - 1);
    if (listType.size()== 0)
    {
        synth->getRuntime().Log("No saved entries");
        return "";
    }
    else
    {
        vector<string>::iterator it = listType.begin();
        int itemNo = 0;
        while (it != listType.end() && itemNo != selection)
        {
            ++ it;
            ++ itemNo;
        }
        if (it != listType.end())
            return *it;
    }
    synth->getRuntime().Log("No such entry");
    return "";
}


int CmdInterpreter::effectsList(Parser& input, bool presets)
{
    list<string>msg;

    size_t presetsPos;
    size_t presetsLast;
    int presetsCount;
    string blanks;
    string left;
    bool all;

    if (bitTest(context, LEVEL::AllFX) && presets == true)
    {
         synth->getRuntime().Log("Type " + fx_list[nFXtype] + "\nPresets -" + fx_presets[nFXtype].substr(fx_presets[nFXtype].find(',') + 1));
         return REPLY::done_msg;
    }
    else if (presets)
    {
        synth->getRuntime().Log("No effect selected");
        return REPLY::done_msg;
    }
    else
        all = input.matchnMove(1, "all");
    if (!all)
        msg.push_back("  effect     presets");
    for (int i = 0; i < 9; ++ i)
    {
        presetsPos = 1;
        presetsLast = fx_presets [i].find(',') + 1; // skip over count
        presetsCount = 0;
        if (all)
        {
            msg.push_back("  " + fx_list[i]);
            msg.push_back("    presets");
            while (presetsPos != string::npos)
            {
                presetsPos = fx_presets [i].find(',', presetsLast);
                msg.push_back("      " + asString(presetsCount + 1) + " =" + fx_presets [i].substr(presetsLast, presetsPos - presetsLast));
                presetsLast = presetsPos + 1;
                ++ presetsCount;
            }
        }
        else
        {
            left = fx_list[i];
            msg.push_back("    " + left + blanks.assign(12 - left.length(), ' ') + fx_presets [i].substr(0, presetsLast - 1));
        }
    }

    synth->cliOutput(msg, LINES);
    return REPLY::done_msg;
}


int CmdInterpreter::effects(Parser& input, unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    int nFXavail;
    int par = nFX;
    int value;
    string dest = "";

    if (bitTest(context, LEVEL::Part))
    {
        nFXavail = NUM_PART_EFX;
        nFX = readControl(synth, 0, PART::control::effectNumber, npart, UNUSED, UNUSED, TOPLEVEL::insert::partEffectSelect);
        nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
    }
    else if (bitTest(context, LEVEL::InsFX))
    {
        nFXavail = NUM_INS_EFX;
        nFX = readControl(synth, 0, EFFECT::sysIns::effectNumber, TOPLEVEL::section::insertEffects);
        nFXtype = synth->insefx[nFX]->geteffect();
    }
    else
    {
        nFXavail = NUM_SYS_EFX;
        nFX = readControl(synth, 0, EFFECT::sysIns::effectNumber, TOPLEVEL::section::systemEffects);
        nFXtype = synth->sysefx[nFX]->geteffect();
        int tmp = input.toggle();
        if (tmp >= 0)
            return sendNormal(synth, 0, tmp, controlType, EFFECT::sysIns::effectEnable, TOPLEVEL::section::systemEffects, UNUSED, nFX);
    }

    if (input.lineEnd(controlType))
    {
        if (bitTest(context, LEVEL::Part))
            dest = "Part" + to_string(int(npart + 1));
        else if (bitTest(context, LEVEL::InsFX))
            dest = "Insert";
        else
            dest = "System";
        Runtime.Log(dest + " effect " + asString(nFX + 1));
        return REPLY::done_msg;
    }

    value = string2int(input);

    if (value > 0)
    {
        value -= 1;
        input.skipChars();
        if (value >= nFXavail)
            return REPLY::range_msg;

        if (value != nFX)
        { // partially updates GUI
            nFX = value;
            if (bitTest(context, LEVEL::Part))
            {
                sendNormal(synth, 0, nFX, TOPLEVEL::type::Write, PART::control::effectNumber, npart, UNUSED, nFX, TOPLEVEL::insert::partEffectSelect);
                nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
                return sendNormal(synth, 0, nFXtype, TOPLEVEL::type::Write, PART::control::effectType, npart, UNUSED, nFX, TOPLEVEL::insert::partEffectSelect);
            }
            if (bitTest(context, LEVEL::InsFX))
            {
                sendNormal(synth, 0, nFX, TOPLEVEL::type::Write, EFFECT::sysIns::effectNumber, TOPLEVEL::section::insertEffects, UNUSED, nFX);

                nFXtype = synth->insefx[nFX]->geteffect();
                return sendNormal(synth, 0, nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::insertEffects, UNUSED, nFX);
            }

                sendNormal(synth, 0, nFX, TOPLEVEL::type::Write, EFFECT::sysIns::effectNumber, TOPLEVEL::section::systemEffects, UNUSED, nFX);

                nFXtype = synth->sysefx[nFX]->geteffect();
                return sendNormal(synth, 0, nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::systemEffects, UNUSED, nFX);
        }
        if (input.lineEnd(controlType))
        {
            Runtime.Log("efx number set to " + asString(nFX + 1));
            return REPLY::done_msg;
        }
    }

    bool effType = false;
    for (int i = 0; i < 9; ++ i)
    {
        //Runtime.Log("command " + string{input} + "  list " + fx_list[i]);
        if (input.matchnMove(2, fx_list[i].c_str()))
        {
            nFXtype = i;
            effType = true;
            break;
        }
    }
    if (effType)
    {
        //std::cout << "nfx " << nFX << std::endl;
        nFXpreset = 0; // always set this on type change
        if (bitTest(context, LEVEL::Part))
        {
            sendDirect(synth, 0, nFXtype, TOPLEVEL::type::Write, PART::control::effectType, npart, UNUSED, nFX);
            return REPLY::done_msg; // TODO find out why not sendNormal
        }
        else if (bitTest(context, LEVEL::InsFX))
            return sendNormal(synth, 0, nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::insertEffects, UNUSED, nFX);
        else
            return sendNormal(synth, 0, nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::systemEffects, UNUSED, nFX);
    }

    if (nFXtype > 0)
    {
        int selected = -1;
        int value = -1;
        string name = string{input}.substr(0, 3);
        /*
         * We can't do a skipChars here as we don't yet know
         * if 'selected' will be valid. For some controls we
         * need to do an on-the-spot skip, otherwise we do so
         * at the end when we know we have a valid result but
         * 'value' has not been set.
         * If it's not valid we don't block, but pass on to
         * other command tests routines.
         */
        if (controlType == type_read)
            value = 1; // dummy value
        switch (nFXtype)
        {
            case 1:
            {
                selected = stringNumInList(name, effreverb, 3);
                if (selected != 7) // EQ
                    nFXeqBand = 0;
                if (selected == 10 && value == -1) // type
                {
                    input.skipChars();
                    if (input.matchnMove(1, "random"))
                        value = 0;
                    else if (input.matchnMove(1, "freeverb"))
                        value = 1;
                    else if (input.matchnMove(1, "bandwidth"))
                        value = 2;
                    else
                        return REPLY::value_msg;
                }
                break;
            }
            case 2:
                selected = stringNumInList(name, effecho, 3);
                break;
            case 3:
            {
                selected = stringNumInList(name, effchorus, 3);
                if (selected == 4 && value == -1) // filtershape
                {
                    input.skipChars();
                    if (input.matchnMove(1, "sine"))
                        value = 0;
                    else if (input.matchnMove(1, "triangle"))
                        value = 1;
                    else return REPLY::value_msg;
                }
                else if (selected == 11) // subtract
                {
                    input.skipChars();
                    value = (input.toggle() == 1);
                }
                break;
            }
            case 4:
            {
                selected = stringNumInList(name, effphaser, 3);
                if (selected == 4 && value == -1) // filtershape
                {
                    input.skipChars();
                    if (input.matchnMove(1, "sine"))
                        value = 0;
                    else if (input.matchnMove(1, "triangle"))
                        value = 1;
                    else return REPLY::value_msg;
                }
                else if (selected == 10 || selected == 12 || selected == 14) // LFO, SUB, ANA
                {
                    input.skipChars();
                    value = (input.toggle() == 1);
                }
                break;
            }
            case 5:
            {
                selected = stringNumInList(name, effalienwah, 3);
                if (selected == 4 && value == -1) // filtershape
                {
                    input.skipChars();
                    if (input.matchnMove(1, "sine"))
                        value = 0;
                    else if (input.matchnMove(1, "triangle"))
                        value = 1;
                    else return REPLY::value_msg;
                }
                break;
            }
            case 6:
            {
                selected = stringNumInList(name, effdistortion, 3);
                if (selected == 5 && value == -1) // filtershape
                {
                    input.skipChars();
                    string name = string{input}.substr(0,3);
                    value = stringNumInList(name, filtershapes, 3) - 1;
                    if (value < 0)
                        return REPLY::value_msg;
                }
                else if (selected == 6 || selected == 9 || selected == 10) // invert, stereo, prefilter
                {
                    input.skipChars();
                    value = (input.toggle() == 1);
                }
                break;
            }
            case 7: // TODO band and type no GUI update
            {
                selected = stringNumInList(name, effeq, 2);
                if (selected == 1) // band
                {
                    if (controlType == TOPLEVEL::type::Write)
                    {
                        input.skipChars();
                        value = string2int(input);
                        if (value < 0 || value >= MAX_EQ_BANDS)
                            return REPLY::range_msg;
                        nFXeqBand = value;
                    }
                }
                else if (selected == 2 && value == -1) // type
                {
                    input.skipChars();
                    string name = string{input}.substr(0,3);
                    value = stringNumInList(name, eqtypes, 3);
                    if (value < 0)
                        return REPLY::value_msg;
                }

                if (selected > 1)
                {
                    selected += 8;
                }
                break;
            }
            case 8:
            {
                selected = stringNumInList(name, effdynamicfilter, 3);
                if (selected == 4 && value == -1) // filtershape
                {
                    input.skipChars();
                    if (input.matchnMove(1, "sine"))
                        value = 0;
                    else if (input.matchnMove(1, "triangle"))
                        value = 1;
                    else return REPLY::value_msg;
                }
                else if (selected == 8) // invert
                {
                    input.skipChars();
                    value = (input.toggle() == 1);
                }
                else if (selected == 10) // filter entry
                {
                    bitSet(context, LEVEL::Filter);
                    return REPLY::done_msg;
                }
            }
        }
        if (selected > -1)
        {
            if (value == -1)
            {
                input.skipChars();
                value = string2int(input);
            }
            //std::cout << "Val " << value << "  type " << controlType << "  cont " << selected << "  part " << context << "  efftype " << int(nFXtype) << "  num " << int(nFX) << std::endl;
            if (bitTest(context, LEVEL::Part))
                return sendNormal(synth, 0, value, controlType, selected, npart, EFFECT::type::none + nFXtype, nFX);
            else if (bitTest(context, LEVEL::InsFX))
                return sendNormal(synth, 0, value, controlType, selected, TOPLEVEL::section::insertEffects, EFFECT::type::none + nFXtype, nFX);
            else
                return sendNormal(synth, 0, value, controlType, selected, TOPLEVEL::section::systemEffects, EFFECT::type::none + nFXtype, nFX);
        }
        // Continue cos it's not for us.
    }

    if (input.matchnMove(2, "send"))
    {
        bool isWrite = (controlType == TOPLEVEL::type::Write);
        if (input.lineEnd(controlType))
            return REPLY::parameter_msg;

        if (!bitTest(context, LEVEL::InsFX))
        {
            par = string2int(input) - 1;
            input.skipChars();
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int127(input);
        }
        else if (isWrite) // system effects
        {
            if (input.matchnMove(1, "master"))
                value = -2;
            else if (input.matchnMove(1, "off"))
                value = -1;
            else
            {
                value = string2int(input) - 1;
                if (value >= Runtime.NumAvailableParts || value < 0)
                    return REPLY::range_msg;
            }
        }

        if (!isWrite)
            value = 1; // dummy
        int control;
        int partno;
        int engine = nFX;
        int insert = UNUSED;

        if (bitTest(context, LEVEL::Part))
        {
            partno = npart;
            control = PART::control::partToSystemEffect1 + par;
            engine = UNUSED;
        }
        else if (bitTest(context, LEVEL::InsFX))
        {
            partno = TOPLEVEL::section::insertEffects;
            control = EFFECT::sysIns::effectDestination;
        }
        else
        {
            if (par <= nFX || par >= NUM_SYS_EFX)
                return REPLY::range_msg;
            partno = TOPLEVEL::section::systemEffects;
            control = EFFECT::sysIns::toEffect1 + par - 1; // TODO this needs sorting
            engine = nFX;
            insert = TOPLEVEL::insert::systemEffectSend;
        }
        return sendNormal(synth, 0, value, controlType, control, partno, UNUSED, engine, insert);
    }

    if (input.matchnMove(3, "preset"))
    {
        /*
         * Using constant strings and bedding the number into the list
         * of presets provides a very simple way to keep track of a
         * moving target with minimal code and data space.
         * However, all of this should really be in src/Effects
         * not here *and* in the gui code!
         */
        int partno;
        nFXpreset = string2int127(input) - 1;
        if (bitTest(context, LEVEL::Part))
            partno = npart;
        else if (bitTest(context, LEVEL::InsFX))
            partno = TOPLEVEL::section::insertEffects;
        else
            partno = TOPLEVEL::section::systemEffects;
        return sendNormal(synth, 0, nFXpreset, controlType, 16, partno,  EFFECT::type::none + nFXtype, nFX);
    }
    return REPLY::op_msg;
}


int CmdInterpreter::midiControllers(Parser& input, unsigned char controlType)
{
    if (input.isAtEnd())
        return REPLY::done_msg;
    int value = -1;
    int cmd = -1;
    bool isWrite = (controlType == TOPLEVEL::type::Write);

    if (input.matchnMove(2, "volume"))
    {
        value = !(input.toggle() == 0);
        cmd = PART::control::volumeEnable;
    }
    if ((cmd == -1) && input.matchnMove(2, "VRange"))
    {
        value = string2int127(input);
        cmd = PART::control::volumeRange;
    }
    if ((cmd == -1) && input.matchnMove(2, "pan"))
    {
        value = string2int127(input);
        cmd = PART::control::panningWidth;
    }
    if ((cmd == -1) && input.matchnMove(2, "modwheel"))
    {
        value = (input.toggle() == 1);
        cmd = PART::control::exponentialModWheel;
    }
    if ((cmd == -1) && input.matchnMove(2, "mrange"))
    {
        value = string2int127(input);
        cmd = PART::control::modWheelDepth;
    }
    if ((cmd == -1) && input.matchnMove(2, "expression"))
    {
        value = !(input.toggle() == 0);
        cmd = PART::control::expressionEnable;
    }
    if ((cmd == -1) && input.matchnMove(2, "sustain"))
    {
        value = !(input.toggle() == 0);
        cmd = PART::control::sustainPedalEnable;
    }
    if ((cmd == -1) && input.matchnMove(2, "pwheel"))
    {
        value = string2int(input);
        cmd = PART::control::pitchWheelRange;
    }
    if ((cmd == -1) && input.matchnMove(2, "breath"))
    {
        value = !(input.toggle() == 0);
        cmd = PART::control::breathControlEnable;
    }
    if ((cmd == -1) && input.matchnMove(2, "cutoff"))
    {
        value = string2int127(input);
        cmd = PART::control::filterCutoffDepth;
    }
    if ((cmd == -1) && input.matchnMove(2, "q"))
    {
        value = string2int127(input);
        cmd = PART::control::filterQdepth;
    }
    if ((cmd == -1) && input.matchnMove(3, "bandwidth"))
    {
        value = (input.toggle() == 1);
        cmd = PART::control::exponentialBandwidth;
    }
    if ((cmd == -1) && input.matchnMove(3, "barange"))
    {
        value = string2int127(input);
        cmd = PART::control::bandwidthDepth;
    }
    if ((cmd == -1) && input.matchnMove(2, "fmamplitude"))
    {
        value = !(input.toggle() == 0);
        cmd = PART::control::FMamplitudeEnable;
    }
    if ((cmd == -1) && input.matchnMove(2, "rcenter"))
    {
        value = string2int127(input);
        cmd = PART::control::resonanceCenterFrequencyDepth;
    }
    if ((cmd == -1) && input.matchnMove(2, "rband"))
    {
        value = string2int127(input);
        cmd = PART::control::resonanceBandwidthDepth;
    }

    // portamento controls
    if (cmd == -1)
    {
        if (input.matchnMove(2, "portamento"))
        {
            value = !(input.toggle() == 0);
            cmd = PART::control::receivePortamento;
        }
        else if (input.matchnMove(2, "ptime"))
        {
            value = string2int127(input);
            cmd = PART::control::portamentoTime;
        }
        else if (input.matchnMove(2, "pdownup"))
        {
            value = string2int127(input);
            cmd = PART::control::portamentoTimeStretch;
        }
        else if (input.matchnMove(2, "pgate"))
        {
            value = string2int127(input);
            cmd = PART::control::portamentoThreshold;
        }
        else if (input.matchnMove(2, "pform"))
        {
            if (input.matchnMove(1, "start"))
                value = 0;
            else if (input.matchnMove(1, "@end"))
                value = 1;
            cmd = PART::control::portamentoThresholdType;
        }
        else if (input.matchnMove(2, "pproportional"))
        {
            value = (input.toggle() == 1);
            cmd = PART::control::enableProportionalPortamento;
        }
        else if (input.matchnMove(2, "pextent"))
        {
            value = string2int127(input);
            cmd = PART::control::proportionalPortamentoRate;
        }
        else if (input.matchnMove(2, "prange"))
        {
            value = string2int127(input);
            cmd = PART::control::proportionalPortamentoDepth;
        }
    }

    if ((cmd == -1) && input.matchnMove(2, "clear"))
    {
        if (isWrite)
            return REPLY::writeOnly_msg;
        value = 0;
        cmd = PART::control::resetAllControllers;
    }

    // midi controllers
    if (cmd == -1 && input.matchnMove(1, "e"))
    {
        if (input.matchnMove(1, "modulation"))
        {
            value = string2int127(input);
            cmd = PART::control::midiModWheel;
        }
        else if (input.matchnMove(1, "expression"))
        {
            value = string2int127(input);
            cmd = PART::control::midiExpression;
        }
        else if (input.matchnMove(2, "breath"))
        {
            value = string2int127(input);
            cmd = PART::control::midiBreath;
        }
        else if (input.matchnMove(1, "cutoff"))
        {
            value = string2int127(input);
            cmd = PART::control::midiFilterCutoff;
        }
        else if (input.matchnMove(1, "q"))
        {
            value = string2int127(input);
            cmd = PART::control::midiFilterQ;
        }
        else if (input.matchnMove(2, "bandwidth"))
        {
            value = string2int127(input);
            cmd = PART::control::midiBandwidth;
        }
    }

    if (value == -1 && controlType != TOPLEVEL::type::Write)
        value = 0;
    if (cmd > -1)
        return sendNormal(synth, 0, value, controlType, cmd, npart);
    return REPLY::available_msg;
}


int CmdInterpreter::LFOselect(Parser& input, unsigned char controlType)
{
    int cmd = -1;
    float value = -1;
    int group = -1;
    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    int engine = contextToEngines(context);
    if (engine == PART::engine::addVoice1)
        engine += voiceNumber;

    if (input.matchnMove(2, "amplitude"))
        group = TOPLEVEL::insertType::amplitude;
    else if (input.matchnMove(2, "frequency"))
        group = TOPLEVEL::insertType::frequency;
    else if (input.matchnMove(2, "filter"))
        group = TOPLEVEL::insertType::filter;
    if (group > -1)
        insertType = group;
    else
        group = insertType;
    switch (group)
    {
        case TOPLEVEL::insertType::amplitude:
            cmd = ADDVOICE::control::enableAmplitudeLFO;
            break;
        case TOPLEVEL::insertType::frequency:
            cmd = ADDVOICE::control::enableFrequencyLFO;
            break;
        case TOPLEVEL::insertType::filter:
            cmd = ADDVOICE::control::enableFilterLFO;
            break;
    }

    value = input.toggle();
    if (value > -1)
    {
        if (engine != PART::engine::addVoice1 + voiceNumber)
            return REPLY::available_msg;
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, engine);
    }
    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    value = -1;
    cmd = -1;

    if (input.matchnMove(1, "rate"))
    {
        cmd = LFOINSERT::control::speed;
        if (controlType == type_read && input.isAtEnd())
            value = 0;

        else
        {
            if (readControl(synth, 0, LFOINSERT::bpm, npart, kitNumber, engine, TOPLEVEL::insert::LFOgroup, group))
            {
                int num = string2int(input);
                input.skipChars();
                if (input.isAtEnd())
                {
                    synth->getRuntime().Log("BPM mode requires two values between 1 and 16");
                    return REPLY::done_msg;
                }
                int div = string2int(input);
                if (num > 3 && div > 3)
                {
                    synth->getRuntime().Log("Cannot have both values greater than 3");
                    return REPLY::done_msg;
                }
                else if (num == div)
                    num = div = 1;
                value = func::BPMfractionLFOfreq(num, div);
            }
            else
            {
                value = string2float(input);
                if (value < 0 || value > 1)
                {
                    synth->getRuntime().Log("frequency requires a value between 0.0 and 1.0");
                    return REPLY::done_msg;
                }
            }
        }
    }
    else if (input.matchnMove(1, "intensity"))
        cmd = LFOINSERT::control::depth;
    else if (input.matchnMove(1, "start"))
        cmd = LFOINSERT::control::start;
    else if (input.matchnMove(1, "delay"))
        cmd = LFOINSERT::control::delay;
    else if (input.matchnMove(1, "expand"))
        cmd = LFOINSERT::control::stretch;
    else if (input.matchnMove(1, "continuous"))
    {
        value = (input.toggle() == 1);
        cmd = LFOINSERT::control::continuous;
    }
    else if (input.matchnMove(1, "bpm"))
    {
        value = (input.toggle() == 1);
        cmd = LFOINSERT::control::bpm;
    }
    else if (input.matchnMove(1, "type"))
    {
        if (controlType == type_read && input.isAtEnd())
            value = 0;
        else
        {
            int idx = 0;
            while (LFOtype [idx] != "@end")
            {
                if (input.matchnMove(2, LFOtype[idx].c_str()))
                {
                    value = idx;
                    break;
                }
                ++idx;
            }
            if (value == -1)
                return REPLY::range_msg;
        }
        cmd = LFOINSERT::control::type;
    }
    else if (input.matchnMove(2, "ar"))
        cmd = LFOINSERT::control::amplitudeRandomness;
    else if (input.matchnMove(2, "fr"))
        cmd = LFOINSERT::control::frequencyRandomness;

    //std::cout << ">> base cmd " << int(cmd) << "  part " << int(npart) << "  kit " << int(kitNumber) << "  engine " << int(engine) << "  parameter " << int(group) << std::endl;

    if (value == -1)
        value = string2float(input);
    return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, engine, TOPLEVEL::insert::LFOgroup, group);
}


int CmdInterpreter::filterSelect(Parser& input, unsigned char controlType)
{
    int cmd = -1;
    float value = -1;
    int thisPart = npart;
    int kit = kitNumber;
    int param = UNUSED;
    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    int engine = contextToEngines(context);
    if (engine == PART::engine::addVoice1)
        engine += voiceNumber;
    bool isDyn = false;
    if (bitTest(context, LEVEL::AllFX) && nFXtype == 8)
    {
        kit = EFFECT::type::dynFilter;
        engine = 0;
        if (bitTest(context, LEVEL::InsFX))
        {
            thisPart = TOPLEVEL::section::insertEffects;
        }
        else if (!bitTest(context, LEVEL::Part))
        {
            thisPart = TOPLEVEL::section::systemEffects;
        }
        isDyn = true;
    }

    if (!isDyn && (engine == PART::engine::subSynth || engine == PART::engine::addVoice1 + voiceNumber))
    {
        value = input.toggle();
        if (value > -1)
        {
            if (engine == PART::engine::subSynth)
                cmd = SUBSYNTH::control::enableFilter;
            else
                cmd = ADDVOICE::control::enableFilter;
            readControl(synth, 0, FILTERINSERT::control::baseType, thisPart, kitNumber, engine, TOPLEVEL::insert::filterGroup);

            return sendNormal(synth, 0, value, controlType, cmd, thisPart, kit, engine);
        }
        value = -1; // leave it as if not set
    }

    if (input.matchnMove(2, "center"))
        cmd = FILTERINSERT::control::centerFrequency;
    else if (input.matchnMove(1, "q"))
        cmd = FILTERINSERT::control::Q;
    else if (input.matchnMove(1, "velocity"))
        cmd = FILTERINSERT::control::velocitySensitivity;
    else if (input.matchnMove(2, "slope"))
        cmd = FILTERINSERT::control::velocityCurve;
    else if (input.matchnMove(1, "gain"))
        cmd = FILTERINSERT::control::gain;
    else if (input.matchnMove(2, "tracking"))
        cmd = FILTERINSERT::control::frequencyTracking;
    else if (input.matchnMove(1, "range"))
    {
        value = (input.toggle() == 1);
        cmd = FILTERINSERT::control::frequencyTrackingRange;
    }
    else if (input.matchnMove(2, "category"))
    {
        if (controlType == type_read && input.isAtEnd())
                value = 0; // dummy value
        else
        {
            if (input.matchnMove(1, "analog"))
                value = 0;
            else if (input.matchnMove(1, "formant"))
            {
                value = 1;
                filterVowelNumber = 0;
                filterFormantNumber = 0;
            }
            else if (input.matchnMove(1, "state"))
                value = 2;
            else
                return REPLY::range_msg;
        }
        cmd = FILTERINSERT::control::baseType;
    }
    else if (input.matchnMove(2, "stages"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        value = string2int(input) - 1;
        cmd = FILTERINSERT::control::stages;
    }

    if (cmd == -1)
    {
        int baseType = readControl(synth, 0, FILTERINSERT::control::baseType, thisPart, kit, engine, TOPLEVEL::insert::filterGroup);
        //std::cout << "baseType " << baseType << std::endl;
        if (baseType == 1) // formant
        {
            if (input.matchnMove(1, "invert"))
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = (input.toggle() == 1);
                cmd = FILTERINSERT::control::negateInput;
            }
            else if (input.matchnMove(2, "fcenter"))
                cmd = FILTERINSERT::control::formantCenter;
            else if (input.matchnMove(2, "frange"))
                cmd = FILTERINSERT::control::formantOctave;
            else if (input.matchnMove(1, "expand"))
                cmd = FILTERINSERT::control::formantStretch;
            else if (input.matchnMove(1, "lucidity"))
                cmd = FILTERINSERT::control::formantClearness;
            else if (input.matchnMove(1, "morph"))
                cmd = FILTERINSERT::control::formantSlowness;
            else if (input.matchnMove(2, "size"))
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int(input);
                if (filterVowelNumber >= value)
                {
                    filterVowelNumber = value -1; // bring back into range
                    filterFormantNumber = 0; // zero as size unknown
                }
                cmd = FILTERINSERT::control::sequenceSize;
            }
            else if (input.matchnMove(2, "count"))
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int(input);
                if (filterFormantNumber >= value)
                    filterFormantNumber = value -1; // bring back into range
                cmd = FILTERINSERT::control::numberOfFormants;
            }
            else if (input.matchnMove(2, "vowel"))
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int(input);
                int number = string2int(input);
                if (number < 0 || number >= filterSequenceSize)
                    return REPLY::range_msg;
                filterVowelNumber = number;
                filterFormantNumber = 0;
                return REPLY::done_msg;
            }
            else if (input.matchnMove(1, "point"))
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int(input);
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                input.skipChars();
                int position = string2int(input);
                //std::cout << "val " << value << "  pos " << position << std::endl;
                return sendNormal(synth, 0, value, controlType, FILTERINSERT::control::vowelPositionInSequence, thisPart, kit, engine, TOPLEVEL::insert::filterGroup, position);
            }
            else if (input.matchnMove(2, "formant"))
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                int number = string2int(input);
                if (number < 0 || number >= filterNumberOfFormants)
                    return REPLY::range_msg;
                filterFormantNumber = number;
                return REPLY::done_msg;
            }
            else
            {
                if (input.matchnMove(2, "ffrequency"))
                    cmd = FILTERINSERT::control::formantFrequency;
                else if (input.matchnMove(2, "fq"))
                    cmd = FILTERINSERT::control::formantQ;
                else if (input.matchnMove(2, "fgain"))
                    cmd = FILTERINSERT::control::formantAmplitude;
                if (cmd == -1)
                    return REPLY::range_msg;
                value = string2int(input);
                return sendNormal(synth, 0, value, controlType, cmd, thisPart, kit, engine, TOPLEVEL::insert::filterGroup, filterFormantNumber, filterVowelNumber);
            }
        }
        else if (input.matchnMove(2, "type"))
        {
            if (controlType == type_read && input.isAtEnd())
                value = 0;
            switch (baseType)
            {
                case 0: // analog
                {
                    if (value == -1)
                    {
                        int idx = 0;
                        while (filterlist [idx] != "l1")
                            idx += 2;
                        int start = idx;
                        while (filterlist [idx] != "hshelf")
                            idx += 2;
                        int end = idx;
                        idx = start;
                        while (idx <= end)
                        {
                            if (input.matchnMove(2, filterlist[idx].c_str()))
                                break;
                            idx += 2;
                        }
                        if (idx > end)
                            return REPLY::range_msg;
                        value = (idx - start) / 2;
                    }
                    cmd = FILTERINSERT::control::analogType;
                    break;
                }
                case 2: // state variable
                {
                    if (value == -1)
                    {
                        int idx = 0;
                        while (filterlist [idx] != "low")
                            idx += 2;
                        int start = idx;
                        while (filterlist [idx] != "stop")
                            idx += 2;
                        int end = idx;
                        idx = start;
                        while (idx <= end)
                        {
                            if (input.matchnMove(2, filterlist[idx].c_str()))
                                break;
                            idx += 2;
                        }
                        if (idx > end)
                            return REPLY::range_msg;
                        value = (idx - start) / 2;
                    }
                    cmd = FILTERINSERT::control::stateVariableType;
                    break;
                }
                default:
                    return REPLY::available_msg;
                    break;
            }
        }
    }

    //std::cout << ">> base cmd " << int(cmd) << "  part " << int(thisPart) << "  kit " << int(kit) << "  engine " << int(engine) << "  parameter " << int(param) << std::endl;

    if (value == -1)
        value = string2float(input);

    return sendNormal(synth, 0, value, controlType, cmd, thisPart, kit, engine, TOPLEVEL::insert::filterGroup, param);
}


int CmdInterpreter::envelopeSelect(Parser& input, unsigned char controlType)
{
    int cmd = -1;
    float value = -1;
    int group = -1;
    unsigned char insert = TOPLEVEL::insert::envelopeGroup;
    unsigned char offset = UNUSED;
    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    int engine = contextToEngines(context);
    if (engine == PART::engine::addVoice1 || engine == PART::engine::addMod1)
        engine += voiceNumber;

    if (input.matchnMove(2, "amplitute"))
        group = TOPLEVEL::insertType::amplitude;
    else if (input.matchnMove(2, "frequency"))
        group = TOPLEVEL::insertType::frequency;
    else if (input.matchnMove(2, "filter"))
        group = TOPLEVEL::insertType::filter;
    else if (input.matchnMove(2, "bandwidth"))
    {
        if (bitTest(context, LEVEL::SubSynth))
            group = TOPLEVEL::insertType::bandwidth;
        else
            return REPLY::available_msg;
    }

    if (group > -1)
        insertType = group;
    else
        group = insertType;

    switch (insertType)
    {
        case TOPLEVEL::insertType::amplitude:
            if (engine < PART::engine::addMod1)
                cmd = ADDVOICE::control::enableAmplitudeEnvelope;
            else
                cmd = ADDVOICE::control::enableModulatorAmplitudeEnvelope;
            break;
        case TOPLEVEL::insertType::frequency:
            if (engine < PART::engine::addMod1)
                cmd = ADDVOICE::control::enableFrequencyEnvelope;
            else
                cmd = ADDVOICE::control::enableModulatorFrequencyEnvelope;
            break;
        case TOPLEVEL::insertType::filter:
            cmd = ADDVOICE::control::enableFilterEnvelope;
            break;
        case TOPLEVEL::insertType::bandwidth:
            cmd = SUBSYNTH::control::enableBandwidthEnvelope;
            break;
    }
    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    value = input.toggle();
    if (value > -1)
    {
        if (engine != PART::engine::addSynth && engine != PART::engine::padSynth)
            return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, engine);
        else
            return REPLY::available_msg;
    }

    if (input.matchnMove(2, "fmode"))
    {
        return sendNormal(synth, 0, (input.toggle() == 1), controlType, ENVELOPEINSERT::control::enableFreeMode, npart, kitNumber, engine, TOPLEVEL::insert::envelopeGroup, insertType);
    }

    // common controls
    value = -1;
    cmd = -1;
    if (input.matchnMove(2, "expand"))
        cmd = ENVELOPEINSERT::control::stretch;
    else if (input.matchnMove(1, "force"))
    {
        cmd = ENVELOPEINSERT::control::forcedRelease;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(2, "linear"))
    {
        cmd = ENVELOPEINSERT::control::linearEnvelope;
        value = (input.toggle() == 1);
    }

    bool freeMode = readControl(synth, 0, ENVELOPEINSERT::control::enableFreeMode, npart, kitNumber, engine, TOPLEVEL::insert::envelopeGroup, insertType);

    if (freeMode && cmd == -1)
    {
        int pointCount = readControl(synth, 0, ENVELOPEINSERT::control::points, npart, kitNumber, engine, insert, insertType);
        if (input.matchnMove(1, "Points"))
        {
            value = 0; // dummy value
            cmd = ENVELOPEINSERT::control::points;
            // not using already fetched value to get normal reporting
        }
        else if (input.matchnMove(1, "Sustain"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            if (value == 0)
            {
                    synth->getRuntime().Log("Sustain can't be at first point");
                    return REPLY::done_msg;
            }
            else if (value >= (pointCount - 1))
            {
                    synth->getRuntime().Log("Sustain can't be at last point");
                    return REPLY::done_msg;
            }
            else if (value < 0)
                return REPLY::range_msg;
            cmd = ENVELOPEINSERT::control::sustainPoint;
        }
        else
        {
            if (input.matchnMove(1, "insert"))
            {
                if ((MAX_ENVELOPE_POINTS - pointCount) < 2)
                {
                    synth->getRuntime().Log("Max points already defined");
                    return REPLY::done_msg;
                }
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;

                cmd = string2int(input); // point number
                if (cmd == 0)
                {
                    synth->getRuntime().Log("Can't add at first point");
                    return REPLY::done_msg;
                }
                if (cmd < 0 || cmd >= pointCount)
                    return REPLY::range_msg;
                input.skipChars();
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;

                offset = string2int(input); // X
                input.skipChars();
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;

                value = string2int(input); // Y
                insert = TOPLEVEL::insert::envelopePoints;

            }
            else if (input.matchnMove(1, "delete"))
            {
                if (pointCount <= 3)
                {
                    synth->getRuntime().Log("Can't have less than three points");
                    return REPLY::done_msg;
                }
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;

                cmd = string2int(input); // point number
                if (cmd == 0)
                {
                    synth->getRuntime().Log("Can't delete first point");
                    return REPLY::done_msg;
                }
                if (cmd >= (pointCount - 1))
                {
                    synth->getRuntime().Log("Can't delete last point");
                    return REPLY::done_msg;
                }
                if (cmd < 0 || cmd >= (MAX_ENVELOPE_POINTS - 1))
                    return REPLY::range_msg;
                insert = TOPLEVEL::insert::envelopePoints;
            }
            else if (input.matchnMove(1, "change"))
            {
                if (input.lineEnd(controlType))
                return REPLY::value_msg;

                cmd = string2int(input); // point number
                if (cmd < 0 || cmd >= (pointCount - 1))
                    return REPLY::range_msg;
                input.skipChars();
                if (input.lineEnd(controlType))
                return REPLY::value_msg;

                offset = string2int(input); // X
                input.skipChars();
                if (input.lineEnd(controlType))
                return REPLY::value_msg;

                value = string2int(input); // Y
                insert = TOPLEVEL::insert::envelopePointChange;
            }
        }
    }
    else if (cmd == -1)
    {
        if (input.matchnMove(1, "attack"))
        {
            if (input.matchnMove(1, "level"))
                cmd = ENVELOPEINSERT::control::attackLevel;
            else if (input.matchnMove(1, "time"))
                cmd = ENVELOPEINSERT::control::attackTime;
        }
        else if (input.matchnMove(1, "decay"))
        {
            if (input.matchnMove(1, "level"))
                cmd = ENVELOPEINSERT::control::decayLevel;
            else if (input.matchnMove(1, "time"))
                cmd = ENVELOPEINSERT::control::decayTime;
        }
        else if (input.matchnMove(1, "sustain"))
            cmd = ENVELOPEINSERT::control::sustainLevel;
        else if (input.matchnMove(1, "release"))
        {
            if (input.matchnMove(1, "level"))
                cmd = ENVELOPEINSERT::control::releaseLevel;
            else if (input.matchnMove(1, "time"))
                cmd = ENVELOPEINSERT::control::releaseTime;
        }
    }

    if (cmd == -1)
        return REPLY::op_msg;

    if (value == -1)
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        value = string2float(input);
    }

    //std::cout << ">> base cmd " << int(cmd) << "  part " << int(npart) << "  kit " << int(kitNumber) << "  engine " << int(engine) << "  parameter " << int(insertType) << std::endl;

    return sendNormal(synth, 0, string2float(input), controlType, cmd, npart, kitNumber, engine, insert, insertType, offset);
}

int CmdInterpreter::commandGroup(Parser& input)
{
    string line;
    float value = string2int(input);
    if (input.isAtEnd())
    {
        synth->getRuntime().Log("\nInstrument Groups");
        int i = 0;
        string entry = type_list[i];
        while (entry != "@end")
        {
            entry = func::stringCaps(entry, 3);
            line = "  " + func::stringCaps(entry, 3);
            synth->getRuntime().Log(line);
            ++ i;
            entry = type_list[i];
        }
        return REPLY::done_msg;
    }
    string name = string{input};
    value = stringNumInList(name, type_list, 2) + 1;
    //std::cout << value << std::endl;
    if (value < 1)
        return REPLY::range_msg;
    synth->getRuntime().Log("\n" + type_list[int(value - 1)] + " Instruments");
    list<string> msg;
    /*
    * Having two lists is messy, but the list routine clears 'msg' and
    * we need 'instrumentGroup' kept for later actual part loads.
    * Also, the search list needs embeded root, bank, and instrument IDs
    * but the reported one only wants the list number.
    */
    input.skipChars();
    bool full = (input.matchnMove(1, "location"));

    int count = 0;
    instrumentGroup.clear();
    do {
        ++ count;
        line = textMsgBuffer.fetch(readControl(synth, 0, BANK::control::findInstrumentName, TOPLEVEL::section::bank, UNUSED, UNUSED, UNUSED, value - 1));
        if (line != "*")
        {
            instrumentGroup.push_back(line);
            if (!full && line.length() > 16)
                line = line.substr(15); // remove root, bank, instrument IDs
            line = to_string(count) + "| " + line; // replace with line count
            msg.push_back(line);
        }
    } while (line != "*");
    synth->cliOutput(msg, LINES);
    return REPLY::done_msg;
}


int CmdInterpreter::commandList(Parser& input)
{
    Config &Runtime = synth->getRuntime();
    int ID;
    int tmp;
    list<string> msg;

    if (input.matchnMove(1, "instruments") || input.matchnMove(2, "programs"))
    {
        if (input.isAtEnd())
            ID = 128;
        else
            ID = string2int(input);
        synth->ListInstruments(ID, msg);
        synth->cliOutput(msg, LINES);
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "roots")) // must be before bank
    {
        synth->ListPaths(msg);
        synth->cliOutput(msg, LINES);
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "banks")
        || (bitFindHigh(context) == LEVEL::Bank && (input.isAtEnd() || input.isdigit())))
    {
        if (input.isAtEnd() | !input.isdigit())
            ID = 128;
        else
            ID = string2int(input);
        synth->ListBanks(ID, msg);
        synth->cliOutput(msg, LINES);
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "vectors"))
    {
        synth->ListVectors(msg);
        synth->cliOutput(msg, LINES);
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "parts"))
    {
        listCurrentParts(input, msg);
        synth->cliOutput(msg, LINES);
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "config"))
    {
        synth->ListSettings(msg);
        synth->cliOutput(msg, LINES);
        return REPLY::done_msg;
    }

    if (input.matchnMove(2, "mlearn"))
    {
        if (input.nextChar('@'))
        {
            input.skip(1);
            input.skipSpace();
            tmp = string2int(input);
            if (tmp > 0)
                synth->midilearn.listLine(tmp - 1);
            else
                return REPLY::value_msg;
        }
        else
        {
            synth->midilearn.listAll(msg);
            synth->cliOutput(msg, LINES);
        }
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "tuning"))
    {
        Runtime.Log("Tuning:\n" + synth->microtonal.tuningtotext());
        return REPLY::done_msg;
    }
    if (input.matchnMove(1, "keymap"))
    {
        Runtime.Log("Keymap:\n" + synth->microtonal.keymaptotext());
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "history"))
    {
        if (input.matchnMove(1, "instruments") || input.matchnMove(2, "program") )
            historyList(TOPLEVEL::XML::Instrument);
        else if (input.matchnMove(1, "patchsets"))
            historyList(TOPLEVEL::XML::Patch);
        else if (input.matchnMove(2, "scales"))
            historyList(TOPLEVEL::XML::Scale);
        else if (input.matchnMove(2, "states"))
            historyList(TOPLEVEL::XML::State);
        else if (input.matchnMove(1, "vectors"))
            historyList(TOPLEVEL::XML::Vector);
        else if (input.matchnMove(2, "mlearn"))
            historyList(TOPLEVEL::XML::MLearn);
        else
            historyList(-1);
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "effects") || input.matchnMove(1, "efx"))
        return effectsList(input);
    if (input.matchnMove(3, "presets"))
        return effectsList(input, true);

    msg.push_back("Lists:");
    helpLoop(msg, listlist, 2);
    if (synth->getRuntime().toConsole)
        // we need this in case someone is working headless
        std::cout << "\nSet CONfig REPorts [s] - set report destination (gui/stderr)\n";
    synth->cliOutput(msg, LINES);
    return REPLY::done_msg;
}


void CmdInterpreter::listCurrentParts(Parser& input, list<string>& msg_buf)
{
    int dest;
    string name = "";
    int avail = readControl(synth, 0, MAIN::control::availableParts, TOPLEVEL::section::main);
    bool full = input.matchnMove(1, "more");
    if (bitFindHigh(context) == LEVEL::Part)
    {
        if (!readControl(synth, 0, PART::control::kitMode, TOPLEVEL::section::part1 + npart))
        {
            if (readControl(synth, 0, PART::control::enable, TOPLEVEL::section::part1 + npart, UNUSED, PART::engine::addSynth))
            {
                name += " AddSynth ";
                if (full)
                {
                    string found = "";
                    for (int voice = 0; voice < NUM_VOICES; ++voice)
                    {
                        if (readControl(synth, 0, PART::control::enableAdd, TOPLEVEL::section::part1 + npart, 0, PART::engine::addVoice1 + voice))
                            found += (" " + std::to_string(voice + 1));
                    }
                    if (found > "")
                        name += ("Voices" + found + " ");
                }
            }
            if (readControl(synth, 0, PART::control::enable, TOPLEVEL::section::part1 + npart, UNUSED, PART::engine::subSynth))
                name += " SubSynth ";
            if (readControl(synth, 0, PART::control::enable, TOPLEVEL::section::part1 + npart, UNUSED, PART::engine::padSynth))
                name += " PadSynth ";
            if (name == "")
                name = "no engines active!";
            msg_buf.push_back(name);
            return;
        }
        msg_buf.push_back("kit items");
        for (int item = 0; item < NUM_KIT_ITEMS; ++item)
        {
            name = "";
            if (readControl(synth, 0, PART::control::enable, TOPLEVEL::section::part1 + npart, item, UNUSED, TOPLEVEL::insert::kitGroup))
            {
                name = "  " + std::to_string(item) + " ";
                {
                if (readControl(synth, 0, PART::control::kitItemMute, TOPLEVEL::section::part1 + npart, item, UNUSED, TOPLEVEL::insert::kitGroup))
                    name += "Quiet";
                else
                {
                    if (full)
                    {
                        name += "  key Min ";
                        int min = int(readControl(synth, 0, PART::control::minNote, TOPLEVEL::section::part1 + npart, item, UNUSED, TOPLEVEL::insert::kitGroup));
                        if (min < 10)
                            name += "  ";
                        else if (min < 100)
                            name += " ";
                        name += std::to_string(min);
                        name += "  Max ";
                        int max = int(readControl(synth, 0, PART::control::maxNote, TOPLEVEL::section::part1 + npart, item, UNUSED, TOPLEVEL::insert::kitGroup));
                        if (max < 10)
                            name += "  ";
                        else if (max < 100)
                            name += " ";

                        name += (std::to_string(max) + "  ");
                        string text = readControlText(synth, TOPLEVEL::action::lowPrio, PART::control::instrumentName, TOPLEVEL::section::part1 + npart, item, UNUSED, TOPLEVEL::insert::kitGroup);
                        if (text > "")
                            name += text;
                        msg_buf.push_back(name);
                        name = "    ";
                    }
                    if (readControl(synth, 0, PART::control::enable, TOPLEVEL::section::part1 + npart, item, PART::engine::addSynth, TOPLEVEL::insert::kitGroup))
                    {
                        name += "AddSynth ";
                        if (full)
                        {
                            string found = "";
                            for (int voice = 0; voice < NUM_VOICES; ++voice)
                            {
                                if (readControl(synth, 0, PART::control::enableAdd, TOPLEVEL::section::part1 + npart, item, PART::engine::addVoice1 + voice))
                                found += (" " + std::to_string(voice + 1));
                            }
                            if (found > "")
                                name += ("Voices" + found + " ");
                        }
                    }
                    if (readControl(synth, 0, PART::control::enable, TOPLEVEL::section::part1 + npart, item, PART::engine::subSynth, TOPLEVEL::insert::kitGroup))
                        name += "SubSynth ";
                    if (readControl(synth, 0, PART::control::enable, TOPLEVEL::section::part1 + npart, item, PART::engine::padSynth, TOPLEVEL::insert::kitGroup))
                        name += "PadSynth ";
                    if (name == "")
                        name = "no engines active!";
                }
            }
            if (name > "")
                msg_buf.push_back(name);
            }
        }
        return;
    }
    msg_buf.push_back(asString(avail) + " parts available");
    for (int partno = 0; partno < NUM_MIDI_PARTS; ++partno)
    {
        string text = readControlText(synth, TOPLEVEL::action::lowPrio, PART::control::instrumentName, TOPLEVEL::section::part1 + partno);
        bool enabled = readControl(synth, 0, PART::control::enable, TOPLEVEL::section::part1 + partno);
        if (text != DEFAULT_NAME || enabled)
        {
            if (partno < 9)
                name = " ";
            else
                name = "";
            if (enabled && partno < avail)
                name += "+";
            else
                name += " ";
            name += std::to_string(partno + 1);
            dest = readControl(synth, 0, PART::control::audioDestination, TOPLEVEL::section::part1 + partno);
            if (partno >= avail)
                name += " - " + text;
            else
            {
                if (dest == 1)
                    name += " Main";
                else if (dest == 2)
                    name += " Part";
                else
                    name += " Both";
                name += "  Chan ";
                int ch = int(readControl(synth, 0, PART::control::midiChannel, TOPLEVEL::section::part1 + partno) + 1);
                if (ch < 10)
                    name += " ";
                name += std::to_string(ch);
                if (full)
                {
                    name += "  key Min ";
                    int min = int(readControl(synth, 0, PART::control::minNote, TOPLEVEL::section::part1 + partno));
                    if (min < 10)
                        name += "  ";
                    else if (min < 100)
                        name += " ";
                    name += std::to_string(min);
                    name += "  Max ";
                    int max = int(readControl(synth, 0, PART::control::maxNote, TOPLEVEL::section::part1 + partno));
                    if (max < 10)
                        name += "  ";
                    else if (max < 100)
                        name += " ";
                    name += std::to_string(max);
                    name += "  Shift ";
                    int shift = int(readControl(synth, TOPLEVEL::action::lowPrio, PART::control::keyShift, TOPLEVEL::section::part1 + partno));
                    if (shift >= 10)
                        name += " ";
                    else if (shift >= 0)
                        name += "  ";
                    else if (shift >= -10)
                        name += " ";
                    name += std::to_string(shift);

                }
                name +=  ("  " + text);
                int mode = readControl(synth, 0, PART::control::kitMode, TOPLEVEL::section::part1 + partno);
                if (mode != PART::kitType::Off)
                    name += " > ";
                switch (mode)
                {
                    case PART::kitType::Multi:
                        name += "Multi";
                        break;
                    case PART::kitType::Single:
                        name += "Single";
                        break;
                    case PART::kitType::CrossFade:
                        name += "Crossfade";
                        break;
                }
            }
            msg_buf.push_back(name);
            if (full)
            {
                name = "    Drum ";
                int drum = readControl(synth, 0, PART::control::drumMode, TOPLEVEL::section::part1 + partno);
                if (drum)
                    name += " on";
                else
                    name += "off";
                name += " Portamento ";
                if (readControl(synth, 0, PART::control::portamento, TOPLEVEL::section::part1 + partno))
                    name += " on";
                else name += "off";
                int key = readControl(synth, 0, PART::control::keyMode, TOPLEVEL::section::part1 + partno);
                switch (key)
                {
                    case 0:
                        name += "  Polphonic";
                        break;
                    case 1:
                        name += "  Monophonic";
                        break;
                    case 2:
                        name += "  Legato";
                        if (drum)
                            name += " (drum blocked)";
                        break;
                }
                msg_buf.push_back(name);
            }
        }
    }
}


int CmdInterpreter::commandMlearn(Parser& input, unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    list<string> msg;
    bitSet(context, LEVEL::Learn);

    if (controlType != TOPLEVEL::type::Write)
    {
        Runtime.Log("Write only");
        return REPLY::done_msg; // will eventually be readable
    }

    if (input.isdigit() || input.nextChar('-')) // negative should never happen!
    {
        int lineNo = string2int(input);
        input.skipChars();
        if (lineNo <= 0)
            return REPLY::value_msg;
        else
            mline = lineNo -1;
    }
    int tmp = synth->midilearn.findSize();
    if (tmp == 0 || tmp <= mline)
    {
        if (tmp == 0)
            Runtime.Log("No learned lines");
        else
            Runtime.Log("Line " + std::to_string(mline + 1) + " Not found");
        mline = 0;
        return (REPLY::done_msg);
    }
    if (input.lineEnd(controlType))
        return REPLY::done_msg;
    {
        unsigned char type = 0;
        unsigned char control = 0;
        unsigned char kit = UNUSED;
        unsigned char engine = UNUSED;
        unsigned char insert = UNUSED;
        unsigned char parameter = UNUSED;

        if (input.matchnMove(2, "cc"))
        {
            if (!input.isdigit())
                return REPLY::value_msg;
            kit = string2int(input);
            if (kit > 129)
            {
                Runtime.Log("Max CC value is 129");
                return REPLY::done_msg;
            }
            control = MIDILEARN::control::CCorChannel;
            Runtime.Log("Lines may be re-ordered");
        }
        else if (input.matchnMove(2, "channel"))
        {
            engine = string2int(input) - 1;
            if (engine > 16)
                engine = 16;
            control = MIDILEARN::control::CCorChannel;
            Runtime.Log("Lines may be re-ordered");
        }
        else if (input.matchnMove(2, "minimum"))
        {
            insert = int((string2float(input)* 2.0f) + 0.5f);
            if (insert > 200)
                return REPLY::value_msg;
            control = MIDILEARN::control::minimum;
        }
        else if (input.matchnMove(2, "maximum"))
        {
            parameter = int((string2float(input)* 2.0f) + 0.5f);
            if (parameter > 200)
                return REPLY::value_msg;
            control = MIDILEARN::control::maximum;
        }
        else if (input.matchnMove(2, "mute"))
        {
            type = (input.toggle() == 1) * 4;
            control = MIDILEARN::control::mute;
        }
        else if (input.matchnMove(2, "limit"))
        {
            type = (input.toggle() == 1) * 2;
            control = MIDILEARN::control::limit;
        }
        else if (input.matchnMove(2, "block"))
        {
            type = (input.toggle() == 1);
            control = MIDILEARN::control::block;
        }
        else if (input.matchnMove(2, "seven"))
        {
            type = (input.toggle() == 1) * 16;
            control = MIDILEARN::control::sevenBit;
        }
        sendNormal(synth, 0, mline, type, control, TOPLEVEL::section::midiLearn, kit, engine, insert, parameter);
        return REPLY::done_msg;
    }
    return REPLY::op_msg;
}


int CmdInterpreter::commandVector(Parser& input, unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    list<string> msg;
    int tmp;
    bitSet(context, LEVEL::Vector);
    if (controlType != TOPLEVEL::type::Write)
    {
        if (synth->SingleVector(msg, chan))
            synth->cliOutput(msg, LINES);
        else
            Runtime.Log("No vector on channel " + asString(chan + 1));
        return REPLY::done_msg;
    }
    if (input.lineEnd(controlType))
    {
        if (!Runtime.vectordata.Enabled[chan])
            Runtime.Log("No vector on channel " + asString(chan + 1));
        return REPLY::done_msg;
    }

    unsigned char ch = string2int127(input);
    if (ch > 0)
    {
        ch -= 1;
        if (ch >= NUM_MIDI_CHANNELS)
            return REPLY::range_msg;
        input.skipChars();
        if (chan != ch)
        {
            chan = ch;
            axis = 0;
        }

        Runtime.Log("Vector channel set to " + asString(chan + 1));
    }

    if (input.matchWord(1, "off"))
    {
        sendDirect(synth, 0, 0,controlType,VECTOR::control::erase, TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
        axis = 0;
        bitClear(context, LEVEL::Vector);
        return REPLY::done_msg;
    }
    if (input.matchnMove(1, "xaxis"))
        axis = 0;
    else if (input.matchnMove(1, "yaxis"))
    {
        if (!Runtime.vectordata.Enabled[chan])
        {
            Runtime.Log("Vector X must be set first");
            return REPLY::done_msg;
        }
        axis = 1;
    }

    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    if (input.matchnMove(2, "cc"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;

        tmp = string2int(input);
        if (axis == 0)
        {
            sendDirect(synth, 0, tmp, controlType, VECTOR::control::Xcontroller, TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
            bitSet(context, LEVEL::Vector);
            return REPLY::done_msg;
        }
        if (Runtime.vectordata.Enabled[chan])
        {
            sendDirect(synth, 0, tmp, controlType, VECTOR::control::Ycontroller, TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
            return REPLY::done_msg;
        }
    }

    if (!Runtime.vectordata.Enabled[chan])
    {
        Runtime.Log("Vector X CC must be set first");
        return REPLY::done_msg;
    }

    if (axis == 1 && (Runtime.vectordata.Yaxis[chan] > 0x7f))
    {
        Runtime.Log("Vector Y CC must be set first");
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "name"))
    {
        string name = "!";
        if (controlType == TOPLEVEL::type::Write)
        {
            name = string{input};
            if (name <= "!")
                return REPLY::value_msg;
        }
        sendDirect(synth, TOPLEVEL::action::lowPrio, 0, controlType, VECTOR::control::name, TOPLEVEL::section::vector, UNUSED, UNUSED, chan, UNUSED, UNUSED, textMsgBuffer.push(name));
        return REPLY::done_msg;
    }

    if (input.matchnMove(1, "features"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        int feat = string2int(input) - 1;
        if (feat < 0 || feat > 3)
            return REPLY::range_msg;
        input.skipChars();
        int enable = 0;
        if (input.toggle() == 1)
            enable = 1;
        else if (feat > 1 && input.matchnMove(1, "reverse"))
            enable = 2;
        sendDirect(synth, 0, enable, controlType, VECTOR::control::Xfeature0 + (axis * (VECTOR::control::Ycontroller - VECTOR::control::Xcontroller)) + feat , TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
        return REPLY::done_msg;
    }

    if (input.matchnMove(2, "program") || input.matchnMove(1, "instrument"))
    {
        int hand = input.peek() | 32;
        input.skipChars(); // in case they type the entire word
        if ((axis == 0 && (hand == 'd' || hand == 'u')) || (axis == 1 && (hand == 'l' || hand == 'r')))
        {
            Runtime.Log("Bad direction for this axis");
            return REPLY::done_msg;
        }
        if (hand == 'l' || hand == 'd')
            hand = 0;
        else if (hand == 'r' || hand == 'u')
            hand = 1;
        else
            return REPLY::op_msg;
        tmp = string2int(input);
        sendDirect(synth, 0, tmp, controlType, VECTOR::control::XleftInstrument + hand + (axis * (VECTOR::control::Ycontroller - VECTOR::control::Xcontroller)), TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
        return REPLY::done_msg;
    }

    // this disabled for now - needs a lot of work.
    /*if (!input.matchnMove(1, "control"))
        return REPLY::op_msg;
    if (input.isdigit())
    {
        int cmd = string2int(input);
        if (cmd < 2 || cmd > 4)
            return REPLY::range_msg;
        input.skipChars();
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        tmp = string2int(input);
        if (!synth->vectorInit(axis * 3 + cmd + 6, chan, tmp))
        {
            synth->vectorSet(axis * 3 + cmd + 6, chan, tmp);
            return REPLY::done_msg;
        }
        else
            return REPLY::value_msg;
    }*/

    return REPLY::op_msg;
}


int CmdInterpreter::commandBank(Parser& input, unsigned char controlType, bool justEntered)
{
    bitSet(context, LEVEL::Bank);
    int isRoot = false;
    if  (input.matchnMove(1, "bank"))
        isRoot = false; // changes nothing as we're already at bank level :)
    if (input.matchnMove(1, "name"))
    {
        string name = string{input};
        if (controlType != type_read && name <= "!")
            return REPLY::value_msg;
        int miscMsg = textMsgBuffer.push(string(input));
        int tmp = readControl(synth, 0, BANK::control::selectBank, TOPLEVEL::section::bank);
        return sendNormal(synth, TOPLEVEL::action::lowPrio, tmp, controlType, BANK::control::renameBank, TOPLEVEL::section::bank, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, miscMsg);
    }

    if (input.matchnMove(2, "instrument"))
    {
        if (input.matchnMove(1, "rename"))
        {
            if (controlType != TOPLEVEL::type::Write)
                return REPLY::available_msg;
            if (!input.isdigit())
                return REPLY::value_msg;
            int tmp = string2int(input) - 1; // could be up to 160
            if (tmp < 0 || tmp >= MAX_INSTRUMENTS_IN_BANK)
                return REPLY::range_msg;
            input.skipChars();
            string name = string{input};
            if (name <= "!")
                return REPLY::value_msg;
            int miscMsg = textMsgBuffer.push(name);
            return sendNormal(synth, TOPLEVEL::action::lowPrio, 0, controlType, BANK::control::renameInstrument, TOPLEVEL::section::bank, UNUSED, UNUSED, tmp, UNUSED, UNUSED, miscMsg);
        }
        if (input.matchnMove(1, "save"))
        {
            if (controlType != TOPLEVEL::type::Write)
                return REPLY::available_msg;
            if (!input.isdigit())
                return REPLY::value_msg;
            int tmp = string2int(input) - 1; // could be up to 160
            if (tmp < 0 || tmp >= MAX_INSTRUMENTS_IN_BANK)
                return REPLY::range_msg;
            string line = textMsgBuffer.fetch(readControl(synth, 0, BANK::control::readInstrumentName, TOPLEVEL::section::bank, UNUSED, UNUSED, UNUSED, tmp));
            if (line > "!")
            {
                if (!query("Slot " + to_string(tmp + 1) + " contains '" + line + "'. Overwrite", false))
                    return REPLY::done_msg;
            }
            return sendNormal(synth, TOPLEVEL::action::lowPrio, 0, controlType, BANK::control::saveInstrument, TOPLEVEL::section::bank, UNUSED, UNUSED, tmp);
        }
        return REPLY::done_msg;
    }
    if (input.matchnMove(1, "root"))
        isRoot = true;
    if (input.lineEnd(controlType))
        return REPLY::done_msg;
    if (input.isdigit() || controlType == type_read)
    {
        int tmp = string2int127(input);
        input.skipChars();
        if (isRoot)
        {
            return sendNormal(synth, TOPLEVEL::action::lowPrio, tmp, controlType, BANK::control::selectRoot, TOPLEVEL::section::bank);
        }
        return sendNormal(synth, TOPLEVEL::action::lowPrio, tmp, controlType, BANK::control::selectBank, TOPLEVEL::section::bank);
        if (input.lineEnd(controlType))
            return REPLY::done_msg;
    }
    if (input.matchnMove(2, "ID"))
    {
        int tmp = string2int127(input);
        if (isRoot)
            return sendNormal(synth, TOPLEVEL::action::lowPrio, tmp, controlType, BANK::control::changeRootId, TOPLEVEL::section::bank);
    }
    if (justEntered)
        return REPLY::done_msg;
    return REPLY::op_msg;
}


int CmdInterpreter::commandConfig(Parser& input, unsigned char controlType)
{
    float value = 0;
    unsigned char command = UNUSED;
    unsigned char action = 0;
    unsigned char miscmsg = UNUSED;

    if (input.isAtEnd())
        return REPLY::done_msg; // someone just came in for a look :)
    if (input.matchnMove(1, "oscillator"))
    {
        command = CONFIG::control::oscillatorSize;
        if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
            return REPLY::value_msg;
        value = string2int(input);
    }
    else if (input.matchnMove(2, "buffer"))
    {
        command = CONFIG::control::bufferSize;
        if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
            return REPLY::value_msg;
        value = string2int(input);
    }
    else if (input.matchnMove(2, "padsynth"))
    {
        command = CONFIG::control::padSynthInterpolation;
        value = !input.matchnMove(1, "linear");
    }
    else if (input.matchnMove(1, "virtual"))
    {
        command = CONFIG::control::virtualKeyboardLayout;
        if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
            return REPLY::value_msg;
        value = string2int(input);
    }
    else if (input.matchnMove(1, "xml"))
    {
        command = CONFIG::control::XMLcompressionLevel;
        if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
            return REPLY::value_msg;
        value = string2int(input);
    }
    else if (input.matchnMove(2, "reports"))
    {
        command = CONFIG::control::reportsDestination;
        value = !input.matchnMove(1, "stdout");
    }
    else if (input.matchnMove(2, "saved"))
    {
        command = CONFIG::control::savedInstrumentFormat;
        if (input.matchnMove(1, "legacy"))
            value = 1;
        else if (input.matchnMove(1, "yoshimi"))
            value = 2;
        else if (input.matchnMove(1, "both"))
            value = 3;
        else if (controlType == TOPLEVEL::type::Write)
            return REPLY::value_msg;
    }
    //else if (input.matchnMove(3, "engines"))
    //{
        //command = CONFIG::control::showEnginesTypes;
        //value = (input.toggle() != 0);
    //}

    else if (input.matchnMove(2, "state"))
    {
        command = CONFIG::control::defaultStateStart;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(2, "single"))
    {
        command = CONFIG::control::enableSinglePath;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(1, "hide"))
    {
        command = CONFIG::control::hideNonFatalErrors;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(1, "display"))
    {
        command = CONFIG::control::showSplash;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(1, "time"))
    {
        command = CONFIG::control::logInstrumentLoadTimes;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(1, "include"))
    {
        command = CONFIG::control::logXMLheaders;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(1, "keep"))
    {
        command = CONFIG::control::saveAllXMLdata;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(1, "gui"))
    {
        command = CONFIG::control::enableGUI;
        value = input.toggle();
        if (value == -1)
            return REPLY::value_msg;
    }
    else if (input.matchnMove(1, "cli"))
    {
        command = CONFIG::control::enableCLI;
        value = input.toggle();
        if (value == -1)
            return REPLY::value_msg;
    }

    else if (input.matchnMove(2, "identify"))
    {
        command = CONFIG::control::enableHighlight;
        value = (input.toggle() == 1);
    }

    else if (input.matchnMove(3, "expose"))
    {
        value = input.toggle();
        if (value == -1 && input.matchnMove(2, "prompt"))
            value = 2;
        if (value == -1)
            return REPLY::value_msg;
        command = CONFIG::control::exposeStatus;
    }

    else if (input.matchnMove(1, "jack"))
    {
        if (input.matchnMove(1, "midi"))
        {
            command = CONFIG::control::jackMidiSource;
            action = TOPLEVEL::action::lowPrio;
            if (controlType != TOPLEVEL::type::Write || !input.isAtEnd())
            {
                if (controlType == TOPLEVEL::type::Write)
                    miscmsg = textMsgBuffer.push(input);
            }
            else
                return REPLY::value_msg;
        }
        else if (input.matchnMove(1, "server"))
        {
            command = CONFIG::control::jackServer;
            action = TOPLEVEL::action::lowPrio;
            if (controlType != TOPLEVEL::type::Write || !input.isAtEnd())
            {
                if (controlType == TOPLEVEL::type::Write)
                    miscmsg = textMsgBuffer.push(input);
            }
            else
                return REPLY::value_msg;
        }
        else if (input.matchnMove(1, "auto"))
        {
            command = CONFIG::control::jackAutoConnectAudio;
            value = (input.toggle() == 1);
        }
        else
            return REPLY::op_msg;
    }

    else if (input.matchnMove(2, "alsa"))
    {
        if (input.matchnMove(1, "type"))
        {
            command = CONFIG::control::alsaMidiType;
            if (input.matchnMove(1, "fixed"))
                value = 0;
            else if (input.matchnMove(1, "search"))
                value = 1;
            else if (input.matchnMove(1, "external"))
                value = 2;
            else
                return REPLY::value_msg;
        }
        else if (input.matchnMove(1, "midi"))
        {
            command = CONFIG::control::alsaMidiSource;
            action = TOPLEVEL::action::lowPrio;
            if (controlType != TOPLEVEL::type::Write || !input.isAtEnd())
            {
                if (controlType == TOPLEVEL::type::Write)
                    miscmsg = textMsgBuffer.push(input);
            }
            else
                return REPLY::value_msg;
        }
        else if (input.matchnMove(1, "audio"))
        {
            command = CONFIG::control::alsaAudioDevice;
            action = TOPLEVEL::action::lowPrio;
            if (controlType != TOPLEVEL::type::Write || !input.isAtEnd())
            {
                if (controlType == TOPLEVEL::type::Write)
                    miscmsg = textMsgBuffer.push(input);
            }
            else
                return REPLY::value_msg;
        }
        else if (input.matchnMove(1, "s"))
        {
            command = CONFIG::control::alsaSampleRate;
            if (controlType == TOPLEVEL::type::Write)
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int(input);
                if (value < 0 || value > 3)
                    return REPLY::range_msg;
            }
        }
        else
            return REPLY::op_msg;
    }

    else if (input.matchnMove(2, "midi"))
    {
        value = 1;
        if (input.matchnMove(1, "alsa"))
            command = CONFIG::control::alsaPreferredMidi;
        else if (controlType != TOPLEVEL::type::Write || input.matchnMove(1, "jack"))
            command = CONFIG::control::jackPreferredMidi;
        else
            return REPLY::value_msg;
    }

    else if (input.matchnMove(2, "audio"))
    {
        value = 1;
        if (input.matchnMove(1, "alsa"))
            command = CONFIG::control::alsaPreferredAudio;
        else if (controlType != TOPLEVEL::type::Write || input.matchnMove(1, "jack"))
            command = CONFIG::control::jackPreferredAudio;
        else
            return REPLY::value_msg;
    }

    else if (input.matchnMove(2, "root"))
    {
        command = CONFIG::control::bankRootCC;
        value = 128; // ignored by range check
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        if (input.matchnMove(1, "msb"))
            value = 0;
        else if (input.matchnMove(1, "lsb"))
            value = 32;
        if (value != 128 && value == readControl(synth, 0, CONFIG::control::bankCC, TOPLEVEL::section::config))
        {
            synth->getRuntime().Log("In use for bank");
            return REPLY::done_msg;
        }
    }
    else if (input.matchnMove(2, "bank"))
    {
        command = CONFIG::control::bankCC;
        value = 128; // ignored by range check
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        if (input.matchnMove(1, "msb"))
            value = 0;
        else if (input.matchnMove(1, "lsb"))
            value = 32;
        if (value != 128 && value == readControl(synth, 0, CONFIG::control::bankRootCC, TOPLEVEL::section::config))
        {
            synth->getRuntime().Log("In use for bank root");
            return REPLY::done_msg;
        }
    }
    else if (input.matchnMove(2, "program") || input.matchnMove(2, "instrument"))
    {
        command = CONFIG::control::enableProgramChange;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(2, "activate"))
    {
        command = CONFIG::control::instChangeEnablesPart;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(3, "extend"))
    {
        command = CONFIG::control::extendedProgramChangeCC;
        if (controlType != TOPLEVEL::type::Write)
            value = 128; // ignored by range check
        else if (input.lineEnd(controlType))
            return REPLY::value_msg;
        else
        {
            value = string2int(input);
            if (value > 128)
                value = 128;
        }
    }
    else if (input.matchnMove(1, "quiet"))
    {
        command = CONFIG::control::ignoreResetAllCCs;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(1, "log"))
    {
        command = CONFIG::control::logIncomingCCs;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(2, "show"))
    {
        command = CONFIG::control::showLearnEditor;
        value = (input.toggle() == 1);
    }
    else if (input.matchnMove(1, "nrpn"))
    {
        command = CONFIG::control::enableNRPNs;
        value = (input.toggle() == 1);
    }

    else if (input.matchnMove(3, "lock"))
    {
        command = CONFIG::control::historyLock;
        value = (input.toggle());
        string name = string{input}.substr(0,2);
        int selected = stringNumInList(name, historyGroup, 2);
        if (selected == -1)
            return REPLY::range_msg;
        input.skipChars();
        value = (input.toggle());
        if (controlType == TOPLEVEL::type::Write && value == -1)
            return REPLY::value_msg;
        return sendDirect(synth, TOPLEVEL::action::lowPrio, value, controlType, command, TOPLEVEL::section::config, selected);
    }

    else
        return  REPLY::op_msg;

    sendDirect(synth, action, value, controlType, command, TOPLEVEL::section::config, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, miscmsg);
    return REPLY::done_msg;
}


int CmdInterpreter::commandScale(Parser& input, unsigned char controlType)
{
    if (input.lineEnd(controlType))
        return REPLY::done_msg;
    Config &Runtime = synth->getRuntime();
    float value = 0;
    unsigned char command = UNUSED;
    unsigned char action = 0;
    unsigned char miscmsg = UNUSED;

    string name;

    if (input.matchnMove(1, "tuning"))
        command = SCALES::control::tuning;
    else if (input.matchnMove(1, "keymap"))
        command = SCALES::control::keyboardMap;
    else if (input.matchnMove(2, "name"))
        command = SCALES::control::name;
    else if (input.matchnMove(2, "description"))
        command = SCALES::control::comment;

    if (command >= SCALES::control::tuning && command <= SCALES::control::comment)
    {
        if (controlType != TOPLEVEL::type::Write && command <= SCALES::control::importKbm)
        {
            Runtime.Log("Write only - use 'list'");
            return REPLY::done_msg;
        }
        if (command <= SCALES::control::keyboardMap)
        {
            if (input.matchnMove(3, "import"))
                command += (SCALES::control::importKbm - SCALES::control::keyboardMap);
        }
        name = string{input};
        if (name == "" && controlType == TOPLEVEL::type::Write)
            return REPLY::value_msg;
        action = TOPLEVEL::action::lowPrio;
        miscmsg = textMsgBuffer.push(name);
    }
    else
    {
        int min = 0;
        int max = 127;
        if (input.matchnMove(2, "frequency"))
        {
            command = SCALES::control::refFrequency;
            min = 1;
            max = 20000;
            controlType &= ~TOPLEVEL::type::Integer; // float
        }
        else if (input.matchnMove(2, "note"))
            command = SCALES::control::refNote;
        else if (input.matchnMove(1, "invert"))
        {
            command = SCALES::control::invertScale;
            max = 1;
        }
        else if (input.matchnMove(2, "center"))
            command = SCALES::control::invertedScaleCenter;
        else if (input.matchnMove(2, "shift"))
        {
            command = SCALES::control::scaleShift;
            min = -63;
            max = 64;
        }
        else if (input.matchnMove(2, "scale"))
        {
            command = SCALES::control::enableMicrotonal;
            max = 1;
        }
        else if (input.matchnMove(2, "mapping"))
        {
            command = SCALES::control::enableKeyboardMap;
            max = 1;
        }
        else if (input.matchnMove(2, "first"))
            command = SCALES::control::lowKey;
        else if (input.matchnMove(2, "middle"))
            command = SCALES::control::middleKey;
        else if (input.matchnMove(1, "last"))
            command = SCALES::control::highKey;
        else if (input.matchnMove(3, "CLEar"))
        {
            input.skip(-1); // sneaky way to force a zero :)
            command = SCALES::control::clearAll;
        }
        else
            return REPLY::todo_msg;

        if (controlType == TOPLEVEL::type::Write)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            if ((input.toggle() == 1))
                value = 1;
            else//if (input.isdigit())
            {
                value = string2float(input);
                if (value < min || value > max)
                    return REPLY::value_msg;
            }
        }
    }
    sendDirect(synth, action, value, controlType, command, TOPLEVEL::section::scales, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, miscmsg);
    return REPLY::done_msg;
}


int CmdInterpreter::modulator(Parser& input, unsigned char controlType)
{
    if (input.lineEnd(controlType))
        return REPLY::done_msg;

// NOTE modulator number always the same as voice.

    int value;
    int cmd = -1;
    string name = string{input}.substr(0,3);
    value = stringNumInList(name, addmodnameslist, 3);
    if (value != -1)
        cmd = ADDVOICE::control::modulatorType;

    if (cmd == -1)
    {
        if (readControl(synth, 0, ADDVOICE::control::modulatorType, npart, kitNumber, PART::engine::addVoice1 + voiceNumber) == 0)
            return REPLY::inactive_msg;
        if (input.matchnMove(2, "waveform"))
        {
            bitSet(context, LEVEL::Oscillator);
            return waveform(input, controlType);
        }

        if (input.matchnMove(2, "source"))
        {
            if (input.matchnMove(1, "local"))
                value = 0;
            else
            {
                int tmp = input.peek() - char('0');
                if (tmp > 0)
                    value = tmp;
            }
            if (value == -1 || value > voiceNumber)
                return REPLY::range_msg;
            if (value == 0)
                value = 0xff;
            else
                value -= 1;
            cmd = ADDVOICE::control::externalModulator;
        }

        if (input.matchnMove(3, "oscillator"))
        {
            if (input.matchnMove(1, "internal"))
                value = 0;
            else
            {
                int tmp = input.peek() - char('0');
                if (tmp > 0)
                    value = tmp;
            }
            if (value == -1 || value > voiceNumber)
                return REPLY::range_msg;
            if (value == 0)
                value = 0xff;
            else
                value -= 1;
            cmd = ADDVOICE::control::modulatorOscillatorSource;
        }

        else if (input.matchnMove(3, "follow"))
        {
            value = (input.toggle() == 1);
            cmd = ADDVOICE::control::modulatorDetuneFromBaseOsc;
        }
        else if (input.matchnMove(3, "fixed"))
        {
            value = (input.toggle() == 1);
            cmd = ADDVOICE::control::modulatorFrequencyAs440Hz;
        }

        else if (input.matchnMove(1, "volume"))
            cmd = ADDVOICE::control::modulatorAmplitude;
        else if (input.matchnMove(2, "velocity"))
            cmd = ADDVOICE::control::modulatorVelocitySense;
        else if (input.matchnMove(2, "damping"))
            cmd = ADDVOICE::control::modulatorHFdamping;
    }

    if (cmd == -1)
    {
        if (readControl(synth, 0, ADDVOICE::control::externalModulator, npart, kitNumber, PART::engine::addVoice1 + voiceNumber) != -1)
            return  REPLY::inactive_msg;

        if (input.matchnMove(2, "local"))
        {
            if (input.matchnMove(1, "internal"))
                value = 0;
            else
            {
                int tmp = input.peek() - char('0');
                if (tmp > 0)
                    value = tmp;
            }
            if (value == -1 || value > voiceNumber)
                return REPLY::range_msg;
            if (value == 0)
                value = 0xff;
            else
                value -= 1;
            cmd = ADDVOICE::control::modulatorOscillatorSource;
        }
        if (input.matchnMove(2, "shift"))
            cmd = ADDVOICE::control::modulatorOscillatorPhase;
    }

    if (cmd == -1)
    {
        if (input.matchnMove(3, "detune"))
        {
            if (input.matchnMove(1, "fine"))
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int(input);
                cmd = ADDVOICE::control::modulatorDetuneFrequency;
            }
            else if (input.matchnMove(1, "coarse"))
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int(input);
                cmd = ADDVOICE::control::modulatorCoarseDetune;
            }
            else if (input.matchnMove(1, "type"))
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                if (controlType == type_read)
                    value = 2; // dummy value
                else
                {
                    string name = string{input}.substr(0,3);
                    value = stringNumInList(name, detuneType, 3);
                }
                if (value == -1)
                    return REPLY::range_msg;
                cmd = ADDVOICE::control::modulatorDetuneType;
            }
        }
        else if (input.matchnMove(3, "octave"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = ADDVOICE::control::modulatorOctave;
        }
    }

    if (cmd > -1)
    {
        if (value == -1)
            value = string2int(input);
        else if (value == 0xff)
            value = -1; // special case for modulator sources
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
    }

    if (input.matchnMove(3, "envelope"))
    {
        bitSet(context, LEVEL::Envelope);
        return envelopeSelect(input, controlType);
    }

    return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
}


int CmdInterpreter::addVoice(Parser& input, unsigned char controlType)
{
    if (input.isdigit())
    {
        int tmp = string2int(input) - 1;
        if (tmp < 0 || tmp >= NUM_VOICES)
            return REPLY::range_msg;
        voiceNumber = tmp;
        input.skipChars();
    }
    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    int enable = (input.toggle());
    if (enable > -1)
        return sendNormal(synth, 0, enable, controlType, ADDVOICE::control::enableVoice, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);

    if (!input.lineEnd(controlType) && !readControl(synth, 0, ADDVOICE::control::enableVoice, npart, kitNumber, PART::engine::addVoice1 + voiceNumber))
        return REPLY::inactive_msg;

    if (input.matchnMove(2, "modulator"))
    {
        bitSet(context, LEVEL::AddMod);
        return modulator(input, controlType);
    }
    else if (input.matchnMove(2, "waveform"))
    {
        bitSet(context, LEVEL::Oscillator);
        return waveform(input, controlType);
    }

    int cmd = -1;
    int tmp = -1;
    if (input.matchnMove(1, "volume"))
        cmd = ADDVOICE::control::volume;
    else if (input.matchnMove(1, "pan"))
        cmd = ADDVOICE::control::panning;
    else if (input.matchnMove(2, "prandom"))
    {
        cmd = ADDVOICE::control::enableRandomPan;
        tmp = (input.toggle() == 1);
    }
    else if (input.matchnMove(2, "pwidth"))
        cmd = ADDVOICE::control::randomWidth;

    else if (input.matchnMove(2, "velocity"))
        cmd = ADDVOICE::control::velocitySense;

    if (cmd != -1)
    {
        if (tmp == -1)
        {
            tmp = string2int127(input);
            if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
                return REPLY::value_msg;
        }
        return sendNormal(synth, 0, tmp, controlType, cmd, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
    }

    int value = 0;
    if (input.matchnMove(3, "detune"))
    {
        if (input.matchnMove(1, "fine"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = ADDVOICE::control::detuneFrequency;
        }
        else if (input.matchnMove(1, "coarse"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = ADDVOICE::control::coarseDetune;
        }
        else if (input.matchnMove(1, "type"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            if (controlType == type_read)
                value = 2; // dummy value
            else
            {
                string name = string{input}.substr(0,3);
                value = stringNumInList(name, detuneType, 3);
            }
            if (value == -1)
                return REPLY::range_msg;
            cmd = ADDVOICE::control::detuneType;
        }
    }
    else if (input.matchnMove(3, "fixed"))
    {
        value = (input.toggle() == 1);
        cmd = ADDVOICE::control::baseFrequencyAs440Hz;
    }
    else if (input.matchnMove(3, "octave"))
    {
        if (input.lineEnd(controlType))
                return REPLY::value_msg;
        value = string2int(input);
        cmd = ADDVOICE::control::octave;
    }

    else
    {
        int tmp_cmd = -1;
        if (input.matchnMove(3, "equal"))
            tmp_cmd = ADDVOICE::control::equalTemperVariation;
        else if (input.matchnMove(3, "bend"))
        {
            if (input.matchnMove(1, "adjust"))
                tmp_cmd = ADDVOICE::control::pitchBendAdjustment;
            else if (input.matchnMove(1, "offset"))
                tmp_cmd = ADDVOICE::control::pitchBendOffset;
        }
        if (tmp_cmd > -1)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = tmp_cmd;
        }
    }

    if (cmd > -1)
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);

    if (input.matchnMove(3, "lfo"))
    {
        bitSet(context, LEVEL::LFO);
        return LFOselect(input, controlType);
    }
    if (input.matchnMove(3, "filter"))
    {
        bitSet(context, LEVEL::Filter);
        return filterSelect(input, controlType);
    }
    if (input.matchnMove(3, "envelope"))
    {
        bitSet(context, LEVEL::Envelope);
        return envelopeSelect(input, controlType);
    }

    value = -1;
    if (input.matchnMove(1, "type"))
    {
        if (input.matchnMove(1, "oscillator"))
            value = 0;
        else if (input.matchnMove(1, "white"))
            value = 1;
        else if (input.matchnMove(1, "pink"))
            value = 2;
        else if (input.matchnMove(1, "spot"))
            value = 3;
        else
            return REPLY::range_msg;
        cmd = ADDVOICE::control::soundType;
    }
    else if (input.matchnMove(3, "oscillator"))
    {
        if (input.matchnMove(1, "internal"))
            value = 0;
        else
        {
            int tmp = input.peek() - char('0');
            if (tmp > 0)
                value = tmp;
        }
        if (value == -1 || value > voiceNumber)
            return REPLY::range_msg;
        if (value == 0)
            value = 0xff;
        else
            value -= 1;
        cmd = ADDVOICE::control::voiceOscillatorSource;
    }
    else if (input.matchnMove(3, "source"))
    {
        if (input.matchnMove(1, "local"))
            value = 0;
        else
        {
            int tmp = input.peek() - char('0');
            if (tmp > 0)
                value = tmp;
        }
        if (value == -1 || value > voiceNumber)
            return REPLY::range_msg;
        if (value == 0)
            value = 0xff;
        else
            value -= 1;
        cmd = ADDVOICE::control::externalOscillator;
    }
    else if (input.matchnMove(1, "phase"))
        cmd = ADDVOICE::control::voiceOscillatorPhase;
    else if (input.matchnMove(1, "minus"))
    {
        value = (input.toggle() == 1);
        cmd = ADDVOICE::control::invertPhase;
    }
    else if (input.matchnMove(3, "delay"))
        cmd = ADDVOICE::control::delay;
    else if (input.matchnMove(1, "resonance"))
    {
        value = (input.toggle() == 1);
        cmd = ADDVOICE::control::enableResonance;
    }
    else if (input.matchnMove(2, "bypass"))
    {
        value = (input.toggle() == 1);
        cmd = ADDVOICE::control::bypassGlobalFilter;
    }
    else if (input.matchnMove(1, "unison"))
    {
        value = input.toggle();
        if (value > -1)
            cmd = ADDVOICE::control::enableUnison;
        else
        {
            if (input.matchnMove(1, "size"))
                cmd = ADDVOICE::control::unisonSize;
            else if (input.matchnMove(1, "frequency"))
                cmd = ADDVOICE::control::unisonFrequencySpread;
            else if (input.matchnMove(1, "phase"))
                cmd = ADDVOICE::control::unisonPhaseRandomise;
            else if (input.matchnMove(1, "width"))
                cmd = ADDVOICE::control::unisonStereoSpread;
            else if (input.matchnMove(1, "vibrato"))
                cmd = ADDVOICE::control::unisonVibratoDepth;
            else if (input.matchnMove(1, "rate"))
                cmd = ADDVOICE::control::unisonVibratoSpeed;
            else if (input.matchnMove(1, "invert"))
            {
                if (controlType == type_read)
                    value = 1; // dummy value
                else
                {
                    value = stringNumInList(string{input}.substr(0, 1), unisonPhase, 1);
                    if (value == -1)
                        return REPLY::range_msg;
                }
                    cmd = ADDVOICE::control::unisonPhaseInvert;
            }
        }
        if (cmd == -1)
            return REPLY::op_msg;
    }
    else
        return REPLY::op_msg;

    if (value == -1)
        value = string2int(input);
    else if (value == 0xff)
            value = -1; // special case for voice and oscillator sources
    return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
}


int CmdInterpreter::addSynth(Parser& input, unsigned char controlType)
{
    int kit = UNUSED;
    int insert = UNUSED;
    if (kitMode)
    {
        kit = kitNumber;
        insert = TOPLEVEL::insert::kitGroup;
    }
    int enable = (input.toggle());
    // This is a part command, but looks like AddSynth the the CLI user
    if (enable > -1)
        sendNormal(synth, 0, enable, controlType, PART::control::enableAdd, npart, kit, UNUSED, insert);

    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    if (!readControl(synth, 0, PART::control::enable, npart, kit, PART::engine::addSynth, insert))
        return REPLY::inactive_msg;

    if (input.matchnMove(2, "resonance"))
    {
        bitSet(context, LEVEL::Resonance);
        return resonance(input, controlType);
    }
    if (input.matchnMove(3, "voice"))
    {
        bitSet(context, LEVEL::AddVoice);
        // starting point for envelopes etc.
        insertType = TOPLEVEL::insertType::amplitude;
        return addVoice(input, controlType);
    }
    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    int cmd = -1;
    int tmp = -1;
    if (input.matchnMove(1, "volume"))
        cmd = ADDSYNTH::control::volume;
    else if (input.matchnMove(1, "pan"))
        cmd = ADDSYNTH::control::panning;
    else if (input.matchnMove(2, "prandom"))
    {
        cmd = ADDSYNTH::control::enableRandomPan;
        tmp = (input.toggle() == 1);
    }
    else if (input.matchnMove(2, "pwidth"))
        cmd = ADDSYNTH::control::randomWidth;
    else if (input.matchnMove(2, "velocity"))
        cmd = ADDSYNTH::control::velocitySense;
    if (cmd != -1)
    {
        if (tmp == -1)
        {
            if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
                return REPLY::value_msg;
            tmp = string2int127(input);
        }

        return sendNormal(synth, 0, tmp, controlType, cmd, npart, kitNumber, PART::engine::addSynth);
    }

    int value = 0;
    if (input.matchnMove(3, "detune"))
    {
        if (input.matchnMove(1, "fine"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = ADDSYNTH::control::detuneFrequency;
        }
        else if (input.matchnMove(1, "coarse"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = ADDSYNTH::control::coarseDetune;
        }
        else if (input.matchnMove(1, "type"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            if (controlType == type_read)
                value = 2; // dummy value
            else
            {
                string name = string{input}.substr(0,3);
                value = stringNumInList(name, detuneType, 3);
            }
            if (value == -1)
                return REPLY::range_msg;
            cmd = ADDSYNTH::control::detuneType;
        }
    }
    else if (input.matchnMove(3, "octave"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        value = string2int(input);
        cmd = ADDSYNTH::control::octave;
    }
    else if (input.matchnMove(3, "stereo"))
    {
        cmd = ADDSYNTH::control::stereo;
        value = (input.toggle() == 1);
    }
    else
    {
        int tmp_cmd = -1;
        if (input.matchnMove(3, "depop"))
            tmp_cmd = ADDSYNTH::control::dePop;
        else if (input.matchnMove(2, "punch"))
        {
            if (input.matchnMove(1, "power"))
                tmp_cmd = ADDSYNTH::control::punchStrength;
            else if (input.matchnMove(1, "duration"))
                tmp_cmd = ADDSYNTH::control::punchDuration;
            else if (input.matchnMove(1, "stretch"))
                tmp_cmd = ADDSYNTH::control::punchStretch;
            else if (input.matchnMove(1, "velocity"))
                tmp_cmd = ADDSYNTH::control::punchVelocity;
        }
        if (tmp_cmd > -1)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = tmp_cmd;
        }
    }

    if (cmd > -1)
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::addSynth);

    if (input.matchnMove(3, "lfo"))
    {
        bitSet(context, LEVEL::LFO);
        return LFOselect(input, controlType);
    }
    if (input.matchnMove(3, "filter"))
    {
        bitSet(context, LEVEL::Filter);
        return filterSelect(input, controlType);
    }
    if (input.matchnMove(3, "envelope"))
    {
        bitSet(context, LEVEL::Envelope);
        return envelopeSelect(input, controlType);
    }

    if (input.matchnMove(2, "bandwidth"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        value = string2int(input);
        cmd = ADDSYNTH::control::relativeBandwidth;
    }
    else if (input.matchnMove(2, "group"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        value = (input.toggle() == 1);
        cmd = ADDSYNTH::control::randomGroup;
    }
    if (cmd == -1)
        return REPLY::available_msg;

    return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::addSynth);
}


int CmdInterpreter::subSynth(Parser& input, unsigned char controlType)
{
    int kit = UNUSED;
    int insert = UNUSED;
    if (kitMode)
    {
        kit = kitNumber;
        insert = TOPLEVEL::insert::kitGroup;
    }
    int enable = (input.toggle());
    // This is a part command, but looks like SubSynth the the CLI user
    if (enable > -1)
        sendNormal(synth, 0, enable, controlType, PART::control::enableSub, npart, kit, UNUSED, insert);

    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    if (!readControl(synth, 0, PART::control::enable, npart, kit, PART::engine::subSynth, insert))
        return REPLY::inactive_msg;

    int cmd = -1;
    int tmp = -1;
    if (input.matchnMove(1, "volume"))
        cmd = SUBSYNTH::control::volume;
    else if (input.matchnMove(1, "pan"))
        cmd = SUBSYNTH::control::panning;
    else if (input.matchnMove(2, "prandom"))
    {
        cmd = SUBSYNTH::control::enableRandomPan;
        tmp = (input.toggle() == 1);
    }
    else if (input.matchnMove(2, "pwidth"))
        cmd = SUBSYNTH::control::randomWidth;

    else if (input.matchnMove(2, "velocity"))
        cmd = SUBSYNTH::control::velocitySense;
    if (cmd != -1)
    {
        if (tmp == -1)
        {
            tmp = string2int127(input);
            if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
                return REPLY::value_msg;
        }
        return sendNormal(synth, 0, tmp, controlType, cmd, npart, kitNumber, PART::engine::subSynth);
    }

    int value = 0;
    if (input.matchnMove(3, "detune"))
    {
        if (input.matchnMove(1, "fine"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = SUBSYNTH::control::detuneFrequency;
        }
        else if (input.matchnMove(1, "coarse"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = SUBSYNTH::control::coarseDetune;
        }
        else if (input.matchnMove(1, "type"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            if (controlType == type_read)
                value = 2; // dummy value
            else
            {
                string name = string{input}.substr(0,3);
                value = stringNumInList(name, detuneType, 3);
            }
            if (value == -1)
                return REPLY::range_msg;
            cmd = SUBSYNTH::control::detuneType;
        }
    }
    else if (input.matchnMove(3, "fixed"))
    {
        value = (input.toggle() == 1);
        cmd = SUBSYNTH::control::baseFrequencyAs440Hz;
    }
    else if (input.matchnMove(3, "octave"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        value = string2int(input);
        cmd = SUBSYNTH::control::octave;
    }
    else if (input.matchnMove(3, "stereo"))
    {
        cmd = SUBSYNTH::control::stereo;
        value = (input.toggle() == 1);
    }

    else
    {
        int tmp_cmd = -1;
        if (input.matchnMove(3, "equal"))
            tmp_cmd = SUBSYNTH::control::equalTemperVariation;
        else if (input.matchnMove(3, "bend"))
        {
            if (input.matchnMove(1, "adjust"))
                tmp_cmd = SUBSYNTH::control::pitchBendAdjustment;
            else if (input.matchnMove(1, "offset"))
                tmp_cmd = SUBSYNTH::control::pitchBendOffset;
        }
        if (tmp_cmd > -1)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = tmp_cmd;
        }
    }

    if (cmd == -1 && input.matchnMove(3, "filter"))
    {
        bitSet(context, LEVEL::Filter);
        return filterSelect(input, controlType);
    }
    if (cmd == -1 && input.matchnMove(3, "envelope"))
    {
        bitSet(context, LEVEL::Envelope);
        return envelopeSelect(input, controlType);
    }

    if (cmd > -1)
    {
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::subSynth);
    }

    value = -1;
    if (input.matchnMove(2, "overtone"))
    {
        if (input.matchnMove(1, "Position"))
        {
            if (controlType == type_read)
                value = 1; // dummy value
            else
            {
                value = stringNumInList(string{input}.substr(0, 2), subPadPosition, 2);
                if (value == -1)
                    return REPLY::range_msg;
            }
            cmd = SUBSYNTH::control::overtonePosition;
        }
        else
        {
            if (input.matchnMove(1, "First"))
                cmd = SUBSYNTH::control::overtoneParameter1;
            else if (input.matchnMove(1, "Second"))
                cmd = SUBSYNTH::control::overtoneParameter2;
            else if (input.matchnMove(1, "Harmonic"))
                cmd = SUBSYNTH::control::overtoneForceHarmonics;
            if (cmd > -1)
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int(input);
            }
        }
    }

    if (cmd > -1)

        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::subSynth);

    if (input.matchnMove(2, "harmonic"))
    {
        int value = -1;
        if (input.matchnMove(1, "stages"))
        {
            cmd = SUBSYNTH::control::filterStages;
            value = string2int(input);
        }
        else if (input.matchnMove(1, "mag"))
        {
            cmd = SUBSYNTH::control::magType;
            if (controlType == TOPLEVEL::type::Write)
            {
                string name = string{input}.substr(0, 2);
                value = stringNumInList(name, subMagType, 2);
            }
        }
        else if (input.matchnMove(1, "position"))
        {
            cmd = SUBSYNTH::control::startPosition;
            if (input.matchnMove(1, "Zero"))
                value = 0;
            else if (input.matchnMove(1, "Random"))
                value = 1;
            else if (input.matchnMove(1, "Maximum"))
                value = 2;
        }
        if (cmd != -1)
        {
            if (value < 0 && controlType == TOPLEVEL::type::Write)
                return REPLY::value_msg;
            return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::subSynth);
        }

        int control = -1;
        unsigned char insert = UNUSED;
        bool set = false;
        if (input.lineEnd(controlType))
            return REPLY::parameter_msg;
        control = string2int(input) - 1;
        input.skipChars();
        if (input.matchnMove(1, "amplitude"))
        {
            insert = TOPLEVEL::insert::harmonicAmplitude;
            set = true;
        }
        else if (input.matchnMove(1, "bandwidth"))
        {
            insert = TOPLEVEL::insert::harmonicPhaseBandwidth;
            set = true;
        }
        if (set)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            return sendNormal(synth, 0, string2int(input), controlType, control, npart, kitNumber, PART::engine::subSynth, insert);
        }
    }

    value = -1;
    if (cmd == -1)
    {
        if (input.matchnMove(2, "band"))
        {
            if (input.matchnMove(1, "width"))
                cmd = SUBSYNTH::control::bandwidth;
            else if (input.matchnMove(1, "scale"))
                cmd = SUBSYNTH::control::bandwidthScale;
            else if (input.matchnMove(1, "envelope"))
            {
                value = (input.toggle() == 1);
                cmd = SUBSYNTH::control::enableBandwidthEnvelope;
            }
        }
        else if (input.matchnMove(2, "frequency"))
        {
            if (input.matchnMove(1, "envelope"))
            {
                value = (input.toggle() == 1);
                cmd = SUBSYNTH::control::enableFrequencyEnvelope;
            }
        }
        else if (input.matchnMove(2, "filter"))
        {
            value = (input.toggle() == 1);
            cmd = SUBSYNTH::control::enableFilter;
        }

    }

    if (cmd != -1)
    {
        //std::cout << "control " << int(cmd) << "  part " << int(npart) << "  kit " << int(kitNumber) << "  engine " << int(PART::engine::subSynth) << std::endl;
        if (value == -1)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
        }
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::subSynth);
    }
    return REPLY::available_msg;
}


int CmdInterpreter::padSynth(Parser& input, unsigned char controlType)
{
    int kit = UNUSED;
    int insert = UNUSED;
    if (kitMode)
    {
        kit = kitNumber;
        insert = TOPLEVEL::insert::kitGroup;
    }
    int enable = (input.toggle());
    // This is a part command, but looks like PadSynth the the CLI user
    if (enable > -1)
        sendNormal(synth, 0, enable, controlType, PART::control::enablePad, npart, kit, UNUSED, insert);

    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    if (!readControl(synth, 0, PART::control::enable, npart, kit, PART::engine::padSynth, insert))
        return REPLY::inactive_msg;

    if (input.matchnMove(2, "resonance"))
    {
        bitSet(context, LEVEL::Resonance);
        return resonance(input, controlType);
    }
    if (input.matchnMove(2, "waveform"))
    {
        bitSet(context, LEVEL::Oscillator);
        return waveform(input, controlType);
    }

    int cmd = -1;
    int tmp = -1;
    if (input.matchnMove(1, "volume"))
        cmd = PADSYNTH::control::volume;
    else if (input.matchnMove(1, "pan"))
        cmd = PADSYNTH::control::panning;
    else if (input.matchnMove(2, "prandom"))
    {
        cmd = SUBSYNTH::control::enableRandomPan;
        tmp = (input.toggle() == 1);
    }
    else if (input.matchnMove(2, "pwidth"))
        cmd = SUBSYNTH::control::randomWidth;

    else if (input.matchnMove(2, "velocity"))
        cmd = PADSYNTH::control::velocitySense;
    if (cmd != -1)
    {
        if (tmp == -1)
        {
            tmp = string2int127(input);
            if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
                return REPLY::value_msg;
        }
        return sendNormal(synth, 0, tmp, controlType, cmd, npart, kitNumber, PART::engine::padSynth);
    }

    int value = 0;
    if (input.matchnMove(3, "detune"))
    {
        if (input.matchnMove(1, "fine"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = PADSYNTH::control::detuneFrequency;
        }
        else if (input.matchnMove(1, "coarse"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = PADSYNTH::control::coarseDetune;
        }
        else if (input.matchnMove(1, "type"))
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            if (controlType == type_read)
                value = 2; // dummy value
            else
            {
                string name = string{input}.substr(0,3);
                value = stringNumInList(name, detuneType, 3);
            }
            if (value == -1)
                return REPLY::range_msg;
            cmd = PADSYNTH::control::detuneType;
        }
    }
    else if (input.matchnMove(3, "fixed"))
    {
        value = (input.toggle() == 1);
        cmd = PADSYNTH::control::baseFrequencyAs440Hz;
    }
    else if (input.matchnMove(3, "octave"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        value = string2int(input);
        cmd = PADSYNTH::control::octave;
    }
    else if (input.matchnMove(3, "stereo"))
    {
        cmd = PADSYNTH::control::stereo;
        value = (input.toggle() == 1);
    }

    else
    {
        int tmp_cmd = -1;
        if (input.matchnMove(3, "equal"))
            tmp_cmd = PADSYNTH::control::equalTemperVariation;
        else if (input.matchnMove(3, "bend"))
        {
            if (input.matchnMove(1, "adjust"))
                tmp_cmd = PADSYNTH::control::pitchBendAdjustment;
            else if (input.matchnMove(1, "offset"))
                tmp_cmd = PADSYNTH::control::pitchBendOffset;
        }
        if (tmp_cmd > -1)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = tmp_cmd;
        }
    }

    if (cmd > -1)
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::padSynth);

    if (input.matchnMove(3, "lfo"))
    {
        bitSet(context, LEVEL::LFO);
        return LFOselect(input, controlType);
    }
    if (input.matchnMove(3, "filter"))
    {
        bitSet(context, LEVEL::Filter);
        return filterSelect(input, controlType);
    }
    if (input.matchnMove(3, "envelope"))
    {
        bitSet(context, LEVEL::Envelope);
        return envelopeSelect(input, controlType);
    }

    value = -1;
    if (input.matchnMove(2, "overtone"))
    {
        if (input.matchnMove(1, "Position"))
        {
            if (controlType == type_read)
                value = 1; // dummy value
            else
            {
                value = stringNumInList(string{input}.substr(0, 2), subPadPosition, 2);
                if (value == -1)
                    return REPLY::range_msg;
            }
            cmd = PADSYNTH::control::overtonePosition;
        }
        else
        {
            if (input.matchnMove(1, "First"))
                cmd = PADSYNTH::control::overtoneParameter1;
            else if (input.matchnMove(1, "Second"))
                cmd = PADSYNTH::control::overtoneParameter2;
            else if (input.matchnMove(1, "Harmonic"))
                cmd = PADSYNTH::control::overtoneForceHarmonics;
            if (cmd > -1)
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int(input);
            }
        }
    }

    else
    {
        int tmp_cmd = -1;
        if (input.matchnMove(3, "depop"))
            tmp_cmd = PADSYNTH::control::dePop;
        else if (input.matchnMove(2, "punch"))
        {
            if (input.matchnMove(1, "power"))
                tmp_cmd = PADSYNTH::control::punchStrength;
            else if (input.matchnMove(1, "duration"))
                tmp_cmd = PADSYNTH::control::punchDuration;
            else if (input.matchnMove(1, "stretch"))
                tmp_cmd = PADSYNTH::control::punchStretch;
            else if (input.matchnMove(1, "velocity"))
                tmp_cmd = PADSYNTH::control::punchVelocity;
        }
        if (tmp_cmd > -1)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            cmd = tmp_cmd;
        }
    }

    if (cmd > -1)
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::padSynth);

    if (input.matchnMove(2, "xport"))
    {
        if (controlType != TOPLEVEL::type::Write)
            return REPLY::writeOnly_msg;
        if (input.isAtEnd())
            return REPLY::value_msg;
        sendDirect(synth, TOPLEVEL::action::lowPrio, 0, controlType, MAIN::control::exportPadSynthSamples, TOPLEVEL::section::main, kitNumber, 2, npart, UNUSED, UNUSED, textMsgBuffer.push(input));
        return REPLY::done_msg;
    }

    value = -1;
    if (input.matchnMove(2, "profile"))
    {
        if (input.matchnMove(1, "gauss"))
            value = 0;
        else if (input.matchnMove(1, "square"))
            value = 1;
        else if (input.matchnMove(1, "double"))
            value = 2;
        else
            return REPLY::value_msg;

        cmd = PADSYNTH::control::baseType;
    }
    else if (input.matchnMove(2, "width"))
    {
        cmd = PADSYNTH::control::baseWidth;
    }
    else if (input.matchnMove(2, "count"))
    {
        cmd = PADSYNTH::control::frequencyMultiplier;
    }
    else if (input.matchnMove(2, "expand"))
    {
        cmd = PADSYNTH::control::modulatorStretch;
    }
    else if (input.matchnMove(2, "frequency"))
    {
        cmd = PADSYNTH::control::modulatorFrequency;
    }
    else if (input.matchnMove(2, "size"))
    {
        cmd = PADSYNTH::control::size;
    }
    else if (input.matchnMove(2, "cross"))
    {
        if (input.matchnMove(1, "full"))
            value = 0;
        else if (input.matchnMove(1, "upper"))
            value = 1;
        else if (input.matchnMove(1, "lower"))
            value = 2;
        else
            return REPLY::value_msg;

        cmd = PADSYNTH::control::harmonicSidebands;
    }
    else if (input.matchnMove(2, "multiplier"))
    {
        if (input.matchnMove(1, "off"))
            value = 0;
        else if (input.matchnMove(1, "gauss"))
            value = 1;
        else if (input.matchnMove(1, "sine"))
            value = 2;
        else if (input.matchnMove(1, "double"))
            value = 3;
        else
            return REPLY::value_msg;

        cmd = PADSYNTH::control::amplitudeMultiplier;
    }
    else if (input.matchnMove(2, "mode"))
    {
        if (input.matchnMove(1, "Sum"))
            value = 0;
        else if (input.matchnMove(1, "mult"))
            value = 1;
        else if (input.matchnMove(1, "d1"))
            value = 2;
        else if (input.matchnMove(1, "d2"))
            value = 3;
        else
            return REPLY::value_msg;

        cmd = PADSYNTH::control::amplitudeMode;
    }
    else if (input.matchnMove(2, "center"))
    {
        cmd = PADSYNTH::control::spectralWidth;
    }
    else if (input.matchnMove(3, "relative"))
    {
        cmd = PADSYNTH::control::spectralAmplitude;
    }
    else if (input.matchnMove(2, "auto"))
    {
        value = (input.toggle() > 0);
        cmd = PADSYNTH::control::autoscale;
    }
    else if (input.matchnMove(3, "base"))
    {
        for (int i = 0; i < 9; ++ i)
        {
            if (basetypes[i] == string{input})
            {
                value = i;
                cmd = PADSYNTH::control::harmonicBase;
                break;
            }
        }
        if (cmd == -1)
            return REPLY::range_msg;
    }
    else if (input.matchnMove(2, "samples"))
    {
        unsigned char sizes[] {1, 2, 4, 6, 8, 12, 24};
        value = string2float(input);
        int tmp = value * 2;
        for (int i = 0; i < 7; ++i)
        {
            if (tmp == sizes[i])
            {
                value = i;
                cmd = PADSYNTH::control::samplesPerOctave;
                break;
            }
        }
        if (cmd == -1)
            return REPLY::range_msg;
    }
    else if (input.matchnMove(2, "range"))
    {
        cmd = PADSYNTH::control::numberOfOctaves;
    }
    else if (input.matchnMove(2, "length"))
    {
        value = bitFindHigh(string2int(input)) - 4;
        if (value > 6)
            return REPLY::range_msg;
        cmd = PADSYNTH::control::sampleSize;
    }
    else if (input.matchnMove(2, "bandwidth"))
    {
        cmd = PADSYNTH::control::bandwidth;
    }
    else if (input.matchnMove(2, "scale"))
    {
        if (input.matchnMove(1, "normal"))
            value = 0;
        else if (input.matchnMove(1, "equalhz"))
            value = 1;
        else if (input.matchnMove(1, "quarter"))
            value = 2;
        else if (input.matchnMove(1, "half"))
            value = 3;
        else if (input.matchnMove(1, "threequart"))
            value = 4;
        else if (input.matchnMove(1, "oneandhalf"))
            value = 5;
        else if (input.matchnMove(1, "double"))
            value = 6;
        else if (input.matchnMove(1, "inversehalf"))
            value = 7;
        else
            return REPLY::range_msg;

        cmd = PADSYNTH::control::bandwidthScale;
    }
    else if (input.matchnMove(2, "spectrum"))
    {
        if (input.matchnMove(1, "bandwidth"))
            value = 0;
        else if (input.matchnMove(1, "discrete"))
            value = 1;
        else if (input.matchnMove(1, "continuous"))
            value = 2;
        else
            return REPLY::range_msg;

        cmd = PADSYNTH::control::spectrumMode;
    }

    if (input.matchnMove(2, "apply"))
    {
        value = 0; // dummy
        cmd = PADSYNTH::control::applyChanges;
    }

    if (cmd > -1)
    {
        if (value == -1)
            value = string2int(input);
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, PART::engine::padSynth);
    }
    return REPLY::available_msg;
}


int CmdInterpreter::resonance(Parser& input, unsigned char controlType)
{
    int value = input.toggle();
    int cmd = -1;
    int engine = contextToEngines(context);
    int insert = TOPLEVEL::insert::resonanceGroup;
    if (value > -1)
    {
        sendNormal(synth, 0, value, controlType, RESONANCE::control::enableResonance, npart, kitNumber, engine, insert);
        return REPLY::done_msg;
    }
    if (input.lineEnd(controlType))
        return REPLY::done_msg;

    if (input.matchnMove(1, "random"))
    {
        if (input.matchnMove(1, "coarse"))
            value = 0;
        else if (input.matchnMove(1, "medium"))
            value = 1;
        else if (input.matchnMove(1, "fine"))
            value = 2;
        else
            return REPLY::value_msg;
        cmd = RESONANCE::control::randomType;
    }
    else if (input.matchnMove(2, "protect"))
    {
        value = (input.toggle() == 1);
        cmd = RESONANCE::control::protectFundamental;
    }
    else if (input.matchnMove(1, "maxdb"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        cmd = RESONANCE::control::maxDb;
        value = string2int(input);
    }
    else if (input.matchnMove(2, "center"))
    {
        value = string2int(input);
        cmd = RESONANCE::control::centerFrequency;
    }
    else if (input.matchnMove(1, "octaves"))
    {
        value = string2int(input);
        cmd = RESONANCE::control::octaves;
    }
    else if (input.matchnMove(1, "interpolate"))
    {
        if (input.matchnMove(1, "linear"))
            value = 1;
        else if (input.matchnMove(1, "smooth"))
            value = 0;
        else return REPLY::value_msg;
        cmd = RESONANCE::control::interpolatePeaks;
    }
    else if (input.matchnMove(1, "smooth"))
        cmd = RESONANCE::control::smoothGraph;
    else if (input.matchnMove(1, "clear"))
        cmd = RESONANCE::control::clearGraph;

    if (cmd > -1)
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, engine, insert);

    if (input.matchnMove(2, "points"))
    {
        insert = TOPLEVEL::insert::resonanceGraphInsert;
        if (input.isAtEnd()) // need to catch reading as well
        {
            if (controlType & TOPLEVEL::type::Limits)
                return sendNormal(synth, 0, 0, controlType, 1, npart, kitNumber, engine, insert);
            else
            {
                for (int i = 0; i < MAX_RESONANCE_POINTS; i += 8)
                {
                    string line = asAlignedString(i + 1, 4) + ">";
                    for (int j = 0; j < (MAX_RESONANCE_POINTS / 32); ++ j)
                    {
                        line += asAlignedString(readControl(synth, 0, RESONANCE::control::graphPoint, npart, kitNumber, engine, insert, i + j), 4);
                    }
                    synth->getRuntime().Log(line);
                }
            }
            return REPLY::done_msg;
        }
        cmd = RESONANCE::control::graphPoint;

        int point = string2int(input) - 1;
        if (point < 0 || point >= MAX_RESONANCE_POINTS)
            return REPLY::range_msg;
        input.skipChars();
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        value = string2int(input);
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, engine, insert, point);
    }

    return REPLY::available_msg;
}


int CmdInterpreter::waveform(Parser& input, unsigned char controlType)
{
    if (input.lineEnd(controlType))
        return REPLY::done_msg;
    float value = -1;
    int cmd = -1;
    int engine = contextToEngines(context);
    unsigned char insert = TOPLEVEL::insert::oscillatorGroup;

    if (controlType == type_read && input.isAtEnd())
        value = 0; // dummy value
    else
    {
        string name = string{input}.substr(0,3);
        value = stringNumInList(name, wavebase, 3);
    }

    if (value != -1)
        cmd = OSCILLATOR::control::baseFunctionType;
    else if (input.matchnMove(1, "harmonic"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;

        if (input.matchnMove(1, "shift"))
            cmd = OSCILLATOR::control::harmonicShift;
        else if (input.matchnMove(1, "before"))
        {
            value = (input.toggle() == 1);
            cmd = OSCILLATOR::control::shiftBeforeWaveshapeAndFilter;
        }
        else
        {
            cmd = string2int(input) - 1;
            if (cmd < 0 || cmd >= MAX_AD_HARMONICS)
                return REPLY::range_msg;
            input.skipChars();

            if (input.matchnMove(1, "amp"))
                insert = TOPLEVEL::insert::harmonicAmplitude;
            else if (input.matchnMove(1, "phase"))
                insert = TOPLEVEL::insert::harmonicPhaseBandwidth;

            if (input.lineEnd(controlType))
                return REPLY::value_msg;
        }
        if (value == -1)
            value = string2int(input);
        return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, engine + voiceNumber, insert);
    }

    else if (input.matchnMove(2, "convert"))
    {
        value = 0; // dummy
        cmd = OSCILLATOR::control::convertToSine;
    }

    else if (input.matchnMove(2, "clear"))
    {
        value = 0; // dummy
        cmd = OSCILLATOR::control::clearHarmonics;
    }

    else if (input.matchnMove(2, "shape"))
    {
        if (input.matchnMove(1, "type"))
        {
            string name = string{input}.substr(0,3);
            value = stringNumInList(name, filtershapes, 3);
            if (value == -1)
                return REPLY::value_msg;
            cmd = OSCILLATOR::control::waveshapeType;
        }
        else if (input.matchnMove(1, "par"))
            cmd = OSCILLATOR::control::waveshapeParameter;
        else return REPLY::op_msg;
    }

    else if (input.matchnMove(1, "filter"))
    {
        if (input.matchnMove(1, "type"))
        {
            if (controlType != TOPLEVEL::type::Write)
                value = 0; // dummy value
            else
            {
                string name = string{input}.substr(0,3);
                value = stringNumInList(name, filtertype, 3);
                if (value == -1)
                    return REPLY::value_msg;
            }
            cmd = OSCILLATOR::control::filterType;
        }
        else if (input.matchnMove(1, "par"))
        {
            switch (input.peek())
            {
                case char('1'):
                    cmd = OSCILLATOR::control::filterParameter1;
                    break;
                case char('2'):
                    cmd = OSCILLATOR::control::filterParameter2;
                    break;
                default:
                    return REPLY::op_msg;
            }
            input.skipChars();
        }
        else if (input.matchnMove(1, "before"))
        {
            value = (input.toggle() == 1);
            cmd = OSCILLATOR::control::filterBeforeWaveshape;
        }
        else return REPLY::op_msg;
    }

    else if (input.matchnMove(1, "base"))
    {
        if (input.matchnMove(1, "par"))
            cmd = OSCILLATOR::control::baseFunctionParameter;
        else if (input.matchnMove(1, "convert"))
        {
            value = (input.toggle() != 0);
            cmd = OSCILLATOR::control::useAsBaseFunction;
        }
        else if (input.matchnMove(1, "mod"))
        {
            if (input.matchnMove(1, "type"))
            {
                if (input.matchnMove(3, "off"))
                    value = 0;
                else if (input.matchnMove(1, "Rev"))
                    value = 1;
                else if (input.matchnMove(1, "Sine"))
                    value = 2;
                else if (input.matchnMove(1, "Power"))
                    value = 3;
                else
                    return REPLY::value_msg;
                cmd = OSCILLATOR::control::baseModulationType;
            }
            else if (input.matchnMove(1, "par"))
            {
                switch (input.peek())
                {
                    case char('1'):
                        cmd = OSCILLATOR::control::baseModulationParameter1;
                        break;
                    case char('2'):
                        cmd = OSCILLATOR::control::baseModulationParameter2;
                        break;
                    case char('3'):
                        cmd = OSCILLATOR::control::baseModulationParameter3;
                        break;
                    default:
                        return REPLY::range_msg;
                }
                input.skipChars();
            }
            else
                return REPLY::op_msg;
        }
        else
            return REPLY::op_msg;
    }

    else if (input.matchnMove(2, "spectrum"))
    {
        if (input.matchnMove(1, "type"))
        {
            if (input.matchnMove(3, "OFF"))
                value = 0;
            else if (input.matchnMove(3, "Power"))
                value = 1;
            else if (input.matchnMove(1, "Down"))
                value = 2;
            else if (input.matchnMove(1, "Up"))
                value = 3;
            else
                return REPLY::value_msg;
            cmd = OSCILLATOR::control::spectrumAdjustType;
        }
        else if (input.matchnMove(1, "par"))
            cmd = OSCILLATOR::control::spectrumAdjustParameter;
        else return REPLY::op_msg;
    }

    else if (input.matchnMove(2, "adaptive"))
    {
        if (input.matchnMove(1, "type"))
        {
            string name = string{input}.substr(0,3);
            value = stringNumInList(name, adaptive, 3);
            if (value == -1)
                return REPLY::value_msg;
            cmd = OSCILLATOR::control::adaptiveHarmonicsType;
        }
        else if (input.matchnMove(1, "base"))
            cmd = OSCILLATOR::control::adaptiveHarmonicsBase;
        else if (input.matchnMove(1, "level"))
            cmd = OSCILLATOR::control::adaptiveHarmonicsPower;
        else if (input.matchnMove(1, "par"))
            cmd = OSCILLATOR::control::adaptiveHarmonicsParameter;
        else
            return REPLY::op_msg;
    }

    else if (input.matchnMove(2, "apply"))
    {
        if (engine != PART::engine::padSynth)
            return REPLY::available_msg;
        value = 0; // dummy
        insert = UNUSED;
        cmd = PADSYNTH::control::applyChanges;
    }
    if (cmd == -1)
        return REPLY::available_msg;
    if (value == -1)
        value = string2float(input);
    return sendNormal(synth, 0, value, controlType, cmd, npart, kitNumber, engine + voiceNumber, insert);
}


int CmdInterpreter::commandPart(Parser& input, unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    int tmp = -1;
    if (bitTest(context, LEVEL::AllFX))
        return effects(input, controlType);
    if (input.matchnMove(2, "bypass"))
    {
        int effnum = string2int(input);
        if (effnum < 1 || effnum > NUM_PART_EFX)
            return REPLY::range_msg;
        input.skipChars();
        bool value = false;
        if (!input.lineEnd(controlType))
            value = (input.toggle() == 1);
        return sendNormal(synth, 0, value, controlType, PART::control::effectBypass, npart, UNUSED, effnum - 1, TOPLEVEL::insert::partEffectSelect);
    }
    if (input.lineEnd(controlType))
        return REPLY::done_msg;
    if (kitMode == PART::kitType::Off)
        kitNumber = UNUSED; // always clear it if not kit mode

    // This is for actual effect definition and editing. See below for kit selection.
    if (!inKitEditor)
    {
        if (input.matchnMove(2, "effects") || input.matchnMove(2, "efx"))
        {
            context = LEVEL::Top;
            bitSet(context, LEVEL::AllFX);
            bitSet(context, LEVEL::Part);
            return effects(input, controlType);
        }
    }

    if (input.isdigit())
    {
        tmp = string2int127(input);
        input.skipChars();
        if (tmp > 0)
        {
            tmp -= 1;
            if (!inKitEditor)
            {
                if (tmp >= Runtime.NumAvailableParts)
                {
                    Runtime.Log("Part number too high");
                    return REPLY::done_msg;
                }

                //if (npart != tmp) // TODO sort this properly!
                {
                    npart = tmp;
                    if (controlType == TOPLEVEL::type::Write)
                    {
                        context = LEVEL::Top;
                        bitSet(context, LEVEL::Part);
                        kitMode = PART::kitType::Off;
                        kitNumber = 0;
                        voiceNumber = 0; // must clear this too!
                        sendNormal(synth, 0, npart, TOPLEVEL::type::Write, MAIN::control::partNumber, TOPLEVEL::section::main);
                    }
                }
                if (input.lineEnd(controlType))
                    return REPLY::done_msg;
            }
            else
            {
                if (controlType == TOPLEVEL::type::Write)
                {
                    if (tmp >= NUM_KIT_ITEMS)
                        return REPLY::range_msg;
                    kitNumber = tmp;
                    voiceNumber = 0;// to avoid confusion
                }
                Runtime.Log("Kit item number " + std::to_string(kitNumber + 1));
                return REPLY::done_msg;
            }
        }
    }

    int enable = input.toggle();
    if (enable != -1)
    {
        if (!inKitEditor)
        {
            int result = sendNormal(synth, 0, enable, controlType, PART::control::enable, npart);
            if (input.lineEnd(controlType))
                return result;
        }
        else if (readControl(synth, 0, PART::control::enable, npart))
        {
            if (enable >= 0)
            {
                if (kitNumber == 0)
                {
                    synth->getRuntime().Log("Kit item 1 always on.");
                    return REPLY::done_msg;
                }
                return sendNormal(synth, 0, enable, controlType, PART::control::enableKitLine, npart, kitNumber, UNUSED, TOPLEVEL::insert::kitGroup);
            }
        }
    }

    if (input.matchnMove(2, "clear"))
    {
        if (controlType != TOPLEVEL::type::Write)
            return REPLY::writeOnly_msg;
        return sendNormal(synth, 0, npart, controlType, MAIN::control::defaultPart, TOPLEVEL::section::main);
    }

    if (input.matchnMove(2, "program") || input.matchnMove(1, "instrument"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            Runtime.Log("Part name is " + synth->part[npart]->Pname);
            return REPLY::done_msg;
        }

        if (!input.isAtEnd()) // force part not channel number
        {
            if (input.matchnMove(1, "group"))
            {
                if (instrumentGroup.empty())
                {
                    Runtime.Log("No list entries, or list not seen");
                    return REPLY::done_msg;
                }
                size_t value = string2int(input);
                string line;
                if (value < 1 || value > instrumentGroup.size())
                    return REPLY::range_msg;
                -- value;

                std::list<string>:: iterator it = instrumentGroup.begin();
                size_t lineNo = 0;
                while (lineNo < value && it != instrumentGroup.end())
                {
                    ++ it;
                    ++ lineNo;
                }
                if (it == instrumentGroup.end())
                    return REPLY::range_msg;
                line = *it;

                int root = string2int(line.substr(0, 3));
                int bank = string2int(line.substr(5, 3));
                int inst = (string2int(line.substr(10, 3))) - 1;

                sendDirect(synth, 0, inst, controlType, MAIN::control::loadInstrumentFromBank, TOPLEVEL::section::main, npart, bank, root);
                return REPLY::done_msg;
            }
            tmp = string2int(input) - 1;
            if (tmp < 0 || tmp >= MAX_INSTRUMENTS_IN_BANK)
                return REPLY::range_msg;
            sendDirect(synth, 0, tmp, controlType, MAIN::control::loadInstrumentFromBank, TOPLEVEL::section::main, npart);
            return REPLY::done_msg;
        }
        else
            return REPLY::value_msg;
    }

    if (input.matchnMove(2, "latest"))
    {
        int result = readControl(synth, 0, BANK::control::lastSeenInBank, TOPLEVEL::section::bank);
        int root = result & 0xff;

        if (root == UNUSED)
        {
            synth->getRuntime().Log("Latest not defined");
            return REPLY::done_msg;
        }
        bool isSave = ((root & 0x80) != 0);
        root &= 0x7f;

        int instrument = result >> 15;
        int bank = (result >> 8) & 0x7f;
        string name = "A part was ";
        if (isSave)
            name += "sent to I ";
        else
            name += "fetched from I ";
        name += (to_string(instrument + 1) + ", B " + to_string(bank) + ", R " + to_string(root));
        synth->getRuntime().Log(name);
        return REPLY::done_msg;
    }


    if (!readControl(synth, 0, PART::control::enable, npart))
        return REPLY::inactive_msg;

    tmp = -1;
    if (input.matchnMove(3, "normal"))
        tmp = PART::kitType::Off;
    else if (input.matchnMove(2, "multi"))
        tmp = PART::kitType::Multi;
    else if (input.matchnMove(2, "single"))
        tmp = PART::kitType::Single;
    else if (input.matchnMove(2, "crossfade"))
        tmp = PART::kitType::CrossFade;
    else if (input.matchnMove(3, "kit"))
    {
        if (kitMode == PART::kitType::Off)
            return REPLY::inactive_msg;
        inKitEditor = true;
        return REPLY::done_msg;
    }

    if (tmp != -1)
    {
        kitNumber = 0;
        voiceNumber = 0; // must clear this too!
        kitMode = tmp;
        inKitEditor = (kitMode != PART::kitType::Off);
        return sendNormal(synth, 0, kitMode, controlType, PART::control::kitMode, npart);
    }

    if (bitTest(context, LEVEL::AllFX))
        return effects(input, controlType);

    if (input.matchnMove(3, "addsynth"))
    {
        bitSet(context, LEVEL::AddSynth);
        insertType = TOPLEVEL::insertType::amplitude;
        return addSynth(input, controlType);
    }

    if (input.matchnMove(3, "subsynth"))
    {
        bitSet(context, LEVEL::SubSynth);
        insertType = TOPLEVEL::insertType::amplitude;
        return subSynth(input, controlType);
    }

    if (input.matchnMove(3, "padsynth"))
    {
        bitSet(context, LEVEL::PadSynth);
        voiceNumber = 0; // TODO find out what *realy* causes this to screw up!
        insertType = TOPLEVEL::insertType::amplitude;
        return padSynth(input, controlType);
    }

    if (input.matchnMove(3, "mcontrol"))
    {
        bitSet(context, LEVEL::MControl);
        return midiControllers(input, controlType);
    }

    if (inKitEditor)
    {
        int value;
        if (input.matchnMove(2, "drum"))
            return sendNormal(synth, 0, (input.toggle() != 0), controlType, PART::control::drumMode, npart);
        if (input.matchnMove(2, "quiet"))
            return sendNormal(synth, 0, (input.toggle() != 0), controlType, PART::control::kitItemMute, npart, kitNumber, UNUSED, TOPLEVEL::insert::kitGroup);
         // This is for selection from 3 part effects. See above for definitions.
        if (input.matchnMove(2,"effect"))
        {
            if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
                return REPLY::value_msg;
            value = string2int(input);
            if (value < 0 || value > NUM_PART_EFX)
                return REPLY::range_msg;
            return sendNormal(synth, 0, value, controlType | TOPLEVEL::type::Integer, PART::control::kitEffectNum, npart, kitNumber, UNUSED, TOPLEVEL::insert::kitGroup);
        }
        if (input.matchnMove(2,"name"))
        {
            int miscmsg = NO_MSG;
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            if (controlType == TOPLEVEL::type::Write)
                miscmsg = textMsgBuffer.push(input);
            return sendNormal(synth, TOPLEVEL::action::muteAndLoop, 0, controlType, PART::control::instrumentName, npart, kitNumber, UNUSED, TOPLEVEL::insert::kitGroup, UNUSED, UNUSED, miscmsg);
        }
    }

    int value = 0;
    int cmd = -1;
    if (input.matchnMove(2, "min"))
    {
        cmd = PART::control::minNote;
        if (controlType == TOPLEVEL::type::Write)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            if (input.matchnMove(1, "last"))
                cmd = PART::control::minToLastKey;
            else
            {
                value = string2int(input);
                if (value > synth->part[npart]->Pmaxkey)
                    return REPLY::high_msg;
            }
        }

    }
    else if (input.matchnMove(2, "max"))
    {
        cmd = PART::control::maxNote;
        if (controlType == TOPLEVEL::type::Write)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            if (input.matchnMove(1, "last"))
                cmd = PART::control::maxToLastKey;
            else
            {
                value = string2int(input);
                if (value < synth->part[npart]->Pminkey)
                    return REPLY::low_msg;
            }
        }

    }
    else if (input.matchnMove(2, "full"))
         cmd = PART::control::resetMinMaxKey;

    if (cmd > -1)
    {
        int insert = UNUSED;
        int kit = kitNumber;
        if (inKitEditor)
            insert = TOPLEVEL::insert::kitGroup;
        else
            kit = UNUSED;
        return sendNormal(synth, 0, value, controlType, cmd, npart, kit, UNUSED, insert);
    }

    if (input.matchnMove(2, "shift"))
    {
        if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
            return REPLY::value_msg;
        int value = string2int(input);
        if (value < MIN_KEY_SHIFT)
            value = MIN_KEY_SHIFT;
        else if (value > MAX_KEY_SHIFT)
            value = MAX_KEY_SHIFT;
        return sendNormal(synth, TOPLEVEL::action::lowPrio, value, controlType, PART::control::keyShift, npart);
    }

    if (input.matchnMove(1, "volume"))
        cmd = PART::control::volume;
    else if (input.matchnMove(1, "pan"))
        cmd = PART::control::panning;
    else if (input.matchnMove(2, "velocity"))
        cmd = PART::control::velocitySense;
    else if (input.matchnMove(2, "LEvel"))
        cmd = PART::control::velocityOffset;
    if (cmd != -1)
    {
        int tmp = string2int127(input);
        if (controlType == TOPLEVEL::type::Write && input.isAtEnd())
            return REPLY::value_msg;
        return sendNormal(synth, 0, tmp, controlType, cmd, npart);
    }

    if (input.matchnMove(2, "channel"))
    {
        tmp = string2int127(input);
        if (controlType == TOPLEVEL::type::Write && tmp < 1)
            return REPLY::value_msg;
        tmp -= 1;
        return sendNormal(synth, 0, tmp, controlType, PART::control::midiChannel, npart);
    }
    if (input.matchnMove(2, "aftertouch"))
    {
        int tmp = PART::aftertouchType::modulation * 2;
        int cmd = PART::control::channelATset;
        if (input.matchnMove(1, "key"))
            cmd = PART::control::keyATset;
        else if (!input.matchnMove(1, "chan"))
            return REPLY::op_msg;
        if (input.matchnMove(1, "Off"))
            tmp = PART::aftertouchType::off;
        else
        {
            if (input.matchnMove(1, "Filter"))
            {
                tmp = PART::aftertouchType::filterCutoff;
                if (input.matchnMove(1, "Down"))
                    tmp |= PART::aftertouchType::filterCutoffDown;
            }
            if (input.matchnMove(1, "Peak"))
            {
                tmp = PART::aftertouchType::filterQ;
                if (input.matchnMove(1, "Down"))
                    tmp |= PART::aftertouchType::filterQdown;
            }
            if (input.matchnMove(1, "Bend"))
            {
                tmp |= PART::aftertouchType::pitchBend;
                if (input.matchnMove(1, "Down"))
                    tmp |= PART::aftertouchType::pitchBendDown;
            }
            if (input.matchnMove(1, "Volume"))
                tmp |= PART::aftertouchType::volume;
            if (input.matchnMove(1, "Modulation"))
                tmp |= PART::aftertouchType::modulation;
        }
        if (tmp == PART::aftertouchType::modulation * 2 && controlType != type_read)
            return REPLY::value_msg;
        return sendNormal(synth, 0, tmp & (PART::aftertouchType::modulation * 2 - 1), controlType, cmd, npart);
    }
    if (input.matchnMove(1, "destination"))
    {
        int dest = 0;
        if (controlType == TOPLEVEL::type::Write)
        {
            if (input.matchnMove(1, "main"))
                dest = 1;
            else if (input.matchnMove(1, "part"))
                dest = 2;
            else if (input.matchnMove(1, "both"))
                dest = 3;
            if (dest == 0)
                return REPLY::range_msg;
        }
        return sendNormal(synth, TOPLEVEL::action::muteAndLoop, dest, controlType, PART::control::audioDestination, npart);
    }
    if (input.matchnMove(1, "note"))
    {
        int value = 0;
        if (controlType == TOPLEVEL::type::Write)
        {
            if (input.lineEnd(controlType))
                return REPLY::value_msg;
            value = string2int(input);
            if (value < 1 || value > POLIPHONY)
                return REPLY::range_msg;
        }
        return sendNormal(synth, 0, value, controlType, PART::control::maxNotes, npart);
    }

    if (input.matchnMove(1, "mode"))
    {
        int value = 0;
        if (controlType == TOPLEVEL::type::Write)
        {
            if (input.matchnMove(1, "poly"))
                value = 0;
            else if (input.matchnMove(1, "mono"))
                value = 1;
            else if (input.matchnMove(1, "legato"))
                value = 2;
            else
                return REPLY::name_msg;
        }
        return sendNormal(synth, 0, value, controlType, PART::control::keyMode, npart);
    }
    if (input.matchnMove(2, "portamento"))
        return sendNormal(synth, 0, (input.toggle() == 1), controlType, PART::control::portamento, npart);
    if (input.matchnMove(1, "humanise"))
    {
        int cmd = -1;
        if (input.matchnMove(1, "pitch"))
            cmd = PART::control::humanise;
        else if (input.matchnMove(1, "velocity"))
            cmd = PART::control::humanvelocity;
        else
            return REPLY::op_msg;
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        return sendNormal(synth, 0, string2int(input), controlType, cmd, npart);
    }
    if (input.matchnMove(2, "name"))
    {
        string name;
        unsigned char miscmsg = NO_MSG;
        if (controlType == TOPLEVEL::type::Write)
        {
            name = string{input};
            if (name.size() < 3)
            {
                Runtime.Log("Name too short");
                return REPLY::done_msg;
            }
            else if (name == DEFAULT_NAME)
            {
                Runtime.Log("Cant use name of default sound");
                return REPLY::done_msg;
            }
            else
                miscmsg = textMsgBuffer.push(name);
        }
        return sendNormal(synth, TOPLEVEL::action::lowPrio, 0, controlType, PART::control::instrumentName, npart, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, miscmsg);
    }
    if (input.matchnMove(3,"copyright"))
    {
        string name;
        if (controlType == TOPLEVEL::type::Write)
        {
            name = string{input};
            if (name.size() < 2)
                return REPLY::value_msg;
        }
        unsigned char miscmsg = textMsgBuffer.push(name);
        return sendNormal(synth, TOPLEVEL::action::lowPrio, 0, controlType, PART::control::instrumentCopyright, npart, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, miscmsg);
    }
    if (input.matchnMove(3,"info"))
    {
        string name;
        if (controlType == TOPLEVEL::type::Write)
        {
            name = string{input};
            if (name.size() < 2)
                return REPLY::value_msg;
        }
        unsigned char miscmsg = textMsgBuffer.push(name);
        return sendNormal(synth, TOPLEVEL::action::lowPrio, 0, controlType, PART::control::instrumentComments, npart, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, miscmsg);
    }
    if (input.matchnMove(3,"type"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        int pos = 0;
        if (controlType == TOPLEVEL::type::Write)
        {
            string name = type_list[pos];
            while (name != "@end" && !input.matchnMove(3, name.c_str()))
            {
                ++ pos;
                name = type_list[pos];
            }
            if (name == "@end")
                pos = 0; // undefined
        }
        return sendNormal(synth, TOPLEVEL::action::lowPrio, pos, controlType, PART::control::instrumentType, npart);
    }
    return REPLY::op_msg;
}


int CmdInterpreter::commandReadnSet(Parser& input, unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    string name;


    /*CommandBlock getData;
    getData.data.value = 0;
    getData.data.part = TOPLEVEL::section::copyPaste;
    getData.data.kit = 0;
    getData.data.engine = 135;
    getData.data.insert = UNUSED;
    std::cout << synth->unifiedpresets.findSectionName(&getData) << std::endl;*/


    if (input.matchnMove(2, "yoshimi"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            //Runtime.Log("Instance " + asString(currentInstance), 1);
            Runtime.Log("Instance " + std::to_string(synth->getUniqueId()));
            return REPLY::done_msg;
        }
        if (input.lineEnd(controlType))
            return REPLY::value_msg;

        resetInstance(string2int(input));
        return REPLY::done_msg;
    }

    if (input.matchnMove(4, "tone"))
    {

        if (controlType != TOPLEVEL::type::Write)
            return REPLY::available_msg;
        if (input.lineEnd(controlType))
            return REPLY::value_msg;

        int chan = string2int(input) - 1;
        input.skipChars();
        if (chan < 0 || chan > 15)
            return REPLY::range_msg;

        int pitch = string2int(input);
        input.skipChars();
        if (pitch < 0 || pitch > 127)
            return REPLY::range_msg;

        int velocity = string2int(input);
        int control;
        if (velocity > 0 && velocity <= 127)
            control = MIDI::noteOn;
        else
            control = MIDI::noteOff;

        sendDirect(synth, 0, velocity, controlType, control, TOPLEVEL::midiIn, chan, pitch);
        return REPLY::done_msg;
    }

    if (input.matchnMove(4, "seed"))
    {
        if (controlType != TOPLEVEL::type::Write)
            return REPLY::available_msg;
        int seed = string2int(input);
        if (seed < 0)
            seed = 0;
        else if (seed > 0xffffff)
            seed = 0xffffff;
        sendDirect(synth, 0, seed, controlType | TOPLEVEL::type::Integer, MAIN::control::reseed, TOPLEVEL::main);
        return REPLY::done_msg;
    }

    switch (bitFindHigh(context))
    {
        case LEVEL::Config:
            return commandConfig(input, controlType);
            break;
        case LEVEL::Bank:
            return commandBank(input, controlType);
            break;
        case LEVEL::Scale:
            return commandScale(input, controlType);
            break;
        case LEVEL::Envelope:
            return envelopeSelect(input, controlType);
            break;
        case LEVEL::Filter:
            return filterSelect(input, controlType);
            break;
        case LEVEL::LFO:
            return LFOselect(input, controlType);
            break;
        case LEVEL::Resonance:
            return resonance(input, controlType);
            break;
        case LEVEL::Oscillator:
            return waveform(input, controlType);
            break;
        case LEVEL::AddMod:
            return modulator(input, controlType);
            break;
        case LEVEL::AddVoice:
            return addVoice(input, controlType);
            break;
        case LEVEL::AddSynth:
            return addSynth(input, controlType);
            break;
        case LEVEL::SubSynth:
            return subSynth(input, controlType);
            break;
        case LEVEL::PadSynth:
            return padSynth(input, controlType);
            break;
        case LEVEL::MControl:
            return midiControllers(input, controlType);
            break;
        case LEVEL::Part:
            return commandPart(input, controlType);
            break;
        case LEVEL::Vector:
            return commandVector(input, controlType);
            break;
        case LEVEL::Learn:
            return commandMlearn(input, controlType);
            break;
    }

    if (input.matchnMove(3, "mono"))
    {
        return sendNormal(synth, 0, (input.toggle() == 1), controlType, MAIN::control::mono, TOPLEVEL::section::main);
    }

    if (input.matchnMove(2, "config"))
    {
        context = LEVEL::Top;
        bitSet(context, LEVEL::Config);
        return commandConfig(input, controlType);
    }

    if (input.matchnMove(1, "bank"))
    {
        context = LEVEL::Top;
        bitSet(context, LEVEL::Bank);
        return commandBank(input, controlType, true);
    }

    if (input.matchnMove(1, "scale"))
    {
        context = LEVEL::Top;
        bitSet(context, LEVEL::Scale);
        return commandScale(input, controlType);
    }

    if (input.matchnMove(1, "part"))
    {
        nFX = 0; // just to be sure
        // TODO get correct part number
        /*if (controlType != TOPLEVEL::type::Write && input.isAtEnd())
        {
            if (synth->partonoffRead(npart))
                name = " enabled";
            else
                name = " disabled";
            Runtime.Log("Current part " + asString(npart + 1) + name, 1);
            return REPLY::done_msg;
        }*/
        context = LEVEL::Top;
        bitSet(context, LEVEL::Part);
        nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
        return commandPart(input, controlType);
    }

    if (input.matchnMove(2, "vector"))
    {
        context = LEVEL::Top;
        return commandVector(input, controlType);
    }

    if (input.matchnMove(2, "mlearn"))
    {
        context = LEVEL::Top;
        return commandMlearn(input, controlType);
    }

    if ((context == LEVEL::Top || bitTest(context, LEVEL::InsFX)) && input.matchnMove(3, "system"))
    {
        bitSet(context,LEVEL::AllFX);
        bitClear(context, LEVEL::InsFX);
        nFX = 0; // just to be sure
        input.matchnMove(2, "effects"); // clear it if given
        input.matchnMove(2, "efx");
        nFXtype = synth->sysefx[nFX]->geteffect();
        return effects(input, controlType);
    }
    if ((context == LEVEL::Top || bitTest(context, LEVEL::AllFX)) && !bitTest(context, LEVEL::Part) && input.matchnMove(3, "insert"))
    {
        bitSet(context,LEVEL::AllFX);
        bitSet(context,LEVEL::InsFX);
        nFX = 0; // just to be sure
        input.matchnMove(2, "effects"); // clear it if given
        input.matchnMove(2, "efx");
        nFXtype = synth->insefx[nFX]->geteffect();
        return effects(input, controlType);
    }
    if (bitTest(context, LEVEL::AllFX))
        return effects(input, controlType);

    if (input.matchnMove(1, "volume"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        return sendNormal(synth, 0, string2int127(input), controlType, MAIN::control::volume, TOPLEVEL::section::main);
    }
    if (input.matchnMove(2, "detune"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        return sendNormal(synth, TOPLEVEL::action::lowPrio, string2int127(input), controlType, MAIN::control::detune, TOPLEVEL::section::main);
    }

    if (input.matchnMove(2, "shift"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        int value = string2int(input);
        return sendNormal(synth, TOPLEVEL::action::lowPrio, value, controlType, MAIN::control::keyShift, TOPLEVEL::section::main);
    }

    if (input.matchnMove(2, "solo"))
    {
        int value = MIDI::SoloType::Disabled;

        if (input.matchnMove(2, "cc"))
        {
            if (controlType == TOPLEVEL::type::Write)
            {
                if (input.lineEnd(controlType))
                    return REPLY::value_msg;
                value = string2int127(input);
                string otherCC = Runtime.masterCCtest(value);
                if (otherCC > "")
                {
                    Runtime.Log("In use for " + otherCC);
                    return REPLY::done_msg;
                }
            }
            return sendNormal(synth, 0, value, controlType, MAIN::control::soloCC, TOPLEVEL::section::main);
        }

        else if (input.matchnMove(2, "row"))
            value = MIDI::SoloType::Row;
        else if (input.matchnMove(2, "column"))
            value = MIDI::SoloType::Column;
        else if (input.matchnMove(2, "loop"))
            value = MIDI::SoloType::Loop;
        else if (input.matchnMove(2, "twoway"))
            value = MIDI::SoloType::TwoWay;
        else if (input.matchnMove(2, "channel"))
            value = MIDI::SoloType::Channel;
        return sendNormal(synth, 0, value, controlType, MAIN::control::soloType, TOPLEVEL::section::main);
    }
    if (input.matchnMove(2, "available")) // 16, 32, 64
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        int value = string2int(input);
        if (controlType == TOPLEVEL::type::Write && value != 16 && value != 32 && value != 64)
            return REPLY::range_msg;
        return sendNormal(synth, 0, value, controlType, MAIN::control::availableParts, TOPLEVEL::section::main);
    }
    if (input.matchnMove(3, "panning"))
    {
        int value = MAIN::panningType::normal;
        if (input.matchnMove(1, "cut"))
            value = MAIN::panningType::cut;
        else if (input.matchnMove(1, "boost"))
            value = MAIN::panningType::boost;
        else if (!input.matchnMove(1, "default") && controlType == TOPLEVEL::type::Write)
            return REPLY::range_msg;
        return sendNormal(synth, 0, value, controlType, MAIN::control::panLawType, TOPLEVEL::section::main);
    }
    if (input.matchnMove(2, "clear"))
    {
        if (input.lineEnd(controlType))
            return REPLY::value_msg;
        int value = string2int(input) -1;
        if (value < 0)
            return REPLY::range_msg;
        return sendNormal(synth, 0, value, controlType, MAIN::control::defaultPart, TOPLEVEL::section::main);
    }
    return REPLY::op_msg;
}


Reply CmdInterpreter::processSrcriptFile(const string& filename, bool toplevel)
{
    if (filename <= "!")
        return Reply::what("Exec");

    Config &Runtime = synth->getRuntime();
    stringstream reader(loadText(filename));
    if (reader.eof())
    {
        Runtime.Log("Can't read file " + filename);
        return Reply::DONE;
    }

    cli::Parser scriptParser;
    if (toplevel)
        context = LEVEL::Top; // start from top level

    string line;
    int lineNo = 0;
    const char DELIM_NEWLINE ='\n';
    while (std::getline(reader, line, DELIM_NEWLINE))
    {
        //std::cout << "line >" << line << "<" << std::endl;
        scriptParser.initWithExternalBuffer(line);
        if (scriptParser.isTooLarge())
        {
            Runtime.Log("*** Error: line " + to_string(lineNo) + " too long");
            return Reply(REPLY::failed_msg);
        }
        ++ lineNo;
        if (line.empty())
            continue; // skip empty line but count it.

        scriptParser.skipSpace();
        if (scriptParser.peek() == '#' || iscntrl((unsigned char) scriptParser.peek()))
        {   // skip comment lines
            continue;
        }
        if (scriptParser.matchnMove(3, "run"))
        {
            Runtime.Log("*** Error: scripts are not recursive @ line " + std::to_string(lineNo) + " ***");
            return Reply(REPLY::failed_msg);
        }
        if (scriptParser.matchnMove(4, "wait"))
        {
            int mSec = string2int(scriptParser);
            if (mSec < 2)
                mSec = 2;
            else if (mSec > 30000)
                mSec = 30000;
            mSec -= 1; //allow for internal time
            Runtime.Log("Waiting " + std::to_string(mSec) + "mS");
            if (mSec > 1000)
            {
                sleep (mSec / 1000);
                mSec = mSec % 1000;
            }
            usleep(mSec * 1000);
        }
        else
        {
            usleep(2000); // the loop is too fast otherwise!
            Reply reply = cmdIfaceProcessCommand(scriptParser);
            if (reply.code > REPLY::done_msg)
            {
                Runtime.Log("*** Error: " + replies[reply.code] + " @ line " + std::to_string(lineNo) + ": " + line + " ***");
                return Reply(REPLY::failed_msg);
            }
        }
    }
    return Reply::DONE;
}


Reply CmdInterpreter::cmdIfaceProcessCommand(Parser& input)
{
    input.trim();

    unsigned int newID = synth->getUniqueId();
    if (newID != currentInstance)
    {
        currentInstance = newID;
        defaults();
    }
    Config &Runtime = synth->getRuntime();

    buildStatus(false);

#ifdef REPORT_NOTES_ON_OFF
    if (input.matchnMove(3, "report")) // note test
    {
        std::cout << "note on sent " << Runtime.noteOnSent << std::endl;
        std::cout << "note on seen " << Runtime.noteOnSeen << std::endl;
        std::cout << "note off sent " << Runtime.noteOffSent << std::endl;
        std::cout << "note off seen " << Runtime.noteOffSeen << std::endl;
        std::cout << "notes hanging sent " << Runtime.noteOnSent - Runtime.noteOffSent << std::endl;
        std::cout << "notes hanging seen " << Runtime.noteOnSeen - Runtime.noteOffSeen << std::endl;
        return Reply::DONE;
    }
#endif
    if (input.matchnMove(5, "filer"))
    {
        string result;
        file::dir2string(result, "/home/will", ".xiz");
        std::cout << result << std::endl;
        return Reply::DONE;
    }

    if (input.matchnMove(2, "exit"))
    {
        if (input.matchnMove(2, "force"))
        {
            sendDirect(synth, 0, 0, TOPLEVEL::type::Write, TOPLEVEL::control::forceExit, UNUSED);
            return Reply::DONE;
        }
        bool echo = (synth->getRuntime().toConsole);
        if (currentInstance > 0)
        {
            if (echo)
                std::cout << "Can only exit from instance 0" << std::endl;
            Runtime.Log("Can only exit from instance 0", 1);
            return Reply::DONE;
        }
        string message;
        if (Runtime.configChanged)
        {
            if (echo)
                std::cout << "System config has been changed. Still exit N/y?" << std::endl;
            message = "System config has been changed. Still exit";
        }
        else
        {
            if (echo)
                std::cout << "All data will be lost. Still exit N/y?" << std::endl;
            message = "All data will be lost. Still exit";
        }
        if (query(message, false))
        {
            // this seems backwards but it *always* saves.
            // seeing configChanged makes it reload the old config first.
            Runtime.runSynth = false;
            return Reply{REPLY::exit_msg};
        }
        return Reply::DONE;
    }

    if (input.nextChar('/'))
    {
        input.skip(1);
        input.skipSpace();
        defaults();
        if (input.isAtEnd())
            return Reply::DONE;
    }

    if (input.matchnMove(3, "reset"))
    {
        int control = MAIN::control::masterReset;
        if (input.matchnMove(3, "all"))
            control = MAIN::control::masterResetAndMlearn;
        if (query("Restore to basic settings", false))
        {
            sendDirect(synth, TOPLEVEL::action::muteAndLoop, 0, TOPLEVEL::type::Write, control, TOPLEVEL::section::main);
            defaults();
        }
        return Reply::DONE;
    }

    if (input.startsWith(".."))
    {
        input.skip(2);
        input.skipSpace();
        if (bitFindHigh(context) == LEVEL::Filter)
        {
            filterVowelNumber = 0;
            filterFormantNumber = 0;
        }

        /*
         * kit mode is a pseudo context level so the code
         * below emulates normal 'back' actions
         */
        if (bitFindHigh(context) == LEVEL::Part && kitMode != PART::kitType::Off)
        {
            int newPart = npart;
            input.markPoint();
            defaults();
            npart = newPart;
            bitSet(context, LEVEL::Part);
            if (input.matchnMove(1, "set"))
            {
                if (!input.isdigit())
                    input.reset_to_mark();
                else
                {
                    int tmp = string2int(input);
                    if (tmp < 1 || tmp > Runtime.NumAvailableParts)
                        return REPLY::range_msg;

                    npart = tmp -1;
                    return Reply::DONE;
                }
            }
            else
                return Reply::DONE;
        }

        if (bitFindHigh(context) == LEVEL::AllFX || bitFindHigh(context) == LEVEL::InsFX)
            defaults();
        else if (bitFindHigh(context) == LEVEL::Part)
        {
            int temPart = npart;
            if (bitTest(context, LEVEL::AllFX) || bitTest(context, LEVEL::InsFX))
            {
                defaults();
                bitSet(context, LEVEL::Part); // restore part level
            }
            else
                defaults();
            npart = temPart;
        }
        else
        {
            bitClearHigh(context);
        }
        if (input.isAtEnd())
            return Reply::DONE;
    }

    if (helpList(input, context))
        return Reply::DONE;

    if (input.matchnMove(2, "stop"))
        return Reply{sendNormal(synth, 0, 0, TOPLEVEL::type::Write,MAIN::control::stopSound, TOPLEVEL::section::main)};
    if (input.matchnMove(1, "list"))
    {
        if (input.matchnMove(1, "group"))
        {
            //if (!readControl(synth, 0, CONFIG::control::showEnginesTypes, TOPLEVEL::section::config))
            //{
                //synth->getRuntime().Log("Instrument engine and type info must be enabled");
                //return REPLY::done_msg;
            //}
            return Reply{commandGroup(input)};
        }
        return Reply{commandList(input)};
    }

    if (input.matchnMove(4, "runlocal"))
        return processSrcriptFile(input, false);
    if (input.matchnMove(3, "run"))
        return processSrcriptFile(input);

    if (input.matchnMove(1, "set"))
    {
        if (!input.isAtEnd())
            return Reply{commandReadnSet(input, TOPLEVEL::type::Write)};
        else
            return Reply::what("set");
    }

    if (input.matchnMove(1, "read") || input.matchnMove(1, "get"))
    {
        /*
         * we no longer test for line end as some contexts can return
         * useful information with a simple read.
         */
        return Reply{commandReadnSet(input, type_read)};
    }

    if (input.matchnMove(3, "minimum"))
    {
        if (!input.isAtEnd())
            return Reply{commandReadnSet(input, TOPLEVEL::type::Minimum | TOPLEVEL::type::Limits)};
        else
        {
            return Reply::what("minimum");
        }
    }

    if (input.matchnMove(3, "maximum"))
    {
        if (!input.isAtEnd())
            return Reply{commandReadnSet(input, TOPLEVEL::type::Maximum | TOPLEVEL::type::Limits)};
        else
        {
            return Reply::what("maximum");
        }
    }

    if (input.matchnMove(3, "default"))
    {
        if (!input.isAtEnd())
            return Reply{commandReadnSet(input, TOPLEVEL::type::Default | TOPLEVEL::type::Limits)};
        else
        {
            return Reply::what("default");
        }
    }

    if (input.matchnMove(2, "mlearn"))
    {
        if (!input.isAtEnd())
            return Reply{commandReadnSet(input, TOPLEVEL::type::LearnRequest)};
        else
        {
            return Reply::what("mlearn");
        }
    }

    if (input.matchnMove(3, "add"))
    {
        if (input.matchnMove(1, "root"))
        {
            return sendNormal(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, BANK::control::addNamedRoot, TOPLEVEL::section::bank, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(input));
        }
        if (input.matchnMove(1, "bank"))
        {
            int root = readControl(synth, 0, BANK::control::selectRoot, TOPLEVEL::section::bank);

            return sendNormal(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, BANK::control::createBank, TOPLEVEL::section::bank, UNUSED, root, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(input));
        }
        if (input.matchnMove(2, "yoshimi"))
        {
            if (currentInstance !=0)
            {
                Runtime.Log("Only instance 0 can start others");
                return Reply::DONE;
            }
            int forceId = string2int(input);
            if (forceId < 1 || forceId >= 32)
                forceId = 0;
            sendDirect(synth, TOPLEVEL::action::lowPrio, forceId, TOPLEVEL::type::Write, MAIN::control::startInstance, TOPLEVEL::section::main);
            return Reply::DONE;
        }
        else
        {
            return Reply::what("add");
        }
    }

    if (input.matchWord(3, "import") || input.matchWord(3, "export") )
    { // need the double test to find which then move along line
        int type = 0;
        string replyMsg;
        if (input.matchnMove(3, "import"))
        {
            type = MAIN::control::importBank;
            replyMsg = "import";
        }
        else if (input.matchnMove(3, "export"))
        {
            type = MAIN::control::exportBank;
            replyMsg = "export";
        }

        int root = UNUSED;
        if (input.matchnMove(1, "root"))
        {
            if (input.isdigit())
            {
                root = string2int(input);
                input.skipChars();
            }
            else
                root = 200; // force invalid root error
        }
        int value = string2int(input);
        input.skipChars();
        string name(input);
        if (root < 0 || (root > 127 && root != UNUSED) || value < 0 || value > 127 || name <="!")
            return Reply{REPLY::what_msg};
        else
        {
            sendDirect(synth, TOPLEVEL::action::lowPrio, value, TOPLEVEL::type::Write, type, TOPLEVEL::section::main, root, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(name));
            return Reply{REPLY::done_msg, replyMsg};
        }
    }

    if (input.matchnMove(3, "remove"))
    {
        if  (input.matchnMove(1, "root"))
        {
            if (input.isdigit())
            {
                int rootID = string2int(input);
                if (rootID >= MAX_BANK_ROOT_DIRS)
                    return Reply{REPLY::range_msg};
                else
                {
                    sendDirect(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write,BANK::deselectRoot, TOPLEVEL::section::bank, rootID);
                    return Reply::DONE;
                }
            }
            else
                return Reply{REPLY::value_msg};
        }
        if (input.matchnMove(1, "bank"))
        {
            if (!input.isdigit())
                return Reply{REPLY::value_msg};

            int bankID = string2int(input);
            if (bankID >= MAX_BANKS_IN_ROOT)
                return Reply{REPLY::range_msg};

            int rootID = readControl(synth, 0, BANK::control::selectRoot, TOPLEVEL::section::bank);
            input.skipChars();
            if (!input.isAtEnd())
            {
                if (input.matchnMove(1, "root"))
                {
                    if (!input.isdigit())
                        return Reply{REPLY::value_msg};
                    rootID = string2int(input);
                    if (rootID >= MAX_BANK_ROOT_DIRS)
                        return Reply{REPLY::range_msg};
                }
            }
            int tmp = int(readControl(synth, TOPLEVEL::action::lowPrio, BANK::control::findBankSize, TOPLEVEL::section::bank, bankID, rootID));
            if (tmp == UNUSED)
            {
                Runtime.Log("No bank at this location");
                return Reply::DONE;
            }
            else if (tmp)
            {
                Runtime.Log("Bank " + to_string(bankID) + " has " + asString(tmp) + " Instruments");
                if (!query("Delete bank and all of these", false))
                {
                    Runtime.Log("Aborted");
                    return Reply::DONE;
                }
            }
            sendDirect(synth, TOPLEVEL::action::lowPrio, bankID, TOPLEVEL::type::Write, MAIN::control::deleteBank, TOPLEVEL::section::main, rootID);
            return Reply::DONE;
        }
        if (input.matchnMove(2, "yoshimi"))
        {
            if (input.isAtEnd())
            {
                return Reply::what("remove");
            }
            else
            {
                unsigned int to_close = string2int(input);
                if (to_close == 0)
                    Runtime.Log("Use 'Exit' to close main instance");
                else if (to_close == currentInstance)
                    Runtime.Log("Instance can't close itself");
                else
                {
                    sendDirect(synth, TOPLEVEL::action::lowPrio, to_close, TOPLEVEL::type::Write, MAIN::control::stopInstance, TOPLEVEL::section::main);
                }
                return Reply::DONE;
            }
        }
        if (input.matchnMove(2, "mlearn"))
        {
            if (input.matchnMove(3, "all"))
            {
                sendNormal(synth, 0, 0, 0, MIDILEARN::control::clearAll, TOPLEVEL::section::midiLearn);
                return Reply::DONE;
            }
            else if (input.nextChar('@'))
            {
                input.skip(1);
                input.skipSpace();
                int tmp = string2int(input);
                if (tmp == 0)
                    return Reply{REPLY::value_msg};
                sendNormal(synth, 0, tmp - 1, 0, MIDILEARN::control::deleteLine, TOPLEVEL::section::midiLearn);
                return Reply::DONE;
            }
        }
        if (input.matchnMove(2, "instrument") || input.matchnMove(2, "program"))
        {
            int tmp = string2int(input);
            if (tmp <= 0 || tmp > MAX_INSTRUMENTS_IN_BANK)
                    return Reply{REPLY::range_msg};
            if (query("Permanently remove instrument " + to_string(tmp) + " from bank", false))
                sendDirect(synth, TOPLEVEL::action::lowPrio, tmp - 1, TOPLEVEL::type::Write, BANK::control::deleteInstrument, TOPLEVEL::section::bank);
            return Reply::DONE;
        }
        return Reply::what("remove");
    }

    else if (input.matchnMove(2, "load"))
    {
        if (input.matchnMove(2, "mlearn"))
        {
            if (input.nextChar('@'))
            {
                input.skip(1);
                int tmp = string2int(input);
                if (tmp == 0)
                    return Reply{REPLY::value_msg};
                sendNormal(synth, 0, tmp - 1, TOPLEVEL::type::Write, MIDILEARN::control::loadFromRecent, TOPLEVEL::section::midiLearn);
                return Reply::DONE;
            }
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            sendNormal(synth, 0, 0, TOPLEVEL::type::Write, MIDILEARN::control::loadList, TOPLEVEL::section::midiLearn, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(string{input}));
            return Reply::DONE;
        }
        if (input.matchnMove(2, "vector"))
        {
            string loadChan;
            unsigned char ch;
            if (input.matchnMove(1, "channel"))
            {
                ch = string2int127(input);
                if (ch > 0)
                {
                    ch -= 1;
                    input.skipChars();
                }
                else
                    ch = chan;
                loadChan = "channel " + asString(ch + 1);
            }
            else
            {
                ch = UNUSED;
                loadChan = "source channel";
            }
            if (ch != UNUSED && ch >= NUM_MIDI_CHANNELS)
                return Reply{REPLY::range_msg};
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            string name;
            if (input.nextChar('@'))
            {
                input.skip(1);
                input.skipSpace();
                int tmp = string2int(input);
                if (tmp <= 0)
                    return Reply{REPLY::value_msg};
                name = historySelect(5, tmp - 1);
                if (name == "")
                    return Reply::DONE;
            }
            else
            {
                name = string{input};
                if (name == "")
                    return Reply{REPLY::name_msg};
            }
            sendDirect(synth, TOPLEVEL::action::muteAndLoop, 0, TOPLEVEL::type::Write, MAIN::control::loadNamedVector, TOPLEVEL::section::main, UNUSED, UNUSED, ch, UNUSED, UNUSED, textMsgBuffer.push(name));
            return Reply::DONE;
        }
        if (input.matchnMove(2, "state"))
        {
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            string name;
            if (input.nextChar('@'))
            {
                input.skip(1);
                input.skipSpace();
                int tmp = string2int(input);
                if (tmp <= 0)
                    return Reply{REPLY::value_msg};
                name = historySelect(4, tmp - 1);
                if (name == "")
                    return Reply::DONE;
            }
            else
            {
                name = string{input};
                if (name == "")
                        return Reply{REPLY::name_msg};
            }
            sendDirect(synth, TOPLEVEL::action::muteAndLoop, 0, TOPLEVEL::type::Write, MAIN::control::loadNamedState, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(name));
            return Reply::DONE;
        }
        if (input.matchnMove(2, "scale"))
        {
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            string name;
            if (input.nextChar('@'))
            {
                input.skip(1);
                input.skipSpace();
                int tmp = string2int(input);
                if (tmp <= 0)
                    return Reply{REPLY::value_msg};
                name = historySelect(3, tmp - 1);
                if (name == "")
                    return Reply::DONE;
            }
            else
            {
                name = string{input};
                if (name == "")
                    return Reply{REPLY::name_msg};
            }
            sendDirect(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, MAIN::control::loadNamedScale, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(name));
            return Reply::DONE;
        }
        if (input.matchnMove(1, "patchset"))
        {
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            string name;
            if (input.nextChar('@'))
            {
                input.skip(1);
                input.skipSpace();
                int tmp = string2int(input);
                if (tmp <= 0)
                    return Reply{REPLY::value_msg};
                name = historySelect(2, tmp - 1);
                if (name == "")
                    return Reply::DONE;
            }
            else
            {
                name = string{input};
                if (name == "")
                    return Reply{REPLY::name_msg};
            }
            sendDirect(synth, TOPLEVEL::action::muteAndLoop, 0, TOPLEVEL::type::Write, MAIN::control::loadNamedPatchset, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(name));
            return Reply::DONE;
        }
        if (input.matchnMove(1, "instrument"))
        {
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            string name;
            if (input.nextChar('@'))
            {
                input.skip(1);
                input.skipSpace();
                int tmp = string2int(input);
                if (tmp <= 0)
                    return Reply{REPLY::value_msg};
                name = historySelect(1, tmp - 1);
                if (name == "")
                    return Reply::DONE;
            }
            else
            {
                name = string{input};
                if (name == "")
                    return Reply{REPLY::name_msg};
            }

            sendDirect(synth, 0, 0, TOPLEVEL::type::Write, MAIN::control::loadInstrumentByName, TOPLEVEL::section::main, npart, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(name));
            return Reply::DONE;
        }
        if  (input.matchnMove(1, "default"))
        {
            if (bitFindHigh(context) == LEVEL::Part)
                return sendNormal(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, PART::control::defaultInstrumentCopyright, TOPLEVEL::section::part1 + npart, UNUSED, UNUSED, UNUSED, 0);
            else
            {
                synth->getRuntime().Log("Only available at part level");
                return Reply::DONE;
            }
        }
        return Reply::what("load");
    }

    if (input.matchnMove(2, "save"))
    {
        if (input.matchnMove(2, "mlearn"))
        {
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};

            sendNormal(synth, 0, 0, TOPLEVEL::type::Write, MIDILEARN::control::saveList, TOPLEVEL::section::midiLearn, 0, 0, 0, 0, UNUSED, textMsgBuffer.push(string{input}));
            return Reply::DONE;
        }
        if (input.matchnMove(2, "vector"))
        {
            int tmp = chan;
            if (input.matchnMove(1, "channel"))
            {
                tmp = string2int127(input) - 1;
                input.skipChars();
            }
            if (tmp >= NUM_MIDI_CHANNELS || tmp < 0)
                return Reply{REPLY::range_msg};
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            chan = tmp;
            sendDirect(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, MAIN::control::saveNamedVector, TOPLEVEL::section::main, UNUSED, UNUSED, chan, UNUSED, UNUSED, textMsgBuffer.push(string{input}));
            return Reply::DONE;
        }
        if (input.matchnMove(2, "state"))
        {
            if (input.isAtEnd())
                return Reply{REPLY::value_msg};
            sendDirect(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, MAIN::control::saveNamedState, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(string{input}));
            return Reply::DONE;
        }
        if (input.matchnMove(1, "config"))
        {
            sendDirect(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, CONFIG::control::saveCurrentConfig, TOPLEVEL::section::config, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push("DUMMY"));
            return Reply::DONE;
        }

        if (input.matchnMove(2, "scale"))
        {
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            sendDirect(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, MAIN::control::saveNamedScale, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(string{input}));
            return Reply::DONE;
        }
        else if (input.matchnMove(1, "patchset"))
        {
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            sendDirect(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, MAIN::control::saveNamedPatchset, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(string{input}));
            return Reply::DONE;
        }
        if (input.matchnMove(1, "instrument"))
        {
            if (readControlText(synth, TOPLEVEL::action::lowPrio, PART::control::instrumentName, TOPLEVEL::section::part1 + npart) == DEFAULT_NAME)
            {
                Runtime.Log("Nothing to save!");
                return Reply::DONE;
            }
            if (input.isAtEnd())
                return Reply{REPLY::name_msg};
            sendDirect(synth, TOPLEVEL::action::lowPrio, npart, TOPLEVEL::type::Write, MAIN::control::saveNamedInstrument, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, textMsgBuffer.push(string{input}));
            return Reply::DONE;
        }
        if  (input.matchnMove(1, "default"))
            return sendNormal(synth, TOPLEVEL::action::lowPrio, 0, TOPLEVEL::type::Write, PART::control::defaultInstrumentCopyright, TOPLEVEL::section::part1 + npart, UNUSED, UNUSED, UNUSED, 1);
        return Reply::what("save");
    }

    if (input.matchnMove(2, "zread"))
    {
        /*
         * This is a very specific test for reading values and is intended to measure
         * the time these calls take. For that reason the return echos to the CLI and
         * GUI are suppressed, and all results are sent to the CLI only.
         *
         * It is only the selection time we are measuring, and that the correct
         * value is returned.
         *
         * The limit to the number of repeats is INT max. Using high repeat numbers
         * reduces the effect of the processing overhead outside the call loop itself.
         */

        std::cout << "here zread" << std::endl;

        // repeats, control, part, kit, engine, insert, parameter, miscmsg
        float result;
        unsigned char control, part;
        unsigned char kit = UNUSED;
        unsigned char engine = UNUSED;
        unsigned char insert = UNUSED;
        unsigned char parameter = UNUSED;
        unsigned char miscmsg = UNUSED;
        int repeats;
        if (input.isAtEnd())
            return REPLY::value_msg;
        repeats = string2int(input);
        if (repeats < 1)
            repeats = 1;
        input.skipChars();
        if (input.isAtEnd())
            return REPLY::value_msg;
        control = string2int(input);
        input.skipChars();
        if (input.isAtEnd())
            return REPLY::value_msg;
        part = string2int(input);
        input.skipChars();
        if (!input.isAtEnd())
        {
            kit = string2int(input);
            input.skipChars();
            if (!input.isAtEnd())
            {
                engine = string2int(input);
                input.skipChars();
                if (!input.isAtEnd())
                {
                    insert = string2int(input);
                    input.skipChars();
                    if (!input.isAtEnd())
                    {
                        parameter = string2int(input);
                        input.skipChars();
                        if (!input.isAtEnd())
                            miscmsg = string2int(input);
                    }
                }
            }
        }

        CommandBlock putData;
        putData.data.value = 0;
        putData.data.control = control;
        putData.data.part = part;
        putData.data.kit = kit;
        putData.data.engine = engine;
        putData.data.insert = insert;
        putData.data.parameter = parameter;
        putData.data.miscmsg = miscmsg;
        putData.data.type = 0;
        putData.data.source = 0;
        struct timeval tv1, tv2;
        gettimeofday(&tv1, NULL);
        for (int i = 0; i < repeats; ++ i)
            result = synth->interchange.readAllData(&putData);
        gettimeofday(&tv2, NULL);

        if (tv1.tv_usec > tv2.tv_usec)
        {
            tv2.tv_sec--;
            tv2.tv_usec += 1000000;
            }
        float actual = (tv2.tv_sec - tv1.tv_sec) *1000000 + (tv2.tv_usec - tv1.tv_usec);
        std::cout << "result " << result << std::endl;
        std::cout << "Loops " << repeats << "  Total time " << actual << "uS" << "  average call time " << actual/repeats * 1000.0f << "nS" << std::endl;
        return REPLY::done_msg;
    }

    // legacyCLIaccess goes here

    return REPLY::unrecognised_msg;
}


}//(End)namespace cli
