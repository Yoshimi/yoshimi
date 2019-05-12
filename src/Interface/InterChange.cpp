/*
    InterChange.cpp - General communications

    Copyright 2016-2019, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    Modified May 2019
*/

#include <iostream>
#include <string>
#include <algorithm>
#include <cfloat>
#include <bitset>
#include <unistd.h>

#include "Interface/InterChange.h"
#include "Misc/MiscFuncs.h"
#include "Misc/Microtonal.h"
#include "Misc/SynthEngine.h"
#include "Misc/Part.h"
#include "Params/Controller.h"
#include "Params/ADnoteParameters.h"
#include "Params/SUBnoteParameters.h"
#include "Params/PADnoteParameters.h"
#include "Params/LFOParams.h"
#include "Params/FilterParams.h"
#include "Params/EnvelopeParams.h"
#include "Effects/EffectMgr.h"
#include "Synth/Resonance.h"
#include "Synth/OscilGen.h"
#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif

extern void mainRegisterAudioPort(SynthEngine *s, int portnum);
extern SynthEngine *firstSynth;

std::string singlePath;
std::string runGui;
int startInstance = 0;

InterChange::InterChange(SynthEngine *_synth) :
    synth(_synth),
#ifndef YOSHIMI_LV2_PLUGIN
    fromCLI(NULL),
#endif
    decodeLoopback(NULL),
#ifdef GUI_FLTK
    fromGUI(NULL),
    toGUI(NULL),
#endif
    fromMIDI(NULL),
    returnsBuffer(NULL),
    blockRead(0),
    tick(0),
    lockTime(0),
    swapRoot1(UNUSED),
    swapBank1(UNUSED),
    swapInstrument1(UNUSED)
{   // this is repeated here as it might somehow get called from LV2
    singlePath = std::string(getenv("HOME")) + "/.yoshimiSingle";
}


bool InterChange::Init()
{
    flagsValue = 0xffffffff;
#ifndef YOSHIMI_LV2_PLUGIN
    fromCLI = new ringBuff(256, commandBlockSize);
#endif
    decodeLoopback = new ringBuff(1024, commandBlockSize);
#ifdef GUI_FLTK
    fromGUI = new ringBuff(512, commandBlockSize);
    toGUI = new ringBuff(1024, commandBlockSize);
#endif
    fromMIDI = new ringBuff(1024, commandBlockSize);

    returnsBuffer = new ringBuff(1024, commandBlockSize);

    if (!synth->getRuntime().startThread(&sortResultsThreadHandle, _sortResultsThread, this, false, 0, "CLI"))
    {
        synth->getRuntime().Log("Failed to start CLI resolve thread");
        goto bail_out;
    }
    return true;


bail_out:
#ifndef YOSHIMI_LV2_PLUGIN
    if (fromCLI)
    {
        delete(fromCLI);
        fromCLI = NULL;
    }
#endif
    if (decodeLoopback)
    {
        delete(decodeLoopback);
        decodeLoopback = NULL;
    }
#ifdef GUI_FLTK
    if (fromGUI)
    {
        delete(fromGUI);
        fromGUI = NULL;
    }
    if (toGUI)
    {
        delete(toGUI);
        toGUI = NULL;
    }
#endif
    if (fromMIDI)
    {
        delete(fromMIDI);
        fromMIDI = NULL;
    }
    if (returnsBuffer)
    {
        delete(returnsBuffer);
        returnsBuffer = NULL;
    }
    return false;
}


void *InterChange::_sortResultsThread(void *arg)
{
    return static_cast<InterChange*>(arg)->sortResultsThread();
}


void *InterChange::sortResultsThread(void)
{
    while(synth->getRuntime().runSynth)
    {
        /*
         * To maitain portability we synthesise a very simple low accuracy
         * timer based on the loop time of this function. As it makes no system
         * calls apart from usleep() it is lightweight and should have no thread
         * safety issues. It is used mostly for timeouts.
         */
        ++ tick;
        /*
        if (!(tick & 8191))
        {
            if (tick & 16383)
                std::cout << "Tick" << std::endl;
            else
                std::cout << "Tock" << std::endl;
        }*/

        // a false positive here is not actually a problem.
        unsigned char testRead = blockRead;//__sync_or_and_fetch(&blockRead, 0);
        if (lockTime == 0 && testRead != 0)
        {
            tick |= 1; // make sure it's not zero
            lockTime = tick;
        }
        else if (lockTime > 0 && testRead == 0)
            lockTime = 0;
 // local to source
        else if (lockTime > 0 && (tick - lockTime) > 32766)
        { // about 4 seconds - may need improving

            std::cout << "stuck read block cleared" << std::endl;
            blockRead = 0;//__sync_and_and_fetch(&blockRead, 0);
            lockTime = 0;
        }

        CommandBlock getData;
        while (decodeLoopback->read(getData.bytes))
        {
            if(getData.data.part == TOPLEVEL::section::midiLearn) // special midi-learn - needs improving
                synth->midilearn.generalOpps(getData.data.value, getData.data.type, getData.data.control, getData.data.part, getData.data.kit, getData.data.engine, getData.data.insert, getData.data.parameter, getData.data.par2);
            else if ((getData.data.parameter >= TOPLEVEL::route::lowPriority) && getData.data.parameter < UNUSED)
                indirectTransfers(&getData);
            else
                resolveReplies(&getData);
        }
        usleep(80); // actually gives around 120 uS

        /*
         * The following are low priority actions initiated by,
         * but isolated from the main audio thread.
         */

        unsigned int flag = flagsReadClear();
        if (flag < 0xffffffff)
            mutedDecode(flag);
    }
    return NULL;
}


InterChange::~InterChange()
{
    if (sortResultsThreadHandle)
        pthread_join(sortResultsThreadHandle, NULL);
#ifndef YOSHIMI_LV2_PLUGIN
    if (fromCLI)
    {
        delete(fromCLI);
        fromCLI = NULL;
    }
#endif
    if (decodeLoopback)
    {
        delete(decodeLoopback);
        decodeLoopback = NULL;
    }
#ifdef GUI_FLTK
    if (fromGUI)
    {
        delete(fromGUI);
        fromGUI = NULL;
    }
    if (toGUI)
    {
        delete(toGUI);
        toGUI = NULL;
    }
#endif
    if (fromMIDI)
    {
        delete(fromMIDI);
        fromMIDI = NULL;
    }
}


