/*
    MidiLearn.cpp

    Copyright 2016-2020, Will Godfrey
    Copyright 2021-2023, Will Godfrey, Rainer Hans Liffers

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


#include "Interface/MidiLearn.h"
#include "Interface/InterChange.h"
#include "Interface/RingBuffer.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/FormatFuncs.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"

#include <list>
#include <vector>
#include <string>
#include <thread>
#include <iostream>

#include <chrono>

using std::this_thread::sleep_for;
using std::chrono_literals::operator ""us;

using file::isRegularFile;
using file::make_legit_filename;
using file::setExtension;

using func::asString;
using func::asHexString;
using std::cout;
using std::endl;
using std::to_string;
using std::string;
using std::vector;
using std::list;

enum scan : int { noList = -3, listEnd, listBlocked};


namespace { // Implementation details...

    TextMsgBuffer& textMsgBuffer = TextMsgBuffer::instance();


    const int DECODE_MODE = 1;
    /*
     * 0 = use legacy numbers
     * 1 = decode from text
     * 2 = use legacy numbers report difference
     * 3 = decode from text report difference
     */
}


MidiLearn::MidiLearn(SynthEngine& synthInstance)
    : synth(synthInstance)
    , data{}
    , learning{false}
    , midi_list{}
    , learnedName{}
    , learnTransferBlock{}
    { }



void MidiLearn::setTransferBlock(CommandBlock& cmd)
{
    memcpy(learnTransferBlock.bytes, cmd.bytes, sizeof(learnTransferBlock));
    learnedName = resolveAll(synth, cmd, false);
    learning = true;
    synth.getRuntime().Log("Learning " + learnedName);
    updateGui(MIDILEARN::control::sendLearnMessage);
}


bool MidiLearn::runMidiLearn(int intVal, ushort CC, uchar chan, bool in_place)
{
    if (learning)
    {
        insertLine(CC, chan);
        return true; // block while learning
    }

    if (findSize() == 0)
        return false; // don't bother if there's no list!

    int lastpos = scan::listBlocked;
    LearnBlock foundEntry;
    bool firstLine = true;
    while (lastpos != scan::listEnd)
    {
        lastpos = findEntry(midi_list, lastpos, CC, chan, foundEntry, false);
        if (lastpos == scan::noList)
            return false;
        int status = foundEntry.status;
        if (status & 4) // it's muted
            continue;
/*
 * Some of the following conversions seem strange but are
 * needed to ensure a control range that is an exact
 * equivalent of 0 to 127 under all conditions
 */
        float value; // needs to be refetched each loop
        if (CC >= MIDI::CC::identNRPN || CC == MIDI::CC::pitchWheelAdjusted)
        {
            if (status & 16) // 7 bit NRPN
                value = float(intVal & 0x7f);
            else
                value = intVal / 128.999f; // convert from 14 bit
        }
        else if (CC != MIDI::CC::keyPressureAdjusted)
            value = float(intVal);
        else
            value = float(intVal >> 8);

        float minIn = foundEntry.min_in / 1.5748f;
        float maxIn = foundEntry.max_in / 1.5748f;
        if (minIn > maxIn)
        {
            value = 127 - value;
            std::swap(minIn, maxIn);
        }

        if (minIn == maxIn)
        {
            if (value <= minIn)
                value = 0;
            else
                value = 127;
        }
        else if (status & 2) // limit
        {
            if (value < minIn)
                value = minIn;
            else if (value > maxIn)
                value = maxIn;
        }
        else // compress
            value = ((maxIn - minIn) * value / 127.0f) + minIn;

        int minOut = foundEntry.min_out;
        int maxOut = foundEntry.max_out;
        if (maxOut - minOut != 127) // its a range change
            value = minOut +((maxOut - minOut) * value / 127.0f);
        else if (minOut != 0) // it's just a shift
            value += minOut;

        CommandBlock resultCmd;
        memcpy(resultCmd.bytes, foundEntry.frame.bytes, sizeof(CommandBlock));
        resultCmd.data.value  = value;
        resultCmd.data.source = TOPLEVEL::action::toAll;
        resultCmd.data.type   = TOPLEVEL::type::Write | (foundEntry.frame.data.type & TOPLEVEL::type::Integer);
                                // publish result command into MIDI stream with original integer / float type
        if (writeMidi(resultCmd, in_place))
        {
            if (firstLine and not in_place)
            {// we only want to send an activity once
             // and it's not relevant to jack freewheeling
                firstLine = false;
                if (CC >= MIDI::CC::identNRPN)
                    resultCmd.data.type |= 0x10; // mark as NRPN
                resultCmd.data.control = MIDILEARN::control::reportActivity;
                resultCmd.data.part    = TOPLEVEL::section::midiLearn;
                resultCmd.data.kit     = (CC & 0xff);
                resultCmd.data.engine  = chan;
                writeMidi(resultCmd, in_place);
            }
        }
        if (lastpos == scan::listBlocked) // blocking all of this CC/chan pair
            return true;
    }
    return false;
}


