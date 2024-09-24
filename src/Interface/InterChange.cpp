/*
    InterChange.cpp - General communications

    Copyright 2016-2019, Will Godfrey & others
    Copyright 2020-2020, Kristian Amlie, Will Godfrey, & others
    Copyright 2021, Will Godfrey, Rainer Hans Liffers, & others
    Copyright 2023 - 2024, Will Godfrey, Ichthyostega & others

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
#include <algorithm>
#include <string>
#include <cfloat>
#include <bitset>
#include <thread>
#include <atomic>

#include "Interface/InterChange.h"
#include "Interface/Vectors.h"
#include "Interface/Data2Text.h"
#include "Interface/TextLists.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/NumericFuncs.h"
#include "Misc/FormatFuncs.h"
#include "Misc/Microtonal.h"
#include "Misc/SynthEngine.h"
#include "Misc/Part.h"
#include "Misc/TextMsgBuffer.h"
#include "Params/UnifiedPresets.h"
#include "Params/Controller.h"
#include "Params/ADnoteParameters.h"
#include "Params/SUBnoteParameters.h"
#include "Params/PADnoteParameters.h"
#include "Params/PADStatus.h"
#include "Params/LFOParams.h"
#include "Params/FilterParams.h"
#include "Params/EnvelopeParams.h"
#include "Effects/EffectMgr.h"
#include "DSP/FFTwrapper.h"
#include "Synth/Resonance.h"
#include "Synth/OscilGen.h"
#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif

enum envControl: uchar {
    input,
    undo,
    redo
};

using std::this_thread::sleep_for;
using std::chrono_literals::operator ""us;
using std::chrono_literals::operator ""ms;

using std::string;
using std::to_string;
using file::localPath;
using file::findFile;
using file::isRegularFile;
using file::createDir;
using file::listDir;
using file::isDirectory;
using file::setExtension;
using file::findLeafName;
using file::createEmptyFile;
using file::deleteFile;
using file::loadText;
using file::saveText;

using func::bitSet;
using func::bitClear;
using func::nearestPowerOf2;

using func::asString;



InterChange::InterChange(SynthEngine& synthInstance)
    : synth(synthInstance),
#ifndef YOSHIMI_LV2_PLUGIN
    fromCLI(),
#endif
    decodeLoopback(),
#ifdef GUI_FLTK
    fromGUI(),
    toGUI(),
#endif
    fromMIDI(),
    returnsBuffer(),
    muteQueue(),
#ifdef GUI_FLTK
    guiDataExchange{[this](CommandBlock const& block){ toGUI.write(block.bytes); }},
#else
    guiDataExchange{[](CommandBlock const&){ /* no communication GUI */ }},
#endif
    syncWrite(false),
    lowPrioWrite(false),
    sortResultsThreadHandle(0),
    swapRoot1(UNUSED),
    swapBank1(UNUSED),
    swapInstrument1(UNUSED),
    searchInst(0),
    searchBank(0),
    searchRoot(0)
{
    noteSeen = false;
    undoLoopBack = false;
    setUndo = false;
    setRedo = false;
    undoStart = false;
    cameFrom = envControl::input;
    undoMarker.data.part = TOPLEVEL::section::undoMark;

    sem_init(&sortResultsThreadSemaphore, 0, 0);
}


bool InterChange::Init()
{
#ifndef YOSHIMI_LV2_PLUGIN
    fromCLI.init ();
#endif
    decodeLoopback.init ();
#ifdef GUI_FLTK
    fromGUI.init ();
    toGUI.init ();
#endif
    fromMIDI.init ();
    returnsBuffer.init ();
    muteQueue.init ();
    if (!synth.getRuntime().startThread(&sortResultsThreadHandle, _sortResultsThread, this, false, 0, "CLI"))
    {
        synth.getRuntime().Log("Failed to start CLI resolve thread");
        return false;
    }
    else
    {
        searchInst = searchBank = searchRoot = 0;
        return true;
    }
}

#ifdef GUI_FLTK
MasterUI& InterChange::createGuiMaster()
{
    CommandBlock bootstrapMsg;
    while (toGUI.read(bootstrapMsg.bytes))
        if (guiDataExchange.isValidPushMsg(bootstrapMsg))
        {
            size_t slotIDX = bootstrapMsg.data.offset;
            guiMaster.reset(new MasterUI(*this, slotIDX));
            assert(guiMaster);
            return *guiMaster;
        }
    throw std::logic_error("Instance Lifecycle broken: expect bootstrap message.");
    // Explanation: after a suitable MusicIO backend has been established, the SynthEngine::Init()
    //              will initialise the InterChange for this Synth and then prime the toGUI ringbuffer
    //              with a key for the GuiDataExchange to pass an InterfaceAnchor record up into the GUI.
    //              See SynthEngine::publishGuiAnchor() and InstanceManager::SynthGroom::dutyCycle();
    //              this bootstrap record provides connection IDs used by various UI components to
    //              receive push-updates from the Core and is thus embedded directly into MasterUI.
}

void InterChange::shutdownGui()
{
    guiMaster.reset();
}
#endif


void InterChange::spinSortResultsThread()
{
    sem_post(&sortResultsThreadSemaphore);
}

void* InterChange::_sortResultsThread(void* arg)
{
    return static_cast<InterChange*>(arg)->sortResultsThread();
}


void* InterChange::sortResultsThread()
{
    while (synth.getRuntime().runSynth.load(std::memory_order_relaxed))
    {
        CommandBlock cmd;

        /* It is possible that several operations initiated from
         * different sources complete within the same period
         * (especially with large buffer sizes) so this small
         * ring buffer ensures they can all clear together.
         */
        while (synth.audioOut.load() == _SYS_::mute::Active)
        {
            if (muteQueue.read(cmd.bytes))
                indirectTransfers(cmd);
            else
                synth.audioOut.store(_SYS_::mute::Complete);
        }

        while (decodeLoopback.read(cmd.bytes))
        {
            if (cmd.data.part == TOPLEVEL::section::midiLearn)
                synth.midilearn.generalOperations(cmd);
            else if (cmd.data.source >= TOPLEVEL::action::lowPrio)
                indirectTransfers(cmd);
            else
                resolveReplies(cmd);
        }

        sem_wait(&sortResultsThreadSemaphore);
    }
    return nullptr;
}


InterChange::~InterChange()
{
    if (sortResultsThreadHandle)
    {
        // Get it to quit.
        spinSortResultsThread();
        pthread_join(sortResultsThreadHandle, 0);
    }
    undoRedoClear();

    sem_destroy(&sortResultsThreadSemaphore);
}


void InterChange::Log(string const& msg)
{
    bool isError{true};
    synth.getRuntime().Log(msg, isError);
}


void InterChange::muteQueueWrite(CommandBlock& cmd)
{
    if (!muteQueue.write(cmd.bytes))
    {
        Log("failed to write to muteQueue");
        return;
    }
    if (synth.audioOut.load() == _SYS_::mute::Idle)
    {
        synth.audioOutStore(_SYS_::mute::Pending);
    }
}


void InterChange::indirectTransfers(CommandBlock& cmd, bool noForward)
{
    int value = lrint(cmd.data.value);
    float valuef = -1;
    uchar type      = cmd.data.type;
    uchar control   = cmd.data.control;
    uchar switchNum = cmd.data.part;
    uchar kititem   = cmd.data.kit;
    uchar engine    = cmd.data.engine;
    uchar insert    = cmd.data.insert;
//synth.CBtest(cmd);

    while (syncWrite)
        sleep_for(10us);
    bool write = (type & TOPLEVEL::type::Write);
    if (write)
        lowPrioWrite = true;
    bool guiTo = false;
    (void) guiTo; // suppress warning when headless build
    uchar newMsg = false;//NO_MSG;

    if (control == TOPLEVEL::control::copyPaste)
    {
        string name = UnifiedPresets{synth, cmd}
                                    .handleStoreLoad();
        /*
         * for Paste (load) 'name' is the type of the preset being loaded
         * for List 'name' lists all the stored presets of the wanted preset type
         *  alternatively it is the group type
         * */
        if (type == TOPLEVEL::type::List)
        {
            cmd.data.value = textMsgBuffer.push(name);
        }
        else if (engine == PART::engine::padSynth && type == TOPLEVEL::type::Paste && insert != UNUSED)
        {
            int localKit = kititem;
            if (localKit >= NUM_KIT_ITEMS)//not part->Pkitmode)
                localKit = 0;
            synth.part[switchNum]->kit[localKit].padpars->buildNewWavetable((cmd.data.parameter == 0));
        }
#ifdef GUI_FLTK
        toGUI.write(cmd.bytes);
#endif
        return; // currently only sending this to the GUI
    }

    if (switchNum == TOPLEVEL::section::main && control == MAIN::control::loadFileFromList)
    {
        int result = synth.LoadNumbered(kititem, engine);
        if (result > NO_MSG)
            cmd.data.miscmsg = result & NO_MSG;
        else
        {
            cmd.data.miscmsg = result;
            switch (kititem) // group
            {
                case TOPLEVEL::XML::Instrument:
                {
                    control = MAIN::control::loadInstrumentByName;
                    cmd.data.kit = insert;
                    break;
                }
                case TOPLEVEL::XML::Patch:
                {
                    control = MAIN::control::loadNamedPatchset;
                    break;
                }
                case TOPLEVEL::XML::Scale:
                {
                    control = MAIN::control::loadNamedScale;
                    break;
                }
                case TOPLEVEL::XML::State:
                {
                    control = MAIN::control::loadNamedState;
                    break;
                }
                case TOPLEVEL::XML::Vector:
                {
                    control = MAIN::control::loadNamedVector;
                    break;
                }
                case TOPLEVEL::XML::MLearn:
                { // this is a bit messy MIDI learn is an edge case
                    cmd.data.control = MIDILEARN::control::loadList;
                    synth.midilearn.generalOperations(cmd);
                    lowPrioWrite = false;
                    return;
                    break;
                }
            }
            cmd.data.control = control;
        }
    }

    string text;
    if (cmd.data.miscmsg != NO_MSG)
    {
        text = textMsgBuffer.fetch(cmd.data.miscmsg);
        cmd.data.miscmsg = NO_MSG; // this may be reset later
    }
    else
        text = "";

    if (control == TOPLEVEL::control::textMessage)
        switchNum = TOPLEVEL::section::message; // this is a bit hacky :(

    switch(switchNum)
    {
        case TOPLEVEL::section::vector:
            value = indirectVector(cmd, newMsg, guiTo, text);
            break;
        case TOPLEVEL::section::midiLearn:
            if (control == MIDILEARN::control::findSize)
                value = synth.midilearn.findSize();
            // very naughty! should do better
            break;
        case TOPLEVEL::section::midiIn: // program / bank / root
            value = indirectMidi(cmd, newMsg, guiTo, text);
            break;
        case TOPLEVEL::section::scales:
            value = indirectScales(cmd, newMsg, guiTo, text);
            break;

        case TOPLEVEL::section::main:
            value = indirectMain(cmd, newMsg, guiTo, text, valuef);
            break;

        case TOPLEVEL::section::bank: // instrument / bank
            value = indirectBank(cmd, newMsg, guiTo, text);
            break;

        case TOPLEVEL::section::config:
            value = indirectConfig(cmd, newMsg, guiTo, text);
            break;
        case TOPLEVEL::section::guideLocation:
        {
            string man{synth.getRuntime().manualFile};
            if (!man.empty())
            {
                size_t pos = man.find("files");
                man = man.substr(0,pos);
            }
            else
                man = "Can't find guide";
            value = textMsgBuffer.push(man);
            noForward = true;
            break;
        }
        case TOPLEVEL::section::message:
            newMsg = true;
            cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            break;
        default:
            if (switchNum < NUM_MIDI_PARTS)
            {
                value = indirectPart(cmd, newMsg, guiTo, text);
            }
            break;
    }

     // CLI message text has to be set here.
    if (!synth.fileCompatible)
        text += "\nIncompatible file from ZynAddSubFX 3.x";

    if (newMsg)
        value = textMsgBuffer.push(text);
    // TODO need to improve message handling for multiple receivers

    if (valuef > -1)
        cmd.data.value = valuef; // curently only master fine detune
    else
        cmd.data.value = float(value);
    if (write)
        lowPrioWrite = false;
    if (noForward)
        return;


    if (cmd.data.source < TOPLEVEL::action::lowPrio)
    {
#ifdef GUI_FLTK
        if (not text.empty() and synth.getRuntime().showGui and (write or guiTo))
        {
            cmd.data.miscmsg = textMsgBuffer.push(text); // pass it on to GUI
        }
#endif
        bool ok = returnsBuffer.write(cmd.bytes);
#ifdef GUI_FLTK
        if (synth.getRuntime().showGui)
        {
            if (switchNum == TOPLEVEL::section::scales && control == SCALES::control::importScl)
            {   // loading a tuning includes a name and comment!
                cmd.data.control = SCALES::control::name;
                cmd.data.miscmsg = textMsgBuffer.push(synth.microtonal.Pname);
                returnsBuffer.write(cmd.bytes);

                cmd.data.control = SCALES::control::comment;
                cmd.data.miscmsg = textMsgBuffer.push(synth.microtonal.Pcomment);
                ok &= returnsBuffer.write(cmd.bytes);
            }

            if (switchNum == TOPLEVEL::section::main && control == MAIN::control::loadNamedState)
                synth.midilearn.updateGui();
                /*
                 * This needs improving. We should only set it
                 * when the state file contains a learn list.
                 */
        }
#endif
        if (not ok)
            synth.getRuntime().Log("Unable to  write to returnsBuffer buffer");

        // cancelling and GUI report must be set after action completed.
        if (not synth.fileCompatible)
        {
            synth.fileCompatible = true;
#ifdef GUI_FLTK
            if (synth.getRuntime().showGui and
               (cmd.data.source & TOPLEVEL::action::noAction) == TOPLEVEL::action::fromGUI)
            {
                cmd.data.control = TOPLEVEL::control::textMessage;
                cmd.data.miscmsg = textMsgBuffer.push("File from ZynAddSubFX 3.0 or later may have parameter types incompatible with earlier versions, and with Yoshimi so might perform strangely.");
                returnsBuffer.write(cmd.bytes);
            }
#endif
        }
    }
    else // don't leave this hanging
    {
        synth.fileCompatible = true;
    }
}


int InterChange::indirectVector(CommandBlock& cmd, uchar& newMsg, bool& guiTo, string& text)
{
    bool write = (cmd.data.type & TOPLEVEL::type::Write);
    int value     = cmd.data.value;
    int control   = cmd.data.control;
    int parameter = cmd.data.parameter;

    switch(control)
    {
        case VECTOR::control::name:
            if (write)
                synth.getRuntime().vectordata.Name[parameter] = text;
            else
                text = synth.getRuntime().vectordata.Name[parameter];
            newMsg = true;
            cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            guiTo = true;
            break;
    }

    return value;
}


int InterChange::indirectMidi(CommandBlock& cmd, uchar& newMsg, bool& guiTo, string& text)
{
    int value = cmd.data.value;
    int control = cmd.data.control;
    int msgID;
    if (control == MIDI::control::instrument)
    {
        msgID = synth.setProgramFromBank(cmd);
        cmd.data.control = MAIN::control::loadInstrumentFromBank;
        cmd.data.part    = TOPLEVEL::section::main;
        // moved to 'main' for return updates.
        if (msgID > NO_MSG)
            text = " FAILED " + text;
        else
            text = "ed ";
    }
    else
    {
        msgID = synth.setRootBank(cmd.data.insert, cmd.data.engine);
        if (msgID > NO_MSG)
            text = "FAILED " + text;
        else
            text = "";
    }
    text += textMsgBuffer.fetch(msgID & NO_MSG);
    newMsg = true;
    cmd.data.source = TOPLEVEL::action::toAll;
    // everyone will want to know about these!
    guiTo = true;
    return value;
}


int InterChange::indirectScales(CommandBlock& cmd, uchar& newMsg, bool& guiTo, string& text)
{
    bool write = (cmd.data.type & TOPLEVEL::type::Write);
    int value   = cmd.data.value;
    int control = cmd.data.control;

    switch(control)
    {
        case SCALES::control::tuning:
            text = formatScales(text);
            value = synth.microtonal.texttotunings(text);
            if (value <= 0)
                synth.microtonal.defaults(1);
            break;
        case SCALES::control::keyboardMap:
            text = formatKeys(text);
            value = synth.microtonal.texttomapping(text);
            if (value <= 0)
                synth.microtonal.defaults(2);
            break;

        case SCALES::control::keymapSize:
            if (write)
            {
                synth.microtonal.Pmapsize = int(value);
                synth.setAllPartMaps();
            }
            else
            {
                value = synth.microtonal.Pmapsize;
            }
            break;

        case SCALES::control::importScl:
            value = synth.microtonal.loadscl(setExtension(text,EXTEN::scalaTuning));
            if (value <= 0)
            {
                synth.microtonal.defaults(1);
            }
            else
            {
                text = synth.microtonal.tuningtotext();
            }
            break;
        case SCALES::control::importKbm:
            value = synth.microtonal.loadkbm(setExtension(text,EXTEN::scalaKeymap));
            if (value < 0)
                synth.microtonal.defaults(2);
            else if (value > 0)
            {
                text = "";
                int map;
                for (int i = 0; i < value; ++ i)
                {
                    if (i > 0)
                        text += "\n";
                    map = synth.microtonal.Pmapping[i];
                    if (map == -1)
                        text += 'x';
                    else
                        text += to_string(map);
                    string comment{synth.microtonal.PmapComment[i]};
                    if (!comment.empty())
                        text += (" ! " + comment);
                }
                cmd.data.parameter = textMsgBuffer.push(synth.microtonal.map2kbm());
            }
            break;

        case SCALES::control::exportScl:
        {
            string newtext{synth.microtonal.scale2scl()};
            string filename{text};
            filename = setExtension(filename, EXTEN::scalaTuning);
            saveText(newtext, filename);
        }
        break;
        case SCALES::control::exportKbm:
        {
            string newtext{synth.microtonal.map2kbm()};
            string filename{text};
            filename = setExtension(filename, EXTEN::scalaKeymap);
            saveText(newtext, filename);
        }
        break;

        case SCALES::control::name:
            if (write)
            {
                synth.microtonal.Pname = text;
            }
            else
                text = synth.microtonal.Pname;
            newMsg = true;
            break;
        case SCALES::control::comment:
            if (write)
                synth.microtonal.Pcomment = text;
            else
                text = synth.microtonal.Pcomment;
            newMsg = true;
            break;
    }
    cmd.data.source &= ~TOPLEVEL::action::lowPrio;
    guiTo = true;
    return value;
}


int InterChange::indirectMain(CommandBlock& cmd, uchar &newMsg, bool &guiTo, string &text, float &valuef)
{
    bool write = (cmd.data.type & TOPLEVEL::type::Write);
    int value   = cmd.data.value;
    int control = cmd.data.control;
    int kititem = cmd.data.kit;
    int insert  = cmd.data.insert;
    switch (control)
    {
        case MAIN::control::detune:
        {
            if (write)
            {
                add2undo(cmd, noteSeen);
                valuef = cmd.data.value;
                synth.microtonal.setglobalfinedetune(valuef);
                synth.setAllPartMaps();
            }
            else
                valuef = synth.microtonal.Pglobalfinedetune;
            break;
        }
        case MAIN::control::keyShift:
        {
            if (write)
            {
                synth.setPkeyshift(value + 64);
                synth.setAllPartMaps();
            }
            else
                value = synth.Pkeyshift - 64;
            break;
        }

        case MAIN::control::exportBank:
        {
            if (kititem == UNUSED)
                kititem = synth.getRuntime().currentRoot;
            text = synth.bank.exportBank(text, kititem, value);
            newMsg = true;
            break;
        }
        case MAIN::control::importBank:
        {
            if (kititem == UNUSED)
                kititem = synth.getRuntime().currentRoot;
            text = synth.bank.importBank(text, kititem, value);
            newMsg = true;
            break;
        }
        case MAIN::control::deleteBank:
        {
            text = synth.bank.removebank(value, kititem);
            newMsg = true;
            break;
        }
        case MAIN::control::loadInstrumentFromBank:
        {
            uint result = synth.setProgramFromBank(cmd);
            text = textMsgBuffer.fetch(result & NO_MSG);
            if (result < 0x1000)
            {
                if (synth.getRuntime().bankHighlight)
                    synth.getRuntime().lastBankPart = (value << 15) | (synth.getRuntime().currentBank << 8) | synth.getRuntime().currentRoot;
                else
                    synth.getRuntime().lastBankPart = UNUSED;
                text = "ed " + text;
            }
            else
                text = " FAILED " + text;
            newMsg = true;
            break;
        }

        case MAIN::control::loadInstrumentByName:
        {
            cmd.data.miscmsg = textMsgBuffer.push(text);
            uint result = synth.setProgramByName(cmd);
            text = textMsgBuffer.fetch(result & NO_MSG);
            synth.getRuntime().lastBankPart = UNUSED;
            if (result < 0x1000)
                text = "ed " + text;
            else
                text = " FAILED " + text;
            newMsg = true;
            break;
        }

        case MAIN::control::saveNamedInstrument:
        {
            bool ok = true;
            int saveType = synth.getRuntime().instrumentFormat;
            // This is both. Below we send them individually.

            if (saveType & 2) // Yoshimi format
                ok = synth.part[value]->saveXML(text, true);
            if (ok && (saveType & 1)) // legacy
                ok = synth.part[value]->saveXML(text, false);

            if (ok)
            {
                synth.getRuntime().sessionSeen[TOPLEVEL::XML::Instrument] = true;
                synth.addHistory(setExtension(text, EXTEN::zynInst), TOPLEVEL::XML::Instrument);
                synth.part[value]->PyoshiType = (saveType & 2);
                text = "d " + text;
            }
            else
                text = " FAILED " + text;
            newMsg = true;
            break;
        }
        case MAIN::control::loadNamedPatchset:
            vectorClear(NUM_MIDI_CHANNELS);
            if (synth.loadPatchSetAndUpdate(text))
            {
                synth.addHistory(setExtension(text, EXTEN::patchset), TOPLEVEL::XML::Patch);
                text = "ed " + text;
            }
            else
                text = " FAILED " + text;
            synth.getRuntime().lastBankPart = UNUSED;
            newMsg = true;
            break;
        case MAIN::control::saveNamedPatchset:
            if (synth.savePatchesXML(text))
            {
                synth.addHistory(setExtension(text, EXTEN::patchset), TOPLEVEL::XML::Patch);
                text = "d " + text;
            }
            else
                text = " FAILED " + text;
            newMsg = true;
            break;
        case MAIN::control::loadNamedVector:
        {
            int tmp = synth.vectorcontrol.loadVectorAndUpdate(insert, text);
            if (tmp < NO_MSG)
            {
                cmd.data.insert = tmp;
                synth.addHistory(setExtension(text, EXTEN::vector), TOPLEVEL::XML::Vector);
                text = "ed " + text + " to chan " + to_string(int(tmp + 1));
            }
            else
                text = " FAILED " + text;
            synth.getRuntime().lastBankPart = UNUSED;
            newMsg = true;
            break;
        }
        case MAIN::control::saveNamedVector:
        {
            string oldname{synth.getRuntime().vectordata.Name[insert]};
            int pos = oldname.find("No Name");
            if (pos >=0 && pos < 2)
                synth.getRuntime().vectordata.Name[insert] = findLeafName(text);
            int tmp = synth.vectorcontrol.saveVector(insert, text, true);
            if (tmp == NO_MSG)
            {
                synth.addHistory(setExtension(text, EXTEN::vector), TOPLEVEL::XML::Vector);
                text = "d " + text;
            }
            else
            {
                string name = textMsgBuffer.fetch(tmp);
                if (name != "FAIL")
                    text = " " + name;
                else
                    text = " FAILED " + text;
            }
            newMsg = true;
            break;
        }
       case MAIN::control::loadNamedScale:
       {
            string filename = setExtension(text, EXTEN::scale);
            int err = synth.microtonal.loadXML(filename);
            if (err == 0)
            {
                synth.addHistory(filename, TOPLEVEL::XML::Scale);
                text = "ed " + text;
            }
            else
            {
                text = " FAILED " + text;
                if (err < 0) // incoming negative values inverted for the text list
                    text += (" " + scale_errors [0 - err]);
            }
            newMsg = true;
            break;
       }
        case MAIN::control::saveNamedScale:
        {
            string filename = setExtension(text, EXTEN::scale);
            if (synth.microtonal.saveXML(filename))
            {
                synth.addHistory(filename, TOPLEVEL::XML::Scale);
                text = "d " + text;
            }
            else
                text = " FAILED " + text;
            newMsg = true;
            break;
        }
        case MAIN::control::loadNamedState:
            vectorClear(NUM_MIDI_CHANNELS);
            if (synth.loadStateAndUpdate(text))
            {
                text = setExtension(text, EXTEN::state);
                string name = file::configDir() + string(YOSHIMI);
                name += ("-" + to_string(synth.getUniqueId()));
                name += ".state";
                if ((text != name)) // never include default state
                    synth.addHistory(text, TOPLEVEL::XML::State);
                text = "ed " + text;
            }
            else
                text = " FAILED " + text;
            synth.getRuntime().lastBankPart = UNUSED;
            newMsg = true;
            break;
        case MAIN::control::saveNamedState:
        {
            string filename = setExtension(text, EXTEN::state);
            if (synth.saveState(filename))
            {
                string name = file::configDir() + string(YOSHIMI);
                name += ("-" + to_string(synth.getUniqueId()));
                name += ".state";
                if ((text != name)) // never include default state
                    synth.addHistory(filename, TOPLEVEL::XML::State);
                text = "d " + text;
            }
            else
                text = " FAILED " + text;
            newMsg = true;
            break;
        }
        case MAIN::control::readLastSeen:
            break; // do nothing here
        case MAIN::control::loadFileFromList:
            break; // do nothing here

        case MAIN::control::defaultPart: // clear entire part
            if (write)
            {
                undoRedoClear();
                synth.part[value]->reset(value);
                synth.getRuntime().sessionSeen[TOPLEVEL::XML::Instrument] = false;
                cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            }
            break;

        case MAIN::control::defaultInstrument: // clear part's instrument
            if (write)
            {
                undoRedoClear();
                doClearPartInstrument(value);
                synth.getRuntime().sessionSeen[TOPLEVEL::XML::Instrument] = false;
                cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            }
            break;

        case MAIN::control::exportPadSynthSamples:
        {
            uchar partnum = insert;
            synth.part[partnum]->kit[kititem].padpars->buildNewWavetable(true); // blocking wait for result
            if (synth.part[partnum]->kit[kititem].padpars->export2wav(text))
            {
                synth.addHistory(text, TOPLEVEL::XML::PadSample);
                text = "d " + text;
            }
            else
                text = " FAILED some samples " + text;
            newMsg = true;
            break;
        }
        case MAIN::control::masterReset:
            synth.resetAll(0);
            break;
        case MAIN::control::masterResetAndMlearn:
            synth.resetAll(1);
            break;
        case MAIN::control::openManual: // display user guide
        {
            text = "";
            cmd.data.control = TOPLEVEL::control::textMessage;
            string found  = synth.getRuntime().manualFile;
            if (not found.empty())
            {

                size_t pos = found.rfind("files/yoshimi_user_guide_version");
                found = found.substr(0, pos);
                file::cmd2string("xdg-open " + found + "index.html &");
            }
            else
            {
                cmd.data.miscmsg = textMsgBuffer.push("Can't find manual :(");
                returnsBuffer.write(cmd.bytes);
                newMsg = true;
            }
            break;

        }
        case MAIN::control::startInstance:
            value = Config::instances().requestNewInstance(value);
            break;
        case MAIN::control::stopInstance:
            text = to_string(value) + " ";
            if (value < 0 || value >= 32)
                text += "Out of range";
            else
            {
                SynthEngine& toClose = Config::instances().findSynthByID(value);
                if (toClose.getUniqueId() == 0 and value > 0)
                    text += "Can't find";
                else
                {
                    toClose.getRuntime().runSynth = false;
                    text += "Closed";
                }
            }
            newMsg = true;
            break;

        case MAIN::control::stopSound:
#ifdef REPORT_NOTES_ON_OFF
            // note test
            synth.getRuntime().Log("note on sent " + to_string(synth.getRuntime().noteOnSent));
            synth.getRuntime().Log("note on seen " + to_string(synth.getRuntime().noteOnSeen));
            synth.getRuntime().Log("note off sent " + to_string(synth.getRuntime().noteOffSent));
            synth.getRuntime().Log("note off seen " + to_string(synth.getRuntime().noteOffSeen));
            synth.getRuntime().Log("notes hanging sent " + to_string(synth.getRuntime().noteOnSent - synth.getRuntime().noteOffSent));
            synth.getRuntime().Log("notes hanging seen " + to_string(synth.getRuntime().noteOnSeen - synth.getRuntime().noteOffSeen));
#endif
            synth.ShutUp();
            break;
    }
    cmd.data.source &= ~TOPLEVEL::action::lowPrio;
    if (control != MAIN::control::startInstance && control != MAIN::control::stopInstance)
        guiTo = true;
    return value;
}