void InterChange::indirectTransfers(CommandBlock *getData)
{
    int value = lrint(getData->data.value);
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char parameter = getData->data.parameter;
    unsigned char par2 = getData->data.par2;
    //std::cout << "Indirect" << std::endl;
    bool write = (type & TOPLEVEL::type::Write);
    if (write)
        __sync_or_and_fetch(&blockRead, 2);
    bool guiTo = false;
    guiTo = guiTo; // suppress warning when headless build
    std::string text;
    if (getData->data.par2 != NO_MSG)
        text = miscMsgPop(getData->data.par2);
    else
        text = "";
    getData->data.par2 = NO_MSG; // this may be reset later
    unsigned int tmp;
    std::string name;

    int switchNum = npart;
    if (control == TOPLEVEL::control::errorMessage && insert != TOPLEVEL::insert::resonanceGraphInsert)
        switchNum = 256; // this is a bit hacky :(

    switch(switchNum)
    {
        case TOPLEVEL::section::vector:
        {
            switch(control)
            {
                case VECTOR::control::name:
                    if (write)
                    {
                        synth->getRuntime().vectordata.Name[insert] = text;
                    }
                    else
                        text = synth->getRuntime().vectordata.Name[insert];
                    value = miscMsgPush(text);
                    getData->data.parameter -= TOPLEVEL::route::lowPriority;
                    guiTo = true;
                    break;
            }
            break;
        }
        case TOPLEVEL::section::midiIn: // program / bank / root
        {
            //std::cout << " interchange prog " << value << "  chan " << int(kititem) << "  bank " << int(engine) << "  root " << int(insert) << "  named " << int(par2) << std::endl;
            if (par2 != NO_MSG) // was named file not numbered
                getData->data.par2 = miscMsgPush(text);

            int msgID = synth->SetRBP(getData);
            if (msgID > NO_MSG)
                text = "FAILED ";
            else
                text = "";
            text += miscMsgPop(msgID & NO_MSG);
            value = miscMsgPush(text);
            synth->getRuntime().finishedCLI = true; // temp
            getData->data.parameter -= TOPLEVEL::route::lowPriority;
            guiTo = true;
            break;
        }
        case TOPLEVEL::section::scales:
        {
            switch(control)
            {
                case SCALES::control::tuning:
                    text = formatScales(text);
                    value = synth->microtonal.texttotunings(text.c_str());
                    if (value > 0)
                        synth->setAllPartMaps();
                    break;
                case SCALES::control::keyboardMap:
                    text = formatScales(text);
                    value = synth->microtonal.texttomapping(text.c_str());
                    if (value > 0)
                        synth->setAllPartMaps();
                    break;

                case SCALES::control::importScl:
                    value = synth->microtonal.loadscl(setExtension(text,EXTEN::scalaTuning));
                    if(value > 0)
                    {
                        text = "";
                        char *buf = new char[100];
                        for (int i = 0; i < value; ++ i)
                        {
                            synth->microtonal.tuningtoline(i, buf, 100);
                            if (i > 0)
                                text += "\n";
                            text += std::string(buf);
                        }
                        delete [] buf;
                    }
                    break;
                case SCALES::control::importKbm:
                    value = synth->microtonal.loadkbm(setExtension(text,EXTEN::scalaKeymap));
                    if(value > 0)
                    {
                        text = "";
                        int map;
                        for (int i = 0; i < value; ++ i)
                        {
                            if (i > 0)
                                text += "\n";
                            map = synth->microtonal.Pmapping[i];
                            if (map == -1)
                                text += 'x';
                            else
                                text += std::to_string(map);
                        }
                        getData->data.kit = synth->microtonal.PAnote;
                        getData->data.engine = synth->microtonal.Pfirstkey;
                        getData->data.insert = synth->microtonal.Pmiddlenote;
                        getData->data.parameter |= synth->microtonal.Plastkey; // need to keep top bit
                        synth->setAllPartMaps();
                    }
                    break;

                case SCALES::control::name:
                    synth->microtonal.Pname = text;
                    break;
                case SCALES::control::comment:
                    synth->microtonal.Pcomment = text;
                    break;
            }
            getData->data.parameter -= TOPLEVEL::route::lowPriority;
            guiTo = true;
            break;
        }
        case TOPLEVEL::section::main:
        {
            switch (control)
            {
                case MAIN::control::detune:
                {
                    if (write)
                    {
                        synth->microtonal.Pglobalfinedetune = value;
                        synth->setAllPartMaps();
                    }
                    else
                        value = synth->microtonal.Pglobalfinedetune;
                    break;
                }
                case MAIN::control::keyShift:
                {
                    if (write)
                    {
                        synth->setPkeyshift(value + 64);
                        synth->setAllPartMaps();
                    }
                    else
                        value = synth->Pkeyshift - 64;
                    break;
                }

                case MAIN::control::exportBank:
                {
                    unsigned int result = synth->bank.exportBank(text, kititem, value);
                    text = miscMsgPop(result & 0xff);
                    if (result < 0x1000)
                        text = " " + text; // need the space
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                }

                case MAIN::control::importBank:
                {
                    unsigned int result = synth->bank.importBank(text, kititem, value);
                    text = miscMsgPop(result & 0xff);
                    if (result < 0x1000)
                        text = "ed " + text;
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                }
                case MAIN::control::deleteBank:
                {
                    unsigned int result = synth->bank.removebank(value, kititem);
                    text = miscMsgPop(result & 0xff);
                    if (result < 0x1000)
                        text = "d " + text;
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                }
                case MAIN::control::saveInstrument:
                {
                    if (kititem == UNUSED)
                    {
                        kititem = synth->ReadBankRoot();
                        getData->data.kit = kititem;
                    }

                    if (engine == UNUSED)
                    {
                        engine = synth->ReadBank();
                        getData->data.engine = engine;
                    }
                    if (value >= 64)
                    {
                        value = synth->getRuntime().currentPart;
                    }
                    //std::cout << "\n\nRoot " << int(kititem) << "  Bank " << int(engine) << "  Part " << int(value) << "  Slot " << int(insert) << "  Par2 " << int(par2) << " \n\n" << std::endl;
                    text = synth->part[value]->Pname + " to " + std::to_string(int(insert));
                    if (synth->getBankRef().savetoslot(kititem, engine, insert, value))
                    {
                        text = "d " + text;
                        synth->part[value]->PyoshiType = (synth->getRuntime().instrumentFormat > 1);
                    }
                    else
                        text = " FAILED " + text;
                    getData->data.parameter |= value; // retain lowPriority, will be detected later
                    value = miscMsgPush(text);
                    break;
                }
                case MAIN::control::saveNamedInstrument:
                {
                    bool ok = true;
                    int saveType = synth->getRuntime().instrumentFormat;

                    if (saveType & 2) // Yoshimi format
                        ok = synth->part[value]->saveXML(text, true);
                    if (ok && (saveType & 1)) // legacy
                        ok = synth->part[value]->saveXML(text, false);

                    if (ok)
                    {
                        synth->addHistory(setExtension(text, EXTEN::zynInst), TOPLEVEL::historyList::Instrument);
                        synth->part[value]->PyoshiType = (saveType & 2);
                        text = "d " + text;
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                }
                case MAIN::control::loadNamedPatchset:
                    vectorClear(NUM_MIDI_CHANNELS);
                    if(synth->loadPatchSetAndUpdate(text))
                    {
                        synth->addHistory(setExtension(text, EXTEN::patchset), TOPLEVEL::historyList::Patch);
                        text = "ed " + text;
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case MAIN::control::saveNamedPatchset:
                    if(synth->savePatchesXML(text))
                    {
                        synth->addHistory(setExtension(text, EXTEN::patchset), TOPLEVEL::historyList::Patch);
                        text = "d " + text;
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case MAIN::control::loadNamedVector:
                    tmp = synth->loadVectorAndUpdate(insert, text);
                    if (tmp < NO_MSG)
                    {
                        getData->data.insert = tmp;
                        synth->addHistory(setExtension(text, EXTEN::vector), TOPLEVEL::historyList::Vector);
                        text = "ed " + text + " to chan " + std::to_string(int(tmp + 1));
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case MAIN::control::saveNamedVector:
                {
                    std::string oldname = synth->getRuntime().vectordata.Name[insert];
                    int pos = oldname.find("No Name");
                    if (pos >=0 && pos < 2)
                        synth->getRuntime().vectordata.Name[insert] = findleafname(text);
                    tmp = synth->saveVector(insert, text, true);
                    if (tmp == NO_MSG)
                    {
                        synth->addHistory(setExtension(text, EXTEN::vector), TOPLEVEL::historyList::Vector);
                        text = "d " + text;
                    }
                    else
                    {
                        name = miscMsgPop(tmp);
                        if (name != "FAIL")
                            text = " " + name;
                        else
                            text = " FAILED " + text;
                    }
                    value = miscMsgPush(text);
                    break;
                }
                case MAIN::control::loadNamedScale:
                    if (synth->loadMicrotonal(text))
                    {
                        synth->addHistory(setExtension(text, EXTEN::scale), TOPLEVEL::historyList::Scale);
                        text = "ed " + text;
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case MAIN::control::saveNamedScale:
                    if (synth->saveMicrotonal(text))
                    {
                        synth->addHistory(setExtension(text, EXTEN::scale), TOPLEVEL::historyList::Scale);
                        text = "d " + text;
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case MAIN::control::loadNamedState:
                    vectorClear(NUM_MIDI_CHANNELS);
                    if (synth->loadStateAndUpdate(text))
                    {
                        string name = synth->getRuntime().ConfigDir + "/yoshimi";
                        if (synth != firstSynth)
                            name += ("-" + to_string(synth->getUniqueId()));
                        name += ".state";
                        if ((text != name)) // never include default state
                            synth->addHistory(text, TOPLEVEL::historyList::State);
                        text = "ed " + text;
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case MAIN::control::saveNamedState:
                {
                    string filename = setExtension(text, EXTEN::state);
                    if (synth->saveState(filename))
                    {
                        string name = synth->getRuntime().ConfigDir + "/yoshimi";
                        if (synth != firstSynth)
                            name += ("-" + to_string(synth->getUniqueId()));
                        name += ".state";
                        if ((text != name)) // never include default state
                            synth->addHistory(filename, TOPLEVEL::historyList::State);
                        text = "d " + text;
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                }
                case MAIN::control::exportPadSynthSamples:
                {
                    unsigned char partnum = insert;
                    synth->partonoffWrite(partnum, -1);
                    setpadparams(partnum, kititem);
                    if (synth->part[partnum]->kit[kititem].padpars->export2wav(text))
                        text = "d " + text;
                    else
                        text = " FAILED some samples " + text;
                    value = miscMsgPush(text);
                    break;
                }
                case MAIN::control::masterReset:
                    synth->resetAll(0);
                    break;
                case MAIN::control::masterResetAndMlearn:
                    synth->resetAll(1);
                    break;
                case MAIN::control::openManualPDF: // display user guide
                {
                    std::string manfile = synth->manualname();
                    unsigned int pos = manfile.rfind(".") + 1;
                    int wanted = std::stoi(manfile.substr(pos, 3));
                    int count = wanted + 1;
                    manfile = manfile.substr(0, pos);
                    std::string path = "";
                    while (path == "" && count >= 0) // scan current then older varsions
                    {
                        --count;
                        path = findfile("/usr/", (manfile + std::to_string(count)).c_str(), "pdf");
                        if (path == "")
                        path = findfile("/usr/", (manfile + std::to_string(count)).c_str(), "pdf.gz");
                        if (path == "")
                        path = findfile("/home/", (manfile + std::to_string(count)).c_str(), "pdf");
                    }

                    if (path == "")
                        text = "Can't find manual :(";
                    else if (count < wanted)
                        text = "Can't find current manual. Using older one";
                    if (path != "")
                    {
                        std::string command = "xdg-open " + path + "&";
                        FILE *fp = popen(command.c_str(), "r");
                        if (fp == NULL)
                            text = "Can't find PDF reader :(";
                        pclose(fp);
                    }
                    value = miscMsgPush(text);
                    break;
                }
                case MAIN::control::startInstance:
                    if (synth == firstSynth)
                    {
                        if (value > 0 && value < 32)
                            startInstance = value | 0x80;
                        else
                            startInstance = 0x81; // next available
                        while (startInstance > 0x80)
                            usleep(1000);
                        value = startInstance; // actual instance found
                        startInstance = 0; // just to be sure
                    }
                    break;
                case MAIN::control::stopInstance:
                    text = std::to_string(value) + " ";
                    if (value < 0 || value >= 32)
                        text += "Out of range";
                    else
                    {
                        SynthEngine *toClose = firstSynth->getSynthFromId(value);
                        if (toClose == firstSynth && value > 0)
                            text += "Can't find";
                        else
                        {
                            toClose->getRuntime().runSynth = false;
                            text += "Closed";
                        }
                    }
                    value = miscMsgPush(text);
                    break;

                case MAIN::control::stopSound:
#ifdef REPORT_NOTES_ON_OFF
                    // note test
                    synth->getRuntime().Log("note on sent " + std::to_string(synth->getRuntime().noteOnSent));
                    synth->getRuntime().Log("note on seen " + std::to_string(synth->getRuntime().noteOnSeen));
                    synth->getRuntime().Log("note off sent " + std::to_string(synth->getRuntime().noteOffSent));
                    synth->getRuntime().Log("note off seen " + std::to_string(synth->getRuntime().noteOffSeen));
                    synth->getRuntime().Log("notes hanging sent " + std::to_string(synth->getRuntime().noteOnSent - synth->getRuntime().noteOffSent));
                    synth->getRuntime().Log("notes hanging seen " + std::to_string(synth->getRuntime().noteOnSeen - synth->getRuntime().noteOffSeen));
#endif
                    synth->ShutUp();
                    synth->Unmute();
                    break;
            }
            getData->data.parameter -= TOPLEVEL::route::lowPriority;
            if (control != MAIN::control::startInstance && control != MAIN::control::stopInstance)
                guiTo = true;
            break;
        }
        case TOPLEVEL::section::bank: // instrument / bank
        {
            switch (control)
            {
                case BANK::control::selectFirstInstrumentToSwap:
                {
                    if(kititem == UNUSED)
                    {
                        kititem = synth->getRuntime().currentBank;
                        getData->data.kit = kititem;
                    }
                    if(engine == UNUSED)
                    {
                        engine = synth->getRuntime().currentRoot;
                        getData->data.engine = engine;
                    }
                    //std::cout << "Int swap 1 I " << int(value)  << "  B " << int(kititem) << "  R " << int(engine) << std::endl;
                    swapInstrument1 = insert;
                    swapBank1 = kititem;
                    swapRoot1 = engine;
                    break;
                }
                case BANK::control::selectSecondInstrumentAndSwap:
                {
                    if(kititem == UNUSED)
                    {
                        kititem = synth->getRuntime().currentBank;
                        getData->data.kit = kititem;
                    }
                    if(engine == UNUSED)
                    {
                        engine = synth->getRuntime().currentRoot;
                        getData->data.engine = engine;
                    }
                    //std::cout << "Int swap 2 I " << int(insert) << "  B " << int(kititem) << "  R " << int(engine) << std::endl;
                    tmp = synth->bank.swapslot(swapInstrument1, insert, swapBank1, kititem, swapRoot1, engine);
                    if (tmp != 0)
                    {
                        text = " FAILED " + miscMsgPop(tmp & 0xfff);
                        value = miscMsgPush(text);
                        if (text.find("nothing", 0, 7) == string::npos)
                            synth->bank.rescanforbanks(); // might have corrupted it
                    }
                    swapInstrument1 = UNUSED;
                    swapBank1 = UNUSED;
                    swapRoot1 = UNUSED;
                    guiTo = true;
                    break;
                }

                case BANK::control::selectFirstBankToSwap:
                    if(engine == UNUSED)
                    {
                        engine = synth->getRuntime().currentRoot;
                        getData->data.engine = engine;
                    }
                    swapBank1 = kititem;
                    swapRoot1 = engine;
                    break;
                case BANK::control::selectSecondBankAndSwap:
                    if(engine == UNUSED)
                    {
                        engine = synth->getRuntime().currentRoot;
                        getData->data.engine = engine;
                    }
                    tmp = synth->bank.swapbanks(swapBank1, kititem, swapRoot1, engine);
                    if (tmp >= 0x1000)
                    {
                        text = " FAILED " + miscMsgPop(tmp & 0xfff);
                        value = miscMsgPush(text);
                        if (text.find("nothing", 0, 7) == string::npos)
                            synth->bank.rescanforbanks(); // might have corrupted it
                    }
                    swapBank1 = UNUSED;
                    swapRoot1 = UNUSED;
                    guiTo = true;
                    break;
            }
            getData->data.parameter -= TOPLEVEL::route::lowPriority;
            break;
        }
        case TOPLEVEL::section::config:
        {
            switch (control)
            {
                case CONFIG::control::jackMidiSource:
                    if (write)
                    {
                        synth->getRuntime().jackMidiDevice = text;
                        synth->getRuntime().configChanged = true;
                    }
                    else
                        text = synth->getRuntime().jackMidiDevice;
                    value = miscMsgPush(text);
                    break;
                case CONFIG::control::jackServer:
                    if (write)
                    {
                        synth->getRuntime().jackServer = text;
                        synth->getRuntime().configChanged = true;
                    }
                    else
                        text = synth->getRuntime().jackServer;
                    value = miscMsgPush(text);
                    break;
                case CONFIG::control::alsaMidiSource:
                    if (write)
                    {
                        synth->getRuntime().alsaMidiDevice = text;
                        synth->getRuntime().configChanged = true;
                    }
                    else
                        text = synth->getRuntime().alsaMidiDevice;
                    value = miscMsgPush(text);
                    break;
                case CONFIG::control::alsaAudioDevice:
                    if (write)
                    {
                        synth->getRuntime().alsaAudioDevice = text;
                        synth->getRuntime().configChanged = true;
                    }
                    else
                        text = synth->getRuntime().alsaAudioDevice;
                    value = miscMsgPush(text);
                    break;
                case CONFIG::control::saveCurrentConfig:
                    if (write)
                    {
                        text = synth->getRuntime().ConfigFile;
                        if(synth->getRuntime().saveConfig())
                            text = "d " + text;
                        else
                            text = " FAILED " + text;
                    }
                    else
                        text = "READ";
                    value = miscMsgPush(text);
                    getData->data.par2 = miscMsgPush(text); // slightly odd case
                    break;
            }
#ifdef GUI_FLTK
            if (!(type & TOPLEVEL::source::GUI))
                guiTo = true;
#endif
            getData->data.parameter -= TOPLEVEL::route::lowPriority;
            break;
        }
        case 256:
        {
            value = miscMsgPush(text);
            getData->data.parameter -= TOPLEVEL::route::lowPriority;
            break;
        }
        default:
        {
            if (npart < NUM_MIDI_PARTS)
            {
                switch(control)
                {
                    case PART::control::keyShift:
                    {
                        if (write)
                        {
                            synth->part[npart]->Pkeyshift = value + 64;
                            synth->setPartMap(npart);
                        }
                        else
                            value = synth->part[npart]->Pkeyshift - 64;
                        getData->data.parameter -= TOPLEVEL::route::lowPriority;
                    }
                    break;

                    case PART::control::defaultInstrument: // clear part
                        if (write)
                        {
                            doClearPart(npart);
                            getData->data.parameter -= TOPLEVEL::route::lowPriority;
                        }
                        break;

                    case PART::control::padsynthParameters:
                        if (write)
                        {
                            setpadparams(npart, kititem);
                            getData->data.parameter -= TOPLEVEL::route::lowPriority;
                        }
                        else
                            value = synth->part[npart]->kit[kititem].padpars->Papplied;
                        break;

                    case PART::control::audioDestination:
                        if (npart < synth->getRuntime().NumAvailableParts)
                        {
                            if (value & 2)
                            {
                                mainRegisterAudioPort(synth, npart);
                            }
                            getData->data.parameter -= TOPLEVEL::route::lowPriority;
                        }
                        break;
                    case PART::control::instrumentName: // part or kit item names
                        if (kititem == UNUSED)
                        {
                            if (write)
                            {
                                synth->part[npart]->Pname = text;
                                guiTo = true;
                            }
                            else
                            {
                                text = synth->part[npart]->Pname;
                            }
                        }
                        else if (synth->part[npart]->Pkitmode)
                        {
                            if (kititem >= NUM_KIT_ITEMS)
                                text = " FAILED out of range";
                            else
                            {
                                if (write)
                                {
                                    synth->part[npart]->kit[kititem].Pname = text;
                                    guiTo = true;
                                }
                                else
                                {
                                    text = synth->part[npart]->kit[kititem].Pname;
                                }
                            }
                        }
                        else
                            text = " FAILED Not in kit mode";
                        getData->data.parameter -= TOPLEVEL::route::lowPriority;
                        value = miscMsgPush(text);
                        break;
                    case PART::control::defaultInstrumentCopyright:
                        if (write)
                        {
                            std::string name = synth->getRuntime().ConfigDir + "/copyright.txt";
                            if ((parameter & 0x7f) == 0) // load
                            {
                                text = loadText(name); // TODO provide failure warning
                                synth->part[npart]->info.Pauthor = text;
                                guiTo = true;
                            }
                            else
                            {
                                text = synth->part[npart]->info.Pauthor;
                                saveText(text, name);
                            }
                            getData->data.parameter -= TOPLEVEL::route::lowPriority;
                            value = miscMsgPush(text);
                        }
                        break;
                }
            }
            break;
        }
    }
    __sync_and_and_fetch(&blockRead, 0xfd);
    if (getData->data.parameter < TOPLEVEL::route::lowPriority)
    {
        getData->data.value = float(value);
#ifdef GUI_FLTK
        if (synth->getRuntime().showGui && write && guiTo)
            getData->data.par2 = miscMsgPush(text); // pass it on to GUI
#endif
        bool ok = returnsBuffer->write(getData->bytes);
#ifdef GUI_FLTK
        if (synth->getRuntime().showGui && npart == TOPLEVEL::section::scales && control == SCALES::control::importScl)
        {   // loading a tuning includes a name and comment!
            getData->data.control = SCALES::control::name;
            getData->data.par2 = miscMsgPush(synth->microtonal.Pname);
            returnsBuffer->write(getData->bytes);
            getData->data.control = SCALES::control::comment;
            getData->data.par2 = miscMsgPush(synth->microtonal.Pcomment);
            ok &= returnsBuffer->write(getData->bytes);
        }
#endif
        if (!ok)
            synth->getRuntime().Log("Unable to  write to returnsBuffer buffer");
    }
}


std::string InterChange::formatScales(std::string text)
{
    text.erase(remove(text.begin(), text.end(), ' '), text.end());
    std::string delimiters = ",";
    size_t current;
    size_t next = -1;
    size_t found;
    std::string word;
    std::string newtext = "";
    do
    {
        current = next + 1;
        next = text.find_first_of( delimiters, current );
        word = text.substr( current, next - current );

        found = word.find('.');
        if (found != string::npos)
        {
            if (found < 4)
            {
                std::string tmp (4 - found, '0'); // leading zeros
                word = tmp + word;
            }
            found = word.size();
            if ( found < 11)
            {
                std::string tmp  (11 - found, '0'); // trailing zeros
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


float InterChange::readAllData(CommandBlock *getData)
{
    if(getData->data.type & TOPLEVEL::type::Limits) // these are static
    {
        //std::cout << "Read Control " << (int) getData->data.control << " Part " << (int) getData->data.part << "  Kit " << (int) getData->data.kit << " Engine " << (int) getData->data.engine << "  Insert " << (int) getData->data.insert << std::endl;
        /*
         * commandtype limits values
         * 0    adjusted input value
         * 1    min
         * 2    max
         * 3    default
         *
         * tryData.data.type will be updated:
         * bit 6 set    MIDI-learnable
         * bit 7 set    Is an integer value
         */
        getData->data.type -= TOPLEVEL::type::Limits;
        float value = returnLimits(getData);
        synth->getRuntime().finishedCLI = true;
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
    CommandBlock tryData;
    unsigned char control = getData->data.control;
    if (getData->data.part == TOPLEVEL::section::main && (control >= MAIN::control::readPartPeak && control <= MAIN::control::readMainLRrms))
    {
        commandSendReal(getData);
        synth->fetchMeterData();
        return getData->data.value;
    }
    //std::cout << "Read Control " << (int) getData->data.control << " Type " << (int) getData->data.type << " Part " << (int) getData->data.part << "  Kit " << (int) getData->data.kit << " Engine " << (int) getData->data.engine << "  Insert " << (int) getData->data.insert << " Parameter " << (int) getData->data.parameter << " Par2 " << (int) getData->data.par2 << std::endl;
    int npart = getData->data.part;
    bool indirect = ((getData->data.parameter & 0xc0) == TOPLEVEL::route::lowPriority);
    if (npart < NUM_MIDI_PARTS && synth->part[npart]->busy)
    {
        getData->data.control = PART::control::partBusy; // part busy message
        getData->data.kit = UNUSED;
        getData->data.engine = UNUSED;
        getData->data.insert = UNUSED;
    }
    reTry:
    memcpy(tryData.bytes, getData->bytes, sizeof(tryData));
    // a false positive here is not actually a problem.
    while (blockRead) // just reading it
        usleep(10);
    if (indirect)
    {
        /*
         * This still isn't quite right there is a very
         * remote chance of getting garbled text :(
         */
        indirectTransfers(&tryData);
        synth->getRuntime().finishedCLI = true;
        return tryData.data.value;
    }
    else
        commandSendReal(&tryData);
    if (blockRead)//__sync_or_and_fetch(&blockRead, 0) > 0)
        goto reTry; // it may have changed mid-process

    if ((tryData.data.type & TOPLEVEL::source::CLI))
        resolveReplies(&tryData);


    synth->getRuntime().finishedCLI = true; // in case it misses lines above
    return tryData.data.value;
}


void InterChange::resolveReplies(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    if (getData->data.source == TOPLEVEL::action::noAction)
        return; // no further action
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char insertParam = getData->data.parameter;
    unsigned char insertPar2 = getData->data.par2;
    if (control == TOPLEVEL::control::errorMessage && insertParam != TOPLEVEL::insert::resonanceGraphInsert) // special case for simple messages
    {
        synth->getRuntime().Log(miscMsgPop(lrint(value)));
        synth->getRuntime().finishedCLI = true;
        return;
    }

    showValue = true;

    Part *part;
    part = synth->part[npart];

    // this is unique and placed here to avoid Xruns
    if (npart == TOPLEVEL::section::scales && (control <= SCALES::control::tuning || control >= SCALES::control::retune))
        synth->setAllPartMaps();

    bool isCli = ((type & (TOPLEVEL::source::CLI | TOPLEVEL::source::GUI )) == TOPLEVEL::source::CLI); // eliminate Gui redraw
    bool isGui = type & TOPLEVEL::source::GUI;
    //std::cout << "Is CLI " << isCli << std::endl;
    //std::cout << "Is GUI " << isGui << std::endl;
    char button = type & 3;
    std::string isValue;
    std::string commandName;

#ifdef ENABLE_REPORTS
    if ((isGui && !(button & 1)) || (isCli && button == 1))
#else
    if (isCli && button == 1)
#endif
    {
        if (button == 0)
            isValue = "\n  Request set default";
        else
        {
            isValue = "\n  Value      " + std::to_string(value);
            if (!(type & TOPLEVEL::type::Integer))
                isValue += "f";
        }
        std::string typemsg = "  Type       ";
        for (int i = 7; i > -1; -- i)
            typemsg += std::to_string((type >> i) & 1);
        list<std::string>msg;
        msg.push_back(isValue);
        msg.push_back(typemsg);
        msg.push_back("  Control    0x" + asHexString(control) + "    " + asString(int(control)));
        msg.push_back("  Part       0x" + asHexString(npart) + "    " + asString(int(npart)));
        msg.push_back("  Kit        0x" + asHexString(kititem) + "    " + asString(int(kititem)));
        msg.push_back("  Engine     0x" + asHexString(engine) + "    " + asString(int(engine)));
        msg.push_back("  Insert     0x" + asHexString(insert) + "    " + asString(int(insert)));
        msg.push_back("  Parameter  0x" + asHexString(insertParam) + "    " + asString(int(insertParam)));
        msg.push_back("  2nd Param  0x" + asHexString(insertPar2) + "    " + asString(int(insertPar2)));
        synth->cliOutput(msg, 10);
        if (isCli)
        {
            synth->getRuntime().finishedCLI = true;
            return; // wanted for test only
        }

    }
    if (npart == TOPLEVEL::section::vector)
        commandName = resolveVector(getData);
    else if (npart == TOPLEVEL::section::scales)
        commandName = resolveMicrotonal(getData);
    else if (npart == TOPLEVEL::section::config)
        commandName = resolveConfig(getData);
    else if (npart == TOPLEVEL::section::bank)
        commandName = resolveBank(getData);
    else if (npart == TOPLEVEL::section::midiIn || npart == TOPLEVEL::section::main)
        commandName = resolveMain(getData);

    else if (npart == TOPLEVEL::section::systemEffects || npart == TOPLEVEL::section::insertEffects)
        commandName = resolveEffects(getData);

    else if ((kititem >= EFFECT::type::none && kititem <= EFFECT::type::dynFilter) || (control >= PART::control::effectNumber && control <= PART::control::effectBypass && kititem == UNUSED))
        commandName = resolveEffects(getData);

    else if (npart >= NUM_MIDI_PARTS)
    {
        showValue = false;
        commandName = "Invalid part " + std::to_string(int(npart) + 1);
    }

    else if (kititem >= NUM_KIT_ITEMS && kititem < UNUSED)
    {
        showValue = false;
        commandName = "Invalid kit " + std::to_string(int(kititem) + 1);
    }

    else if (kititem != 0 && engine != UNUSED && control != PART::control::enable && part->kit[kititem].Penabled == false)
        commandName = "Part " + std::to_string(int(npart) + 1) + " Kit item " + std::to_string(int(kititem) + 1) + " not enabled";

    else if (kititem == UNUSED || insert == TOPLEVEL::insert::kitGroup)
    {
        if (control != PART::control::kitMode && kititem != UNUSED && part->Pkitmode == 0)
        {
            showValue = false;
            commandName = "Part " + std::to_string(int(npart) + 1) + " Kitmode not enabled";
        }
        else
            commandName = resolvePart(getData);
    }
    else if (kititem > 0 && part->Pkitmode == 0)
    {
        showValue = false;
        commandName = "Part " + std::to_string(int(npart) + 1) + " Kitmode not enabled";
    }

    else if (engine == PART::engine::padSynth)
    {
        switch(insert)
        {
            case UNUSED:
                commandName = resolvePad(getData);
                break;
            case TOPLEVEL::insert::LFOgroup:
                commandName = resolveLFO(getData);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandName = resolveFilter(getData);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePoints:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::oscillatorGroup:
                commandName = resolveOscillator(getData);
                break;
            case TOPLEVEL::insert::harmonicAmplitude:
                commandName = resolveOscillator(getData);
                break;
            case TOPLEVEL::insert::harmonicPhaseBandwidth:
                commandName = resolveOscillator(getData);
                break;
            case TOPLEVEL::insert::resonanceGroup:
                commandName = resolveResonance(getData);
                break;
            case TOPLEVEL::insert::resonanceGraphInsert:
                commandName = resolveResonance(getData);
                break;
        }
    }

    else if (engine == PART::engine::subSynth)
    {
        switch (insert)
        {
            case UNUSED:
                commandName = resolveSub(getData);
                break;
            case TOPLEVEL::insert::harmonicAmplitude:
                commandName = resolveSub(getData);
                break;
            case TOPLEVEL::insert::harmonicPhaseBandwidth:
                commandName = resolveSub(getData);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandName = resolveFilter(getData);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePoints:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandName = resolveEnvelope(getData);
                break;
        }
    }

    else if (engine >= PART::engine::addVoice1)
    {
        switch (insert)
        {
            case UNUSED:
                commandName = resolveAddVoice(getData);
                break;
            case TOPLEVEL::insert::LFOgroup:
                commandName = resolveLFO(getData);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandName = resolveFilter(getData);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePoints:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::oscillatorGroup:
                commandName = resolveOscillator(getData);
                break;
            case TOPLEVEL::insert::harmonicAmplitude:
                commandName = resolveOscillator(getData);
                break;
            case TOPLEVEL::insert::harmonicPhaseBandwidth:
                commandName = resolveOscillator(getData);
                break;
        }
    }

    else if (engine == PART::engine::addSynth)
    {
        switch (insert)
        {
            case UNUSED:
                commandName = resolveAdd(getData);
                break;
            case TOPLEVEL::insert::LFOgroup:
                commandName = resolveLFO(getData);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandName = resolveFilter(getData);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePoints:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandName = resolveEnvelope(getData);
                break;
            case TOPLEVEL::insert::resonanceGroup:
                commandName = resolveResonance(getData);
                break;
            case TOPLEVEL::insert::resonanceGraphInsert:
                commandName = resolveResonance(getData);
                break;
        }
    }

    std::string actual = "";
    if (showValue)
    {
        actual = " Value ";
        if (type & TOPLEVEL::type::Integer)
            actual += std::to_string(lrint(value));
        else
            actual += std::to_string(value);
    }
    if ((isGui || isCli) && button == 3)
    {
        std::string toSend;
        size_t pos = commandName.find(" - ");
        if (pos < 1 || pos >= commandName.length())
            toSend = commandName;
        else
            toSend = commandName.substr(0, pos);
        synth->midilearn.setTransferBlock(getData, toSend);
        return;
    }

    if (value == FLT_MAX)
    { // This corrupts par2 but it shouldn't matter if used as intended
        getData->data.par2 = miscMsgPush(commandName);
        return;
    }

    else if (isGui || isCli) // not midi !!!
        synth->getRuntime().Log(commandName + actual);
// in case it was called from CLI
    synth->getRuntime().finishedCLI = true;
}


std::string InterChange::resolveVector(CommandBlock *getData)
{
    int value_int = lrint(getData->data.value);
    unsigned char control = getData->data.control;
    unsigned int chan = getData->data.insert;

    std::string contstr = "";
    switch (control)
    {
        //case 0:
            //contstr = "Base Channel"; // local to source
            //break;
        //case 1:
            //contstr = "Options";
            //break;
        case VECTOR::control::name:
            showValue = false;
            contstr = "Name " + miscMsgPop(value_int);
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
            contstr = "Feature 0";
            break;
        case VECTOR::control::Xfeature1:
        case VECTOR::control::Yfeature1:
            contstr = "Feature 1";
            break;
        case VECTOR::control::Xfeature2:
        case VECTOR::control::Yfeature2:
            contstr = "Feature 2 ";
            break;
        case VECTOR::control::Xfeature3:
        case VECTOR::control::Yfeature3:
            contstr = "Feature 3";
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
                contstr = "chan " + std::to_string(chan + 1);
            return("Vector cleared on " + contstr);
            break;

        case 127:
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    if (control == VECTOR::control::undefined)
    {
        showValue = false;
        return("Vector " + contstr + " set to " + std::to_string(chan + 1));
    }
    std::string name = "Vector Chan " + std::to_string(chan + 1) + " ";
    if (control == 127)
        name += " all ";
    else if (control >= VECTOR::control::Ycontroller)
        name += "Y ";
    else if(control >= VECTOR::control::Xcontroller)
        name += "X ";

    return (name + contstr);
}


std::string InterChange::resolveMicrotonal(CommandBlock *getData)
{
    int value = getData->data.value;
    unsigned char control = getData->data.control;

    std::string contstr = "";
    switch (control)
    {
        case SCALES::control::Afrequency:
            contstr = "'A' Frequency";
            break;
        case SCALES::control::Anote:
            contstr = "'A' Note";
            break;
        case SCALES::control::invertScale:
            contstr = "Invert Keys";
            break;
        case SCALES::control::invertedScaleCenter:
            contstr = "Key Center";
            break;
        case SCALES::control::scaleShift:
            contstr = "Scale Shift";
            break;
        case SCALES::control::enableMicrotonal:
            contstr = "Enable Microtonal";
            break;

        case SCALES::control::enableKeyboardMap:
            contstr = "Enable Keyboard Mapping";
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
        case SCALES::control::importScl:
            contstr = "Tuning Import ";
            showValue = false;
            break;
        case SCALES::control::importKbm:
            contstr = "Keymap Import ";
            showValue = false;
            break;

        case SCALES::control::name:
            contstr = "Name: " + std::string(synth->microtonal.Pname);
            showValue = false;
            break;
        case SCALES::control::comment:
            contstr = "Description: " + std::string(synth->microtonal.Pcomment);
            showValue = false;
            break;
        case SCALES::control::retune:
            contstr = "Retune";
            showValue = false;
            break;

        case SCALES::control::clearAll:
            contstr = "Clear all settings";
            showValue = false;
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";

    }

    if (value < 1 && control >= SCALES::control::tuning && control <= SCALES::control::importKbm)
    { // errors :@(
        switch (value)
        {
            case 0:
                contstr += "Empty entry";
                break;
            case -1:
                contstr += "Value too small";
                break;
            case -2:
                contstr += "Invalid entry";
                break;
            case -3:
                contstr += "File not found";
                break;
            case -4:
                contstr += "Empty file";
                break;
            case -5:
                contstr += "Short or corrupted file";
                break;
            case -6:
                if (control == SCALES::control::tuning || control == SCALES::control::importScl)
                    contstr += "Invalid octave size";
                else
                    contstr += "Invalid keymap size";
                break;
            case -7:
                contstr += "Invalid note number";
                break;
            case -8:
                contstr += "Out of range";
                break;
        }
    }
    return ("Scales " + contstr);
}

std::string InterChange::resolveConfig(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char control = getData->data.control;
    bool write = getData->data.type & TOPLEVEL::type::Write;
    int value_int = lrint(value);
    bool value_bool = YOSH::F2B(value);
    bool yesno = false;
    std::string contstr = "";
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
            if (value_bool)
                contstr += "cubic";
            else
                contstr += "linear";
            showValue = false;
            break;
        case CONFIG::control::virtualKeyboardLayout:
            contstr = "Virtual keyboard ";
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
            showValue = false;
            break;
        case CONFIG::control::XMLcompressionLevel:
            contstr = "XML compression";
            break;
        case CONFIG::control::reportsDestination:
            contstr = "Reports to ";
            if (value_bool)
                contstr += "console window";
            else
                contstr += "stdout";
            showValue = false;
            break;
        case CONFIG::control::savedInstrumentFormat:
            contstr = "Saved Instrument Format ";
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
            contstr += "Enable Auto Instance";
            yesno = true;
            break;
        case CONFIG::control::exposeStatus:
            showValue = false;
            contstr += "Show CLI context ";
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
            break;

        case CONFIG::control::jackMidiSource:
            contstr += "JACK MIDI source: ";
            contstr += miscMsgPop(value_int);
            showValue = false;
            break;
        case CONFIG::control::jackPreferredMidi:
            contstr += "Start with JACK MIDI";
            yesno = true;
            break;
        case CONFIG::control::jackServer:
            contstr += "JACK server: ";
            contstr += miscMsgPop(value_int);
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
            contstr += miscMsgPop(value_int);
            showValue = false;
            break;
        case CONFIG::control::alsaPreferredMidi:
            contstr += "Start with ALSA MIDI";
            yesno = true;
            break;
        case CONFIG::control::alsaAudioDevice:
            contstr += "ALSA audio device: ";
            contstr += miscMsgPop(value_int);
            showValue = false;
            break;
        case CONFIG::control::alsaPreferredAudio:
            contstr += "Start with ALSA audio";
            yesno = true;
            break;
        case CONFIG::control::alsaSampleRate:
            contstr += "ALSA sample rate: ";
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
            showValue = false;
            break;

        case CONFIG::control::bankRootCC:
            contstr += "Bank root CC ";
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
            showValue = false;
            break;

        case CONFIG::control::bankCC:
            contstr += "Bank CC ";
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
            showValue = false;
            break;
        case CONFIG::control::enableProgramChange:
            contstr += "Enable program change";
            yesno = true;
            break;
        case CONFIG::control::programChangeEnablesPart:
            contstr += "Program change enables part";
            yesno = true;
            break;
        /*case 70:
            contstr += "Enable extended program change";
            yesno = true;
            break;*/
        case CONFIG::control::extendedProgramChangeCC:
            contstr += "CC for extended program change";
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

        case CONFIG::control::enableNRPNs:
            contstr += "Enable NRPN";
            yesno = true;
            break;

        case CONFIG::control::saveCurrentConfig:
        {
            std::string name = miscMsgPop(value_int);
            if (write)
                contstr += ("save" + name);
            else
            {
                contstr += "Condition - ";
                 if(synth->getRuntime().configChanged)
                     contstr += "DIRTY";
                 else
                     contstr += "CLEAN";
            }
            showValue = false;
            break;
        }
        default:
            contstr = "Unrecognised";
            break;
    }

    if (yesno)
    {
        if (value_bool)
            contstr += " - yes";
        else
            contstr += " - no";
        showValue = false;
    }
    return ("Config " + contstr);
}


std::string InterChange::resolveBank(CommandBlock *getData)
{
    int value_int = lrint(getData->data.value);
    int control = getData->data.control;
    int kititem = getData->data.kit;
    int engine = getData->data.engine;
    int insert = getData->data.insert;
    std::string name = miscMsgPop(value_int);
    std::string contstr = "";
    showValue = false;
    switch(control)
    {
        case BANK::control::selectFirstInstrumentToSwap:
            contstr = "Set Instrument ID " + std::to_string(insert + 1) + "  Bank ID " + std::to_string(kititem) + "  Root ID " + std::to_string(engine) + " for swap";
            break;
        case BANK::control::selectSecondInstrumentAndSwap:
            if (name == "")
                name = "ped with Instrument ID " + std::to_string(insert + 1) + "  Bank ID " + std::to_string(kititem) + "  Root ID " + std::to_string(engine);
            contstr = "Swap" + name;
            break;

        case BANK::control::selectFirstBankToSwap:
            contstr = "Set Bank ID " + std::to_string(kititem) + "  Root ID " + std::to_string(engine) + " for swap";
            break;
        case BANK::control::selectSecondBankAndSwap:
            if (name == "")
                name = "ped with Bank ID " + std::to_string(kititem) + "  Root ID " + std::to_string(engine);
            contstr = "Swap" + name;
            break;
        default:
            contstr = "Unrecognised";
            break;
    }
    return ("Bank " + contstr);
}

std::string InterChange::resolveMain(CommandBlock *getData)
{
    float value = getData->data.value;
    int value_int = lrint(value);
//    bool write = ((getData->data.type & TOPLEVEL::type::Write) != 0);
    unsigned char control = getData->data.control;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
//    unsigned char par2 = getData->data.par2;
    std::string name;
    std::string contstr = "";
    if (getData->data.part == TOPLEVEL::section::midiIn)
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
                contstr = "CC " + std::to_string(int(engine)) + " ";
                break;
            case MIDI::control::programChange:
                showValue = false;
                contstr = miscMsgPop(value_int);
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
            contstr = "Part Number " + std::to_string(value_int + 1);
            break;
        case MAIN::control::availableParts:
            contstr = "Available Parts";
            break;

        case MAIN::control::detune:
            contstr = "Detune";
            break;
        case MAIN::control::keyShift:
            contstr = "Key Shift";
            break;
        case MAIN::control::mono:
            contstr = "Master Mono/Stereo ";
            showValue = false;
            if (value_int)
                contstr += "Mono";
            else
                contstr += "Stereo";
            break;

        case MAIN::control::soloType:
            showValue = false;
            contstr = "Chan 'solo' Switch - ";
            switch (value_int)
            {
                case 0:
                    contstr += "Off";
                    break;
                case 1:
                    contstr += "Row";
                    break;
                case 2:
                    contstr += "Column";
                    break;
                case 3:
                    contstr += "Loop";
                    break;
                case 4:
                    contstr += "Twoway";
                    break;
            }
            break;
        case MAIN::control::soloCC:
            showValue = false;
            contstr = "Chan 'solo' Switch CC ";
            if (value_int > 127)
                contstr += "undefined - set mode first";
            else
                contstr += std::to_string(value_int);
            break;
        case MAIN::control::exportBank:
            showValue = false;
            contstr = "Bank Export" + miscMsgPop(value_int);
            break;
        case MAIN::control::importBank:
            showValue = false;
            contstr = "Bank Import" + miscMsgPop(value_int);
            break;
        case MAIN::control::deleteBank:
            showValue = false;
            contstr = "Bank delete" + miscMsgPop(value_int);
            break;
        case MAIN::control::saveInstrument:
            showValue = false;
            contstr = "Bank Slot Save" + miscMsgPop(value_int);
            break;

        case MAIN::control::saveNamedInstrument:
            showValue = false;
            contstr = "Instrument Save" + miscMsgPop(value_int);
            break;

        case MAIN::control::loadNamedPatchset:
            showValue = false;
            contstr = "Patchset Load" + miscMsgPop(value_int);
            break;

        case MAIN::control::saveNamedPatchset:
            showValue = false;
            contstr = "Patchset Save" + miscMsgPop(value_int);
            break;

        case MAIN::control::loadNamedVector:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "Vector Load" + name;
            break;

        case MAIN::control::saveNamedVector:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "Vector Save" + name;
            break;

        case MAIN::control::loadNamedScale:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "Scale Load" + name;
            break;

        case MAIN::control::saveNamedScale:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "Scale Save" + name;
            break;

        case MAIN::control::loadNamedState:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "State Load" + name;
            break;

        case MAIN::control::saveNamedState:
            showValue = false;
            contstr = "State Save" + miscMsgPop(value_int);
            break;

        case MAIN::control::exportPadSynthSamples:
            showValue = false;
            contstr = "PadSynth Samples Save" + miscMsgPop(value_int);
            break;

        case MAIN::control::masterReset:
            showValue = false;
            contstr = "Reset All";
            break;
        case MAIN::control::masterResetAndMlearn:
            showValue = false;
            contstr = "Reset All including MIDI-learn";
            break;

        case MAIN::control::openManualPDF:
            showValue = false;
            contstr = "Open manual in PDF reader " + miscMsgPop(value_int);
            break;

        case MAIN::control::startInstance:
            showValue = false;
            contstr = "Start new instance " + std::to_string(value_int);
            break;
        case MAIN::control::stopInstance:
            showValue = false;
            contstr = "Close instance - " + miscMsgPop(value_int);
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
            contstr += std::to_string(int(kititem));
            if (value < 0.0f)
                contstr += " silent ";
            contstr += (" peak level " + std::to_string(value));
            break;
        case MAIN::control::readMainLRpeak:
            showValue = false;
            if(kititem == 1)
                contstr = "Right";
            else
                contstr = "Left";
            contstr += (" peak level " + std::to_string(value));
            break;
        case MAIN::control::readMainLRrms:
            showValue = false;
            if(kititem == 1)
                contstr = "Right";
            else
                contstr = "Left";
            contstr += (" RMS level " + std::to_string(value));
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Main " + contstr);
}


std::string InterChange::resolvePart(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char parameter = getData->data.parameter;
    unsigned char effNum = engine;

    bool kitType = (insert == TOPLEVEL::insert::kitGroup);
    int value_int = lrint(value);
    bool value_bool = YOSH::F2B(value);
    bool yesno = false;

    if (control == UNUSED)
        return "Number of parts";

    std::string kitnum;
    if (kitType)
        kitnum = " Kit " + std::to_string(kititem + 1) + " ";
    else
        kitnum = " ";

    std::string name = "";
    if (control >= PART::control::volumeRange && control <= PART::control::receivePortamento)
    {
        name = "Controller ";
        if (control >= PART::control::portamentoTime)
            name += "Portamento ";
    }
    else if (control >= PART::control::midiModWheel && control <= PART::control::midiBandwidth)
        name = "MIDI ";
    else if (kititem != UNUSED)
    {
        switch (engine)
        {
            case PART::engine::addSynth:
                name = "AddSynth ";
                break;
            case PART::engine::subSynth:
                name = "SubSynth ";
                break;
            case PART::engine::padSynth:
                name = "PadSynth ";
                break;
        }
    }

    std::string contstr = "";
    switch (control)
    {
        case PART::control::volume:
            contstr = "Volume";
            break;
        case PART::control::velocitySense:
            contstr = "Vel Sens";
            break;
        case PART::control::panning:
            contstr = "Panning";
            break;
        case PART::control::velocityOffset:
            contstr = "Vel Offset";
            break;
        case PART::control::midiChannel:
            showValue = false;
            contstr = "Midi CH - " + std::to_string(value_int + 1);
            if (value_int >= NUM_MIDI_CHANNELS * 2)
                contstr += " Midi ignored";
            else if (value_int >= NUM_MIDI_CHANNELS)
                contstr = contstr + " Note off only on CH " + std::to_string(value_int + 1 - NUM_MIDI_CHANNELS);
            break;
        case PART::control::keyMode:
            showValue = false;
            contstr = "Mode - ";
            if (value_int == 0)
                contstr += "Poly";
            else if (value_int == 1)
                contstr += "Mono";
            else if (value_int >= 2)
                contstr += "Legato";
            break;
        case PART::control::portamento:
            contstr = "Portamento Enable";
            yesno = true;
            break;
        case PART::control::enable:
            contstr = "Enable";
            if (!kitType)
            {
                switch(engine)
                {
                    case PART::engine::addSynth:
                        contstr = "AddSynth " + contstr;
                        break;
                    case PART::engine::subSynth:
                        contstr = "SubSynth " + contstr;
                        break;
                    case PART::engine::padSynth:
                        contstr = "PadSynth " + contstr;
                        break;
                }
            }
            break;
        case PART::control::kitItemMute:
            if (kitType)
                contstr = "Mute";
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
            contstr = "Reset Key Range";
            break;

        case PART::control::kitEffectNum:
            if (kitType)
                contstr = "Effect Number";
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
            contstr = "Humanise";
            break;

        case PART::control::drumMode:
            contstr = "Drum Mode";
            break;
        case PART::control::kitMode:
            contstr = "Kit Mode ";
            showValue = false;
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
            break;

        case PART::control::effectNumber:
            contstr = "Effect Number";
            break;
        case PART::control::effectType:
            contstr = "Effect " + std::to_string(effNum + 1) + " Type";
            break;
        case PART::control::effectDestination:
            contstr = "Effect " + std::to_string(effNum + 1) + " Destination";
            break;
        case PART::control::effectBypass:
            contstr = "Bypass Effect "+ std::to_string(effNum + 1);
            break;

        case PART::control::defaultInstrument: // doClearPart
            contstr = "Set Default Instrument";
            break;

        case PART::control::audioDestination:
            contstr = "Audio destination ";
            showValue = false;
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
            break;

        case PART::control::volumeRange:
            contstr = "Vol Range"; // not the *actual* volume
            break;
        case PART::control::volumeEnable:
            contstr = "Vol Enable";
            break;
        case PART::control::panningWidth:
            contstr = "Pan Width";
            break;
        case PART::control::modWheelDepth:
            contstr = "Mod Wheel Depth";
            break;
        case PART::control::exponentialModWheel:
            contstr = "Exp Mod Wheel";
            break;
        case PART::control::bandwidthDepth:
            contstr = "Bandwidth depth";
            break;
        case PART::control::exponentialBandwidth:
            contstr = "Exp Bandwidth";
            break;
        case PART::control::expressionEnable:
            contstr = "Expression Enable";
            break;
        case PART::control::FMamplitudeEnable:
            contstr = "FM Amp Enable";
            break;
        case PART::control::sustainPedalEnable:
            contstr = "Sustain Ped Enable";
            break;
        case PART::control::pitchWheelRange:
            contstr = "Pitch Wheel Range";
            break;
        case PART::control::filterQdepth:
            contstr = "Filter Q Depth";
            break;
        case PART::control::filterCutoffDepth:
            contstr = "Filter Cutoff Depth";
            break;
        case PART::control::breathControlEnable:
            yesno = true;
            contstr = "Breath Control";
            break;

        case PART::control::resonanceCenterFrequencyDepth:
            contstr = "Res Cent Freq Depth";
            break;
        case PART::control::resonanceBandwidthDepth:
            contstr = "Res Band Depth";
            break;

        case PART::control::portamentoTime:
            contstr = "Time";
            break;
        case PART::control::portamentoTimeStretch:
            contstr = "Tme Stretch";
            break;
        case PART::control::portamentoThreshold:
            contstr = "Threshold";
            break;
        case PART::control::portamentoThresholdType:
            contstr = "Threshold Type";
            break;
        case PART::control::enableProportionalPortamento:
            contstr = "Prop Enable";
            break;
        case PART::control::proportionalPortamentoRate:
            contstr = "Prop Rate";
            break;
        case PART::control::proportionalPortamentoDepth:
            contstr = "Prop depth";
            break;
        case PART::control::receivePortamento:
            contstr = "Receive";
            break;

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

        case PART::control::instrumentCopyright:
            ; // not yet
            break;
        case PART::control::instrumentComments:
            ; // not yet
            break;
        case PART::control::instrumentName:
            showValue = false;
            contstr = "Name is: " + miscMsgPop(value_int);
            break;
        case PART::control::defaultInstrumentCopyright:
            showValue = false;
            contstr = "Copyright ";
            if (parameter == 0)
                contstr += "load:\n";
            else
                contstr += "save:\n";
            contstr += miscMsgPop(value_int);
            break;
        case PART::control::resetAllControllers:
            contstr = "Clear controllers";
            break;

        case PART::control::partBusy:
            showValue = false;
            if (value_bool)
                contstr = "is busy";
            else
                contstr = "is free";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";

    }

    if (yesno)
    {
        if (value_bool)
            contstr += " - yes";
        else
            contstr += " - no";
        showValue = false;
    }
    return ("Part " + std::to_string(npart + 1) + kitnum + name + contstr);
}


std::string InterChange::resolveAdd(CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;

    std::string name = "";
    if (control <= ADDSYNTH::control::panning)
        name = " Amplitude ";
    else if (control >= ADDSYNTH::control::detuneFrequency && control <= ADDSYNTH::control::relativeBandwidth)
        name = "Frequency";

    std::string contstr = "";

    switch (control)
    {
        case ADDSYNTH::control::volume:
            contstr = "Volume";

            break;
        case ADDSYNTH::control::velocitySense:
            contstr = "Vel Sens";
            break;
        case ADDSYNTH::control::panning:
            contstr = "Panning";
            break;

        case ADDSYNTH::control::detuneFrequency:
            contstr = "Detune";
            break;

        case ADDSYNTH::control::octave:
            contstr = "Octave";
            break;
        case ADDSYNTH::control::detuneType:
            contstr = "Det type";
            break;
        case ADDSYNTH::control::coarseDetune:
            contstr = "Coarse Det";
            break;
        case ADDSYNTH::control::relativeBandwidth:
            contstr = "Rel B Wdth";
            break;

        case ADDSYNTH::control::stereo:
            contstr = "Stereo";
            break;
        case ADDSYNTH::control::randomGroup:
            contstr = "Rnd Grp";
            break;

        case ADDSYNTH::control::dePop:
            contstr = "De Pop";
            break;
        case ADDSYNTH::control::punchStrength:
            contstr = "Punch Strngth";
            break;
        case ADDSYNTH::control::punchDuration:
            contstr = "Punch Time";
            break;
        case ADDSYNTH::control::punchStretch:
            contstr = "Punch Strtch";
            break;
        case ADDSYNTH::control::punchVelocity:
            contstr = "Punch Vel";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + " AddSynth " + name + contstr);
}


std::string InterChange::resolveAddVoice(CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    int nvoice;
    if (engine >= PART::engine::addMod1)
        nvoice = engine - PART::engine::addMod1;
    else
        nvoice = engine - PART::engine::addVoice1;

    std::string name = "";
    switch (control & 0xf0)
    {
        case ADDVOICE::control::volume:
            name = " Amplitude ";
            break;
        case ADDVOICE::control::modulatorType:
            name = " Modulator ";
            break;
        case ADDVOICE::control::detuneFrequency:
            name = " Frequency ";
            break;
        case ADDVOICE::control::unisonFrequencySpread:
            name = " Unison ";
            break;
        case ADDVOICE::control::bypassGlobalFilter:
            name = " Filter ";
            break;
        case ADDVOICE::control::modulatorAmplitude:
            name = " Modulator Amp ";
            break;
        case ADDVOICE::control::modulatorDetuneFrequency:
            name = " Modulator Freq ";
            break;
        case ADDVOICE::control::modulatorOscillatorPhase:
            name = " Modulator Osc ";
            break;
    }

    std::string contstr = "";

    switch (control)
    {
        case ADDVOICE::control::volume:
            contstr = "Volume";
            break;
        case ADDVOICE::control::velocitySense:
            contstr = "Vel Sens";
            break;
        case ADDVOICE::control::panning:
            contstr = "Panning";
            break;
        case ADDVOICE::control::invertPhase:
            contstr = "Minus";
            break;
        case ADDVOICE::control::enableAmplitudeEnvelope:
            contstr = "Enable Env";
            break;
        case ADDVOICE::control::enableAmplitudeLFO:
            contstr = "Enable LFO";
            break;

        case ADDVOICE::control::modulatorType:
            contstr = "Type";
            break;
        case ADDVOICE::control::externalModulator:
            contstr = "Extern Mod";
            break;

        case ADDVOICE::control::externalOscillator:
            contstr = "Extern Osc";
            break;

        case ADDVOICE::control::detuneFrequency:
            contstr = "Detune";
            break;
        case ADDVOICE::control::equalTemperVariation:
            contstr = "Eq T";
            break;
        case ADDVOICE::control::baseFrequencyAs440Hz:
            contstr = "440Hz";
            break;
        case ADDVOICE::control::octave:
            contstr = "Octave";
            break;
        case ADDVOICE::control::detuneType:
            contstr = "Det type";
            break;
        case ADDVOICE::control::coarseDetune:
            contstr = "Coarse Det";
            break;
        case ADDVOICE::control::pitchBendAdjustment:
            contstr = "Bend Adj";
            break;
        case ADDVOICE::control::pitchBendOffset:
            contstr = "Offset Hz";
            break;
        case ADDVOICE::control::enableFrequencyEnvelope:
            contstr = "Enable Env";
            break;
        case ADDVOICE::control::enableFrequencyLFO:
            contstr = "Enable LFO";
            break;

        case ADDVOICE::control::unisonFrequencySpread:
            contstr = "Freq Spread";
            break;
        case ADDVOICE::control::unisonPhaseRandomise:
            contstr = "Phase Rnd";
            break;
        case ADDVOICE::control::unisonStereoSpread:
            contstr = "Stereo";
            break;
        case ADDVOICE::control::unisonVibratoDepth:
            contstr = "Vibrato";
            break;
        case ADDVOICE::control::unisonVibratoSpeed:
            contstr = "Vib Speed";
            break;
        case ADDVOICE::control::unisonSize:
            contstr = "Size";
            break;
        case ADDVOICE::control::unisonPhaseInvert:
            contstr = "Invert";
            break;
        case ADDVOICE::control::enableUnison:
            contstr = "Enable";
            break;

        case ADDVOICE::control::bypassGlobalFilter:
            contstr = "Bypass Global";
            break;
        case ADDVOICE::control::enableFilter:
            contstr = "Enable";
            break;
        case ADDVOICE::control::enableFilterEnvelope:
            contstr = "Enable Env";
            break;
        case ADDVOICE::control::enableFilterLFO:
            contstr = "Enable LFO";
            break;

        case ADDVOICE::control::modulatorAmplitude:
            contstr = "Volume";
            break;
        case ADDVOICE::control::modulatorVelocitySense:
            contstr = "V Sense";
            break;
        case ADDVOICE::control::modulatorHFdamping:
            contstr = "F Damp";
            break;
        case ADDVOICE::control::enableModulatorAmplitudeEnvelope:
            contstr = "Enable Env";
            break;

        case ADDVOICE::control::modulatorDetuneFrequency:
            break;
        case ADDVOICE::control::modulatorFrequencyAs440Hz:
            contstr = "440Hz";
            break;
        case ADDVOICE::control::modulatorDetuneFromBaseOsc:
            contstr = "Follow voice";
            break;
        case ADDVOICE::control::modulatorOctave:
            contstr = "Octave";
            break;
        case ADDVOICE::control::modulatorDetuneType:
            contstr = "Det type";
            break;
        case ADDVOICE::control::modulatorCoarseDetune:
            contstr = "Coarse Det";
            break;
        case ADDVOICE::control::enableModulatorFrequencyEnvelope: // local, external
            contstr = "Enable Env";
            break;

        case ADDVOICE::control::modulatorOscillatorPhase:
            contstr = " Phase";
            break;
        case ADDVOICE::control::modulatorOscillatorSource:
            contstr = " Source";
            break;

        case ADDVOICE::control::delay:
            contstr = " Delay";
            break;
        case ADDVOICE::control::enableVoice:
            contstr = " Enable";
            break;
        case ADDVOICE::control::enableResonance:
            contstr = " Resonance Enable";
            break;
        case ADDVOICE::control::voiceOscillatorPhase:
            contstr = " Osc Phase";
            break;
        case ADDVOICE::control::voiceOscillatorSource:
            contstr = " Osc Source";
            break;
        case ADDVOICE::control::soundType:
            contstr = " Sound type";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + " Add Voice " + std::to_string(nvoice + 1) + name + contstr);
}


std::string InterChange::resolveSub(CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char insert = getData->data.insert;

    if (insert == TOPLEVEL::insert::harmonicAmplitude || insert == TOPLEVEL::insert::harmonicPhaseBandwidth)
    {
        std::string Htype;
        if (insert == TOPLEVEL::insert::harmonicAmplitude)
            Htype = " Amplitude";
        else
            Htype = " Bandwidth";

        return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + " SubSynth Harmonic " + std::to_string(control + 1) + Htype);
    }

    std::string name = "";
    switch (control & 0x70)
    {
        case SUBSYNTH::control::volume:
            name = " Amplitude ";
            break;
        case SUBSYNTH::control::bandwidth:
            name = " Bandwidth ";
            break;
        case SUBSYNTH::control::detuneFrequency:
            name = " Frequency ";
            break;
        case SUBSYNTH::control::overtoneParameter1:
            name = " Overtones ";
            break;
        case SUBSYNTH::control::enableFilter:
            name = " Filter ";
            break;
    }

    std::string contstr = "";
    switch (control)
    {
        case SUBSYNTH::control::volume:
            contstr = "Volume";
            break;
        case SUBSYNTH::control::velocitySense:
            contstr = "Vel Sens";
            break;
        case SUBSYNTH::control::panning:
            contstr = "Panning";
            break;

        case SUBSYNTH::control::bandwidth:
            contstr = "";
            break;
        case SUBSYNTH::control::bandwidthScale:
            contstr = "Band Scale";
            break;
        case SUBSYNTH::control::enableBandwidthEnvelope:
            contstr = "Env Enab";
            break;

        case SUBSYNTH::control::detuneFrequency:
            contstr = "Detune";
            break;
        case SUBSYNTH::control::equalTemperVariation:
            contstr = "Eq T";
            break;
        case SUBSYNTH::control::baseFrequencyAs440Hz:
            contstr = "440Hz";
            break;
        case SUBSYNTH::control::octave:
            contstr = "Octave";
            break;
        case SUBSYNTH::control::detuneType:
            contstr = "Det type";
            break;
        case SUBSYNTH::control::coarseDetune:
            contstr = "Coarse Det";
            break;
        case SUBSYNTH::control::pitchBendAdjustment:
            contstr = "Bend Adj";
            break;
        case SUBSYNTH::control::pitchBendOffset:
            contstr = "Offset Hz";
            break;
        case SUBSYNTH::control::enableFrequencyEnvelope:
            contstr = "Env Enab";
            break;

        case SUBSYNTH::control::overtoneParameter1:
            contstr = "Par 1";
            break;
        case SUBSYNTH::control::overtoneParameter2:
            contstr = "Par 2";
            break;
        case SUBSYNTH::control::overtoneForceHarmonics:
            contstr = "Force H";
            break;
        case SUBSYNTH::control::overtonePosition:
            contstr = "Position";
            break;

        case SUBSYNTH::control::enableFilter:
            contstr = "Enable";
            break;

        case SUBSYNTH::control::filterStages:
            contstr = "Filt Stages";
            break;
        case SUBSYNTH::control::magType:
            contstr = "Mag Type";
            break;
        case SUBSYNTH::control::startPosition:
            contstr = "Start";
            break;

        case SUBSYNTH::control::clearHarmonics:
            contstr = "Clear Harmonics";
            break;

        case SUBSYNTH::control::stereo:
            contstr = "Stereo";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + " SubSynth " + name + contstr);
}


std::string InterChange::resolvePad(CommandBlock *getData)
{
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    bool write = (type & TOPLEVEL::type::Write) > 0;

    std::string name = "";
    switch (control & 0x70)
    {
        case PADSYNTH::control::volume:
            name = " Amplitude ";
            break;
        case PADSYNTH::control::bandwidth:
            name = " Bandwidth ";
            break;
        case PADSYNTH::control::detuneFrequency:
            name = " Frequency ";
            break;
        case PADSYNTH::control::overtoneParameter1:
            name = " Overtones ";
            break;
        case PADSYNTH::control::baseWidth:
            name = " Harmonic Base ";
            break;
        case PADSYNTH::control::harmonicBase:
            name = " Harmonic Samples ";
            break;
    }

    std::string contstr = "";
    switch (control)
    {
        case PADSYNTH::control::volume:
            contstr = "Volume";
            break;
        case PADSYNTH::control::velocitySense:
            contstr = "Vel Sens";
            break;
        case PADSYNTH::control::panning:
            contstr = "Panning";
            break;

        case PADSYNTH::control::bandwidth:
            contstr = "Bandwidth";
            break;
        case PADSYNTH::control::bandwidthScale:
            contstr = "Band Scale";
            break;
        case PADSYNTH::control::spectrumMode:
            contstr = "Spect Mode";
            break;

        case PADSYNTH::control::detuneFrequency:
            contstr = "Detune";
            break;
        case PADSYNTH::control::equalTemperVariation:
            contstr = "Eq T";
            break;
        case PADSYNTH::control::baseFrequencyAs440Hz:
            contstr = "440Hz";
            break;
        case PADSYNTH::control::octave:
            contstr = "Octave";
            break;
        case PADSYNTH::control::detuneType:
            contstr = "Det type";
            break;
        case PADSYNTH::control::coarseDetune:
            contstr = "Coarse Det";
            break;

        case PADSYNTH::control::pitchBendAdjustment:
            contstr = "Bend Adj";
            break;
        case PADSYNTH::control::pitchBendOffset:
            contstr = "Offset Hz";
            break;

        case PADSYNTH::control::overtoneParameter1:
            contstr = "Overt Par 1";
            break;
        case PADSYNTH::control::overtoneParameter2:
            contstr = "Overt Par 2";
            break;
        case PADSYNTH::control::overtoneForceHarmonics:
            contstr = "Force H";
            break;
        case PADSYNTH::control::overtonePosition:
            contstr = "Position";
            break;

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
        case PADSYNTH::control::autoscale:
            contstr = "Autoscale";
            break;

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

        case PADSYNTH::control::applyChanges:
            showValue = false;
            contstr = "Changes Applied";
            break;

        case PADSYNTH::control::stereo:
            contstr = "Stereo";
            break;

        case PADSYNTH::control::dePop:
            contstr = "De Pop";
            break;
        case PADSYNTH::control::punchStrength:
            contstr = "Punch Strngth";
            break;
        case PADSYNTH::control::punchDuration:
            contstr = "Punch Time";
            break;
        case PADSYNTH::control::punchStretch:
            contstr = "Punch Strtch";
            break;
        case PADSYNTH::control::punchVelocity:
            contstr = "Punch Vel";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    std::string isPad = "";

    if (write && ((control >= PADSYNTH::control::bandwidth && control <= PADSYNTH::control::spectrumMode) || (control >= PADSYNTH::control::overtoneParameter1 && control <= PADSYNTH::control::sampleSize)))
        isPad += " - Need to Apply";
    return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + " PadSynth " + name + contstr + isPad);
}


std::string InterChange::resolveOscillator(CommandBlock *getData)
{
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    bool write = (type & TOPLEVEL::type::Write) > 0;

    std::string isPad = "";
    std::string eng_name;
    if (engine == PART::engine::padSynth)
    {
        eng_name = " Padsysnth";
        if (write)
            isPad = " - Need to Apply";
    }
    else
    {
        int eng;
        if (engine >= PART::engine::addMod1)
            eng = engine - PART::engine::addMod1;
        else
            eng = engine - PART::engine::addVoice1;
        eng_name = " Add Voice " + std::to_string(eng + 1);
        if (engine >= PART::engine::addMod1)
            eng_name += " Modulator";
    }

    if (insert == TOPLEVEL::insert::harmonicAmplitude)
    {
        return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + eng_name + " Harmonic " + std::to_string((int)control + 1) + " Amplitude" + isPad);
    }
    else if(insert == TOPLEVEL::insert::harmonicPhaseBandwidth)
    {
        return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + eng_name + " Harmonic " + std::to_string((int)control + 1) + " Phase" + isPad);
    }

    std::string name;
    if (control >= OSCILLATOR::control::clearHarmonics || control <= OSCILLATOR::control::harmonicRandomnessType)
        name = " Oscillator";
    else if (control >= OSCILLATOR::control::harmonicShift)
        name = " Harm Mods";
    else if (control >= OSCILLATOR::control::autoClear)
        name = " Base Mods";
    else
        name = " Base Funct";

    std::string contstr;
    switch (control)
    {
        case OSCILLATOR::control::phaseRandomness:
            contstr = " Random";
            break;
        case OSCILLATOR::control::magType:
            contstr = " Mag Type";
            break;
        case OSCILLATOR::control::harmonicAmplitudeRandomness:
            contstr = " Harm Rnd";
            break;
        case OSCILLATOR::control::harmonicRandomnessType:
            contstr = " Harm Rnd Type";
            break;

        case OSCILLATOR::control::baseFunctionParameter:
            contstr = " Par";
            break;
        case OSCILLATOR::control::baseFunctionType:
            contstr = " Type";
            break;
        case OSCILLATOR::control::baseModulationParameter1:
            contstr = " Mod Par 1";
            break;
        case OSCILLATOR::control::baseModulationParameter2:
            contstr = " Mod Par 2";
            break;
        case OSCILLATOR::control::baseModulationParameter3:
            contstr = " Mod Par 3";
            break;
        case OSCILLATOR::control::baseModulationType:
            contstr = " Mod Type";
            break;

        case OSCILLATOR::control::autoClear: // this is local to the GUI
            break;
        case OSCILLATOR::control::useAsBaseFunction:
            contstr = " Osc As Base";
            break;
        case OSCILLATOR::control::waveshapeParameter:
            contstr = " Waveshape Par";
            break;
        case OSCILLATOR::control::waveshapeType:
            contstr = " Waveshape Type";
            break;
        case OSCILLATOR::control::filterParameter1:
            contstr = " Osc Filt Par 1";
            break;
        case OSCILLATOR::control::filterParameter2:
            contstr = " Osc Filt Par 2";
            break;
        case OSCILLATOR::control::filterBeforeWaveshape:
            contstr = " Osc Filt B4 Waveshape";
            break;
        case OSCILLATOR::control::filterType:
            contstr = " Osc Filt Type";
            break;
        case OSCILLATOR::control::modulationParameter1:
            contstr = " Osc Mod Par 1";
            break;
        case OSCILLATOR::control::modulationParameter2:
            contstr = " Osc Mod Par 2";
            break;
        case OSCILLATOR::control::modulationParameter3:
            contstr = " Osc Mod Par 3";
            break;
        case OSCILLATOR::control::modulationType:
            contstr = " Osc Mod Type";
            break;
        case OSCILLATOR::control::spectrumAdjustParameter:
            contstr = " Osc Spect Par";
            break;
        case OSCILLATOR::control::spectrumAdjustType:
            contstr = " Osc Spect Type";
            break;

        case OSCILLATOR::control::harmonicShift:
            contstr = " Shift";
            break;
        case OSCILLATOR::control::clearHarmonicShift:
            contstr = " Reset";
            break;
        case OSCILLATOR::control::shiftBeforeWaveshapeAndFilter:
            contstr = " B4 Waveshape & Filt";
            break;
        case OSCILLATOR::control::adaptiveHarmonicsParameter:
            contstr = " Adapt Param";
            break;
        case OSCILLATOR::control::adaptiveHarmonicsBase:
            contstr = " Adapt Base Freq";
            break;
        case OSCILLATOR::control::adaptiveHarmonicsPower:
            contstr = " Adapt Power";
            break;
        case OSCILLATOR::control::adaptiveHarmonicsType:
            contstr = " Adapt Type";
            break;

        case OSCILLATOR::control::clearHarmonics:
            contstr = " Clear Harmonics";
            break;
        case OSCILLATOR::control::convertToSine:
            contstr = " Conv To Sine";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + eng_name + name + contstr + isPad);
}


std::string InterChange::resolveResonance(CommandBlock *getData)
{
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    bool write = (type & TOPLEVEL::type::Write) > 0;

    std::string name;
    std::string isPad = "";
    if (engine == PART::engine::padSynth)
    {
        name = " PadSynth";
        if (write)
            isPad = " - Need to Apply";
    }
    else
        name = " AddSynth";

    if (insert == TOPLEVEL::insert::resonanceGraphInsert)
    {
        if (write == true && engine == PART::engine::padSynth)
            isPad = " - Need to Apply";
        return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + name + " Resonance Point " + std::to_string(control + 1) + isPad);
    }

    if (write == true && engine == PART::engine::padSynth && control != 104)
        isPad = " - Need to Apply";
    std::string contstr;
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
            break;

        case RESONANCE::control::randomType:
            contstr = "Random";
            break;

        case RESONANCE::control::interpolatePeaks:
            contstr = "Interpolate Peaks";
            break;
        case RESONANCE::control::protectFundamental:
            contstr = "Protect Fundamental";
            break;

        case RESONANCE::control::clearGraph:
            contstr = "Clear";
            break;
        case RESONANCE::control::smoothGraph:
            contstr = "Smooth";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + name + " Resonance " + contstr + isPad);
}


std::string InterChange::resolveLFO(CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insertParam = getData->data.parameter;

    std::string name;
    std::string lfo;

    if (engine == PART::engine::addSynth)
        name = " AddSynth";
    else if (engine == PART::engine::padSynth)
        name = " PadSynth";
    else if (engine >= PART::engine::addVoice1)
    {
        int nvoice = engine - PART::engine::addVoice1;
        name = " Add Voice " + std::to_string(nvoice + 1);
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

    std::string contstr;
    switch (control)
    {
        case LFOINSERT::control::speed:
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
            contstr = "AmpRand";
            break;
        case LFOINSERT::control::type:
            contstr = "Type";
            break;
        case LFOINSERT::control::continuous:
            contstr = "Cont";
            break;
        case LFOINSERT::control::frequencyRandomness:
            contstr = "FreqRand";
            break;
        case LFOINSERT::control::stretch:
            contstr = "Stretch";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + name + lfo + " LFO " + contstr);
}


std::string InterChange::resolveFilter(CommandBlock *getData)
{
    int value_int = int(getData->data.value);
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;

    int nseqpos = getData->data.parameter;
    int nformant = getData->data.parameter;
    int nvowel = getData->data.par2;

    std::string name;

    if (engine == PART::engine::addSynth)
        name = " AddSynth";
    else if (engine == PART::engine::subSynth)
        name = " SubSynth";
    else if (engine == PART::engine::padSynth)
        name = " PadSynth";
    else if (engine >= PART::engine::addVoice1)
        name = " Adsynth Voice " + std::to_string((engine - PART::engine::addVoice1) + 1);
    std::string contstr;
    switch (control)
    {
        case FILTERINSERT::control::centerFrequency:
            contstr = "C_Freq";
            break;
        case FILTERINSERT::control::Q:
            contstr = "Q";
            break;
        case FILTERINSERT::control::frequencyTracking:
            contstr = "FreqTrk";
            break;
        case FILTERINSERT::control::velocitySensitivity:
            contstr = "VsensA";
            break;
        case FILTERINSERT::control::velocityCurve:
            contstr = "Vsens";
            break;
        case FILTERINSERT::control::gain:
            contstr = "gain";
            break;
        case FILTERINSERT::control::stages:
            showValue = false;
            contstr = "Stages " + std::to_string(value_int + 1);
            break;
        case FILTERINSERT::control::baseType:
            contstr = "Filt Type";
            break;
        case FILTERINSERT::control::analogType:
            contstr = "An Type";
            break;
        case FILTERINSERT::control::stateVariableType:
            contstr = "SV Type";
            break;
        case FILTERINSERT::control::frequencyTrackingRange:
            contstr = "Fre Trk Offs";
            break;
        case FILTERINSERT::control::formantSlowness:
            contstr = "Form Fr Sl";
            break;
        case FILTERINSERT::control::formantClearness:
            contstr = "Form Vw Cl";
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
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }
    std::string extra = "";
    if (control >= FILTERINSERT::control::formantFrequency && control <= FILTERINSERT::control::formantAmplitude)
        extra ="Vowel " + std::to_string(nvowel) + " Formant " + std::to_string(nformant) + " ";
    else if (control == FILTERINSERT::control::vowelPositionInSequence)
        extra = "Seq Pos " + std::to_string(nseqpos) + " ";

    return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(kititem + 1) + name + " Filter " + extra + contstr);
}


std::string InterChange::resolveEnvelope(CommandBlock *getData)
{
    int value = lrint(getData->data.value);
    bool write = (getData->data.type & TOPLEVEL::type::Write) > 0;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char insertParam = getData->data.parameter;
    int par2 = getData->data.par2;

    std::string env;
    std::string name;
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
        name += std::to_string(nvoice + 1);
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
            env = " B.Width";
            break;
    }

    if (insert == TOPLEVEL::insert::envelopePoints)
    {
        if (!write)
        {
            return ("Freemode add/remove is write only. Current points " + std::to_string(int(par2)));
        }
        if (par2 != UNUSED)
            return ("Part " + std::to_string(int(npart + 1)) + " Kit " + std::to_string(int(kititem + 1)) + name  + env + " Env Added Freemode Point " + std::to_string(int(control & 0x3f)) + " X increment " + std::to_string(int(par2)) + " Y");
        else
        {
            showValue = false;
            return ("Part " + std::to_string(int(npart + 1)) + " Kit " + std::to_string(int(kititem + 1)) + name  + env + " Env Removed Freemode Point " +  std::to_string(int(control)) + "  Remaining " +  std::to_string(value));
        }
    }

    if (insert == TOPLEVEL::insert::envelopePointChange)
    {
        return ("Part " + std::to_string(int(npart + 1)) + " Kit " + std::to_string(int(kititem + 1)) + name  + env + " Env Freemode Point " +  std::to_string(int(control)) + " X increment " + std::to_string(int(par2)) + " Y");
    }

    std::string contstr;
    switch (control)
    {
        case ENVELOPEINSERT::control::attackLevel:
            contstr = "A val";
            break;
        case ENVELOPEINSERT::control::attackTime:
            contstr = "A dt";
            break;
        case ENVELOPEINSERT::control::decayLevel:
            contstr = "D val";
            break;
        case ENVELOPEINSERT::control::decayTime:
            contstr = "D dt";
            break;
        case ENVELOPEINSERT::control::sustainLevel:
            contstr = "S val";
            break;
        case ENVELOPEINSERT::control::releaseTime:
            contstr = "R dt";
            break;
        case ENVELOPEINSERT::control::releaseLevel:
            contstr = "R val";
            break;
        case ENVELOPEINSERT::control::stretch:
            contstr = "Stretch";
            break;

        case ENVELOPEINSERT::control::forcedRelease:
            contstr = "frcR";
            break;
        case ENVELOPEINSERT::control::linearEnvelope:
            contstr = "L";
            break;

        case ENVELOPEINSERT::control::edit:
            contstr = "Edit";
            break;

        case ENVELOPEINSERT::control::enableFreeMode:
            contstr = "Freemode";
            break;
        case ENVELOPEINSERT::control::points:
            contstr = "Points";
            break;
        case ENVELOPEINSERT::control::sustainPoint:
            contstr = "Sust";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + std::to_string(npart + 1) + " Kit " + std::to_string(int(kititem + 1)) + name  + env + " Env " + contstr);
}


std::string InterChange::resolveEffects(CommandBlock *getData)
{
    int value = lrint(getData->data.value);
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char effnum = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char parameter = getData->data.parameter;

    std::string name;
    std::string actual;
    if (npart == TOPLEVEL::section::systemEffects)
        name = "System";
    else if (npart == TOPLEVEL::section::insertEffects)
        name = "Insert";
    else
        name = "Part " + std::to_string(npart + 1);

    if (kititem == EFFECT::type::dynFilter && getData->data.insert != UNUSED)
    {
        if (npart == TOPLEVEL::section::systemEffects)
            name = "System";
        else if (npart == TOPLEVEL::section::insertEffects)
            name = "Insert";
        else name = "Part " + std::to_string(npart + 1);
        name += " Effect " + std::to_string(effnum + 1);

        return (name + " DynFilter ~ Filter Internal Control " + std::to_string(control));
    }

    name += " Effect " + std::to_string(effnum + 1);

    std::string effname = "";
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
            effname = " bypassed";
        else
            showValue = false;
        return (name + effname);
    }
    else if (npart >= TOPLEVEL::section::systemEffects && kititem == UNUSED)
    {
        std::string contstr;
        std::string second = "";
        if (npart == TOPLEVEL::section::systemEffects && insert == TOPLEVEL::insert::systemEffectSend)
        {
            name = "System ";
            contstr = "from Effect " + std::to_string(effnum + 1);
            second = " to Effect " + std::to_string(control + 1);
            return (name + contstr + second);
        }
        if (npart == TOPLEVEL::section::insertEffects && control == EFFECT::sysIns::effectDestination)
        {
            contstr = " To ";
            if (value == -2)
                contstr += "Master out";
            else if (value == -1)
                contstr = " Off";
            else
            {
                contstr += "Part ";
                second = std::to_string(value + 1);
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
    std::string contstr = "";
    if ((npart < NUM_MIDI_PARTS && control == PART::control::effectType) || (npart > TOPLEVEL::section::main && kititem == UNUSED && control == EFFECT::sysIns::effectType))
    {
        name += " set to";
        kititem = value | EFFECT::type::none; // TODO fix this!
        showValue = false;
    }
    else
        contstr = " Control " + std::to_string(control + 1);

    switch (kititem)
    {
        case EFFECT::type::none:
            effname = " None";
            contstr = " ";
            break;
        case EFFECT::type::reverb:
            effname = " Reverb";
            break;
        case EFFECT::type::echo:
            effname = " Echo";
            break;
        case EFFECT::type::chorus:
            effname = " Chorus";
            break;
        case EFFECT::type::phaser:
            effname = " Phaser";
            break;
        case EFFECT::type::alienWah:
            effname = " AlienWah";
            break;
        case EFFECT::type::distortion:
            effname = " Distortion";
            break;
        case EFFECT::type::eq:
            effname = " EQ";
            if (control > 1)
                contstr = " (Band " + std::to_string(int(parameter)) + ") Control " + std::to_string(control);
            break;
        case EFFECT::type::dynFilter:
            effname = " DynFilter";
            break;

        default:
            showValue = false;
            contstr = " Unrecognised";
    }

    if (kititem != EFFECT::type::eq && control == EFFECT::control::preset)
    {
        contstr = " Preset " + std::to_string (lrint(value) + 1);
        showValue = false;
    }

    return (name + effname + contstr);
}


void InterChange::mediate()
{
    CommandBlock getData;
    bool more;
    do
    {
        more = false;
#ifndef YOSHIMI_LV2_PLUGIN
        if (fromCLI->read(getData.bytes))
        {
            more = true;
            if(getData.data.part != TOPLEVEL::section::midiLearn) // Not special midi-learn message
                commandSend(&getData);
            returns(&getData);
        }
#endif
#ifdef GUI_FLTK

        if (fromGUI->read(getData.bytes))
        {
            more = true;
            if(getData.data.part != TOPLEVEL::section::midiLearn) // Not special midi-learn message
                commandSend(&getData);
            returns(&getData);
        }
#endif
        if (fromMIDI->read(getData.bytes))
        {
            more = true;
            if(getData.data.part != TOPLEVEL::section::midiLearn) // Not special midi-learn message
            {
                commandSend(&getData);
                returns(&getData);
            }
#ifdef GUI_FLTK
            else if (getData.data.control == MIDILEARN::control::reportActivity)
            {
                if (!toGUI->write(getData.bytes))
                synth->getRuntime().Log("Unable to write to toGUI buffer");
            }
#endif
        }
        else if (getData.data.control == TOPLEVEL::section::midiLearn) // not part!
        {
            synth->mididecode.midiProcess(getData.data.kit, getData.data.engine, getData.data.insert, false);
        }
        if (returnsBuffer->read(getData.bytes))
        {
            returns(&getData);
            more = true;
        }
    }
    while (more && synth->getRuntime().runSynth);
}


void InterChange::mutedDecode(unsigned int altData)
{ // these have all gone through a master fade down and mute
    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));
    putData.data.part = TOPLEVEL::section::main;
    putData.data.parameter = TOPLEVEL::route::lowPriority;

    switch (altData & 0xff)
    {
        case TOPLEVEL::muted::stopSound:
            putData.data.control = MAIN::control::stopSound;
            putData.data.type = 0xf0;
            break;
        case TOPLEVEL::muted::masterReset:
            putData.data.control = (altData >> 8) & 0xff;
            putData.data.type = altData >> 24;
            break;
        case TOPLEVEL::muted::patchsetLoad:
            putData.data.control = MAIN::control::loadNamedPatchset;
            putData.data.type = altData >> 24;
            putData.data.par2 = (altData >> 8) & 0xff;
            break;
        case TOPLEVEL::muted::vectorLoad:
            putData.data.control = MAIN::control::loadNamedVector;
            putData.data.type = altData >> 24;
            putData.data.insert = (altData >> 16) & 0xff;
            putData.data.par2 = (altData >> 8) & 0xff;
            break;
        case TOPLEVEL::muted::stateLoad:
            putData.data.control = MAIN::control::loadNamedState;
            putData.data.type = altData >> 24;
            putData.data.par2 = (altData >> 8) & 0xff;
            break;
        default:
            return;
            break;
    }
    indirectTransfers(&putData);
}

void InterChange::returns(CommandBlock *getData)
{
    unsigned char type = getData->data.type; // back from synth
    synth->getRuntime().finishedCLI = true; // belt and braces :)
    if (getData->data.source == TOPLEVEL::action::noAction)
        return; // no further action

    if (getData->data.parameter < TOPLEVEL::route::lowPriority || getData->data.parameter >= TOPLEVEL::route::adjustAndLoopback)
    {
#ifdef GUI_FLTK
        bool isMidi = type & TOPLEVEL::source::MIDI;
        bool write = (type & TOPLEVEL::type::Write) > 0;
        bool isCliOrGuiRedraw = type & TOPLEVEL::source::CLI; // separated out for clarity
        bool isOKtoRedraw = (isCliOrGuiRedraw && write) || isMidi;

        if (synth->guiMaster && isOKtoRedraw)
        {
            //std::cout << "writing to GUI" << std::endl;
            if (!toGUI->write(getData->bytes))
                synth->getRuntime().Log("Unable to write to toGUI buffer");
        }
#endif
    }
    if (!decodeLoopback->write(getData->bytes))
        synth->getRuntime().Log("Unable to write to decodeLoopback buffer");
}


void InterChange::setpadparams(int npart, int kititem)
{
    synth->part[npart]->busy = true;
    if (synth->part[npart]->kit[kititem].padpars != NULL)
        synth->part[npart]->kit[kititem].padpars->applyparameters();
    synth->part[npart]->busy = false;
    synth->partonoffWrite(npart, 2);
}


void InterChange::doClearPart(int npart)
{
    synth->part[npart]->defaultsinstrument();
    synth->part[npart]->cleanup();
    synth->getRuntime().currentPart = npart;
    synth->partonoffWrite(npart, 2);
}

bool InterChange::commandSend(CommandBlock *getData)
{
    bool isChanged = commandSendReal(getData);
    bool isWrite = (getData->data.type & TOPLEVEL::type::Write) > 0;
    if (isWrite && isChanged) //write command
    {
        synth->setNeedsSaving(true);
        unsigned char control = getData->data.control;
        unsigned char npart = getData->data.part;
        unsigned char insert = getData->data.insert;
        if (npart < NUM_MIDI_PARTS && (insert != UNUSED || (control != PART::control::enable && control != PART::control::instrumentName)))
        {
            if (synth->part[npart]->Pname == "Simple Sound")
            {
                synth->part[npart]->Pname ="No Title";
                getData->data.type |= TOPLEVEL::source::GUI; // force GUI to update
            }
        }
    }
    return isChanged;
}


bool InterChange::commandSendReal(CommandBlock *getData)
{
    unsigned char npart = getData->data.part;
    if (npart == TOPLEVEL::section::midiIn) // music input takes priority!
    {
        commandMidi(getData);
        __sync_and_and_fetch(&blockRead, 2); // clear it now it's done
        return false;
    }
//    float value = getData->data.value;
    unsigned char parameter = getData->data.parameter;
    if (parameter >= TOPLEVEL::route::lowPriority && parameter < TOPLEVEL::route::adjustAndLoopback)
        return true; // indirect transfer

    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    bool isGui = type & TOPLEVEL::source::GUI;
    char button = type & 3;

    //std::cout << "Type " << int(type) << "  Control " << int(control) << "  Part " << int(npart) << "  Kit " << int(kititem) << "  Engine " << int(engine) << std::endl;
    //std::cout  << "Insert " << int(insert)<< "  Parameter " << int(parameter) << "  Par2 " << int(par2) << std::endl;

    if (!isGui && button == 1)
    {
        __sync_and_and_fetch(&blockRead, 2); // just to be sure
        return false;
    }

    if (npart == TOPLEVEL::section::vector)
    {
        commandVector(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    if (npart == TOPLEVEL::section::scales)
    {
        commandMicrotonal(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    if (npart == TOPLEVEL::section::config)
    {
        commandConfig(getData);
        return true;
    }
    if (npart == TOPLEVEL::section::main)
    {
        commandMain(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    if ((npart == TOPLEVEL::section::systemEffects || npart == TOPLEVEL::section::insertEffects) && kititem == UNUSED)
    {
        commandSysIns(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    if (kititem >= EFFECT::type::none && kititem <= EFFECT::type::dynFilter)
    {
        commandEffects(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }

    if (npart >= NUM_MIDI_PARTS)
    {
        __sync_and_and_fetch(&blockRead, 2);
        return false; // invalid part number
    }

    if (kititem >= NUM_KIT_ITEMS && kititem != UNUSED)
    {
        __sync_and_and_fetch(&blockRead, 2);
        return false; // invalid kit number
    }

    Part *part = synth->part[npart];

    if (part->busy && engine == PART::engine::padSynth)
    {
        getData->data.type &= ~TOPLEVEL::type::Write; // turn it into a read
        getData->data.control = PART::control::partBusy;
        getData->data.kit = UNUSED;
        getData->data.engine = UNUSED;
        getData->data.insert = UNUSED;
        return false;
    }
    if (control == PART::control::partBusy)
    {
        getData->data.value = part->busy;
        return false;
    }
    if (kititem != UNUSED && kititem != 0 && engine != UNUSED && control != 8 && part->kit[kititem].Penabled == false)
    {
        __sync_and_and_fetch(&blockRead, 2);
        return false; // attempt to access not enabled kititem
    }

    if (kititem == UNUSED || insert == TOPLEVEL::insert::kitGroup)
    {
        if (control != PART::control::kitMode && kititem != UNUSED && part->Pkitmode == 0)
        {
            __sync_and_and_fetch(&blockRead, 2);
            return false;
        }
        commandPart(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }

    if (kititem > 0 && kititem != UNUSED && part->Pkitmode == 0)
    {
        __sync_and_and_fetch(&blockRead, 2);
        return false;
    }

    if (engine == PART::engine::padSynth)
    {
        switch(insert)
        {
            case UNUSED:
                commandPad(getData);
                break;
            case TOPLEVEL::insert::LFOgroup:
                commandLFO(getData);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandFilter(getData);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePoints:
                commandEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandEnvelope(getData);
                break;
            case TOPLEVEL::insert::oscillatorGroup:
                commandOscillator(getData,  part->kit[kititem].padpars->oscilgen);
                break;
            case TOPLEVEL::insert::harmonicAmplitude:
                commandOscillator(getData,  part->kit[kititem].padpars->oscilgen);
                break;
            case TOPLEVEL::insert::harmonicPhaseBandwidth:
                commandOscillator(getData,  part->kit[kititem].padpars->oscilgen);
                break;
            case TOPLEVEL::insert::resonanceGroup:
                commandResonance(getData, part->kit[kititem].padpars->resonance);
                break;
            case TOPLEVEL::insert::resonanceGraphInsert:
                commandResonance(getData, part->kit[kititem].padpars->resonance);
                break;
        }
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }

    if (engine == PART::engine::subSynth)
    {
        switch (insert)
        {
            case UNUSED:
                commandSub(getData);
                break;
            case TOPLEVEL::insert::harmonicAmplitude:
                commandSub(getData);
                break;
            case TOPLEVEL::insert::harmonicPhaseBandwidth:
                commandSub(getData);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandFilter(getData);
                break;
            case TOPLEVEL::insert::envelopeGroup:
                commandEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePoints:
                commandEnvelope(getData);
                break;
            case TOPLEVEL::insert::envelopePointChange:
                commandEnvelope(getData);
                break;
        }
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }

    if (engine >= PART::engine::addVoice1)
    {
        if ((engine > PART::engine::addVoice8 && engine < PART::engine::addMod1) || engine > PART::engine::addMod8)
        {
            getData->data.source = TOPLEVEL::action::noAction;
            synth->getRuntime().Log("Invalid voice number");
            synth->getRuntime().finishedCLI = true;
            __sync_and_and_fetch(&blockRead, 2);
            return false;
        }
        switch (insert)
        {
            case UNUSED:
                commandAddVoice(getData);
                break;
            case TOPLEVEL::insert::LFOgroup:
                commandLFO(getData);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandFilter(getData);
                break;
            case TOPLEVEL::insert::envelopeGroup:
            case TOPLEVEL::insert::envelopePoints:
            case TOPLEVEL::insert::envelopePointChange:
                commandEnvelope(getData);
                break;
            case TOPLEVEL::insert::oscillatorGroup:
            case TOPLEVEL::insert::harmonicAmplitude:
            case TOPLEVEL::insert::harmonicPhaseBandwidth:
                if (engine >= PART::engine::addMod1)
                {
                    engine -= PART::engine::addMod1;
                    if (control != 113)
                    {
                        int voicechange = part->kit[kititem].adpars->VoicePar[engine].PextFMoscil;
                        //std::cout << "ext Mod osc " << voicechange << std::endl;
                        if (voicechange != -1)
                        {
                            engine = voicechange;
                            getData->data.engine = engine +  PART::addMod1;
                        }   // force it to external mod
                    }

                    commandOscillator(getData,  part->kit[kititem].adpars->VoicePar[engine].FMSmp);
                }
                else
                {
                    engine -= PART::engine::addVoice1;
                    if (control != 137)
                    {
                        int voicechange = part->kit[kititem].adpars->VoicePar[engine].Pextoscil;
                        //std::cout << "ext voice osc " << voicechange << std::endl;
                        if (voicechange != -1)
                        {
                            engine = voicechange;
                            getData->data.engine = engine | PART::engine::addVoice1;
                        }   // force it to external voice
                    }

                    commandOscillator(getData,  part->kit[kititem].adpars->VoicePar[engine].OscilSmp);
                }
                break;
        }
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }

    if (engine == PART::engine::addSynth)
    {
        switch (insert)
        {
            case UNUSED:
                commandAdd(getData);
                break;
            case TOPLEVEL::insert::LFOgroup:
                commandLFO(getData);
                break;
            case TOPLEVEL::insert::filterGroup:
                commandFilter(getData);
                break;
            case TOPLEVEL::insert::envelopeGroup:
            case TOPLEVEL::insert::envelopePoints:
            case TOPLEVEL::insert::envelopePointChange:
                commandEnvelope(getData);
                break;
            case TOPLEVEL::insert::resonanceGroup:
            case TOPLEVEL::insert::resonanceGraphInsert:
                commandResonance(getData, part->kit[kititem].adpars->GlobalPar.Reson);
                break;
        }
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    getData->data.source = TOPLEVEL::action::noAction;
    synth->getRuntime().Log("Invalid engine number");
    synth->getRuntime().finishedCLI = true;
    __sync_and_and_fetch(&blockRead, 2);
    return false;
}


void InterChange::commandMidi(CommandBlock *getData)
{
    int value_int = lrint(getData->data.value);
    unsigned char control = getData->data.control;
    unsigned char chan = getData->data.kit;
    unsigned int char1 = getData->data.engine;
    unsigned char par2 = getData->data.par2;

    //std::cout << "value " << value_int << "  control " << int(control) << "  chan " << int(chan) << "  char1 " << char1 << "  char2 " << int(char2) << "  param " << int(parameter) << "  par2 " << int(par2) << std::endl;

    if (control == 2 && char1 >= 0x80)
    {
        char1 |= 0x200; // for 'specials'
    }

    switch(control)
    {
        case MIDI::control::noteOn:
            synth->NoteOn(chan, char1, value_int);
            synth->getRuntime().finishedCLI = true;
            getData->data.source = TOPLEVEL::action::noAction; // till we know what to do!
            break;
        case MIDI::control::noteOff:
            synth->NoteOff(chan, char1);
            synth->getRuntime().finishedCLI = true;
            getData->data.source = TOPLEVEL::action::noAction; // till we know what to do!
            break;
        case MIDI::control::controller:
            //std::cout << "Midi controller ch " << std::to_string(int(chan)) << "  type " << std::to_string(int(char1)) << "  val " << std::to_string(value_int) << std::endl;
            __sync_or_and_fetch(&blockRead, 1);
            synth->SetController(chan, char1, value_int);
            break;

        case MIDI::control::programChange: // Program / Bank / Root
            getData->data.parameter = TOPLEVEL::route::lowPriority;
            if ((value_int != UNUSED || par2 != NO_MSG) && chan < synth->getRuntime().NumAvailableParts)
            {
                synth->partonoffLock(chan & 0x3f, -1);
                synth->getRuntime().finishedCLI = true;
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
        synth->getRuntime().vectordata.Xaxis[ch] = UNUSED;
        synth->getRuntime().vectordata.Yaxis[ch] = UNUSED;
        synth->getRuntime().vectordata.Xfeatures[ch] = 0;
        synth->getRuntime().vectordata.Yfeatures[ch] = 0;
        synth->getRuntime().vectordata.Enabled[ch] = false;
        synth->getRuntime().vectordata.Name[ch] = "No Name " + std::to_string(ch + 1);
    }
}


void InterChange::commandVector(CommandBlock *getData)
{
    int value = getData->data.value; // no floats here
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned int chan = getData->data.insert;
    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    unsigned int features;

    if (control == VECTOR::control::erase)
    {
        vectorClear(chan);
        synth->setLastfileAdded(5, "");
        return;
    }
    if (write)
    {
        if (control >= VECTOR::control::Xfeature0 && control <= VECTOR::control::Xfeature3)
            features = synth->getRuntime().vectordata.Xfeatures[chan];
        else if (control >= VECTOR::control::Yfeature0 && control <= VECTOR::control::Yfeature3)
            features = synth->getRuntime().vectordata.Yfeatures[chan];
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
                        synth->vectorSet(127, chan, 0);
                        break;
                    case 4:
                        for (int ch = 0; ch < NUM_MIDI_CHANNELS; ++ ch)
                            synth->vectorSet(127, ch, 0);
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
                    if (!synth->vectorInit(0, chan, value))
                        synth->vectorSet(0, chan, value);
                    else
                        getData->data.value = 0;
                }
            }
            else
            {
                ;
            }
            break;
        case VECTOR::control::XleftInstrument:
            if (write)
                synth->vectorSet(4, chan, value);
            else
            {
                ;
            }
            break;
        case VECTOR::control::XrightInstrument:
            if (write)
                synth->vectorSet(5, chan, value);
            else
            {
                ;
            }
            break;
        case VECTOR::control::Xfeature0:
        case VECTOR::control::Yfeature0: // volume
            if (write)
                if (value == 0)
                    bitClear(features, 0);
                else
                    bitSet(features, 0);
            else
            {
                ;
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
                    if (!synth->vectorInit(1, chan, value))
                        synth->vectorSet(1, chan, value);
                    else
                        getData->data.value = 0;
                }
            }
            else
            {
                ;
            }
            break;
        case VECTOR::control::YupInstrument:
            if (write)
                synth->vectorSet(6, chan, value);
            else
            {
                ;
            }
            break;
        case VECTOR::control::YdownInstrument:
            if (write)
                synth->vectorSet(7, chan, value);
            else
            {
                ;
            }
            break;
    }

    if (write)
    {
        if (control >= VECTOR::control::Xfeature0 && control <= VECTOR::control::Xfeature3)
            synth->getRuntime().vectordata.Xfeatures[chan] = features;
        else if (control >= VECTOR::control::Yfeature0 && control <= VECTOR::control::Yfeature3)
            synth->getRuntime().vectordata.Yfeatures[chan] = features;
    }
}


void InterChange::commandMicrotonal(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    bool value_bool = YOSH::F2B(value);

    switch (control)
    {
        case SCALES::control::Afrequency:
            if (write)
            {
                if (value > 2000)
                    value = 2000;
                else if (value < 1)
                    value = 1;
                synth->microtonal.PAfreq = value;
            }
            else
                value = synth->microtonal.PAfreq;
            break;

        case SCALES::control::Anote:
            if (write)
                synth->microtonal.PAnote = value_int;
            else
                value = synth->microtonal.PAnote;
            break;
        case SCALES::control::invertScale:
            if (write)
                synth->microtonal.Pinvertupdown = value_bool;
            else
                value = synth->microtonal.Pinvertupdown;
            break;
        case SCALES::control::invertedScaleCenter:
            if (write)
                synth->microtonal.Pinvertupdowncenter = value_int;
            else
                value = synth->microtonal.Pinvertupdowncenter;
            break;
        case SCALES::control::scaleShift:
            if (write)
                synth->microtonal.Pscaleshift = value_int + 64;
            else
                value = synth->microtonal.Pscaleshift - 64;
            break;

        case SCALES::control::enableMicrotonal:
            if (write)
                synth->microtonal.Penabled = value_bool;
            else
                value = synth->microtonal.Penabled;
            break;

        case SCALES::control::enableKeyboardMap:
            if (write)
                synth->microtonal.Pmappingenabled = value_bool;
            else
               value = synth->microtonal.Pmappingenabled;
            break;
        case SCALES::control::lowKey:
            if (write)
            {
                if (value_int < 0)
                {
                    value_int = 0;
                    getData->data.value = value_int;
                }
                else if (value_int >= synth->microtonal.Pmiddlenote)
                {
                    value_int = synth->microtonal.Pmiddlenote - 1;
                    getData->data.value = value_int;
                }
                synth->microtonal.Pfirstkey = value_int;
            }
            else
                value = synth->microtonal.Pfirstkey;
            break;
        case SCALES::control::middleKey:
            if (write)
            {
                if (value_int <= synth->microtonal.Pfirstkey)
                {
                    value_int = synth->microtonal.Pfirstkey + 1;
                    getData->data.value = value_int;
                }
                else if (value_int >= synth->microtonal.Plastkey)
                {
                    value_int = synth->microtonal.Plastkey - 1;
                    getData->data.value = value_int;
                }
                synth->microtonal.Pmiddlenote = value_int;
            }
            else
                value = synth->microtonal.Pmiddlenote;
            break;
        case SCALES::control::highKey:
            if (write)
            {
                if (value_int <= synth->microtonal.Pmiddlenote)
                {
                    value_int = synth->microtonal.Pmiddlenote + 1;
                    getData->data.value = value_int;
                }
                else if (value_int > 127)
                {
                    value_int = 127;
                    getData->data.value = value_int;
                }
                synth->microtonal.Plastkey = value_int;
            }
            else
                value = synth->microtonal.Plastkey;
            break;

        case SCALES::control::tuning:
            // done elsewhere
            break;
        case SCALES::control::keyboardMap:
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

        case SCALES::control::retune:
            // done elsewhere
            break;
        case SCALES::control::clearAll: // Clear scales
            synth->microtonal.defaults();
            break;
    }

    if (!write)
        getData->data.value = value;
}


void InterChange::commandConfig(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    bool mightChange = true;
    int value_int = lrint(value);
    bool value_bool = YOSH::F2B(value);

    switch (control)
    {
// main
        case CONFIG::control::oscillatorSize:
            if (write)
            {
                value = nearestPowerOf2(value_int, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);
                getData->data.value = value;
                synth->getRuntime().Oscilsize = value;
            }
            else
                value = synth->getRuntime().Oscilsize;
            break;
        case CONFIG::control::bufferSize:
            if (write)
            {
                value = nearestPowerOf2(value_int, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);
                getData->data.value = value;
                synth->getRuntime().Buffersize = value;
            }
            else
                value = synth->getRuntime().Buffersize;
            break;
        case CONFIG::control::padSynthInterpolation:
            if (write)
                 synth->getRuntime().Interpolation = value_bool;
            else
                value = synth->getRuntime().Interpolation;
            break;
        case CONFIG::control::virtualKeyboardLayout:
            if (write)
                 synth->getRuntime().VirKeybLayout = value_int;
            else
                value = synth->getRuntime().VirKeybLayout;
            break;
        case CONFIG::control::XMLcompressionLevel:
            if (write)
                 synth->getRuntime().GzipCompression = value_int;
            else
                value = synth->getRuntime().GzipCompression;
            break;
        case CONFIG::control::reportsDestination:
            if (write)
                 synth->getRuntime().toConsole = value_bool;
            else
                value = synth->getRuntime().toConsole;
            break;
        case CONFIG::control::savedInstrumentFormat:
            if (write)
                 synth->getRuntime().instrumentFormat = value_int;
            else
                value = synth->getRuntime().instrumentFormat;
            break;
// switches
        case CONFIG::control::defaultStateStart:
            if (write)
                synth->getRuntime().loadDefaultState = value_bool;
            else
                value = synth->getRuntime().loadDefaultState;
            break;
        case CONFIG::control::hideNonFatalErrors:
            if (write)
                synth->getRuntime().hideErrors = value_bool;
            else
                value = synth->getRuntime().hideErrors;
            break;
        case CONFIG::control::showSplash:
            if (write)
                synth->getRuntime().showSplash = value_bool;
            else
                value = synth->getRuntime().showSplash;
            break;
        case CONFIG::control::logInstrumentLoadTimes:
            if (write)
                synth->getRuntime().showTimes = value_bool;
            else
                value = synth->getRuntime().showTimes;
            break;
        case CONFIG::control::logXMLheaders:
            if (write)
                synth->getRuntime().logXMLheaders = value_bool;
            else
                value = synth->getRuntime().logXMLheaders;
            break;
        case CONFIG::control::saveAllXMLdata:
            if (write)
                synth->getRuntime().xmlmax = value_bool;
            else
                value = synth->getRuntime().xmlmax;
            break;
        case CONFIG::control::enableGUI:
            if (write)
            {
                cout << "here" << endl;
                synth->getRuntime().showGui = value_bool;
                if (value_bool)
                    createEmptyFile(runGui);
                else
                    deleteFile(runGui);
            }
            else
                value = synth->getRuntime().showGui;
            break;
        case CONFIG::control::enableCLI:
            if (write)
                synth->getRuntime().showCLI = value_bool;
            else
                value = synth->getRuntime().showCLI;
            break;
        case CONFIG::control::enableAutoInstance:
            if (write)
                synth->getRuntime().autoInstance = value_bool;
            else
                value = synth->getRuntime().autoInstance;
            break;
        case CONFIG::control::enableSinglePath:
            if (write)
            {
                if (value_bool)
                    createEmptyFile(singlePath);
                else
                    deleteFile(singlePath);
            }
            else
                value = isRegFile(singlePath);
            break;
        case CONFIG::control::exposeStatus:
            if (write)
                firstSynth->getRuntime().showCLIcontext = value_int;
            else
                value = firstSynth->getRuntime().showCLIcontext;
            break;
// jack
        case CONFIG::control::jackMidiSource: // done elsewhere
            break;
        case CONFIG::control::jackPreferredMidi:
            if (write)
            {
                if (value_bool)
                    synth->getRuntime().midiEngine = jack_midi;
                else
                    synth->getRuntime().midiEngine = alsa_midi;
            }
            else
                value = (synth->getRuntime().midiEngine == jack_midi);
            break;
        case CONFIG::control::jackServer: // done elsewhere
            break;
        case CONFIG::control::jackPreferredAudio:
            if (write)
            {
                if (value_bool)
                    synth->getRuntime().audioEngine = jack_audio;
                else
                    synth->getRuntime().audioEngine = alsa_audio;
            }
            else
                value = (synth->getRuntime().audioEngine == jack_audio);
            break;
        case CONFIG::control::jackAutoConnectAudio:
            if (write)
            {
                synth->getRuntime().connectJackaudio = value_bool;
                synth->getRuntime().audioEngine = jack_audio;
            }
            else
                value = synth->getRuntime().connectJackaudio;
            break;
// alsa
        case CONFIG::control::alsaMidiSource: // done elsewhere
            break;
        case CONFIG::control::alsaPreferredMidi:
            if (write)
            {
                if (value_bool)
                    synth->getRuntime().midiEngine = alsa_midi;
                else
                    synth->getRuntime().midiEngine = jack_midi;
            }
            else
                value = (synth->getRuntime().midiEngine == alsa_midi);
            break;
        case CONFIG::control::alsaAudioDevice: // done elsewhere
            break;
        case CONFIG::control::alsaPreferredAudio:
            if (write)
            {
                if (value_bool)
                    synth->getRuntime().audioEngine = alsa_audio;
                else
                    synth->getRuntime().audioEngine = jack_audio;
            }
            else
                value = (synth->getRuntime().audioEngine == alsa_audio);
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
                synth->getRuntime().Samplerate = value;
                getData->data.value = value;
            }
            else
                switch(synth->getRuntime().Samplerate)
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
                    getData->data.value = value_int;
                }
                synth->getRuntime().midi_bank_root = value_int;
            }
            else
                value = synth->getRuntime().midi_bank_root;
            break;

        case CONFIG::control::bankCC:
            if (write)
            {
                if (value_int != 0 && value_int != 32)
                {
                    value_int = 128;
                    getData->data.value = value_int;
                }
                synth->getRuntime().midi_bank_C = value_int;
            }
            else
                value = synth->getRuntime().midi_bank_C;
            break;
        case CONFIG::control::enableProgramChange:
            if (write)
                synth->getRuntime().EnableProgChange = value_bool;
            else
                value = synth->getRuntime().EnableProgChange;
            break;
        case CONFIG::control::programChangeEnablesPart:
            if (write)
                synth->getRuntime().enable_part_on_voice_load = value_bool;
            else
                value = synth->getRuntime().enable_part_on_voice_load;
            break;
        //case CONFIG::control::enableExtendedProgramChange:
            //break;
        case CONFIG::control::extendedProgramChangeCC:
            if (write)
            {
                if (value_int > 119)
                {
                    value_int = 128;
                    getData->data.value = value_int;
                }
                synth->getRuntime().midi_upper_voice_C = value_int;
            }
            else
                value = synth->getRuntime().midi_upper_voice_C;
            break;
        case CONFIG::control::ignoreResetAllCCs:
            if (write)
                synth->getRuntime().ignoreResetCCs = value_bool;
            else
                value = synth->getRuntime().ignoreResetCCs;
            break;
        case CONFIG::control::logIncomingCCs:
            if (write)
                synth->getRuntime().monitorCCin = value_bool;
            else
                value = synth->getRuntime().monitorCCin;
            break;
        case CONFIG::control::showLearnEditor:
            if (write)
                synth->getRuntime().showLearnedCC = value_bool;
            else
                value = synth->getRuntime().showLearnedCC;
            break;
        case CONFIG::control::enableNRPNs:
            if (write)
                synth->getRuntime().enable_NRPN = value_bool;
            else
                value = synth->getRuntime().enable_NRPN;
            break;
// save config
        case CONFIG::control::saveCurrentConfig: //done elsewhere
            break;
        default:
            mightChange = false;
        break;
    }
    __sync_and_and_fetch(&blockRead, 2);
    if (!write)
        getData->data.value = value;
    else if (mightChange)
        synth->getRuntime().configChanged = true;
}


void InterChange::commandMain(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char parameter = getData->data.parameter;
    unsigned char par2 = getData->data.par2;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);
    int value_int = lrint(value);

    switch (control)
    {
        case MAIN::control::volume:
            if (write)
                synth->setPvolume(value);
            else
                value = synth->Pvolume;
            break;

        case MAIN::control::partNumber:
            if (write)
                synth->getRuntime().currentPart = value_int;
            else
                value = synth->getRuntime().currentPart;
            break;
        case MAIN::control::availableParts:
            if ((write) && (value == 16 || value == 32 || value == 64))
                synth->getRuntime().NumAvailableParts = value;
            else
                value = synth->getRuntime().NumAvailableParts;
            break;

        case MAIN::control::detune: // done elsewhere
            break;
        case MAIN::control::keyShift: // done elsewhere
            break;

        case MAIN::control::mono:
            if (write)
                synth->masterMono = value;
            else
                value = synth->masterMono;
            break;

        case MAIN::control::soloType: // solo mode
            if (write && value_int <= 4)
            {
                synth->getRuntime().channelSwitchType = value_int;
                synth->getRuntime().channelSwitchCC = 128;
                synth->getRuntime().channelSwitchValue = 0;
                if ((value_int & 5) == 0)
                {
                    for (int i = 0; i < NUM_MIDI_PARTS; ++i)
                        synth->part[i]->Prcvchn = (i & 15);
                }
                else
                {
                    for (int i = 1; i < NUM_MIDI_CHANNELS; ++i)
                    {
                        synth->part[i]->Prcvchn = 16;
                    }
                    synth->part[0]->Prcvchn = 0;
                }
            }
            else
            {
                write = false; // for an invalid write attempt
                value = synth->getRuntime().channelSwitchType;
            }
            break;
        case MAIN::control::soloCC:
            if (write && synth->getRuntime().channelSwitchType > 0)
                synth->getRuntime().channelSwitchCC = value_int;
            else
            {
                write = false; // for an invalid write attempt
                value = synth->getRuntime().channelSwitchCC;
            }
            break;

        case MAIN::control::setCurrentRootBank: // set current root and bank
            if (write)
            {
                if (kititem < 0x80) // should test for success
                    synth->getBankRef().setCurrentRootID(kititem);
                if (engine < 0x80) // should test for success
                    synth->getBankRef().setCurrentBankID(engine, true);
            }
            break;

        case MAIN::control::loadInstrument: // load instrument from ID
            /*
             * this is the lazy way to move all program changes
             * to the new MIDI method.
             */
            synth->partonoffLock(value_int, -1);
            getData->data.control = 8;
            getData->data.part = TOPLEVEL::section::midiIn;
            getData->data.kit = value_int;
            getData->data.value = par2;
            getData->data.parameter = TOPLEVEL::route::lowPriority;
            getData->data.par2 = UNUSED;
            break;
        case MAIN::control::loadNamedInstrument: // load named instrument
            synth->partonoffLock(value_int & 0x3f, -1);
            // as above for named instruments :)
            getData->data.control = 8;
            getData->data.part = TOPLEVEL::section::midiIn;
            getData->data.kit = value_int & 0x3f;
            getData->data.value = UNUSED;
            getData->data.parameter = TOPLEVEL::route::lowPriority;
            break;

        case MAIN::control::loadNamedPatchset:
            if (write && (parameter == TOPLEVEL::route::adjustAndLoopback))
            {
                synth->allStop(TOPLEVEL::muted::patchsetLoad | (par2 << 8) | (type << 24));
                getData->data.source = TOPLEVEL::action::noAction;
            }
            break;
        case MAIN::control::loadNamedVector:
            if (write && (parameter == TOPLEVEL::route::adjustAndLoopback))
            {
                synth->allStop(TOPLEVEL::muted::vectorLoad | (par2 << 8) | (insert << 16) | (type << 24));
                getData->data.source = TOPLEVEL::action::noAction;
            }
            break;
        case MAIN::control::saveNamedVector: // done elsewhere
            break;
        case MAIN::control::loadNamedScale: // done elsewhere
            break;
        case MAIN::control::saveNamedScale: // done elsewhere
            break;
        case MAIN::control::loadNamedState:
            if (write && (parameter == TOPLEVEL::route::adjustAndLoopback))
            {
                synth->allStop(TOPLEVEL::muted::stateLoad | (par2 << 8) | (type << 24));
                getData->data.source = TOPLEVEL::action::noAction;
            }
            break;
        case MAIN::control::saveNamedState: // done elsewhere
            break;
        case MAIN::control::masterReset:
        case MAIN::control::masterResetAndMlearn:
            if (write && (parameter == TOPLEVEL::route::adjustAndLoopback))
            {
                synth->allStop(TOPLEVEL::muted::masterReset | (control << 8) | (type << 24));
                getData->data.source = TOPLEVEL::action::noAction;
            }
            break;
        case MAIN::control::startInstance: // done elsewhere
            break;
        case MAIN::control::stopInstance: // done elsewhere
            break;
        case MAIN::control::stopSound: // just stop
            if (write)
                synth->allStop(TOPLEVEL::muted::stopSound);
            getData->data.source = TOPLEVEL::action::noAction;
            break;

        case MAIN::control::readPartPeak:
            if (!write && kititem < NUM_MIDI_PARTS)
            {
                if (engine == 1)
                    value = synth->VUdata.values.partsR[kititem];
                else
                    value = synth->VUdata.values.parts[kititem];
            }
            break;
        case MAIN::control::readMainLRpeak:
            if (!write)
            {
                if (kititem == 1)
                    value = synth->VUdata.values.vuOutPeakR;
                else
                    value = synth->VUdata.values.vuOutPeakL;
            }
            break;
        case MAIN::control::readMainLRrms:
            if (!write)
            {
                if (kititem == 1)
                    value = synth->VUdata.values.vuRmsPeakR;
                else
                    value = synth->VUdata.values.vuRmsPeakL;
            }
            break;

        case 254:
            synth->Mute();
            getData->data.source = TOPLEVEL::action::noAction;
            break;
    }

    if (!write)
        getData->data.value = value;
}


void InterChange::commandPart(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    bool kitType = (insert == TOPLEVEL::insert::kitGroup);

    if ( kitType && kititem >= NUM_KIT_ITEMS)
    {
        getData->data.source = TOPLEVEL::action::noAction;
        synth->getRuntime().Log("Invalid kit number");
        return;
    }
    int value_int = lrint(value);
    char value_bool = YOSH::F2B(value);

    Part *part;
    part = synth->part[npart];
    unsigned char effNum = part->Peffnum;
    switch (control)
    {
        case PART::control::volume:
            if (write)
                part->setVolume(value);
            else
                value = part->Pvolume;
            break;
        case PART::control::velocitySense:
            if (write)
                part->Pvelsns = value;
            else
                value = part->Pvelsns;
            break;
        case PART::control::panning:
            if (write)
                part->SetController(MIDI::CC::panning, value);
            else
                value = part->Ppanning;
            break;
        case PART::control::velocityOffset:
            if (write)
                part->Pveloffs = value;
            else
                value = part->Pveloffs;
            break;
        case PART::control::midiChannel:
            if (write)
                part->Prcvchn = value_int;
            else
                value = part->Prcvchn;
            break;
        case PART::control::keyMode:
            if (write)
                synth->SetPartKeyMode(npart, value_int);
            else
                value = (synth->ReadPartKeyMode(npart)) & 3; // clear out temporary legato
            break;
        case PART::control::portamento:
            if (write)
                part->ctl->portamento.portamento = value_bool;
            else
                value = part->ctl->portamento.portamento;
            break;
        case PART::control::enable:
            if (kitType)
            {
                switch(engine)
                {
                    case PART::engine::addSynth:
                        if (write)
                            part->kit[kititem].Padenabled = value_bool;
                        else
                            value = part->kit[kititem].Padenabled;
                        break;
                    case PART::engine::subSynth:
                        if (write)
                            part->kit[kititem].Psubenabled = value_bool;
                        else
                            value = part->kit[kititem].Psubenabled;
                        break;
                    case PART::engine::padSynth:
                        if (write)
                            part->kit[kititem].Ppadenabled = value_bool;
                        else
                            value = part->kit[kititem].Ppadenabled;
                        break;
                    default:
                        if (write)
                            part->setkititemstatus(kititem, value_bool);
                        else
                            value = part->kit[kititem].Penabled;
                        break;
                }
            }
            else
            {
                switch(engine)
                {
                    case PART::engine::addSynth:
                        if (write)
                            part->kit[0].Padenabled = value_bool;
                        else
                            value = part->kit[0].Padenabled;
                        break;
                    case PART::engine::subSynth:
                        if (write)
                            part->kit[0].Psubenabled = value_bool;
                        else
                            value = part->kit[0].Psubenabled;
                        break;
                    case PART::engine::padSynth:
                        if (write)
                            part->kit[0].Ppadenabled = value_bool;
                        else
                            value = part->kit[0].Ppadenabled;
                        break;
                    case UNUSED:
                        if (write)
                        {
                            if (value_bool && synth->getRuntime().currentPart != npart) // make it a part change
                            {
                                synth->partonoffWrite(npart, 1);
                                synth->getRuntime().currentPart = npart;
                                getData->data.value = npart;
                                getData->data.control = 14;
                                getData->data.part = TOPLEVEL::section::main;
                            }
                            else
                                synth->partonoffWrite(npart, value_int);
                        }
                        else
                            value = synth->partonoffRead(npart);
                        break;
                }
            }
            break;
        case PART::control::kitItemMute:
            if (kitType)
            {
                if (write)
                    part->kit[kititem].Pmuted = value_bool;
                else
                    value = part->kit[kititem].Pmuted;
            }
            break;

        case PART::control::minNote: // always return actual value
            if (kitType)
            {
                if (write)
                {
                    if (value_int > part->kit[kititem].Pmaxkey)
                        part->kit[kititem].Pminkey = part->kit[kititem].Pmaxkey;
                    else
                        part->kit[kititem].Pminkey = value_int;
                }
                value = part->kit[kititem].Pminkey;
            }
            else
            {
                if (write)
                {
                    if (value_int > part->Pmaxkey)
                        part->Pminkey = part->Pmaxkey;
                    else
                        part->Pminkey = value_int;
                }
                value = part->Pminkey;
            }
            break;
        case PART::control::maxNote: // always return actual value
            if (kitType)
            {
                if (write)
                {
                    if (value_int < part->kit[kititem].Pminkey)
                        part->kit[kititem].Pmaxkey = part->kit[kititem].Pminkey;
                    else
                        part->kit[kititem].Pmaxkey = value_int;
                }
                value = part->kit[kititem].Pmaxkey;
            }
            else
            {
                if (write)
                {
                    if (value_int < part->Pminkey)
                        part->Pmaxkey = part->Pminkey;
                    else
                        part->Pmaxkey = value_int;
                }
                value = part->Pmaxkey;
            }
            break;
        case PART::control::minToLastKey: // always return actual value
            value_int = part->lastnote;
            if (kitType)
            {
                if ((write) && value_int >= 0)
                {
                    if (value_int > part->kit[kititem].Pmaxkey)
                        part->kit[kititem].Pminkey = part->kit[kititem].Pmaxkey;
                    else
                        part->kit[kititem].Pminkey = part->lastnote;
                }
                value = part->kit[kititem].Pminkey;
            }
            else
            {
                if ((write) && part->lastnote >= 0)
                {
                    if(value_int > part->Pmaxkey)
                        part->Pminkey = part->Pmaxkey;
                    else
                        part->Pminkey = part->lastnote;
                }
                value = part->Pminkey;
            }
            break;
        case PART::control::maxToLastKey: // always return actual value
            value_int = part->lastnote;
            if (kitType)
            {
                if ((write) && part->lastnote >= 0)
                {
                    if(value_int < part->kit[kititem].Pminkey)
                        part->kit[kititem].Pmaxkey = part->kit[kititem].Pminkey;
                    else
                        part->kit[kititem].Pmaxkey = part->lastnote;
                }
                value = part->kit[kititem].Pmaxkey;
            }
            else
            {
                if ((write) && part->lastnote >= 0)
                {
                    if(value_int < part->Pminkey)
                        part->Pmaxkey = part->Pminkey;
                    else
                        part->Pmaxkey = part->lastnote;
                }
                value = part->Pmaxkey;
            }
            break;
        case PART::control::resetMinMaxKey:
            if (kitType)
            {
                if (write)
                {
                    part->kit[kititem].Pminkey = 0;
                    part->kit[kititem].Pmaxkey = 127;
                }
            }
            else
            {
                if (write)
                {
                    part->Pminkey = 0;
                    part->Pmaxkey = 127;
                }
            }
            break;

        case PART::control::kitEffectNum:
            if (kitType)
            {
                if (write)
                {
                    if (value_int == 0 )
                        part->kit[kititem].Psendtoparteffect = 127;
                    else
                        part->kit[kititem].Psendtoparteffect = value_int - 1;
                }
                else
                    value = part->kit[kititem].Psendtoparteffect;
            }
            break;

        case PART::control::maxNotes:
            if (write)
                part->setkeylimit(value_int);
            else
                value = part->Pkeylimit;
            break;
        case PART::control::keyShift: // done elsewhere
            break;

        case PART::control::partToSystemEffect1:
            if (write)
                synth->setPsysefxvol(npart,0, value);
            else
                value = synth->Psysefxvol[0][npart];
            break;
        case PART::control::partToSystemEffect2:
            if (write)
                synth->setPsysefxvol(npart,1, value);
            else
                value = synth->Psysefxvol[1][npart];
            break;
        case PART::control::partToSystemEffect3:
            if (write)
                synth->setPsysefxvol(npart,2, value);
            else
                value = synth->Psysefxvol[2][npart];
            break;
        case PART::control::partToSystemEffect4:
            if (write)
                synth->setPsysefxvol(npart,3, value);
            else
                value = synth->Psysefxvol[3][npart];
            break;

        case PART::control::humanise:
            if (write)
                part->Pfrand = value;
            else
                value = part->Pfrand;
            break;

        case PART::control::drumMode:
            if (write)
            {
                part->legatoFading = 0;
                part->Pdrummode = value_bool;
                synth->setPartMap(npart);
            }
            else
                value = part->Pdrummode;
            break;
        case PART::control::kitMode:
            if (write)
            {
                if (value == 3)
                {
                    part->Pkitmode = 1;
                    part->Pkitfade = true;
                    value = 1; // just to be sure
                }
                else
                {
                    part->Pkitfade = false;
                    part->Pkitmode = value_int;
                }
            }
            else
            {
                value = part->Pkitmode;
                if (value == 1 && part->Pkitfade == true)
                    value = 3;
            }
            break;

        case PART::control::effectNumber:
            if (write)
            {
                part->Peffnum = value_int;
                getData->data.parameter = (part->partefx[value_int]->geteffectpar(-1) != 0);
                getData->data.engine = value_int;
                getData->data.type |= TOPLEVEL::source::CLI;
            }
            else
                value = part->Peffnum;
            break;

        case PART::control::effectType:
            if (write)
                part->partefx[effNum]->changeeffect(value_int);
            else
                value = part->partefx[effNum]->geteffect();
            getData->data.parameter = (part->partefx[effNum]->geteffectpar(-1) != 0);
            break;
        case PART::control::effectDestination:
            if (write)
            {
                part->Pefxroute[effNum] = value_int;
                part->partefx[effNum]->setdryonly(value_int == 2);
            }
            else
                value = part->Pefxroute[effNum];
            break;
        case PART::control::effectBypass:
            if (write)
                part->Pefxbypass[effNum] = value_bool;
            else
                value = part->Pefxbypass[effNum];
            break;

        case PART::control::defaultInstrument: // doClearPart
            if(write)
            {
                synth->partonoffWrite(npart, -1);
                getData->data.parameter = TOPLEVEL::route::lowPriority;
            }
            else
                getData->data.source = TOPLEVEL::action::noAction;
            break;

        case PART::control::audioDestination:
            if (synth->partonoffRead(npart) != 1)
            {
                getData->data.value = part->Paudiodest; // specific for this control
                return;
            }
            else if (write)
            {
                if (npart < synth->getRuntime().NumAvailableParts)
                    synth->part[npart]->Paudiodest = value_int;
                getData->data.parameter = TOPLEVEL::route::lowPriority;
            }
            else
                value = part->Paudiodest;
            break;

        case PART::control::volumeRange: // start of controllers
            if (write)
                part->ctl->setvolume(value_int); // not the *actual* volume
            else
                value = part->ctl->volume.data;
            break;
        case PART::control::volumeEnable:
            if (write)
                part->ctl->volume.receive = value_bool;
            else
                value = part->ctl->volume.receive;
            break;
        case PART::control::panningWidth:
            if (write)
                part->ctl->setPanDepth(value_int);
            else
                value = part->ctl->panning.depth;
            break;
        case PART::control::modWheelDepth:
            if (write)
                part->ctl->modwheel.depth = value;
            else
                value = part->ctl->modwheel.depth;
            break;
        case PART::control::exponentialModWheel:
            if (write)
                part->ctl->modwheel.exponential = value_bool;
            else
                value = part->ctl->modwheel.exponential;
            break;
        case PART::control::bandwidthDepth:
            if (write)
                part->ctl->bandwidth.depth = value;
            else
                value = part->ctl->bandwidth.depth;
            break;
        case PART::control::exponentialBandwidth:
            if (write)
                part->ctl->bandwidth.exponential = value_bool;
            else
                value = part->ctl->bandwidth.exponential;
            break;
        case PART::control::expressionEnable:
            if (write)
                part->ctl->expression.receive = value_bool;
            else
                value = part->ctl->expression.receive;
            break;
        case PART::control::FMamplitudeEnable:
            if (write)
                part->ctl->fmamp.receive = value_bool;
            else
                value = part->ctl->fmamp.receive;
            break;
        case PART::control::sustainPedalEnable:
            if (write)
                part->ctl->sustain.receive = value_bool;
            else
                value = part->ctl->sustain.receive;
            break;
        case PART::control::pitchWheelRange:
            if (write)
                part->ctl->pitchwheel.bendrange = value_int;
            else
                value = part->ctl->pitchwheel.bendrange;
            break;
        case PART::control::filterQdepth:
            if (write)
                part->ctl->filterq.depth = value;
            else
                value = part->ctl->filterq.depth;
            break;
        case PART::control::filterCutoffDepth:
            if (write)
                part->ctl->filtercutoff.depth = value;
            else
                value = part->ctl->filtercutoff.depth;
            break;
        case PART::control::breathControlEnable:
            if (write)
                if (value_bool)
                    part->PbreathControl = 2;
                else
                    part->PbreathControl = 128; // impossible CC value
            else
                value = part->PbreathControl;
            break;

        case PART::control::resonanceCenterFrequencyDepth:
            if (write)
                part->ctl->resonancecenter.depth = value;
            else
                value = part->ctl->resonancecenter.depth;
            break;
        case PART::control::resonanceBandwidthDepth:
            if (write)
                part->ctl->resonancebandwidth.depth = value;
            else
                value = part->ctl->resonancebandwidth.depth;
            break;

        case PART::control::portamentoTime:
            if (write)
                part->ctl->portamento.time = value;
            else
                value = part->ctl->portamento.time;
            break;
        case PART::control::portamentoTimeStretch:
            if (write)
                part->ctl->portamento.updowntimestretch = value;
            else
                value = part->ctl->portamento.updowntimestretch;
            break;
        case PART::control::portamentoThreshold:
            if (write)
                part->ctl->portamento.pitchthresh = value;
            else
                value = part->ctl->portamento.pitchthresh;
            break;
        case PART::control::portamentoThresholdType:
            if (write)
                part->ctl->portamento.pitchthreshtype = value_int;
            else
                value = part->ctl->portamento.pitchthreshtype;
            break;
        case PART::control::enableProportionalPortamento:
            if (write)
                part->ctl->portamento.proportional = value_int;
            else
                value = part->ctl->portamento.proportional;
            break;
        case PART::control::proportionalPortamentoRate:
            if (write)
                part->ctl->portamento.propRate = value;
            else
                value = part->ctl->portamento.propRate;
            break;
        case PART::control::proportionalPortamentoDepth:
            if (write)
                part->ctl->portamento.propDepth = value;
            else
                value = part->ctl->portamento.propDepth;
            break;

        case PART::control::receivePortamento: // end of controllers
            if (write)
                part->ctl->portamento.receive = value_bool;
            else
                value = part->ctl->portamento.receive;
            break;

        case PART::control::midiModWheel:
            if (write)
                part->ctl->setmodwheel(value);
            else
                value = part->ctl->modwheel.data;
            break;
        case PART::control::midiBreath:
            ; // not yet
            break;
        case PART::control::midiExpression:
            if (write)
            {
                part->SetController(MIDI::CC::expression, value);
            }
            else
                value = part->ctl->expression.data;
            break;
        case PART::control::midiSustain:
            ; // not yet
            break;
        case PART::control::midiPortamento:
            ; // not yet
            break;
        case PART::control::midiFilterQ:
            if (write)
                part->ctl->setfilterq(value);
            else
                value = part->ctl->filterq.data;
            break;
        case PART::control::midiFilterCutoff:
            if (write)
                part->ctl->setfiltercutoff(value);
            else
                value = part->ctl->filtercutoff.data;
            break;
        case PART::control::midiBandwidth:
            if (write)
                part->ctl->setbandwidth(value);
            else
                value = part->ctl->bandwidth.data;
            break;

        case PART::control::instrumentCopyright:
            ; // not yet
            break;
        case PART::control::instrumentComments:
            ; // not yet
            break;
        case PART::control::instrumentName: // done elsewhere
            break;
        case PART::control::defaultInstrumentCopyright: // done elsewhere
            ;
        case PART::control::resetAllControllers:
            if (write)
                part->SetController(0x79,0);
            break;
    }

    if (!write || control == 18 || control == 19)
        getData->data.value = value;
}


void InterChange::commandAdd(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    char value_bool = YOSH::F2B(value);

    Part *part;
    part = synth->part[npart];
    ADnoteParameters *pars;
    pars = part->kit[kititem].adpars;

    switch (control)
    {
        case ADDSYNTH::control::volume:
            if (write)
                pars->GlobalPar.PVolume = value_int;
            else
                value = pars->GlobalPar.PVolume;
            break;
        case ADDSYNTH::control::velocitySense:
            if (write)
                pars->GlobalPar.PAmpVelocityScaleFunction = value_int;
            else
                value = pars->GlobalPar.PAmpVelocityScaleFunction;
            break;
        case ADDSYNTH::control::panning:
            if (write)
                pars->setGlobalPan(value_int);
            else
                value = pars->GlobalPar.PPanning;
            break;

        case ADDSYNTH::control::detuneFrequency:
            if (write)
                pars->GlobalPar.PDetune = value_int + 8192;
            else
                value = pars->GlobalPar.PDetune - 8192;
            break;

        case ADDSYNTH::control::octave:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 16;
                pars->GlobalPar.PCoarseDetune = k * 1024 + pars->GlobalPar.PCoarseDetune % 1024;
            }
            else
            {
                k = pars->GlobalPar.PCoarseDetune / 1024;
                if (k >= 8)
                    k -= 16;
                value = k;
            }
            break;
        }
        case ADDSYNTH::control::detuneType:
            if (write)
                pars->GlobalPar.PDetuneType = value_int;
            else
                value = pars->GlobalPar.PDetuneType;
            break;
        case ADDSYNTH::control::coarseDetune:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 1024;
                pars->GlobalPar.PCoarseDetune = k + (pars->GlobalPar.PCoarseDetune / 1024) * 1024;
            }
            else
            {
                k = pars->GlobalPar.PCoarseDetune % 1024;
                if (k >= 512)
                    k -= 1024;
                value = k;
            }
            break;
        }
        case ADDSYNTH::control::relativeBandwidth:
            if (write)
            {
                pars->GlobalPar.PBandwidth = value_int;
                 pars->getBandwidthDetuneMultiplier();
            }
            else
                value = pars->GlobalPar.PBandwidth;
            break;

        case ADDSYNTH::control::stereo:
            if (write)
                pars->GlobalPar.PStereo = value_bool;
            else
                value = pars->GlobalPar.PStereo;
            break;
        case ADDSYNTH::control::randomGroup:
            if (write)
                pars->GlobalPar.Hrandgrouping = value_bool;
            else
                value = pars->GlobalPar.Hrandgrouping;
            break;

        case ADDSYNTH::control::dePop:
            if (write)
                pars->GlobalPar.Fadein_adjustment = value_int;
            else
                value = pars->GlobalPar.Fadein_adjustment;
            break;
        case ADDSYNTH::control::punchStrength:
            if (write)
                pars->GlobalPar.PPunchStrength = value_int;
            else
                value = pars->GlobalPar.PPunchStrength;
            break;
        case ADDSYNTH::control::punchDuration:
            if (write)
                pars->GlobalPar.PPunchTime = value_int;
            else
                value = pars->GlobalPar.PPunchTime;
            break;
        case ADDSYNTH::control::punchStretch:
            if (write)
                pars->GlobalPar.PPunchStretch = value_int;
            else
                value = pars->GlobalPar.PPunchStretch;
            break;
        case ADDSYNTH::control::punchVelocity:
            if (write)
                pars->GlobalPar.PPunchVelocitySensing = value_int;
            else
                value = pars->GlobalPar.PPunchVelocitySensing;
            break;
    }
    if (!write)
        getData->data.value = value;
}


void InterChange::commandAddVoice(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    int nvoice;
    if (engine >= PART::engine::addMod1)
        nvoice = engine - PART::engine::addMod1;
    else
        nvoice = engine - PART::engine::addVoice1;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    char value_bool = YOSH::F2B(value);

    Part *part;
    part = synth->part[npart];
    ADnoteParameters *pars;
    pars = part->kit[kititem].adpars;

    switch (control)
    {
        case ADDVOICE::control::volume:
            if (write)
                pars->VoicePar[nvoice].PVolume = value_int;
            else
                value = pars->VoicePar[nvoice].PVolume;
            break;
        case ADDVOICE::control::velocitySense:
            if (write)
                pars->VoicePar[nvoice].PAmpVelocityScaleFunction = value_int;
            else
                value = pars->VoicePar[nvoice].PAmpVelocityScaleFunction;
            break;
        case ADDVOICE::control::panning:
            if (write)
                 pars->setVoicePan(nvoice, value_int);
            else
                value = pars->VoicePar[nvoice].PPanning;
            break;
        case ADDVOICE::control::invertPhase:
            if (write)
                pars->VoicePar[nvoice].PVolumeminus = value_bool;
            else
                value = pars->VoicePar[nvoice].PVolumeminus;
            break;
        case ADDVOICE::control::enableAmplitudeEnvelope:
            if (write)
                pars->VoicePar[nvoice].PAmpEnvelopeEnabled = value_bool;
            else
                value = pars->VoicePar[nvoice].PAmpEnvelopeEnabled;
            break;
        case ADDVOICE::control::enableAmplitudeLFO:
            if (write)
                pars->VoicePar[nvoice].PAmpLfoEnabled = value_bool;
            else
                value = pars->VoicePar[nvoice].PAmpLfoEnabled;
            break;

        case ADDVOICE::control::modulatorType:
            if (write)
                pars->VoicePar[nvoice].PFMEnabled = value_int;
            else
                value = pars->VoicePar[nvoice].PFMEnabled;
            break;
        case ADDVOICE::control::externalModulator:
            if (write)
                pars->VoicePar[nvoice].PFMVoice = value_int;
            else
                value = pars->VoicePar[nvoice].PFMVoice;
            break;

        case ADDVOICE::control::externalOscillator:
            if (write)
                pars->VoicePar[nvoice].PVoice = value_int;
            else
                value = pars->VoicePar[nvoice].PVoice;
            break;

        case ADDVOICE::control::detuneFrequency:
            if (write)
                pars->VoicePar[nvoice].PDetune = value_int + 8192;
            else
                value = pars->VoicePar[nvoice].PDetune-8192;
            break;
        case ADDVOICE::control::equalTemperVariation:
            if (write)
                pars->VoicePar[nvoice].PfixedfreqET = value_int;
            else
                value = pars->VoicePar[nvoice].PfixedfreqET;
            break;
        case ADDVOICE::control::baseFrequencyAs440Hz:
            if (write)
                 pars->VoicePar[nvoice].Pfixedfreq = value_bool;
            else
                value = pars->VoicePar[nvoice].Pfixedfreq;
            break;
        case ADDVOICE::control::octave:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 16;
                pars->VoicePar[nvoice].PCoarseDetune = k * 1024 + pars->VoicePar[nvoice].PCoarseDetune % 1024;
            }
            else
            {
                k = pars->VoicePar[nvoice].PCoarseDetune / 1024;
                if (k >= 8)
                    k -= 16;
                value = k;
            }
            break;
        }
        case ADDVOICE::control::detuneType:
            if (write)
                pars->VoicePar[nvoice].PDetuneType = value_int;
            else
                value = pars->VoicePar[nvoice].PDetuneType;
            break;
        case ADDVOICE::control::coarseDetune:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 1024;
                pars->VoicePar[nvoice].PCoarseDetune = k + (pars->VoicePar[nvoice].PCoarseDetune / 1024) * 1024;
            }
            else
            {
                k = pars->VoicePar[nvoice].PCoarseDetune % 1024;
                if (k >= 512)
                    k -= 1024;
                value = k;
            }
            break;
        }
        case ADDVOICE::control::pitchBendAdjustment:
            if (write)
                pars->VoicePar[nvoice].PBendAdjust = value_int;
            else
                value = pars->VoicePar[nvoice].PBendAdjust;
            break;
        case ADDVOICE::control::pitchBendOffset:
            if (write)
                pars->VoicePar[nvoice].POffsetHz = value_int;
            else
                value = pars->VoicePar[nvoice].POffsetHz;
            break;
        case ADDVOICE::control::enableFrequencyEnvelope:
            if (write)
                pars->VoicePar[nvoice].PFreqEnvelopeEnabled = value_int;
            else
                value = pars->VoicePar[nvoice].PFreqEnvelopeEnabled;
            break;
        case ADDVOICE::control::enableFrequencyLFO:
            if (write)
                pars->VoicePar[nvoice].PFreqLfoEnabled = value_int;
            else
                value = pars->VoicePar[nvoice].PFreqLfoEnabled;
            break;

        case ADDVOICE::control::unisonFrequencySpread:
            if (write)
                pars->VoicePar[nvoice].Unison_frequency_spread = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_frequency_spread;
            break;
        case ADDVOICE::control::unisonPhaseRandomise:
            if (write)
                pars->VoicePar[nvoice].Unison_phase_randomness = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_phase_randomness;
            break;
        case ADDVOICE::control::unisonStereoSpread:
            if (write)
                pars->VoicePar[nvoice].Unison_stereo_spread = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_stereo_spread;
            break;
        case ADDVOICE::control::unisonVibratoDepth:
            if (write)
                pars->VoicePar[nvoice].Unison_vibratto = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_vibratto;
            break;
        case ADDVOICE::control::unisonVibratoSpeed:
            if (write)
                pars->VoicePar[nvoice].Unison_vibratto_speed = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_vibratto_speed;
            break;
        case ADDVOICE::control::unisonSize:
            if (write)
            {
                if (value < 2)
                    value = 2;
                pars->VoicePar[nvoice].Unison_size = value_int;
            }
            else
                value = pars->VoicePar[nvoice].Unison_size;
            break;
        case ADDVOICE::control::unisonPhaseInvert:
            if (write)
                pars->VoicePar[nvoice].Unison_invert_phase = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_invert_phase;
            break;
        case ADDVOICE::control::enableUnison:
        {
            int k;
            if (write)
            {
                k = value_bool + 1;
                if (pars->VoicePar[nvoice].Unison_size < 2 || k == 1)
                    pars->VoicePar[nvoice].Unison_size = k;
            }
            else
                value = (pars->VoicePar[nvoice].Unison_size > 1);
            break;
        }

        case ADDVOICE::control::bypassGlobalFilter:
            if (write)
                pars->VoicePar[nvoice].Pfilterbypass = value_bool;
            else
                value = pars->VoicePar[nvoice].Pfilterbypass;
            break;
        case ADDVOICE::control::enableFilter:
            if (write)
                 pars->VoicePar[nvoice].PFilterEnabled =  value_bool;
            else
                value = pars->VoicePar[nvoice].PFilterEnabled;
            break;
        case ADDVOICE::control::enableFilterEnvelope:
            if (write)
                pars->VoicePar[nvoice].PFilterEnvelopeEnabled= value_bool;
            else
                value = pars->VoicePar[nvoice].PFilterEnvelopeEnabled;
            break;
        case ADDVOICE::control::enableFilterLFO:
            if (write)
                pars->VoicePar[nvoice].PFilterLfoEnabled= value_bool;
            else
                value = pars->VoicePar[nvoice].PFilterLfoEnabled;
            break;

        case ADDVOICE::control::modulatorAmplitude:
            if (write)
                pars->VoicePar[nvoice].PFMVolume = value_int;
            else
                value = pars->VoicePar[nvoice].PFMVolume;
            break;
        case ADDVOICE::control::modulatorVelocitySense:
            if (write)
                pars->VoicePar[nvoice].PFMVelocityScaleFunction = value_int;
            else
                value = pars->VoicePar[nvoice].PFMVelocityScaleFunction;
            break;
        case ADDVOICE::control::modulatorHFdamping:
            if (write)
                pars->VoicePar[nvoice].PFMVolumeDamp = value_int + 64;
            else
                value = pars->VoicePar[nvoice].PFMVolumeDamp - 64;
            break;
        case ADDVOICE::control::enableModulatorAmplitudeEnvelope:
            if (write)
                pars->VoicePar[nvoice].PFMAmpEnvelopeEnabled = value_bool;
            else
                value =  pars->VoicePar[nvoice].PFMAmpEnvelopeEnabled;
            break;

        case ADDVOICE::control::modulatorDetuneFrequency:
            if (write)
                pars->VoicePar[nvoice].PFMDetune = value_int + 8192;
            else
                value = pars->VoicePar[nvoice].PFMDetune - 8192;
            break;
        case ADDVOICE::control::modulatorDetuneFromBaseOsc:
            if (write)
                pars->VoicePar[nvoice].PFMDetuneFromBaseOsc = value_bool;
            else
                value = pars->VoicePar[nvoice].PFMDetuneFromBaseOsc;
            break;
        case ADDVOICE::control::modulatorFrequencyAs440Hz:
            if (write)
                pars->VoicePar[nvoice].PFMFixedFreq = value_bool;
            else
                value = pars->VoicePar[nvoice].PFMFixedFreq;
            break;
        case ADDVOICE::control::modulatorOctave:
        {
            int k;
            if (write)
            {
                k = value_int;
                if (k < 0)
                    k += 16;
                pars->VoicePar[nvoice].PFMCoarseDetune = k * 1024 + pars->VoicePar[nvoice].PFMCoarseDetune % 1024;
            }
            else
            {
                k = pars->VoicePar[nvoice].PFMCoarseDetune / 1024;
                if (k >= 8)
                    k -= 16;
                value = k;
            }
            break;
        }
        case ADDVOICE::control::modulatorDetuneType:
            if (write)
                pars->VoicePar[nvoice].PFMDetuneType = value_int;
            else
                value = pars->VoicePar[nvoice].PFMDetuneType;
            break;
        case ADDVOICE::control::modulatorCoarseDetune:
        {
            int k;
            if (write)
            {
                int k = value_int;
                if (k < 0)
                    k += 1024;
                pars->VoicePar[nvoice].PFMCoarseDetune = k + (pars->VoicePar[nvoice].PFMCoarseDetune / 1024) * 1024;
            }
            else
            {
                k = pars->VoicePar[nvoice].PFMCoarseDetune % 1024;
                if (k >= 512)
                    k-= 1024;
                value = k;
            }
            break;
        }
        case ADDVOICE::control::enableModulatorFrequencyEnvelope:
            if (write)
                pars->VoicePar[nvoice].PFMFreqEnvelopeEnabled = value_int;
            else
                value = pars->VoicePar[nvoice].PFMFreqEnvelopeEnabled;
            break;

        case ADDVOICE::control::modulatorOscillatorPhase:
            if (write)
                pars->VoicePar[nvoice].PFMoscilphase = 64 - value_int;
            else
                value = 64 - pars->VoicePar[nvoice].PFMoscilphase;
            break;
        case ADDVOICE::control::modulatorOscillatorSource:
            if (write)
                pars->VoicePar[nvoice].PextFMoscil = value_int;
            else
                value = pars->VoicePar[nvoice].PextFMoscil;
            break;

        case ADDVOICE::control::delay:
            if (write)
                pars->VoicePar[nvoice].PDelay = value_int;
            else
                value = pars->VoicePar[nvoice].PDelay;
            break;
        case ADDVOICE::control::enableVoice:
            if (write)
                pars->VoicePar[nvoice].Enabled = value_bool;
            else
                value = pars->VoicePar[nvoice].Enabled;
            break;
        case ADDVOICE::control::enableResonance:
            if (write)
                pars->VoicePar[nvoice].Presonance = value_bool;
            else
                value = pars->VoicePar[nvoice].Presonance;
            break;
        case ADDVOICE::control::voiceOscillatorPhase:
            if (write)
                pars->VoicePar[nvoice].Poscilphase = 64 - value_int;
            else
                value = 64 - pars->VoicePar[nvoice].Poscilphase;
            break;
        case ADDVOICE::control::voiceOscillatorSource:
            if (write)
                pars->VoicePar[nvoice].Pextoscil = value_int;
            else
                value = pars->VoicePar[nvoice].Pextoscil;
            break;
        case ADDVOICE::control::soundType:
            if (write)
                pars->VoicePar[nvoice].Type = value_int;
            else
                value = pars->VoicePar[nvoice].Type;
            break;
    }

    if (!write)
        getData->data.value = value;
}


void InterChange::commandSub(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char insert = getData->data.insert & 0x1f; // ensure no stray filter

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    char value_bool = YOSH::F2B(value);

    Part *part;
    part = synth->part[npart];
    SUBnoteParameters *pars;
    pars = part->kit[kititem].subpars;

    if (insert == TOPLEVEL::insert::harmonicAmplitude || insert == TOPLEVEL::insert::harmonicPhaseBandwidth)
    {
        if (insert == TOPLEVEL::insert::harmonicAmplitude)
        {
            if (write)
                pars->Phmag[control] = value;
            else
                value = pars->Phmag[control];
        }
        else
        {
            if (write)
                pars->Phrelbw[control] = value;
            else
                value = pars->Phrelbw[control];
        }

        if (!write)
            getData->data.value = value;
        else
            pars->PfilterChanged[control] = insert;
        return;
    }

    switch (control)
    {
        case SUBSYNTH::control::volume:
            if (write)
                pars->PVolume = value;
            else
                value = pars->PVolume;
            break;
        case SUBSYNTH::control::velocitySense:
            if (write)
                pars->PAmpVelocityScaleFunction = value;
            else
                value = pars->PAmpVelocityScaleFunction;
            break;
        case SUBSYNTH::control::panning:
            if (write)
                pars->setPan(value);
            else
                value = pars->PPanning;
            break;

        case SUBSYNTH::control::bandwidth:
            if (write)
                pars->Pbandwidth = value;
            else
                value = pars->Pbandwidth;
            break;
        case SUBSYNTH::control::bandwidthScale:
            if (write)
                pars->Pbwscale = value + 64;
            else
                value = pars->Pbwscale - 64;
            break;
        case SUBSYNTH::control::enableBandwidthEnvelope:
            if (write)
                pars->PBandWidthEnvelopeEnabled = value_bool;
            else
                value = pars->PBandWidthEnvelopeEnabled;
            break;

        case SUBSYNTH::control::detuneFrequency:
            if (write)
                pars->PDetune = value + 8192;
            else
                value = pars->PDetune - 8192;
            break;
        case SUBSYNTH::control::equalTemperVariation:
            if (write)
                pars->PfixedfreqET = value;
            else
                value = pars->PfixedfreqET;
            break;
        case SUBSYNTH::control::baseFrequencyAs440Hz:
            if (write)
                pars->Pfixedfreq = value_bool;
            else
                value = pars->Pfixedfreq;
            break;
        case SUBSYNTH::control::octave:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 16;
                pars->PCoarseDetune = k * 1024 + pars->PCoarseDetune % 1024;
            }
            else
            {
                k = pars->PCoarseDetune / 1024;
                if (k >= 8)
                    k -= 16;
                value = k;
            }
            break;
        }
        case SUBSYNTH::control::detuneType:
            if (write)
                pars->PDetuneType = value_int + 1;
            else
                value = pars->PDetuneType;
            break;
        case SUBSYNTH::control::coarseDetune:
        {
            int k;
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 1024;
                pars->PCoarseDetune = k + (pars->PCoarseDetune / 1024) * 1024;
            }
            else
            {
                k = pars->PCoarseDetune % 1024;
                if (k >= 512)
                    k -= 1024;
                value = k;
            }
            break;
        }

        case SUBSYNTH::control::pitchBendAdjustment:
            if (write)
                pars->PBendAdjust = value;
            else
                value = pars->PBendAdjust;
            break;

        case SUBSYNTH::control::pitchBendOffset:
            if (write)
                pars->POffsetHz = value;
            else
                value = pars->POffsetHz;
            break;

        case SUBSYNTH::control::enableFrequencyEnvelope:
            if (write)
                pars->PFreqEnvelopeEnabled = value_bool;
            else
                value = pars->PFreqEnvelopeEnabled;
            break;

        case SUBSYNTH::control::overtoneParameter1:
            if (write)
            {
                pars->POvertoneSpread.par1 = value;
                pars->updateFrequencyMultipliers();
            }
            else
                value = pars->POvertoneSpread.par1;
            break;
        case SUBSYNTH::control::overtoneParameter2:
            if (write)
            {
                pars->POvertoneSpread.par2 = value;
                pars->updateFrequencyMultipliers();
            }
            else
                value = pars->POvertoneSpread.par2;
            break;
        case SUBSYNTH::control::overtoneForceHarmonics:
            if (write)
            {
                pars->POvertoneSpread.par3 = value;
                pars->updateFrequencyMultipliers();
            }
            else
                value = pars->POvertoneSpread.par3;
            break;
        case SUBSYNTH::control::overtonePosition:
            if (write)
            {
                pars->POvertoneSpread.type =  value_int;
                pars->updateFrequencyMultipliers();
            }
            else
                value = pars->POvertoneSpread.type;
            break;

        case SUBSYNTH::control::enableFilter:
            if (write)
                pars->PGlobalFilterEnabled = value_bool;
            else
                value = pars->PGlobalFilterEnabled;
            break;

        case SUBSYNTH::control::filterStages:
            if (write)
                pars->Pnumstages = value_int;
            else
                value = pars->Pnumstages;
            break;
        case SUBSYNTH::control::magType:
            if (write)
                pars->Phmagtype = value_int;
            break;
        case SUBSYNTH::control::startPosition:
            if (write)
                pars->Pstart = value_int;
            else
                value = pars->Pstart;
            break;

        case SUBSYNTH::control::clearHarmonics:
            if (write)
            {
                for (int i = 0; i < MAX_SUB_HARMONICS; i++)
                {
                    pars->Phmag[i] = 0;
                    pars->Phrelbw[i] = 64;
                }
                pars->Phmag[0] = 127;
            }
            break;

        case SUBSYNTH::control::stereo:
            if (write)
                pars->Pstereo = value_bool;
            break;
    }

    if (!write)
        getData->data.value = value;
}


void InterChange::commandPad(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    char value_bool = YOSH::F2B(value);

    Part *part;
    part = synth->part[npart];
    PADnoteParameters *pars;
    pars = part->kit[kititem].padpars;

    switch (control)
    {
        case PADSYNTH::control::volume:
            if (write)
                pars->PVolume = value;
            else
                value = pars->PVolume;
            break;
        case PADSYNTH::control::velocitySense:
            if (write)
                pars->PAmpVelocityScaleFunction = value;
            else
                value = pars->PAmpVelocityScaleFunction;
            break;
        case PADSYNTH::control::panning:
            if (write)
                pars->setPan(value);
            else
                value = pars->PPanning;
            break;

        case PADSYNTH::control::bandwidth:
            if (write)
                pars->setPbandwidth(value_int);
            else
                value = pars->Pbandwidth;
            break;
        case PADSYNTH::control::bandwidthScale:
            if (write)
                pars->Pbwscale = value_int;
            else
                value = pars->Pbwscale;
            break;
        case PADSYNTH::control::spectrumMode:
            if (write)
                pars->Pmode = value_int;
            else
                value = pars->Pmode;
            break;

        case PADSYNTH::control::detuneFrequency:
            if (write)
                pars->PDetune = value_int + 8192;
            else
                value = pars->PDetune - 8192;
            break;
        case PADSYNTH::control::equalTemperVariation:
            if (write)
                pars->PfixedfreqET = value_int;
            else
                value = pars->PfixedfreqET;
            break;
        case PADSYNTH::control::baseFrequencyAs440Hz:
            if (write)
                pars->Pfixedfreq = value_bool;
            else
                value = pars->Pfixedfreq;
            break;
        case PADSYNTH::control::octave:
            if (write)
            {
                int tmp = value;
                if (tmp < 0)
                    tmp += 16;
                pars->PCoarseDetune = tmp * 1024 + pars->PCoarseDetune % 1024;
            }
            else
            {
                int tmp = pars->PCoarseDetune / 1024;
                if (tmp >= 8)
                    tmp -= 16;
                value = tmp;
            }
            break;
        case PADSYNTH::control::detuneType:
            if (write)
                 pars->PDetuneType = value_int + 1;
            else
                value =  pars->PDetuneType - 1;
            break;
        case PADSYNTH::control::coarseDetune:
            if (write)
            {
                int tmp = value;
                if (tmp < 0)
                    tmp += 1024;
                 pars->PCoarseDetune = tmp + (pars->PCoarseDetune / 1024) * 1024;
            }
            else
            {
                int tmp = pars->PCoarseDetune % 1024;
                if (tmp >= 512)
                    tmp -= 1024;
                value = tmp;
            }
            break;

        case PADSYNTH::control::pitchBendAdjustment:
            if (write)
                pars->PBendAdjust = value_int;
            else
                value = pars->PBendAdjust;
            break;
        case PADSYNTH::control::pitchBendOffset:
            if (write)
                pars->POffsetHz = value_int;
            else
                value = pars->POffsetHz;
            break;

        case PADSYNTH::control::overtoneParameter1:
            if (write)
                pars->Phrpos.par1 = value_int;
            else
                value = pars->Phrpos.par1;
            break;
        case PADSYNTH::control::overtoneParameter2:
            if (write)
                pars->Phrpos.par2 = value_int;
            else
                value = pars->Phrpos.par2;
            break;
        case PADSYNTH::control::overtoneForceHarmonics:
            if (write)
                pars->Phrpos.par3 = value_int;
            else
                value = pars->Phrpos.par3;
            break;
        case PADSYNTH::control::overtonePosition:
            if (write)
                pars->Phrpos.type = value_int;
            else
                value = pars->Phrpos.type;
            break;

        case PADSYNTH::control::baseWidth:
            if (write)
                pars->Php.base.par1 = value_int;
            else
                value = pars->Php.base.par1;
            break;
        case PADSYNTH::control::frequencyMultiplier:
            if (write)
                pars->Php.freqmult = value_int;
            else
                value = pars->Php.freqmult;
            break;
        case PADSYNTH::control::modulatorStretch:
            if (write)
                pars->Php.modulator.par1 = value_int;
            else
                value = pars->Php.modulator.par1;
            break;
        case PADSYNTH::control::modulatorFrequency:
            if (write)
                pars->Php.modulator.freq = value_int;
            else
                value = pars->Php.modulator.freq;
            break;
        case PADSYNTH::control::size:
            if (write)
                pars->Php.width = value_int;
            else
                value = pars->Php.width;
            break;
        case PADSYNTH::control::baseType:
            if (write)
                pars->Php.base.type = value;
            else
                value = pars->Php.base.type;
            break;
        case PADSYNTH::control::harmonicSidebands:
            if (write)
                 pars->Php.onehalf = value;
            else
                value = pars->Php.onehalf;
            break;
        case PADSYNTH::control::spectralWidth:
            if (write)
                pars->Php.amp.par1 = value_int;
            else
                value = pars->Php.amp.par1;
            break;
        case PADSYNTH::control::spectralAmplitude:
            if (write)
                pars->Php.amp.par2 = value_int;
            else
                value = pars->Php.amp.par2;
            break;
        case PADSYNTH::control::amplitudeMultiplier:
            if (write)
                pars->Php.amp.type = value;
            else
                value = pars->Php.amp.type;
            break;
        case PADSYNTH::control::amplitudeMode:
            if (write)
                pars->Php.amp.mode = value;
            else
                value = pars->Php.amp.mode;
            break;
        case PADSYNTH::control::autoscale:
            if (write)
                pars->Php.autoscale = value_bool;
            else
                value = pars->Php.autoscale;
            break;

        case PADSYNTH::control::harmonicBase:
            if (write)
                pars->Pquality.basenote = value_int;
            else
                value = pars->Pquality.basenote;
            break;
        case PADSYNTH::control::samplesPerOctave:
            if (write)
                pars->Pquality.smpoct = value_int;
            else
                value = pars->Pquality.smpoct;
            break;
        case PADSYNTH::control::numberOfOctaves:
            if (write)
                pars->Pquality.oct = value_int;
            else
                value = pars->Pquality.oct;
            break;
        case PADSYNTH::control::sampleSize:
            if (write)
                pars->Pquality.samplesize = value_int;
            else
                value = pars->Pquality.samplesize;
            break;

        case PADSYNTH::control::applyChanges:
            if (write)
            {
                synth->partonoffWrite(npart, -1);
                getData->data.parameter = TOPLEVEL::route::lowPriority;
            }
            break;

        case PADSYNTH::control::stereo:
            if (write)
                pars->PStereo = value_bool;
            else
            {
                ;
            }
            break;

        case PADSYNTH::control::dePop:
            if (write)
                pars->Fadein_adjustment = value_int;
            else
                value = pars->Fadein_adjustment;
            break;
        case PADSYNTH::control::punchStrength:
            if (write)
                pars->PPunchStrength = value_int;
            else
                value = pars->PPunchStrength;
            break;
        case PADSYNTH::control::punchDuration:
            if (write)
                pars->PPunchTime = value_int;
            else
                value = pars->PPunchTime;
            break;
        case PADSYNTH::control::punchStretch:
            if (write)
                pars->PPunchStretch = value_int;
            else
                value = pars->PPunchStretch;
            break;
        case PADSYNTH::control::punchVelocity:
            if (write)
                pars->PPunchVelocitySensing = value_int;
            else
                value = pars->PPunchVelocitySensing;
            break;
    }

    if (!write)
        getData->data.value = value;
}


void InterChange::commandOscillator(CommandBlock *getData, OscilGen *oscil)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char insert = getData->data.insert;

    int value_int = lrint(value);
    bool value_bool = YOSH::F2B(value);
    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    if (insert == TOPLEVEL::insert::harmonicAmplitude)
    {
        if (write)
        {
            oscil->Phmag[control] = value_int;
            if (value_int == 64)
                oscil->Phphase[control] = 64;
            oscil->prepare();
        }
        else
            getData->data.value = oscil->Phmag[control];
        return;
    }
    else if(insert == TOPLEVEL::insert::harmonicPhaseBandwidth)
    {
        if (write)
        {
            oscil->Phphase[control] = value_int;
            oscil->prepare();
        }
        else
            getData->data.value = oscil->Phphase[control];
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
                oscil->useasbase();
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
                oscil->prepare();
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
                oscil->prepare();
            }
            break;
        case OSCILLATOR::control::convertToSine:
            if (write)
                oscil->convert2sine();
            break;
    }
    if (!write)
        getData->data.value = value;
}