bool MidiLearn::writeMidi(CommandBlock& cmd, bool in_place)
{
    cmd.data.source |= TOPLEVEL::action::fromMIDI;
    uint tries{0};
    bool ok{true};
    if (in_place)
    {
        synth.interchange.commandSend(cmd);
        synth.interchange.returns(cmd);
    }
    else
    {
        do
        {
            ++ tries;
            ok = synth.interchange.fromMIDI.write(cmd.bytes);
            if (not ok)
                sleep_for(1us);
        // we can afford a short delay for buffer to clear
        }
        while (not ok and tries < 3);

        if (not ok)
            synth.getRuntime().Log("MidiLearn: congestion on MIDI->Engine");
    }
    return ok;
}


/*
 * This will only be called by incoming midi. It is the only function that
 * needs to be really quick
 */
int MidiLearn::findEntry(list<LearnBlock>& midi_list, int lastpos, ushort CC, uchar chan, LearnBlock& block, bool show)
{
    int newpos = 0; // 'last' comes in at listBlocked for the first call
    list<LearnBlock>::iterator it = midi_list.begin();

    while (newpos <= lastpos && it != midi_list.end())
    {
        ++ it;
        ++ newpos;
    }
    if (it == midi_list.end())
        return scan::noList;

    while ((CC != it->CC || (it->chan != 16 && chan != it->chan)) &&  it != midi_list.end())
    {
        ++ it;
        ++ newpos;
    }
    if (it == midi_list.end())
        return scan::noList;

    while (CC == it->CC && it != midi_list.end())
    {
        if ((it->chan >= 16 || chan == it->chan) && CC == it->CC)
        {
            if (show)
                synth.getRuntime().Log("Found line " + findName(it) + "  at " + to_string(newpos)); // a test!
            block.chan    = it->chan;
            block.CC      = it->CC;
            block.min_in  = it->min_in;
            block.max_in  = it->max_in;
            block.status  = it->status;
            block.min_out = it->min_out;
            block.max_out = it->max_out;
            block.frame.data = it->frame.data;
            if ((it->status & 5) == 1) // blocked, not muted
                return scan::listBlocked; // don't allow any more of this CC and channel;
            return newpos;
        }
        ++ it;
        ++ newpos;
    }
    return scan::listEnd;
}


int MidiLearn::findSize()
{
    return int(midi_list.size());
}


void MidiLearn::listLine(int lineNo)
{
    list<LearnBlock>::iterator it = midi_list.begin();
    int found = 0;
    if (midi_list.size() == 0)
    {
        synth.getRuntime().Log("No learned lines");
        return;
    }

    while (it != midi_list.end() && found < lineNo)
    {
        ++ it;
        ++ found;
    }
    if (it == midi_list.end())
    {
        synth.getRuntime().Log("No entry for number " + to_string(lineNo + 1));
        return;
    }
    else
    {
        int status = it->status;
        string mute = "";
        if (status & 4)
            mute = "  muted";
        string limit = "";
        if (status & 2)
            limit = "  limiting";
        string block = "";
        if (status & 1)
            block = "  blocking";
        string nrpn = "";
        if (status & 8)
        {
            nrpn = "  NRPN";
            if (status & 16)
                nrpn += " sevenBit";
        }
        string chan = "  Chan ";
        if ((it->chan) >= NUM_MIDI_CHANNELS)
            chan += "All";
        else
            chan += to_string(int(it->chan + 1));
        string  CCtype;
        int CC = it->CC;
        if (CC < 0xff)
            CCtype = to_string(CC);
        else
            CCtype = asHexString((CC >> 7) & 0x7f) + asHexString(CC & 0x7f) + " h";
        synth.getRuntime().Log("Line " + to_string(lineNo + 1) + mute
                + "  CC " + CCtype
                + chan
                + "  Min " + asString(float(it->min_in / 2.0f)) + "%"
                + "  Max " + asString(float(it->max_in / 2.0f)) + "%"
                + limit + block + nrpn + "  " + findName(it));
    }
}