int InterChange::indirectBank(CommandBlock& cmd, uchar& newMsg, bool& guiTo, string& text)
{
    bool write = (cmd.data.type & TOPLEVEL::type::Write);
    int value   = cmd.data.value;
    int control = cmd.data.control;
    int kititem = cmd.data.kit;
    int engine  = cmd.data.engine;
    int insert  = cmd.data.insert;
    int parameter = cmd.data.parameter;
    switch (control)
    {
        case BANK::control::renameInstrument:
        {
            if (kititem == UNUSED)
            {
                kititem = synth.getRuntime().currentBank;
                cmd.data.kit = kititem;
            }
            if (engine == UNUSED)
            {
                engine = synth.getRuntime().currentRoot;
                cmd.data.engine = engine;
            }
            int msgID = synth.bank.setInstrumentName(text, insert, kititem, engine);
            if (msgID > NO_MSG)
                text = " FAILED ";
            else
                text = " ";
            text += textMsgBuffer.fetch(msgID & NO_MSG);
            synth.getRuntime().lastBankPart = UNUSED;
            newMsg = true;
            break;
        }
        case BANK::control::saveInstrument:
        {
            if (kititem == UNUSED)
            {
                kititem = synth.getRuntime().currentBank;
                cmd.data.kit = kititem;
            }
            if (engine == UNUSED)
            {
                engine = synth.getRuntime().currentRoot;
                cmd.data.engine = engine;
            }
            if (parameter == UNUSED)
            {
                parameter = synth.getRuntime().currentPart;
                cmd.data.parameter = parameter;
            }
            text = synth.part[parameter]->Pname;
            if (text == DEFAULT_NAME)
                text = "FAILED Can't save default instrument type";
            else if (!synth.bank.savetoslot(engine, kititem, insert, parameter))
                text = "FAILED Could not save " + text + " to " + to_string(insert + 1);
            else
            { // 0x80 on engine indicates it is a save not a load
                if (synth.getRuntime().bankHighlight)
                    synth.getRuntime().lastBankPart = (insert << 15) | (kititem << 8) | engine | 0x80;
                text = "" + to_string(insert + 1) +". " + text;
            }
            newMsg = true;
            break;
        }
        case BANK::control::deleteInstrument:
        {
            text  = synth.bank.clearslot(value, synth.getRuntime().currentRoot,  synth.getRuntime().currentBank);
            synth.getRuntime().lastBankPart = UNUSED;
            newMsg = true;
            break;
        }

        case BANK::control::selectFirstInstrumentToSwap:
        {
            if (kititem == UNUSED)
            {
                kititem = synth.getRuntime().currentBank;
                cmd.data.kit = kititem;
            }
            if (engine == UNUSED)
            {
                engine = synth.getRuntime().currentRoot;
                cmd.data.engine = engine;
            }
            swapInstrument1 = insert;
            swapBank1 = kititem;
            swapRoot1 = engine;
            break;
        }
        case BANK::control::selectSecondInstrumentAndSwap:
        {
            if (kititem == UNUSED)
            {
                kititem = synth.getRuntime().currentBank;
                cmd.data.kit = kititem;
            }
            if (engine == UNUSED)
            {
                engine = synth.getRuntime().currentRoot;
                cmd.data.engine = engine;
            }
            text = synth.bank.swapslot(swapInstrument1, insert, swapBank1, kititem, swapRoot1, engine);
            swapInstrument1 = UNUSED;
            swapBank1 = UNUSED;
            swapRoot1 = UNUSED;
            synth.getRuntime().lastBankPart = UNUSED;
            newMsg = true;
            guiTo = true;
            break;
        }
        case BANK::control::selectBank:
            if (engine == UNUSED)
                engine = cmd.data.engine = synth.getRuntime().currentRoot;
            if (write)
            {
                text = textMsgBuffer.fetch(synth.setRootBank(engine, value) & NO_MSG);
                synth.getRuntime().updateConfig(CONFIG::control::changeBank, synth.getRuntime().currentBank);

            }
            else
            {
                int tmp = synth.getRuntime().currentBank;
                text = "Current: " +(to_string(tmp)) + " " + synth.bank.getBankName(tmp, cmd.data.engine);
            }
            newMsg = true;
            break;
        case BANK::control::renameBank:
            if (engine == UNUSED)
                engine = cmd.data.engine = synth.getRuntime().currentRoot;
            if (write)
            {
                int tmp = synth.bank.changeBankName(cmd.data.engine, value, text);
                text = textMsgBuffer.fetch(tmp & NO_MSG);
                if (tmp > NO_MSG)
                    text = "FAILED: " + text;
                guiTo = true;
            }
            else
            {
                text = " Name: " + synth.bank.getBankName(value, cmd.data.engine);
            }
            newMsg = true;
            break;
        case BANK::control::createBank:
            {
                bool isOK = true;
                int newbank = kititem;
                int rootID = engine;
                if (rootID == UNUSED)
                    rootID = synth.getRuntime().currentRoot;
                if (newbank == UNUSED)
                {
                    isOK = false;
                    newbank = 5; // offset to avoid zero for as long as possible
                    for (int i = 0; i < MAX_BANKS_IN_ROOT; ++i)
                    {
                        newbank = (newbank + 5) & 0x7f;
                        if (synth.bank.getBankName(newbank, rootID).empty())
                        {
                            isOK = true;
                            break;
                        }
                    }
                    if (!isOK)
                        text = "FAILED: Root " + to_string(rootID) + " has no space";
                }
                if (isOK)
                {
                    string trytext{synth.bank.getBankName(newbank, rootID)};
                    if (!trytext.empty())
                    {
                        text = "FAILED: ID " + to_string(newbank) + " already contains " + trytext;
                        isOK = false;
                    }

                    if (isOK and not synth.bank.newIDbank(text, newbank))
                    {
                        text = "FAILED Could not create bank " + text + " for ID " + asString(newbank);
                        isOK = false;
                    }
                }
                if (isOK)
                    text = "Created " + text + " at ID " + to_string(newbank) + " in root " + to_string(rootID);
                newMsg = true;
                guiTo = true;
            }
            break;

        case BANK::control::deleteBank:
            break; // not yet!

        case BANK::control::findBankSize:
            if (engine == UNUSED)
                engine = synth.getRuntime().currentRoot;
            if (synth.bank.getBankName(kititem, engine).empty())
                value = UNUSED;
            else
                value = synth.bank.getBankSize(kititem, engine);
            break;

        case BANK::control::selectFirstBankToSwap:
            if (engine == UNUSED)
            {
                engine = synth.getRuntime().currentRoot;
                cmd.data.engine = engine;
            }
            swapBank1 = kititem;
            swapRoot1 = engine;
            break;
        case BANK::control::selectSecondBankAndSwap:
            if (engine == UNUSED)
            {
                engine = synth.getRuntime().currentRoot;
                cmd.data.engine = engine;
            }
            text = synth.bank.swapbanks(swapBank1, kititem, swapRoot1, engine);
            swapBank1 = UNUSED;
            swapRoot1 = UNUSED;
            newMsg = true;
            guiTo = true;
            break;

        case BANK::control::selectRoot:
            if (write)
            {
                int msgID = synth.setRootBank(value, UNUSED);
                if (msgID < NO_MSG)
                    synth.saveBanks(); // do we need this when only selecting?
                text = textMsgBuffer.fetch(msgID & NO_MSG);
                synth.getRuntime().updateConfig(CONFIG::control::changeRoot, synth.getRuntime().currentRoot);
            }
            else
            {
                int tmp = synth.getRuntime().currentRoot;
                text = "Current Root: " +(to_string(tmp)) + " " + synth.bank.getRootPath(tmp);
            }
            newMsg = true;
            break;
        case BANK::control::changeRootId:
            if (engine == UNUSED)
                cmd.data.engine = synth.getRuntime().currentRoot;
            synth.bank.changeRootID(cmd.data.engine, value);
            synth.saveBanks();
            break;
        case BANK::addNamedRoot:
            if (write) // not realistically readable
            {
                if (kititem != UNUSED)
                {
                    kititem = synth.bank.generateSingleRoot(text, false);
                    cmd.data.kit = kititem;
                    synth.bank.installNewRoot(kititem, text);
                    synth.saveBanks();
                }
                else
                {
                    size_t found = synth.bank.addRootDir(text);
                    if (found)
                    {
                        synth.bank.installNewRoot(found, text);
                        synth.saveBanks();
                    }
                    else
                    {
                        value = UNUSED;
                        text = "Can't find path " + text;
                    }
                }
                newMsg = true;
            }
            break;
        case BANK::deselectRoot:
            if (write) // not realistically readable
            {
                if (synth.bank.removeRoot(kititem))
                    value = UNUSED;
                synth.saveBanks();
            }
            break;

        case BANK::control::refreshDefaults:
            if (value)
                synth.bank.checkLocalBanks();
            synth.getRuntime().banksChecked = true;
            synth.getRuntime().updateConfig(CONFIG::control::banksChecked, 1);
        break;
    }
    cmd.data.source &= ~TOPLEVEL::action::lowPrio;
    return value;
}


int InterChange::indirectConfig(CommandBlock& cmd, uchar& newMsg, bool& guiTo, string& text)
{
    bool write = (cmd.data.type & TOPLEVEL::type::Write);
    int value   = cmd.data.value;
    int control = cmd.data.control;
    int kititem = cmd.data.kit;
    switch (control)
    {
        case CONFIG::control::jackMidiSource:
            if (write)
            {
                synth.getRuntime().jackMidiDevice = text;
                synth.getRuntime().updateConfig(CONFIG::control::jackMidiSource, textMsgBuffer.push(text));
            }
            else
                text = synth.getRuntime().jackMidiDevice;
            newMsg = true;
            break;
        case CONFIG::control::jackServer:
            if (write)
            {
                synth.getRuntime().jackServer = text;
                synth.getRuntime().updateConfig(CONFIG::control::jackServer, textMsgBuffer.push(text));
            }
            else
                text = synth.getRuntime().jackServer;
            newMsg = true;
            break;
        case CONFIG::control::alsaMidiSource:
            if (write)
            {
                synth.getRuntime().alsaMidiDevice = text;
                synth.getRuntime().updateConfig(control, textMsgBuffer.push(text));
            }
            else
                text = synth.getRuntime().alsaMidiDevice;
            newMsg = true;
            break;
        case CONFIG::control::alsaAudioDevice:
            if (write)
            {
                synth.getRuntime().alsaAudioDevice = text;
                synth.getRuntime().updateConfig(control, textMsgBuffer.push(text));
            }
            else
                text = synth.getRuntime().alsaAudioDevice;
            newMsg = true;
            break;
        case CONFIG::control::addPresetRootDir:
        {
            bool isOK = false;
            if (isDirectory(text))
                isOK= true;
            else
            {
                if (createDir(text))
                {
                    text = " FAILED could not create " + text;
                }
                else
                    isOK = true;
            }
            if (isOK)
            {
                int i = 0;
                while (!synth.getRuntime().presetsDirlist[i].empty())
                    ++i;
                if (i > (MAX_PRESETS - 2))
                    text = " FAILED preset list full";
                else
                {
                    synth.getRuntime().presetsDirlist[i] = text;
                    text = "ed " + text;
                }
                synth.getRuntime().savePresetsList();
            }
            newMsg = true;
            break;
        }
        case CONFIG::control::removePresetRootDir:
        {
            int i = value;
            text = synth.getRuntime().presetsDirlist[i];
            while (!synth.getRuntime().presetsDirlist[i + 1].empty())
            {
                synth.getRuntime().presetsDirlist[i] = synth.getRuntime().presetsDirlist[i + 1];
                ++i;
            }
            synth.getRuntime().presetsDirlist[i] = "";
            synth.getRuntime().presetsRootID = 0;
            newMsg = true;
            synth.getRuntime().savePresetsList();
            break;
        }
        case CONFIG::control::currentPresetRoot:
        {
            if (write)
            {
                synth.getRuntime().presetsRootID = value;
            }
            else
                value = synth.getRuntime().presetsRootID = value;
            text = synth.getRuntime().presetsDirlist[value];
            newMsg = true;
            break;
        }
        case CONFIG::control::saveCurrentConfig:
            if (write)
            {
                text = synth.getRuntime().configFile;
                if (synth.getRuntime().saveInstanceConfig())
                    text = "d " + text;
                else
                    text = " FAILED " + text;
            }
            else
                text = "READ";
            newMsg = true;
            cmd.data.miscmsg = textMsgBuffer.push(text); // slightly odd case
            break;
        case CONFIG::control::historyLock:
        {
            if (write)
            {
                synth.setHistoryLock(kititem, value);
            }
            else
                value = synth.getHistoryLock(kititem);
            break;
        }
    }
    if ((cmd.data.source & TOPLEVEL::action::noAction) != TOPLEVEL::action::fromGUI)
        guiTo = true;
    cmd.data.source &= ~TOPLEVEL::action::lowPrio;
    return value;
}


int InterChange::indirectPart(CommandBlock& cmd, uchar& newMsg, bool& guiTo, string& text)
{
    bool write = (cmd.data.type & TOPLEVEL::type::Write);
    int value   = cmd.data.value;
    int control = cmd.data.control;
    uint npart  = cmd.data.part;
    int kititem = cmd.data.kit;
    int parameter = cmd.data.parameter;

    assert(npart < NUM_MIDI_PARTS);
    Part& part{*synth.part[npart]};

    switch(control)
    {
        case PART::control::keyShift:
        {
            if (write)
            {
                part.Pkeyshift = value + 64;
                synth.setPartMap(npart);
            }
            else
                value = part.Pkeyshift - 64;
            cmd.data.source &= ~TOPLEVEL::action::lowPrio;
        }
        break;
        case PART::control::enableKitLine:
            if (write)
            {
                part.setkititemstatus(kititem, value);
                synth.partonoffWrite(npart, 2);
                cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            }
        break;

        case PADSYNTH::control::applyChanges:
            // it appears Pkitmode is not being recognised here :(
            if (kititem >= NUM_KIT_ITEMS)//not part.Pkitmode)
                kititem = 0;
            if (write)
            {
                // esp. a "blocking Apply" is redirected from Synth-Thread: commandSendReal() -> commandPad() -> returns() -> indirectTransfers()
                synth.part[npart]->kit[kititem].padpars->buildNewWavetable((parameter == 0));  // parameter == 0 causes blocking wait
                cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            }
            else
                value = not part.kit[kititem].padpars->futureBuild.isUnderway();
            break;

        case PART::control::audioDestination:
            if (npart < synth.getRuntime().numAvailableParts)
            {
                if (value & 2)
                {
                    Config::instances().registerAudioPort(synth.getUniqueId(), npart);
                }
                cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            }
            break;
        case PART::control::instrumentCopyright:
            if (write)
            {
                part.info.Pauthor = text;
                guiTo = true;
            }
            else
                text = part.info.Pauthor;
            cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            newMsg = true;
            break;
        case PART::control::instrumentComments:
            if (write)
            {
                part.info.Pcomments = text;
                guiTo = true;
            }
            else
                text = part.info.Pcomments;
            cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            newMsg = true;
            break;
        case PART::control::instrumentName: // part or kit item names
            if (kititem == UNUSED)
            {
                if (write)
                {
                    part.Pname = text;
                    if (part.Poriginal.empty() || part.Poriginal == UNTITLED)
                        part.Poriginal = text;
                    guiTo = true;
                }
                else
                {
                    text = part.Pname;
                }
            }
            else if (part.Pkitmode)
            {
                if (kititem >= NUM_KIT_ITEMS)
                    text = " FAILED out of range";
                else
                {
                    if (write)
                    {
                        part.kit[kititem].Pname = text;
                        guiTo = true;
                    }
                    else
                    {
                        text = part.kit[kititem].Pname;
                    }
                }
            }
            else
                text = " FAILED Not in kit mode";
            cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            newMsg = true;
            break;
        case PART::control::instrumentType:
            if (write)
            {
                part.info.Ptype = value;
                guiTo = true;
            }
            else
                value = part.info.Ptype;
            cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            break;
        case PART::control::defaultInstrumentCopyright:
            string name = file::configDir() + "/copyright.txt";
            if (parameter == 0) // load
            {
                text = loadText(name); // TODO provide failure warning
                text = func::formatTextLines(text, 54);
                part.info.Pauthor = text;
                guiTo = true;
            }
            else
            {
                text = part.info.Pauthor;
                saveText(text, name);
            }
            cmd.data.source &= ~TOPLEVEL::action::lowPrio;
            newMsg = true;
            break;
    }
    return value;
}

string InterChange::formatScales(string text)
{
    string delimiters{","};
    size_t current;
    size_t next = -1;
    size_t found;
    string word;
    string newtext;
    do
    {
        current = next + 1;
        next = text.find_first_of(delimiters, current );
        word = func::trimEnds(text.substr(current, next - current));

        found = word.find('.');
        if (found != string::npos)
        {
            if (found < 4)
            {
                string tmp (4 - found, '0'); // leading zeros
                word = tmp + word;
            }
            found = word.size();
            if (found < 11)
            {
                string tmp  (11 - found, '0'); // trailing zeros
                word += tmp;
            }
        }
        newtext += word;
        if (next != string::npos)
            newtext += "\n";
    }
    while (next != string::npos);
    return newtext;
}


string InterChange::formatKeys(string text)
{
    string delimiters{","};
    size_t current;
    size_t next = -1;
    string word;
    string newtext;
    do
    {
        current = next + 1;
        next = text.find_first_of(delimiters, current );
        word = func::trimEnds(text.substr(current, next - current));
        if (word[0] < '0' || word[0] > '9')
        {
            word = "x";
        }
        newtext += word;
        if (next != string::npos)
            newtext += "\n";
    }
    while (next != string::npos);
    return newtext;
}


float InterChange::readAllData(CommandBlock& cmd)
{
    if (cmd.data.part == TOPLEVEL::instanceID)
    {
        return synth.getUniqueId();
    }

    if (cmd.data.part == TOPLEVEL::windowTitle)
    {
        return buildWindowTitle(cmd);
    }

    if (cmd.data.type & TOPLEVEL::type::Limits) // these are static
    {
        /*
         * commandtype limits values
         * 0    adjusted input value
         * 1    min
         * 2    max
         * 3    default
         *
         * tryData.data.type will be updated:
         * bit 5 set    MIDI-learnable
         * bit 7 set    Is an integer value
         */
        cmd.data.type -= TOPLEVEL::type::Limits;
        float value = returnLimits(cmd);
        synth.getRuntime().finishedCLI = true;
        return value;
    }

    // these are not!

    /*
     * VU always responds even when loading a *huge*
     * PadSynth instrument. This is safe because the part
     * being changed is disabled, so won't be seen.
     *
     * Other reads will be blocked.
     * This needs improving.
     */
    CommandBlock forwardCmd;
    uchar control = cmd.data.control;
    if (cmd.data.part == TOPLEVEL::section::main && (control >= MAIN::control::readPartPeak && control <= MAIN::control::readMainLRrms))
    {
        commandSendReal(cmd);
        synth.fetchMeterData();
        return cmd.data.value;
    }
    int npart = cmd.data.part;
    bool indirect = ((cmd.data.source & TOPLEVEL::action::muteAndLoop) == TOPLEVEL::action::lowPrio);
    if (npart < NUM_MIDI_PARTS && synth.part[npart]->busy)
    {
        cmd.data.control = TOPLEVEL::control::partBusy; // part busy message
        cmd.data.kit = UNUSED;
        cmd.data.engine = UNUSED;
        cmd.data.insert = UNUSED;
    }
    reTry:
    memcpy(forwardCmd.bytes, cmd.bytes, sizeof(forwardCmd));
    while (syncWrite || lowPrioWrite)
        sleep_for(10us);
    if (indirect)
    {
        /*
         * This still isn't quite right there is a very
         * remote chance of getting garbled text :(
         */
        indirectTransfers(forwardCmd, true);
        synth.getRuntime().finishedCLI = true;
        return forwardCmd.data.value;
    }
    else
        commandSendReal(forwardCmd);
    if (syncWrite || lowPrioWrite)
        goto reTry; // it may have changed mid-process

    if ((forwardCmd.data.source & TOPLEVEL::action::noAction) == TOPLEVEL::action::fromCLI)
        resolveReplies(forwardCmd);


    synth.getRuntime().finishedCLI = true; // in case it misses lines above
    return forwardCmd.data.value;
}


float InterChange::buildWindowTitle(CommandBlock& cmd)
{
    string sent_name = synth.textMsgBuffer.fetch((int(cmd.data.value))); // catch this early
    string name = synth.makeUniqueName("");
    int section = cmd.data.control;
    int engine  = cmd.data.engine;
    if (section >= NUM_MIDI_PARTS)
    {
        if (section == TOPLEVEL::section::systemEffects)
            name += "System Effect ";
        else if (section == TOPLEVEL::section::insertEffects)
            name += "Insert Effect ";
        if (section != UNUSED)
        {
            name += to_string(engine + 1);
            name += " - ";
        }
        if (cmd.data.kit == EFFECT::type::dynFilter)
            name += "DynFilter ";
        name += sent_name;
        return synth.textMsgBuffer.push(name);
    }

    if (engine == PART::engine::padSynth)
        name += "PadSynth ";
    else if (engine == PART::engine::subSynth)
        name += "SubSynth ";
    else if (engine < PART::engine::addVoiceModEnd)
    {
        name += "AddSynth ";
        if (cmd.data.engine >= PART::engine::addMod1)
            name += "Modulator ";
        else if (engine >= PART::engine::addVoice1)
        {
            name += "Voice ";
        }
        if (engine != PART::engine::addSynth)
        {
            name += to_string((engine & 7) + 1);
            name += " ";
        }
    }
    if (cmd.data.insert == TOPLEVEL::insert::envelopeGroup)
    {
        int group = int(cmd.data.parameter);
        switch (group)
        {
            case TOPLEVEL::insertType::amplitude:
                name += "Amplitude ";
                break;
            case TOPLEVEL::insertType::frequency:
                name += "Frequency ";
                break;
            case TOPLEVEL::insertType::filter:
                name += "Filter ";
                break;
            case TOPLEVEL::insertType::bandwidth:
                name += "Bandwidth ";
                break;
        }
    }
    name += sent_name;
    if (section < NUM_MIDI_PARTS) // it's at part level
    {
        name += " - Part ";
        name += to_string(section + 1);
        name += " ";
        name += synth.part[section]->Pname;

        if (synth.part[section]->Pkitmode != 0)
        {
            int kititem = int(cmd.data.kit);
            name += ", Kit ";
            if (kititem < NUM_KIT_ITEMS)
            {
                name += to_string(kititem + 1);
                name += " ";
                string kitname = synth.part[section]->kit[kititem].Pname;
                if (!kitname.empty())
                {
                    name += "- ";
                    name += kitname;
                }
            }
        }
    }
    return synth.textMsgBuffer.push(name);
}


void InterChange::resolveReplies(CommandBlock& cmd)
{
    //synth.CBtest(cmd, true);

    uchar source = cmd.data.source & TOPLEVEL::action::noAction;
    // making sure there are no stray top bits.
    if (source == TOPLEVEL::action::noAction)
    {
        // in case it was originally called from CLI
        synth.getRuntime().finishedCLI = true;
        return; // no further action
    }

    if (cmd.data.type & TOPLEVEL::type::LearnRequest)
    {
        synth.midilearn.setTransferBlock(cmd);
        return;
    }

    if (source != TOPLEVEL::action::fromMIDI && !setUndo)
        synth.getRuntime().Log(resolveAll(synth, cmd, _SYS_::LogNotSerious));

    if (source == TOPLEVEL::action::fromCLI)
        synth.getRuntime().finishedCLI = true;
}


// This is only used when no valid banks can be found
void InterChange::generateSpecialInstrument(int npart, string name)
{
    assert(npart < NUM_MIDI_PARTS);
    Part& part{*synth.part[npart]};
    part.Pname = name;
    part.partefx[0]->changeeffect(1);
    part.kit[0].Padenabled = false;
    part.kit[0].Psubenabled = true;

    SUBnoteParameters& pars{ * part.kit[0].subpars};
    pars.Phmag[1] = 75;
    pars.Phmag[2] = 40;
    pars.Pbandwidth = 60;
}



/**********************************************************************************//**
 * Core operation : retrieve, evaluate and forward command messages.
 * @warning this function runs at the begin of each audio computation cycle.
 * @remark Command messages are fetched from the ringbuffers
 *         - fromCLI
 *         - fromGUI
 *         - fromMIDI
 *         Commands are then either directly processed (`commandSend()`), or dispatched
 *         indirectly with the help of the background-thread. Responses and retrieved data
 *         is collected by side-effect in the COmmandData block and send back (`returns()*);
 *         moreover, result values are also published through the toGUI ringbuffer, from where
 *         they are dispatched by the »duty-cycle« in the event handling thread.
 */