void InterChange::commandResonance(CommandBlock *getData, Resonance *respar)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char insert = getData->data.insert;
    int value_int = lrint(value);
    bool value_bool = YOSH::F2B(value);
    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    if (insert == TOPLEVEL::insert::resonanceGraphInsert)
    {
        if (write)
            respar->setpoint(control, value_int);
        else
            getData->data.value = respar->Prespoints[control];
        return;
    }

    switch (control)
    {
        case RESONANCE::control::maxDb:
            if (write)
                respar->PmaxdB = value_int;
            else
                value = respar->PmaxdB;
            break;
        case RESONANCE::control::centerFrequency:
            if (write)
                respar->Pcenterfreq = value_int;
            else
                value = respar->Pcenterfreq;
            break;
        case RESONANCE::control::octaves:
            if (write)
                respar->Poctavesfreq = value_int;
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
        getData->data.value = value;
}


void InterChange::commandLFO(CommandBlock *getData)
{
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insertParam = getData->data.parameter;

    Part *part;
    part = synth->part[npart];

    if (engine == PART::engine::addSynth)
    {
       switch (insertParam)
        {
           case TOPLEVEL::insertType::amplitude:
                lfoReadWrite(getData, part->kit[kititem].adpars->GlobalPar.AmpLfo);
                break;
            case TOPLEVEL::insertType::frequency:
                lfoReadWrite(getData, part->kit[kititem].adpars->GlobalPar.FreqLfo);
                break;
            case TOPLEVEL::insertType::filter:
                lfoReadWrite(getData, part->kit[kititem].adpars->GlobalPar.FilterLfo);
                break;
        }
    }
    else if (engine == PART::engine::padSynth)
    {
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                lfoReadWrite(getData, part->kit[kititem].padpars->AmpLfo);
                break;
            case TOPLEVEL::insertType::frequency:
                lfoReadWrite(getData, part->kit[kititem].padpars->FreqLfo);
                break;
            case TOPLEVEL::insertType::filter:
                lfoReadWrite(getData, part->kit[kititem].padpars->FilterLfo);
                break;
        }
    }
    else if (engine >= PART::engine::addVoice1)
    {
        int nvoice = engine - PART::engine::addVoice1;
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                lfoReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].AmpLfo);
                break;
            case TOPLEVEL::insertType::frequency:
                lfoReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FreqLfo);
                break;
            case TOPLEVEL::insertType::filter:
                lfoReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FilterLfo);
                break;
        }
    }
}