void MidiLearn::listAll(list<string>& msg_buf)
{
    list<LearnBlock>::iterator it = midi_list.begin();
    int lineNo = 0;
    if (midi_list.size() == 0)
    {
        msg_buf.push_back("No learned lines");
        return;
    }
    string CCtype;
    int CC;
    msg_buf.push_back("Midi learned:");
    while (it != midi_list.end())
    {
        CC = it->CC;
        if (CC < 0xff)
            CCtype = to_string(CC);
        else
            CCtype = asHexString((CC >> 7) & 0x7f) + asHexString(CC & 0x7f) + " h";
        string chan = "  Chan ";
        if ((it->chan) >= NUM_MIDI_CHANNELS)
            chan += "All";
        else
            chan += to_string(int(it->chan + 1));

        msg_buf.push_back("Line " + to_string(lineNo + 1) + "  CC " + CCtype + chan + "  " + findName(it));
        ++ it;
        ++ lineNo;
    }
}


bool MidiLearn::remove(int itemNumber)
{
    list<LearnBlock>::iterator it = midi_list.begin();
    int found = 0;
    while (found < itemNumber && it != midi_list.end())
    {
        ++ found;
        ++ it;
    }
    if (it != midi_list.end())
    {
        midi_list.erase(it);
        return true;
    }
    return false;
}


