/*
    InterChange.cpp - General communications

    Copyright 2016-2018, Will Godfrey & others

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

    Modified May 2018
*/

#include <iostream>
#include <string>
#include <algorithm>
#include <cfloat>
#include <bitset>
#include <unistd.h>

using namespace std;

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
#include "MasterUI.h"

extern void mainRegisterAudioPort(SynthEngine *s, int portnum);
extern int mainCreateNewInstance(unsigned int forceId, bool loadState);
extern SynthEngine *firstSynth;

InterChange::InterChange(SynthEngine *_synth) :
    synth(_synth),
    fromCLI(NULL),
    toCLI(NULL),
    fromGUI(NULL),
    toGUI(NULL),
    fromMIDI(NULL),
    returnsLoopback(NULL),
    blockRead(0),
    tick(0),
    lockTime(0),
    swapRoot1(0xff),
    swapBank1(0xff),
    swapInstrument1(0xff)
{
    ;
}


bool InterChange::Init()
{
    flagsValue = 0xffffffff;
    if (!(fromCLI = jack_ringbuffer_create(sizeof(commandSize) * 256)))
    {
        synth->getRuntime().Log("InterChange failed to create 'fromCLI' ringbuffer");
        goto bail_out;
    }
    if (jack_ringbuffer_mlock(fromCLI))
    {
        synth->getRuntime().LogError("Failed to lock fromCLI memory");
        goto bail_out;
    }
    jack_ringbuffer_reset(fromCLI);

    if (!(toCLI = jack_ringbuffer_create(sizeof(commandSize) * 512)))
    {
        synth->getRuntime().Log("InterChange failed to create 'toCLI' ringbuffer");
        goto bail_out;
    }
    if (jack_ringbuffer_mlock(toCLI))
    {
        synth->getRuntime().Log("Failed to lock toCLI memory");
        goto bail_out;
    }
    jack_ringbuffer_reset(toCLI);

    if (!(fromGUI = jack_ringbuffer_create(sizeof(commandSize) * 1024)))
    {
        synth->getRuntime().Log("InterChange failed to create 'fromGUI' ringbuffer");
        goto bail_out;
    }
    if (jack_ringbuffer_mlock(fromGUI))
    {
        synth->getRuntime().Log("Failed to lock fromGUI memory");
        goto bail_out;
    }
    jack_ringbuffer_reset(fromGUI);

    if (!(toGUI = jack_ringbuffer_create(sizeof(commandSize) * 1024)))
    {
        synth->getRuntime().Log("InterChange failed to create 'toGUI' ringbuffer");
        goto bail_out;
    }
    if (jack_ringbuffer_mlock(toGUI))
    {
        synth->getRuntime().Log("Failed to lock toGUI memory");
        goto bail_out;
    }
    jack_ringbuffer_reset(toGUI);

    if (!(fromMIDI = jack_ringbuffer_create(sizeof(commandSize) * 1024)))
    {
        synth->getRuntime().Log("InterChange failed to create 'fromMIDI' ringbuffer");
        goto bail_out;
    }
    if (jack_ringbuffer_mlock(fromMIDI))
    {
        synth->getRuntime().Log("Failed to lock fromMIDI memory");
        goto bail_out;
    }
    jack_ringbuffer_reset(fromMIDI);

    if (!(returnsLoopback = jack_ringbuffer_create(sizeof(commandSize) * 1024)))
    {
        synth->getRuntime().Log("InterChange failed to create 'returnsLoopback' ringbuffer");
        goto bail_out;
    }
    if (jack_ringbuffer_mlock(returnsLoopback))
    {
        synth->getRuntime().Log("Failed to lock 'returnsLoopback' memory");
        goto bail_out;
    }
    jack_ringbuffer_reset(returnsLoopback);


    if (!synth->getRuntime().startThread(&sortResultsThreadHandle, _sortResultsThread, this, false, 0, false, "CLI"))
    {
        synth->getRuntime().Log("Failed to start CLI resolve thread");
        goto bail_out;
    }
    return true;


bail_out:
    if (fromCLI)
    {
        jack_ringbuffer_free(fromCLI);
        fromCLI = NULL;
    }
    if (toCLI)
    {
        jack_ringbuffer_free(toCLI);
        toCLI = NULL;
    }
    if (fromGUI)
    {
        jack_ringbuffer_free(fromGUI);
        fromGUI = NULL;
    }
    if (toGUI)
    {
        jack_ringbuffer_free(toGUI);
        toGUI = NULL;
    }
    if (fromMIDI)
    {
        jack_ringbuffer_free(fromMIDI);
        fromGUI = NULL;
    }
    if (returnsLoopback)
    {
        jack_ringbuffer_free(returnsLoopback);
        fromGUI = NULL;
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
        /*if (!(tick & 8191))
        {
            if (tick & 16383)
                cout << "Tick" << endl;
            else
                cout << "Tock" << endl;
        }*/

        ++ tick;
        unsigned char testRead = __sync_or_and_fetch(&blockRead, 0);
        if (lockTime == 0 && testRead != 0)
        {
            tick |= 1; // make sure it's not zero
            lockTime = tick;
        }
        else if (lockTime > 0 && testRead == 0)
            lockTime = 0;

        else if (lockTime > 0 && (tick - lockTime) > 32766)
        { // about 4 seconds - may need improving

            cout << "stuck read block cleared" << endl;
            __sync_and_and_fetch(&blockRead, 0);
            lockTime = 0;
        }

        CommandBlock getData;
        char *point;
        while (jack_ringbuffer_read_space(synth->interchange.toCLI)  >= synth->interchange.commandSize)
        {
            int toread = commandSize;
            point = (char*) &getData.bytes;
            jack_ringbuffer_read(toCLI, point, toread);
            if(getData.data.part == 0xd8) // special midi-learn - needs improving
                synth->midilearn.generalOpps(getData.data.value, getData.data.type, getData.data.control, getData.data.part, getData.data.kit, getData.data.engine, getData.data.insert, getData.data.parameter, getData.data.par2);
            else if ((getData.data.parameter & 0x80) && getData.data.parameter < 0xff)
                indirectTransfers(&getData);
            else
                resolveReplies(&getData);
        }
        usleep(80); // actually gives around 120 uS

        /*
         * The following are low priority actions initiated by,
         * but isolated from the main audio thread.
         */

        //unsigned int point = flagsReadClear();
    }
    return NULL;
}


InterChange::~InterChange()
{
    if (sortResultsThreadHandle)
        pthread_join(sortResultsThreadHandle, NULL);

    if (fromCLI)
    {
        jack_ringbuffer_free(fromCLI);
        fromCLI = NULL;
    }
    if (toCLI)
    {
        jack_ringbuffer_free(toCLI);
        toCLI = NULL;
    }
    if (fromGUI)
    {
        jack_ringbuffer_free(fromGUI);
        fromGUI = NULL;
    }
    if (toGUI)
    {
        jack_ringbuffer_free(toGUI);
        toGUI = NULL;
    }
    if (fromMIDI)
    {
        jack_ringbuffer_free(fromMIDI);
        fromGUI = NULL;
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
    //cout << "Indirect" << endl;
    bool (write) = (type & 0x40);
    if (write)
        __sync_or_and_fetch(&blockRead, 2);
    bool guiTo = false;
    string text;
    if (getData->data.par2 != NO_MSG)
        text = miscMsgPop(getData->data.par2);
    else
        text = "";
    getData->data.par2 = NO_MSG; // this may be reset later
    unsigned int tmp;
    string name;

    int switchNum = npart;
    if (control == 254 && insert !=9)
        switchNum = 256; // this is a bit hacky :(

    switch(switchNum)
    {
        case 192: // vector
        {
            switch(control)
            {
                case 8:
                    if (write)
                    {
                        synth->getRuntime().vectordata.Name[insert] = text;
                    }
                    else
                        text = synth->getRuntime().vectordata.Name[insert];
                    value = miscMsgPush(text);
                    getData->data.parameter &= 0x7f;
                    guiTo = true;
                    break;
            }
            break;
        }
        case 217: // program / bank / root
        {
            //cout << " interchange prog " << value << "  chan " << int(kititem) << "  bank " << int(engine) << "  root " << int(insert) << "  named " << int(par2) << endl;
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
            getData->data.parameter &= 0x7f;
            guiTo = true;
            break;
        }
        case 232: // scales
        {
            switch(control)
            {
                case 32: // tunings
                    text = formatScales(text);
                    value = synth->microtonal.texttotunings(text.c_str());
                    if (value > 0)
                        synth->setAllPartMaps();
                    break;
                case 33: // keyboard map
                    text = formatScales(text);
                    value = synth->microtonal.texttomapping(text.c_str());
                    if (value > 0)
                        synth->setAllPartMaps();
                    break;

                case 48: // import .scl
                    value = synth->microtonal.loadscl(setExtension(text,"scl"));
                    if(value > 0)
                    {
                        text = "";
                        char *buf = new char[100];
                        for (int i = 0; i < value; ++ i)
                        {
                            synth->microtonal.tuningtoline(i, buf, 100);
                            if (i > 0)
                                text += "\n";
                            text += string(buf);
                        }
                        delete [] buf;
                    }
                    break;
                case 49: // import .kbm
                    value = synth->microtonal.loadkbm(setExtension(text,"kbm"));
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
                                text += to_string(map);
                        }
                        getData->data.kit = synth->microtonal.PAnote;
                        getData->data.engine = synth->microtonal.Pfirstkey;
                        getData->data.insert = synth->microtonal.Pmiddlenote;
                        getData->data.parameter = synth->microtonal.Plastkey;

                    }
                    break;

                case 64: // set name
                    synth->microtonal.Pname = text;
                    break;
                case 65: // set comment
                    synth->microtonal.Pcomment = text;
                    break;
            }
            getData->data.parameter &= 0x7f;
            guiTo = true;
            break;
        }
        case 240: // main
        {
            switch (control)
            {
                case 32: // master fine detune
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
                case 35: // master key shift
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

                case 59: // export bank
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

                case 60: // import bank
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
                case 61: // delete bank and contents
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
                case 75: // bank instrument save
                {
                    if (kititem == 0xff)
                    {
                        kititem = synth->ReadBankRoot();
                        getData->data.kit = kititem;
                    }

                    if (engine == 0xff)
                    {
                        engine = synth->ReadBank();
                        getData->data.engine = engine;
                    }
                    if (value >= 64)
                    {
                        value = synth->getRuntime().currentPart;
                    }
                    //cout << "\n\nRoot " << int(kititem) << "  Bank " << int(engine) << "  Part " << int(value) << "  Slot " << int(insert) << "  Par2 " << int(par2) << " \n\n" << endl;
                    text = synth->part[value]->Pname + " to " + to_string(int(insert));
                    if (synth->getBankRef().savetoslot(kititem, engine, insert, value))
                    {
                        text = "d " + text;
                        synth->part[value]->PyoshiType = (synth->getRuntime().instrumentFormat > 1);
                    }
                    else
                        text = " FAILED " + text;
                    getData->data.parameter = value;
                    value = miscMsgPush(text);
                    break;
                }
                case 79: // named instrument save
                {
                    bool ok = true;
                    int saveType = synth->getRuntime().instrumentFormat;

                    if (saveType & 2) // Yoshimi format
                        ok = synth->part[value]->saveXML(text, true);
                    if (ok && (saveType & 1)) // legacy
                        ok = synth->part[value]->saveXML(text, false);

                    if (ok)
                    {
                        synth->addHistory(text, 1);
                        synth->part[value]->PyoshiType = (saveType & 2);
                        text = "d " + text;
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                }
                case 80:
                    vectorClear(NUM_MIDI_CHANNELS);
                    if(synth->loadPatchSetAndUpdate(text))
                        text = "ed " + text;
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case 81: // patch set save
                    if(synth->savePatchesXML(text))
                        text = "d " + text;
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case 84: // vector load
                    tmp = synth->loadVectorAndUpdate(insert, text);
                    if (tmp < 0xff)
                    {
                        getData->data.insert = tmp;
                        text = "ed " + text + " to chan " + to_string(int(tmp + 1));
                    }
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case 85: // vector save
                {
                    string oldname = synth->getRuntime().vectordata.Name[insert];
                    int pos = oldname.find("No Name");
                    if (pos >=0 && pos < 2)
                        synth->getRuntime().vectordata.Name[insert] = findleafname(text);
                    tmp = synth->saveVector(insert, text, true);
                    if (tmp == 0xff)
                        text = "d " + text;
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
                case 88: // scales load
                    if (synth->loadMicrotonal(text))
                        text = "ed " + text;
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                break;
                case 89: // scales save
                    if (synth->saveMicrotonal(text))
                        text = "d " + text;
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case 92: // state load
                    vectorClear(NUM_MIDI_CHANNELS);
                    if (synth->loadStateAndUpdate(text))
                        text = "ed " + text;
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case 93: // state save
                    if (synth->saveState(text))
                        text = "d " + text;
                    else
                        text = " FAILED " + text;
                    value = miscMsgPush(text);
                    break;
                case 94: // initialise PadSynth
                    synth->partonoffWrite(npart, -1);
                    setpadparams(parameter | (kititem << 8));
                    if (synth->part[parameter & 0x3f]->kit[kititem].padpars->export2wav(text))
                        text = "d " + text;
                    else
                        text = " FAILED some samples " + text;
                    value = miscMsgPush(text);
                    break;
                case 96: // master reset
                case 97: // include MIDI-learn
                    synth->resetAll(control & 1);
                    break;
                case 104:
                    if (value > 0 && value < 32)
                        value = mainCreateNewInstance(value, false);
                    else
                        value = mainCreateNewInstance(0, false);
                    break;
                case 105:
                    text = to_string(value) + " ";
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

                case 128: // panic stop
#ifdef REPORT_NOTES_ON_OFF
                    // note test
                    synth->getRuntime().Log("note on sent " + to_string(synth->getRuntime().noteOnSent));
                    synth->getRuntime().Log("note on seen " + to_string(synth->getRuntime().noteOnSeen));
                    synth->getRuntime().Log("note off sent " + to_string(synth->getRuntime().noteOffSent));
                    synth->getRuntime().Log("note off seen " + to_string(synth->getRuntime().noteOffSeen));
                    synth->getRuntime().Log("notes hanging sent " + to_string(synth->getRuntime().noteOnSent - synth->getRuntime().noteOffSent));
                    synth->getRuntime().Log("notes hanging seen " + to_string(synth->getRuntime().noteOnSeen - synth->getRuntime().noteOffSeen));
#endif
                    synth->ShutUp();
                    synth->Unmute();
                    break;
            }
            getData->data.parameter &= 0x7f;
            if (control != 104 && control != 105)
                guiTo = true;
            break;
        }
        case 244: // instrument / bank
        {
            switch (control)
            {
                case 4: // instrument swap select first
                {
                    if(kititem == 0xff)
                    {
                        kititem = synth->getBankRef().getCurrentBankID();
                        getData->data.kit = kititem;
                    }
                    if(engine == 0xff)
                    {
                        engine = synth->getBankRef().getCurrentRootID();
                        getData->data.engine = engine;
                    }
                    //cout << "Int swap 1 I " << int(value)  << "  B " << int(kititem) << "  R " << int(engine) << endl;
                    swapInstrument1 = insert;
                    swapBank1 = kititem;
                    swapRoot1 = engine;
                    break;
                }
                case 5: // instrument swap select second and complete
                {
                    if(kititem == 0xff)
                    {
                        kititem = synth->getBankRef().getCurrentBankID();
                        getData->data.kit = kititem;
                    }
                    if(engine == 0xff)
                    {
                        engine = synth->getBankRef().getCurrentRootID();
                        getData->data.engine = engine;
                    }
                    //cout << "Int swap 2 I " << int(insert) << "  B " << int(kititem) << "  R " << int(engine) << endl;
                    tmp = synth->bank.swapslot(swapInstrument1, insert, swapBank1, kititem, swapRoot1, engine);
                    if (tmp != 0)
                    {
                        text = " FAILED " + miscMsgPop(tmp & 0xfff);
                        value = miscMsgPush(text);
                        if (text.find("nothing", 0, 7) == string::npos)
                            synth->bank.rescanforbanks(); // might have corrupted it
                    }
                    swapInstrument1 = 0xff;
                    swapBank1 = 0xff;
                    swapRoot1 = 0xff;
                    guiTo = true;
                    break;
                }

                case 20: // bank swap select first
                    if(engine == 0xff)
                    {
                        engine = synth->getBankRef().getCurrentRootID();
                        getData->data.engine = engine;
                    }
                    swapBank1 = kititem;
                    swapRoot1 = engine;
                    break;
                case 21: // banks swap select second and complete
                    if(engine == 0xff)
                    {
                        engine = synth->getBankRef().getCurrentRootID();
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
                    swapBank1 = 0xff;
                    swapRoot1 = 0xff;
                    guiTo = true;
                    break;
            }
            getData->data.parameter &= 0x7f;
            break;
        }
        case 248: // config
        {
            switch (control)
            {
                case 32:
                    if (write)
                    {
                        synth->getRuntime().jackMidiDevice = text;
                        synth->getRuntime().configChanged = true;
                    }
                    else
                        text = synth->getRuntime().jackMidiDevice;
                    value = miscMsgPush(text);
                    break;
                case 34:
                    if (write)
                    {
                        synth->getRuntime().jackServer = text;
                        synth->getRuntime().configChanged = true;
                    }
                    else
                        text = synth->getRuntime().jackServer;
                    value = miscMsgPush(text);
                    break;
                case 48:
                    if (write)
                    {
                        synth->getRuntime().alsaMidiDevice = text;
                        synth->getRuntime().configChanged = true;
                    }
                    else
                        text = synth->getRuntime().alsaMidiDevice;
                    value = miscMsgPush(text);
                    break;
                case 50:
                    if (write)
                    {
                        synth->getRuntime().alsaAudioDevice = text;
                        synth->getRuntime().configChanged = true;
                    }
                    else
                        text = synth->getRuntime().alsaAudioDevice;
                    value = miscMsgPush(text);
                    break;
                case 80: // save config
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
                    getData->data.parameter &= 0x7f;
                        value = miscMsgPush(text);
                    break;
            }
            if (!(type & 0x20))
                guiTo = true;
            getData->data.parameter &= 0x7f;
            break;
        }
        case 256:
        {
            value = miscMsgPush(text);
            getData->data.parameter &= 0x7f;
            break;
        }
        default:
        {
            if (npart < 64)
            {
                switch(control)
                {
                    case 35:
                    {
                        if (write)
                        {
                            synth->part[npart]->Pkeyshift = value + 64;
                            synth->setPartMap(npart);
                        }
                        else
                            value = synth->part[npart]->Pkeyshift - 64;
                        getData->data.parameter &= 0x7f;
                    }
                    break;

                    case 96: // clear part
                        if (write)
                        {
                            doClearPart(npart);
                            getData->data.parameter &= 0x7f;
                        }
                        break;

                    case 104: // set padsynth parameters
                        if (write)
                        {
                            setpadparams(npart | (kititem << 8));
                            getData->data.parameter &= 0x7f;
                        }
                        break;

                    case 120: // audio destination
                        if (npart < synth->getRuntime().NumAvailableParts)
                        {
                            if (value & 2)
                            {
                                mainRegisterAudioPort(synth, npart);
                            }
                            getData->data.parameter &= 0x7f;
                        }
                        break;
                    case 222: // part / kit item names
                        if (parameter == 128)
                        {
                            if (write)
                            {
                                synth->part[npart]->Pname = text;
                                guiTo = true;
                            }
                            else
                                text = synth->part[npart]->Pname;
                        }
                        else if (synth->part[npart]->Pkitmode == true)
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
                                    text = synth->part[npart]->kit[kititem].Pname;
                            }
                        }
                        else
                            text = " FAILED Not in kit mode";
                        getData->data.parameter &= 0x7f;
                        value = miscMsgPush(text);
                        break;
                    case 223: // copyright info
                        if (write)
                        {
                            string name = synth->getRuntime().ConfigDir + "/copyright.txt";
                            if ((parameter & 0x7f) == 0) // load
                            {
                                text = miscMsgPop(loadText(name));
                                synth->part[npart]->info.Pauthor = text;
                                guiTo = true;
                            }
                            else
                            {
                                text = synth->part[npart]->info.Pauthor;
                                saveText(text, name);
                            }
                            getData->data.parameter &= 0x7f;
                            value = miscMsgPush(text);
                        }
                        break;
                }
            }
            break;
        }
    }
    __sync_and_and_fetch(&blockRead, 0xfd);
    if (getData->data.parameter < 0x80)
    {
        if (jack_ringbuffer_write_space(returnsLoopback) >= commandSize)
        {
            getData->data.value = float(value);
            if (synth->getRuntime().showGui && write && guiTo)
                getData->data.par2 = miscMsgPush(text); // pass it on to GUI

            jack_ringbuffer_write(returnsLoopback, (char*) getData->bytes, commandSize);
            if (synth->getRuntime().showGui && npart == 232 && control == 48)
            {   // loading a tuning includes a name!
                getData->data.control = 64;
                getData->data.par2 = miscMsgPush(synth->microtonal.Pname);
                jack_ringbuffer_write(returnsLoopback, (char*) getData->bytes, commandSize);
                getData->data.control = 65;
                getData->data.par2 = miscMsgPush(synth->microtonal.Pcomment);
                jack_ringbuffer_write(returnsLoopback, (char*) getData->bytes, commandSize);
            }
        }
        else
            synth->getRuntime().Log("Unable to  write to returnsLoopback buffer");
    }
}