void InterChange::lfoReadWrite(CommandBlock *getData, LFOParams *pars)
{
    bool write = (getData->data.type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    float val = getData->data.value;

    switch (getData->data.control)
    {
        case LFOINSERT::control::speed:
            if (write)
                pars->setPfreq(val * Fmul2I);
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
                pars->setPcontinous((val > 0.5f));
            else
                val = pars->Pcontinous;
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

    if (!write)
        getData->data.value = val;
}


void InterChange::commandFilter(CommandBlock *getData)
{
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;

    Part *part;
    part = synth->part[npart];

    if (engine == PART::engine::addSynth)
    {
        filterReadWrite(getData, part->kit[kititem].adpars->GlobalPar.GlobalFilter
                    , &part->kit[kititem].adpars->GlobalPar.PFilterVelocityScale
                    , &part->kit[kititem].adpars->GlobalPar.PFilterVelocityScaleFunction);
    }
    else if (engine == PART::engine::subSynth)
    {
        filterReadWrite(getData, part->kit[kititem].subpars->GlobalFilter
                    , &part->kit[kititem].subpars->PGlobalFilterVelocityScale
                    , &part->kit[kititem].subpars->PGlobalFilterVelocityScaleFunction);
    }
    else if (engine == PART::engine::padSynth)
    {
        filterReadWrite(getData, part->kit[kititem].padpars->GlobalFilter
                    , &part->kit[kititem].padpars->PFilterVelocityScale
                    , &part->kit[kititem].padpars->PFilterVelocityScaleFunction);
    }
    else if (engine >= PART::engine::addVoice1)
    {
        int eng = engine - PART::engine::addVoice1;
        filterReadWrite(getData, part->kit[kititem].adpars->VoicePar[eng].VoiceFilter
                    , &part->kit[kititem].adpars->VoicePar[eng].PFilterVelocityScale
                    , &part->kit[kititem].adpars->VoicePar[eng].PFilterVelocityScaleFunction);
    }
}


void InterChange::filterReadWrite(CommandBlock *getData, FilterParams *pars, unsigned char *velsnsamp, unsigned char *velsns)
{
    bool write = (getData->data.type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    float val = getData->data.value;
    int value_int = lrint(val);

    int nseqpos = getData->data.parameter;
    int nformant = getData->data.parameter;
    int nvowel = getData->data.par2;

    switch (getData->data.control)
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

    if (!write)
        getData->data.value = val;
}


void InterChange::commandEnvelope(CommandBlock *getData)
{
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insertParam = getData->data.parameter;

    Part *part;
    part = synth->part[npart];

    std::string env;
    std::string name;
    if (engine == PART::engine::addSynth)
    {
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(getData, part->kit[kititem].adpars->GlobalPar.AmpEnvelope);
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(getData, part->kit[kititem].adpars->GlobalPar.FreqEnvelope);
                break;
            case TOPLEVEL::insertType::filter:
                envelopeReadWrite(getData, part->kit[kititem].adpars->GlobalPar.FilterEnvelope);
                break;
        }
    }
    else if (engine == PART::engine::subSynth)
    {
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(getData, part->kit[kititem].subpars->AmpEnvelope);
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(getData, part->kit[kititem].subpars->FreqEnvelope);
                break;
            case TOPLEVEL::insertType::filter:
                envelopeReadWrite(getData, part->kit[kititem].subpars->GlobalFilterEnvelope);
                break;
            case TOPLEVEL::insertType::bandwidth:
                envelopeReadWrite(getData, part->kit[kititem].subpars->BandWidthEnvelope);
                break;
        }
    }
    else if (engine == PART::engine::padSynth)
    {
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(getData, part->kit[kititem].padpars->AmpEnvelope);
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(getData, part->kit[kititem].padpars->FreqEnvelope);
                break;
            case TOPLEVEL::insertType::filter:
                envelopeReadWrite(getData, part->kit[kititem].padpars->FilterEnvelope);
                break;
        }
    }

    else if (engine >= PART::engine::addMod1)
    {
        int nvoice = engine - PART::engine::addMod1;
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FMAmpEnvelope);
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FMFreqEnvelope);
                break;
        }
    }

    else if (engine >= PART::engine::addVoice1)
    {
        int nvoice = engine - PART::engine::addVoice1;
        switch (insertParam)
        {
            case TOPLEVEL::insertType::amplitude:
                envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].AmpEnvelope);
                break;
            case TOPLEVEL::insertType::frequency:
                envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FreqEnvelope);
                break;
            case TOPLEVEL::insertType::filter:
                envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FilterEnvelope);
                break;
        }
    }
}