void MidiLearn::generalOperations(CommandBlock& cmd)
{
    int  value      = cmd.data.value;
    uchar type      = cmd.data.type;
    uchar control   = cmd.data.control;
//  uchar part      = cmd.data.part;
    uint  kit       = cmd.data.kit; // may need to set as an NRPN
    uchar engine    = cmd.data.engine;
    uchar insert    = cmd.data.insert;
    uchar parameter = cmd.data.parameter;
    uchar offset    = cmd.data.offset;
    uchar par2      = cmd.data.miscmsg;

    if (control == MIDILEARN::control::sendRefreshRequest)
    {
        updateGui();
        return;
    }

    if (control == MIDILEARN::control::clearAll)
    {
        midi_list.clear();
        updateGui();
        synth.getRuntime().Log("List cleared");
        return;
    }

    string name;
    if (control == MIDILEARN::control::loadList)
    {
        name = (textMsgBuffer.fetch(par2));
        if (loadList(name))
        {
            updateGui();
            synth.getRuntime().Log("Loaded " + name);
        }
        synth.getRuntime().finishedCLI = true;
        return;
    }
    if (control == MIDILEARN::control::loadFromRecent)
    {
        int pos = 0;
        vector<string>& listtype{synth.getHistory(TOPLEVEL::XML::MLearn)};
        vector<string>::iterator it = listtype.begin();
        while (it != listtype.end() && pos != value)
        {
            ++ it;
            ++ pos;
        }
        if (it == listtype.end())
        {
            synth.getRuntime().Log("No entry for number " + to_string(int(value + 1)));
        }
        else
        {
            name = *it;
            if (loadList(name))
                synth.getRuntime().Log("Loaded " + name);
            updateGui();
        }
        synth.getRuntime().finishedCLI = true;
        return;
    }
    if (control == MIDILEARN::control::saveList)
    {
        name = (textMsgBuffer.fetch(par2));
        if (saveList(name))
            synth.getRuntime().Log("Saved " + name);
        synth.getRuntime().finishedCLI = true;
        return;
    }
    if (control == MIDILEARN::control::cancelLearn)
    {
        learning = false;
        synth.getRuntime().finishedCLI = true;
        synth.getRuntime().Log("Midi Learn cancelled");
        updateGui(MIDILEARN::control::cancelLearn);
        return;
    }

    // line controls
    LearnBlock entry;
    int lineNo = 0;
    list<LearnBlock>::iterator it = midi_list.begin();
    it = midi_list.begin();

    while (lineNo < value && it != midi_list.end())
    {
        ++ it;
        ++lineNo;
    }

    string lineName;
    if (it == midi_list.end())
    {
        synth.getRuntime().Log("Line " + to_string(lineNo + 1) + " not found");
        return;
    }

    if (insert == UNUSED) // don't change
        insert = it->min_in;
    else
        lineName = "Min = " + asString(float(insert / 2.0f)) + "%";

    if (parameter == UNUSED)
        parameter = it->max_in;
    else
        lineName = "Max = " + asString(float(parameter / 2.0f)) + "%";

    if (kit == UNUSED || it->CC > 0xff) // might be an NRPN
        kit = it->CC; // remember NRPN has a high bit set
    else
        lineName = "CC = " + to_string(int(kit));

    if (engine == UNUSED)
        engine = it->chan;
    else
    {
        if (engine == 16)
            lineName = "Chan = All";
        else
            lineName = "Chan = " + to_string(int(engine) + 1);
    }

    if (control == MIDILEARN::control::CCorChannel)
    {
        bool moveLine = true;
        list<LearnBlock>::iterator nextit = it;
        list<LearnBlock>::iterator lastit = it;
        ++ nextit;
        -- lastit;
        if (it == midi_list.begin() && nextit->CC >= kit)
        {
            if (nextit->CC > kit || nextit->chan >= engine)
                moveLine = false;
        }
        else if (nextit == midi_list.end() && lastit->CC <= kit)
        {
            if (lastit->CC < kit || lastit->chan <= engine)
                moveLine = false;
        }

        // here be dragons :(
        else if (kit > it->CC)
        {
            if (nextit->CC > kit)
                moveLine = false;
        }
        else if (kit < it->CC)
        {
            if (lastit->CC < kit)
                moveLine = false;
        }
        else if (engine > it->chan)
        {
            if (nextit->CC > kit || nextit->chan >= engine)
                moveLine = false;
        }
        else if (engine < it->chan)
        {
            if (lastit->CC < kit || lastit->chan <= engine)
                moveLine = false;
        }

        if (!moveLine)
            control = MIDILEARN::control::ignoreMove; // change this as we're not moving the line
    }

    if (control == MIDILEARN::control::deleteLine)
    {
        remove(value);
        updateGui();
        synth.getRuntime().Log("Removed line " + to_string(int(value + 1)));
        return;
    }

    if (control < MIDILEARN::control::deleteLine)
    {
        if (control > MIDILEARN::control::sevenBit)
        {
            type = it->status;
            synth.getRuntime().Log("Line " + to_string(lineNo + 1) + " " + lineName);
        }
        else{
            uchar tempType = it->status;
            bool isOn = (type & 0x1f) > 0;
            string name;
            switch (control)
            {
                case MIDILEARN::control::block:
                    type = (tempType & 0xfe) | (type & 1);
                    name = "Block";
                    break;
                case MIDILEARN::control::limit:
                    type = (tempType & 0xfd) | (type & 2);
                    name = "Limit";
                    break;
                case MIDILEARN::control::mute:
                    type = (tempType & 0xfb) | (type & 4);
                    name = "Mute";
                    break;
                case MIDILEARN::control::sevenBit:
                    type = (tempType & 0xef) | (type & 16);
                    name = "7bit";
                    break;
            }
            if (isOn)
                name += " enabled";
            else
                name += " disabled";
            synth.getRuntime().Log("Line " + to_string(lineNo + 1) + " " + name);
        }

        CommandBlock response;
        memset(&response.bytes, 255, sizeof(response));
        // need to work on this more
        response.data.value     = value;
        response.data.type      = type;
        response.data.control   = MIDILEARN::control::ignoreMove;
        response.data.kit       = kit;
        response.data.engine    = engine;
        response.data.insert    = insert;
        response.data.parameter = parameter;
        response.data.offset    = offset;
        it->CC = kit;
        it->chan = engine;
        it->min_in = insert;
        it->max_in = parameter;
        it->status = type;
        writeToGui(response);
        return;
    }

    if (control == MIDILEARN::control::CCorChannel)
    {
        entry.CC = kit;
        entry.chan = engine;
        entry.min_in = insert;
        entry.max_in = parameter;
        entry.status = type;
        entry.min_out = it->min_out;
        entry.max_out = it->max_out;
        entry.frame.data = it->frame.data;
        uint CC = entry.CC;
        int chan = entry.chan;

        midi_list.erase(it);

        it = midi_list.begin();
        int lineNo = 0;
        if (midi_list.size() > 0)
        { // CC is priority
            while (CC > it->CC && it != midi_list.end())
            { // find start of group
                ++it;
                ++lineNo;
            }
            while (CC == it->CC && chan >= it->chan && it != midi_list.end())
            { // insert at end of same channel
                ++it;
                ++lineNo;
            }
        }

        if (it == midi_list.end())
            midi_list.push_back(entry);
        else
            midi_list.insert(it, entry);

        synth.getRuntime().Log("Moved line to " + to_string(lineNo + 1) + " " + lineName);
        updateGui();
        return;
    }
    // there may be more later!
}