string InterChange::formatScales(string text)
{
    text.erase(remove(text.begin(), text.end(), ' '), text.end());
    string delimiters = ",";
    size_t current;
    size_t next = -1;
    size_t found;
    string word;
    string newtext = "";
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
                string tmp (4 - found, '0'); // leading zeros
                word = tmp + word;
            }
            found = word.size();
            if ( found < 11)
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


float InterChange::readAllData(CommandBlock *getData)
{
    if(getData->data.type & 4) // these are static
    {
        //cout << "Read Control " << (int) getData->data.control << " Part " << (int) getData->data.part << "  Kit " << (int) getData->data.kit << " Engine " << (int) getData->data.engine << "  Insert " << (int) getData->data.insert << endl;
        /*
         * commandtype values
         * 0    adjusted input value
         * 1    min
         * 2    default
         * 3    max
         *
         * tryData.data.type will be updated:
         * bit 6 set    MIDI-learnable
         * bit 7 set    Is an integer value
         */
        getData->data.type &= 0xfb; // clear the error bit
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
    if (getData->data.part == 0xf0 && (control >=200 && control <= 202))
    {
        commandSendReal(getData);
        synth->fetchMeterData();
        return getData->data.value;
    }
    //cout << "Read Control " << (int) getData->data.control << " Type " << (int) getData->data.type << " Part " << (int) getData->data.part << "  Kit " << (int) getData->data.kit << " Engine " << (int) getData->data.engine << "  Insert " << (int) getData->data.insert << " Parameter " << (int) getData->data.parameter << " Par2 " << (int) getData->data.par2 << endl;
    int npart = getData->data.part;
    bool indirect = ((getData->data.parameter & 0xf0) == 0x80);
    if (npart < NUM_MIDI_PARTS && synth->part[npart]->busy)
    {
        getData->data.control = 252; // part busy message
        getData->data.kit = 0xff;
        getData->data.engine = 0xff;
        getData->data.insert = 0xff;
    }
    reTry:
    memcpy(tryData.bytes, getData->bytes, sizeof(tryData));
    while (__sync_or_and_fetch(&blockRead, 0) > 0) // just reading it
        usleep(100);
    if (indirect)
    {
        /*
         * This still isn't quite right there is a very
         * remote chance of getting garbled text :(
         */
        indirectTransfers(&tryData);
        return 0;
    }
    else
        commandSendReal(&tryData);
    if (__sync_or_and_fetch(&blockRead, 0) > 0)
        goto reTry; // it may have changed mid-process

    if ((tryData.data.type & 0x10))
        resolveReplies(&tryData);


    synth->getRuntime().finishedCLI = true; // in case it misses lines above
    return tryData.data.value;
}


void InterChange::resolveReplies(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    if (type == 0xff)
        return; // no action required
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char insertParam = getData->data.parameter;
    unsigned char insertPar2 = getData->data.par2;
    if (control == 0xfe && insertParam != 9) // special case for simple messages
    {
        synth->getRuntime().Log(miscMsgPop(lrint(value)));
        synth->getRuntime().finishedCLI = true;
        return;
    }

    showValue = true;

    Part *part;
    part = synth->part[npart];

    // this is unique and placed here to avoid Xruns
    if (npart == 0xe8 && (control <= 32 || control >= 49))
        synth->setAllPartMaps();

    bool isCli = ((type & 0x30) == 0x10); // elminate Gui redraw
    bool isGui = type & 0x20;
    char button = type & 3;
    string isValue;
    string commandName;

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
            isValue = "\n  Value      " + to_string(value);
            if (!(type & 0x80))
                isValue += "f";
        }
        string typemsg = "  Type       ";
        for (int i = 7; i > -1; -- i)
            typemsg += to_string((type >> i) & 1);
        list<string>msg;
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
    if (npart == 0xc0)
        commandName = resolveVector(getData);
    else if (npart == 0xe8)
        commandName = resolveMicrotonal(getData);
    else if (npart == 0xf8)
        commandName = resolveConfig(getData);
    else if (npart == 0xf4)
        commandName = resolveBank(getData);
    else if (npart == 0xd9 || npart == 0xf0)
        commandName = resolveMain(getData);

    else if (npart == 0xf1 || npart == 0xf2)
        commandName = resolveEffects(getData);

    else if ((kititem >= 0x80 && kititem != 0xff) || (control >= 64 && control <= 67 && kititem == 0xff))
        commandName = resolveEffects(getData);

    else if (npart >= NUM_MIDI_PARTS)
    {
        showValue = false;
        commandName = "Invalid part " + to_string(int(npart) + 1);
    }

    else if (kititem >= NUM_KIT_ITEMS && kititem < 0xff)
    {
        showValue = false;
        commandName = "Invalid kit " + to_string(int(kititem) + 1);
    }

    else if (kititem != 0 && engine != 0xff && control != 8 && part->kit[kititem].Penabled == false)
        commandName = "Part " + to_string(int(npart) + 1) + " Kit item " + to_string(int(kititem) + 1) + " not enabled";

    else if (kititem == 0xff || insert == 0x20)
    {
        if (control != 58 && kititem < 0xff && part->Pkitmode == 0)
        {
            showValue = false;
            commandName = "Part " + to_string(int(npart) + 1) + " Kitmode not enabled";
        }
        else
            commandName = resolvePart(getData);
    }
    else if (kititem > 0 && part->Pkitmode == 0)
    {
        showValue = false;
        commandName = "Part " + to_string(int(npart) + 1) + " Kitmode not enabled";
    }

    else if (engine == 2)
    {
        switch(insert)
        {
            case 0xff:
                commandName = resolvePad(getData);
                break;
            case 0:
                commandName = resolveLFO(getData);
                break;
            case 1:
                commandName = resolveFilter(getData);
                break;
            case 2:
            case 3:
            case 4:
                commandName = resolveEnvelope(getData);
                break;
            case 5:
            case 6:
            case 7:
                commandName = resolveOscillator(getData);
                break;
            case 8:
            case 9:
                commandName = resolveResonance(getData);
                break;
        }
    }

    else if (engine == 1)
    {
        switch (insert)
        {
            case 0xff:
            case 6:
            case 7:
                commandName = resolveSub(getData);
                break;
            case 1:
                commandName = resolveFilter(getData);
                break;
            case 2:
            case 3:
            case 4:
                commandName = resolveEnvelope(getData);
                break;
        }
    }

    else if (engine >= 0x80)
    {
        switch (insert)
        {
            case 0xff:
                commandName = resolveAddVoice(getData);
                break;
            case 0:
                commandName = resolveLFO(getData);
                break;
            case 1:
                commandName = resolveFilter(getData);
                break;
            case 2:
            case 3:
            case 4:
                commandName = resolveEnvelope(getData);
                break;
            case 5:
            case 6:
            case 7:
                if (engine >= 0xC0)
                    commandName = resolveOscillator(getData);
                else
                    commandName = resolveOscillator(getData);
                break;
        }
    }

    else if (engine == 0)
    {
        switch (insert)
        {
            case 0xff:
                commandName = resolveAdd(getData);
                break;
            case 0:
                commandName = resolveLFO(getData);
                break;
            case 1:
                commandName = resolveFilter(getData);
                break;
            case 2:
            case 3:
            case 4:
                commandName = resolveEnvelope(getData);
                break;
            case 8:
            case 9:
                commandName = resolveResonance(getData);
                break;
        }
    }

    string actual = "";
    if (showValue)
    {
        actual = " Value ";
        if (type & 0x80)
            actual += to_string(lrint(value));
        else
            actual += to_string(value);
    }
    if ((isGui || isCli) && button == 3)
    {
        string toSend;
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


string InterChange::resolveVector(CommandBlock *getData)
{
    int value_int = lrint(getData->data.value);
    unsigned char control = getData->data.control;
    unsigned int chan = getData->data.insert;

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Base Channel"; // local to source
            break;
        case 1:
            contstr = "Options";
            break;
        case 8:
            showValue = false;
            contstr = "Name " + miscMsgPop(value_int);
            break;

        case 16:
            contstr = "Controller";
            break;
        case 17:
            contstr = "Left Instrument";
            break;
        case 18:
            contstr = "Right Instrument";
            break;
        case 19:
        case 35:
            contstr = "Feature 0";
            break;
        case 20:
        case 36:
            contstr = "Feature 1";
            break;
        case 21:
        case 37:
            contstr = "Feature 2 ";
            break;
        case 22:
        case 38:
            contstr = "Feature 3";
            break;

        case 32:
            contstr = "Controller";
            break;
        case 33:
            contstr = "Up Instrument";
            break;
        case 34:
            contstr = "Down Instrument";
            break;

        case 96:
            showValue = false;
            if (chan > NUM_MIDI_CHANNELS)
                contstr = "all channels";
            else
                contstr = "chan " + to_string(chan + 1);
            return("Vector cleared on " + contstr);
            break;

        case 127:
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    if (control == 0)
    {
        showValue = false;
        return("Vector " + contstr + " set to " + to_string(chan + 1));
    }
    string name = "Vector Chan " + to_string(chan + 1) + " ";
    if (control == 127)
        name += " all ";
    else if (control >= 32)
        name += "Y ";
    else if(control >= 16)
        name += "X ";

    return (name + contstr);
}


string InterChange::resolveMicrotonal(CommandBlock *getData)
{
    int value = getData->data.value;
    unsigned char control = getData->data.control;

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "'A' Frequency";
            break;
        case 1:
            contstr = "'A' Note";
            break;
        case 2:
            contstr = "Invert Keys";
            break;
        case 3:
            contstr = "Key Center";
            break;
        case 4:
            contstr = "Scale Shift";
            break;
        case 8:
            contstr = "Enable Microtonal";
            break;

        case 16:
            contstr = "Enable Keyboard Mapping";
            break;
        case 17:
            contstr = "Keyboard First Note";
            break;
        case 18:
            contstr = "Keyboard Middle Note";
            break;
        case 19:
            contstr = "Keyboard Last Note";
            break;

        case 32:
            contstr = "Tuning ";
            showValue = false;
            break;
        case 33:
            contstr = "Keymap ";
            showValue = false;
            break;
        case 48:
            contstr = "Tuning Import ";
            showValue = false;
            break;
        case 49:
            contstr = "Keymap Import ";
            showValue = false;
            break;

        case 64:
            contstr = "Name: " + string(synth->microtonal.Pname);
            showValue = false;
            break;
        case 65:
            contstr = "Description: " + string(synth->microtonal.Pcomment);
            showValue = false;
            break;
        case 80:
            contstr = "Retune";
            showValue = false;
            break;

        case 96:
            contstr = "Clear all settings";
            showValue = false;
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";

    }

    if (value < 1 && control >= 32 && control <= 49)
    {
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
                if ((control & 1) == 0)
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

string InterChange::resolveConfig(CommandBlock *getData)
{
    int value_int = lrint(getData->data.value);
    unsigned char control = getData->data.control;
    bool write = getData->data.type & 0x40;
    bool value_bool = value_int > 0;
    bool yesno = false;
    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "AddSynth oscillator size";
            break;
        case 1:
            contstr = "Internal buffer size";
            break;
        case 2:
            contstr = "PadSynth interpolation ";
            if (value_bool)
                contstr += "cubic";
            else
                contstr += "linear";
            showValue = false;
            break;
        case 3:
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
        case 4:
            contstr = "XML compression";
            break;
        case 5:
            contstr = "Reports to ";
            if (value_bool)
                contstr += "console window";
            else
                contstr += "stdout";
            showValue = false;
            break;
        case 6:
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
        case 16:
            contstr += "Autoload default state";
            yesno = true;
            break;
        case 17:
            contstr += "Hide non-fatal errors";
            yesno = true;
            break;
        case 18:
            contstr += "Show splash screen";
            yesno = true;
            break;
        case 19:
            contstr += "Log instrument load times";
            yesno = true;
            break;
        case 20:
            contstr += "Log XML headers";
            yesno = true;
            break;
        case 21:
            contstr += "Save ALL XML data";
            yesno = true;
            break;
        case 22:
            contstr += "Enable GUI";
            yesno = true;
            break;
        case 23:
            contstr += "Enable CLI";
            yesno = true;
            break;
        case 24:
            contstr += "Enable Auto Instance";
            yesno = true;
            break;

        case 32:
            contstr += "JACK MIDI source: ";
            contstr += miscMsgPop(value_int);
            showValue = false;
            break;
        case 33:
            contstr += "Start with JACK MIDI";
            yesno = true;
            break;
        case 34:
            contstr += "JACK server: ";
            contstr += miscMsgPop(value_int);
            showValue = false;
            break;
        case 35:
            contstr += "Start with JACK audio";
            yesno = true;
            break;
        case 36:
            contstr += "Auto-connect to JACK server";
            yesno = true;
            break;

        case 48:
            contstr += "ALSA MIDI source: ";
            contstr += miscMsgPop(value_int);
            showValue = false;
            break;
        case 49:
            contstr += "Start with ALSA MIDI";
            yesno = true;
            break;
        case 50:
            contstr += "ALSA audio device: ";
            contstr += miscMsgPop(value_int);
            showValue = false;
            break;
        case 51:
            contstr += "Start with ALSA audio";
            yesno = true;
            break;
        case 52:
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

        /*case 64:
            contstr += "Enable bank root change";
            yesno = true;
            break;*/
        case 65:
            contstr += "Bank root CC";
            break;

        case 67:
            contstr += "Bank CC";
            break;
        case 68:
            contstr += "Enable program change";
            yesno = true;
            break;
        case 69:
            contstr += "Program change enables part";
            yesno = true;
            break;
        /*case 70:
            contstr += "Enable extended program change";
            yesno = true;
            break;*/
        case 71:
            contstr += "CC for extended program change";
            break;
        case 72:
            contstr += "Ignore 'reset all CCs'";
            yesno = true;
            break;
        case 73:
            contstr += "Log incoming CCs";
            yesno = true;
            break;
        case 74:
            contstr += "Auto-open GUI MIDI-learn editor";
            yesno = true;
            break;

        case 75:
            contstr += "Enable NRPN";
            yesno = true;
            break;

        case 80:
        {
            string name = miscMsgPop(value_int);
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


string InterChange::resolveBank(CommandBlock *getData)
{
    int value_int = lrint(getData->data.value);
    int control = getData->data.control;
    int kititem = getData->data.kit;
    int engine = getData->data.engine;
    int insert = getData->data.insert;
    string name = miscMsgPop(value_int);
    string contstr = "";
    showValue = false;
    switch(control)
    {
        case 4:
            contstr = "Set Instrument ID " + to_string(insert) + "  Bank ID " + to_string(kititem) + "  Root ID " + to_string(engine) + " for swap";
            break;
        case 5:
            if (name == "")
                name = "ped with Instrument ID " + to_string(insert) + "  Bank ID " + to_string(kititem) + "  Root ID " + to_string(engine);
            contstr = "Swap" + name;
            break;

        case 20:
            contstr = "Set Bank ID " + to_string(kititem) + "  Root ID " + to_string(engine) + " for swap";
            break;
        case 21:
            if (name == "")
                name = "ped with Bank ID " + to_string(kititem) + "  Root ID " + to_string(engine);
            contstr = "Swap" + name;
            break;
        default:
            contstr = "Unrecognised";
            break;
    }
    return ("Bank " + contstr);
}

string InterChange::resolveMain(CommandBlock *getData)
{
    float value = getData->data.value;
    int value_int = lrint(value);
//    bool write = ((getData->data.type & 0x40) != 0);
    unsigned char control = getData->data.control;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
//    unsigned char par2 = getData->data.par2;
    string name;
    string contstr = "";
    if (getData->data.part == 0xd9) // MIDI
    {
        switch (control)
        {
            case 0:
            case 1:
                showValue = false;
                break;
            case 2:
                contstr = "CC " + to_string(int(engine)) + " ";
                break;
            case 8:
                showValue = false;
                contstr = miscMsgPop(value_int);
                break;
        }
        return contstr;
    }

    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;

        case 14:
            showValue = false;
            contstr = "Part Number " + to_string(value_int + 1);
            break;
        case 15:
            contstr = "Available Parts";
            break;

        case 32:
            contstr = "Detune";
            break;
        case 35:
            contstr = "Key Shift";
            break;

        case 48:
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
        case 49:
            showValue = false;
            contstr = "Chan 'solo' Switch CC ";
            if (value_int > 127)
                contstr += "undefined - set mode first";
            else
                contstr += to_string(value_int);
            break;
        case 59:
            showValue = false;
            contstr = "Bank Export" + miscMsgPop(value_int);
            break;
        case 60:
            showValue = false;
            contstr = "Bank Import" + miscMsgPop(value_int);
            break;
        case 61:
            showValue = false;
            contstr = "Bank delete" + miscMsgPop(value_int);
            break;
        case 75:
            showValue = false;
            contstr = "Bank Slot Save" + miscMsgPop(value_int);
            break;

        case 79:
            showValue = false;
            contstr = "Instrument Save" + miscMsgPop(value_int);
            break;

        case 80:
            showValue = false;
            contstr = "Patchset Load" + miscMsgPop(value_int);
            break;

        case 81:
            showValue = false;
            contstr = "Patchset Save" + miscMsgPop(value_int);
            break;

        case 84:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "Vector Load" + name;
            break;

        case 85:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "Vector Save" + name;
            break;

        case 88:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "Scale Load" + name;
            break;

        case 89:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "Scale Save" + name;
            break;

        case 92:
            showValue = false;
            name = miscMsgPop(value_int);
            contstr = "State Load" + name;
            break;

        case 93:
            showValue = false;
            contstr = "State Save" + miscMsgPop(value_int);
            break;

        case 94:
            showValue = false;
            contstr = "PadSynth Samples Save" + miscMsgPop(value_int);
            break;

        case 96: // doMasterReset
            showValue = false;
            contstr = "Reset All";
            break;
        case 97:
            showValue = false;
            contstr = "Reset All including MIDI-learn";
            break;

        case 104:
            showValue = false;
            contstr = "Start new instance " + to_string(value_int);
            break;
        case 105: // close instance
            showValue = false;
            contstr = "Close instance - " + miscMsgPop(value_int);
            break;

        case 128:
            showValue = false;
            contstr = "Sound Stopped";
            break;

        case 200:
            showValue = false;
            contstr = "Part " + to_string(int(kititem));
            if (value < 0.0f)
                contstr += " silent ";
            contstr += (" peak level " + to_string(value));
            break;
        case 201:
            showValue = false;
            if(kititem == 1)
                contstr = "Right";
            else
                contstr = "Left";
            contstr += (" peak level " + to_string(value));
            break;
        case 202:
            showValue = false;
            if(kititem == 1)
                contstr = "Right";
            else
                contstr = "Left";
            contstr += (" RMS level " + to_string(value));
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Main " + contstr);
}


string InterChange::resolvePart(CommandBlock *getData)
{
    int value_int = lrint(getData->data.value);
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char parameter = getData->data.parameter;
    //unsigned char par2 = getData->data.par2;
    unsigned char effNum = engine;

    bool kitType = (insert == 0x20);
    bool value_bool = value_int > 0;
    bool yesno = false;

    if (control == 255)
        return "Number of parts";

    string kitnum;
    if (kitType)
        kitnum = " Kit " + to_string(kititem + 1) + " ";
    else
        kitnum = " ";

    string name = "";
    if (control >= 0x80)
    {
        if (control < 0xc0)
        {
            name = "Controller ";
            if (control >= 0xa0)
                name += "Portamento ";
        }
        else if (control < 0xdc)
            name = "MIDI ";
    }
    else if (kititem < 0xff)
    {
        switch (engine)
        {
            case 0:
                name = "AddSynth ";
                break;
            case 1:
                name = "SubSynth ";
                break;
            case 2:
                name = "PadSynth ";
                break;
        }
    }

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;
        case 1:
            contstr = "Vel Sens";
            break;
        case 2:
            contstr = "Panning";
            break;
        case 4:
            contstr = "Vel Offset";
            break;
        case 5:
            showValue = false;
            contstr = "Midi CH - " + to_string(value_int + 1);
            if (value_int >= NUM_MIDI_CHANNELS * 2)
                contstr += " Midi ignored";
            else if (value_int >= NUM_MIDI_CHANNELS)
                contstr = contstr + " Note off only on CH " + to_string(value_int + 1 - NUM_MIDI_CHANNELS);
            break;
        case 6:
            showValue = false;
            contstr = "Mode - ";
            if (value_int == 0)
                contstr += "Poly";
            else if (value_int == 1)
                contstr += "Mono";
            else if (value_int >= 2)
                contstr += "Legato";
            break;
        case 7:
            contstr = "Portamento Enable";
            yesno = true;
            break;
        case 8:
            contstr = "Enable";
            if (!kitType)
            {
                switch(engine)
                {
                    case 0:
                        contstr = "AddSynth " + contstr;
                        break;
                    case 1:
                        contstr = "SubSynth " + contstr;
                        break;
                    case 2:
                        contstr = "PadSynth " + contstr;
                        break;
                }
            }
            break;
        case 9:
            if (kitType)
                contstr = "Mute";
            break;

        case 16:
            contstr = "Min Note";
            break;
        case 17:
            contstr = "Max Note";
            break;
        case 18: // always return actual value
            contstr = "Min To Last";
            break;
        case 19: // always return actual value
            contstr = "Max To Last";
            break;
        case 20:
            contstr = "Reset Key Range";
            break;

        case 24:
            if (kitType)
                contstr = "Effect Number";
            break;

        case 33:
            contstr = "Key Limit";
            break;
        case 35:
            contstr = "Key Shift";
            break;

        case 40:
            contstr = "Effect Send 1";
            break;
        case 41:
            contstr = "Effect Send 2";
            break;
        case 42:
            contstr = "Effect Send 3";
            break;
        case 43:
            contstr = "Effect Send 4";
            break;

        case 48:
            contstr = "Humanise";
            break;

        case 57:
            contstr = "Drum Mode";
            break;
        case 58:
            contstr = "Kit Mode";
            break;

        case 64: // local to source
            contstr = "Effect Number";
            break;
        case 65:
            contstr = "Effect " + to_string(effNum + 1) + " Type";
            break;
        case 66:
            contstr = "Effect " + to_string(effNum + 1) + " Destination";
            break;
        case 67:
            contstr = "Bypass Effect "+ to_string(effNum + 1);
            break;

        case 96: // doClearPart
            contstr = "Set Default Instrument";
            break;

        case 120:
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
                default:
                    contstr += "main";
                    break;
            }
            break;

        case 128:
            contstr = "Vol Range"; // not the *actual* volume
            break;
        case 129:
            contstr = "Vol Enable";
            break;
        case 130:
            contstr = "Pan Width";
            break;
        case 131:
            contstr = "Mod Wheel Depth";
            break;
        case 132:
            contstr = "Exp Mod Wheel";
            break;
        case 133:
            contstr = "Bandwidth depth";
            break;
        case 134:
            contstr = "Exp Bandwidth";
            break;
        case 135:
            contstr = "Expression Enable";
            break;
        case 136:
            contstr = "FM Amp Enable";
            break;
        case 137:
            contstr = "Sustain Ped Enable";
            break;
        case 138:
            contstr = "Pitch Wheel Range";
            break;
        case 139:
            contstr = "Filter Q Depth";
            break;
        case 140:
            contstr = "Filter Cutoff Depth";
            break;
        case 141:
            yesno = true;
            contstr = "Breath Control";
            break;

        case 144:
            contstr = "Res Cent Freq Depth";
            break;
        case 145:
            contstr = "Res Band Depth";
            break;

        case 160:
            contstr = "Time";
            break;
        case 161:
            contstr = "Tme Stretch";
            break;
        case 162:
            contstr = "Threshold";
            break;
        case 163:
            contstr = "Threshold Type";
            break;
        case 164:
            contstr = "Prop Enable";
            break;
        case 165:
            contstr = "Prop Rate";
            break;
        case 166:
            contstr = "Prop depth";
            break;
        case 168:
            contstr = "Enable";
            break;

        case 192:
            contstr = "Modulation";
            break;
        case 194:
            contstr = "Expression";
            break;
        case 197:
            contstr = "Filter Q";
            break;
        case 198:
            contstr = "Filter Cutoff";
            break;
        case 199:
            contstr = "Bandwidth";
            break;

        case 222:
            showValue = false;
            contstr = "Name is: " + miscMsgPop(value_int);
            break;
        case 223:
            showValue = false;
            contstr = "Copyright ";
            if (parameter == 0)
                contstr += "load:\n";
            else
                contstr += "save:\n";
            contstr += miscMsgPop(value_int);
            break;
        case 224:
            contstr = "Clear controllers";
            break;

        case 252:
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
    return ("Part " + to_string(npart + 1) + kitnum + name + contstr);
}


string InterChange::resolveAdd(CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;

    string name = "";
    switch (control & 0x70)
    {
        case 0:
            name = " Amplitude ";
            break;
        case 32:
            name = " Frequency ";
            break;
    }

    string contstr = "";

    switch (control)
    {
        case 0:
            contstr = "Volume";

            break;
        case 1:
            contstr = "Vel Sens";
            break;
        case 2:
            contstr = "Panning";
            break;

        case 32:
            contstr = "Detune";
            break;

        case 35:
            contstr = "Octave";
            break;
        case 36:
            contstr = "Det type";
            break;
        case 37:
            contstr = "Coarse Det";
            break;
        case 39:
            contstr = "Rel B Wdth";
            break;

        case 112:
            contstr = "Stereo";
            break;
        case 113:
            contstr = "Rnd Grp";
            break;

        case 120:
            contstr = "De Pop";
            break;
        case 121:
            contstr = "Punch Strngth";
            break;
        case 122:
            contstr = "Punch Time";
            break;
        case 123:
            contstr = "Punch Strtch";
            break;
        case 124:
            contstr = "Punch Vel";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " AddSynth " + name + contstr);
}


string InterChange::resolveAddVoice(CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    int nvoice = engine & 0x1f;

    string name = "";
    switch (control & 0xf0)
    {
        case 0:
            name = " Amplitude ";
            break;
        case 16:
            name = " Modulator ";
            break;
        case 32:
            name = " Frequency ";
            break;
        case 48:
            name = " Unison ";
            break;
        case 64:
            name = " Filter ";
            break;
        case 80:
            name = " Modulator Amp ";
            break;
        case 96:
            name = " Modulator Freq ";
            break;
        case 112:
            name = " Modulator Osc ";
            break;
    }

    string contstr = "";

    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;
        case 1:
            contstr = "Vel Sens";
            break;
        case 2:
            contstr = "Panning";
            break;
        case 4:
            contstr = "Minus";
            break;
        case 8:
            contstr = "Enable Env";
            break;
        case 9:
            contstr = "Enable LFO";
            break;

        case 16:
            contstr = "Type";
            break;
        case 17:
            contstr = "Extern Mod";
            break;

        case 32:
            contstr = "Detune";
            break;
        case 33:
            contstr = "Eq T";
            break;
        case 34:
            contstr = "440Hz";
            break;
        case 35:
            contstr = "Octave";
            break;
        case 36:
            contstr = "Det type";
            break;
        case 37:
            contstr = "Coarse Det";
            break;
        case 38:
            contstr = "Bend Adj";
            break;
        case 39:
            contstr = "Offset Hz";
            break;
        case 40:
            contstr = "Enable Env";
            break;
        case 41:
            contstr = "Enable LFO";
            break;

        case 48:
            contstr = "Freq Spread";
            break;
        case 49:
            contstr = "Phase Rnd";
            break;
        case 50:
            contstr = "Stereo";
            break;
        case 51:
            contstr = "Vibrato";
            break;
        case 52:
            contstr = "Vib Speed";
            break;
        case 53:
            contstr = "Size";
            break;
        case 54:
            contstr = "Invert";
            break;
        case 56:
            contstr = "Enable";
            break;

        case 64:
            contstr = "Bypass Global";
            break;
        case 68:
            contstr = "Enable";
            break;
        case 72:
            contstr = "Enable Env";
            break;
        case 73:
            contstr = "Enable LFO";
            break;

        case 80:
            contstr = "Volume";
            break;
        case 81:
            contstr = "V Sense";
            break;
        case 82:
            contstr = "F Damp";
            break;
        case 88:
            contstr = "Enable Env";
            break;

        case 96:
            break;
        case 98:
            contstr = "440Hz";
            break;
        case 99:
            contstr = "Octave";
            break;
        case 100:
            contstr = "Det type";
            break;
        case 101:
            contstr = "Coarse Det";
            break;
        case 104:
            contstr = "Enable Env";
            break;

        case 112:
            contstr = " Phase";
            break;
        case 113:
            contstr = " Source";
            break;

        case 128:
            contstr = " Delay";
            break;
        case 129:
            contstr = " Enable";
            break;
        case 130:
            contstr = " Resonance Enable";
            break;
        case 136:
            contstr = " Osc Phase";
            break;
        case 137:
            contstr = " Osc Source";
            break;
        case 138:
            contstr = " Sound type";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " Add Voice " + to_string(nvoice + 1) + name + contstr);
}


string InterChange::resolveSub(CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char insert = getData->data.insert;

    if (insert == 6 || insert == 7)
    {
        string Htype;
        if (insert == 6)
            Htype = " Amplitude";
        else
            Htype = " Bandwidth";

        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " SubSynth Harmonic " + to_string(control + 1) + Htype);
    }

    string name = "";
    switch (control & 0x70)
    {
        case 0:
            name = " Amplitude ";
            break;
        case 16:
            name = " Bandwidth ";
            break;
        case 32:
            name = " Frequency ";
            break;
        case 48:
            name = " Overtones ";
            break;
        case 64:
            name = " Filter ";
            break;
    }

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;
        case 1:
            contstr = "Vel Sens";
            break;
        case 2:
            contstr = "Panning";
            break;

        case 16:
            contstr = "";
            break;
        case 17:
            contstr = "Band Scale";
            break;
        case 18:
            contstr = "Env Enab";
            break;

        case 32:
            contstr = "Detune";
            break;
        case 33:
            contstr = "Eq T";
            break;
        case 34:
            contstr = "440Hz";
            break;
        case 35:
            contstr = "Octave";
            break;
        case 36:
            contstr = "Det type";
            break;
        case 37:
            contstr = "Coarse Det";
            break;
        case 38:
            contstr = "Bend Adj";
            break;
        case 39:
            contstr = "Offset Hz";
            break;
        case 40:
            contstr = "Env Enab";
            break;

        case 48:
            contstr = "Par 1";
            break;
        case 49:
            contstr = "Par 2";
            break;
        case 50:
            contstr = "Force H";
            break;
        case 51:
            contstr = "Position";
            break;

        case 64:
            contstr = "Enable";
            break;

        case 80:
            contstr = "Filt Stages";
            break;
        case 81:
            contstr = "Mag Type";
            break;
        case 82:
            contstr = "Start";
            break;

        case 96:
            contstr = "Clear Harmonics";
            break;

        case 112:
            contstr = "Stereo";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " SubSynth " + name + contstr);
}


string InterChange::resolvePad(CommandBlock *getData)
{
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    bool write = (type & 0x40) > 0;

    string name = "";
    switch (control & 0x70)
    {
        case 0:
            name = " Amplitude ";
            break;
        case 16:
            name = " Bandwidth ";
            break;
        case 32:
            name = " Frequency ";
            break;
        case 48:
            name = " Overtones ";
            break;
        case 64:
            name = " Harmonic Base ";
            break;
        case 80:
            name = " Harmonic Samples ";
            break;
    }

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;
        case 1:
            contstr = "Vel Sens";
            break;
        case 2:
            contstr = "Panning";
            break;

        case 16:
            contstr = "Bandwidth";
            break;
        case 17:
            contstr = "Band Scale";
            break;
        case 19:
            contstr = "Spect Mode";
            break;

        case 32:
            contstr = "Detune";
            break;
        case 33:
            contstr = "Eq T";
            break;
        case 34:
            contstr = "440Hz";
            break;
        case 35:
            contstr = "Octave";
            break;
        case 36:
            contstr = "Det type";
            break;
        case 37:
            contstr = "Coarse Det";
            break;

        case 38:
            contstr = "Bend Adj";
            break;
        case 39:
            contstr = "Offset Hz";
            break;

        case 48:
            contstr = "Overt Par 1";
            break;
        case 49:
            contstr = "Overt Par 2";
            break;
        case 50:
            contstr = "Force H";
            break;
        case 51:
            contstr = "Position";
            break;

        case 64:
            contstr = "Width";
            break;
        case 65:
            contstr = "Freq Mult";
            break;
        case 66:
            contstr = "Str";
            break;
        case 67:
            contstr = "S freq";
            break;
        case 68:
            contstr = "Size";
            break;
        case 69:
            contstr = "Type";
            break;
        case 70:
            contstr = "Halves";
            break;
        case 71:
            contstr = "Amp Par 1";
            break;
        case 72:
            contstr = "Amp Par 2";
            break;
        case 73:
            contstr = "Amp Mult";
            break;
        case 74:
            contstr = "Amp Mode";
            break;
        case 75:
            contstr = "Autoscale";
            break;

        case 80:
            contstr = "Base";
            break;
        case 81:
            contstr = "samp/Oct";
            break;
        case 82:
            contstr = "Num Oct";
            break;
        case 83:
            break;

        case 104:// set pad parameters
            showValue = false;
            contstr = "Changes Applied";
            break;

        case 112:
            contstr = "Stereo";
            break;

        case 120:
            contstr = "De Pop";
            break;
        case 121:
            contstr = "Punch Strngth";
            break;
        case 122:
            contstr = "Punch Time";
            break;
        case 123:
            contstr = "Punch Strtch";
            break;
        case 124:
            contstr = "Punch Vel";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    string isPad = "";

    if (write && ((control >= 16 && control <= 19) || (control >= 48 && control <= 83)))
        isPad += " - Need to Apply";
    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + " PadSynth " + name + contstr + isPad);
}