void InterChange::envelopeReadWrite(CommandBlock *getData, EnvelopeParams *pars)
{
    int val = lrint(getData->data.value); // these are all integers or bool
    bool write = (getData->data.type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    unsigned char point = getData->data.control;
    unsigned char insert = getData->data.insert;
    unsigned char Xincrement = getData->data.par2;

    int envpoints = pars->Penvpoints;
    bool isAddpoint = (Xincrement < 0xff);

    if (insert == TOPLEVEL::insert::envelopePoints) // here be dragons :(
    {
        if (!pars->Pfreemode)
        {
            getData->data.value = 0xff;
            getData->data.par2 = 0xff;
            return;
        }

        if (!write || point == 0 || point >= envpoints)
        {
            getData->data.value = 0xff;
            getData->data.par2 = envpoints;
            return;
        }

        if (isAddpoint)
        {
            if (envpoints < MAX_ENVELOPE_POINTS)
            {
                pars->Penvpoints += 1;
                for (int i = envpoints; i >= point; -- i)
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
                getData->data.value = val;
                getData->data.par2 = Xincrement;
            }
            else
                getData->data.value = 0xff;
            return;
        }
        else if (envpoints < 4)
        {
            getData->data.value = 0xff;
            getData->data.par2 = 0xff;
            return; // can't have less than 4
        }
        else
        {
            envpoints -= 1;
            for (int i = point; i < envpoints; ++ i)
            {
                pars->Penvdt[i] = pars->Penvdt[i + 1];
                pars->Penvval[i] = pars->Penvval[i + 1];
            }
            if (point <= pars->Penvsustain)
                -- pars->Penvsustain;
            pars->Penvpoints = envpoints;
            getData->data.value = envpoints;
        }
        return;
    }

    if (insert == TOPLEVEL::insert::envelopePointChange)
    {
        if (!pars->Pfreemode || point >= envpoints)
        {
            getData->data.value = 0xff;
            getData->data.par2 = 0xff;
            return;
        }
        if (write)
        {
            pars->Penvval[point] = val;
            if (point == 0)
                Xincrement = 0;
            else
                pars->Penvdt[point] = Xincrement;
        }
        else
        {
            val = pars->Penvval[point];
            Xincrement = pars->Penvdt[point];
        }
        getData->data.value = val;
        getData->data.par2 = Xincrement;
        return;
    }

    switch (getData->data.control)
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

        case ENVELOPEINSERT::control::enableFreeMode:
            if (write)
            {
                if (val != 0)
                    pars->Pfreemode = 1;
                else
                    pars->Pfreemode = 0;
            }
            else
                val = pars->Pfreemode;
            break;
        case ENVELOPEINSERT::control::points:
            if (!pars->Pfreemode)
            {
                val = 0xff;
                Xincrement = 0xff;
            }
            else
            {
                val = envpoints;
                Xincrement = envpoints; // don't really need this now
            }
            break;
        case ENVELOPEINSERT::control::sustainPoint:
            if (write)
                pars->Penvsustain = val;
            else
                val = pars->Penvsustain;
            break;
    }
    getData->data.value = val;
    getData->data.par2 = Xincrement;
    return;
}