string MidiLearn::findName(list<LearnBlock>::iterator it)
{
    CommandBlock cmd;
    memcpy(cmd.bytes, it->frame.bytes, sizeof(CommandBlock));
    cmd.data.value = 0;
    cmd.data.source = 0;
    return resolveAll(synth, cmd, false);
}


void MidiLearn::insertLine(ushort CC, uchar chan)
{
    /*
     * This will eventually be part of a paging system of
     * 128 lines for the Gui.
     */
    if (midi_list.size() >= MIDI_LEARN_BLOCK)
    {
        CommandBlock cmd;
        memset(&cmd, 0xff, sizeof(cmd));
        cmd.data.value     = 0;
        cmd.data.source    = TOPLEVEL::action::toAll;
        cmd.data.type      = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
        cmd.data.control   = TOPLEVEL::control::textMessage;
        cmd.data.part      = TOPLEVEL::section::midiIn;
        cmd.data.parameter = 0x80;
        cmd.data.miscmsg   = textMsgBuffer.push("Midi Learn full!");
        writeMidi(cmd, false);
        learning = false;
        return;
    }

    uchar status{0};
    if (CC >= MIDI::CC::channelPressureAdjusted)
        status |= 1; // set 'block'
    if (CC >= MIDI::CC::identNRPN)
        status |= 8; // mark as NRPN
    LearnBlock entry;
    entry.chan = chan;
    entry.CC = CC;
    entry.min_in = 0;
    entry.max_in = 200;
    entry.status = status;

    uchar type = learnTransferBlock.data.type & TOPLEVEL::type::Integer;
    learnTransferBlock.data.type = (type | TOPLEVEL::type::Limits | TOPLEVEL::type::Minimum);
    entry.min_out = synth.interchange.readAllData(learnTransferBlock);
    learnTransferBlock.data.type = (type | TOPLEVEL::type::Limits | TOPLEVEL::type::Maximum);
    entry.max_out = synth.interchange.readAllData(learnTransferBlock);

    memcpy(entry.frame.bytes, learnTransferBlock.bytes, sizeof(CommandBlock));
    entry.frame.data.type = type;
    list<LearnBlock>::iterator it;
    it = midi_list.begin();
    int lineNo = 0;
    if (midi_list.size() > 0)
    { // CC is priority
        while (CC > it->CC && it != midi_list.end()) // CC is priority
        { // find start of group
            ++it;
            ++lineNo;
        }
        while (CC == it->CC && chan >= it->chan && it != midi_list.end())
        { // insert at end of same channel
            ++it;
            ++lineNo;
        }
    }
    if (it == midi_list.end())
        midi_list.push_back(entry);
    else
        midi_list.insert(it, entry);

    uint CCh = entry.CC;
    string CCtype;
    if (CCh < 0xff)
        CCtype = "CC " + to_string(CCh);
    else
        CCtype = "NRPN " + asHexString((CCh >> 7) & 0x7f) + " " + asHexString(CCh & 0x7f);
    synth.getRuntime().Log("Learned " + CCtype + "  Chan " + to_string((int)entry.chan + 1) + "  " + learnedName);
    updateGui(MIDILEARN::control::limit);
    learning = false;
}