string InterChange::resolveOscillator(CommandBlock *getData)
{
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    bool write = (type & 0x40) > 0;

    string isPad = "";
    string eng_name;
    if (engine == 2)
    {
        eng_name = " Padsysnth";
        if (write)
            isPad = " - Need to Apply";
    }
    else
    {
        eng_name = " Add Voice " + to_string((engine & 0x3f) + 1);
        if (engine & 0x40)
            eng_name += " Modulator";
    }

    if (insert == 6)
    {
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + eng_name + " Harmonic " + to_string((int)control + 1) + " Amplitude" + isPad);
    }
    else if(insert == 7)
    {
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + eng_name + " Harmonic " + to_string((int)control + 1) + " Phase" + isPad);
    }

    string name = "";
    switch (control & 0x70)
    {
        case 0:
            name = " Oscillator";
            break;
        case 16:
            name = " Base Funct";
            break;
        case 32:
            name = " Base Mods";
            break;
        case 64:
            name = " Harm Mods";
            break;
    }

    string contstr;
    switch (control)
    {
        case 0:
            contstr = " Random";
            break;
        case 1:
            contstr = " Mag Type";
            break;
        case 2:
            contstr = " Harm Rnd";
            break;
        case 3:
            contstr = " Harm Rnd Type";
            break;

        case 16:
            contstr = " Par";
            break;
        case 17:
            contstr = " Type";
            break;
        case 18:
            contstr = " Mod Par 1";
            break;
        case 19:
            contstr = " Mod Par 2";
            break;
        case 20:
            contstr = " Mod Par 3";
            break;
        case 21:
            contstr = " Mod Type";
            break;

        case 32: // this is local to the source
            break;
        case 33:
            contstr = " Osc As Base";
            break;

        case 34:
            contstr = " Waveshape Par";
            break;
        case 35:
            contstr = " Waveshape Type";
            break;

        case 36:
            contstr = " Osc Filt Par 1";
            break;
        case 37:
            contstr = " Osc Filt Par 2";
            break;
        case 38:
            contstr = " Osc Filt B4 Waveshape";
            break;
        case 39:
            contstr = " Osc Filt Type";
            break;

        case 40:
            contstr = " Osc Mod Par 1";
            break;
        case 41:
            contstr = " Osc Mod Par 2";
            break;
        case 42:
            contstr = " Osc Mod Par 3";
            break;
        case 43:
            contstr = " Osc Mod Type";
            break;

        case 44:
            contstr = " Osc Spect Par";
            break;
        case 45:
            contstr = " Osc Spect Type";
            break;

        case 64:
            contstr = " Shift";
            break;
        case 65:
            contstr = " Reset";
            break;
        case 66:
            contstr = " B4 Waveshape & Filt";
            break;

        case 67:
            contstr = " Adapt Param";
            break;
        case 68:
            contstr = " Adapt Base Freq";
            break;
        case 69:
            contstr = " Adapt Power";
            break;
        case 70:
            contstr = " Adapt Type";
            break;

        case 96:
            contstr = " Clear Harmonics";
            break;
        case 97:
            contstr = " Conv To Sine";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + eng_name + name + contstr + isPad);
}