void InterChange::commandSysIns(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char effnum = getData->data.engine;
    unsigned char insert = getData->data.insert;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    //std::cout << "Value " << value_int << "  Control " << int(control) << "  Part " << int(npart) << "  Effnum " << int(effnum) << "  Insert " << int(insert) << std::endl;
    bool isSysEff = (npart == TOPLEVEL::section::systemEffects);
    if (isSysEff)
        effnum = synth->syseffnum;
    else
        effnum = synth->inseffnum;

    if (insert == UNUSED)
    {
        switch (control)
        {
            case EFFECT::sysIns::effectNumber:
                if (write)
                {
                    if (isSysEff)
                    {
                        synth->syseffnum = value_int;
                        getData->data.parameter = (synth->sysefx[value_int]->geteffectpar(-1) != 0);
                    }
                    else
                    {
                        synth->inseffnum = value_int;
                        getData->data.parameter = (synth->insefx[value_int]->geteffectpar(-1) != 0);
                    }
                    getData->data.type |= TOPLEVEL::source::CLI;
                    getData->data.engine = value_int;
                }
                else
                {
                    if (isSysEff)
                        value = synth->syseffnum;
                    else
                        value = synth->inseffnum;
                }
                break;
            case EFFECT::sysIns::effectType:
                if (write)
                {
                    if (isSysEff)
                    {
                        synth->sysefx[effnum]->changeeffect(value_int);
                        getData->data.parameter = (synth->sysefx[effnum]->geteffectpar(-1) != 0);
                    }
                    else
                    {
                        synth->insefx[effnum]->changeeffect(value_int);
                        getData->data.parameter = (synth->insefx[effnum]->geteffectpar(-1) != 0);
                    }
                }
                else
                {
                    if (isSysEff)
                        value = synth->sysefx[effnum]->geteffect();
                    else
                        value = synth->insefx[effnum]->geteffect();
                }
                break;
            case EFFECT::sysIns::effectDestination: // insert only
                if (write)
                {
                    synth->Pinsparts[effnum] = value_int;
                    if (value_int == -1)
                        synth->insefx[effnum]->cleanup();
                }
                else
                    value = synth->Pinsparts[effnum];
                break;
        }
    }
    else // system only
    {
        if (write)
            synth->setPsysefxsend(effnum, control, value);
        else
            value = synth->Psysefxsend[effnum][control];
    }

    if (!write)
        getData->data.value = value;
}