void MidiLearn::writeToGui(CommandBlock& cmd)
{
#ifdef GUI_FLTK
    if (!synth.getRuntime().showGui)
        return;
    cmd.data.part = TOPLEVEL::section::midiLearn;
    int tries = 0;
    bool ok = false;
    do
    {
        ok = synth.interchange.toGUI.write(cmd.bytes);
        ++tries;
        if (!ok)
                sleep_for(100us);
        // we can afford a short delay for buffer to clear
    }
    while (!ok && tries < 3);

    if (!ok)
        synth.getRuntime().Log("toGui buffer full!", _SYS_::LogNotSerious | _SYS_::LogError);
#endif // GUI_FLTK
}


void MidiLearn::updateGui(int opp)
{
    if (!synth.getRuntime().showGui)
        return;
    CommandBlock cmd;
    if (opp == MIDILEARN::control::sendLearnMessage)
    {
        cmd.data.control = MIDILEARN::control::sendLearnMessage;
        cmd.data.miscmsg = textMsgBuffer.push("Learning " + learnedName);
    }
    else if (opp == MIDILEARN::control::cancelLearn)
    {
        cmd.data.control = MIDILEARN::control::cancelLearn;
        cmd.data.miscmsg = NO_MSG;
    }
    else if (opp == MIDILEARN::control::limit)
    {
        cmd.data.control = TOPLEVEL::control::textMessage;
        cmd.data.miscmsg = NO_MSG;
    }
    else
    {
        cmd.data.control = MIDILEARN::control::clearAll;
        cmd.data.miscmsg = NO_MSG;
        if (opp == MIDILEARN::control::hideGUI)
            return;
    }
    cmd.data.value = 0;
    writeToGui(cmd);

    if (opp >= MIDILEARN::control::hideGUI) // just sending back gui message
        return;

    int lineNo = 0;
    list<LearnBlock>::iterator it;
    it = midi_list.begin();
/*
using std::chrono::steady_clock;
using Dur = std::chrono::duration<double, std::micro>;
auto start = steady_clock::now();
*/
    while (it != midi_list.end())
    {
        ushort newCC = (it->CC) & MIDI::CC::maxNRPN;
        cmd.data.value     = lineNo;
        cmd.data.type      = it->status;
        cmd.data.source    = TOPLEVEL::action::toAll;
        cmd.data.control   = MIDILEARN::control::CCorChannel;
        cmd.data.kit       = (newCC & 0xff);
        cmd.data.engine    = it->chan;
        cmd.data.insert    = it->min_in;
        cmd.data.parameter = it->max_in;
        cmd.data.miscmsg   = textMsgBuffer.push(findName(it));
        writeToGui(cmd);
        if (it->status & 8)
        { // status used in case NRPN is < 0x100
            cmd.data.control = MIDILEARN::control::nrpnDetected; // it's an NRPN
            cmd.data.engine = (newCC >> 8);
            writeToGui(cmd);
        }
        ++it;
        ++lineNo;
        if (lineNo & 32)
            sleep_for(10us); // allow message list to clear a bit
    }
/*
Dur duration = steady_clock::now () - start;
std::cout << "MidiLearn->GUI: Δt = " << duration.count() << "µs" << std::endl;
*/
    if (synth.getRuntime().showLearnedCC == true && !midi_list.empty()) // open the gui editing window
    {
        cmd.data.control = MIDILEARN::control::sendRefreshRequest;
        writeToGui(cmd);
    }
}