string InterChange::resolveResonance(CommandBlock *getData)
{
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    bool write = (type & 0x40) > 0;

    string name;
    string isPad = "";
    if (engine == 2)
    {
        name = " PadSynth";
        if (write)
            isPad = " - Need to Apply";
    }
    else
        name = " AddSynth";

    if (insert == 9)
    {
        if (write == true && engine == 2)
            isPad = " - Need to Apply";
        return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + name + " Resonance Point " + to_string(control + 1) + isPad);
    }

    if (write == true && engine == 2 && control != 104)
        isPad = " - Need to Apply";
    string contstr;
    switch (control)
    {
        case 0:
            contstr = "Max dB";
            break;
        case 1:
            contstr = "Center Freq";
            break;
        case 2:
            contstr = "Octaves";
            break;

        case 8:
            contstr = "Enable";
            break;

        case 10:
            contstr = "Random";
            break;

        case 20:
            contstr = "Interpolate Peaks";
            break;
        case 21:
            contstr = "Protect Fundamental";
            break;

        case 96:
            contstr = "Clear";
            break;
        case 97:
            contstr = "Smooth";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + name + " Resonance " + contstr + isPad);
}


string InterChange::resolveLFO(CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insertParam = getData->data.parameter;

    string name;
    string lfo;

    if (engine == 0)
        name = " AddSynth";
    else if (engine == 2)
        name = " PadSynth";
    else if (engine >= 0x80)
    {
        int nvoice = engine & 0x3f;
        name = " Add Voice " + to_string(nvoice + 1);
    }

    switch (insertParam)
    {
        case 0:
            lfo = " Amp";
            break;
        case 1:
            lfo = " Freq";
            break;
        case 2:
            lfo = " Filt";
            break;
    }

    string contstr;
    switch (control)
    {
        case 0:
            contstr = "Freq";
            break;
        case 1:
            contstr = "Depth";
            break;
        case 2:
            contstr = "Delay";
            break;
        case 3:
            contstr = "Start";
            break;
        case 4:
            contstr = "AmpRand";
            break;
        case 5:
            contstr = "Type";
            break;
        case 6:
            contstr = "Cont";
            break;
        case 7:
            contstr = "FreqRand";
            break;
        case 8:
            contstr = "Stretch";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + name + lfo + " LFO " + contstr);
}


string InterChange::resolveFilter(CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;

    int nseqpos = getData->data.parameter;
    int nformant = getData->data.parameter;
    int nvowel = getData->data.par2;

    string name;

    if (engine == 0)
        name = " AddSynth";
    else if (engine == 1)
        name = " SubSynth";
    else if (engine == 2)
        name = " PadSynth";
    else if (engine >= 0x80)
        name = " Adsynth Voice " + to_string((engine & 0x3f) + 1);
    string contstr;
    switch (control)
    {
        case 0:
            contstr = "C_Freq";
            break;
        case 1:
            contstr = "Q";
            break;
        case 2:
            contstr = "FreqTrk";
            break;
        case 3:
            contstr = "VsensA";
            break;
        case 4:
            contstr = "Vsens";
            break;
        case 5:
            contstr = "gain";
            break;
        case 6:
            contstr = "Stages";
            break;
        case 7:
            contstr = "Filt Type";
            break;
        case 8:
            contstr = "An Type";
            break;
        case 9:
            contstr = "SV Type";
            break;
        case 10:
            contstr = "Fre Trk Offs";
            break;
        case 16:
            contstr = "Form Fr Sl";
            break;
        case 17:
            contstr = "Form Vw Cl";
            break;
        case 18:
            contstr = "Form Freq";
            break;
        case 19:
            contstr = "Form Q";
            break;
        case 20:
            contstr = "Form Amp";
            break;
        case 21:
            contstr = "Form Stretch";
            break;
        case 22:
            contstr = "Form Cent Freq";
            break;
        case 23:
            contstr = "Form Octave";
            break;

        case 32:
            contstr = "Formants";
            break;
        case 33:
            contstr = "Vowel Num";
            break;
        case 34:
            contstr = "Formant Num";
            break;
        case 35:
            contstr = "Seq Size";
            break;
        case 36:
            contstr = "Seq Pos";
            break;
        case 37:
            contstr = "Vowel";
            break;
        case 38:
            contstr = "Neg Input";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }
    string extra = "";
    if (control >= 18 && control <= 20)
        extra ="Vowel " + to_string(nvowel) + " Formant " + to_string(nformant) + " ";
    else if (control == 37)
        extra = "Seq Pos " + to_string(nseqpos) + " ";

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(kititem + 1) + name + " Filter " + extra + contstr);
}


string InterChange::resolveEnvelope(CommandBlock *getData)
{
    int value = lrint(getData->data.value);
    bool write = (getData->data.type & 0x40) > 0;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char insertParam = getData->data.parameter;
    int par2 = getData->data.par2;

    string env;
    string name;
    if (engine == 0)
        name = " AddSynth";
    else if (engine == 1)
        name = " SubSynth";

    else if (engine == 2)
        name = " PadSynth";
    else if (engine >= 0x80)
    {
        name = " Add Voice ";
        int nvoice = engine & 0x3f;
        name += to_string(nvoice + 1);
        if (engine >= 0xC0)
            name += " Modulator";
    }

    switch(insertParam)
    {
        case 0:
            env = " Amp";
            break;
        case 1:
            env = " Freq";
            break;
        case 2:
            env = " Filt";
            break;
        case 3:
            env = " B.Width";
            break;
    }

    if (insert == 3)
    {
        if (!write)
        {
            return ("Freemode add/remove is write only. Current points " + to_string(int(par2)));
        }
        if (par2 != NO_MSG)
            return ("Part " + to_string(int(npart + 1)) + " Kit " + to_string(int(kititem + 1)) + name  + env + " Env Added Freemode Point " + to_string(int((control & 0x3f) + 1)) + " X increment " + to_string(int(par2)) + " Y");
        else
        {
            showValue = false;
            return ("Part " + to_string(int(npart + 1)) + " Kit " + to_string(int(kititem + 1)) + name  + env + " Env Removed Freemode Point " +  to_string(int(control + 1)) + "  Remaining " +  to_string(value));
        }
    }

    if (insert == 4)
    {
        return ("Part " + to_string(int(npart + 1)) + " Kit " + to_string(int(kititem + 1)) + name  + env + " Env Freemode Point " +  to_string(int(control + 1)) + " X increment " + to_string(int(par2)) + " Y");
    }

    string contstr;
    switch (control)
    {
        case 0:
            contstr = "A val";
            break;
        case 1:
            contstr = "A dt";
            break;
        case 2:
            contstr = "D val";
            break;
        case 3:
            contstr = "D dt";
            break;
        case 4:
            contstr = "S val";
            break;
        case 5:
            contstr = "R dt";
            break;
        case 6:
            contstr = "R val";
            break;
        case 7:
            contstr = "Stretch";
            break;

        case 16:
            contstr = "frcR";
            break;
        case 17:
            contstr = "L";
            break;

        case 24:
            contstr = "Edit";
            break;

        case 32:
            contstr = "Freemode";
            break;
        case 34:
            contstr = "Points";
            contstr += to_string((int) par2);
            break;
        case 35:
            contstr = "Sust";
            break;

        default:
            showValue = false;
            contstr = "Unrecognised";
    }

    return ("Part " + to_string(npart + 1) + " Kit " + to_string(int(kititem + 1)) + name  + env + " Env " + contstr);
}


string InterChange::resolveEffects(CommandBlock *getData)
{
    int value = lrint(getData->data.value);
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char effnum = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char parameter = getData->data.parameter;

    string name;
    string actual;
    if (npart == 0xf1)
        name = "System";
    else if (npart == 0xf2)
        name = "Insert";
    else
        name = "Part " + to_string(npart + 1);

    if (kititem == 0x88 && getData->data.insert < 0xff)
    {
        if (npart == 0xf1)
            name = "System";
        else if (npart == 0xf2)
            name = "Insert";
        else name = "Part " + to_string(npart + 1);
        name += " Effect " + to_string(effnum + 1);

        return (name + " DynFilter ~ Filter Internal Control " + to_string(control));
    }

    name += " Effect " + to_string(effnum + 1);

    string effname = "";
    if (npart < NUM_MIDI_PARTS && (control == 64 || control == 66 || control == 67))
    {
        if (control == 64)
            name = "Set " + name;
        else if (control == 66)
        {
            effname = " sent to ";
            if (value == 0)
                effname += "next effect";
            else if (value == 1)
                effname += "part out";
            else if (value == 1)
                effname += "dry out";
        }
        if (control == 67)
            effname = " bypassed";
        else
            showValue = false;
        return (name + effname);
    }
    else if (npart > 0xf0 && kititem == 0xff)
    {
        string contstr;
        string second = "";
        if (npart == 0xf1 && insert == 16)
        {
                name = "System ";
                contstr = "from Effect " + to_string(effnum + 1);
                second = " to Effect " + to_string(control + 1);
                return (name + contstr + second);
        }
        if (npart == 0xf2 && control == 2)
        {
            contstr = " To ";
            if (value == -2)
                contstr += "Master out";
            else if (value == -1)
                contstr = " Off";
            else
            {
                contstr += "Part ";
                second = to_string(value + 1);
            }
            showValue = false;
            return ("Send " + name + contstr + second);
        }
        if (control == 0)
        {
            name = "Set " + name;
            showValue = false;
            return (name + effname);
        }
    }
    string contstr = "";
    if ((npart < NUM_MIDI_PARTS && control == 65) || (npart > 0xf0 && kititem == 0xff && control == 1))
    {
        name += " set to";
        kititem = value;
        showValue = false;
    }
    else
        contstr = " Control " + to_string(control + 1);

    switch (kititem & 0x7f)
    {
        case 0:
            effname = " None";
            contstr = " ";
            break;
        case 1:
            effname = " Reverb";
            break;
        case 2:
            effname = " Echo";
            break;
        case 3:
            effname = " Chorus";
            break;
        case 4:
            effname = " Phaser";
            break;
        case 5:
            effname = " AlienWah";
            break;
        case 6:
            effname = " Distortion";
            break;
        case 7:
            effname = " EQ";
            if (control > 1)
                contstr = " (Band " + to_string(int(parameter)) + ") Control " + to_string(control);
            break;
        case 8:
            effname = " DynFilter";
            break;

        default:
            showValue = false;
            contstr = " Unrecognised";
    }

    if (kititem != 0x87 && control == 16)
    {
        contstr = " Preset " + to_string (lrint(value) + 1);
        showValue = false;
    }

    return (name + effname + contstr);
}


void InterChange::mediate()
{
    CommandBlock getData;
    size_t commandSize = sizeof(getData);
    bool more;
    size_t size;
    int toread;
    char *point;
    do
    {
        more = false;
        size = jack_ringbuffer_read_space(fromCLI);
        if (size >= commandSize)
        {
            if (size > commandSize)
                more = true;
            toread = commandSize;
            point = (char*) &getData.bytes;
            jack_ringbuffer_read(fromCLI, point, toread);
            if(getData.data.part != 0xd8) // Not special midi-learn message
                commandSend(&getData);
            returns(&getData);
        }

        size = jack_ringbuffer_read_space(fromGUI);
        if (size >= commandSize)
        {
            if (size > commandSize)
                more = true;
            toread = commandSize;
            point = (char*) &getData.bytes;
            jack_ringbuffer_read(fromGUI, point, toread);

            if(getData.data.part != 0xd8) // Not special midi-learn message
                commandSend(&getData);
            returns(&getData);
        }

        size = jack_ringbuffer_read_space(fromMIDI);
        if (size >= commandSize)
        {
            if (size > commandSize)
                more = true;
            toread = commandSize;
            point = (char*) &getData.bytes;
            jack_ringbuffer_read(fromMIDI, point, toread);
            if(getData.data.part != 0xd8) // Not special midi-learn message
            {
                commandSend(&getData);
                returns(&getData);
            }
            else if (getData.data.control == 24) // activity LED
            {
                if (jack_ringbuffer_write_space(toGUI) >= commandSize)
                jack_ringbuffer_write(toGUI, (char*) getData.bytes, commandSize);
            }
            else if (getData.data.control == 0xd8) // not part!
            {
                synth->mididecode.midiProcess(getData.data.kit, getData.data.engine, getData.data.insert, false);
            }
        }
        size = jack_ringbuffer_read_space(returnsLoopback);
        if (size >= commandSize)
        {
            if (size > commandSize)
                more = true;
            toread = commandSize;
            point = (char*) &getData.bytes;
            jack_ringbuffer_read(returnsLoopback, point, toread);
            returns(&getData);
        }
    }
    while (more && synth->getRuntime().runSynth);
}


void InterChange::returnsDirect(int altData)
{
    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));

    switch (altData & 0xff)
    {
        case 1:
            putData.data.control = 128;
            putData.data.type = 0xf0; // Stop
            putData.data.part = 0xf0;
            putData.data.parameter = 0x80;
            break;
        case 2:
            putData.data.control = (altData >> 8) & 0xff; // master reset
            putData.data.type = altData >> 24;
            putData.data.part = 0xf0;
            putData.data.parameter = 0x80;
            break;
        case 3:
            putData.data.control = 80; // patch set load
            putData.data.type = altData >> 24;
            putData.data.part = 0xf0;
            putData.data.parameter = 0x80;
            putData.data.par2 = (altData >> 8) & 0xff;
            break;
        case 4:
            putData.data.control = 84; // vector load
            putData.data.type = altData >> 24;
            putData.data.part = 0xf0;
            putData.data.insert = (altData >> 16) & 0xff;
            putData.data.parameter = 0x80;
            putData.data.par2 = (altData >> 8) & 0xff;
            break;
        case 5:
            putData.data.control = 92; // state load
            putData.data.type = altData >> 24;
            putData.data.part = 0xf0;
            putData.data.parameter = 0x80;
            putData.data.par2 = (altData >> 8) & 0xff;
            break;
        case 6:
            putData.data.control = 88; // scales load
            putData.data.type = altData >> 24;
            putData.data.part = 0xf0;
            putData.data.parameter = 0x80;
            putData.data.par2 = (altData >> 8) & 0xff;
        default:
            return;
            break;
    }
    returns(&putData);
}

void InterChange::returns(CommandBlock *getData)
{
    unsigned char type = getData->data.type;// | 4; // back from synth

    if (type == 0xff)
        return;
    if ((getData->data.parameter & 0x80) && getData->data.parameter < 0xc0)
    {
        if (jack_ringbuffer_write_space(toCLI) >= commandSize)
        jack_ringbuffer_write(toCLI, (char*) getData->bytes, commandSize); // this will redirect where needed.
        return;
    }

    bool isCliOrGuiRedraw = type & 0x10; // separated out for clarity
    bool isMidi = type & 8;
    bool write = (type & 0x40) > 0;
    bool isOKtoRedraw = (isCliOrGuiRedraw && write) || isMidi;

    if (synth->guiMaster && isOKtoRedraw)
    {
        //cout << "writing to GUI" << endl;
        if (jack_ringbuffer_write_space(toGUI) >= commandSize)
            jack_ringbuffer_write(toGUI, (char*) getData->bytes, commandSize);
        else
            synth->getRuntime().Log("Unable to write to toGUI buffer");
    }

    if (jack_ringbuffer_write_space(toCLI) >= commandSize)
        jack_ringbuffer_write(toCLI, (char*) getData->bytes, commandSize);
    else
        synth->getRuntime().Log("Unable to write to toCLI buffer");
}