void InterChange::commandEffects(CommandBlock *getData)
{
    float value = getData->data.value;
    int value_int = int(value + 0.5f);
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char effnum = getData->data.engine;

    bool write = (type & TOPLEVEL::type::Write) > 0;
    if (write)
    {
        __sync_or_and_fetch(&blockRead, 1);
        getData->data.type |= TOPLEVEL::source::CLI;
    }

    EffectMgr *eff;

    if (npart == TOPLEVEL::section::systemEffects)
        eff = synth->sysefx[effnum];

    else if (npart == TOPLEVEL::section::insertEffects)
        eff = synth->insefx[effnum];
    else if (npart < NUM_MIDI_PARTS)
        eff = synth->part[npart]->partefx[effnum];
    else
        return; // invalid part number
    if (kititem > EFFECT::type::dynFilter)
        return;
    if (kititem == EFFECT::type::dynFilter && getData->data.insert != UNUSED)
    {
        if (write)
            eff->seteffectpar(-1, true); // effect changed
        filterReadWrite(getData, eff->filterpars,NULL,NULL);
        return;
    }
    if (control >= EFFECT::control::changed)
    {
        if (!write)
        {
            value = eff->geteffectpar(-1);
            getData->data.value = value;
            //std::cout << "Eff Changed " << int(value) << std::endl;
        }
        return; // specific for reading change status
    }
    if (write)
    {
        if (kititem == EFFECT::type::eq)
        /*
         * specific to EQ
         * Control 1 is not a saved parameter, but a band index.
         * Also, EQ does not have presets, and 16 is the control
         * for the band 1 frequency parameter
        */
        {
            if (control <= 1)
                eff->seteffectpar(control, value_int);
            else
            {
                eff->seteffectpar(control + (eff->geteffectpar(1) * 5), value_int);
                getData->data.parameter = eff->geteffectpar(1);
            }
        }
        else
        {
            if (control == EFFECT::control::preset)
                eff->changepreset(value_int);
            else
            {
                if (kititem == EFFECT::type::reverb && control == 10 && value_int == 2)
                    // this needs to use the defaults
                    // to all for future upgrades
                    getData->data.par2 = 20;
                eff->seteffectpar(control, value_int);
            }
        }
        //std::cout << "eff value " << value << "  control " << int(control) << "  band " << synth->getRuntime().EQband << std::endl;
    }
    else
    {
        if (kititem == EFFECT::type::eq && control > 1) // specific to EQ
        {
            value = eff->geteffectpar(control + (eff->geteffectpar(1) * 5));
            getData->data.parameter = eff->geteffectpar(1);
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
        getData->data.value = value;
}

// tests and returns corrected values
void InterChange::testLimits(CommandBlock *getData)
{
    float value = getData->data.value;

    int control = getData->data.control;
    /*
     * This is a special case as existing defined
     * midi CCs need to be checked.
     * I don't like special cases either :(
     */
    if (getData->data.part == TOPLEVEL::section::config
        && (control == CONFIG::control::bankRootCC
        || control == CONFIG::control::bankCC
        || control == CONFIG::control::extendedProgramChangeCC))
    {
        getData->data.par2 = NO_MSG; // just to be sure
        if (value > 119)
            return;
        std::string text;
        if (control == CONFIG::control::bankRootCC)
        {
            text = synth->getRuntime().masterCCtest(int(value));
            if (text != "")
                getData->data.par2 = miscMsgPush(text);
            return;
        }
        if(control == CONFIG::control::bankCC)
        {
            if (value != 0 && value != 32)
                return;
            text = synth->getRuntime().masterCCtest(int(value));
            if (text != "")
                getData->data.par2 = miscMsgPush(text);
            return;
        }
        text = synth->getRuntime().masterCCtest(int(value));
        if (text != "")
            getData->data.par2 = miscMsgPush(text);
        return;
    }
}


// more work needed here :(
float InterChange::returnLimits(CommandBlock *getData)
{
    // intermediate bits of type are preserved so we know the source
    // bit 6 set is used to denote midi learnable
    // bit 7 set denotes the value is used as an integer

    int control = (int) getData->data.control;
    int npart = (int) getData->data.part;
    int kititem = (int) getData->data.kit;
    int engine = (int) getData->data.engine;
    int insert = (int) getData->data.insert;
    int parameter = (int) getData->data.parameter;
    int par2 = (int) getData->data.par2;

    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default); // catches Adj, Min, Max, Def

    //std::cout << "Limits Control " << control << " Part " << npart << "  Kit " << kititem << " Engine " << engine << "  Insert " << insert << "  Parameter " << parameter << std::endl;

    //std::cout << "Top request " << request << std::endl;

    getData->data.type &= 0x3f; //  clear top bits
    getData->data.type |= TOPLEVEL::type::Integer; // default is integer & not learnable

    if (npart == TOPLEVEL::section::config)
        return synth->getConfigLimits(getData);

    if (npart == TOPLEVEL::section::main)
        return synth->getLimits(getData);

    if (npart == TOPLEVEL::section::scales)
        return synth->microtonal.getLimits(getData);

    if (npart == TOPLEVEL::section::vector)
        return synth->getVectorLimits(getData);

    float min;
    float max;
    float def;

    if (insert == TOPLEVEL::insert::filterGroup)
    {
        filterLimit filterLimits;
        return filterLimits.getFilterLimits(getData);
    }
    // should prolly move other inserts up here

    if (kititem >= EFFECT::type::none && kititem <= EFFECT::type::dynFilter)
    {
        LimitMgr limits;
        return limits.geteffectlimits(getData);
    }

    if (npart < NUM_MIDI_PARTS)
    {
        Part *part;
        part = synth->part[npart];

        if (engine == PART::engine::subSynth && (insert == UNUSED || (insert >= TOPLEVEL::oscillatorGroup && insert <= TOPLEVEL::harmonicPhaseBandwidth)) && parameter == UNUSED)
        {
            SUBnoteParameters *subpars;
            subpars = part->kit[kititem].subpars;
            return subpars->getLimits(getData);
        }

        if (insert == TOPLEVEL::insert::partEffectSelect || (engine == UNUSED && (kititem == UNUSED || insert == TOPLEVEL::insert::kitGroup)))
            return part->getLimits(getData);

        if ((insert == TOPLEVEL::insert::kitGroup || insert == UNUSED) && parameter == UNUSED && par2 == UNUSED)
        {
            if (engine == PART::engine::addSynth || (engine >= PART::engine::addVoice1 && engine <= PART::engine::addMod8))
            {
                ADnoteParameters *adpars;
                adpars = part->kit[kititem].adpars;
                return adpars->getLimits(getData);
            }
            if (engine == PART::engine::subSynth)
            {
                SUBnoteParameters *subpars;
                subpars = part->kit[kititem].subpars;
                return subpars->getLimits(getData);
            }
            if (engine == PART::engine::padSynth)
            {
                PADnoteParameters *padpars;
                padpars = part->kit[kititem].padpars;
                return padpars->getLimits(getData);
            }
            // there may be other stuff

            min = 0;
            max = 127;
            def = 0;

            std::cout << "Using engine defaults" << std::endl;
            switch (request)
            {
                case TOPLEVEL::type::Adjust:
                    if(value < min)
                        value = min;
                    else if(value > max)
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
        if (insert >= TOPLEVEL::insert::oscillatorGroup && insert <= TOPLEVEL::insert::harmonicPhaseBandwidth)
        {
            return part->kit[0].adpars->VoicePar[0].OscilSmp->getLimits(getData);
            // we also use this for pad limits
            // as oscillator values identical
        }
        if (insert == TOPLEVEL::insert::resonanceGroup || insert == TOPLEVEL::insert::resonanceGraphInsert)
        {
            ResonanceLimits resonancelimits;
            return resonancelimits.getLimits(getData);
        }
        if (insert == TOPLEVEL::insert::LFOgroup && engine != PART::engine::subSynth && parameter <= TOPLEVEL::insertType::filter)
        {
            LFOlimit lfolimits;
            return lfolimits.getLFOlimits(getData);
        }
        if (insert == TOPLEVEL::insert::envelopeGroup)
        {
            envelopeLimit envelopeLimits;
            return envelopeLimits.getEnvelopeLimits(getData);
        }
        if (insert == TOPLEVEL::insert::envelopePoints || insert == TOPLEVEL::insert::envelopePointChange)
            return 1; // temporary solution :(
        min = 0;
        max = 127;
        def = 0;
        std::cout << "Using insert defaults" << std::endl;

            switch (request)
            {
                case TOPLEVEL::type::Adjust:
                    if(value < min)
                        value = min;
                    else if(value > max)
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

    // these two should realy be in effects
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
                getData->data.type |= TOPLEVEL::type::Learnable;
                break;
            case EFFECT::sysIns::effectNumber:
                max = 3;
                break;
            case EFFECT::sysIns::effectType:
                break;
        }

        switch (request)
        {
            case TOPLEVEL::type::Adjust:
                if(value < min)
                    value = min;
                else if(value > max)
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
                if(value < min)
                    value = min;
                else if(value > max)
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


    min = 0;
    max = 127;
    def = 0;
    std::cout << "Using unknown part number defaults" << std::endl;

    switch (request)
    {
        case TOPLEVEL::type::Adjust:
            if(value < min)
                value = min;
            else if(value > max)
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