bool MidiLearn::saveList(string const& name)
{
    if (name.empty())
    {
        synth.getRuntime().Log("No filename");
        return false;
    }

    if (midi_list.size() == 0)
    {
        synth.getRuntime().Log("No Midi Learn list");
        return false;
    }

    string file = setExtension(name, EXTEN::mlearn);
    synth.getRuntime().xmlType = TOPLEVEL::XML::MLearn;
    auto xml{std::make_unique<XMLwrapper>(synth, true)};
    bool ok = insertMidiListData(*xml);
    if (xml->saveXMLfile(file))
        synth.addHistory(file, TOPLEVEL::XML::MLearn);
    else
    {
        synth.getRuntime().Log("Failed to save data to " + file);
        ok = false;
    }
    return ok;
}


bool MidiLearn::insertMidiListData(XMLwrapper& xml)
{
    if (midi_list.size() == 0)
        return false;
    int ID = 0;
    list<LearnBlock>::iterator it;
    it = midi_list.begin();
    xml.beginbranch("MIDILEARN");
        while (it != midi_list.end())
        {
            xml.beginbranch("LINE", ID);
            xml.addparbool("Mute", (it->status & 4) > 0);
            xml.addparbool("NRPN", (it->status & 8) > 0);
            xml.addparbool("7_bit", (it->status & 16) > 0);
            xml.addpar("Midi_Controller", it->CC & 0x7fff);
            /*
             * Clear out top bit - NRPN marker
             * Yoshimi NRPNs are internally stored as
             * integers in 'CC', not MIDI 14 bit pairs.
             * A high bit marker is added to identify these.
             * For user display they are split and shown as
             * MSB and LSB.
             */
            xml.addpar("Midi_Channel",  it->chan);
            xml.addparreal("Midi_Min",  it->min_in / 1.575f);
            xml.addparreal("Midi_Max",  it->max_in / 1.575f);
            xml.addparbool("Limit",    (it->status & 2) > 0);
            xml.addparbool("Block",    (it->status & 1) > 0);
            xml.addpar("Convert_Min",   it->min_out);
            xml.addpar("Convert_Max",   it->max_out);
            xml.beginbranch("COMMAND");
                xml.addpar("Type",      it->frame.data.type);
                xml.addpar("Control",   it->frame.data.control);
                xml.addpar("Part",      it->frame.data.part);
                xml.addpar("Kit_Item",  it->frame.data.kit);
                xml.addpar("Engine",    it->frame.data.engine);
                xml.addpar("Insert",    it->frame.data.insert);
                xml.addpar("Parameter", it->frame.data.parameter);
                xml.addpar("Secondary_Parameter", it->frame.data.offset);
                xml.addparstr("Command_Name",findName(it));
                xml.endbranch();
            xml.endbranch();
            ++it;
            ++ID;
        }
    xml.endbranch(); // MIDILEARN
    return true;
}


bool MidiLearn::loadList(string const& name)
{
    if (name.empty())
    {
        synth.getRuntime().Log("No filename");
        return false;
    }
    string file{setExtension(name, EXTEN::mlearn)};
    if (!isRegularFile(file))
    {
        synth.getRuntime().Log("Can't find " + file);
        return false;
    }

    auto xml{std::make_unique<XMLwrapper>(synth, true)};
    xml->loadXMLfile(file);
    bool ok = extractMidiListData(true,  *xml);
    if (!ok)
        return false;
    synth.addHistory(file, TOPLEVEL::XML::MLearn);
    return true;
}