void InterChange::setpadparams(int point)
{
    int npart = point & 0x3f;
    int kititem = point >> 8;

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
    bool isWrite = (getData->data.type & 0x40) > 0;
    if (isWrite && isChanged) //write command
    {
        synth->setNeedsSaving(true);
        unsigned char control = getData->data.control;
        unsigned char npart = getData->data.part;
        unsigned char insert = getData->data.insert;
        if (npart < NUM_MIDI_PARTS && (insert < 0xff || (control != 8 && control != 222)))
        {
            if (synth->part[npart]->Pname == "Simple Sound")
            {
                synth->part[npart]->Pname ="No Title";
                getData->data.type |= 0x10; // force GUI to update
            }
        }
    }
    return isChanged;
}


bool InterChange::commandSendReal(CommandBlock *getData)
{
    unsigned char npart = getData->data.part;
    if (npart == 0xD9) // music input takes priority!
    {
        commandMidi(getData);
        __sync_and_and_fetch(&blockRead, 2); // clear it now it's done
        return false;
    }
//    float value = getData->data.value;
    unsigned char parameter = getData->data.parameter;
    if ((parameter & 0x80) && parameter < 0xc0)
        return true; // indirect transfer

    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
//    unsigned char par2 = getData->data.par2;
//    bool isCli = type & 0x10;
    bool isGui = type & 0x20;
    char button = type & 3;

    //cout << "Type " << int(type) << "  Control " << int(control) << "  Part " << int(npart) << "  Kit " << int(kititem) << "  Engine " << int(engine) << endl;
    //cout  << "Insert " << int(insert)<< "  Parameter " << int(parameter) << "  Par2 " << int(par2) << endl;

    if (!isGui && button == 1)
    {
        __sync_and_and_fetch(&blockRead, 2); // just to be sure
        return false;
    }

    if (npart == 0xc0)
    {
        commandVector(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    if (npart == 0xe8)
    {
        commandMicrotonal(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    if (npart == 0xf8)
    {
        commandConfig(getData);
        return true;
    }
    if (npart == 0xf0)
    {
        commandMain(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    if ((npart == 0xf1 || npart == 0xf2) && kititem == 0xff)
    {
        commandSysIns(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    if (kititem >= 0x80 && kititem != 0xff)
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

    if (kititem >= NUM_KIT_ITEMS && kititem < 0xff)
    {
        __sync_and_and_fetch(&blockRead, 2);
        return false; // invalid kit number
    }

    Part *part = synth->part[npart];

    if (part->busy && engine == 2) // it's a PadSynth control
    {
        getData->data.type &= 0xbf; // turn it into a read
        getData->data.control = 252; // part busy message
        getData->data.kit = 0xff;
        getData->data.engine = 0xff;
        getData->data.insert = 0xff;
        return false;
    }
    if (control == 252)
    {
        getData->data.value = part->busy;
        return false;
    }
    if (kititem != 0xff && kititem != 0 && engine != 0xff && control != 8 && part->kit[kititem].Penabled == false)
    {
        __sync_and_and_fetch(&blockRead, 2);
        return false; // attempt to access not enabled kititem
    }

    if (kititem == 0xff || insert == 0x20)
    {
        if (control != 58 && kititem < 0xff && part->Pkitmode == 0)
        {
            __sync_and_and_fetch(&blockRead, 2);
            return false;
        }
        commandPart(getData);
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }

    if (kititem > 0 && kititem < 0xff && part->Pkitmode == 0)
    {
        __sync_and_and_fetch(&blockRead, 2);
        return false;
    }

    if (engine == 2)
    {
        switch(insert)
        {
            case 0xff:
                commandPad(getData);
                break;
            case 0:
                commandLFO(getData);
                break;
            case 1:
                commandFilter(getData);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(getData);
                break;
            case 5:
            case 6:
            case 7:
                commandOscillator(getData,  part->kit[kititem].padpars->oscilgen);
                break;
            case 8:
            case 9:
                commandResonance(getData, part->kit[kititem].padpars->resonance);
                break;
        }
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }

    if (engine == 1)
    {
        switch (insert)
        {
            case 0xff:
            case 6:
            case 7:
                commandSub(getData);
                break;
            case 1:
                commandFilter(getData);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(getData);
                break;
        }
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }

    if (engine >= 0x80)
    {
        if ((engine & 0x3f) > 7)
        {
            getData->data.type = 0xff; // block any further action
            synth->getRuntime().Log("Invalid voice number");
            synth->getRuntime().finishedCLI = true;
            __sync_and_and_fetch(&blockRead, 2);
            return false;
        }
        switch (insert)
        {
            case 0xff:
                commandAddVoice(getData);
                break;
            case 0:
                commandLFO(getData);
                break;
            case 1:
                commandFilter(getData);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(getData);
                break;
            case 5:
            case 6:
            case 7:
                if (engine >= 0xc0)
                {
                    engine &= 7;
                    if (control != 113)
                    {
                        int voicechange = part->kit[kititem].adpars->VoicePar[engine].PextFMoscil;
                        //cout << "ext Mod osc " << voicechange << endl;
                        if (voicechange != -1)
                        {
                            engine = voicechange;
                            getData->data.engine = engine | 0xc0;
                        }   // force it to external mod
                    }

                    commandOscillator(getData,  part->kit[kititem].adpars->VoicePar[engine].FMSmp);
                }
                else
                {
                    engine &= 7;
                    if (control != 137)
                    {
                        int voicechange = part->kit[kititem].adpars->VoicePar[engine].Pextoscil;
                        //cout << "ext voice osc " << voicechange << endl;
                        if (voicechange != -1)
                        {
                            engine = voicechange;
                            getData->data.engine = engine | 0x80;
                        }   // force it to external voice
                    }

                    commandOscillator(getData,  part->kit[kititem].adpars->VoicePar[engine].OscilSmp);
                }
                break;
        }
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }

    if (engine == 0)
    {
        switch (insert)
        {
            case 0xff:
                commandAdd(getData);
                break;
            case 0:
                commandLFO(getData);
                break;
            case 1:
                commandFilter(getData);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(getData);
                break;
            case 8:
            case 9:
                commandResonance(getData, part->kit[kititem].adpars->GlobalPar.Reson);
                break;
        }
        __sync_and_and_fetch(&blockRead, 2);
        return true;
    }
    getData->data.type = 0xff; // block any further action
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
    //unsigned char char2 = getData->data.insert;
    //unsigned char parameter = getData->data.parameter;
    unsigned char par2 = getData->data.par2;

    //cout << "value " << value_int << "  control " << int(control) << "  chan " << int(chan) << "  char1 " << char1 << "  char2 " << int(char2) << "  param " << int(parameter) << "  par2 " << int(par2) << endl;

    if (control == 2 && char1 >= 0x80)
    {
        char1 |= 0x200; // for 'specials'
    }

    switch(control)
    {
        case 0:
            synth->NoteOn(chan, char1, value_int);
            synth->getRuntime().finishedCLI = true;
            getData->data.type = 0xff; // till we know what to do!
            break;
        case 1:
            synth->NoteOff(chan, char1);
            synth->getRuntime().finishedCLI = true;
            getData->data.type = 0xff; // till we know what to do!
            break;
        case 2:
            //cout << "Midi controller ch " << to_string(int(chan)) << "  type " << to_string(int(char1)) << "  val " << to_string(value_int) << endl;
            __sync_or_and_fetch(&blockRead, 1);
            synth->SetController(chan, char1, value_int);
            break;

        case 8: // Program / Bank / Root
            getData->data.parameter = 0x80;
            if ((value_int < 0xff || par2 != NO_MSG) && chan < synth->getRuntime().NumAvailableParts)
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
        synth->getRuntime().vectordata.Xaxis[ch] = 0xff;
        synth->getRuntime().vectordata.Yaxis[ch] = 0xff;
        synth->getRuntime().vectordata.Xfeatures[ch] = 0;
        synth->getRuntime().vectordata.Yfeatures[ch] = 0;
        synth->getRuntime().vectordata.Enabled[ch] = false;
        synth->getRuntime().vectordata.Name[ch] = "No Name " + to_string(ch + 1);
    }
}


void InterChange::commandVector(CommandBlock *getData)
{
    int value = getData->data.value; // no floats here
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned int chan = getData->data.insert;
    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    unsigned int features;

    if (control == 96)
    {
        vectorClear(chan);
        synth->setLastfileAdded(5, "");
        return;
    }
    if (write)
    {
        if (control >= 19 && control <= 22)
            features = synth->getRuntime().vectordata.Xfeatures[chan];
        else if (control >= 35 && control <= 38)
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
                    case 2:  // local to source
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

        case 8:
            break; // handled elsewhere

        case 16: // enable vector and set X CC
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
        case 17: // left instrument
            if (write)
                synth->vectorSet(4, chan, value);
            else
            {
                ;
            }
            break;
        case 18: // right instrument
            if (write)
                synth->vectorSet(5, chan, value);
            else
            {
                ;
            }
            break;
        case 19:
        case 35: // volume feature
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
        case 20:
        case 36: // panning feature
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
        case 21:
        case 37: // filter cutoff feature
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
        case 22:
        case 38: // modulation feature
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

        case 32: // enable Y and set CC
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
        case 33: // up instrument
            if (write)
                synth->vectorSet(6, chan, value);
            else
            {
                ;
            }
            break;
        case 34: // down instrument
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
        if (control >= 19 && control <= 22)
            synth->getRuntime().vectordata.Xfeatures[chan] = features;
        else if (control >= 35 && control <= 38)
            synth->getRuntime().vectordata.Yfeatures[chan] = features;
    }
}


void InterChange::commandMicrotonal(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    char value_bool = (value > 0.5f);

    switch (control)
    {
        case 0: // 'A' Frequency
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

        case 1: // 'A' Note
            if (write)
                synth->microtonal.PAnote = value_int;
            else
                value = synth->microtonal.PAnote;
            break;
        case 2: // Invert Keys
            if (write)
                synth->microtonal.Pinvertupdown = value_bool;
            else
                value = synth->microtonal.Pinvertupdown;
            break;
        case 3: // Key Center
            if (write)
                synth->microtonal.Pinvertupdowncenter = value_int;
            else
                value = synth->microtonal.Pinvertupdowncenter;
            break;
        case 4: // Scale Shift
            if (write)
                synth->microtonal.Pscaleshift = value_int + 64;
            else
                value = synth->microtonal.Pscaleshift - 64;
            break;

        case 8: // Enable Microtonal
            if (write)
                synth->microtonal.Penabled = value_bool;
            else
                value = synth->microtonal.Penabled;
            break;

        case 16: // Enable Keyboard Mapping
            if (write)
                synth->microtonal.Pmappingenabled = value_bool;
            else
               value = synth->microtonal.Pmappingenabled;
            break;
        case 17: // Keyboard First Note
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
        case 18: // Keyboard Middle Note
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
        case 19: // Keyboard Last Note
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

        case 32: // Tuning
            // done elsewhere
            break;
        case 33: // Keyboard Map
            // done elsewhere
            break;

        case 48: // Import .scl File
            // done elsewhere
            break;
        case 49: // Import .kbm File
            // done elsewhere
            break;

        case 64: // Name
            // done elsewhere
            break;
        case 65: // Comments
            // done elsewhere
            break;

        case 80: // Retune
            // done elsewhere
            break;
        case 96: // Clear scales
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

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    bool mightChange = true;
    int value_int = lrint(value);
    bool value_bool = value_int > 0;

    switch (control)
    {
// main
        case 0:
            if (write)
            {
                value = nearestPowerOf2(value_int, 256, 16384);
                getData->data.value = value;
                synth->getRuntime().Oscilsize = value;
            }
            else
                value = synth->getRuntime().Oscilsize;
            break;
        case 1:
            if (write)
            {
                value = nearestPowerOf2(value_int, 16, 4096);
                getData->data.value = value;
                synth->getRuntime().Buffersize = value;
            }
            else
                value = synth->getRuntime().Buffersize;
            break;
        case 2:
            if (write)
                 synth->getRuntime().Interpolation = value_bool;
            else
                value = synth->getRuntime().Interpolation;
            break;
        case 3:
            if (write)
                 synth->getRuntime().VirKeybLayout = value_int;
            else
                value = synth->getRuntime().VirKeybLayout;
            break;
        case 4:
            if (write)
                 synth->getRuntime().GzipCompression = value_int;
            else
                value = synth->getRuntime().GzipCompression;
            break;
        case 5:
            if (write)
                 synth->getRuntime().toConsole = value_bool;
            else
                value = synth->getRuntime().toConsole;
            break;
        case 6:
            if (write)
                 synth->getRuntime().instrumentFormat = value_int;
            else
                value = synth->getRuntime().instrumentFormat;
            break;
// switches
        case 16:
            if (write)
                synth->getRuntime().loadDefaultState = value_bool;
            else
                value = synth->getRuntime().loadDefaultState;
            break;
        case 17:
            if (write)
                synth->getRuntime().hideErrors = value_bool;
            else
                value = synth->getRuntime().hideErrors;
            break;
        case 18:
            if (write)
                synth->getRuntime().showSplash = value_bool;
            else
                value = synth->getRuntime().showSplash;
            break;
        case 19:
            if (write)
                synth->getRuntime().showTimes = value_bool;
            else
                value = synth->getRuntime().showTimes;
            break;
        case 20:
            if (write)
                synth->getRuntime().logXMLheaders = value_bool;
            else
                value = synth->getRuntime().logXMLheaders;
            break;
        case 21:
            if (write)
                synth->getRuntime().xmlmax = value_bool;
            else
                value = synth->getRuntime().xmlmax;
            break;
        case 22:
            if (write)
                synth->getRuntime().showGui = value_bool;
            else
                value = synth->getRuntime().showGui;
            break;
        case 23:
            if (write)
                synth->getRuntime().showCLI = value_bool;
            else
                value = synth->getRuntime().showCLI;
            break;
        case 24:
            if (write)
                synth->getRuntime().autoInstance = value_bool;
            else
                value = synth->getRuntime().autoInstance;
            break;
// jack
        case 32: // done elsewhere
            break;
        case 33:
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
        case 34: // done elsewhere
            break;
        case 35:
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
        case 36:
            if (write)
            {
                synth->getRuntime().connectJackaudio = value_bool;
                synth->getRuntime().audioEngine = jack_audio;
            }
            else
                value = synth->getRuntime().connectJackaudio;
            break;
// alsa
        case 48: // done elsewhere
            break;
        case 49:
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
        case 50: // done elsewhere
            break;
        case 51:
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
        case 52:
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
                    default:
                        value = 3;
                        break;
                }
            break;
// midi
        case 64:
            break;
        case 65:
            if (write)
            {
                if (value_int > 119)
                {
                    value_int = 128;
                    getData->data.value = value_int;
                }
                synth->getRuntime().midi_bank_root = value_int;
            }
            else
                value = synth->getRuntime().midi_bank_root;
            break;

        case 67:
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
        case 68:
            if (write)
                synth->getRuntime().EnableProgChange = value_bool;
            else
                value = synth->getRuntime().EnableProgChange;
            break;
        case 69:
            if (write)
                synth->getRuntime().enable_part_on_voice_load = value_bool;
            else
                value = synth->getRuntime().enable_part_on_voice_load;
            break;
        case 70:
            break;
        case 71:
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
        case 72:
            if (write)
                synth->getRuntime().ignoreResetCCs = value_bool;
            else
                value = synth->getRuntime().ignoreResetCCs;
            break;
        case 73:
            if (write)
                synth->getRuntime().monitorCCin = value_bool;
            else
                value = synth->getRuntime().monitorCCin;
            break;
        case 74:
            if (write)
                synth->getRuntime().showLearnedCC = value_bool;
            else
                value = synth->getRuntime().showLearnedCC;
            break;
        case 75:
            if (write)
                synth->getRuntime().enable_NRPN = value_bool;
            else
                value = synth->getRuntime().enable_NRPN;
            break;
// save config
        case 80: //done elsewhere
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

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);
    int value_int = lrint(value);

    switch (control)
    {
        case 0:
            if (write)
                synth->setPvolume(value);
            else
                value = synth->Pvolume;
            break;

        case 14:
            if (write)
                synth->getRuntime().currentPart = value_int;
            else
                value = synth->getRuntime().currentPart;
            break;
        case 15:
            if ((write) && (value == 16 || value == 32 || value == 64))
                synth->getRuntime().NumAvailableParts = value;
            else
                value = synth->getRuntime().NumAvailableParts;
            break;

        case 32: // done elsewhere
            break;
        case 35: // done elsewhere
            break;

        case 48: // solo mode
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
        case 49: // solo ch number
            if (write && synth->getRuntime().channelSwitchType > 0)
                synth->getRuntime().channelSwitchCC = value_int;
            else
            {
                write = false; // for an invalid write attempt
                value = synth->getRuntime().channelSwitchCC;
            }
            break;

        case 73: // set current root and bank
            if (write)
            {
                if (kititem < 0x80) // should test for success
                    synth->getBankRef().setCurrentRootID(kititem);
                if (engine < 0x80) // should test for success
                    synth->getBankRef().setCurrentBankID(engine, true);
            }
            break;

        case 74: // load instrument from ID
            /*
             * this is the lazy way to move all program changes
             * to the new MIDI method.
             */
            synth->partonoffLock(value_int, -1);
            getData->data.control = 8;
            getData->data.part = 0xd9;
            getData->data.kit = value_int;
            getData->data.value = par2;
            getData->data.parameter = 0x80;
            getData->data.par2 = 0xff;
            break;
        case 78: // load named instrument
            synth->partonoffLock(value_int & 0x3f, -1);
            // as above for named instruments :)
            getData->data.control = 8;
            getData->data.part = 0xd9;
            getData->data.kit = value_int & 0x3f;
            getData->data.value = 0xff;
            getData->data.parameter = 0x80;
            break;

        case 80: // load patchset
            if (write && (parameter == 0xc0))
            {
                synth->allStop(3 | (par2 << 8) | (type << 24));
                getData->data.type = 0xff; // stop further action
            }
            break;
        case 84: // load vector
            if (write && (parameter == 0xc0))
            {
                synth->allStop(4 | (par2 << 8) | (insert << 16) | (type << 24));
                getData->data.type = 0xff; // stop further action
            }
            break;
        case 85:
            break; // done elsewhere
        case 88: // load scale
            returnsDirect(6 | (par2 << 8) | (type << 24));
            break;
        case 89: // done elsewhere
            break;
        case 92: // load state
            if (write && (parameter == 0xc0))
            {
                synth->allStop(5 | (par2 << 8) | (type << 24));
                getData->data.type = 0xff; // stop further action
            }
            break;
        case 93: // done elsewhere
            break;
        case 96: // master reset
        case 97: // reset including MIDI-learn
            if (write && (parameter == 0xc0))
            {
                synth->allStop(2 | (control << 8) | (type << 24));
                getData->data.type = 0xff; // stop further action);
            }
            break;
        case 104: // done elsewhere
            break;
        case 105: // done elsewhere
            break;
        case 128: // just stop
            if (write)
                synth->allStop(1);
            getData->data.type = 0xff; // stop further action);
            break;

        case 200:
            if (!write && kititem < NUM_MIDI_PARTS)
                value = synth->VUdata.values.parts[kititem];
            break;
        case 201:
            if (!write)
            {
                if (kititem == 1)
                    value = synth->VUdata.values.vuOutPeakR;
                else
                    value = synth->VUdata.values.vuOutPeakL;
            }
            break;
        case 202:
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
            getData->data.type = 0xff; // stop further action);
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
    //unsigned char par2 = getData->data.par2;
    unsigned char effNum = engine;

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    bool kitType = (insert == 0x20);

    if ( kitType && kititem >= NUM_KIT_ITEMS)
    {
        getData->data.type = 0xff; // block any further action
        synth->getRuntime().Log("Invalid kit number");
        return;
    }
    int value_int = lrint(value);
    char value_bool = (value > 0.5f);

    Part *part;
    part = synth->part[npart];

    switch (control)
    {
        case 0:
            if (write)
                part->setVolume(value);
            else
                value = part->Pvolume;
            break;
        case 1:
            if (write)
                part->Pvelsns = value;
            else
                value = part->Pvelsns;
            break;
        case 2:
            if (write)
                part->SetController(C_panning, value);
            else
                value = part->Ppanning;
            break;
        case 4:
            if (write)
                part->Pveloffs = value;
            else
                value = part->Pveloffs;
            break;
        case 5:
            if (write)
            {
                part->Prcvchn = value_int;
                /*if (synth->getRuntime().channelSwitchType > 0 && synth->getRuntime().channelSwitchType != 2)
                {
                    for (int i = 0; i < NUM_MIDI_CHANNELS; ++i)
                        synth->part[i]->Prcvchn = 16;
                    synth->getRuntime().channelSwitchValue = npart;
                    part->Prcvchn = 0;
                }*/
            }
            else
                value = part->Prcvchn;
            break;
        case 6:
            if (write)
                synth->SetPartKeyMode(npart, value_int);
            else
                value = (synth->ReadPartKeyMode(npart)) & 3; // clear out temporary legato
            break;
        case 7:
            if (write)
                part->ctl->portamento.portamento = value_bool;
            else
                value = part->ctl->portamento.portamento;
            break;
        case 8:
            if (kitType)
            {
                switch(engine)
                {
                    case 0:
                        if (write)
                            part->kit[kititem].Padenabled = value_bool;
                        else
                            value = part->kit[kititem].Padenabled;
                        break;
                    case 1:
                        if (write)
                            part->kit[kititem].Psubenabled = value_bool;
                        else
                            value = part->kit[kititem].Psubenabled;
                        break;
                    case 2:
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
                    case 0:
                        if (write)
                            part->kit[0].Padenabled = value_bool;
                        else
                            value = part->kit[0].Padenabled;
                        break;
                    case 1:
                        if (write)
                            part->kit[0].Psubenabled = value_bool;
                        else
                            value = part->kit[0].Psubenabled;
                        break;
                    case 2:
                        if (write)
                            part->kit[0].Ppadenabled = value_bool;
                        else
                            value = part->kit[0].Ppadenabled;
                        break;
                    case 255:
                        if (write)
                        {
                            if (value_bool && synth->getRuntime().currentPart != npart) // make it a part change
                            {
                                synth->partonoffWrite(npart, 1);
                                synth->getRuntime().currentPart = npart;
                                getData->data.value = npart;
                                getData->data.control = 14;
                                getData->data.part = 0xf0;
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
        case 9:
            if (kitType)
            {
                if (write)
                    part->kit[kititem].Pmuted = value_bool;
                else
                    value = part->kit[kititem].Pmuted;
            }
            break;

        case 16: // always return actual value
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
        case 17: // always return actual value
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
        case 18: // always return actual value
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
        case 19: // always return actual value
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
        case 20:
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

        case 24:
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

        case 33:
            if (write)
                part->setkeylimit(value_int);
            else
                value = part->Pkeylimit;
            break;
        case 35: // done elsewhere
            break;

        case 40:
            if (write)
                synth->setPsysefxvol(npart,0, value);
            else
                value = synth->Psysefxvol[0][npart];
            break;
        case 41:
            if (write)
                synth->setPsysefxvol(npart,1, value);
            else
                value = synth->Psysefxvol[1][npart];
            break;
        case 42:
            if (write)
                synth->setPsysefxvol(npart,2, value);
            else
                value = synth->Psysefxvol[2][npart];
            break;
        case 43:
            if (write)
                synth->setPsysefxvol(npart,3, value);
            else
                value = synth->Psysefxvol[3][npart];
            break;

        case 48:
            if (write)
                part->Pfrand = value;
            else
                value = part->Pfrand;
            break;

        case 57:
            if (write)
            {
                part->legatoFading = 0;
                part->Pdrummode = value_bool;
                synth->setPartMap(npart);
            }
            else
                value = part->Pdrummode;
            break;
        case 58:
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
                value = part->Pkitmode;
            break;

        case 64: // local to source
            break;

        case 65:
            if (write)
                part->partefx[effNum]->changeeffect(value_int);
            else
                value = part->partefx[effNum]->geteffect();
            break;
        case 66:
            if (write)
            {
                part->Pefxroute[effNum] = value_int;
                part->partefx[effNum]->setdryonly(value_int == 2);
            }
            else
                value = part->Pefxroute[effNum];
            break;
        case 67:
            if (write)
                part->Pefxbypass[effNum] = value_bool;
            else
                value = part->Pefxbypass[effNum];
            break;

        case 96: // doClearPart
            if(write)
            {
                synth->partonoffWrite(npart, -1);
                getData->data.parameter = 0x80;
            }
            else
                getData->data.type = 0xff; // block any further action
            break;

        case 120:
            if (synth->partonoffRead(npart) != 1)
            {
                getData->data.value = part->Paudiodest; // specific for this control
                return;
            }
            else if (write)
            {
                if (npart < synth->getRuntime().NumAvailableParts)
                    synth->part[npart]->Paudiodest = value_int;
                getData->data.parameter = 0x80;
            }
            else
                value = part->Paudiodest;
            break;

        case 128:
            if (write)
                part->ctl->setvolume(value_int); // not the *actual* volume
            else
                value = part->ctl->volume.data;
            break;
        case 129:
            if (write)
                part->ctl->volume.receive = value_bool;
            else
                value = part->ctl->volume.receive;
            break;
        case 130:
            if (write)
                part->ctl->setPanDepth(value_int);
            else
                value = part->ctl->panning.depth;
            break;
        case 131:
            if (write)
                part->ctl->modwheel.depth = value;
            else
                value = part->ctl->modwheel.depth;
            break;
        case 132:
            if (write)
                part->ctl->modwheel.exponential = value_bool;
            else
                value = part->ctl->modwheel.exponential;
            break;
        case 133:
            if (write)
                part->ctl->bandwidth.depth = value;
            else
                value = part->ctl->bandwidth.depth;
            break;
        case 134:
            if (write)
                part->ctl->bandwidth.exponential = value_bool;
            else
                value = part->ctl->bandwidth.exponential;
            break;
        case 135:
            if (write)
                part->ctl->expression.receive = value_bool;
            else
                value = part->ctl->expression.receive;
            break;
        case 136:
            if (write)
                part->ctl->fmamp.receive = value_bool;
            else
                value = part->ctl->fmamp.receive;
            break;
        case 137:
            if (write)
                part->ctl->sustain.receive = value_bool;
            else
                value = part->ctl->sustain.receive;
            break;
        case 138:
            if (write)
                part->ctl->pitchwheel.bendrange = value_int;
            else
                value = part->ctl->pitchwheel.bendrange;
            break;
        case 139:
            if (write)
                part->ctl->filterq.depth = value;
            else
                value = part->ctl->filterq.depth;
            break;
        case 140:
            if (write)
                part->ctl->filtercutoff.depth = value;
            else
                value = part->ctl->filtercutoff.depth;
            break;
        case 141:
            if (write)
                if (value_bool)
                    part->PbreathControl = 2;
                else
                    part->PbreathControl = 255;
            else
                value = part->PbreathControl;
            break;

        case 144:
            if (write)
                part->ctl->resonancecenter.depth = value;
            else
                value = part->ctl->resonancecenter.depth;
            break;
        case 145:
            if (write)
                part->ctl->resonancebandwidth.depth = value;
            else
                value = part->ctl->resonancebandwidth.depth;
            break;

        case 160:
            if (write)
                part->ctl->portamento.time = value;
            else
                value = part->ctl->portamento.time;
            break;
        case 161:
            if (write)
                part->ctl->portamento.updowntimestretch = value;
            else
                value = part->ctl->portamento.updowntimestretch;
            break;
        case 162:
            if (write)
                part->ctl->portamento.pitchthresh = value;
            else
                value = part->ctl->portamento.pitchthresh;
            break;
        case 163:
            if (write)
                part->ctl->portamento.pitchthreshtype = value_int;
            else
                value = part->ctl->portamento.pitchthreshtype;
            break;
        case 164:
            if (write)
                part->ctl->portamento.proportional = value_int;
            else
                value = part->ctl->portamento.proportional;
            break;
        case 165:
            if (write)
                part->ctl->portamento.propRate = value;
            else
                value = part->ctl->portamento.propRate;
            break;
        case 166:
            if (write)
                part->ctl->portamento.propDepth = value;
            else
                value = part->ctl->portamento.propDepth;
            break;
        case 168:
            if (write)
                part->ctl->portamento.receive = value_bool;
            else
                value = part->ctl->portamento.receive;
            break;

        case 192:
            if (write)
                part->ctl->setmodwheel(value);
            else
                value = part->ctl->modwheel.data;
            break;
        case 194:
            if (write)
            {
                part->SetController(C_expression, value);
            }
            else
                value = part->ctl->expression.data;
            break;
        case 197:
            if (write)
                part->ctl->setfilterq(value);
            else
                value = part->ctl->filterq.data;
            break;
        case 198:
            if (write)
                part->ctl->setfiltercutoff(value);
            else
                value = part->ctl->filtercutoff.data;
            break;
        case 199:
            if (write)
                part->ctl->setbandwidth(value);
            else
                value = part->ctl->bandwidth.data;
            break;

        case 222: // done elsewhere
            break;
        case 224:
            if (write)
                part->SetController(0x79,0); // C_resetallcontrollers
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

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    char value_bool = (value > 0.5f);

    Part *part;
    part = synth->part[npart];
    ADnoteParameters *pars;
    pars = part->kit[kititem].adpars;

    int k; // temp variable for detune
    switch (control)
    {
        case 0:
            if (write)
                pars->GlobalPar.PVolume = value_int;
            else
                value = pars->GlobalPar.PVolume;
            break;
        case 1:
            if (write)
                pars->GlobalPar.PAmpVelocityScaleFunction = value_int;
            else
                value = pars->GlobalPar.PAmpVelocityScaleFunction;
            break;
        case 2:
            if (write)
                pars->setGlobalPan(value_int);
            else
                value = pars->GlobalPar.PPanning;
            break;

        case 32:
            if (write)
                pars->GlobalPar.PDetune = value_int + 8192;
            else
                value = pars->GlobalPar.PDetune - 8192;
            break;

        case 35:
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
        case 36:
            if (write)
                pars->GlobalPar.PDetuneType = value_int;
            else
                value = pars->GlobalPar.PDetuneType;
            break;
        case 37:
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
        case 39:
            if (write)
            {
                pars->GlobalPar.PBandwidth = value_int;
                 pars->getBandwidthDetuneMultiplier();
            }
            else
                value = pars->GlobalPar.PBandwidth;
            break;

        case 112:
            if (write)
                pars->GlobalPar.PStereo = value_bool;
            else
                value = pars->GlobalPar.PStereo;
            break;
        case 113:
            if (write)
                pars->GlobalPar.Hrandgrouping = value_bool;
            else
                value = pars->GlobalPar.Hrandgrouping;
            break;

        case 120:
            if (write)
                pars->GlobalPar.Fadein_adjustment = value_int;
            else
                value = pars->GlobalPar.Fadein_adjustment;
            break;
        case 121:
            if (write)
                pars->GlobalPar.PPunchStrength = value_int;
            else
                value = pars->GlobalPar.PPunchStrength;
            break;
        case 122:
            if (write)
                pars->GlobalPar.PPunchTime = value_int;
            else
                value = pars->GlobalPar.PPunchTime;
            break;
        case 123:
            if (write)
                pars->GlobalPar.PPunchStretch = value_int;
            else
                value = pars->GlobalPar.PPunchStretch;
            break;
        case 124:
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
    int nvoice = engine & 0x1f;

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    char value_bool = (value > 0.5f);

    Part *part;
    part = synth->part[npart];
    ADnoteParameters *pars;
    pars = part->kit[kititem].adpars;

    int k; // temp variable for detune

    switch (control)
    {
        case 0:
            if (write)
                pars->VoicePar[nvoice].PVolume = value_int;
            else
                value = pars->VoicePar[nvoice].PVolume;
            break;
        case 1:
            if (write)
                pars->VoicePar[nvoice].PAmpVelocityScaleFunction = value_int;
            else
                value = pars->VoicePar[nvoice].PAmpVelocityScaleFunction;
            break;
        case 2:
            if (write)
                 pars->setVoicePan(nvoice, value_int);
            else
                value = pars->VoicePar[nvoice].PPanning;
            break;
        case 4:
            if (write)
                pars->VoicePar[nvoice].PVolumeminus = value_bool;
            else
                value = pars->VoicePar[nvoice].PVolumeminus;
            break;
        case 8:
            if (write)
                pars->VoicePar[nvoice].PAmpEnvelopeEnabled = value_bool;
            else
                value = pars->VoicePar[nvoice].PAmpEnvelopeEnabled;
            break;
        case 9:
            if (write)
                pars->VoicePar[nvoice].PAmpLfoEnabled = value_bool;
            else
                value = pars->VoicePar[nvoice].PAmpLfoEnabled;
            break;

        case 16:
            if (write)
                pars->VoicePar[nvoice].PFMEnabled = value_int;
            else
                value = pars->VoicePar[nvoice].PFMEnabled;
            break;
        case 17:
            if (write)
                pars->VoicePar[nvoice].PFMVoice = value_int;
            else
                value = pars->VoicePar[nvoice].PFMVoice;
            break;

        case 32:
            if (write)
                pars->VoicePar[nvoice].PDetune = value_int + 8192;
            else
                value = pars->VoicePar[nvoice].PDetune-8192;
            break;
        case 33:
            if (write)
                pars->VoicePar[nvoice].PfixedfreqET = value_int;
            else
                value = pars->VoicePar[nvoice].PfixedfreqET;
            break;
        case 34:
            if (write)
                 pars->VoicePar[nvoice].Pfixedfreq = value_bool;
            else
                value = pars->VoicePar[nvoice].Pfixedfreq;
            break;
        case 35:
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
        case 36:
            if (write)
                pars->VoicePar[nvoice].PDetuneType = value_int;
            else
                value = pars->VoicePar[nvoice].PDetuneType;
            break;
        case 37:
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
        case 38:
            if (write)
                pars->VoicePar[nvoice].PBendAdjust = value_int;
            else
                value = pars->VoicePar[nvoice].PBendAdjust;
            break;
        case 39:
            if (write)
                pars->VoicePar[nvoice].POffsetHz = value_int;
            else
                value = pars->VoicePar[nvoice].POffsetHz;
            break;
        case 40:
            if (write)
                pars->VoicePar[nvoice].PFreqEnvelopeEnabled = value_int;
            else
                value = pars->VoicePar[nvoice].PFreqEnvelopeEnabled;
            break;
        case 41:
            if (write)
                pars->VoicePar[nvoice].PFreqLfoEnabled = value_int;
            else
                value = pars->VoicePar[nvoice].PFreqLfoEnabled;
            break;

        case 48:
            if (write)
                pars->VoicePar[nvoice].Unison_frequency_spread = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_frequency_spread;
            break;
        case 49:
            if (write)
                pars->VoicePar[nvoice].Unison_phase_randomness = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_phase_randomness;
            break;
        case 50:
            if (write)
                pars->VoicePar[nvoice].Unison_stereo_spread = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_stereo_spread;
            break;
        case 51:
            if (write)
                pars->VoicePar[nvoice].Unison_vibratto = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_vibratto;
            break;
        case 52:
            if (write)
                pars->VoicePar[nvoice].Unison_vibratto_speed = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_vibratto_speed;
            break;
        case 53:
            if (write)
            {
                if (value < 2)
                    value = 2;
                pars->VoicePar[nvoice].Unison_size = value_int;
            }
            else
                value = pars->VoicePar[nvoice].Unison_size;
            break;
        case 54:
            if (write)
                pars->VoicePar[nvoice].Unison_invert_phase = value_int;
            else
                value = pars->VoicePar[nvoice].Unison_invert_phase;
            break;
        case 56:
            if (write)
            {
                k = value_bool + 1;
                if (pars->VoicePar[nvoice].Unison_size < 2 || k == 1)
                    pars->VoicePar[nvoice].Unison_size = k;
            }
            else
                value = (pars->VoicePar[nvoice].Unison_size > 1);
            break;

        case 64:
            if (write)
                pars->VoicePar[nvoice].Pfilterbypass = value_bool;
            else
                value = pars->VoicePar[nvoice].Pfilterbypass;
            break;
        case 68:
            if (write)
                 pars->VoicePar[nvoice].PFilterEnabled =  value_bool;
            else
                value = pars->VoicePar[nvoice].PFilterEnabled;
            break;
        case 72:
            if (write)
                pars->VoicePar[nvoice].PFilterEnvelopeEnabled= value_bool;
            else
                value = pars->VoicePar[nvoice].PFilterEnvelopeEnabled;
            break;
        case 73:
            if (write)
                pars->VoicePar[nvoice].PFilterLfoEnabled= value_bool;
            else
                value = pars->VoicePar[nvoice].PFilterLfoEnabled;
            break;

        case 80:
            if (write)
                pars->VoicePar[nvoice].PFMVolume = value_int;
            else
                value = pars->VoicePar[nvoice].PFMVolume;
            break;
        case 81:
            if (write)
                pars->VoicePar[nvoice].PFMVelocityScaleFunction = value_int;
            else
                value = pars->VoicePar[nvoice].PFMVelocityScaleFunction;
            break;
        case 82:
            if (write)
                pars->VoicePar[nvoice].PFMVolumeDamp = value_int;
            else
                value = pars->VoicePar[nvoice].PFMVolumeDamp;
            break;
        case 88:
            if (write)
                pars->VoicePar[nvoice].PFMAmpEnvelopeEnabled = value_bool;
            else
                value =  pars->VoicePar[nvoice].PFMAmpEnvelopeEnabled;
            break;

        case 96:
            if (write)
                pars->VoicePar[nvoice].PFMDetune = value_int + 8192;
            else
                value = pars->VoicePar[nvoice].PFMDetune - 8192;
            break;
        case 98:
            if (write)
                pars->VoicePar[nvoice].PFMFixedFreq = value_bool;
            else
                value = pars->VoicePar[nvoice].PFMFixedFreq;
            break;
        case 99:
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
        case 100:
            if (write)
                pars->VoicePar[nvoice].PFMDetuneType = value_int;
            else
                value = pars->VoicePar[nvoice].PFMDetuneType;
            break;
        case 101:
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
        case 104:
            if (write)
                pars->VoicePar[nvoice].PFMFreqEnvelopeEnabled = value_int;
            else
                value = pars->VoicePar[nvoice].PFMFreqEnvelopeEnabled;
            break;

        case 112:
            if (write)
                pars->VoicePar[nvoice].PFMoscilphase = 64 - value_int;
            else
                value = 64 - pars->VoicePar[nvoice].PFMoscilphase;
            break;
        case 113:
            if (write)
                pars->VoicePar[nvoice].PextFMoscil = value_int;
            else
                value = pars->VoicePar[nvoice].PextFMoscil;
            break;

        case 128:
            if (write)
                pars->VoicePar[nvoice].PDelay = value_int;
            else
                value = pars->VoicePar[nvoice].PDelay;
            break;
        case 129:
            if (write)
                pars->VoicePar[nvoice].Enabled = value_bool;
            else
                value = pars->VoicePar[nvoice].Enabled;
            break;
        case 130:
            if (write)
                pars->VoicePar[nvoice].Presonance = value_bool;
            else
                value = pars->VoicePar[nvoice].Presonance;
            break;
        case 136:
            if (write)
                pars->VoicePar[nvoice].Poscilphase =64 - value_int;
            else
                value = 64 - pars->VoicePar[nvoice].Poscilphase;
            break;
        case 137:
            if (write)
                pars->VoicePar[nvoice].Pextoscil = value_int;
            else
                value = pars->VoicePar[nvoice].Pextoscil;
            break;
        case 138:
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

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    char value_bool = (value > 0.5f);

    Part *part;
    part = synth->part[npart];
    SUBnoteParameters *pars;
    pars = part->kit[kititem].subpars;

    if (insert == 6 || insert == 7)
    {
        if (insert == 6)
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

    int k; // temp variable for detune
    switch (control)
    {
        case 0:
            if (write)
                pars->PVolume = value;
            else
                value = pars->PVolume;
            break;
        case 1:
            if (write)
                pars->PAmpVelocityScaleFunction = value;
            else
                value = pars->PAmpVelocityScaleFunction;
            break;
        case 2:
            if (write)
                pars->setPan(value);
            else
                value = pars->PPanning;
            break;

        case 16:
            if (write)
                pars->Pbandwidth = value;
            else
                value = pars->Pbandwidth;
            break;
        case 17:
            if (write)
                pars->Pbwscale = value + 64;
            else
                value = pars->Pbwscale - 64;
            break;
        case 18:
            if (write)
                pars->PBandWidthEnvelopeEnabled = value_bool;
            else
                value = pars->PBandWidthEnvelopeEnabled;
            break;

        case 32:
            if (write)
                pars->PDetune = value + 8192;
            else
                value = pars->PDetune - 8192;
            break;
        case 33:
            if (write)
                pars->PfixedfreqET = value;
            else
                value = pars->PfixedfreqET;
            break;
        case 34:
            if (write)
                pars->Pfixedfreq = value_bool;
            else
                value = pars->Pfixedfreq;
            break;
        case 35:
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
        case 36:
            if (write)
                pars->PDetuneType = value_int + 1;
            else
                value = pars->PDetuneType;
            break;
        case 37:
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

        case 38:
            if (write)
                pars->PBendAdjust = value;
            else
                value = pars->PBendAdjust;
            break;

        case 39:
            if (write)
                pars->POffsetHz = value;
            else
                value = pars->POffsetHz;
            break;

        case 40:
            if (write)
                pars->PFreqEnvelopeEnabled = value_bool;
            else
                value = pars->PFreqEnvelopeEnabled;
            break;

        case 48:
            if (write)
            {
                pars->POvertoneSpread.par1 = value;
                pars->updateFrequencyMultipliers();
            }
            else
                value = pars->POvertoneSpread.par1;
            break;
        case 49:
            if (write)
            {
                pars->POvertoneSpread.par2 = value;
                pars->updateFrequencyMultipliers();
            }
            else
                value = pars->POvertoneSpread.par2;
            break;
        case 50:
            if (write)
            {
                pars->POvertoneSpread.par3 = value;
                pars->updateFrequencyMultipliers();
            }
            else
                value = pars->POvertoneSpread.par3;
            break;
        case 51:
            if (write)
            {
                pars->POvertoneSpread.type =  value_int;
                pars->updateFrequencyMultipliers();
            }
            else
                value = pars->POvertoneSpread.type;
            break;

        case 64:
            if (write)
                pars->PGlobalFilterEnabled = value_bool;
            else
                value = pars->PGlobalFilterEnabled;
            break;

        case 80:
            if (write)
                pars->Pnumstages = value_int;
            else
                value = pars->Pnumstages;
            break;
        case 81:
            if (write)
                pars->Phmagtype = value_int;
            break;
        case 82:
            if (write)
                pars->Pstart = value_int;
            else
                value = pars->Pstart;
            break;

        case 96:
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

        case 112:
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

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);
    char value_bool = (value > 0.5f);

    Part *part;
    part = synth->part[npart];
    PADnoteParameters *pars;
    pars = part->kit[kititem].padpars;

    switch (control)
    {
        case 0:

            if (write)
                pars->PVolume = value;
            else
                value = pars->PVolume;
            break;
        case 1:
            if (write)
                pars->PAmpVelocityScaleFunction = value;
            else
                value = pars->PAmpVelocityScaleFunction;
            break;
        case 2:
            if (write)
                pars->setPan(value);
            else
                value = pars->PPanning;
            break;

        case 16:
            if (write)
                pars->setPbandwidth(value_int);
            else
                value = pars->Pbandwidth;
            break;
        case 17:
            if (write)
                pars->Pbwscale = value_int;
            else
                value = pars->Pbwscale;
            break;
        case 19:
            if (write)
                pars->Pmode = value_int;
            else
                value = pars->Pmode;
            break;

        case 32:
            if (write)
                pars->PDetune = value_int + 8192;
            else
                value = pars->PDetune - 8192;
            break;
        case 33:
            if (write)
                pars->PfixedfreqET = value_int;
            else
                value = pars->PfixedfreqET;
            break;
        case 34:
            if (write)
                pars->Pfixedfreq = value_bool;
            else
                value = pars->Pfixedfreq;
            break;
        case 35:
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
        case 36:
            if (write)
                 pars->PDetuneType = value_int + 1;
            else
                value =  pars->PDetuneType - 1;
            break;
        case 37:
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

        case 38:
            if (write)
                pars->PBendAdjust = value_int;
            else
                value = pars->PBendAdjust;
            break;
        case 39:
            if (write)
                pars->POffsetHz = value_int;
            else
                value = pars->POffsetHz;
            break;

        case 48:
            if (write)
                pars->Phrpos.par1 = value_int;
            else
                value = pars->Phrpos.par1;
            break;
        case 49:
            if (write)
                pars->Phrpos.par2 = value_int;
            else
                value = pars->Phrpos.par2;
            break;
        case 50:
            if (write)
                pars->Phrpos.par3 = value_int;
            else
                value = pars->Phrpos.par3;
            break;
        case 51:
            if (write)
                pars->Phrpos.type = value_int;
            else
                value = pars->Phrpos.type;
            break;

        case 64:
            if (write)
                pars->Php.base.par1 = value_int;
            else
                value = pars->Php.base.par1;
            break;
        case 65:;
            if (write)
                pars->Php.freqmult = value_int;
            else
                value = pars->Php.freqmult;
            break;
        case 66:
            if (write)
                pars->Php.modulator.par1 = value_int;
            else
                value = pars->Php.modulator.par1;
            break;
        case 67:
            if (write)
                pars->Php.modulator.freq = value_int;
            else
                value = pars->Php.modulator.freq;
            break;
        case 68:
            if (write)
                pars->Php.width = value_int;
            else
                value = pars->Php.width;
            break;
        case 69:
            if (write)
                pars->Php.base.type = value;
            else
                value = pars->Php.base.type;
            break;
        case 70:
            if (write)
                 pars->Php.onehalf = value;
            else
                value = pars->Php.onehalf;
            break;
        case 71:
            if (write)
                pars->Php.amp.par1 = value_int;
            else
                value = pars->Php.amp.par1;
            break;
        case 72:
            if (write)
                pars->Php.amp.par2 = value_int;
            else
                value = pars->Php.amp.par2;
            break;
        case 73:
            if (write)
                pars->Php.amp.type = value;
            else
                value = pars->Php.amp.type;
            break;
        case 74:
            if (write)
                pars->Php.amp.mode = value;
            else
                value = pars->Php.amp.mode;
            break;
        case 75:
            if (write)
                pars->Php.autoscale = value_bool;
            else
                value = pars->Php.autoscale;
            break;

        case 80:
            if (write)
                pars->Pquality.basenote = value_int;
            else
                value = pars->Pquality.basenote;
            break;
        case 81:
            if (write)
                pars->Pquality.smpoct = value_int;
            else
                value = pars->Pquality.smpoct;
            break;
        case 82:
            if (write)
                pars->Pquality.oct = value_int;
            else
                value = pars->Pquality.oct;
            break;
        case 83:
            if (write)
                pars->Pquality.samplesize = value_int;
            else
                value = pars->Pquality.samplesize;
            break;

        case 104: // set pad parameters
            if (write)
            {
                synth->partonoffWrite(npart, -1);
                getData->data.parameter = 0x80;
            }
            break;

        case 112:
            if (write)
                pars->PStereo = value_bool;
            else
            {
                ;
            }
            break;

        case 120:
            if (write)
                pars->Fadein_adjustment = value_int;
            else
                value = pars->Fadein_adjustment;
            break;
        case 121:
            if (write)
                pars->PPunchStrength = value_int;
            else
                value = pars->PPunchStrength;
            break;
        case 122:
            if (write)
                pars->PPunchTime = value_int;
            else
                value = pars->PPunchTime;
            break;
        case 123:
            if (write)
                pars->PPunchStretch = value_int;
            else
                value = pars->PPunchStretch;
            break;
        case 124:
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
    int value = lrint(getData->data.value); // no floats here!
    char value_bool = (getData->data.value > 0.5f);
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char insert = getData->data.insert;

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    if (insert == 6)
    {
        if (write)
        {
            oscil->Phmag[control] = value;
            if (value == 64)
                oscil->Phphase[control] = 64;
            oscil->prepare();
        }
        else
            getData->data.value = oscil->Phmag[control];
        return;
    }
    else if(insert == 7)
    {
        if (write)
        {
            oscil->Phphase[control] = value;
            oscil->prepare();
        }
        else
            getData->data.value = oscil->Phphase[control];
        return;
    }

    switch (control)
    {
        case 0:
            if (write)
                oscil->Prand = value + 64;
            else
                value = oscil->Prand - 64;
            break;
        case 1:
            if (write)
                oscil->Phmagtype = value;
            else
                value = oscil->Phmagtype;
            break;
        case 2:
            if (write)
                oscil->Pamprandpower = value;
            else
                value = oscil->Pamprandpower;
            break;
        case 3:
            if (write)
                oscil->Pamprandtype = value;
            else
                value = oscil->Pamprandtype;
            break;

        case 16:
            if (write)
                oscil->Pbasefuncpar = value + 64;
            else
                value = oscil->Pbasefuncpar - 64;
            break;
        case 17:
            if (write)
                oscil->Pcurrentbasefunc = value;
            else
                value = oscil->Pcurrentbasefunc;
            break;
        case 18:
            if (write)
                oscil->Pbasefuncmodulationpar1 = value;
            else
                value = oscil->Pbasefuncmodulationpar1;
            break;
        case 19:
            if (write)
                oscil->Pbasefuncmodulationpar2 = value;
            else
                value = oscil->Pbasefuncmodulationpar2;
            break;
        case 20:
            if (write)
                oscil->Pbasefuncmodulationpar3 = value;
            else
                value = oscil->Pbasefuncmodulationpar3;
            break;
        case 21:
            if (write)
                oscil->Pbasefuncmodulation = value;
            else
                value = oscil->Pbasefuncmodulation;
            break;

        case 32: // this is local to the source
            break;
        case 33:
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

        case 34:
            if (write)
                oscil->Pwaveshaping = value + 64;
            else
                value = oscil->Pwaveshaping - 64;
            break;
        case 35:
            if (write)
                oscil->Pwaveshapingfunction = value;
            else
                value = oscil->Pwaveshapingfunction;
            break;

        case 36:
            if (write)
                oscil->Pfilterpar1 = value;
            else
                value = oscil->Pfilterpar1;
            break;
        case 37:
            if (write)
                oscil->Pfilterpar2 = value;
            else
                value = oscil->Pfilterpar2;
            break;
        case 38:
            if (write)
                oscil->Pfilterbeforews = value_bool;
            else
                value = oscil->Pfilterbeforews;
            break;
        case 39:
            if (write)
                oscil->Pfiltertype = value;
            else
                value = oscil->Pfiltertype;
            break;

        case 40:
            if (write)
                oscil->Pmodulationpar1 = value;
            else
                value = oscil->Pmodulationpar1;
            break;
        case 41:
            if (write)
                oscil->Pmodulationpar2 = value;
            else
                value = oscil->Pmodulationpar2;
            break;
        case 42:
            if (write)
                oscil->Pmodulationpar3 = value;
            else
                value = oscil->Pmodulationpar3;
            break;
        case 43:
            if (write)
                oscil->Pmodulation = value;
            else
                value = oscil->Pmodulation;
            break;

        case 44:
            if (write)
                oscil->Psapar = value;
            else
                value = oscil->Psapar;
            break;
        case 45:
            if (write)
                oscil->Psatype = value;
            else
                value = oscil->Psatype;
            break;

        case 64:
            if (write)
                oscil->Pharmonicshift = value;
            else
                value = oscil->Pharmonicshift;
            break;
        case 65:
            if (write)
                oscil->Pharmonicshift = 0;
            break;
        case 66:
            if (write)
                oscil->Pharmonicshiftfirst = value_bool;
            else
                value = oscil->Pharmonicshiftfirst;
            break;

        case 67:
            if (write)
                oscil->Padaptiveharmonicspar = value;
            else
                value = oscil->Padaptiveharmonicspar;
            break;
        case 68:
            if (write)
                oscil->Padaptiveharmonicsbasefreq = value;
            else
                value = oscil->Padaptiveharmonicsbasefreq;
            break;
        case 69:
            if (write)
                oscil->Padaptiveharmonicspower = value;
            else
                value = oscil->Padaptiveharmonicspower;
            break;
        case 70:
            if (write)
                oscil->Padaptiveharmonics = value;
            else
                value = oscil->Padaptiveharmonics;
            break;

        case 96:
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
        case 97:
            if (write)
                oscil->convert2sine();
            break;
    }
    if (!write)
        getData->data.value = value;
}


void InterChange::commandResonance(CommandBlock *getData, Resonance *respar)
{
    int value = lrint(getData->data.value); // no floats here
    char value_bool = (getData->data.value > 0.5);
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char insert = getData->data.insert;

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    if (insert == 9)
    {
        if (write)
            respar->setpoint(control, value);
        else
            value = respar->Prespoints[control];
        if (!write)
            getData->data.value = value;
        return;
    }

    switch (control)
    {
        case 0:
            if (write)
                respar->PmaxdB = value;
            else
                value = respar->PmaxdB;
            break;
        case 1:
            if (write)
                respar->Pcenterfreq = value;
            else
                value = respar->Pcenterfreq;
            break;
        case 2:
            if (write)
                respar->Poctavesfreq = value;
            else
                value = respar->Poctavesfreq;
            break;

        case 8:
            if (write)
                respar->Penabled = value_bool;
            else
                value = respar->Penabled;
            break;

        case 10:
            if (write)
                respar->randomize(value);
            break;

        case 20:
            if (write)
                respar->interpolatepeaks(value_bool);
            break;
        case 21:
            if (write)
                respar->Pprotectthefundamental = value_bool;
            else
                value = respar->Pprotectthefundamental;
            break;

        case 96:
            if (write)
                for (int i = 0; i < MAX_RESONANCE_POINTS; ++ i)
                    respar->setpoint(i, 64);
            break;
        case 97:
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

    if (engine == 0)
    {
       switch (insertParam)
        {
            case 0:
                lfoReadWrite(getData, part->kit[kititem].adpars->GlobalPar.AmpLfo);
                break;
            case 1:
                lfoReadWrite(getData, part->kit[kititem].adpars->GlobalPar.FreqLfo);
                break;
            case 2:
                lfoReadWrite(getData, part->kit[kititem].adpars->GlobalPar.FilterLfo);
                break;
        }
    }
    else if (engine == 2)
    {
        switch (insertParam)
        {
            case 0:
                lfoReadWrite(getData, part->kit[kititem].padpars->AmpLfo);
                break;
            case 1:
                lfoReadWrite(getData, part->kit[kititem].padpars->FreqLfo);
                break;
            case 2:
                lfoReadWrite(getData, part->kit[kititem].padpars->FilterLfo);
                break;
        }
    }
    else if (engine >= 0x80)
    {
        int nvoice = engine & 0x3f;
        switch (insertParam)
        {
            case 0:
                lfoReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].AmpLfo);
                break;
            case 1:
                lfoReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FreqLfo);
                break;
            case 2:
                lfoReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FilterLfo);
                break;
        }
    }
}


void InterChange::lfoReadWrite(CommandBlock *getData, LFOParams *pars)
{
    bool write = (getData->data.type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    float val = getData->data.value;

    switch (getData->data.control)
    {
        case 0:
            if (write)
                pars->setPfreq(val);
            else
                val = pars->Pfreq;
            break;
        case 1:
            if (write)
                pars->setPintensity(val);
            else
                val = pars->Pintensity;
            break;
        case 2:
            if (write)
                pars->setPdelay(val);
            else
                val = pars->Pdelay;
            break;
        case 3:
            if (write)
                pars->setPstartphase(val);
            else
                val = pars->Pstartphase;
            break;
        case 4:
            if (write)
                pars->setPrandomness(val);
            else
                val = pars->Prandomness;
            break;
        case 5:
            if (write)
                pars->setPLFOtype(lrint(val));
            else
                val = pars->PLFOtype;
            break;
        case 6:
            if (write)
                pars->setPcontinous((val > 0.5f));
            else
                val = pars->Pcontinous;
            break;
        case 7:
            if (write)
                pars->setPfreqrand(val);
            else
                val = pars->Pfreqrand;
            break;
        case 8:
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

    if (engine == 0)
    {
        filterReadWrite(getData, part->kit[kititem].adpars->GlobalPar.GlobalFilter
                    , &part->kit[kititem].adpars->GlobalPar.PFilterVelocityScale
                    , &part->kit[kititem].adpars->GlobalPar.PFilterVelocityScaleFunction);
    }
    else if (engine == 1)
    {
        filterReadWrite(getData, part->kit[kititem].subpars->GlobalFilter
                    , &part->kit[kititem].subpars->PGlobalFilterVelocityScale
                    , &part->kit[kititem].subpars->PGlobalFilterVelocityScaleFunction);
    }
    else if (engine == 2)
    {
        filterReadWrite(getData, part->kit[kititem].padpars->GlobalFilter
                    , &part->kit[kititem].padpars->PFilterVelocityScale
                    , &part->kit[kititem].padpars->PFilterVelocityScaleFunction);
    }
    else if (engine >= 0x80)
    {
        filterReadWrite(getData, part->kit[kititem].adpars->VoicePar[engine & 0x1f].VoiceFilter
                    , &part->kit[kititem].adpars->VoicePar[engine & 0x1f].PFilterVelocityScale
                    , &part->kit[kititem].adpars->VoicePar[engine & 0x1f].PFilterVelocityScaleFunction);
    }
}


void InterChange::filterReadWrite(CommandBlock *getData, FilterParams *pars, unsigned char *velsnsamp, unsigned char *velsns)
{
    bool write = (getData->data.type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    float val = getData->data.value;
    int value_int = lrint(val);

    int nseqpos = getData->data.parameter;
    int nformant = getData->data.parameter;
    int nvowel = getData->data.par2;

    switch (getData->data.control)
    {
        case 0:
            if (write)
                pars->Pfreq = val;
            else
                val = pars->Pfreq;
            break;
        case 1:
            if (write)
                pars->Pq = val;
            else
                val = pars->Pq;
            break;
        case 2:
            if (write)
                pars->Pfreqtrack = val;
            else
                val = pars->Pfreqtrack;
            break;
        case 3:
            if (velsnsamp != NULL)
            {
                if (write)
                    *velsnsamp = value_int;
                else
                    val = *velsnsamp;
            }
            break;
        case 4:
            if (velsns != NULL)
            {
                if (write)
                    *velsns = value_int;
                else
                    val = *velsns;
            }
            break;
        case 5:
            if (write)
            {
                pars->Pgain = val;
                pars->changed = true;
            }
            else
                val = pars->Pgain;
            break;
        case 6:
            if (write)
            {
                pars->Pstages = value_int;
                pars->changed = true;
            }
            else
                val = pars->Pstages;
            break;
        case 7:
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
        case 8:
        case 9:
            if (write)
            {
                pars->Ptype = value_int;
                pars->changed = true;
            }
            else
                val = pars->Ptype;
            break;
        case 10:
            if (write)
            {
                pars->Pfreqtrackoffset = (value_int != 0);
                pars->changed = true;
            }
            else
                val = pars->Pfreqtrackoffset;
            break;

        case 16:
            if (write)
            {
                pars->Pformantslowness = val;
                pars->changed = true;
            }
            else
                val = pars->Pformantslowness;
            break;
        case 17:
            if (write)
            {
                pars->Pvowelclearness = val;
                pars->changed = true;
            }
            else
                val = pars->Pvowelclearness;
            break;
        case 18:
            if (write)
            {
                pars->Pvowels[nvowel].formants[nformant].freq = val;
                pars->changed = true;
            }
            else
                val = pars->Pvowels[nvowel].formants[nformant].freq;
            break;
        case 19:
            if (write)
            {
                pars->Pvowels[nvowel].formants[nformant].q = val;
                pars->changed = true;
            }
            else
                val = pars->Pvowels[nvowel].formants[nformant].q;
            break;
        case 20:
            if (write)
            {
                pars->Pvowels[nvowel].formants[nformant].amp = val;
                pars->changed = true;
            }
            else
                val = pars->Pvowels[nvowel].formants[nformant].amp;
            break;
        case 21:
            if (write)
            {
                pars->Psequencestretch = val;
                pars->changed = true;
            }
            else
                val = pars->Psequencestretch;
            break;
        case 22:
            if (write)
            {
                pars->Pcenterfreq = val;
                pars->changed = true;
            }
            else
                val = pars->Pcenterfreq;
            break;
        case 23:
            if (write)
            {
                pars->Poctavesfreq = val;
                pars->changed = true;
            }
            else
                val = pars->Poctavesfreq;
            break;

        case 32:
            if (write)
            {
                pars->Pnumformants = value_int;
                pars->changed = true;
            }
            else
                val = pars->Pnumformants;
            break;
        case 33: // this is local to the source
            break;
        case 34: // this is local to the source
            break;
        case 35:
            if (write)
            {
                pars->Psequencesize = value_int;
                pars->changed = true;
            }
            else
                val = pars->Psequencesize;
            break;
        case 36:
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
        case 37:
            if (write)
            {
                pars->Psequence[nseqpos].nvowel = value_int;
                pars->changed = true;
            }
            else
                val = pars->Psequence[nseqpos].nvowel;
            break;
        case 38:
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

    string env;
    string name;
    if (engine == 0)
    {
        switch (insertParam)
        {
            case 0:
                envelopeReadWrite(getData, part->kit[kititem].adpars->GlobalPar.AmpEnvelope);
                break;
            case 1:
                envelopeReadWrite(getData, part->kit[kititem].adpars->GlobalPar.FreqEnvelope);
                break;
            case 2:
                envelopeReadWrite(getData, part->kit[kititem].adpars->GlobalPar.FilterEnvelope);
                break;
        }
    }
    else if (engine == 1)
    {
        switch (insertParam)
        {
            case 0:
                envelopeReadWrite(getData, part->kit[kititem].subpars->AmpEnvelope);
                break;
            case 1:
                envelopeReadWrite(getData, part->kit[kititem].subpars->FreqEnvelope);
                break;
            case 2:
                envelopeReadWrite(getData, part->kit[kititem].subpars->GlobalFilterEnvelope);
                break;
            case 3:
                envelopeReadWrite(getData, part->kit[kititem].subpars->BandWidthEnvelope);
                break;
        }
    }
    else if (engine == 2)
    {
        switch (insertParam)
        {
            case 0:
                envelopeReadWrite(getData, part->kit[kititem].padpars->AmpEnvelope);
                break;
            case 1:
                envelopeReadWrite(getData, part->kit[kititem].padpars->FreqEnvelope);
                break;
            case 2:
                envelopeReadWrite(getData, part->kit[kititem].padpars->FilterEnvelope);
                break;
        }
    }
    else if (engine >= 0x80)
    {
        int nvoice = engine & 0x3f;
        if (engine >= 0xC0)
        {
            switch (insertParam)
            {
                case 0:
                    envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FMAmpEnvelope);
                    break;
                case 1:
                    envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FMFreqEnvelope);
                    break;
            }
        }
        else
        {
            switch (insertParam)
            {
                case 0:
                    envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].AmpEnvelope);
                    break;
                case 1:
                    envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FreqEnvelope);
                    break;
                case 2:
                    envelopeReadWrite(getData, part->kit[kititem].adpars->VoicePar[nvoice].FilterEnvelope);
                    break;
            }
        }
    }
}


void InterChange::envelopeReadWrite(CommandBlock *getData, EnvelopeParams *pars)
{
    int val = lrint(getData->data.value); // these are all integers or bool
    bool write = (getData->data.type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    unsigned char point = getData->data.control;
    unsigned char insert = getData->data.insert;
    unsigned char Xincrement = getData->data.par2;

    int envpoints = pars->Penvpoints;
    bool isAddpoint = (Xincrement < 0xff);

    if (insert == 3) // here be dragons :(
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

    if (insert == 4)
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
        case 0:
            if (write)
                pars->PA_val = val;
            else
                val = pars->PA_val;
            break;
        case 1:
            if (write)
                pars->PA_dt = val;
            else
                val = pars->PA_dt;
            break;
        case 2:
            if (write)
                pars->PD_val = val;
            else
                val = pars->PD_val;
            break;
        case 3:
            if (write)
                pars->PD_dt = val;
            else
                val = pars->PD_dt;
            break;
        case 4:
            if (write)
                pars->PS_val = val;
            else
                val = pars->PS_val;
            break;
        case 5:
            if (write)
                pars->PR_dt = val;
            else
                val = pars->PR_dt;
            break;
        case 6:
            if (write)
                pars->PR_val = val;
            else
                val = pars->PR_val;
            break;
        case 7:
            if (write)
                pars->Penvstretch = val;
            else
                val = pars->Penvstretch;
            break;

        case 16:
            if (write)
                pars->Pforcedrelease = (val != 0);
            else
                val = pars->Pforcedrelease;
            break;
        case 17:
            if (write)
                pars->Plinearenvelope = (val != 0);
            else
                val = pars->Plinearenvelope;
            break;

        case 24: // this is local to the source
            break;

        case 32:
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
        case 34:
            if (!pars->Pfreemode)
            {
                val = 0xff;
                Xincrement = 0xff;
            }
            else
                Xincrement = envpoints;
            break;
        case 35:
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

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    int value_int = lrint(value);

    bool isSysEff = (npart == 0xf1);
    if (insert == 0xff)
    {
        switch (control)
        {
            case 0: // only relevant to GUI
                break;
            case 1:
                if (write)
                {
                    if (isSysEff)
                        synth->sysefx[effnum]->changeeffect(value_int);
                    else
                        synth->insefx[effnum]->changeeffect(value_int);
                }
                else
                {
                    if (isSysEff)
                        value = synth->sysefx[effnum]->geteffect();
                    else
                        value = synth->insefx[effnum]->geteffect();
                }
                break;
            case 2: // insert only
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
    unsigned char type = getData->data.type;
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit & 0x7f ;
    unsigned char effnum = getData->data.engine;

    bool write = (type & 0x40) > 0;
    if (write)
        __sync_or_and_fetch(&blockRead, 1);

    EffectMgr *eff;

    if (npart == 0xf1)
        eff = synth->sysefx[effnum];

    else if (npart == 0xf2)
        eff = synth->insefx[effnum];
    else if (npart < NUM_MIDI_PARTS)
        eff = synth->part[npart]->partefx[effnum];
    else
        return; // invalid part number
    if (kititem > 8)
        return;
    if (kititem == 8 && getData->data.insert < 0xff)
    {
        filterReadWrite(getData, eff->filterpars,NULL,NULL);
        return;
    }

    if (write)
    {
        if (kititem == 7)
        /*
         * specific to EQ
         * Control 1 is not a saved parameter, but a band index.
         * Also, EQ does not have presets, and 16 is the control
         * for the band 1 frequency parameter
        */
        {
            if (control <= 1)
                eff->seteffectpar(control, lrint(value));
            else
            {
                eff->seteffectpar(control + (eff->geteffectpar(1) * 5), lrint(value));
                getData->data.parameter = eff->geteffectpar(1);
            }
        }
        else
        {
            if (control == 16)
                eff->changepreset(lrint(value));
            else
                eff->seteffectpar(control, lrint(value));
        }
        //cout << "eff value " << value << "  control " << int(control) << "  band " << synth->getRuntime().EQband << endl;
    }
    else
    {
        if (kititem == 7 && control > 1) // specific to EQ
        {
            value = eff->geteffectpar(control + (eff->geteffectpar(1) * 5));
            getData->data.parameter = eff->geteffectpar(1);
        }
        else
            value = eff->geteffectpar(control);
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
    if (getData->data.part == 0xf8 && (control == 65 || control == 67 || control == 71))
    {
        getData->data.par2 = 0xff; // just to be sure
        if (value > 119)
            return;
        string text;
        if (control == 65)
        {
            text = synth->getRuntime().masterCCtest(int(value));
            if (text != "")
                getData->data.par2 = miscMsgPush(text);
            return;
        }
        if(control == 67)
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
    int request = int(getData->data.type & 3);

    //cout << "Top  Control " << control << " Part " << (int) getData->data.part << "  Kit " << kititem << " Engine " << engine << "  Insert " << insert << endl;

    //cout << "Top request " << request << endl;

    getData->data.type &= 0x3f; //  clear top bits
    getData->data.type |= 0x80; // default is integer & not learnable

    if (npart == 248) // config limits
        return synth->getConfigLimits(getData);

    if (npart == 240) // main control limits
        return synth->getLimits(getData);

    if (npart == 232) // microtonal limits
        return synth->microtonal.getLimits(getData);

    if (npart == 192) // vector limits
        return synth->getVectorLimits(getData);

    float min;
    float max;
    float def;

    if (kititem >= 0x80 && kititem <= 0x88) // effects.
    {
        LimitMgr limits;
        return limits.geteffectlimits(getData);
    }

    if (npart < NUM_MIDI_PARTS)
    {
        Part *part;
        part = synth->part[npart];

        if (engine == 1 && (insert == 0xff || (insert >= 5 && insert <= 7)) && parameter == 0xff)
        {
            SUBnoteParameters *subpars;
            subpars = part->kit[kititem].subpars;
            return subpars->getLimits(getData);
        }

        if ((engine & 0x7f) == 0x7f && (kititem == 0xff || insert == 0x20)) // part level controls
        { // TODO why is engine not 0xff? it used to be.
            return part->getLimits(getData);
        }
        if ((insert == 0x20 || insert == 0xff) && parameter == 0xff && par2 == 0xff)
        {
            if (engine == 0 || (engine >= 0x80 && engine <= 0x8f))
            {
                ADnoteParameters *adpars;
                adpars = part->kit[kititem].adpars;
                return adpars->getLimits(getData);
            }
            if (engine == 1)
            {
                SUBnoteParameters *subpars;
                subpars = part->kit[kititem].subpars;
                return subpars->getLimits(getData);
            }
            if (engine == 2)
            {
                PADnoteParameters *padpars;
                padpars = part->kit[kititem].padpars;
                return padpars->getLimits(getData);
            }
            // there may be other stuff

            min = 0;
            max = 127;
            def = 0;

            cout << "Using engine defaults" << endl;
            switch (request)
            {
                case 0:
                    if(value < min)
                        value = min;
                    else if(value > max)
                        value = max;
                    break;
                case 1:
                    value = min;
                    break;
                case 2:
                    value = max;
                    break;
                case 3:
                    value = def;
                    break;
            }
            return value;
        }
        if (insert >= 5 && insert <= 7)
        {
            return part->kit[0].adpars->VoicePar[0].OscilSmp->getLimits(getData);
            // we also use this for pad limits
            // as oscillator values identical
        }
        if (insert == 8) // resonance
        {
            if (control == 0) // a cheat!
            {
                min = 1;
                max = 90;
                def = 50;

                switch (request)
                {
                    case 0:
                        if(value < min)
                            value = min;
                        else if(value > max)
                            value = max;
                        break;
                    case 1:
                        value = min;
                        break;
                    case 2:
                        value = max;
                        break;
                    case 3:
                        value = def;
                        break;
                }
                return value;
            }
            // there may be other stuff
            min = 0;
            max = 127;
            def = 0;

            cout << "Using resonance defaults" << endl;
            switch (request)
            {
                case 0:
                    if(value < min)
                        value = min;
                    else if(value > max)
                        value = max;
                    break;
                case 1:
                    value = min;
                    break;
                case 2:
                    value = max;
                    break;
                case 3:
                    value = def;
                    break;
            }
            return value;
        }
        if (insert == 0 && parameter <= 2) // LFO
        {
            if (control == 0) // another cheat!
            {
                getData->data.type = 0x40;
                min = 0;
                max = 1;
                def = 0.5f;
                switch (request)
                {
                    case 0:
                        if(value < min)
                            value = min;
                        else if(value > max)
                            value = max;
                        break;
                    case 1:
                        value = min;
                        break;
                    case 2:
                        value = max;
                        break;
                    case 3:
                        value = def;
                        break;
                }
                return value;
            }
            min = 0;
            max = 127;
            def = 0;
            cout << "Using LFO defaults" << endl;

            switch (request)
            {
                case 0:
                    if(value < min)
                        value = min;
                    else if(value > max)
                        value = max;
                    break;
                case 1:
                    value = min;
                    break;
                case 2:
                    value = max;
                    break;
                case 3:
                    value = def;
                    break;
            }
            return value;
        }
        min = 0;
        max = 127;
        def = 0;
        cout << "Using defaults" << endl;

            switch (request)
            {
                case 0:
                    if(value < min)
                        value = min;
                    else if(value > max)
                        value = max;
                break;
                case 1:
                    value = min;
                    break;
                case 2:
                    value = max;
                    break;
                case 3:
                    value = def;
                    break;
            }
            return value;
    }
    min = 0;
    max = 127;
    def = 0;
    cout << "Using unknown part number defaults" << endl;

    switch (request)
    {
        case 0:
            if(value < min)
                value = min;
            else if(value > max)
                value = max;
        break;
        case 1:
            value = min;
            break;
        case 2:
            value = max;
            break;
        case 3:
            value = def;
            break;
    }
    return value;
}