void InterChange::mediate()
{
    CommandBlock cmd;
    cmd.data.control = UNUSED; // No other data element could be read uninitialised
    syncWrite = true;
    if (setUndo)
    {
        int step = 0;
        while (setUndo and step < 16)
        {
            undoLast(cmd);
            commandSend(cmd);
            returns(cmd);
            ++ step;
        }
    }

    bool more;
    do
    {
        more = false;
#ifndef YOSHIMI_LV2_PLUGIN
        if (fromCLI.read(cmd.bytes))
        {
            more = true;
            cameFrom = envControl::input;
            if (cmd.data.part != TOPLEVEL::section::midiLearn) // Not special midi-learn message
                commandSend(cmd);
            returns(cmd);
        }
#endif
#ifdef GUI_FLTK
        if (synth.getRuntime().showGui
            && fromGUI.read(cmd.bytes))
        {
            more = true;
            cameFrom = envControl::input;
            if (cmd.data.part != TOPLEVEL::section::midiLearn) // Not special midi-learn message
                commandSend(cmd);
            returns(cmd);
        }
#endif
        if (fromMIDI.read(cmd.bytes))
        {
            more = true;
            cameFrom = envControl::input;
            if (cmd.data.part != TOPLEVEL::section::midiLearn)
                // Normal MIDI message, not special midi-learn message
            {
                historyActionCheck(cmd);
                commandSend(cmd);
                returns(cmd);
            }
#ifdef GUI_FLTK
            else if (synth.getRuntime().showGui
                    && cmd.data.control == MIDILEARN::control::reportActivity)
                toGUI.write(cmd.bytes);
#endif
        }
        else if (cmd.data.control == TOPLEVEL::section::midiLearn)
        {
            // we are looking at the MIDI learn control type that any section *except* MIDI can send.
            synth.mididecode.midiProcess(cmd.data.kit, cmd.data.engine, cmd.data.insert, false);
        }
        if (returnsBuffer.read(cmd.bytes))
        {
            returns(cmd);
            more = true;
        }
    }
    while (more and synth.getRuntime().runSynth.load(std::memory_order_relaxed));
    syncWrite = false;
}



/*
 * Currently this is only used by MIDI NRPNs but eventually
 * be used as a unified way of catching all list loads.
 */
void InterChange::historyActionCheck(CommandBlock& cmd)
{
    if (cmd.data.part != TOPLEVEL::section::main || cmd.data.control != MAIN::control::loadFileFromList)
        return;
    cmd.data.type |= TOPLEVEL::type::Write; // just to be sure
    switch (cmd.data.kit)
    {
        case TOPLEVEL::XML::Instrument:
            cmd.data.source |= TOPLEVEL::action::lowPrio;
            synth.partonoffWrite((cmd.data.insert << 4), -1);
            break;
        case TOPLEVEL::XML::Patch:
            cmd.data.source |= TOPLEVEL::action::muteAndLoop;
            break;
        case TOPLEVEL::XML::Scale:
            cmd.data.source |= TOPLEVEL::action::lowPrio;
            break;
        case TOPLEVEL::XML::State:
            cmd.data.source |= TOPLEVEL::action::muteAndLoop;
            break;
        case TOPLEVEL::XML::Vector:
            cmd.data.source |= TOPLEVEL::action::muteAndLoop;
            break;
    }
}


/**
 * Publish results and retrieved values up into the GUI
 */
void InterChange::returns(CommandBlock& cmd)
{
    synth.getRuntime().finishedCLI = true; // belt and braces :)
    if ((cmd.data.source & TOPLEVEL::action::noAction) == TOPLEVEL::action::noAction)
        return; // no further action

    if (cmd.data.source < TOPLEVEL::action::lowPrio)
    { // currently only used by gui. this may change!
#ifdef GUI_FLTK
        if (synth.getRuntime().showGui)
        {
            uchar type = cmd.data.type; // back from synth
            int tmp = (cmd.data.source & TOPLEVEL::action::noAction);
            if (cmd.data.source & TOPLEVEL::action::forceUpdate)
                tmp = TOPLEVEL::action::toAll;
            /*
             * by the time we reach this point setUndo will have been cleared for single
             * undo/redo actions. It will also have been cleared for the last one of a group.
             * By suppressing the GUI return for the resonance window we avoid a lot of
             * unnecessary redraw actions for the entire graphic area.
             */
            if (!(setUndo && cmd.data.insert == TOPLEVEL::insert::resonanceGraphInsert))
            {
                if (type & TOPLEVEL::type::Write)
                {
                    if (tmp != TOPLEVEL::action::fromGUI)
                    {
                        toGUI.write(cmd.bytes);
                    }
                    if (cameFrom == 1)
                        synth.getRuntime().Log("Undo:");
                    else if (cameFrom == 2)
                        synth.getRuntime().Log("Redo:");
                }
            }
        }
#endif
    }
    if (!decodeLoopback.write(cmd.bytes))
        synth.getRuntime().Log("Unable to write to decodeLoopback buffer");
    spinSortResultsThread();
}


void InterChange::doClearPartInstrument(int npart)
{
    synth.part[npart]->defaultsinstrument();
    synth.part[npart]->cleanup();
    synth.getRuntime().currentPart = npart;
    synth.partonoffWrite(npart, 2);
    synth.pushEffectUpdate(npart);
}


bool InterChange::commandSend(CommandBlock& cmd)
{
    bool isChanged = commandSendReal(cmd);
    bool isWrite = (cmd.data.type & TOPLEVEL::type::Write) > 0;
    if (isWrite && isChanged) // write command
    {
        synth.setNeedsSaving(true);
        uchar control = cmd.data.control;
        uchar npart   = cmd.data.part;
        uchar insert  = cmd.data.insert;
        if (npart < NUM_MIDI_PARTS && (insert != UNUSED || (control != PART::control::enable && control != PART::control::instrumentName)))
        {
            if (synth.part[npart]->Pname == DEFAULT_NAME)
            {
                synth.part[npart]->Pname = UNTITLED;
                cmd.data.source |= TOPLEVEL::action::forceUpdate;
            }
        }
    }
    return isChanged;
}


/**
 * Process the given command message directly within this (Synth) thread.
 */
bool InterChange::commandSendReal(CommandBlock& cmd)
{
    uchar npart = cmd.data.part;
    if (npart == TOPLEVEL::section::midiIn) // music input takes priority!
    {
        commandMidi(cmd);
        return false;
    }
    if (cmd.data.control == TOPLEVEL::control::forceExit)
    {
        cmd.data.source = TOPLEVEL::action::noAction;
        Config::primary().exitType = FORCED_EXIT;
        Config::primary().runSynth = false;
        return false;
    }
    if (npart == TOPLEVEL::section::undoMark)
    {
        if (cmd.data.control == MAIN::control::undo && !undoList.empty())
        {
            setUndo = true;
            undoStart = true;
        }
        else if (cmd.data.control == MAIN::control::redo && !redoList.empty())
        {
            setUndo = true;
            setRedo = true;
            undoStart = true;
        }
    }

    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    if ((cmd.data.source & TOPLEVEL::action::muteAndLoop) == TOPLEVEL::action::lowPrio)
    {
        return true; // indirect transfer
    }

    uchar kititem = cmd.data.kit;
    uchar effSend = cmd.data.kit;
    uchar engine  = cmd.data.engine;
    uchar insert  = cmd.data.insert;

    bool isGui = ((cmd.data.source & TOPLEVEL::action::noAction) == TOPLEVEL::action::fromGUI);
    char button = type & 3;

    if (not isGui and button == 1)
    {
        return false;
    }

    if (npart == TOPLEVEL::section::vector)
    {
        commandVector(cmd);
        return true;
    }
    if (npart == TOPLEVEL::section::scales)
    {
        commandMicrotonal(cmd);
        return true;
    }
    if (npart == TOPLEVEL::section::config)
    {
        commandConfig(cmd);
        return true;
    }
    if (npart == TOPLEVEL::section::bank)
    {
        commandBank(cmd);
        return true;
    }

    if (npart == TOPLEVEL::section::main)
    {
        commandMain(cmd);
        return true;
    }

    if ((npart == TOPLEVEL::section::systemEffects or npart == TOPLEVEL::section::insertEffects) and effSend == UNUSED)
    {
        commandSysIns(cmd);
        return true;
    }

    if (effSend >= (EFFECT::type::none) and effSend < (EFFECT::type::count))
    {
        commandEffects(cmd);
        return true;
    }

    if (npart >= NUM_MIDI_PARTS)
    {
        return false; // invalid part number
    }

    if (kititem >= NUM_KIT_ITEMS && kititem != UNUSED)
    {
        return false; // invalid kit number
    }

    assert(npart < NUM_MIDI_PARTS);
    Part& part{*synth.part[npart]};
    if (part.busy && engine == PART::engine::padSynth)
    {
        cmd.data.type &= ~TOPLEVEL::type::Write; // turn it into a read
        cmd.data.control = TOPLEVEL::control::partBusy;
        cmd.data.kit    = UNUSED;
        cmd.data.engine = UNUSED;
        cmd.data.insert = UNUSED;
        return false;
    }
    if (control == TOPLEVEL::control::partBusy)
    {
        cmd.data.value = part.busy;
        return false;
    }
    if (kititem == UNUSED || insert == TOPLEVEL::insert::kitGroup)
    {
        commandPart(cmd);
        return true;
    }

    if (kititem > 0 and kititem != UNUSED)
    {
        if (not part.Pkitmode)
            return false;
        else if (not part.kit[kititem].Penabled)
            return false;
    }

    if (engine == PART::engine::addSynth)
        return processAdd(cmd, synth);

    if (engine == PART::engine::subSynth)
        return processSub(cmd, synth);
    if (engine == PART::engine::padSynth)
        return processPad(cmd);

    if (engine >= PART::engine::addVoice1)
    {
        if ( engine >= PART::engine::addVoiceModEnd)
        {
            cmd.data.source = TOPLEVEL::action::noAction;
            synth.getRuntime().Log("Invalid voice number");
            synth.getRuntime().finishedCLI = true;
            return false;
        }
        return processVoice(cmd, synth);
    }

    cmd.data.source = TOPLEVEL::action::noAction;
    synth.getRuntime().Log("Invalid engine number");
    synth.getRuntime().finishedCLI = true;
    return false;
}


bool InterChange::processAdd(CommandBlock& cmd, SynthEngine& synth)
{
    Part& part = * synth.part[cmd.data.part];
    int kititem = cmd.data.kit;
    switch(cmd.data.insert)
    {
        case UNUSED:
            commandAdd(cmd);
            part.kit[kititem].adpars->paramsChanged();
            break;
        case TOPLEVEL::insert::LFOgroup:
            commandLFO(cmd);
            break;
        case TOPLEVEL::insert::filterGroup:
            commandFilter(cmd);
            break;
        case TOPLEVEL::insert::envelopeGroup:
        case TOPLEVEL::insert::envelopePointAdd:
        case TOPLEVEL::insert::envelopePointDelete:
        case TOPLEVEL::insert::envelopePointChange:
            commandEnvelope(cmd);
            break;
        case TOPLEVEL::insert::resonanceGroup:
        case TOPLEVEL::insert::resonanceGraphInsert:
            commandResonance(cmd, part.kit[kititem].adpars->GlobalPar.Reson);
            part.kit[kititem].adpars->paramsChanged();
            break;
        }
    return true;
}


bool InterChange::processVoice(CommandBlock& cmd, SynthEngine& synth)
{
    Part& part = *synth.part[cmd.data.part];
    int control = cmd.data.control;
    int kititem = cmd.data.kit;
    int engine  = cmd.data.engine;
    switch(cmd.data.insert)
    {
        case UNUSED:
            commandAddVoice(cmd);
            part.kit[kititem].adpars->paramsChanged();
            break;
        case TOPLEVEL::insert::LFOgroup:
            commandLFO(cmd);
            break;
        case TOPLEVEL::insert::filterGroup:
            commandFilter(cmd);
            break;
        case TOPLEVEL::insert::envelopeGroup:
        case TOPLEVEL::insert::envelopePointAdd:
        case TOPLEVEL::insert::envelopePointDelete:
        case TOPLEVEL::insert::envelopePointChange:
            commandEnvelope(cmd);
            break;
        case TOPLEVEL::insert::oscillatorGroup:
        case TOPLEVEL::insert::harmonicAmplitude:
        case TOPLEVEL::insert::harmonicPhase:
            if (engine >= PART::engine::addMod1)
            {
                engine -= PART::engine::addMod1;
                if (control != ADDVOICE::control::modulatorOscillatorSource)
                {
                    int voicechange = part.kit[kititem].adpars->VoicePar[engine].PextFMoscil;
                    if (voicechange != -1)
                    {
                        engine = voicechange;
                        cmd.data.engine = engine +  PART::addMod1;
                    }   // force it to external mod
                }

                commandOscillator(cmd,  part.kit[kititem].adpars->VoicePar[engine].POscilFM);
            }
            else
            {
                engine -= PART::engine::addVoice1;
                if (control != PART::control::sustainPedalEnable)
                {
                    int voicechange = part.kit[kititem].adpars->VoicePar[engine].Pextoscil;
                    if (voicechange != -1)
                    {
                        engine = voicechange;
                        cmd.data.engine = engine | PART::engine::addVoice1;
                    }   // force it to external voice
                }
                commandOscillator(cmd,  part.kit[kititem].adpars->VoicePar[engine].POscil);
            }
            part.kit[kititem].adpars->paramsChanged();
            break;
    }
    return true;
}


bool InterChange::processSub(CommandBlock& cmd, SynthEngine& synth)
{
    Part& part = *synth.part[cmd.data.part];
    int kititem = cmd.data.kit;
    bool write  = (cmd.data.type & TOPLEVEL::type::Write) > 0;
    switch(cmd.data.insert)
    {
        case UNUSED:
            commandSub(cmd);
            if (write)
                part.kit[kititem].subpars->paramsChanged();
            break;
        case TOPLEVEL::insert::harmonicAmplitude:
            commandSub(cmd);
            if (write)
                part.kit[kititem].subpars->paramsChanged();
            break;
        case TOPLEVEL::insert::harmonicBandwidth:
            commandSub(cmd);
            if (write)
                part.kit[kititem].subpars->paramsChanged();
            break;
        case TOPLEVEL::insert::filterGroup:
            commandFilter(cmd);
            break;
        case TOPLEVEL::insert::envelopeGroup:
            commandEnvelope(cmd);
            break;
        case TOPLEVEL::insert::envelopePointAdd:
        case TOPLEVEL::insert::envelopePointDelete:
            commandEnvelope(cmd);
            break;
        case TOPLEVEL::insert::envelopePointChange:
            commandEnvelope(cmd);
            break;
    }
    return true;
}

namespace {
    inline PADnoteParameters& getPADnoteParameters(CommandBlock& cmd, SynthEngine& synth)
    {
        size_t partNo = cmd.data.part;
        size_t item   = cmd.data.kit;
        PADnoteParameters* padPars = synth.part[partNo]->kit[item].padpars;
        assert (padPars);
        return *padPars;
    }
}

bool InterChange::processPad(CommandBlock& cmd)
{
    PADnoteParameters& pars = getPADnoteParameters(cmd, synth);

    bool needApply{false};
    switch(cmd.data.insert)
    {
        case UNUSED:
            needApply = commandPad(cmd, pars);
            pars.paramsChanged();
            break;
        case TOPLEVEL::insert::LFOgroup:
            commandLFO(cmd);
            break;
        case TOPLEVEL::insert::filterGroup:
            commandFilter(cmd);
            break;
        case TOPLEVEL::insert::envelopeGroup:
            commandEnvelope(cmd);
            break;
        case TOPLEVEL::insert::envelopePointAdd:
        case TOPLEVEL::insert::envelopePointDelete:
            commandEnvelope(cmd);
            break;
        case TOPLEVEL::insert::envelopePointChange:
            commandEnvelope(cmd);
            break;
        case TOPLEVEL::insert::oscillatorGroup:
            commandOscillator(cmd,  pars.POscil.get());
            pars.paramsChanged();
            needApply = true;
            break;
        case TOPLEVEL::insert::harmonicAmplitude:
            commandOscillator(cmd,  pars.POscil.get());
            pars.paramsChanged();
            needApply = true;
            break;
        case TOPLEVEL::insert::harmonicPhase:
            commandOscillator(cmd,  pars.POscil.get());
            pars.paramsChanged();
            needApply = true;
            break;
        case TOPLEVEL::insert::resonanceGroup:
            commandResonance(cmd, pars.resonance.get());
            pars.paramsChanged();
            needApply = true;
            break;
        case TOPLEVEL::insert::resonanceGraphInsert:
            commandResonance(cmd, pars.resonance.get());
            pars.paramsChanged();
            needApply = true;
            break;
    }
    if (needApply and (cmd.data.type & TOPLEVEL::type::Write))
    {
        PADStatus::mark(PADStatus::DIRTY, *this, pars.partID, pars.kitID);

        if (synth.getRuntime().usePadAutoApply())
        {// »Auto Apply« - trigger rebuilding of wavetable on each relevant change
            synth.getRuntime().Log("PADSynth: trigger background wavetable build...");
            pars.buildNewWavetable();
        }
        cmd.data.offset = 0;
    }
    return true;
}


void InterChange::commandMidi(CommandBlock& cmd)
{
    int value_int = lrint(cmd.data.value);
    uchar control = cmd.data.control;
    uchar chan    = cmd.data.kit;
    uint  char1   = cmd.data.engine;
    uchar miscmsg = cmd.data.miscmsg;

    if (control == MIDI::control::controller && char1 >= 0x80)
        char1 |= 0x200; // for 'specials'

    switch(control)
    {
        case MIDI::control::noteOn:
            synth.NoteOn(chan, char1, value_int);
            synth.getRuntime().finishedCLI = true;
            cmd.data.source = TOPLEVEL::action::noAction; // till we know what to do!
            break;
        case MIDI::control::noteOff:
            synth.NoteOff(chan, char1);
            synth.getRuntime().finishedCLI = true;
            cmd.data.source = TOPLEVEL::action::noAction; // till we know what to do!
            break;
        case MIDI::control::controller:
            synth.SetController(chan, char1, value_int);
            break;

        case MIDI::control::instrument:
            cmd.data.source |= TOPLEVEL::action::lowPrio;
            cmd.data.part = TOPLEVEL::section::midiIn;
            synth.partonoffLock(chan & 0x3f, -1);
            synth.getRuntime().finishedCLI = true;
            break;

        case MIDI::control::bankChange:
            cmd.data.source = TOPLEVEL::action::lowPrio;
            if ((value_int != UNUSED || miscmsg != NO_MSG) && chan < synth.getRuntime().numAvailableParts)
            {
                synth.partonoffLock(chan & 0x3f, -1);
                synth.getRuntime().finishedCLI = true;
            }
            break;
    }
}


void InterChange::vectorClear(int Nvector)
{
    int start;
    int end;
    if (Nvector >= NUM_MIDI_CHANNELS)
    {
        start = 0;
        end = NUM_MIDI_CHANNELS;
    }
    else
    {
        start = Nvector;
        end = Nvector + 1;
    }
    for (int ch = start; ch < end; ++ ch)
    {
        synth.getRuntime().vectordata.Xaxis[ch] = UNUSED;
        synth.getRuntime().vectordata.Yaxis[ch] = UNUSED;
        synth.getRuntime().vectordata.Xfeatures[ch] = 0;
        synth.getRuntime().vectordata.Yfeatures[ch] = 0;
        synth.getRuntime().vectordata.Enabled[ch] = false;
        synth.getRuntime().vectordata.Name[ch] = "No Name " + to_string(ch + 1);
    }
}


void InterChange::commandVector(CommandBlock& cmd)
{
    int value     = cmd.data.value; // no floats here
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    uint  chan    = cmd.data.parameter;
    bool  write   = (type & TOPLEVEL::type::Write) > 0;
    uint features = 0;

    if (control == VECTOR::control::erase)
    {
        vectorClear(chan);
        return;
    }
    if (write)
    {
        if (control >= VECTOR::control::Xfeature0 && control <= VECTOR::control::Xfeature3)
            features = synth.getRuntime().vectordata.Xfeatures[chan];
        else if (control >= VECTOR::control::Yfeature0 && control <= VECTOR::control::Yfeature3)
            features = synth.getRuntime().vectordata.Yfeatures[chan];
    }

    switch (control)
    {
        case 0:
            break;
        case 1:
            if (write)
            {
                switch (value)
                {
                    case 0:
                    case 1:
                    case 2: // local to source
                        break;
                    case 3:
                        synth.vectorSet(127, chan, 0);
                        break;
                    case 4:
                        for (int ch = 0; ch < NUM_MIDI_CHANNELS; ++ ch)
                            synth.vectorSet(127, ch, 0);
                        break;
                }
            }
            break;

        case VECTOR::control::name:
            break; // handled elsewhere

        case VECTOR::control::Xcontroller: // also enable vector
            if (write)
            {
                if (value >= 14)
                {
                    if (!synth.vectorInit(0, chan, value))
                        synth.vectorSet(0, chan, value);
                    else
                        cmd.data.value = 0;
                }
            }
            else
            {
                ;
            }
            break;
        case VECTOR::control::XleftInstrument:
            if (write)
                synth.vectorSet(4, chan, value);
            break;
        case VECTOR::control::XrightInstrument:
            if (write)
                synth.vectorSet(5, chan, value);
            break;
        case VECTOR::control::Xfeature0:
        case VECTOR::control::Yfeature0: // volume
            if (write)
            {   if (value == 0)
                    bitClear(features, 0);
                else
                    bitSet(features, 0);
            }
            break;
        case VECTOR::control::Xfeature1:
        case VECTOR::control::Yfeature1: // panning
            if (write)
            {
                bitClear(features, 1);
                bitClear(features, 4);
                if (value > 0)
                {
                    bitSet(features, 1);
                    if (value == 2)
                        bitSet(features, 4);
                }
            }
            else
            {
                ;
            }
            break;
        case VECTOR::control::Xfeature2:
        case VECTOR::control::Yfeature2: // filter cutoff
            if (write)
            {
                bitClear(features, 2);
                bitClear(features, 5);
                if (value > 0)
                {
                    bitSet(features, 2);
                    if (value == 2)
                        bitSet(features, 5);
                }
            }
            else
            {
                ;
            }
            break;
        case VECTOR::control::Xfeature3:
        case VECTOR::control::Yfeature3: // modulation
            if (write)
            {
                bitClear(features, 3);
                bitClear(features, 6);
                if (value > 0)
                {
                    bitSet(features, 3);
                    if (value == 2)
                        bitSet(features, 6);
                }
            }
            else
            {
                ;
            }
            break;

        case VECTOR::control::Ycontroller: // also enable Y
            if (write)
            {
                if (value >= 14)
                {
                    if (!synth.vectorInit(1, chan, value))
                        synth.vectorSet(1, chan, value);
                    else
                        cmd.data.value = 0;
                }
            }
            else
            {
                ;
            }
            break;
        case VECTOR::control::YupInstrument:
            if (write)
                synth.vectorSet(6, chan, value);
            else
            {
                ;
            }
            break;
        case VECTOR::control::YdownInstrument:
            if (write)
                synth.vectorSet(7, chan, value);
            else
            {
                ;
            }
            break;
    }

    if (write)
    {
        if (control >= VECTOR::control::Xfeature0 && control <= VECTOR::control::Xfeature3)
            synth.getRuntime().vectordata.Xfeatures[chan] = features;
        else if (control >= VECTOR::control::Yfeature0 && control <= VECTOR::control::Yfeature3)
            synth.getRuntime().vectordata.Yfeatures[chan] = features;
    }
}


void InterChange::commandMicrotonal(CommandBlock& cmd)
{
    float value = cmd.data.value;
    uchar type = cmd.data.type;
    uchar control = cmd.data.control;

    bool write = (type & TOPLEVEL::type::Write) > 0;

    int value_int = lrint(value);
    bool value_bool = _SYS_::F2B(value);
    bool retune = false;
    switch (control)
    {
        case SCALES::control::refFrequency:
            if (write)
            {
                if (value > 2000)
                    value = 2000;
                else if (value < 1)
                    value = 1;
                synth.microtonal.PrefFreq = value;
                retune = true;
            }
            else
                value = synth.microtonal.PrefFreq;
            cmd.data.parameter = synth.microtonal.PrefNote;
            break;

        case SCALES::control::refNote:
            if (write)
            {
                synth.microtonal.PrefNote = value_int;
                retune = true;
            }
            else
                value = synth.microtonal.PrefNote;
            break;
        case SCALES::control::invertScale:
            if (write)
            {
                synth.microtonal.Pinvertupdown = value_bool;
                retune = true;
            }
            else
                value = synth.microtonal.Pinvertupdown;
            break;
        case SCALES::control::invertedScaleCenter:
            if (write)
            {
                synth.microtonal.Pinvertupdowncenter = value_int;
                retune = true;
            }
            else
                value = synth.microtonal.Pinvertupdowncenter;
            break;
        case SCALES::control::scaleShift:
            if (write)
            {
                synth.microtonal.Pscaleshift = value_int + 64;
                retune = true;
            }
            else
                value = synth.microtonal.Pscaleshift - 64;
            break;

        case SCALES::control::enableMicrotonal:
            if (write)
            {
                synth.microtonal.Penabled = value_bool;
                synth.microtonal.Pmappingenabled = false;
                retune = true;
            }
            else
                value = synth.microtonal.Penabled;
            break;

        case SCALES::control::enableKeyboardMap:
            if (write)
            {
                synth.microtonal.Pmappingenabled = value_bool;
                retune = true;
            }
            else
               value = synth.microtonal.Pmappingenabled;
            break;
        case SCALES::control::lowKey:
            if (write)
            {
                if (value_int < 0)
                {
                    value_int = 0;
                    cmd.data.value = value_int;
                }
                else if (value_int > synth.microtonal.Pmiddlenote)
                {
                    value_int = synth.microtonal.Pmiddlenote;
                    cmd.data.value = value_int;
                }
                synth.microtonal.Pfirstkey = value_int;
            }
            else
                value = synth.microtonal.Pfirstkey;
            break;
        case SCALES::control::middleKey:
            if (write)
            {
                if (value_int < synth.microtonal.Pfirstkey)
                {
                    value_int = synth.microtonal.Pfirstkey;
                    cmd.data.value = value_int;
                }
                else if (value_int > synth.microtonal.Plastkey)
                {
                    value_int = synth.microtonal.Plastkey;
                    cmd.data.value = value_int;
                }
                synth.microtonal.Pmiddlenote = value_int;
                retune = true;
            }
            else
                value = synth.microtonal.Pmiddlenote;
            break;
        case SCALES::control::highKey:
            if (write)
            {
                if (value_int < synth.microtonal.Pmiddlenote)
                {
                    value_int = synth.microtonal.Pmiddlenote;
                    cmd.data.value = value_int;
                }
                else if (value_int >= MAX_OCTAVE_SIZE)
                {
                    value_int = MAX_OCTAVE_SIZE - 1;
                    cmd.data.value = value_int;
                }
                synth.microtonal.Plastkey = value_int;
            }
            else
                value = synth.microtonal.Plastkey;
            break;

        case SCALES::control::tuning:
            // done elsewhere
            break;
        case SCALES::control::keyboardMap:
            // done elsewhere
            break;

        case SCALES::control::keymapSize:
            // done elsewhere
            break;

        case SCALES::control::importScl:
            // done elsewhere
            break;
        case SCALES::control::importKbm:
            // done elsewhere
            break;

        case SCALES::control::name:
            // done elsewhere
            break;
        case SCALES::control::comment:
            // done elsewhere
            break;

        case SCALES::control::clearAll: // Clear scales
            synth.microtonal.defaults();
            retune = true;
            break;
    }
    if (write)
    {
        if (retune)
            synth.setAllPartMaps();
    }
    else
        cmd.data.value = value;
}