bool MidiLearn::extractMidiListData(bool full,  XMLwrapper& xml)
{
    midi_list.clear();
    if (!xml.enterbranch("MIDILEARN"))
    {
        if (full)
            synth.getRuntime().Log("Extract Data, no MIDILEARN branch");
        return false;
    }
    LearnBlock entry;
    CommandBlock cmd;
    midi_list.clear();
    int ID = 0;
    int status;
    uint ident;
    while (true)
    {
        status = 0;
        ident = 0;
        if (!xml.enterbranch("LINE", ID))
            break;
        else
        {
            if (xml.getparbool("Mute", 0))
                status |= 4;
            if (xml.getparbool("NRPN", 0))
            {
                ident = MIDI::CC::identNRPN; // set top bit for NRPN indication
                status |= 8;
            }
            if (xml.getparbool("7_bit",0))
                status |= 16;

            entry.CC = ident | xml.getpar("Midi_Controller", 0, 0, MIDI::CC::maxNRPN);

            entry.chan = xml.getpar127("Midi_Channel", 0);

            int min = int((xml.getparreal("Midi_Min", 200.0f) * 1.575f) + 0.1f);
            entry.min_in = min;

            int max = int((xml.getparreal("Midi_Max", 200.0f) * 1.575f) + 0.1f);
            entry.max_in = max;

            if (xml.getparbool("Limit",0))
                status |= 2;
            if (xml.getparbool("Block",0))
                status |= 1;
            entry.min_out = xml.getpar("Convert_Min", 0, -16384, 16383);
            entry.max_out = xml.getpar("Convert_Max", 0, -16384, 16383);
            xml.enterbranch("COMMAND");
                entry.frame.data.type      = xml.getpar255("Type", 0); // ??
                entry.frame.data.control   = xml.getpar255("Control", 0);
                entry.frame.data.part      = xml.getpar255("Part", 0);
                entry.frame.data.kit       = xml.getpar255("Kit_Item", 0);
                entry.frame.data.engine    = xml.getpar255("Engine", 0);
                entry.frame.data.insert    = xml.getpar255("Insert", 0);
                entry.frame.data.parameter = xml.getpar255("Parameter", 0);
                entry.frame.data.offset    = xml.getpar255("Secondary_Parameter", 0);

if (DECODE_MODE)
{
                string test = xml.getparstr("Command_Name");
                TextData::encodeAll(&synth, test, cmd);
}
if (DECODE_MODE >= 2)
{
                bool ok = true;
                if (ID == 0)
                    cout << endl;
                cout << "line " << (ID + 1);
                if (cmd.data.control != entry.frame.data.control)
                {
                    ok = false;
                    cout << " changed control Old " << int(entry.frame.data.control) << " > New " << int(cmd.data.control);
                }
                if (cmd.data.part != entry.frame.data.part)
                {
                    ok = false;
                    cout << " changed part Old " << int(entry.frame.data.part) << " > New " << int(cmd.data.part);
                }
                if (cmd.data.kit != entry.frame.data.kit)
                {
                    ok = false;
                    cout << " changed kit Old " << int(entry.frame.data.kit) << " > New " << int(cmd.data.kit);
                }
                if (cmd.data.engine != entry.frame.data.engine)
                {
                    ok = false;
                    cout << " changed engine Old " << int(entry.frame.data.engine) << " > New " << int(cmd.data.engine);
                }
                if (cmd.data.insert != entry.frame.data.insert)
                {
                    ok = false;
                    cout << " changed insert Old " << int(entry.frame.data.insert) << " > New " << int(cmd.data.insert);
                }
                if (cmd.data.parameter != entry.frame.data.parameter)
                {
                    ok = false;
                    cout << " changed parameter Old " << int(entry.frame.data.parameter) << " > New " << int(cmd.data.parameter);
                }
                if (cmd.data.offset != entry.frame.data.offset)
                {
                    ok = false;
                    cout << " changed offset Old " << int(entry.frame.data.offset) << " > " << int(cmd.data.offset);
                }
                if (ok)
                {
                    cout << " OK";
                }
                cout << endl;
}
if (DECODE_MODE & 1)
{
                entry.frame.data.control   = cmd.data.control;
                entry.frame.data.part      = cmd.data.part;
                entry.frame.data.kit       = cmd.data.kit;
                entry.frame.data.engine    = cmd.data.engine;
                entry.frame.data.insert    = cmd.data.insert;
                entry.frame.data.parameter = cmd.data.parameter;
                entry.frame.data.offset    = cmd.data.offset;
}
                xml.exitbranch();
            xml.exitbranch();
            entry.status = status;
            midi_list.push_back(entry);
            ++ ID;
        }
    }
    xml.exitbranch(); // MIDILEARN
    return true;
}