void InterChange::commandConfig(CommandBlock& cmd)
{
    float value   = cmd.data.value;
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;

    bool write = (type & TOPLEVEL::type::Write) > 0;

    bool mightChange = true;
    int  value_int  = lrint(value);
    bool value_bool = _SYS_::F2B(value);

    switch (control)
    {
// main
        case CONFIG::control::oscillatorSize:
            if (write)
            {
                value = nearestPowerOf2(value_int, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);
                cmd.data.value = value;
                synth.getRuntime().oscilsize = value;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().oscilsize;
            break;
        case CONFIG::control::bufferSize:
            if (write)
            {
                value = nearestPowerOf2(value_int, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);
                cmd.data.value = value;
                synth.getRuntime().buffersize = value;
                synth.getRuntime().updateConfig(control, value);
            }
            else
            {
                value = synth.getRuntime().buffersize;
            }
            break;
        case CONFIG::control::padSynthInterpolation:
            if (write)
            {
                 synth.getRuntime().Interpolation = value_bool;
                 synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().Interpolation;
            break;
        case CONFIG::control::virtualKeyboardLayout:
            if (write)
            {
                 synth.getRuntime().virKeybLayout = value_int;
                 synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().virKeybLayout;
            break;
        case CONFIG::control::reportsDestination:
            if (write)
            {
                 synth.getRuntime().toConsole = value_bool;
                 synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().toConsole;
            break;
        case CONFIG::control::logTextSize:
            if (write)
            {
                 synth.getRuntime().consoleTextSize = value_int;
                 synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().consoleTextSize;
            break;
        case CONFIG::control::savedInstrumentFormat:
            if (write)
            {
                 synth.getRuntime().instrumentFormat = value_int;
                 synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().instrumentFormat;
            break;
        case CONFIG::control::handlePadSynthBuild:
            if (write)
            {
                synth.getRuntime().handlePadSynthBuild = value_int;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().handlePadSynthBuild;
            break;
// switches
        case CONFIG::control::enableGUI:
            if (write)
            {
                synth.getRuntime().storedGui = value_bool;
                synth.getRuntime().showGui = value_bool;
                synth.getRuntime().updateConfig(control, value);
            }
            else
                value = synth.getRuntime().showGui;
            break;
        case CONFIG::control::enableCLI:
            if (write)
            {
                synth.getRuntime().storedCli = value_bool;
                synth.getRuntime().showCli = value_bool;
                synth.getRuntime().updateConfig(control, value);
            }
            else
                value = synth.getRuntime().showCli;
            break;
        case CONFIG::control::showSplash:
            if (write)
            {
                synth.getRuntime().updateConfig(control, value);
                synth.getRuntime().showSplash = value;
            }
            else
                value = synth.getRuntime().showSplash;
            break;
        case CONFIG::control::enableSinglePath:
            if (write)
            {
                synth.getRuntime().singlePath = value;
                synth.getRuntime().updateConfig(control, value);
            }
            else
                value = synth.getRuntime().singlePath;
            break;
        case CONFIG::control::enableAutoInstance:
            if (write)
            {
                synth.getRuntime().autoInstance = value;
                synth.getRuntime().updateConfig(control, value);
            }
            else
                value = synth.getRuntime().autoInstance;
            break;
        case CONFIG::control::exposeStatus:
            if (write)
            {
                synth.getRuntime().showCLIcontext = value;
                synth.getRuntime().updateConfig(control, value);
            }
            else
                value = Config::primary().showCLIcontext;
            break;
        case CONFIG::control::XMLcompressionLevel:
            if (write)
            {
                synth.getRuntime().gzipCompression = value;
                synth.getRuntime().updateConfig(control, value);
            }
            else
                value = synth.getRuntime().gzipCompression;
            break;

        case CONFIG::control::defaultStateStart:
            if (write)
            {
                synth.getRuntime().loadDefaultState = value_bool;
                synth.getRuntime().updateConfig(control, value);
            }
            else
                value = synth.getRuntime().loadDefaultState;
            break;
        case CONFIG::control::hideNonFatalErrors:
            if (write)
            {
                synth.getRuntime().hideErrors = value_bool;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().hideErrors;
            break;
        case CONFIG::control::logInstrumentLoadTimes:
            if (write)
            {
                synth.getRuntime().showTimes = value_bool;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().showTimes;
            break;
        case CONFIG::control::logXMLheaders:
            if (write)
            {
                synth.getRuntime().logXMLheaders = value_bool;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().logXMLheaders;
            break;
        case CONFIG::control::saveAllXMLdata:
            if (write)
            {
                synth.getRuntime().xmlmax = value_bool;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().xmlmax;
            break;
        case CONFIG::control::enableHighlight:
            if (write)
            {
                synth.getRuntime().bankHighlight = value;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().bankHighlight;
            break;

        case CONFIG::control::readAudio:
        {
            value = int(synth.getRuntime().audioEngine);
            synth.getRuntime().updateConfig(control, value_int);
        }
            break;
        case CONFIG::control::readMIDI:
        {
            value = int(synth.getRuntime().midiEngine);
            synth.getRuntime().updateConfig(control, value_int);
        }
            break;
// jack
        case CONFIG::control::jackMidiSource: // done elsewhere
            break;
        case CONFIG::control::jackPreferredMidi:
            if (write)
            {
                if (value_bool)
                {
                    synth.getRuntime().midiEngine = jack_midi;
                    synth.getRuntime().updateConfig(CONFIG::control::readMIDI, jack_midi);
                }
                else
                {
                    synth.getRuntime().midiEngine = alsa_midi;
                    synth.getRuntime().updateConfig(CONFIG::control::readMIDI, alsa_midi);
                }
            }
            else
                value = (synth.getRuntime().midiEngine == jack_midi);
            break;
        case CONFIG::control::jackServer: // done elsewhere
            break;
        case CONFIG::control::jackPreferredAudio:
            if (write)
            {
                if (value_bool)
                {
                    synth.getRuntime().audioEngine = jack_audio;
                    synth.getRuntime().updateConfig(CONFIG::control::readAudio, jack_audio);
                }
                else
                {
                    synth.getRuntime().audioEngine = alsa_audio;
                    synth.getRuntime().updateConfig(CONFIG::control::readAudio, alsa_audio);
                }
            }
            else
                value = (synth.getRuntime().audioEngine == jack_audio);
            break;
        case CONFIG::control::jackAutoConnectAudio:
            if (write)
            {
                synth.getRuntime().connectJackaudio = value_bool;
                synth.getRuntime().audioEngine = jack_audio;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().connectJackaudio;
            break;
// alsa
        case CONFIG::control::alsaMidiSource: // done elsewhere
            break;
        case CONFIG::control::alsaPreferredMidi:
            if (write)
            {
                if (value_bool)
                {
                    synth.getRuntime().midiEngine = alsa_midi;
                    synth.getRuntime().updateConfig(CONFIG::control::readMIDI, alsa_midi);
                }
                else
                {
                    synth.getRuntime().midiEngine = jack_midi;
                    synth.getRuntime().updateConfig(CONFIG::control::readMIDI, jack_midi);
                }
            }
            else
                value = (synth.getRuntime().midiEngine == alsa_midi);
            break;
        case CONFIG::control::alsaMidiType:
            if (write)
            {
                synth.getRuntime().alsaMidiType = value_int;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().alsaMidiType;
            break;

        case CONFIG::control::alsaAudioDevice: // done elsewhere
            break;
        case CONFIG::control::alsaPreferredAudio:
            if (write)
            {
                if (value_bool)
                {
                    synth.getRuntime().audioEngine = alsa_audio;
                    synth.getRuntime().updateConfig(CONFIG::control::readAudio, alsa_audio);
                }
                else
                {
                    synth.getRuntime().audioEngine = jack_audio;
                    synth.getRuntime().updateConfig(CONFIG::control::readAudio, jack_audio);
                }

            }
            else
                value = (synth.getRuntime().audioEngine == alsa_audio);
            break;
        case CONFIG::control::alsaSampleRate:
            if (write)
            {
                switch(value_int)
                {
                    case 0:
                        value = 192000;
                        break;
                    case 1:
                        value = 96000;
                        break;
                    case 2:
                        value = 48000;
                        break;
                    case 3:
                        value = 44100;
                        break;
                    default:
                        value = 44100;
                        break;
                }
                synth.getRuntime().samplerate = value;
                cmd.data.value = value;
                synth.getRuntime().updateConfig(control, value);
            }
            else
                switch(synth.getRuntime().samplerate)
                {
                    case 192000:
                        value = 0;
                        break;
                    case 96000:
                        value = 1;
                        break;
                    case 48000:
                        value = 2;
                        break;
                    case 44100:
                        value = 3;
                        break;
                    default:
                        value = 3;
                        break;
                }
            break;
// midi
        case CONFIG::control::bankRootCC:
            if (write)
            {
                if (value_int != 0 && value_int != 32)
                {
                    value_int = 128;
                    cmd.data.value = value_int;
                }
                synth.getRuntime().midi_bank_root = value_int;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().midi_bank_root;
            break;

        case CONFIG::control::bankCC:
            if (write)
            {
                if (value_int != 0 && value_int != 32)
                {
                    value_int = 128;
                    cmd.data.value = value_int;
                }
                synth.getRuntime().midi_bank_C = value_int;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().midi_bank_C;
            break;
        case CONFIG::control::enableProgramChange:
            if (write)
            {
                synth.getRuntime().enableProgChange = value_bool;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().enableProgChange;
            break;
        case CONFIG::control::extendedProgramChangeCC:
            if (write)
            {
                if (value_int > 119)
                {
                    value_int = 128;
                    cmd.data.value = value_int;
                }
                synth.getRuntime().midi_upper_voice_C = value_int;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().midi_upper_voice_C;
            break;
        case CONFIG::control::ignoreResetAllCCs:
            if (write)
            {
                synth.getRuntime().ignoreResetCCs = value_bool;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().ignoreResetCCs;
            break;
        case CONFIG::control::logIncomingCCs:
            if (write)
            {
                synth.getRuntime().monitorCCin = value_bool;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().monitorCCin;
            break;
        case CONFIG::control::showLearnEditor:
            if (write)
            {
                synth.getRuntime().showLearnedCC = value_bool;
                synth.getRuntime().updateConfig(control, value_bool);
            }
            else
                value = synth.getRuntime().showLearnedCC;
            break;
        case CONFIG::control::enableNRPNs:
            if (write)
            {
                synth.getRuntime().enable_NRPN = value_bool;
                synth.getRuntime().updateConfig(control, value_int);
            }
            else
                value = synth.getRuntime().enable_NRPN;
            break;
// save config
        case CONFIG::control::saveCurrentConfig: //done elsewhere
            break;
        default:
            mightChange = false;
        break;
    }
    if (!write)
        cmd.data.value = value;
    else if (mightChange)
        synth.getRuntime().configChanged = true;
}


void InterChange::commandMain(CommandBlock& cmd)
{
    float value   = cmd.data.value;
    uchar type    = cmd.data.type;
    uchar action  = cmd.data.source;
    uchar control = cmd.data.control;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    int value_int = lrint(value);

    switch (control)
    {
        case MAIN::control::volume:
            if (write)
            {
                add2undo(cmd, noteSeen);
                synth.setPvolume(value);
            }
            else
                value = synth.Pvolume;
            break;

        case MAIN::control::partNumber:
            if (write)
            {   // from various causes which change the current active part
                synth.getRuntime().currentPart = value_int;
                synth.pushEffectUpdate(value_int);
            }           // send current part-effect data to GUI
            else
                value = synth.getRuntime().currentPart;
            break;
        case MAIN::control::availableParts:
            if ((write) && (value == 16 || value == 32 || value == 64))
            {
                if (value < synth.getRuntime().numAvailableParts)
                    undoRedoClear(); // references might no longer exist
                synth.getRuntime().numAvailableParts = value;
                // Note: in MasterUI::updatepart() the current part number
                //       will possibly be capped, causing npartcounter->do_callback();
                //       to send a command MAIN::control::partNumber ...
            }
            else
                value = synth.getRuntime().numAvailableParts;
            break;
        case MAIN::control::panLawType:
            if (write)
                synth.getRuntime().panLaw = value_int;
            else
                value = synth.getRuntime().panLaw;
            break;


        case MAIN::control::detune: // writes indirect
            value = synth.microtonal.Pglobalfinedetune;
            break;
        case MAIN::control::keyShift: // done elsewhere
            break;

        case MAIN::control::bpmFallback:
            if (write)
                synth.PbpmFallback = value;
            else
                value = synth.PbpmFallback;
            break;

        case MAIN::control::mono:
            if (write)
                synth.masterMono = value;
            else
                value = synth.masterMono;
            break;

        case MAIN::control::reseed:
            synth.setReproducibleState(int(value));
            break;

        case MAIN::control::soloType:
            if (write && value_int <= MIDI::SoloType::Channel)
            {
                synth.getRuntime().channelSwitchType = value_int;
                synth.getRuntime().channelSwitchCC = 128;
                synth.getRuntime().channelSwitchValue = 0;
                switch (value_int)
                {
                    case MIDI::SoloType::Disabled:
                        for (int i = 0; i < NUM_MIDI_PARTS; ++i)
                            synth.part[i]->Prcvchn = (i & (NUM_MIDI_CHANNELS - 1));
                        break;

                    case MIDI::SoloType::Row:
                        for (int i = 1; i < NUM_MIDI_CHANNELS; ++i)
                            synth.part[i]->Prcvchn = NUM_MIDI_CHANNELS;
                        synth.part[0]->Prcvchn = 0;
                        break;

                    case MIDI::SoloType::Column:
                        for (int i = 0; i < NUM_MIDI_PARTS; ++i)
                            synth.part[i]->Prcvchn = (i & (NUM_MIDI_CHANNELS - 1));
                        break;

                    case MIDI::SoloType::Loop:
                    case MIDI::SoloType::TwoWay:
                        for (int i = 0; i < NUM_MIDI_CHANNELS; ++i)
                            synth.part[i]->Prcvchn = NUM_MIDI_CHANNELS;
                        synth.part[0]->Prcvchn = 0;
                        break;

                    case MIDI::SoloType::Channel:
                        for (int p = 0; p < NUM_MIDI_PARTS; ++p)
                        {
                            if (synth.part[p]->Prcvchn >= NUM_MIDI_CHANNELS)
                                synth.part[p]->Prcvchn = p &(NUM_MIDI_CHANNELS - 1);
                        }
                        break;
                }
            }
            else
            {
                write = false; // for an invalid write attempt
                value = synth.getRuntime().channelSwitchType;
            }
            break;
        case MAIN::control::soloCC:
            if (write && synth.getRuntime().channelSwitchType > 0)
                synth.getRuntime().channelSwitchCC = value_int;
            else
            {
                write = false; // for an invalid write attempt
                value = synth.getRuntime().channelSwitchCC;
            }
            break;

        case MAIN::control::loadInstrumentFromBank:
            synth.partonoffLock(kititem, -1);
            //std::cout << "Main bank ins load" << std::endl;
            cmd.data.source |= TOPLEVEL::action::lowPrio;
            break;

        case MAIN::control::loadInstrumentByName:
            synth.partonoffLock(kititem, -1);
            //std::cout << "Main ins load" << std::endl;
            cmd.data.source |= TOPLEVEL::action::lowPrio;
            break;

        case MAIN::control::loadNamedPatchset:
            if (write && ((action & TOPLEVEL::action::muteAndLoop) == TOPLEVEL::action::muteAndLoop))
            {
                muteQueueWrite(cmd);
                cmd.data.source = TOPLEVEL::action::noAction;
            }
            break;

        case MAIN::control::loadNamedVector:
            if (write && ((action & TOPLEVEL::action::muteAndLoop) == TOPLEVEL::action::muteAndLoop))
            {
                muteQueueWrite(cmd);
                cmd.data.source = TOPLEVEL::action::noAction;
            }
            break;
        case MAIN::control::saveNamedVector: // done elsewhere
            break;
        case MAIN::control::loadNamedScale: // done elsewhere
            break;
        case MAIN::control::saveNamedScale: // done elsewhere
            break;
        case MAIN::control::loadNamedState:
            if (write && ((action & TOPLEVEL::action::muteAndLoop) == TOPLEVEL::action::muteAndLoop))
            {
                muteQueueWrite(cmd);
                cmd.data.source = TOPLEVEL::action::noAction;
            }
            break;
        case MAIN::control::saveNamedState: // done elsewhere
            break;
        case MAIN::control::readLastSeen: // read only
            value = textMsgBuffer.push(synth.lastItemSeen(value));
            break;
        case MAIN::control::loadFileFromList:
            muteQueueWrite(cmd);
            cmd.data.source = TOPLEVEL::action::noAction;
            break;

        case MAIN::control::defaultPart: // clear entire part
            if (write)
            {
                synth.partonoffWrite(value_int, -1);
                cmd.data.source = TOPLEVEL::action::lowPrio;
            }
            else
                cmd.data.source = TOPLEVEL::action::noAction;
            break;

        case MAIN::control::defaultInstrument: // clear part's instrument
            if (write)
            {
                synth.partonoffWrite(value_int, -1);
                cmd.data.source = TOPLEVEL::action::lowPrio;
            }
            else
                cmd.data.source = TOPLEVEL::action::noAction;
            break;

        case MAIN::control::masterReset:
        case MAIN::control::masterResetAndMlearn:
            if (write && ((action & TOPLEVEL::action::muteAndLoop) == TOPLEVEL::action::muteAndLoop))
            {
                muteQueueWrite(cmd);
                cmd.data.source = TOPLEVEL::action::noAction;
            }
            break;
        case TOPLEVEL::control::dataExchange:
            // this trigger is sent immediately after a new instance becomes operational
            synth.postBootHook(cmd.data.parameter);
            cmd.data.source  = TOPLEVEL::action::toAll | TOPLEVEL::action::forceUpdate;
                              //    cause InterChange::returns() to also to forward this into GUI -> MasterUI::refreshInit()
            break;
        case MAIN::control::undo:
        case MAIN::control::redo:
            if ((action & TOPLEVEL::action::muteAndLoop) == TOPLEVEL::action::muteAndLoop)
            {
                muteQueueWrite(cmd);
                cmd.data.source = TOPLEVEL::action::noAction;
            }
            break;
        case MAIN::control::stopSound: // just stop
            if (write)
                muteQueueWrite(cmd);
            cmd.data.source = TOPLEVEL::action::noAction;
            break;

        case MAIN::control::readPartPeak:
            if (!write && kititem < NUM_MIDI_PARTS)
            {
                if (engine == 1)
                    value = synth.VUdata.values.partsR[kititem];
                else
                    value = synth.VUdata.values.parts[kititem];
            }
            break;
        case MAIN::control::readMainLRpeak:
            if (!write)
            {
                if (kititem == 1)
                    value = synth.VUdata.values.vuOutPeakR;
                else
                    value = synth.VUdata.values.vuOutPeakL;
            }
            break;
        case MAIN::control::readMainLRrms:
            if (!write)
            {
                if (kititem == 1)
                    value = synth.VUdata.values.vuRmsPeakR;
                else
                    value = synth.VUdata.values.vuRmsPeakL;
            }
            break;

        case TOPLEVEL::control::textMessage:
            cmd.data.source = TOPLEVEL::action::noAction;
            break;
    }

    if (!write)
        cmd.data.value = value;
}


void InterChange::commandBank(CommandBlock& cmd)
{
    int value_int = int(cmd.data.value + 0.5f);
    uchar type      = cmd.data.type;
    uchar control   = cmd.data.control;
    uchar kititem   = cmd.data.kit;
    uchar engine    = cmd.data.engine;
    uchar parameter = cmd.data.parameter;

    bool write = (type & TOPLEVEL::type::Write) > 0;

    switch (control)
    {
        case BANK::control::readInstrumentName:
        {
            if (kititem == UNUSED)
            {
                kititem = synth.getRuntime().currentBank;
                cmd.data.kit = kititem;
            }
            if (engine == UNUSED)
            {
                engine = synth.getRuntime().currentRoot;
                cmd.data.engine = engine;
            }
            textMsgBuffer.push(synth.bank.getname(parameter, kititem, engine));
            break;
        }
        case BANK::control::findInstrumentName:
        {
            if (parameter == UNUSED) // return the name of a specific instrument.
                textMsgBuffer.push(synth.bank.getname(value_int, kititem, engine));
            else
            {
                int offset = type_offset [parameter];
                /*
                 * This version of the call is for building up lists of instruments that match the given type.
                 * It will find the next in the series until the entire bank structure has been scanned.
                 * It returns the terminator when this has been completed so the calling function knows the
                 * entire list has been scanned, and resets ready for a new set of calls.
                 */

                if (offset == -1)
                {
                    synth.getRuntime().Log("caught invalid instrument type (-1)");
                    textMsgBuffer.push("@end");
                }

                do {
                    do {
                        do {
                            if (synth.bank.getType(searchInst, searchBank, searchRoot) == offset)
                            {
                                textMsgBuffer.push(asString(searchRoot, 3) + ": " + asString(searchBank, 3) + ". " + asString(searchInst + 1, 3) + "  " + synth.bank.getname(searchInst, searchBank, searchRoot));
                                ++ searchInst;
                                return;
                                /*
                                 * notice this exit point!
                                 */
                            }
                            ++searchInst;
                        } while (searchInst < MAX_INSTRUMENTS_IN_BANK);

                        searchInst = 0;
                        ++searchBank;
                    } while (searchBank < MAX_BANKS_IN_ROOT);
                    searchBank = 0;
                    ++searchRoot;
                } while (searchRoot < MAX_BANK_ROOT_DIRS);
                searchRoot = 0;
                textMsgBuffer.push("@end");
            }
            break;
        }
        case BANK::control::lastSeenInBank: // read only
            value_int = synth.getRuntime().lastBankPart;
            break;
        case BANK::control::selectBank: // done elsewhere for write
            value_int = synth.ReadBank();
            break;
        case BANK::control::selectRoot:
            value_int = synth.getRuntime().currentRoot; // currently read only
            break;
        default:
            cmd.data.source = TOPLEVEL::action::noAction;
            break;
    }

    if (!write)
        cmd.data.value = value_int;
}


void InterChange::commandPart(CommandBlock& cmd)
{
    float value   = cmd.data.value;
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;
    uchar insert  = cmd.data.insert;

    bool write = (type & TOPLEVEL::type::Write) > 0;

    bool kitType = (insert == TOPLEVEL::insert::kitGroup);

    if (kitType && kititem >= NUM_KIT_ITEMS)
    {
        cmd.data.source = TOPLEVEL::action::noAction;
        synth.getRuntime().Log("Invalid kit number");
        return;
    }
    int value_int = lrint(value);
    char value_bool = _SYS_::F2B(value);

    assert(npart < NUM_MIDI_PARTS);
    Part& part{* synth.part[npart]};
    if (not part.Pkitmode)
    {
        kitType = false;
        if (control != PART::control::kitMode && kititem != UNUSED)
        {
            cmd.data.source = TOPLEVEL::action::noAction;
            synth.getRuntime().Log("Not in kit mode");
        }
    }
    else if (control != PART::control::enableKitLine && !part.kit[kititem].Penabled && kititem < UNUSED)
    {
        cmd.data.source = TOPLEVEL::action::noAction;
        synth.getRuntime().Log("Kit item " +  to_string(kititem + 1) + " not enabled");
        return;
    }

    if (write)
    {
        /*
         * The following is not quite correct.the sections will be inactive
         * but still present, so although an undo might appear to do nothing
         * it won't actually cause a problem.
         *
        if (control == PART::control::enableKitLine || control == PART::control::kitMode)
            undoRedoClear(); // these would become completely invalid!
        else
            */
        if (control == PART::control::resetAllControllers)
        { // setup for group undo
            CommandBlock resetCmd;
            memset(resetCmd.bytes, 255, sizeof(CommandBlock));
            resetCmd.data.source = TOPLEVEL::action::forceUpdate;
            resetCmd.data.part = npart;

            for (int contl = PART::control::volumeRange; contl < PART::control::resetAllControllers; ++contl)
            {
                noteSeen = true;
                resetCmd.data.value = 0;
                resetCmd.data.type = 0;
                resetCmd.data.control = contl;

                commandControllers(resetCmd, false);

                resetCmd.data.type |= TOPLEVEL::type::Write;
                if (contl == PART::control::volumeRange)
                    add2undo(resetCmd, noteSeen);
                else
                    add2undo(resetCmd, noteSeen, true);
            }
        }
        else
            add2undo(cmd, noteSeen);
    }

    if (control >= PART::control::volumeRange && control < PART::control::resetAllControllers)
    {
        commandControllers(cmd, write);
        return;
    }

    uchar effNum = part.Peffnum;
    if (!kitType)
        kititem = 0;

    switch (control)
    {
        case PART::control::enable:
            if (write)
            {
                if (value_bool && synth.getRuntime().currentPart != npart) // make it a part change
                {
                    synth.partonoffWrite(npart, 1);
                    synth.getRuntime().currentPart = npart;
                    cmd.data.value = npart;
                    cmd.data.control = MAIN::control::partNumber;
                    cmd.data.part = TOPLEVEL::section::main;
                    synth.pushEffectUpdate(npart); // send current part-effect data to GUI
                }
                else
                    synth.partonoffWrite(npart, value_int);
            }
            else
                value = synth.partonoffRead(npart);
            break;
        case PART::control::enableAdd:
            if (write)
                part.kit[kititem].Padenabled = value_bool;
            else
                value = part.kit[kititem].Padenabled;
            break;
        case PART::control::enableSub:
            if (write)
                part.kit[kititem].Psubenabled = value_bool;
            else
                value = part.kit[kititem].Psubenabled;
            break;
        case PART::control::enablePad:
            if (write && (part.kit[kititem].Ppadenabled != value_bool))
            {
                part.kit[kititem].Ppadenabled = value_bool;
                if (synth.getRuntime().useLegacyPadBuild())
                {// do the blocking build in the CMD-Dispatch background thread ("sortResultsThread")
#ifdef GUI_FLTK
                    toGUI.write(cmd.bytes);                      // cause update in the GUI to enable the edit button
#endif
                    cmd.data.source = TOPLEVEL::action::lowPrio; // marker to cause dispatch in InterChange::sortResultsThread()
                    cmd.data.control = PADSYNTH::control::applyChanges;
                }
                else
                    part.kit[kititem].padpars->buildNewWavetable();  // this triggers a rebuild via background thread
            }
            else
                value = part.kit[kititem].Ppadenabled;
            break;
        case PART::control::enableKitLine:
            if (write)
            {
                if (!_SYS_::F2B(value))
                    undoRedoClear();
                synth.partonoffWrite(npart, -1);
                cmd.data.source = TOPLEVEL::action::lowPrio;
            }
            else
                value = part.kit[kititem].Penabled;
            break;

        case PART::control::volume:
            if (write)
                part.setVolume(value);
            else
                value = part.Pvolume;
            break;
        case PART::control::velocitySense:
            if (write)
                part.Pvelsns = value;
            else
                value = part.Pvelsns;
            break;
        case PART::control::panning:
            if (write)
                part.SetController(MIDI::CC::panning, value);
            else
                value = part.Ppanning;
            break;
        case PART::control::velocityOffset:
            if (write)
                part.Pveloffs = value;
            else
                value = part.Pveloffs;
            break;
        case PART::control::midiChannel:
            if (write)
                part.Prcvchn = value_int;
            else
                value = part.Prcvchn;
            break;
        case PART::control::keyMode:
            if (write)
                synth.SetPartKeyMode(npart, value_int);
            else
                value = (synth.ReadPartKeyMode(npart)) & 3; // clear out temporary legato
            break;
        case PART::control::channelATset:
            if (write)
            {
                part.PchannelATchoice = value_int;
                int tmp1, tmp2;
                tmp1 = tmp2 = part.PkeyATchoice;
                tmp1 &= ~value_int;
                if (tmp1 != tmp2)
                {
                    part.PkeyATchoice  = tmp1; // can't have the same
                    cmd.data.parameter = tmp1; // send possible correction
                }
            }
            else
                value = part.PchannelATchoice;
            break;
        case PART::control::keyATset:
            if (write)
            {
                part.PkeyATchoice = value_int;
                int tmp1, tmp2;
                tmp1 = tmp2 = part.PchannelATchoice;
                tmp1 &= ~value_int;
                if (tmp1 != tmp2)
                {
                    part.PchannelATchoice = tmp1; // can't have the same
                    cmd.data.parameter = tmp1;   //  send possible correction
                }
            }
            else
                value = part.PkeyATchoice;
            break;
        case PART::control::portamento:
            if (write)
                part.ctl->portamento.portamento = value_bool;
            else
                value = part.ctl->portamento.portamento;
            break;
        case PART::control::kitItemMute:
            if (kitType)
            {
                if (write)
                    part.kit[kititem].Pmuted = value_bool;
                else
                    value = part.kit[kititem].Pmuted;
            }
            break;

        case PART::control::minNote: // always return actual value
            if (kitType)
            {
                if (write)
                {
                    if (value_int > part.kit[kititem].Pmaxkey)
                        part.kit[kititem].Pminkey = part.kit[kititem].Pmaxkey;
                    else
                        part.kit[kititem].Pminkey = value_int;
                }
                value = part.kit[kititem].Pminkey;
            }
            else
            {
                if (write)
                {
                    if (value_int > part.Pmaxkey)
                        part.Pminkey = part.Pmaxkey;
                    else
                        part.Pminkey = value_int;
                }
                value = part.Pminkey;
            }
            break;
        case PART::control::maxNote: // always return actual value
            if (kitType)
            {
                if (write)
                {
                    if (value_int < part.kit[kititem].Pminkey)
                        part.kit[kititem].Pmaxkey = part.kit[kititem].Pminkey;
                    else
                        part.kit[kititem].Pmaxkey = value_int;
                }
                value = part.kit[kititem].Pmaxkey;
            }
            else
            {
                if (write)
                {
                    if (value_int < part.Pminkey)
                        part.Pmaxkey = part.Pminkey;
                    else
                        part.Pmaxkey = value_int;
                }
                value = part.Pmaxkey;
            }
            break;
        case PART::control::minToLastKey: // always return actual value
            value_int = part.getLastNote();
            if (kitType)
            {
                if ((write) && value_int >= 0)
                {
                    if (value_int > part.kit[kititem].Pmaxkey)
                        part.kit[kititem].Pminkey = part.kit[kititem].Pmaxkey;
                    else
                        part.kit[kititem].Pminkey = part.getLastNote();
                }
                value = part.kit[kititem].Pminkey;
            }
            else
            {
                if ((write) && part.getLastNote() >= 0)
                {
                    if (value_int > part.Pmaxkey)
                        part.Pminkey = part.Pmaxkey;
                    else
                        part.Pminkey = part.getLastNote();
                }
                value = part.Pminkey;
            }
            break;
        case PART::control::maxToLastKey: // always return actual value
            value_int = part.getLastNote();
            if (kitType)
            {
                if ((write) && part.getLastNote() >= 0)
                {
                    if (value_int < part.kit[kititem].Pminkey)
                        part.kit[kititem].Pmaxkey = part.kit[kititem].Pminkey;
                    else
                        part.kit[kititem].Pmaxkey = part.getLastNote();
                }
                value = part.kit[kititem].Pmaxkey;
            }
            else
            {
                if ((write) && part.getLastNote() >= 0)
                {
                    if (value_int < part.Pminkey)
                        part.Pmaxkey = part.Pminkey;
                    else
                        part.Pmaxkey = part.getLastNote();
                }
                value = part.Pmaxkey;
            }
            break;
        case PART::control::resetMinMaxKey:
            if (kitType)
            {
                if (write)
                {
                    part.kit[kititem].Pminkey = 0;
                    part.kit[kititem].Pmaxkey = 127;
                }
            }
            else
            {
                if (write)
                {
                    part.Pminkey = 0;
                    part.Pmaxkey = 127;
                }
            }
            break;

        case PART::control::kitEffectNum:
            if (kitType)
            {
                if (write)
                {
                    if (value_int == 0 )
                        part.kit[kititem].Psendtoparteffect = 127;
                    else
                        part.kit[kititem].Psendtoparteffect = value_int - 1;
                }
                else
                    value = part.kit[kititem].Psendtoparteffect;
            }
            break;

        case PART::control::maxNotes:
            if (write)
            {
                part.Pkeylimit = value_int;
                if (part.Pkeymode == PART_NORMAL)
                    part.enforcekeylimit();
            }
            else
                value = part.Pkeylimit;
            break;
        case PART::control::keyShift: // done elsewhere
            break;

        case PART::control::partToSystemEffect1:
            if (write)
                synth.setPsysefxvol(npart,0, value);
            else
                value = synth.Psysefxvol[0][npart];
            break;
        case PART::control::partToSystemEffect2:
            if (write)
                synth.setPsysefxvol(npart,1, value);
            else
                value = synth.Psysefxvol[1][npart];
            break;
        case PART::control::partToSystemEffect3:
            if (write)
                synth.setPsysefxvol(npart,2, value);
            else
                value = synth.Psysefxvol[2][npart];
            break;
        case PART::control::partToSystemEffect4:
            if (write)
                synth.setPsysefxvol(npart,3, value);
            else
                value = synth.Psysefxvol[3][npart];
            break;

        case PART::control::humanise:
            if (write)
                part.Pfrand = value;
            else
                value = part.Pfrand;
            break;

        case PART::control::humanvelocity:
            if (write)
                part.Pvelrand = value;
            else
                value = part.Pvelrand;
            break;

        case PART::control::drumMode:
            if (write)
            {
                part.Pdrummode = value_bool;
                synth.setPartMap(npart);
            }
            else
                value = part.Pdrummode;
            break;
        case PART::control::kitMode:
            if (write)
            {
                if (value_int == 3) // crossfade
                {
                    part.Pkitmode = 1; // normal kit mode (multiple kit items playing)
                    part.PkitfadeType = 1;
                    value = 1; // just to be sure
                }
                else
                {
                    part.PkitfadeType = 0;
                    part.Pkitmode = value_int;
                }
            }
            else
            {
                value = part.Pkitmode;
                if (value == 1 && part.PkitfadeType == 1)
                    value = 3; // encode crossfade velocity mode
            }
            break;

        case PART::control::effectNumber:
            if (write)
            {
                part.Peffnum = value_int;
                cmd.data.parameter = (part.partefx[value_int]->geteffectpar(-1) != 0);
                cmd.data.engine = value_int;
                cmd.data.source |= cmd.data.source |= TOPLEVEL::action::forceUpdate;
                // the line above is to show it's changed from preset values
                synth.pushEffectUpdate(npart);
            }
            else
                value = part.Peffnum;
            break;

        case PART::control::effectType:
            if (write)
            {
                part.partefx[effNum]->changeeffect(value_int);
                synth.pushEffectUpdate(npart);
            }
            else
                value = part.partefx[effNum]->geteffect();
            cmd.data.offset = 0;
            break;
        case PART::control::effectDestination:
            if (write)
            {
                part.Pefxroute[effNum] = value_int;
                part.partefx[effNum]->setdryonly(value_int == 2);
                synth.pushEffectUpdate(npart);
            }
            else
                value = part.Pefxroute[effNum];
            break;
        case PART::control::effectBypass:
        {
            int tmp = part.Peffnum;
            part.Peffnum = engine;
            if (write)
            {
                bool newSwitch = value_bool;
                bool oldSwitch = part.Pefxbypass[engine];
                part.Pefxbypass[engine] = newSwitch;
                if (newSwitch != oldSwitch)
                    part.partefx[engine]->cleanup();
                synth.pushEffectUpdate(npart);
            }
            else
                value = part.Pefxbypass[engine];
            part.Peffnum = tmp; // leave it as it was before
            break;
        }

        case PART::control::audioDestination:
            if (synth.partonoffRead(npart) != 1)
            {
                cmd.data.value = part.Paudiodest; // specific for this control
                return;
            }
            else if (write)
            {
                if (npart < synth.getRuntime().numAvailableParts)
                    synth.part[npart]->Paudiodest = value_int;
                cmd.data.source = TOPLEVEL::action::lowPrio;
            }
            else
                value = part.Paudiodest;
            break;

        case PART::control::resetAllControllers:
            if (write)
                part.ctl->resetall();
            break;

        case PART::control::midiModWheel:
            if (write)
                part.ctl->setmodwheel(value);
            else
                value = part.ctl->modwheel.data;
            break;
        case PART::control::midiBreath:
            ; // not yet
            break;
        case PART::control::midiExpression:
            if (write)
                part.SetController(MIDI::CC::expression, value);
            else
                value = part.ctl->expression.data;
            break;
        case PART::control::midiSustain:
            if (write)
                part.ctl->setsustain(value);
            else
                value = part.ctl->sustain.data;
            break;
        case PART::control::midiPortamento:
            if (write)
                part.ctl->setportamento(value);
            else
                value = part.ctl->portamento.data;
            break;
        case PART::control::midiFilterQ:
            if (write)
                part.ctl->setfilterq(value);
            else
                value = part.ctl->filterq.data;
            break;
        case PART::control::midiFilterCutoff:
            if (write)
                part.ctl->setfiltercutoff(value);
            else
                value = part.ctl->filtercutoff.data;
            break;
        case PART::control::midiBandwidth:
            if (write)
                part.ctl->setbandwidth(value);
            else
                value = part.ctl->bandwidth.data;
            break;

        case PART::control::midiFMamp:
            if (write)
                part.ctl->setfmamp(value);
            else
                value = part.ctl->fmamp.data;
            break;
        case PART::control::midiResonanceCenter:
            if (write)
                part.ctl->setresonancecenter(value);
            else
                value = part.ctl->resonancecenter.data;
            break;
        case PART::control::midiResonanceBandwidth:
            if (write)
                part.ctl->setresonancebw(value);
            else
                value = part.ctl->resonancebandwidth.data;
            break;

        case PART::control::instrumentCopyright: // done elsewhere
            break;
        case PART::control::instrumentComments: // done elsewhere
            break;
        case PART::control::instrumentName: // done elsewhere
            break;
        case PART::control::instrumentType:// done elsewhere
            break;
        case PART::control::defaultInstrumentCopyright: // done elsewhere
            break;
    }

    if (!write || control == PART::control::minToLastKey || control == PART::control::maxToLastKey)
        cmd.data.value = value;
}


void InterChange::commandControllers(CommandBlock& cmd, bool write)
{
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    float value   = cmd.data.value;
    int value_int = int(value);
    char value_bool = _SYS_::F2B(value);

    assert(npart < NUM_MIDI_PARTS);
    Part& part{* synth.part[npart]};

    switch (control)
    {
        case PART::control::volumeRange: // start of controllers
            if (write)
                part.ctl->setvolume(value_int); // not the *actual* volume
            else
                value = part.ctl->volume.data;
            break;
        case PART::control::volumeEnable:
            if (write)
                part.ctl->volume.receive = value_bool;
            else
                value = part.ctl->volume.receive;
            break;
        case PART::control::panningWidth:
            if (write)
                part.ctl->setPanDepth(value_int);
            else
                value = part.ctl->panning.depth;
            break;
        case PART::control::modWheelDepth:
            if (write)
                part.ctl->modwheel.depth = value;
            else
                value = part.ctl->modwheel.depth;
            break;
        case PART::control::exponentialModWheel:
            if (write)
                part.ctl->modwheel.exponential = value_bool;
            else
                value = part.ctl->modwheel.exponential;
            break;
        case PART::control::bandwidthDepth:
            if (write)
                part.ctl->bandwidth.depth = value;
            else
                value = part.ctl->bandwidth.depth;
            break;
        case PART::control::exponentialBandwidth:
            if (write)
                part.ctl->bandwidth.exponential = value_bool;
            else
                value = part.ctl->bandwidth.exponential;
            break;
        case PART::control::expressionEnable:
            if (write)
                part.ctl->expression.receive = value_bool;
            else
                value = part.ctl->expression.receive;
            break;
        case PART::control::FMamplitudeEnable:
            if (write)
                part.ctl->fmamp.receive = value_bool;
            else
                value = part.ctl->fmamp.receive;
            break;
        case PART::control::sustainPedalEnable:
            if (write)
                part.ctl->sustain.receive = value_bool;
            else
                value = part.ctl->sustain.receive;
            break;
        case PART::control::pitchWheelRange:
            if (write)
                part.ctl->pitchwheel.bendrange = value_int;
            else
                value = part.ctl->pitchwheel.bendrange;
            break;
        case PART::control::filterQdepth:
            if (write)
                part.ctl->filterq.depth = value;
            else
                value = part.ctl->filterq.depth;
            break;
        case PART::control::filterCutoffDepth:
            if (write)
                part.ctl->filtercutoff.depth = value;
            else
                value = part.ctl->filtercutoff.depth;
            break;
        case PART::control::breathControlEnable:
            if (write)
                if (value_bool)
                    part.PbreathControl = MIDI::CC::breath;
                else
                    part.PbreathControl = UNUSED; // impossible CC value
            else
                value = part.PbreathControl;
            break;

        case PART::control::resonanceCenterFrequencyDepth:
            if (write)
                part.ctl->resonancecenter.depth = value;
            else
                value = part.ctl->resonancecenter.depth;
            break;
        case PART::control::resonanceBandwidthDepth:
            if (write)
                part.ctl->resonancebandwidth.depth = value;
            else
                value = part.ctl->resonancebandwidth.depth;
            break;

        case PART::control::portamentoTime:
            if (write)
                part.ctl->portamento.time = value;
            else
                value = part.ctl->portamento.time;
            break;
        case PART::control::portamentoTimeStretch:
            if (write)
                part.ctl->portamento.updowntimestretch = value;
            else
                value = part.ctl->portamento.updowntimestretch;
            break;
        case PART::control::portamentoThreshold:
            if (write)
                part.ctl->portamento.pitchthresh = value;
            else
                value = part.ctl->portamento.pitchthresh;
            break;
        case PART::control::portamentoThresholdType:
            if (write)
                part.ctl->portamento.pitchthreshtype = value_int;
            else
                value = part.ctl->portamento.pitchthreshtype;
            break;
        case PART::control::enableProportionalPortamento:
            if (write)
                part.ctl->portamento.proportional = value_int;
            else
                value = part.ctl->portamento.proportional;
            break;
        case PART::control::proportionalPortamentoRate:
            if (write)
                part.ctl->portamento.propRate = value;
            else
                value = part.ctl->portamento.propRate;
            break;
        case PART::control::proportionalPortamentoDepth:
            if (write)
                part.ctl->portamento.propDepth = value;
            else
                value = part.ctl->portamento.propDepth;
            break;

        case PART::control::receivePortamento: // end of controllers
            if (write)
                part.ctl->portamento.receive = value_bool;
            else
                value = part.ctl->portamento.receive;
            break;
    }

    if (!write || control == PART::control::minToLastKey || control == PART::control::maxToLastKey)
        cmd.data.value = value;
}


void InterChange::commandAdd(CommandBlock& cmd)
{
    float value   = cmd.data.value;
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;

    bool write = (type & TOPLEVEL::type::Write) > 0;

    int value_int = lrint(value);
    char value_bool = _SYS_::F2B(value);

    assert(npart < NUM_MIDI_PARTS);
    ADnoteParameters& param{*(synth.part[npart]->kit[kititem].adpars)};
    if (write)
        add2undo(cmd, noteSeen);

    switch (control)
    {
        case ADDSYNTH::control::volume:
            if (write)
                param.GlobalPar.PVolume = value_int;
            else
                value = param.GlobalPar.PVolume;
            break;
        case ADDSYNTH::control::velocitySense:
            if (write)
                param.GlobalPar.PAmpVelocityScaleFunction = value_int;
            else
                value = param.GlobalPar.PAmpVelocityScaleFunction;
            break;
        case ADDSYNTH::control::panning:
            if (write)
                param.setGlobalPan(value_int, synth.getRuntime().panLaw);
            else
                value = param.GlobalPar.PPanning;
            break;
        case ADDSYNTH::control::enableRandomPan:
            if (write)
                param.GlobalPar.PRandom = value_int;
            else
                value = param.GlobalPar.PRandom;
            break;
        case ADDSYNTH::control::randomWidth:
            if (write)
                param.GlobalPar.PWidth = value_int;
            else
                value = param.GlobalPar.PWidth;
            break;

        case ADDSYNTH::control::detuneFrequency:
            if (write)
                param.GlobalPar.PDetune = value_int + 8192;
            else // these steps are done to keep the GUI happy - sliders are strange :(
                value = param.GlobalPar.PDetune - 8192;
            break;

        case ADDSYNTH::control::octave:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 16;
                param.GlobalPar.PCoarseDetune = k * 1024 + param.GlobalPar.PCoarseDetune % 1024;
            }
            else
            {
                k = param.GlobalPar.PCoarseDetune / 1024;
                if (k >= 8)
                    k -= 16;
                value = k;
            }
            break;
        }
        case ADDSYNTH::control::detuneType:
            if (write)
            {
                if (value_int < 1) // can't be default for addsynth
                {
                    cmd.data.value = 1;
                    value_int = 1;
                }
                param.GlobalPar.PDetuneType = value_int;
            }
            else
            {
                value = param.GlobalPar.PDetuneType;
                if (value < 1)
                    value = 1;
            }
            break;
        case ADDSYNTH::control::coarseDetune:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 1024;
                param.GlobalPar.PCoarseDetune = k + (param.GlobalPar.PCoarseDetune / 1024) * 1024;
            }
            else
            {
                k = param.GlobalPar.PCoarseDetune % 1024;
                if (k >= 512)
                    k -= 1024;
                value = k;
            }
            break;
        }
        case ADDSYNTH::control::relativeBandwidth:
            if (write)
            {
                param.GlobalPar.PBandwidth = value_int;
                param.getBandwidthDetuneMultiplier();
            }
            else
                value = param.GlobalPar.PBandwidth;
            break;

        case ADDSYNTH::control::bandwidthMultiplier:
            if (write)
                write = false; // read only
            value = param.getBandwidthDetuneMultiplier();
        break;

        case ADDSYNTH::control::stereo:
            if (write)
                param.GlobalPar.PStereo = value_bool;
            else
                value = param.GlobalPar.PStereo;
            break;
        case ADDSYNTH::control::randomGroup:
            if (write)
                param.GlobalPar.Hrandgrouping = value_bool;
            else
                value = param.GlobalPar.Hrandgrouping;
            break;

        case ADDSYNTH::control::dePop:
            if (write)
                param.GlobalPar.Fadein_adjustment = value_int;
            else
                value = param.GlobalPar.Fadein_adjustment;
            break;
        case ADDSYNTH::control::punchStrength:
            if (write)
                param.GlobalPar.PPunchStrength = value_int;
            else
                value = param.GlobalPar.PPunchStrength;
            break;
        case ADDSYNTH::control::punchDuration:
            if (write)
                param.GlobalPar.PPunchTime = value_int;
            else
                value = param.GlobalPar.PPunchTime;
            break;
        case ADDSYNTH::control::punchStretch:
            if (write)
                param.GlobalPar.PPunchStretch = value_int;
            else
                value = param.GlobalPar.PPunchStretch;
            break;
        case ADDSYNTH::control::punchVelocity:
            if (write)
                param.GlobalPar.PPunchVelocitySensing = value_int;
            else
                value = param.GlobalPar.PPunchVelocitySensing;
            break;
    }
    if (!write)
        cmd.data.value = value;
}


void InterChange::commandAddVoice(CommandBlock& cmd)
{
    float value = cmd.data.value;
    uchar type = cmd.data.type;
    uchar control = cmd.data.control;
    uchar npart = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine = cmd.data.engine;
    int nvoice;
    if (engine >= PART::engine::addMod1)
        nvoice = engine - PART::engine::addMod1;
    else
        nvoice = engine - PART::engine::addVoice1;

    bool write = (type & TOPLEVEL::type::Write) > 0;

    int value_int = lrint(value);
    char value_bool = _SYS_::F2B(value);

    if (write)
        add2undo(cmd, noteSeen);

    assert(npart < NUM_MIDI_PARTS);
    ADnoteParameters& param{ *(synth.part[npart]->kit[kititem].adpars)};
    switch (control)
    {
        case ADDVOICE::control::volume:
            if (write)
                param.VoicePar[nvoice].PVolume = value_int;
            else
                value = param.VoicePar[nvoice].PVolume;
            break;
        case ADDVOICE::control::velocitySense:
            if (write)
                param.VoicePar[nvoice].PAmpVelocityScaleFunction = value_int;
            else
                value = param.VoicePar[nvoice].PAmpVelocityScaleFunction;
            break;
        case ADDVOICE::control::panning:
            if (write)
                 param.setVoicePan(nvoice, value_int, synth.getRuntime().panLaw);
            else
                value = param.VoicePar[nvoice].PPanning;
            break;
            case ADDVOICE::control::enableRandomPan:
                if (write)
                    param.VoicePar[nvoice].PRandom = value_int;
                else
                    value = param.VoicePar[nvoice].PRandom;
                break;
            case ADDVOICE::control::randomWidth:
                if (write)
                    param.VoicePar[nvoice].PWidth = value_int;
                else
                    value = param.VoicePar[nvoice].PWidth;
                break;

        case ADDVOICE::control::invertPhase:
            if (write)
                param.VoicePar[nvoice].PVolumeminus = value_bool;
            else
                value = param.VoicePar[nvoice].PVolumeminus;
            break;
        case ADDVOICE::control::enableAmplitudeEnvelope:
            if (write)
                param.VoicePar[nvoice].PAmpEnvelopeEnabled = value_bool;
            else
                value = param.VoicePar[nvoice].PAmpEnvelopeEnabled;
            break;
        case ADDVOICE::control::enableAmplitudeLFO:
            if (write)
                param.VoicePar[nvoice].PAmpLfoEnabled = value_bool;
            else
                value = param.VoicePar[nvoice].PAmpLfoEnabled;
            break;

        case ADDVOICE::control::modulatorType:
            if (write)
            {
                param.VoicePar[nvoice].PFMEnabled = value_int;
                cmd.data.value = value_int; // we have to do this otherwise GUI goes out of sync
            }
            else
                value = param.VoicePar[nvoice].PFMEnabled;
            break;
        case ADDVOICE::control::externalModulator:
            if (write)
                param.VoicePar[nvoice].PFMVoice = value_int;
            else
                value = param.VoicePar[nvoice].PFMVoice;
            break;

        case ADDVOICE::control::externalOscillator:
            if (write)
                param.VoicePar[nvoice].PVoice = value_int;
            else
                value = param.VoicePar[nvoice].PVoice;
            break;

        case ADDVOICE::control::detuneFrequency:
            if (write)
                param.VoicePar[nvoice].PDetune = value_int + 8192;
            else
                value = param.VoicePar[nvoice].PDetune-8192;
            break;
        case ADDVOICE::control::equalTemperVariation:
            if (write)
                param.VoicePar[nvoice].PfixedfreqET = value_int;
            else
                value = param.VoicePar[nvoice].PfixedfreqET;
            break;
        case ADDVOICE::control::baseFrequencyAs440Hz:
            if (write)
                 param.VoicePar[nvoice].Pfixedfreq = value_bool;
            else
                value = param.VoicePar[nvoice].Pfixedfreq;
            break;
        case ADDVOICE::control::octave:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 16;
                param.VoicePar[nvoice].PCoarseDetune = k * 1024 + param.VoicePar[nvoice].PCoarseDetune % 1024;
            }
            else
            {
                k = param.VoicePar[nvoice].PCoarseDetune / 1024;
                if (k >= 8)
                    k -= 16;
                value = k;
            }
            break;
        }
        case ADDVOICE::control::detuneType:
            if (write)
                param.VoicePar[nvoice].PDetuneType = value_int;
            else
                value = param.VoicePar[nvoice].PDetuneType;
            break;
        case ADDVOICE::control::coarseDetune:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 1024;
                param.VoicePar[nvoice].PCoarseDetune = k + (param.VoicePar[nvoice].PCoarseDetune / 1024) * 1024;
            }
            else
            {
                k = param.VoicePar[nvoice].PCoarseDetune % 1024;
                if (k >= 512)
                    k -= 1024;
                value = k;
            }
            break;
        }
        case ADDVOICE::control::pitchBendAdjustment:
            if (write)
                param.VoicePar[nvoice].PBendAdjust = value_int;
            else
                value = param.VoicePar[nvoice].PBendAdjust;
            break;
        case ADDVOICE::control::pitchBendOffset:
            if (write)
                param.VoicePar[nvoice].POffsetHz = value_int;
            else
                value = param.VoicePar[nvoice].POffsetHz;
            break;
        case ADDVOICE::control::enableFrequencyEnvelope:
            if (write)
                param.VoicePar[nvoice].PFreqEnvelopeEnabled = value_int;
            else
                value = param.VoicePar[nvoice].PFreqEnvelopeEnabled;
            break;
        case ADDVOICE::control::enableFrequencyLFO:
            if (write)
                param.VoicePar[nvoice].PFreqLfoEnabled = value_int;
            else
                value = param.VoicePar[nvoice].PFreqLfoEnabled;
            break;

        case ADDVOICE::control::unisonFrequencySpread:
            if (write)
                param.VoicePar[nvoice].Unison_frequency_spread = value_int;
            else
                value = param.VoicePar[nvoice].Unison_frequency_spread;
            break;
        case ADDVOICE::control::unisonSpreadCents:
            if (write)
                write = false; // read only
            value = param.getUnisonFrequencySpreadCents(nvoice);
            break;
        case ADDVOICE::control::unisonPhaseRandomise:
            if (write)
                param.VoicePar[nvoice].Unison_phase_randomness = value_int;
            else
                value = param.VoicePar[nvoice].Unison_phase_randomness;
            break;
        case ADDVOICE::control::unisonStereoSpread:
            if (write)
                param.VoicePar[nvoice].Unison_stereo_spread = value_int;
            else
                value = param.VoicePar[nvoice].Unison_stereo_spread;
            break;
        case ADDVOICE::control::unisonVibratoDepth:
            if (write)
                param.VoicePar[nvoice].Unison_vibrato = value_int;
            else
                value = param.VoicePar[nvoice].Unison_vibrato;
            break;
        case ADDVOICE::control::unisonVibratoSpeed:
            if (write)
                param.VoicePar[nvoice].Unison_vibrato_speed = value_int;
            else
                value = param.VoicePar[nvoice].Unison_vibrato_speed;
            break;
        case ADDVOICE::control::unisonSize:
            if (write)
            {
                if (value < 2)
                    value = 2;
                param.VoicePar[nvoice].Unison_size = value_int;
            }
            else
                value = param.VoicePar[nvoice].Unison_size;
            break;
        case ADDVOICE::control::unisonPhaseInvert:
            if (write)
                param.VoicePar[nvoice].Unison_invert_phase = value_int;
            else
                value = param.VoicePar[nvoice].Unison_invert_phase;
            break;
        case ADDVOICE::control::enableUnison:
        {
            int k;
            if (write)
            {
                k = value_bool + 1;
                if (param.VoicePar[nvoice].Unison_size < 2 || k == 1)
                    param.VoicePar[nvoice].Unison_size = k;
            }
            else
                value = (param.VoicePar[nvoice].Unison_size);
            break;
        }

        case ADDVOICE::control::bypassGlobalFilter:
            if (write)
                param.VoicePar[nvoice].Pfilterbypass = value_bool;
            else
                value = param.VoicePar[nvoice].Pfilterbypass;
            break;
        case ADDVOICE::control::enableFilter:
            if (write)
                 param.VoicePar[nvoice].PFilterEnabled =  value_bool;
            else
                value = param.VoicePar[nvoice].PFilterEnabled;
            break;
        case ADDVOICE::control::enableFilterEnvelope:
            if (write)
                param.VoicePar[nvoice].PFilterEnvelopeEnabled= value_bool;
            else
                value = param.VoicePar[nvoice].PFilterEnvelopeEnabled;
            break;
        case ADDVOICE::control::enableFilterLFO:
            if (write)
                param.VoicePar[nvoice].PFilterLfoEnabled= value_bool;
            else
                value = param.VoicePar[nvoice].PFilterLfoEnabled;
            break;

        case ADDVOICE::control::modulatorAmplitude:
            if (write)
                param.VoicePar[nvoice].PFMVolume = value_int;
            else
                value = param.VoicePar[nvoice].PFMVolume;
            break;
        case ADDVOICE::control::modulatorVelocitySense:
            if (write)
                param.VoicePar[nvoice].PFMVelocityScaleFunction = value_int;
            else
                value = param.VoicePar[nvoice].PFMVelocityScaleFunction;
            break;
        case ADDVOICE::control::modulatorHFdamping:
            if (write)
                param.VoicePar[nvoice].PFMVolumeDamp = value_int + 64;
            else
                value = param.VoicePar[nvoice].PFMVolumeDamp - 64;
            break;
        case ADDVOICE::control::enableModulatorAmplitudeEnvelope:
            if (write)
                param.VoicePar[nvoice].PFMAmpEnvelopeEnabled = value_bool;
            else
                value =  param.VoicePar[nvoice].PFMAmpEnvelopeEnabled;
            break;

        case ADDVOICE::control::modulatorDetuneFrequency:
            if (write)
                param.VoicePar[nvoice].PFMDetune = value_int + 8192;
            else
                value = param.VoicePar[nvoice].PFMDetune - 8192;
            break;
        case ADDVOICE::control::modulatorDetuneFromBaseOsc:
            if (write)
                param.VoicePar[nvoice].PFMDetuneFromBaseOsc = value_bool;
            else
                value = param.VoicePar[nvoice].PFMDetuneFromBaseOsc;
            break;
        case ADDVOICE::control::modulatorFrequencyAs440Hz:
            if (write)
                param.VoicePar[nvoice].PFMFixedFreq = value_bool;
            else
                value = param.VoicePar[nvoice].PFMFixedFreq;
            break;
        case ADDVOICE::control::modulatorOctave:
        {
            int k;
            if (write)
            {
                k = value_int;
                if (k < 0)
                    k += 16;
                param.VoicePar[nvoice].PFMCoarseDetune = k * 1024 + param.VoicePar[nvoice].PFMCoarseDetune % 1024;
            }
            else
            {
                k = param.VoicePar[nvoice].PFMCoarseDetune / 1024;
                if (k >= 8)
                    k -= 16;
                value = k;
            }
            break;
        }
        case ADDVOICE::control::modulatorDetuneType:
            if (write)
                param.VoicePar[nvoice].PFMDetuneType = value_int;
            else
                value = param.VoicePar[nvoice].PFMDetuneType;
            break;
        case ADDVOICE::control::modulatorCoarseDetune:
        {
            int k;
            if (write)
            {
                int k = value_int;
                if (k < 0)
                    k += 1024;
                param.VoicePar[nvoice].PFMCoarseDetune = k + (param.VoicePar[nvoice].PFMCoarseDetune / 1024) * 1024;
            }
            else
            {
                k = param.VoicePar[nvoice].PFMCoarseDetune % 1024;
                if (k >= 512)
                    k-= 1024;
                value = k;
            }
            break;
        }
        case ADDVOICE::control::enableModulatorFrequencyEnvelope:
            if (write)
                param.VoicePar[nvoice].PFMFreqEnvelopeEnabled = value_int;
            else
                value = param.VoicePar[nvoice].PFMFreqEnvelopeEnabled;
            break;

        case ADDVOICE::control::modulatorOscillatorPhase:
            if (write)
                param.VoicePar[nvoice].PFMoscilphase = 64 - value_int;
            else
                value = 64 - param.VoicePar[nvoice].PFMoscilphase;
            break;
        case ADDVOICE::control::modulatorOscillatorSource:
            if (write)
                param.VoicePar[nvoice].PextFMoscil = value_int;
            else
                value = param.VoicePar[nvoice].PextFMoscil;
            break;

        case ADDVOICE::control::delay:
            if (write)
                param.VoicePar[nvoice].PDelay = value_int;
            else
                value = param.VoicePar[nvoice].PDelay;
            break;
        case ADDVOICE::control::enableVoice:
            if (write)
                param.VoicePar[nvoice].Enabled = value_bool;
            else
                value = param.VoicePar[nvoice].Enabled;
            break;
        case ADDVOICE::control::enableResonance:
            if (write)
                param.VoicePar[nvoice].Presonance = value_bool;
            else
                value = param.VoicePar[nvoice].Presonance;
            break;
        case ADDVOICE::control::voiceOscillatorPhase:
            if (write)
                param.VoicePar[nvoice].Poscilphase = 64 - value_int;
            else
                value = 64 - param.VoicePar[nvoice].Poscilphase;
            break;
        case ADDVOICE::control::voiceOscillatorSource:
            if (write)
                param.VoicePar[nvoice].Pextoscil = value_int;
            else
                value = param.VoicePar[nvoice].Pextoscil;
            break;
        case ADDVOICE::control::soundType:
            if (write)
                param.VoicePar[nvoice].Type = value_int;
            else
                value = param.VoicePar[nvoice].Type;
            break;
    }

    if (!write)
        cmd.data.value = value;
}


void InterChange::commandSub(CommandBlock& cmd)
{
    float value   = cmd.data.value;
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar insert  = cmd.data.insert & 0x1f; // ensure no stray filter

    bool write = (type & TOPLEVEL::type::Write) > 0;

    int value_int = lrint(value);
    char value_bool = _SYS_::F2B(value);

    assert(npart < NUM_MIDI_PARTS);
    SUBnoteParameters& param{ * synth.part[npart]->kit[kititem].subpars};

    if (write)
    {
        if(control == SUBSYNTH::control::clearHarmonics)
        {
            CommandBlock undoCmd;
            memcpy(undoCmd.bytes, cmd.bytes, sizeof(CommandBlock));
            undoCmd.data.source = 0;
            undoCmd.data.type &= TOPLEVEL::type::Write;

            undoCmd.data.insert = TOPLEVEL::insert::harmonicAmplitude;
            bool markerSet = false;
            int target = 127; // first harmonic amplitude
            for (int i = 0; i < MAX_SUB_HARMONICS; ++i)
            {
                int val = param.Phmag[i];
                if (val != target)
                {
                    undoCmd.data.value = val;
                    undoCmd.data.control = i;
                    noteSeen = true;
                    undoLoopBack = false;
                    if (!markerSet)
                    {
                        add2undo(undoCmd, noteSeen);
                        markerSet = true;
                    }
                    else
                        add2undo(undoCmd, noteSeen, true);
                    if (target == 127)
                        target = 0;
                }
            }
            undoCmd.data.insert = TOPLEVEL::insert::harmonicBandwidth;
            for (int i = 0; i < MAX_SUB_HARMONICS; ++i)
            {
                int val = param.Phrelbw[i];
                undoCmd.data.control = i;
                noteSeen = true;
                undoLoopBack = false;
                if (val != 64)
                {
                    undoCmd.data.value = val;
                    undoCmd.data.control = i;
                    noteSeen = true;
                    undoLoopBack = false;
                    if (!markerSet)
                    {
                        add2undo(undoCmd, noteSeen);
                        markerSet = true;
                    }
                    else
                        add2undo(undoCmd, noteSeen, true);
                }
            }

            for (int i = 0; i < MAX_SUB_HARMONICS; i++)
            {
                param.Phmag[i] = 0;
                param.Phrelbw[i] = 64;
            }
            param.Phmag[0] = 127;

            return;
        }
        else
            add2undo(cmd, noteSeen);
    }

    if (insert == TOPLEVEL::insert::harmonicAmplitude || insert == TOPLEVEL::insert::harmonicBandwidth)
    {
        if (insert == TOPLEVEL::insert::harmonicAmplitude)
        {
            if (write)
                param.Phmag[control] = value;
            else
            {
                value = param.Phmag[control];
                cmd.data.value = value;
            }
        }
        else
        {
            if (write)
                param.Phrelbw[control] = value;
            else
            {
                value = param.Phrelbw[control];
                cmd.data.value = value;
            }
        }
        return;
    }

    switch (control)
    {
        case SUBSYNTH::control::volume:
            if (write)
                param.PVolume = value;
            else
                value = param.PVolume;
            break;
        case SUBSYNTH::control::velocitySense:
            if (write)
                param.PAmpVelocityScaleFunction = value;
            else
                value = param.PAmpVelocityScaleFunction;
            break;
        case SUBSYNTH::control::panning:
            if (write)
                param.setPan(value, synth.getRuntime().panLaw);
            else
                value = param.PPanning;
            break;
        case SUBSYNTH::control::enableRandomPan:
            if (write)
                param.PRandom = value_int;
            else
                value = param.PRandom;
            break;
        case SUBSYNTH::control::randomWidth:
            if (write)
                param.PWidth = value_int;
            else
                value = param.PWidth;
            break;

        case SUBSYNTH::control::bandwidth:
            if (write)
                param.Pbandwidth = value;
            else
                value = param.Pbandwidth;
            break;
        case SUBSYNTH::control::bandwidthScale:
            if (write)
                param.Pbwscale = value + 64;
            else
                value = param.Pbwscale - 64;
            break;
        case SUBSYNTH::control::enableBandwidthEnvelope:
            if (write)
                param.PBandWidthEnvelopeEnabled = value_bool;
            else
                value = param.PBandWidthEnvelopeEnabled;
            break;

        case SUBSYNTH::control::detuneFrequency:
            if (write)
                param.PDetune = value + 8192;
            else
                value = param.PDetune - 8192;
            break;
        case SUBSYNTH::control::equalTemperVariation:
            if (write)
                param.PfixedfreqET = value;
            else
                value = param.PfixedfreqET;
            break;
        case SUBSYNTH::control::baseFrequencyAs440Hz:
            if (write)
                param.Pfixedfreq = value_bool;
            else
                value = param.Pfixedfreq;
            break;
        case SUBSYNTH::control::octave:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 16;
                param.PCoarseDetune = k * 1024 + param.PCoarseDetune % 1024;
            }
            else
            {
                k = param.PCoarseDetune / 1024;
                if (k >= 8)
                    k -= 16;
                value = k;
            }
            break;
        }
        case SUBSYNTH::control::detuneType:
            if (write)
            {
                if (value_int < 1) // can't be default for subsynth
                {
                    cmd.data.value = 1;
                    value_int = 1;
                }
                param.PDetuneType = value_int;
            }
            else
                value = param.PDetuneType;
            break;
        case SUBSYNTH::control::coarseDetune:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 1024;
                param.PCoarseDetune = k + (param.PCoarseDetune / 1024) * 1024;
            }
            else
            {
                k = param.PCoarseDetune % 1024;
                if (k >= 512)
                    k -= 1024;
                value = k;
            }
            break;
        }

        case SUBSYNTH::control::pitchBendAdjustment:
            if (write)
                param.PBendAdjust = value;
            else
                value = param.PBendAdjust;
            break;

        case SUBSYNTH::control::pitchBendOffset:
            if (write)
                param.POffsetHz = value;
            else
                value = param.POffsetHz;
            break;

        case SUBSYNTH::control::enableFrequencyEnvelope:
            if (write)
                param.PFreqEnvelopeEnabled = value_bool;
            else
                value = param.PFreqEnvelopeEnabled;
            break;

        case SUBSYNTH::control::overtoneParameter1:
            if (write)
            {
                param.POvertoneSpread.par1 = value;
                param.updateFrequencyMultipliers();
            }
            else
                value = param.POvertoneSpread.par1;
            break;
        case SUBSYNTH::control::overtoneParameter2:
            if (write)
            {
                param.POvertoneSpread.par2 = value;
                param.updateFrequencyMultipliers();
            }
            else
                value = param.POvertoneSpread.par2;
            break;
        case SUBSYNTH::control::overtoneForceHarmonics:
            if (write)
            {
                param.POvertoneSpread.par3 = value;
                param.updateFrequencyMultipliers();
            }
            else
                value = param.POvertoneSpread.par3;
            break;
        case SUBSYNTH::control::overtonePosition:
            if (write)
            {
                param.POvertoneSpread.type =  value_int;
                param.updateFrequencyMultipliers();
            }
            else
                value = param.POvertoneSpread.type;
            break;

        case SUBSYNTH::control::enableFilter:
            if (write)
                param.PGlobalFilterEnabled = value_bool;
            else
                value = param.PGlobalFilterEnabled;
            break;

        case SUBSYNTH::control::filterStages:
            if (write)
                param.Pnumstages = value_int;
            else
                value = param.Pnumstages;
            break;
        case SUBSYNTH::control::magType:
            if (write)
                param.Phmagtype = value_int;
            else
                value = param.Phmagtype;
            break;
        case SUBSYNTH::control::startPosition:
            if (write)
                param.Pstart = value_int;
            else
                value = param.Pstart;
            break;
        case SUBSYNTH::control::stereo:
            if (write)
                param.Pstereo = value_bool;
            else
                value = param.Pstereo;
            break;
    }

    if (!write)
        cmd.data.value = value;
}


bool InterChange::commandPad(CommandBlock& cmd, PADnoteParameters& param)
{
    uchar control   = cmd.data.control;
    float value     = cmd.data.value;
    int value_int   = lrint(value);
    char value_bool = _SYS_::F2B(value);

    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;

    if (write && control != PADSYNTH::control::applyChanges)
        add2undo(cmd, noteSeen);

    switch (control)
    {
        case PADSYNTH::control::volume:
            if (write)
                param.PVolume = value;
            else
                value = param.PVolume;
            break;
        case PADSYNTH::control::velocitySense:
            if (write)
                param.PAmpVelocityScaleFunction = value;
            else
                value = param.PAmpVelocityScaleFunction;
            break;
        case PADSYNTH::control::panning:
            if (write)
                param.setPan(value, synth.getRuntime().panLaw);
            else
                value = param.PPanning;
            break;
        case PADSYNTH::control::enableRandomPan:
            if (write)
                param.PRandom = value_int;
            else
                value = param.PRandom;
            break;
        case PADSYNTH::control::randomWidth:
            if (write)
                param.PWidth = value_int;
            else
                value = param.PWidth;
            break;

        case PADSYNTH::control::bandwidth:
            if (write)
                param.Pbandwidth = value_int;
            else
                value = param.Pbandwidth;
            break;
        case PADSYNTH::control::bandwidthScale:
            if (write)
                param.Pbwscale = value_int;
            else
                value = param.Pbwscale;
            break;
        case PADSYNTH::control::spectrumMode:
            if (write)
                param.Pmode = value_int;
            else
                value = param.Pmode;
            break;
        case PADSYNTH::control::xFadeUpdate:
            if (write)
                param.PxFadeUpdate = value_int;
            else
                value = param.PxFadeUpdate;
            break;
        case PADSYNTH::control::rebuildTrigger:
            if (write)
                param.PrebuildTrigger = value_int;
            else
                value = param.PrebuildTrigger;
            break;
        case PADSYNTH::control::randWalkDetune:
            if (write)
            {
                param.PrandWalkDetune = value_int;
                param.randWalkDetune.setSpread(value_int);
            }
            else
                value = param.PrandWalkDetune;
            break;
        case PADSYNTH::control::randWalkBandwidth:
            if (write)
            {
                param.PrandWalkBandwidth = value_int;
                param.randWalkBandwidth.setSpread(value_int);
            }
            else
                value = param.PrandWalkBandwidth;
            break;
        case PADSYNTH::control::randWalkFilterFreq:
            if (write)
            {
                param.PrandWalkFilterFreq = value_int;
                param.randWalkFilterFreq.setSpread(value_int);
            }
            else
                value = param.PrandWalkFilterFreq;
            break;
        case PADSYNTH::control::randWalkProfileWidth:
            if (write)
            {
                param.PrandWalkProfileWidth = value_int;
                param.randWalkProfileWidth.setSpread(value_int);
            }
            else
                value = param.PrandWalkProfileWidth;
            break;
        case PADSYNTH::control::randWalkProfileStretch:
            if (write)
            {
                param.PrandWalkProfileStretch = value_int;
                param.randWalkProfileStretch.setSpread(value_int);
            }
            else
                value = param.PrandWalkProfileStretch;
            break;

        case PADSYNTH::control::detuneFrequency:
            if (write)
                param.PDetune = value_int + 8192;
            else
                value = param.PDetune - 8192;
            break;
        case PADSYNTH::control::equalTemperVariation:
            if (write)
                param.PfixedfreqET = value_int;
            else
                value = param.PfixedfreqET;
            break;
        case PADSYNTH::control::baseFrequencyAs440Hz:
            if (write)
                param.Pfixedfreq = value_bool;
            else
                value = param.Pfixedfreq;
            break;
        case PADSYNTH::control::octave:
            if (write)
            {
                int tmp = value;
                if (tmp < 0)
                    tmp += 16;
                param.PCoarseDetune = tmp * 1024 + param.PCoarseDetune % 1024;
            }
            else
            {
                int tmp = param.PCoarseDetune / 1024;
                if (tmp >= 8)
                    tmp -= 16;
                value = tmp;
            }
            break;
        case PADSYNTH::control::detuneType:
            if (write)
            {
                if (value_int < 1) // can't be default for padsynth
                {
                    cmd.data.value = 1;
                    value_int = 1;
                }
                 param.PDetuneType = value_int;
            }
            else
                value =  param.PDetuneType;
            break;
        case PADSYNTH::control::coarseDetune:
            if (write)
            {
                int tmp = value;
                if (tmp < 0)
                    tmp += 1024;
                 param.PCoarseDetune = tmp + (param.PCoarseDetune / 1024) * 1024;
            }
            else
            {
                int tmp = param.PCoarseDetune % 1024;
                if (tmp >= 512)
                    tmp -= 1024;
                value = tmp;
            }
            break;

        case PADSYNTH::control::pitchBendAdjustment:
            if (write)
                param.PBendAdjust = value_int;
            else
                value = param.PBendAdjust;
            break;
        case PADSYNTH::control::pitchBendOffset:
            if (write)
                param.POffsetHz = value_int;
            else
                value = param.POffsetHz;
            break;

        case PADSYNTH::control::overtoneParameter1:
            if (write)
                param.Phrpos.par1 = value_int;
            else
                value = param.Phrpos.par1;
            break;
        case PADSYNTH::control::overtoneParameter2:
            if (write)
                param.Phrpos.par2 = value_int;
            else
                value = param.Phrpos.par2;
            break;
        case PADSYNTH::control::overtoneForceHarmonics:
            if (write)
                param.Phrpos.par3 = value_int;
            else
                value = param.Phrpos.par3;
            break;
        case PADSYNTH::control::overtonePosition:
            if (write)
                param.Phrpos.type = value_int;
            else
                value = param.Phrpos.type;
            break;

        case PADSYNTH::control::baseWidth:
            if (write)
                param.PProfile.base.pwidth = value_int;
            else
                value = param.PProfile.base.pwidth;
            break;
        case PADSYNTH::control::frequencyMultiplier:
            if (write)
                param.PProfile.freqmult = value_int;
            else
                value = param.PProfile.freqmult;
            break;
        case PADSYNTH::control::modulatorStretch:
            if (write)
                param.PProfile.modulator.pstretch = value_int;
            else
                value = param.PProfile.modulator.pstretch;
            break;
        case PADSYNTH::control::modulatorFrequency:
            if (write)
                param.PProfile.modulator.freq = value_int;
            else
                value = param.PProfile.modulator.freq;
            break;
        case PADSYNTH::control::size:
            if (write)
                param.PProfile.width = value_int;
            else
                value = param.PProfile.width;
            break;
        case PADSYNTH::control::baseType:
            if (write)
                param.PProfile.base.type = value;
            else
                value = param.PProfile.base.type;
            break;
        case PADSYNTH::control::harmonicSidebands:
            if (write)
                 param.PProfile.onehalf = value;
            else
                value = param.PProfile.onehalf;
            break;
        case PADSYNTH::control::spectralWidth:
            if (write)
                param.PProfile.amp.par1 = value_int;
            else
                value = param.PProfile.amp.par1;
            break;
        case PADSYNTH::control::spectralAmplitude:
            if (write)
                param.PProfile.amp.par2 = value_int;
            else
                value = param.PProfile.amp.par2;
            break;
        case PADSYNTH::control::amplitudeMultiplier:
            if (write)
                param.PProfile.amp.type = value;
            else
                value = param.PProfile.amp.type;
            break;
        case PADSYNTH::control::amplitudeMode:
            if (write)
                param.PProfile.amp.mode = value;
            else
                value = param.PProfile.amp.mode;
            break;
        case PADSYNTH::control::autoscale:
            if (write)
                param.PProfile.autoscale = value_bool;
            else
                value = param.PProfile.autoscale;
            break;

        case PADSYNTH::control::harmonicBase:
            if (write)
                param.Pquality.basenote = value_int;
            else
                value = param.Pquality.basenote;
            break;
        case PADSYNTH::control::samplesPerOctave:
            if (write)
                param.Pquality.smpoct = value_int;
            else
                value = param.Pquality.smpoct;
            break;
        case PADSYNTH::control::numberOfOctaves:
            if (write)
                param.Pquality.oct = value_int;
            else
                value = param.Pquality.oct;
            break;
        case PADSYNTH::control::sampleSize:
            if (write)
                param.Pquality.samplesize = value_int;
            else
                value = param.Pquality.samplesize;
            break;

        case PADSYNTH::control::applyChanges:
            if (write && value >= 0.5f)
            {
                bool blocking = (synth.getRuntime().useLegacyPadBuild()
                                 or cmd.data.parameter == 0);
                if (blocking)
                {// do the blocking build in the CMD-Dispatch background thread ("sortResultsThread")
                    cmd.data.source = TOPLEVEL::action::lowPrio; // marker to cause dispatch in InterChange::sortResultsThread()
                }
                else
                {// build will run in parallel within a dedicated background thread
                    param.buildNewWavetable();
                }
            }
            else
                value = not param.futureBuild.isUnderway();
            break;

        case PADSYNTH::control::stereo:
            if (write)
                param.PStereo = value_bool;
            else
                value = param.PStereo;
            break;

        case PADSYNTH::control::dePop:
            if (write)
                param.Fadein_adjustment = value_int;
            else
                value = param.Fadein_adjustment;
            break;
        case PADSYNTH::control::punchStrength:
            if (write)
                param.PPunchStrength = value_int;
            else
                value = param.PPunchStrength;
            break;
        case PADSYNTH::control::punchDuration:
            if (write)
                param.PPunchTime = value_int;
            else
                value = param.PPunchTime;
            break;
        case PADSYNTH::control::punchStretch:
            if (write)
                param.PPunchStretch = value_int;
            else
                value = param.PPunchStretch;
            break;
        case PADSYNTH::control::punchVelocity:
            if (write)
                param.PPunchVelocitySensing = value_int;
            else
                value = param.PPunchVelocitySensing;
            break;
    }
    bool needApply{false};
    if (write)
    {
        uchar control = cmd.data.control;
        needApply = (control >= PADSYNTH::control::bandwidth and control < PADSYNTH::control::rebuildTrigger);
        cmd.data.offset = 0;
    }
    else
        cmd.data.value = value;

    return needApply;
}


void InterChange::commandOscillator(CommandBlock& cmd, OscilParameters *oscil)
{
    float value   = cmd.data.value;
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    uchar insert  = cmd.data.insert;

    int value_int = lrint(value);
    bool value_bool = _SYS_::F2B(value);
    bool write = (type & TOPLEVEL::type::Write) > 0;

    if (write)
    {
        if (control == OSCILLATOR::control::clearHarmonics)
        {
            /*CommandBlock tempData;
            memcpy(tempData.bytes, cmd.bytes, sizeof(CommandBlock));
            tempData.data.source = 0;
            tempData.data.insert = TOPLEVEL::insert::harmonicAmplitude;
            for (int i = 0; i < MAX_AD_HARMONICS; ++i)
            {
                tempData.data.value = oscil->Phmag[i];
                tempData.data.control = i;
                noteSeen = true;
                undoLoopBack = false;
                if(i == 0) // first line sets marker
                    add2undo(&tempData, noteSeen);
                else
                    add2undo(&tempData, noteSeen, true);
            }
            tempData.data.insert = TOPLEVEL::insert::harmonicPhase;
            for (int i = 0; i < MAX_AD_HARMONICS; ++i)
            {
                tempData.data.value = oscil->Phphase[i];
                tempData.data.control = i;
                noteSeen = true;
                undoLoopBack = false;
                add2undo(&tempData, noteSeen, true);
            }*/
        }
        else if (control != OSCILLATOR::control::convertToSine && control != OSCILLATOR::control::useAsBaseFunction && control != OSCILLATOR::control::clearHarmonics)
            add2undo(cmd, noteSeen);
    }

    if (insert == TOPLEVEL::insert::harmonicAmplitude)
    {
        if (write)
        {
            oscil->Phmag[control] = value_int;
            if (value_int == 64)
                oscil->Phphase[control] = 64;
            oscil->paramsChanged();
        }
        else
            cmd.data.value = oscil->Phmag[control];
        return;
    }
    else if (insert == TOPLEVEL::insert::harmonicPhase)
    {
        if (write)
        {
            oscil->Phphase[control] = value_int;
            oscil->paramsChanged();
        }
        else
            cmd.data.value = oscil->Phphase[control];
        return;
    }

    switch (control)
    {
        case OSCILLATOR::control::phaseRandomness:
            if (write)
                oscil->Prand = value_int + 64;
            else
                value = oscil->Prand - 64;
            break;
        case OSCILLATOR::control::magType:
            if (write)
                oscil->Phmagtype = value_int;
            else
                value = oscil->Phmagtype;
            break;
        case OSCILLATOR::control::harmonicAmplitudeRandomness:
            if (write)
                oscil->Pamprandpower = value_int;
            else
                value = oscil->Pamprandpower;
            break;
        case OSCILLATOR::control::harmonicRandomnessType:
            if (write)
                oscil->Pamprandtype = value_int;
            else
                value = oscil->Pamprandtype;
            break;

        case OSCILLATOR::control::baseFunctionParameter:
            if (write)
                oscil->Pbasefuncpar = value_int + 64;
            else
                value = oscil->Pbasefuncpar - 64;
            break;
        case OSCILLATOR::control::baseFunctionType:
            if (write)
                oscil->Pcurrentbasefunc = value_int;
            else
                value = oscil->Pcurrentbasefunc;
            break;
        case OSCILLATOR::control::baseModulationParameter1:
            if (write)
                oscil->Pbasefuncmodulationpar1 = value_int;
            else
                value = oscil->Pbasefuncmodulationpar1;
            break;
        case OSCILLATOR::control::baseModulationParameter2:
            if (write)
                oscil->Pbasefuncmodulationpar2 = value_int;
            else
                value = oscil->Pbasefuncmodulationpar2;
            break;
        case OSCILLATOR::control::baseModulationParameter3:
            if (write)
                oscil->Pbasefuncmodulationpar3 = value_int;
            else
                value = oscil->Pbasefuncmodulationpar3;
            break;
        case OSCILLATOR::control::baseModulationType:
            if (write)
                oscil->Pbasefuncmodulation = value_int;
            else
                value = oscil->Pbasefuncmodulation;
            break;

        case OSCILLATOR::control::autoClear: // this is local to the GUI
            break;
        case OSCILLATOR::control::useAsBaseFunction:
            if (write)
            {
                fft::Calc fft(synth.oscilsize);
                OscilGen gen(fft, NULL, &synth, oscil);
                gen.useasbase();
                if (value_bool)
                {
                    for (int i = 0; i < MAX_AD_HARMONICS; ++ i)
                    {
                        oscil->Phmag[i] = 64;
                        oscil->Phphase[i] = 64;
                    }
                    oscil->Phmag[0] = 127;
                    oscil->Pharmonicshift = 0;
                    oscil->Pwaveshapingfunction = 0;
                    oscil->Pfiltertype = 0;
                    oscil->Psatype = 0;
                }
                oscil->paramsChanged();
            }
            break;

        case OSCILLATOR::control::waveshapeParameter:
            if (write)
                oscil->Pwaveshaping = value_int + 64;
            else
                value = oscil->Pwaveshaping - 64;
            break;
        case OSCILLATOR::control::waveshapeType:
            if (write)
                oscil->Pwaveshapingfunction = value_int;
            else
                value = oscil->Pwaveshapingfunction;
            break;

        case OSCILLATOR::control::filterParameter1:
            if (write)
                oscil->Pfilterpar1 = value_int;
            else
                value = oscil->Pfilterpar1;
            break;
        case OSCILLATOR::control::filterParameter2:
            if (write)
                oscil->Pfilterpar2 = value_int;
            else
                value = oscil->Pfilterpar2;
            break;
        case OSCILLATOR::control::filterBeforeWaveshape:
            if (write)
                oscil->Pfilterbeforews = value_bool;
            else
                value = oscil->Pfilterbeforews;
            break;
        case OSCILLATOR::control::filterType:
            if (write)
                oscil->Pfiltertype = value_int;
            else
                value = oscil->Pfiltertype;
            break;
        case OSCILLATOR::control::modulationParameter1:
            if (write)
                oscil->Pmodulationpar1 = value_int;
            else
                value = oscil->Pmodulationpar1;
            break;
        case OSCILLATOR::control::modulationParameter2:
            if (write)
                oscil->Pmodulationpar2 = value_int;
            else
                value = oscil->Pmodulationpar2;
            break;
        case OSCILLATOR::control::modulationParameter3:
            if (write)
                oscil->Pmodulationpar3 = value_int;
            else
                value = oscil->Pmodulationpar3;
            break;
        case OSCILLATOR::control::modulationType:
            if (write)
                oscil->Pmodulation = value_int;
            else
                value = oscil->Pmodulation;
            break;
        case OSCILLATOR::control::spectrumAdjustParameter:
            if (write)
                oscil->Psapar = value_int;
            else
                value = oscil->Psapar;
            break;
        case OSCILLATOR::control::spectrumAdjustType:
            if (write)
                oscil->Psatype = value_int;
            else
                value = oscil->Psatype;
            break;

        case OSCILLATOR::control::harmonicShift:
            if (write)
                oscil->Pharmonicshift = value_int;
            else
                value = oscil->Pharmonicshift;
            break;
        case OSCILLATOR::control::clearHarmonicShift:
            if (write)
                oscil->Pharmonicshift = 0;
            break;
        case OSCILLATOR::control::shiftBeforeWaveshapeAndFilter:
            if (write)
                oscil->Pharmonicshiftfirst = value_bool;
            else
                value = oscil->Pharmonicshiftfirst;
            break;
        case OSCILLATOR::control::adaptiveHarmonicsParameter:
            if (write)
                oscil->Padaptiveharmonicspar = value_int;
            else
                value = oscil->Padaptiveharmonicspar;
            break;
        case OSCILLATOR::control::adaptiveHarmonicsBase:
            if (write)
                oscil->Padaptiveharmonicsbasefreq = value_int;
            else
                value = oscil->Padaptiveharmonicsbasefreq;
            break;
        case OSCILLATOR::control::adaptiveHarmonicsPower:
            if (write)
                oscil->Padaptiveharmonicspower = value_int;
            else
                value = oscil->Padaptiveharmonicspower;
            break;
        case OSCILLATOR::control::adaptiveHarmonicsType:
            if (write)
                oscil->Padaptiveharmonics = value_int;
            else
                value = oscil->Padaptiveharmonics;
            break;

        case OSCILLATOR::control::clearHarmonics:
            if (write)
            {
                for (int i = 0; i < MAX_AD_HARMONICS; ++ i)
                {
                    oscil->Phmag[i]=64;
                    oscil->Phphase[i]=64;
                }
                oscil->Phmag[0]=127;
                oscil->paramsChanged();
            }
            break;
        case OSCILLATOR::control::convertToSine:
            if (write)
            {
                fft::Calc fft(synth.oscilsize);
                OscilGen gen(fft, NULL, &synth, oscil);
                gen.convert2sine();
                oscil->paramsChanged();
            }
            break;
    }
    if (!write)
        cmd.data.value = value;
}


void InterChange::commandResonance(CommandBlock& cmd, Resonance *respar)
{
    float value     = cmd.data.value;
    uchar type      = cmd.data.type;
    uchar control   = cmd.data.control;
    uchar insert    = cmd.data.insert;
    uchar parameter = cmd.data.parameter;
    int value_int   = lrint(value);
    bool value_bool = _SYS_::F2B(value);
    bool write = (type & TOPLEVEL::type::Write) > 0;

    if (write)
    {
        if (control == RESONANCE::control::randomType ||
            control == RESONANCE::control::clearGraph ||
            control == RESONANCE::control::interpolatePeaks ||
            control == RESONANCE::control::smoothGraph)
        {
            CommandBlock undoCmd;
            memcpy(undoCmd.bytes, cmd.bytes, sizeof(CommandBlock));
            undoCmd.data.control = RESONANCE::control::graphPoint;
            undoCmd.data.insert = TOPLEVEL::insert::resonanceGraphInsert;
            bool markerSet = false;
            for (int i = 0; i < MAX_RESONANCE_POINTS; ++i)
            {
                int val = respar->Prespoints[i];
                undoCmd.data.value = val;
                undoCmd.data.parameter = i;
                noteSeen = true;
                undoLoopBack = false;
                if (!markerSet)
                {
                    add2undo(undoCmd, noteSeen);
                    markerSet = true;
                }
                else
                    add2undo(undoCmd, noteSeen, true);
            }
        }
        else
            add2undo(cmd, noteSeen);
    }

    if (insert == TOPLEVEL::insert::resonanceGraphInsert)
    {
        if (write)
            respar->setpoint(parameter, value_int);
        else
            cmd.data.value = respar->Prespoints[parameter];
        return;
    }

    switch (control)
    {
        case RESONANCE::control::maxDb:
            if (write)
                respar->PmaxdB = value;
            else
                value = respar->PmaxdB;
            break;
        case RESONANCE::control::centerFrequency:
            if (write)
                respar->Pcenterfreq = value;
            else
                value = respar->Pcenterfreq;
            break;
        case RESONANCE::control::octaves:
            if (write)
                respar->Poctavesfreq = value;
            else
                value = respar->Poctavesfreq;
            break;

        case RESONANCE::control::enableResonance:
            if (write)
                respar->Penabled = value_bool;
            else
                value = respar->Penabled;
            break;

        case RESONANCE::control::randomType:
            if (write)
                respar->randomize(value_int);
            break;

        case RESONANCE::control::interpolatePeaks:
            if (write)
                respar->interpolatepeaks(value_bool);
            break;
        case RESONANCE::control::protectFundamental:
            if (write)
                respar->Pprotectthefundamental = value_bool;
            else
                value = respar->Pprotectthefundamental;
            break;

        case RESONANCE::control::clearGraph:
            if (write)
                for (int i = 0; i < MAX_RESONANCE_POINTS; ++ i)
                    respar->setpoint(i, 64);
            break;
        case RESONANCE::control::smoothGraph:
            if (write)
                respar->smooth();
            break;
    }
    if (!write)
        cmd.data.value = value;
}


void InterChange::commandLFO(CommandBlock& cmd)
{
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;
    uchar insertParam = cmd.data.parameter;

    assert(npart < NUM_MIDI_PARTS);
    Part& part{*synth.part[npart]};

    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;
    if (write)
        add2undo(cmd, noteSeen);

    if (engine == PART::engine::addSynth)
    {
       switch (insertParam)
        {
           case TOPLEVEL::insertType::amplitude:
                lfoReadWrite(cmd, part.kit[kititem].adpars->GlobalPar.AmpLfo);
                break;
            case TOPLEVEL::insertType::frequency:
                lfoReadWrite(cmd, part.kit[kititem].adpars->GlobalPar.FreqLfo);
                break;
            case TOPLEVEL::insertType::filter:
                lfoReadWrite(cmd, part.kit[kititem].adpars->GlobalPar.FilterLfo);
                break;
        }
    }
    else if (engine == PART::engine::padSynth)
    {
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                lfoReadWrite(cmd, part.kit[kititem].padpars->AmpLfo.get());
                break;
            case TOPLEVEL::insertType::frequency:
                lfoReadWrite(cmd, part.kit[kititem].padpars->FreqLfo.get());
                break;
            case TOPLEVEL::insertType::filter:
                lfoReadWrite(cmd, part.kit[kititem].padpars->FilterLfo.get());
                break;
        }
    }
    else if (engine >= PART::engine::addVoice1)
    {
        int nvoice = engine - PART::engine::addVoice1;
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                lfoReadWrite(cmd, part.kit[kititem].adpars->VoicePar[nvoice].AmpLfo);
                break;
            case TOPLEVEL::insertType::frequency:
                lfoReadWrite(cmd, part.kit[kititem].adpars->VoicePar[nvoice].FreqLfo);
                break;
            case TOPLEVEL::insertType::filter:
                lfoReadWrite(cmd, part.kit[kititem].adpars->VoicePar[nvoice].FilterLfo);
                break;
        }
    }
}


void InterChange::lfoReadWrite(CommandBlock& cmd, LFOParams *pars)
{
    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;

    float val = cmd.data.value;

    switch (cmd.data.control)
    {
        case LFOINSERT::control::speed:
            if(pars->Pbpm) // set a flag so CLI can read the status
                cmd.data.offset = 1;
            if (write)
                pars->setPfreq(val * float(Fmul2I));
            else
                val = float(pars->PfreqI) / float(Fmul2I);
            break;
        case LFOINSERT::control::depth:
            if (write)
                pars->setPintensity(val);
            else
                val = pars->Pintensity;
            break;
        case LFOINSERT::control::delay:
            if (write)
                pars->setPdelay(val);
            else
                val = pars->Pdelay;
            break;
        case LFOINSERT::control::start:
            if (write)
                pars->setPstartphase(val);
            else
                val = pars->Pstartphase;
            break;
        case LFOINSERT::control::amplitudeRandomness:
            if (write)
                pars->setPrandomness(val);
            else
                val = pars->Prandomness;
            break;
        case LFOINSERT::control::type:
            if (write)
                pars->setPLFOtype(lrint(val));
            else
                val = pars->PLFOtype;
            break;
        case LFOINSERT::control::continuous:
            if (write)
                pars->setPcontinous(_SYS_::F2B(val));
            else
                val = pars->Pcontinous;
            break;
        case LFOINSERT::control::bpm:
            if (write)
                pars->setPbpm(_SYS_::F2B(val));
            else
                val = pars->Pbpm;
            break;
        case LFOINSERT::control::frequencyRandomness:
            if (write)
                pars->setPfreqrand(val);
            else
                val = pars->Pfreqrand;
            break;
        case LFOINSERT::control::stretch:
            if (write)
                pars->setPstretch(val);
            else
                val = pars->Pstretch;
            break;
    }

    if (write)
        pars->paramsChanged();
    else
        cmd.data.value = val;
}


void InterChange::commandFilter(CommandBlock& cmd)
{
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;

    assert(npart < NUM_MIDI_PARTS);
    Part& part{*(synth.part[npart])};

    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;
    if (write)
        add2undo(cmd, noteSeen);

    if (engine == PART::engine::addSynth)
    {
        filterReadWrite(cmd, part.kit[kititem].adpars->GlobalPar.GlobalFilter
                    , &part.kit[kititem].adpars->GlobalPar.PFilterVelocityScale
                    , &part.kit[kititem].adpars->GlobalPar.PFilterVelocityScaleFunction);
    }
    else if (engine == PART::engine::subSynth)
    {
        filterReadWrite(cmd, part.kit[kititem].subpars->GlobalFilter
                    , &part.kit[kititem].subpars->PGlobalFilterVelocityScale
                    , &part.kit[kititem].subpars->PGlobalFilterVelocityScaleFunction);
    }
    else if (engine == PART::engine::padSynth)
    {
        filterReadWrite(cmd, part.kit[kititem].padpars->GlobalFilter.get()
                    , &part.kit[kititem].padpars->PFilterVelocityScale
                    , &part.kit[kititem].padpars->PFilterVelocityScaleFunction);
    }
    else if (engine >= PART::engine::addVoice1)
    {
        int eng = engine - PART::engine::addVoice1;
        filterReadWrite(cmd, part.kit[kititem].adpars->VoicePar[eng].VoiceFilter
                    , &part.kit[kititem].adpars->VoicePar[eng].PFilterVelocityScale
                    , &part.kit[kititem].adpars->VoicePar[eng].PFilterVelocityScaleFunction);
    }
}


void InterChange::filterReadWrite(CommandBlock& cmd, FilterParams *pars, uchar *velsnsamp, uchar *velsns)
{
    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;

    float     val = cmd.data.value;
    int value_int = lrint(val);

    int nseqpos   = cmd.data.parameter;
    int nformant  = cmd.data.parameter;
    int nvowel    = cmd.data.offset;

    switch (cmd.data.control)
    {
        case FILTERINSERT::control::centerFrequency:
            if (write)
                pars->Pfreq = val;
            else
                val = pars->Pfreq;
            break;
        case FILTERINSERT::control::Q:
            if (write)
                pars->Pq = val;
            else
                val = pars->Pq;
            break;
        case FILTERINSERT::control::frequencyTracking:
            if (write)
                pars->Pfreqtrack = val;
            else
                val = pars->Pfreqtrack;
            break;
        case FILTERINSERT::control::velocitySensitivity:
            if (velsnsamp != NULL)
            {
                if (write)
                    *velsnsamp = value_int;
                else
                    val = *velsnsamp;
            }
            break;
        case FILTERINSERT::control::velocityCurve:
            if (velsns != NULL)
            {
                if (write)
                    *velsns = value_int;
                else
                    val = *velsns;
            }
            break;
        case FILTERINSERT::control::gain:
            if (write)
            {
                pars->Pgain = val;
                pars->changed = true;
            }
            else
                val = pars->Pgain;
            break;
        case FILTERINSERT::control::stages:
            if (write)
            {
                pars->Pstages = value_int;
                pars->changed = true;
            }
            else
                val = pars->Pstages;
            break;
        case FILTERINSERT::control::baseType:
            if (write)
            {
                if (pars->Pcategory != value_int)
                {
                    pars->Pgain = 64;
                    pars->Ptype = 0;
                    pars->changed = true;
                    pars->Pcategory = value_int;
                }
            }
            else
                val = pars->Pcategory;
            break;
        case FILTERINSERT::control::analogType:
        case FILTERINSERT::control::stateVariableType:
            if (write)
            {
                pars->Ptype = value_int;
                pars->changed = true;
            }
            else
                val = pars->Ptype;
            break;
        case FILTERINSERT::control::frequencyTrackingRange:
            if (write)
            {
                pars->Pfreqtrackoffset = (value_int != 0);
                pars->changed = true;
            }
            else
                val = pars->Pfreqtrackoffset;
            break;

        case FILTERINSERT::control::formantSlowness:
            if (write)
            {
                pars->Pformantslowness = val;
                pars->changed = true;
            }
            else
                val = pars->Pformantslowness;
            break;
        case FILTERINSERT::control::formantClearness:
            if (write)
            {
                pars->Pvowelclearness = val;
                pars->changed = true;
            }
            else
                val = pars->Pvowelclearness;
            break;
        case FILTERINSERT::control::formantFrequency:
            if (write)
            {
                pars->Pvowels[nvowel].formants[nformant].freq = val;
                pars->changed = true;
            }
            else
                val = pars->Pvowels[nvowel].formants[nformant].freq;
            break;
        case FILTERINSERT::control::formantQ:
            if (write)
            {
                pars->Pvowels[nvowel].formants[nformant].q = val;
                pars->changed = true;
            }
            else
                val = pars->Pvowels[nvowel].formants[nformant].q;
            break;
        case FILTERINSERT::control::formantAmplitude:
            if (write)
            {
                pars->Pvowels[nvowel].formants[nformant].amp = val;
                pars->changed = true;
            }
            else
                val = pars->Pvowels[nvowel].formants[nformant].amp;
            break;
        case FILTERINSERT::control::formantStretch:
            if (write)
            {
                pars->Psequencestretch = val;
                pars->changed = true;
            }
            else
                val = pars->Psequencestretch;
            break;
        case FILTERINSERT::control::formantCenter:
            if (write)
            {
                pars->Pcenterfreq = val;
                pars->changed = true;
            }
            else
                val = pars->Pcenterfreq;
            break;
        case FILTERINSERT::control::formantOctave:
            if (write)
            {
                pars->Poctavesfreq = val;
                pars->changed = true;
            }
            else
                val = pars->Poctavesfreq;
            break;

        case FILTERINSERT::control::numberOfFormants:
            if (write)
            {
                pars->Pnumformants = value_int;
                pars->changed = true;
            }
            else
                val = pars->Pnumformants;
            break;
        case FILTERINSERT::control::vowelNumber: // this is local to the GUI
            break;
        case FILTERINSERT::control::formantNumber: // this is local to the GUI
            break;
        case FILTERINSERT::control::sequenceSize:
            if (write)
            {
                pars->Psequencesize = value_int;
                pars->changed = true;
            }
            else
                val = pars->Psequencesize;
            break;
        case FILTERINSERT::control::sequencePosition:
            /*
             * this appears to be just setting the GUI
             * reference point yet sets pars changed.
             * why?
             */
            if (write)
                pars->changed = true;
            else
            {
                ;
            }
            break;
        case FILTERINSERT::control::vowelPositionInSequence:
            if (write)
            {
                pars->Psequence[nseqpos].nvowel = value_int;
                pars->changed = true;
            }
            else
                val = pars->Psequence[nseqpos].nvowel;
            break;
        case FILTERINSERT::control::negateInput:
            if (write)
            {
                pars->Psequencereversed = (value_int != 0);
                pars->changed = true;
            }
            else
                val = pars->Psequencereversed;
            break;
    }

    if (write)
        pars->paramsChanged();
    else
        cmd.data.value = val;
}


void InterChange::commandEnvelope(CommandBlock& cmd)
{
    uchar npart   = cmd.data.part;
    uchar kititem = cmd.data.kit;
    uchar engine  = cmd.data.engine;
    uchar insertParam = cmd.data.parameter;

    assert(npart < NUM_MIDI_PARTS);
    Part& part{*synth.part[npart]};

    if (engine == PART::engine::addSynth)
    {
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(cmd, part.kit[kititem].adpars->GlobalPar.AmpEnvelope);
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(cmd, part.kit[kititem].adpars->GlobalPar.FreqEnvelope);
                break;
            case TOPLEVEL::insertType::filter:
                envelopeReadWrite(cmd, part.kit[kititem].adpars->GlobalPar.FilterEnvelope);
                break;
        }
    }
    else if (engine == PART::engine::subSynth)
    {
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(cmd, part.kit[kititem].subpars->AmpEnvelope);
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(cmd, part.kit[kititem].subpars->FreqEnvelope);
                break;
            case TOPLEVEL::insertType::filter:
                envelopeReadWrite(cmd, part.kit[kititem].subpars->GlobalFilterEnvelope);
                break;
            case TOPLEVEL::insertType::bandwidth:
                envelopeReadWrite(cmd, part.kit[kititem].subpars->BandWidthEnvelope);
                break;
        }
    }
    else if (engine == PART::engine::padSynth)
    {
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(cmd, part.kit[kititem].padpars->AmpEnvelope.get());
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(cmd, part.kit[kititem].padpars->FreqEnvelope.get());
                break;
            case TOPLEVEL::insertType::filter:
                envelopeReadWrite(cmd, part.kit[kititem].padpars->FilterEnvelope.get());
                break;
        }
    }

    else if (engine >= PART::engine::addMod1)
    {
        int nvoice = engine - PART::engine::addMod1;
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(cmd, part.kit[kititem].adpars->VoicePar[nvoice].FMAmpEnvelope);
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(cmd, part.kit[kititem].adpars->VoicePar[nvoice].FMFreqEnvelope);
                break;
        }
    }

    else if (engine >= PART::engine::addVoice1)
    {
        int nvoice = engine - PART::engine::addVoice1;
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(cmd, part.kit[kititem].adpars->VoicePar[nvoice].AmpEnvelope);
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(cmd, part.kit[kititem].adpars->VoicePar[nvoice].FreqEnvelope);
                break;
            case TOPLEVEL::insertType::filter:
                envelopeReadWrite(cmd, part.kit[kititem].adpars->VoicePar[nvoice].FilterEnvelope);
                break;
        }
    }
}


void InterChange::envelopeReadWrite(CommandBlock& cmd, EnvelopeParams *pars)
{
    //int val = int(cmd.data.value) & 0x7f; // redo not currently restoring correct values
    float val = cmd.data.value;
    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;
    uchar insert = cmd.data.insert;
    uchar Xincrement = cmd.data.offset;

    if (cmd.data.control == ENVELOPEINSERT::control::enableFreeMode)
    {
        if (write)
        {
            add2undo(cmd, noteSeen);
            pars->Pfreemode = (val != 0);
        }
        else
            val = pars->Pfreemode;
        cmd.data.value = pars->Pfreemode;
        return;
    }

    size_t envpoints = pars->Penvpoints;

    if (pars->Pfreemode)
    {
        bool doReturn = true;
        switch (insert)
        {
            case TOPLEVEL::insert::envelopePointAdd:
                envelopePointAdd(cmd, pars);
                break;
            case TOPLEVEL::insert::envelopePointDelete:
                envelopePointDelete(cmd, pars);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                envelopePointChange(cmd, pars);
                break;
            default:
            {
                if (cmd.data.control == ENVELOPEINSERT::control::points)
                {
                    if (!pars->Pfreemode)
                    {
                        val = UNUSED;
                        Xincrement = UNUSED;
                    }
                    else
                    {
                        val = envpoints;
                        Xincrement = envpoints; // don't really need this now
                    }
                }
                else if (cmd.data.control == ENVELOPEINSERT::control::sustainPoint)
                {
                    if (write)
                        pars->Penvsustain = val<0? 0 : val;
                    else
                        val = pars->Penvsustain;
                }
                else
                    doReturn = false;
            }
            break;
        }
        if (doReturn) // some controls are common to both
        {
            cmd.data.value = val;
            cmd.data.offset = Xincrement;
            return;
        }
    }
    else if (insert != TOPLEVEL::insert::envelopeGroup)
    {
        cmd.data.value = UNUSED;
        cmd.data.offset = UNUSED;
        return;
    }

    if (write)
        add2undo(cmd, noteSeen);

    switch (cmd.data.control)
    {
        case ENVELOPEINSERT::control::attackLevel:
            if (write)
                pars->PA_val = val;
            else
                val = pars->PA_val;
            break;
        case ENVELOPEINSERT::control::attackTime:
            if (write)
                pars->PA_dt = val;
            else
                val = pars->PA_dt;
            break;
        case ENVELOPEINSERT::control::decayLevel:
            if (write)
                pars->PD_val = val;
            else
                val = pars->PD_val;
            break;
        case ENVELOPEINSERT::control::decayTime:
            if (write)
                pars->PD_dt = val;
            else
                val = pars->PD_dt;
            break;
        case ENVELOPEINSERT::control::sustainLevel:
            if (write)
                pars->PS_val = val;
            else
                val = pars->PS_val;
            break;
        case ENVELOPEINSERT::control::releaseTime:
            if (write)
                pars->PR_dt = val;
            else
                val = pars->PR_dt;
            break;
        case ENVELOPEINSERT::control::releaseLevel:
            if (write)
                pars->PR_val = val;
            else
                val = pars->PR_val;
            break;
        case ENVELOPEINSERT::control::stretch:
            if (write)
                pars->Penvstretch = val;
            else
                val = pars->Penvstretch;
            break;

        case ENVELOPEINSERT::control::forcedRelease:
            if (write)
                pars->Pforcedrelease = (val != 0);
            else
                val = pars->Pforcedrelease;
            break;
        case ENVELOPEINSERT::control::linearEnvelope:
            if (write)
                pars->Plinearenvelope = (val != 0);
            else
                val = pars->Plinearenvelope;
            break;

        case ENVELOPEINSERT::control::edit:
            break;

        default:
            val = UNUSED;
            Xincrement = UNUSED;
            break;
    }
    if (write)
    {
        pars->paramsChanged();
    }
    cmd.data.value = val;
    cmd.data.offset = Xincrement;
    return;
}


void InterChange::envelopePointAdd(CommandBlock& cmd, EnvelopeParams *pars)
{
    uchar point = cmd.data.control;
    uchar Xincrement = cmd.data.offset;
    float val  =  cmd.data.value;
    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;
    size_t envpoints = pars->Penvpoints;


        if (!write || point == 0 || point >= envpoints)
        {
            cmd.data.value = UNUSED;
            cmd.data.offset = envpoints;
            return;
        }

        if (cameFrom != envControl::undo)
        {
            if (envpoints < MAX_ENVELOPE_POINTS)
            {
                if (cameFrom == envControl::input)
                    addFixed2undo(cmd);

                pars->Penvpoints += 1;
                assert (0 < point && point < envpoints);
                for (size_t i = envpoints; i >= point; -- i)
                {
                    pars->Penvdt[i + 1] = pars->Penvdt[i];
                    pars->Penvval[i + 1] = pars->Penvval[i];
                }

                if (point == 0)
                    pars->Penvdt[1] = 64;

                if (point <= pars->Penvsustain)
                    ++ pars->Penvsustain;

                pars->Penvdt[point] = Xincrement;
                pars->Penvval[point] = val;
                cmd.data.value = val;
                cmd.data.offset = Xincrement;
                pars->paramsChanged();
            }
            else
            {
                cmd.data.value = UNUSED;
            }
            return;
        }

        if (envpoints < 4)
        {
            cmd.data.value = UNUSED;
            cmd.data.offset = UNUSED;
            return; // can't have less than 4
        }
        else
        {
            assert (0 < point && point < envpoints);
            assert (3 < envpoints);
            envpoints -= 1;
            for (size_t i = point; i < envpoints; ++ i)
            {
                pars->Penvdt[i] = pars->Penvdt[i + 1];
                pars->Penvval[i] = pars->Penvval[i + 1];
            }
            if (point <= pars->Penvsustain)
                -- pars->Penvsustain;
            pars->Penvpoints = envpoints;
            cmd.data.value = envpoints;
            pars->paramsChanged();
        }

}


void InterChange::envelopePointDelete(CommandBlock& cmd, EnvelopeParams *pars)
{
    uchar point = cmd.data.control;
    uchar Xincrement = cmd.data.offset;
    float val  =  cmd.data.value;
    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;
    size_t envpoints = pars->Penvpoints;

        if (!write || point == 0 || point >= envpoints)
        {
            cmd.data.value = UNUSED;
            cmd.data.offset = envpoints;
            return;
        }

        if (cameFrom != envControl::input && cameFrom != envControl::redo)
        {
            if (envpoints < MAX_ENVELOPE_POINTS)
            {
                pars->Penvpoints += 1;
                for (size_t i = envpoints; i >= point; -- i)
                {
                    pars->Penvdt[i + 1] = pars->Penvdt[i];
                    pars->Penvval[i + 1] = pars->Penvval[i];
                }

                if (point == 0)
                    pars->Penvdt[1] = 64;

                if (point <= pars->Penvsustain)
                    ++ pars->Penvsustain;

                pars->Penvdt[point] = Xincrement;
                pars->Penvval[point] = val;
                cmd.data.value = val;
                cmd.data.offset = Xincrement;
                pars->paramsChanged();
            }
            else
            {
                cmd.data.value = UNUSED;
            }
            return;
        }

        if (envpoints < 4)
        {
            cmd.data.value = UNUSED;
            cmd.data.offset = UNUSED;
            return; // can't have less than 4
        }
        else
        {
            if (cameFrom == envControl::input)
            {
                cmd.data.source = 0;
                cmd.data.type &= TOPLEVEL::type::Write;
                cmd.data.offset = pars->Penvdt[point];
                cmd.data.value = pars->Penvval[point];
                addFixed2undo(cmd);
            }
            assert (0 < point && point < envpoints);
            assert (3 < envpoints);
            envpoints -= 1;
            for (size_t i = point; i < envpoints; ++ i)
            {
                pars->Penvdt[i] = pars->Penvdt[i + 1];
                pars->Penvval[i] = pars->Penvval[i + 1];
            }
            if (point <= pars->Penvsustain)
                -- pars->Penvsustain;
            pars->Penvpoints = envpoints;
            cmd.data.value = envpoints;
            pars->paramsChanged();
        }
}


void InterChange::envelopePointChange(CommandBlock& cmd, EnvelopeParams *pars)
{
    uchar point = cmd.data.control;
    uchar Xincrement = cmd.data.offset;
    float val  =  cmd.data.value;
    bool write = (cmd.data.type & TOPLEVEL::type::Write) > 0;
    size_t envpoints = pars->Penvpoints;

    if (point >= envpoints)
    {
        cmd.data.value = UNUSED;
        cmd.data.offset = UNUSED;
        return;
    }
    if (write)
    {
        add2undo(cmd, noteSeen);
        pars->Penvval[point] = val;
        if (point == 0)
        {
            Xincrement = 0;
        }
        else
        {
            pars->Penvdt[point] = Xincrement;
        }
        pars->paramsChanged();
    }
    else
    {
        val = pars->Penvval[point];
        Xincrement = pars->Penvdt[point];
    }
    cmd.data.value = val;
    cmd.data.offset = Xincrement;
    return;
}


void InterChange::commandSysIns(CommandBlock& cmd)
{
    float value   = cmd.data.value;
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar insert  = cmd.data.insert;

    bool write = (type & TOPLEVEL::type::Write) > 0;

    int value_int = lrint(value);
    bool isSysEff = (npart == TOPLEVEL::section::systemEffects);
    uchar effnum = isSysEff? synth.syseffnum
                           : synth.inseffnum;

    if (insert == UNUSED)
    {
        switch (control)
        {
            case EFFECT::sysIns::effectNumber:
                if (write)
                {
                    if (isSysEff)
                    {
                        synth.syseffnum = value_int;
                        cmd.data.parameter = (synth.sysefx[value_int]->geteffectpar(-1) != 0);
                    }
                    else
                    {
                        synth.inseffnum = value_int;
                        cmd.data.parameter = (synth.insefx[value_int]->geteffectpar(-1) != 0);
                    }
                    synth.pushEffectUpdate(npart);
                    cmd.data.source |= cmd.data.source |= TOPLEVEL::action::forceUpdate;
                    // the line above is to show it's changed from preset values
                    cmd.data.engine = value_int;
                }
                else
                {
                    if (isSysEff)
                        value = synth.syseffnum;
                    else
                        value = synth.inseffnum;
                }
                break;
            case EFFECT::sysIns::effectType:
                if (write)
                {
                    if (isSysEff)
                    {
                        synth.sysefx[effnum]->changeeffect(value_int);
                    }
                    else
                    {
                        synth.insefx[effnum]->changeeffect(value_int);
                        auto& destination = synth.Pinsparts[effnum];
                        if (value_int > 0 and destination == -1)
                        {// if it was disabled before, pre-select current part as convenience
                            destination = synth.getRuntime().currentPart;
                        }
                    }   // push GUI update since module for effnum is currently exposed in GUI
                    synth.pushEffectUpdate(npart);
                    cmd.data.offset = 0;
                }
                else
                {
                    if (isSysEff)
                        value = synth.sysefx[effnum]->geteffect();
                    else
                        value = synth.insefx[effnum]->geteffect();
                }
                break;
            case EFFECT::sysIns::effectDestination: // insert only
                if (write)
                {
                    synth.Pinsparts[effnum] = value_int;
                    if (value_int == -1)
                        synth.insefx[effnum]->cleanup();
                    synth.pushEffectUpdate(npart); // isInsert and currently exposed in GUI
                }
                else
                    value = synth.Pinsparts[effnum];
                break;
            case EFFECT::sysIns::effectEnable: // system only
                if (write)
                {
                    bool newSwitch = _SYS_::F2B(value);
                    bool oldSwitch = synth.syseffEnable[effnum];
                    synth.syseffEnable[effnum] = newSwitch;
                    if (newSwitch != oldSwitch)
                    {
                        synth.sysefx[effnum]->cleanup();
                        synth.pushEffectUpdate(npart); // not isInsert currently exposed in GUI
                    }
                }
                else
                    value = synth.syseffEnable[effnum];
                break;
        }
    }
    else // system only
    {
        if (write)
        {
            synth.setPsysefxsend(effnum, control, value);
            synth.pushEffectUpdate(npart);
        }
        else
            value = synth.Psysefxsend[effnum][control];
    }

    if (!write)
        cmd.data.value = value;

}


void InterChange::commandEffects(CommandBlock& cmd)
{
    float value   = cmd.data.value;
    int value_int = int(value + 0.5f);
    uchar type    = cmd.data.type;
    uchar control = cmd.data.control;
    uchar npart   = cmd.data.part;
    uchar effSend = cmd.data.kit;
    uchar effnum  = cmd.data.engine;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
    {
        cmd.data.source |= TOPLEVEL::action::forceUpdate;
        // the line above is to show it's changed from preset values
        add2undo(cmd, noteSeen);
    }

    EffectMgr *eff;

    if (npart == TOPLEVEL::section::systemEffects)
        eff = synth.sysefx[effnum];
    else if (npart == TOPLEVEL::section::insertEffects)
        eff = synth.insefx[effnum];
    else if (npart < NUM_MIDI_PARTS)
        eff = synth.part[npart]->partefx[effnum];
    else
        return; // invalid part number

    if (effSend >= EFFECT::type::count)
        return; // invalid kit number


    auto maybeUpdateGui = [&]{
                                bool isPart{npart < NUM_MIDI_PARTS};
                                bool isInsert{npart != TOPLEVEL::section::systemEffects};
                                int currPartNr = synth.getRuntime().currentPart;
                                uchar currEff = isPart? synth.part[currPartNr]->Peffnum : isInsert? synth.inseffnum : synth.syseffnum;
                                // push update to GUI if the change affects the current effect module exposed in GUI
                                if (effnum == currEff && (!isPart || npart == currPartNr))
                                    synth.pushEffectUpdate(npart);
                             };

    if (control != PART::control::effectType && effSend != (eff->geteffect() + EFFECT::type::none)) // geteffect not yet converted
    {
        if ((cmd.data.source & TOPLEVEL::action::noAction) != TOPLEVEL::action::fromMIDI && control != TOPLEVEL::control::copyPaste)
            synth.getRuntime().Log("Not Available"); // TODO sort this better for CLI as well as MIDI
        cmd.data.source = TOPLEVEL::action::noAction;
        return;
    }

    if (eff->geteffectpar(EFFECT::control::bpm) == 1)
    {
        cmd.data.offset = 1; // mark this for reporting in Data2Text
        if (eff->geteffectpar(EFFECT::control::sepLRDelay) == 1)
            cmd.data.offset = 3; // specific for Echo effect
    }

    if (effSend == EFFECT::type::dynFilter && cmd.data.insert != UNUSED)
    {
        if (write)
            eff->seteffectpar(-1, true); // effect changed
        filterReadWrite(cmd, eff->filterpars,NULL,NULL);
        if (write)
            maybeUpdateGui();
        return;
    }
    if (control == EFFECT::control::changed)
    {
        if (!write)
        {
            value = eff->geteffectpar(-1);
            cmd.data.value = value;
        }
        return; // specific for reading change status
    }
    if (write)
    {
        if (effSend == EFFECT::type::eq)
        /*
         * specific to EQ
         * Control 1 is not a normal parameter, but a band index.
         * Also, EQ does not have presets, and 16 is the control
         * for the band 1 frequency parameter
        */
        {
            uchar band = cmd.data.parameter;
            if (control <= 1)
                eff->seteffectpar(control, value_int);
            else
            {
                if (band != UNUSED)
                {
                   eff->seteffectpar(1, band); // should always be the case
                }
                else
                {
                    band = eff->geteffectpar(1);
                    cmd.data.parameter = band;
                }
                eff->seteffectpar(control + (band * 5), value_int);
            }
        }
        else
        {
            if (control == EFFECT::control::preset)
                eff->changepreset(value_int);
            else
            {
                eff->seteffectpar(control, value_int);
                if (effSend == EFFECT::type::reverb && control == 10 && value_int == 2)
                    // bandwidth type update for GUI
                    cmd.data.offset = eff->geteffectpar(12);
            }
        }
        maybeUpdateGui();
    }
    else
    {
        if (effSend == EFFECT::type::eq && control > 1) // specific to EQ
        {
            value = eff->geteffectpar(control + (eff->geteffectpar(1) * 5));
            cmd.data.parameter = eff->geteffectpar(1);
        }
        else
        {
            if (control == EFFECT::control::preset)
                value = eff->getpreset();
            else
                value = eff->geteffectpar(control);
        }
    }

    if (!write)
        cmd.data.value = value;
}

void InterChange::addFixed2undo(CommandBlock& cmd)
{
    redoList.clear(); // always invalidated on new entry
    undoList.push_back(undoMarker);
    undoList.push_back(cmd);
}


void InterChange::add2undo(CommandBlock& cmd, bool& noteSeen, bool group)
{
    if (undoLoopBack)
    {
        undoLoopBack = false;
        return; // don't want to reset what we've just undone!
    }

    redoList.clear(); // always invalidated on new entry

    if (noteSeen || undoList.empty())
    {
        noteSeen = false;
        if (!group)
        {
            undoList.push_back(undoMarker);
        }
    }
    else if (!group)
    {
        if (undoList.back().data.control == cmd.data.control
            && undoList.back().data.part == cmd.data.part
            && undoList.back().data.kit == cmd.data.kit
            && undoList.back().data.engine == cmd.data.engine
            && undoList.back().data.insert == cmd.data.insert
            && undoList.back().data.parameter == cmd.data.parameter)
            return;
        undoList.push_back(undoMarker);
    }

    /*
     * the following is used to read the current value of the specific
     * control as that is what we will want to revert to.
     */
    CommandBlock candidate;
    memcpy(candidate.bytes, cmd.bytes, sizeof(CommandBlock));
    candidate.data.type &= TOPLEVEL::type::Integer;
    candidate.data.source = 0;
    commandSendReal(candidate);

    candidate.data.source = cmd.data.source | TOPLEVEL::action::forceUpdate;
    candidate.data.type = cmd.data.type;
    undoList.push_back(candidate);
}


void InterChange::undoLast(CommandBlock& candidate)
{
    std::list<CommandBlock> *source;
    std::list<CommandBlock> *dest;
    if (!setRedo)
    {
        source = &undoList;
        dest = &redoList;
        cameFrom = envControl::undo;
    }
    else
    {
        cameFrom = envControl::redo;
        source = &redoList;
        dest = &undoList;
    }

    if (source->empty())
    {
        setUndo = false;
        setRedo = false;
        return;
    }
    if (source->back().data.part == TOPLEVEL::undoMark)
    {
        setUndo = false;
        setRedo = false;
        source->pop_back();
        return;
    }
    undoLoopBack = true;
    CommandBlock oldCommand;
    memcpy(candidate.bytes, source->back().bytes, sizeof(CommandBlock));

    if (undoStart)
    {
        dest->push_back(undoMarker);
        undoStart = false;
    }
    memcpy(oldCommand.bytes, source->back().bytes, sizeof(CommandBlock));
    char tempsource = oldCommand.data.source;
    if(oldCommand.data.insert != TOPLEVEL::insert::envelopePointAdd && oldCommand.data.insert != TOPLEVEL::insert::envelopePointDelete)
    {
        char temptype = oldCommand.data.type;
        oldCommand.data.type &= TOPLEVEL::type::Integer;
        oldCommand.data.source = 0;
        commandSendReal(oldCommand);
        oldCommand.data.type = temptype;
    }
    oldCommand.data.source = tempsource;
    dest->push_back(oldCommand);
    source->pop_back();

    if (source->empty())
    {
        setUndo = false;
        setRedo = false;
    }
    else if (source->back().data.part == TOPLEVEL::undoMark)
    {
        setUndo = false;
        setRedo = false;
        source->pop_back();
    }
}


void InterChange::undoRedoClear()
{
    undoList.clear();
    redoList.clear();
    noteSeen = false;
    undoLoopBack = false;
    undoStart = false;
}


// tests and returns corrected values
void InterChange::testLimits(CommandBlock& cmd)
{
    float value = cmd.data.value;
    int control = cmd.data.control;

    /*
     * This is a special case as existing defined
     * midi CCs need to be checked.
     * I don't like special cases either :(
     */
    if (cmd.data.part == TOPLEVEL::section::config &&
        (
            control == CONFIG::control::bankRootCC
         || control == CONFIG::control::bankCC
         || control == CONFIG::control::extendedProgramChangeCC)
        )
    {
        cmd.data.miscmsg = NO_MSG; // just to be sure
        if (value > 119) // we don't want controllers above this
            return;
        // TODO can bank and bankroot be combined
        // as they now have the same options?
        string text;
        if (control == CONFIG::control::bankRootCC)
        {
            text = synth.getRuntime().masterCCtest(int(value));
            if (text != "")
                cmd.data.miscmsg = textMsgBuffer.push(text);
            return;
        }
        if (control == CONFIG::control::bankCC)
        {
            if (value != 0 && value != 32)
                return;
            text = synth.getRuntime().masterCCtest(int(value));
            if (text != "")
                cmd.data.miscmsg = textMsgBuffer.push(text);
            return;
        }
        text = synth.getRuntime().masterCCtest(int(value));
        if (text != "")
            cmd.data.miscmsg = textMsgBuffer.push(text);
        return;
    }
}


// more work needed here :(
float InterChange::returnLimits(CommandBlock& cmd)
{
    // bit 5 set is used to denote midi learnable
    // bit 7 set denotes the value is used as an integer

    int control   = int( cmd.data.control);
    int npart     = int( cmd.data.part   );
    int kititem   = int( cmd.data.kit    );
    int effSend   = int( cmd.data.kit    );
    int engine    = int( cmd.data.engine );
    int insert    = int( cmd.data.insert );
    int parameter = int( cmd.data.parameter);
    int miscmsg   = int( cmd.data.miscmsg);

    float value = cmd.data.value;

    cmd.data.type &= TOPLEVEL::type::Default; // clear all flags
    int request = cmd.data.type; // catches Adj, Min, Max, Def
    cmd.data.type |= TOPLEVEL::type::Integer; // default is integer & not learnable

    if (npart == TOPLEVEL::section::config)
    {
        std::cout << "calling config limits" << std::endl;
        return synth.getRuntime().getConfigLimits(&cmd);
    }

    if (npart == TOPLEVEL::section::bank)
        return value;

    if (npart == TOPLEVEL::section::main)
        return synth.getLimits(&cmd);

    if (npart == TOPLEVEL::section::scales)
        return synth.microtonal.getLimits(&cmd);

    if (npart == TOPLEVEL::section::vector)
    {
        std::cout << "calling vector limits" << std::endl;
        return synth.vectorcontrol.getVectorLimits(&cmd);
    }

    float min;
    float max;
    float def;

    if (insert == TOPLEVEL::insert::filterGroup)
    {
        filterLimit filterLimits;
        if (kititem == EFFECT::type::dynFilter)
        {
            /*
             * This is somewhat convoluted!
             * Only for dynFilter we need to find the preset number.
             * Default frequency and Q are different over the 5 presets.
             */
            CommandBlock effectCmd;
            memcpy(effectCmd.bytes, cmd.bytes, sizeof(CommandBlock));
            effectCmd.data.type = 0;
            effectCmd.data.source = 0;
            effectCmd.data.insert = UNUSED;
            cmd.data.offset = (cmd.data.offset & 15) | (int(effectCmd.data.value) >> 4);
            effectCmd.data.control = EFFECT::control::preset;
            commandEffects(effectCmd);
        }
        return filterLimits.getFilterLimits(&cmd);
    }
    // should prolly move other inserts up here

    if (effSend >= EFFECT::type::none and effSend < EFFECT::type::count)
    {
        LimitMgr limits;
        return limits.geteffectlimits(&cmd);
    }

    if (npart < NUM_MIDI_PARTS)
    {
        Part& part{* synth.part[npart]};

        if (engine == PART::engine::subSynth && (insert == UNUSED || (insert >= TOPLEVEL::oscillatorGroup && insert <= TOPLEVEL::harmonicBandwidth)) && parameter == UNUSED)
        {
            SUBnoteParameters *subpars;
            subpars = part.kit[kititem].subpars;
            return subpars->getLimits(&cmd);
        }

        if (insert == TOPLEVEL::insert::partEffectSelect || (engine == UNUSED && (kititem == UNUSED || insert == TOPLEVEL::insert::kitGroup)))
            return part.getLimits(&cmd);

        if ((insert == TOPLEVEL::insert::kitGroup || insert == UNUSED) && parameter == UNUSED && miscmsg == UNUSED)
        {
            if (engine == PART::engine::addSynth || (engine >= PART::engine::addVoice1 && engine < PART::engine::addVoiceModEnd))
            {
                ADnoteParameters* adpars{part.kit[kititem].adpars};
                return adpars->getLimits(&cmd);
            }
            if (engine == PART::engine::subSynth)
            {
                SUBnoteParameters* subpars{part.kit[kititem].subpars};
                return subpars->getLimits(&cmd);
            }
            if (engine == PART::engine::padSynth)
            {
                PADnoteParameters* padpars{part.kit[kititem].padpars};
                return padpars->getLimits(&cmd);
            }
            // there may be other stuff

            min = 0;
            max = 127;
            def = 0;

            Log("Using engine defaults");
            switch (request)
            {
                case TOPLEVEL::type::Adjust:
                    if (value < min)
                        value = min;
                    else if (value > max)
                        value = max;
                    break;
                case TOPLEVEL::type::Minimum:
                    value = min;
                    break;
                case TOPLEVEL::type::Maximum:
                    value = max;
                    break;
                case TOPLEVEL::type::Default:
                    value = def;
                    break;
            }
            return value;
        }
        if (insert >= TOPLEVEL::insert::oscillatorGroup && insert <= TOPLEVEL::insert::harmonicPhase)
        {
            return part.kit[0].adpars->VoicePar[0].POscil->getLimits(&cmd);
            // we also use this for pad limits
            // as oscillator values identical
        }
        if (insert == TOPLEVEL::insert::resonanceGroup || insert == TOPLEVEL::insert::resonanceGraphInsert)
        {
            ResonanceLimits resonancelimits;
            return resonancelimits.getLimits(&cmd);
        }
        if (insert == TOPLEVEL::insert::LFOgroup && engine != PART::engine::subSynth && parameter <= TOPLEVEL::insertType::filter)
        {
            LFOlimit lfolimits;
            return lfolimits.getLFOlimits(&cmd);
        }
        if (insert == TOPLEVEL::insert::envelopeGroup)
        {
            envelopeLimit envelopeLimits;
            return envelopeLimits.getEnvelopeLimits(&cmd);
        }
        if (insert == TOPLEVEL::insert::envelopePointAdd || insert == TOPLEVEL::insert::envelopePointDelete || insert == TOPLEVEL::insert::envelopePointChange)
            return 1; // temporary solution :(
        min = 0;
        max = 127;
        def = 0;
        Log("Using insert defaults");

            switch (request)
            {
                case TOPLEVEL::type::Adjust:
                    if (value < min)
                        value = min;
                    else if (value > max)
                        value = max;
                break;
                case TOPLEVEL::type::Minimum:
                    value = min;
                    break;
                case TOPLEVEL::type::Maximum:
                    value = max;
                    break;
                case TOPLEVEL::type::Default:
                    value = def;
                    break;
            }
            return value;
    }

    // not sure where the following should really be
    if (npart == TOPLEVEL::section::systemEffects)
    {
        min = 0;
        def = 0;
        max = 8;
        switch (control)
        {
            case EFFECT::sysIns::toEffect1:
            case EFFECT::sysIns::toEffect2:
            case EFFECT::sysIns::toEffect3:
                max = 127;
                cmd.data.type |= TOPLEVEL::type::Learnable;
                break;
            case EFFECT::sysIns::effectNumber:
                max = 3;
                break;
            case EFFECT::sysIns::effectType:
                break;
            case EFFECT::sysIns::effectEnable:
                def = 1;
                max = 1;
                cmd.data.type |= TOPLEVEL::type::Learnable;
                break;
        }

        switch (request)
        {
            case TOPLEVEL::type::Adjust:
                if (value < min)
                    value = min;
                else if (value > max)
                    value = max;
            break;
            case TOPLEVEL::type::Minimum:
                value = min;
                break;
            case TOPLEVEL::type::Maximum:
                value = max;
                break;
            case TOPLEVEL::type::Default:
                value = def;
                break;
        }
        return value;
    }

    if (npart == TOPLEVEL::section::insertEffects)
    {
        min = 0;
        def = 0;
        max = 8;
        switch (control)
        {
            case EFFECT::sysIns::effectNumber:
                max = 7;
                break;
            case EFFECT::sysIns::effectType:
                break;
            case EFFECT::sysIns::effectDestination:
                min = -2;
                def = -1;
                max = 63;
                break;
        }

        switch (request)
        {
            case TOPLEVEL::type::Adjust:
                if (value < min)
                    value = min;
                else if (value > max)
                    value = max;
            break;
            case TOPLEVEL::type::Minimum:
                value = min;
                break;
            case TOPLEVEL::type::Maximum:
                value = max;
                break;
            case TOPLEVEL::type::Default:
                value = def;
                break;
        }
        return value;
    }

    if (npart == TOPLEVEL::section::midiIn)
    {
        min = 0;
        max = 127;
        switch (control)
        {
            case PART::control::volume:
                def = 96;
                break;
            case PART::control::midiExpression:
                def = 127;
                break;
            case PART::control::midiSustain:
                def = 0;
                break;
            case PART::control::midiPortamento:
                def = 0;
                break;
            case PART::control::midiFMamp:
                def = 127;
                break;
            default:
                def = 64;
                break;
        }

        switch (request)
        {
            case TOPLEVEL::type::Adjust:
                if (value < min)
                    value = min;
                else if (value > max)
                    value = max;
            break;
            case TOPLEVEL::type::Minimum:
                value = min;
                break;
            case TOPLEVEL::type::Maximum:
                value = max;
                break;
            case TOPLEVEL::type::Default:
                value = def;
                break;
        }
        return value;
    }

    if (npart == TOPLEVEL::section::undoMark)
    {
        switch (control)
        {
            case MAIN::control::undo:
            case MAIN::control::redo:
                return value; // these have no limits!
            break;
        }
    }

    min = 0;
    max = 127;
    def = 0;
    Log("Unidentified Limit request: using dummy defaults");

    switch (request)
    {
        case TOPLEVEL::type::Adjust:
            if (value < min)
                value = min;
            else if (value > max)
                value = max;
        break;
        case TOPLEVEL::type::Minimum:
            value = min;
            break;
        case TOPLEVEL::type::Maximum:
            value = max;
            break;
        case TOPLEVEL::type::Default:
            value = def;
            break;
    }
    return value;
}
