/*
    SynthEngine.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2014-2016, Will Godfrey & others

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

    This file is derivative of original ZynAddSubFX code, last modified March 2016
*/

#include<stdio.h>
#include <sys/time.h>
#include <set>

using namespace std;

#include "MasterUI.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>


static unsigned int getRemoveSynthId(bool remove = false, unsigned int idx = 0)
{
    static set<unsigned int> idMap;
    if (remove)
    {
        if (idMap.count(idx) > 0)
            idMap.erase(idx);
        return 0;
    }
    else if (idx > 0)
    {
        if (idMap.count(idx) == 0)
        {
            idMap.insert(idx);
            return idx;
        }
    }
    set<unsigned int>::const_iterator itEnd = idMap.end();
    set<unsigned int>::const_iterator it;
    unsigned int nextId = 0;
    for (it = idMap.begin(); it != itEnd && nextId == *it; ++it, ++nextId)
    {
    }
    idMap.insert(nextId);
    return nextId;
}

// histories
static vector<string> ParamsHistory;
static vector<string> ScaleHistory;
static vector<string> StateHistory;
static vector<string> VectorHistory;


SynthEngine::SynthEngine(int argc, char **argv, bool _isLV2Plugin, unsigned int forceId) :
    uniqueId(getRemoveSynthId(false, forceId)),
    isLV2Plugin(_isLV2Plugin),
    bank(this),
    Runtime(this, argc, argv),
    presetsstore(this),
    shutup(false),
    samplerate(48000),
    samplerate_f(samplerate),
    halfsamplerate_f(samplerate / 2),
    buffersize(512),
    buffersize_f(buffersize),
    oscilsize(1024),
    oscilsize_f(oscilsize),
    halfoscilsize(oscilsize / 2),
    halfoscilsize_f(halfoscilsize),
    p_buffersize(0),
    p_bufferbytes(0),
    p_buffersize_f(0),
    ctl(NULL),
    microtonal(this),
    fft(NULL),
    muted(0xFF),
    tmpmixl(NULL),
    tmpmixr(NULL),
    processLock(NULL),
    vuringbuf(NULL),
    RBPringbuf(NULL),
    stateXMLtree(NULL),
    guiMaster(NULL),
    guiClosedCallback(NULL),
    guiCallbackArg(NULL),
    LFOtime(0),
    windowTitle("Yoshimi" + asString(uniqueId))
{
    if (bank.roots.empty())
        bank.addDefaultRootDirs();
    memset(&random_state, 0, sizeof(random_state));

    ctl = new Controller(this);
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart] = NULL;
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx] = NULL;
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx] = NULL;
    shutup = false;
}


SynthEngine::~SynthEngine()
{
    closeGui();
    if (vuringbuf)
        jack_ringbuffer_free(vuringbuf);
    if (RBPringbuf)
        jack_ringbuffer_free(RBPringbuf);

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if (part[npart])
            delete part[npart];

    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        if (insefx[nefx])
            delete insefx[nefx];

    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        if (sysefx[nefx])
            delete sysefx[nefx];

    if (tmpmixl)
        fftwf_free(tmpmixl);
    if (tmpmixr)
        fftwf_free(tmpmixr);
    if (fft)
        delete fft;
    pthread_mutex_destroy(&processMutex);
    sem_destroy(&partlock);
    if (ctl)
        delete ctl;
    getRemoveSynthId(true, uniqueId);

}


bool SynthEngine::Init(unsigned int audiosrate, int audiobufsize)
{
    samplerate_f = samplerate = audiosrate;
    halfsamplerate_f = samplerate_f / 2;
    buffersize_f = buffersize = Runtime.Buffersize;
    if (buffersize_f > audiobufsize)
        buffersize_f = audiobufsize;
     // because its now *groups* of audio buffers.
    p_all_buffersize_f = buffersize_f;

    bufferbytes = buffersize * sizeof(float);
    oscilsize_f = oscilsize = Runtime.Oscilsize;
    halfoscilsize_f = halfoscilsize = oscilsize / 2;
    fadeStep = 10.0f / samplerate; // 100mS fade;
    int found = 0;

    if (!pthread_mutex_init(&processMutex, NULL))
        processLock = &processMutex;
    else
    {
        Runtime.Log("SynthEngine actionLock init fails :-(");
        processLock = NULL;
        goto bail_out;
    }

    memset(random_state, 0, sizeof(random_state));
    memset(&random_buf, 0, sizeof(random_buf));

    if (initstate_r(samplerate + buffersize + oscilsize, random_state,
                    sizeof(random_state), &random_buf))
        Runtime.Log("SynthEngine Init failed on general randomness");

    if (oscilsize < (buffersize / 2))
    {
        Runtime.Log("Enforcing oscilsize to half buffersize, "
                    + asString(oscilsize) + " -> " + asString(buffersize / 2));
        oscilsize_f = oscilsize = buffersize / 2;
        halfoscilsize_f = halfoscilsize = oscilsize / 2;
    }

    if (!(fft = new FFTwrapper(oscilsize)))
    {
        Runtime.Log("SynthEngine failed to allocate fft");
        goto bail_out;
    }

    if (!(vuringbuf = jack_ringbuffer_create(sizeof(VUtransfer))))
    {
        Runtime.Log("SynthEngine failed to create vu ringbuffer");
        goto bail_out;
    }

    if (!(RBPringbuf = jack_ringbuffer_create(512)))
    {
        Runtime.Log("SynthEngine failed to create GUI ringbuffer");
        goto bail_out;
    }

    tmpmixl = (float*)fftwf_malloc(bufferbytes);
    tmpmixr = (float*)fftwf_malloc(bufferbytes);
    if (!tmpmixl || !tmpmixr)
    {
        Runtime.Log("SynthEngine tmpmix allocations failed");
        goto bail_out;
    }

    sem_init(&partlock, 0, 1);

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart] = new Part(&microtonal, fft, this);
        if (!part[npart])
        {
            Runtime.Log("Failed to allocate new Part");
            goto bail_out;
        }
        VUpeak.values.parts[npart] = -0.2;
    }

    // Insertion Effects init
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        if (!(insefx[nefx] = new EffectMgr(1, this)))
        {
            Runtime.Log("Failed to allocate new Insertion EffectMgr");
            goto bail_out;
        }
    }

    // System Effects init
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        if (!(sysefx[nefx] = new EffectMgr(0, this)))
        {
            Runtime.Log("Failed to allocate new System Effects EffectMgr");
            goto bail_out;
        }
    }

    defaults();
    ClearNRPNs();
    if (Runtime.restoreJackSession) // the following are not fatal if failed
    {
        if (!Runtime.restoreJsession())
        {
            Runtime.Log("Restore jack session failed. Using defaults");
            defaults();
        }
    }
    else if (Runtime.restoreState)
    {
        if (!Runtime.stateRestore())
         {
             Runtime.Log("Restore state failed. Using defaults");
             defaults();
         }
    }
    else
    {
        if (Runtime.paramsLoad.size())
        {
            if (loadXML(Runtime.paramsLoad))
            {
                applyparameters();
                addHistory(Runtime.paramsLoad, 2);
                Runtime.Log("Loaded " + Runtime.paramsLoad + " parameters");
            }
            else
            {
                Runtime.Log("Failed to load parameters " + Runtime.paramsLoad);
                Runtime.paramsLoad = "";
            }
        }
        else if (Runtime.instrumentLoad.size())
        {
            int loadtopart = 0;
            if (part[loadtopart]->loadXMLinstrument(Runtime.instrumentLoad))
                Runtime.Log("Instrument file " + Runtime.instrumentLoad + " loaded");
            else
            {
                Runtime.Log("Failed to load instrument file " + Runtime.instrumentLoad);
                Runtime.instrumentLoad = "";
            }
        }
    }

    if (Runtime.rootDefine.size())
    {
        found = bank.addRootDir(Runtime.rootDefine);
        if (found)
        {
            cout << "Defined new root ID " << asString(found) << " as " << Runtime.rootDefine << endl;
            bank.scanrootdir(found);
        }
        else
            cout << "Can't find path " << Runtime.rootDefine << endl;
    }


    if (!Runtime.startThread(&RBPthreadHandle, _RBPthread, this, true, 7, false))
    {
        Runtime.Log("Failed to start RBP thread");
        goto bail_out;
    }
    return true;


bail_out:
    if (fft)
        delete fft;
    fft = NULL;

    if (vuringbuf)
        jack_ringbuffer_free(vuringbuf);
    vuringbuf = NULL;

    if (RBPringbuf)
        jack_ringbuffer_free(RBPringbuf);
    RBPringbuf = NULL;

    if (tmpmixl)
        fftwf_free(tmpmixl);
    tmpmixl = NULL;

    if (tmpmixr)
        fftwf_free(tmpmixr);
    tmpmixr = NULL;

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (part[npart])
            delete part[npart];
        part[npart] = NULL;
    }

    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        if (insefx[nefx])
            delete insefx[nefx];
        insefx[nefx] = NULL;
    }

    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        if (sysefx[nefx])
            delete sysefx[nefx];
        sysefx[nefx] = NULL;
    }
    return false;
}


void *SynthEngine::_RBPthread(void *arg)
{
    return static_cast<SynthEngine*>(arg)->RBPthread();
}


void *SynthEngine::RBPthread(void)
{
    struct RBP_data block;
    unsigned int readsize = sizeof(RBP_data);
    memset(block.data, 0, readsize);
    char *point;
    unsigned int toread;
    unsigned int read;
    unsigned int found;
    unsigned int tries;
    while (Runtime.runSynth)
    {
        if (jack_ringbuffer_read_space(RBPringbuf) >= readsize)
        {
            toread = readsize;
            read = 0;
            tries = 0;
            point = (char*)&block;
            while (toread && tries < 3)
            {
                found = jack_ringbuffer_read(RBPringbuf, point, toread);
                read += found;
                point += found;
                toread -= found;
                ++tries;
            }
            if (!toread)
            {
                switch ((unsigned char)block.data[0])
                {
                    case 1:
                        SetBankRoot(block.data[1]);
                        break;

                    case 2:
                        SetBank(block.data[1]);
                        break;

                    case 3:
                        SetProgram(block.data[1], block.data[2]);
                        break;
                }
            }
            else
                Runtime.Log("Unable to read data from Root/bank/Program");
        }
        else
            usleep(100); // yes it's a hack but is totally reliable
    }
    return NULL;
}


void SynthEngine::defaults(void)
{
    setPvolume(90);
    setPkeyshift(64);
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart]->defaults();
        part[npart]->Prcvchn = npart % NUM_MIDI_CHANNELS;
    }

    partonoffWrite(0, 1); // enable the first part
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        insefx[nefx]->defaults();
        Pinsparts[nefx] = -1;
    }

    // System Effects init
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        sysefx[nefx]->defaults();
        for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
            setPsysefxvol(npart, nefx, 0);
        for (int nefxto = 0; nefxto < NUM_SYS_EFX; ++nefxto)
            setPsysefxsend(nefx, nefxto, 0);
    }
    microtonal.defaults();
    Runtime.currentPart = 0;
    //CmdInterface.defaults(); // **** need to work out how to call this
    Runtime.NumAvailableParts = NUM_MIDI_CHANNELS;
    ShutUp();
}


// Note On Messages (velocity == 0 => NoteOff)
void SynthEngine::NoteOn(unsigned char chan, unsigned char note, unsigned char velocity)
{
    if (!velocity)
        this->NoteOff(chan, note);
    else if (!isMuted())
        for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (chan == part[npart]->Prcvchn)
            {
               if (partonoffRead(npart))
                {
                    actionLock(lock);
                    part[npart]->NoteOn(note, velocity, keyshift);
                    actionLock(unlock);
                }
                else if (VUpeak.values.parts[npart] > (-velocity))
                    VUpeak.values.parts[npart] = -(0.2 + velocity); // ensure fake is always negative
            }
        }
}


// Note Off Messages
void SynthEngine::NoteOff(unsigned char chan, unsigned char note)
{
    for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
    {
        // mask values 16 - 31 to still allow a note off
        if (chan == (part[npart]->Prcvchn & 0xef) && partonoffRead(npart))
        {
            actionLock(lock);
            part[npart]->NoteOff(note);
            actionLock(unlock);
        }
    }
}


// Controllers
void SynthEngine::SetController(unsigned char chan, int type, short int par)
{
    if (type == Runtime.midi_bank_C)
    {
        SetBank(par); //shouldn't get here. Banks are set directly via SetBank method from MusicIO class
    }
    else
    { // bank change doesn't directly affect parts.
        int npart;
        if (chan < NUM_MIDI_CHANNELS)
        {
            for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
            {   // Send the controller to all part assigned to the channel
                if (chan == part[npart]->Prcvchn && partonoffRead(npart))
                {
                    part[npart]->SetController(type, par);
                    if (type == 7 || type == 10) // currently only volume and pan
                        GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePanelItem, npart);
                }
            }
        }
        else
        {
            npart = chan & 0x7f;
            if (npart < Runtime.NumAvailableParts)
            {
                part[npart]->SetController(type, par);
                if (type == 7 || type == 10) // currently only volume and pan
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePanelItem, npart);
            }
        }
        if (type == C_allsoundsoff)
        {   // cleanup insertion/system FX
            for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
                sysefx[nefx]->cleanup();
            for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
                insefx[nefx]->cleanup();
        }
    }
}


void SynthEngine::SetZynControls()
{
    unsigned char parnum = Runtime.dataH;
    unsigned char value = Runtime.dataL;

    if (parnum <= 0x7f && value <= 0x7f)
    {
        Runtime.dataL = 0xff; // use once then clear it out
        unsigned char effnum = Runtime.nrpnL;
        unsigned char efftype = (parnum & 0x60);
        int data = (effnum << 8);
        parnum &= 0x1f;

        if (Runtime.nrpnH == 8)
        {
            data |= (1 << 22);
            if (efftype == 0x40) // select effect
            {
                actionLock(lockmute);
                insefx[effnum]->changeeffect(value);
                actionLock(unlock);
            }
            else if (efftype == 0x20) // select part
            {
                if (value >= 0x7e)
                    Pinsparts[effnum] = value - 0x80; // set for 'Off' and 'Master out'
                else if (value < Runtime.NumAvailableParts)
                    Pinsparts[effnum] = value;
            }
            else
                insefx[effnum]->seteffectpar(parnum, value);
            data |= ((Pinsparts[effnum] + 2) << 24); // needed for both operations
        }
        else
        {
            if (efftype == 0x40) // select effect
                sysefx[effnum]->changeeffect(value);
            else if (efftype == 0x20) // select output level
            {
                // setPsysefxvol(effnum, parnum, value); // this isn't correct!

            }
            else
                sysefx[effnum]->seteffectpar(parnum, value);
        }
        GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateEffects, data);
    }
}


void SynthEngine::SetEffects(unsigned char category, unsigned char command, unsigned char nFX, unsigned char nType, int nPar, unsigned char value)
{
    // category 0-sysFX, 1-insFX, 2-partFX
    // command 1-set effect, 4-set param, 8-set preset

    int npart = getRuntime().currentPart;
    int data = (nFX) << 8;

    switch (category)
    {
        case 1:
            data |= (1 << 22);

            switch (command)
            {
                case 1:
                    insefx[nFX]->changeeffect(nType);
                    data |= ((Pinsparts[nFX] + 2) << 24);
                    break;

                case 4:
                    Pinsparts[nFX] = nPar;
                    data |= ((nPar + 2) << 24);
                    break;

                case 8:
                    insefx[nFX]->changepreset(value);
                    data |= ((Pinsparts[nFX] + 2) << 24);
                    break;
            }
            break;

        case 2:
            data |= (2 << 22);
            switch (command)
            {
                case 1:
                    part[npart]->partefx[nFX]->changeeffect(nType);
                    break;

                case 4:
                    setPsysefxvol(npart, nPar, value);
                    break;

                case 8:
                    part[npart]->partefx[nFX]->changepreset(value);
                    break;
            }
            break;

        default:
            switch (command)
            {
                case 1:
                    sysefx[nFX]->changeeffect(nType);
                    break;

                case 4:
                    setPsysefxsend(nFX, nPar, value);
                    break;

                case 8:
                    sysefx[nFX]->changepreset(value);
                    break;
            }
            break;
    }
    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateEffects, data);
}


void SynthEngine::SetBankRoot(int rootnum)
{
    string name;
    int currentRoot;
    if (bank.setCurrentRootID(rootnum))
    {
        if (Runtime.showGui)
        {
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateBankRootDirs, 0);
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::RescanForBanks, 0);
        }
        currentRoot = bank.getCurrentRootID();
        if (rootnum != currentRoot)
            name = "Cant find ID " + asString(rootnum) + ". Current root is ";
        else name = "Root set to ";
        Runtime.Log(name + asString(currentRoot) + " " + bank.getRootPath(currentRoot));
    }
    else
        Runtime.Log("No match for root ID " + asString(rootnum));
}


int SynthEngine::ReadBankRoot(void)
{
    return bank.currentRootID;
}


void SynthEngine::SetBank(int banknum)
{
    /*  we use either msb or lsb for bank changes
    128 banks is enough for anybody :-)
    this is configurable to suit different hardware synths
    */

    //new implementation uses only 1 call :)
    if (bank.setCurrentBankID(banknum, true))
    {
        if (Runtime.showGui)
        {
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::RefreshCurBank, 0);
        }
        Runtime.Log("Bank set to " + asString(banknum) + " " + bank.roots [bank.currentRootID].banks [banknum].dirname);
    }
    else
    {
        Runtime.Log("No bank " + asString(banknum)+ " in this root. Current bank is " + asString(ReadBank()));
    }
}


int SynthEngine::ReadBank(void)
{
    return bank.currentBankID;
}


void SynthEngine::commandFetch(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char insertParam)
{
    /*
     * while testing, this simply sends everything to commandSend but eventually it will
     * partially do the decding and direction via ring buffers for actualy control.
     */
    //commandSend(value, type, control, part, kit, engine, insert, insertParam);
    return;
}


void SynthEngine::commandSend(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char insertParam)
{
    bool isGui = type & 0x20;
    char button = type & 0x1f;
    string isf;
    if (isGui && button != 2)
    {
        if (!(type &0x80))
            isf = "f";
        Runtime.Log("\nButton " + asString((int) type & 7) + "\nPart " + asString((int) part) + "\nKit " + asString((int) kit) + "\nEngine " + asString((int) engine) + "\nInsert " + asString((int) insert) + "  Insert Param " + asString((int) insertParam) + "\nControl " + asString((int) control) + "  Value " + asString(value) + isf);
        return;
    }
    if (part == 0xc0)
        commandVector(value, type, control);
    else if (part == 0xf0)
        commandMain(value, type, control);
    else if ((part == 0xf1 || part == 0xf2) && kit == 0xff)
        commandSysIns(value, type, control, part, engine, insert);
    else if (kit == 0xff || (kit & 0x20))
        commandPart(value, type, control, part, kit, engine);
    else if (kit >= 0x80)
    {
        if (insert < 0xff)
            commandFilter(value, type, control, part, kit, engine, insert);
        else
            commandEffects(value, type, control, part, kit, engine);
    }
    else if (engine == 2)
    {
        switch(insert)
        {
            case 0xff:
                commandPad(value, type, control, part, kit);
                break;
            case 0:
                commandLFO(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 1:
                commandFilter(value, type, control, part, kit, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 5:
            case 6:
            case 7:
                commandOscillator(value, type, control, part, kit, engine, insert);
                break;
            case 8:
            case 9:
                commandResonance(value, type, control, part, kit, engine, insert);
                break;
        }
    }
    else if (engine == 1)
    {
        switch (insert)
        {
            case 0xff:
                commandSub(value, type, control, part, kit, insert);
                break;
            case 1:
                commandFilter(value, type, control, part, kit, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, part, kit, engine, insert, insertParam);
                break;
        }
    }
    else if (engine >= 0x80)
    {
        switch (insert)
        {
            case 0xff:
                commandAddVoice(value, type, control, part, kit, engine);
                break;
            case 0:
                commandLFO(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 1:
                commandFilter(value, type, control, part, kit, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 5:
            case 6:
            case 7:
                commandOscillator(value, type, control, part, kit, engine, insert);
                break;
        }
    }
    else if (engine == 0)
    {
        switch (insert)
        {
            case 0xff:
                commandAdd(value, type, control, part, kit);
                break;
            case 0:
                commandLFO(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 1:
                commandFilter(value, type, control, part, kit, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 8:
                commandResonance(value, type, control, part, kit, engine, insert);
                break;
        }
    }
    // just do nothing if not recognised
}


void SynthEngine::commandVector(float value, unsigned char type, unsigned char control)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Base Channel";
            break;
        case 1:
            contstr = "Options";
            break;

        case 16:
        case 32:
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
        case 33:
            contstr = "Up Instrument";
            break;
        case 34:
            contstr = "Down Instrument";
            break;
    }
    string name = "Vector ";
    if (control >= 32)
        name += "Y ";

    else if(control >= 16)
        name += "X ";
    Runtime.Log(name + contstr + " value " + actual);
}


void SynthEngine::commandMain(float value, unsigned char type, unsigned char control)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;

        case 14:
            contstr = "Part Number";
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

        case 96:
            contstr = "Reset All";
            break;
        case 128:
            contstr = "Stop";
            break;
    }
    Runtime.Log("Main " + contstr + " value " + actual);
}


void SynthEngine::commandPart(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);


    string kitnum;
    if (kit < 0xff)
        kitnum = "  Kit " + to_string(kit & 0x1f) + " ";
    else
        kitnum = "  ";

    string name;
    if (control >= 0x80)
    {
        if (control >= 0xe0)
            name = "";
        else
        {
            name = "Controller ";
            if (control >= 0xa0)
                name += "Portamento ";
        }
    }
    else
    {
        switch (engine) // needs aligning with other engine numbers
        {
            case 1:
                name = "AddSynth ";
                break;
            case 2:
                name = "SubSynth ";
                break;
            case 3:
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
            contstr = "Midi";
            break;
        case 6:
            contstr = "Mode";
            break;
        case 7:
            contstr = "Portamento";
            break;
        case 8:
            contstr = "Enable";
            break;
        case 9:
            contstr = "Mute";
            break;

        case 16:
            contstr = "Min Note";
            break;
        case 17:
            contstr = "Max Note";
            break;
        case 18:
            contstr = "Min To Last";
            break;
        case 19:
            contstr = "Max To Last";
            break;

        case 33:
            contstr = "Key Limit";
            break;
        case 35:
            contstr = "Key Shift";
            break;

        case 40:
            contstr = "Effect Send 0";
            break;
        case 41:
            contstr = "Effect Send 1";
            break;
        case 42:
            contstr = "Effect Send 2";
            break;
        case 43:
            contstr = "Effect Send 3";
            break;

        case 48:
            contstr = "Humanise";
            break;

        case 56:
            contstr = "Mode";
            break;
        case 57:
            contstr = "Drum Mode";
            break;
        case 58:
            contstr = "Kit Mode";
            break;

        case 64:
            contstr = "Effect Number";
            break;

        case 96:
            contstr = "Reset Note Range";
            break;

        case 120:
            contstr = "Audio destination";
            break;

        case 128:
            contstr = "Vol Range";
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

        case 224:
            contstr = "Clear";
            break;
    }
    Runtime.Log("Part " + to_string(part) + kitnum + name + contstr + " value " + actual);
}


void SynthEngine::commandAdd(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

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
        case 39:
            contstr = "Rel B Wdth";
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
    }

    Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  AddSynth " + name + contstr + " value " + actual);
}


void SynthEngine::commandAddVoice(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit,unsigned char engine)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name = "";
    switch (control & 0xF0)
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
            contstr = "Detune";
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
    }

    Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  AddSynth Voice " + to_string(engine & 0x1f) + name + contstr + " value " + actual);
}


void SynthEngine::commandSub(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    if (insert == 6)
    {
        Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  Subsynth Harmonic " + to_string(control) + " Amplitude value " + actual);
        return;
    }

    if (insert == 7)
    {
        Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  Subsynth Harmonic " + to_string(control) + " Bandwidth value " + actual);
        return;
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
            contstr = "Bandwidth";
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
            contstr = "Env Ena";
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
    }

    Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  SubSynth " + name + contstr + " value " + actual);
}


void SynthEngine::commandPad(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

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
            contstr = "Env Ena";
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
            contstr = "Par 1";
            break;
        case 72:
            contstr = "Par 2";
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
            contstr = "Size";
            break;

        case 104:
            contstr = "Apply Changes";
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
    }

    Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  PadSynth " + name + contstr + " value " + actual);
}


void SynthEngine::commandOscillator(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string eng_name;
    if (engine == 2)
        eng_name = "  Padsysnth";
    else
    {
        eng_name = "  Addsynth Voice " + to_string( engine & 0x3f);
        if (engine & 0x40)
            eng_name += " Modulator";
    }

    if (insert == 6)
    {
        Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + eng_name + " Harmonic " + to_string(control) + " Amplitude value " + actual);
        return;
    }
    else if(insert == 7)
    {
        Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + eng_name + " Harmonic " + to_string(control) + " Phase value " + actual);
        return;
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

        case 32:
            contstr = " Osc Autoclear";
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

        case 104:
            contstr = " Apply Changes";
            break;
    }

    Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + eng_name + name + contstr + "  value " + actual);

}


void SynthEngine::commandResonance(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (engine == 0)
        name = "  AddSynth";
    else
        name = "  PadSynth";

    if (insert == 9)
    {
        Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name + " Resonance Point " + to_string(control) + "  value " + actual);
        return;
    }

    string contstr;
    switch (control)
    {
        case 0:
            contstr = "Max dB";
            break;
        case 1:
            contstr = "Centre Freq";
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

        case 104:
            contstr = "Apply Changes";
            break;
    }

    Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name + " Resonance " + contstr + "  value " + actual);
}


void SynthEngine::commandLFO(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (engine == 0)
        name = "  AddSynth";
    else if (engine == 2)
        name = "  PadSynth";
    else if (engine >= 0x80)
        name = "  AddSynth Voice " + to_string(engine & 0x3f);

    string lfo;
    switch (parameter)
    {
        case 0:
            lfo = "  Amp";
            break;
        case 1:
            lfo = "  Freq";
            break;
        case 2:
            lfo = "  Filt";
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
    }

    Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name + lfo + " LFO  " + contstr + " value " + actual);
}


void SynthEngine::commandFilter(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (kit >= 0x80)
    {
        string efftype;
        switch (kit & 0xf)
        {
            case 0:
                efftype = " NO Effect";
                break;
            case 1:
                efftype = " Reverb";
                break;
            case 2:
                efftype = " Echo";
                break;
            case 3:
                efftype = " Chorus";
                break;
            case 4:
                efftype = " Phaser";
                break;
            case 5:
                efftype = " AlienWah";
                break;
            case 6:
                efftype = " Distortion";
                break;
            case 7:
                efftype = " EQ";
                break;
            case 8:
                efftype = " DynFilter";
                break;
        }

        if (part == 0xf1)
            name = "System";
        else if (part == 0xf2)
            name = "Insert";
        else name = "Part " + to_string(part);
        name += " Effect " + to_string(engine); // this is the effect number
        Runtime.Log(name + efftype + " ~ Filter Parameter " + to_string(control) + "  Value " + actual);
    return;
    }

    if (engine == 0)
        name = "  AddSynth";
    else if (engine == 1)
        name = "  SubSynth";
    else if (engine == 2)
        name = "  PadSynth";
    else if (engine >= 0x80)
        name = "  Adsynth Voice " + to_string(engine & 0x3f);

    string contstr;
    switch (control)
    {
        case 0:
            contstr = "C_Freq";
            break;
        case 1:
            contstr = "Q";
            break;
        case 3:
            contstr = "VsensA";
            break;
        case 4:
            contstr = "Vsens";
            break;
        case 2:
            contstr = "FreqTr";
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
            contstr = "St Type";
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
            contstr = "Seq Vowel";
            break;
        case 38:
            contstr = "Neg Input";
            break;
    }

    Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name + " Filter  " + contstr + " value " + actual);
}


void SynthEngine::commandEnvelope(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (engine == 0)
        name = "  AddSynth";
    else if (engine == 1)
        name = "  SubSynth";
    else if (engine == 2)
        name = "  PadSynth";
    else if (engine >= 0x80)
    {
        name = "  Adsynth Voice ";
        if (engine >= 0xC0)
            name += "Modulator ";
        name += to_string(engine & 0x3f);
    }

    string env;
    switch(parameter)
    {
        case 0:
            env = "  Amp";
            break;
        case 1:
            env = "  Freq";
            break;
        case 2:
            env = "  Filt";
            break;
    }


    if (insert == 3)
    {
        Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name  + env + " Env Freemode Control " + to_string(control) + "  X value " + actual);
        return;
    }
    if (insert == 4)
    {
        Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name  + env + " Env Freemode Control " +  to_string(control) + "  Y value " + actual);
        return;
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
        case 33:
            contstr = "Add Point";
            break;
        case 34:
            contstr = "Del Point";
            break;
        case 35:
            contstr = "Sust";
            break;
        case 39:
            contstr = "Stretch";
            break;

        case 48:
            contstr = "frcR";
            break;
        case 49:
            contstr = "L";
            break;
    }

    Runtime.Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name  + env + " Env  " + contstr + " value " + actual);
}


void SynthEngine::commandSysIns(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (part == 0xf1)
        name = "System ";
    else
        name = "Insert ";

    string contstr;
    string second;
    if (insert == 0xff)
    {
        second = "";
        switch (control)
        {
            case 0:
                contstr = "Number ";
                break;
            case 1:
                contstr = to_string(engine) + " Type ";
                break;
            case 2:
                contstr = to_string(engine) + " To ";
                break;
        }
        contstr = "Effect " + contstr + actual;
    }
    else
    {
        contstr = "From Effect " + to_string(engine);
        second = " To Effect " + to_string(control)  + "  Value " + actual;
    }

    Runtime.Log(name  + contstr + second);
}


void SynthEngine::commandEffects(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (part == 0xf1)
        name = "System";
    else if (part == 0xf2)
        name = "Insert";
    else
        name = "Part " + to_string(part);
    name += " Effect " + to_string(engine);

    string efftype;
    switch (kit & 0xf)
    {
        case 0:
            efftype = " NO Effect";
            break;
        case 1:
            efftype = " Reverb";
            break;
        case 2:
            efftype = " Echo";
            break;
        case 3:
            efftype = " Chorus";
            break;
        case 4:
            efftype = " Phaser";
            break;
        case 5:
            efftype = " AlienWah";
            break;
        case 6:
            efftype = " Distortion";
            break;
        case 7:
            efftype = " EQ";
            break;
        case 8:
            efftype = " DynFilter";
            break;
    }

    string contstr = "  Control " + to_string(control);

    Runtime.Log(name + efftype + contstr + "  Value " + actual);
}



void SynthEngine::SetProgram(unsigned char chan, unsigned short pgm)
{
    bool partOK = true;
    int npart;
    string fname = bank.getfilename(pgm);

    if ((fname == "") || (bank.getname(pgm) < "!")) // can't get a program name less than this
        Runtime.Log("No Program " + asString(pgm) + " in this bank");
    else
    {
        if (chan <  NUM_MIDI_CHANNELS) // a normal program change
        {
            for (npart = 0; npart < NUM_MIDI_CHANNELS; ++npart)
                // we don't want upper parts (16 - 63) activiated!
                if (chan == part[npart]->Prcvchn)
                {
                    // all listening parts must succeed
                    if (!SetProgramToPart(npart, pgm, fname))
                    {
                        partOK = false;
                        break;
                    }
                }
        }
        else
        {
            npart = chan & 0x7f;
            if (npart < Runtime.NumAvailableParts)
                partOK = SetProgramToPart(npart, pgm, fname);
        }
        if (!partOK)
            Runtime.Log("SynthEngine setProgram: Invalid program data");
    }
}


// for de-duplicating bits in SetProgram() and for calling from everywhere else
// this replaces bank->loadfromslot for thread safety etc.
bool SynthEngine::SetProgramToPart(int npart, int pgm, string fname)
{
    bool loadOK = false;
    int enablestate;
    sem_wait (&partlock);

    if (Runtime.enable_part_on_voice_load)
        enablestate = 1;
    else
        enablestate = partonoffRead(npart);
    partonoffWrite(npart, 0);
    if (part[npart]->loadXMLinstrument(fname))
    {
        partonoffWrite(npart, enablestate); // must be here to update gui
        loadOK = true;
        // show file instead of program if we got here from Instruments -> Load External...
        Runtime.Log("Loaded " +
                    ((pgm == -1) ? fname : asString(pgm) + " \"" + bank.getname(pgm) + "\"")
                    + " to Part " + asString(npart));
        if (Runtime.showGui && guiMaster && guiMaster->partui
                            && guiMaster->partui->instrumentlabel
                            && guiMaster->partui->part)
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePartProgram, npart);
    }
    else
        partonoffWrite(npart, enablestate); // also here to restore failed load state.
    sem_post (&partlock);
    return loadOK;
}


// Set part's channel number
void SynthEngine::SetPartChan(unsigned char npart, unsigned char nchan)
{
    if (npart < Runtime.NumAvailableParts)
    {
        /* We allow direct controls to set out of range channel numbers.
         * This gives us a way to disable all channel messages to a part.
         * Values 16 to 31 will still allow a note off but values greater
         * than that allow a drone to be set.
         * Sending a valid channel number will restore normal operation
         * as will using the GUI controls.
         */
        part[npart]->Prcvchn =  nchan;
        if (Runtime.showGui && guiMaster && guiMaster->partui
                            && guiMaster->partui->instrumentlabel
                            && guiMaster->partui->part)
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePartProgram, npart);
    }
}


/*
 * Send message to register jack port if jack client is active,
 * but only if the part individual destination is set.
 */
void SynthEngine::SetPartDestination(unsigned char npart, unsigned char dest)
{
    part[npart]->Paudiodest = dest;

    if (part[npart]->Paudiodest & 2)
        GuiThreadMsg::sendMessage(this, GuiThreadMsg::RegisterAudioPort, npart);
    string name;
    switch (dest)
    {
        case 1:
            name = "Main";
            break;

        case 2:
            name = "Part";
            break;

        case 3:
            name = "Both";
            break;
    }
    Runtime.Log("Part " +asString((int) npart) + " sent to " + name);

    // next line only really needed for direct part control.
    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePanelItem, npart);
}


void SynthEngine::SetPartShift(unsigned char npart, unsigned char shift)
{
    if (shift < MIN_KEY_SHIFT + 64)
        shift = MIN_KEY_SHIFT + 64;
    else if(shift > MAX_KEY_SHIFT + 64)
        shift = MAX_KEY_SHIFT + 64;
    part[npart]->Pkeyshift = shift;
    Runtime.Log("Part " +asString((int) npart) + "  key shift set to " + asString(shift - 64));
    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePart, 0);
}


void SynthEngine::SetPartPortamento(int npart, bool state)
{
    part[npart]->ctl->portamento.portamento = state;
}


bool SynthEngine::ReadPartPortamento(int npart)
{
    return part[npart]->ctl->portamento.portamento;
}


void SynthEngine::SetPartKeyMode(int npart, int mode)
{
    switch(mode)
    {
        case 2:
            part[npart]->Ppolymode = 0;
            part[npart]->Plegatomode = 1;
            Runtime.Log("mode set to 'legato'");
            break;
        case 1:
            part[npart]->Ppolymode = 0;
            part[npart]->Plegatomode = 0;
            Runtime.Log("mode set to 'mono'");
            break;
        case 0:
        default:
            part[npart]->Ppolymode = 1;
            part[npart]->Plegatomode = 0;
            Runtime.Log("mode set to 'poly'");
            break;
    }
}


int SynthEngine::ReadPartKeyMode(int npart)
{
    int result;
    bool poly = part[npart]->Ppolymode;
    bool legato = part[npart]->Plegatomode;
    if (!poly && legato)
        result = 2;
    else if (!poly && !legato)
        result = 1;
    else
        result = 0;;
    return result;
}


/*
 * This should really be in MiscFuncs but it has two runtime calls
 * and I can't work out a way to implement that :(
 * We also have to fake long pages when calling via NRPNs as there
 * is no readline entry to set the page length.
 */

void SynthEngine::cliOutput(list<string>& msg_buf, unsigned int lines)
{
    list<string>::iterator it;
    list<string>::reverse_iterator rx;

    if (Runtime.toConsole)
    { // reversed so they come out the right order in the console.
        for (rx = msg_buf.rbegin(); rx != msg_buf.rend(); ++rx)
            Runtime.Log(*rx);
            // we need this in case someone is working headless
        cout << "\nReports sent to console window\n\n";
    }
    else if (msg_buf.size() < lines) // Output will fit the screen
        for (it = msg_buf.begin(); it != msg_buf.end(); ++it)
            Runtime.Log(*it);
    else // Output is too long, page it
    {
        // JBS: make that a class member variable
        string page_filename = "/tmp/yoshimi-pager-" + asString(getpid()) + ".log";
        ofstream fout(page_filename.c_str(),(ios_base::out | ios_base::trunc));
        for (it = msg_buf.begin(); it != msg_buf.end(); ++it)
            fout << *it << endl;
        fout.close();
        string cmd = "less -X -i -M -PM\"q=quit /=search PgUp/PgDown=scroll (line %lt of %L)\" " + page_filename;
        system(cmd.c_str());
        unlink(page_filename.c_str());
    }
    msg_buf.clear();
}


void SynthEngine::ListPaths(list<string>& msg_buf)
{
    string label;
    int idx;
    msg_buf.push_back("Root Paths");

    for (idx = 0; idx < MAX_BANK_ROOT_DIRS; ++ idx)
    {
        if (bank.roots.count(idx) > 0 && !bank.roots [idx].path.empty())
        {
            label = bank.roots [idx].path;
            if (label.at(label.size() - 1) == '/')
                label = label.substr(0, label.size() - 1);
            msg_buf.push_back("    ID " + asString(idx) + "     " + label);
        }
    }
}


void SynthEngine::ListBanks(int rootNum, list<string>& msg_buf)
{
    string label;

    if (rootNum < 0 || rootNum >= MAX_BANK_ROOT_DIRS)
        rootNum = bank.currentRootID;
    if (bank.roots.count(rootNum) > 0
                && !bank.roots [rootNum].path.empty())
    {
        label = bank.roots [rootNum].path;
        if (label.at(label.size() - 1) == '/')
            label = label.substr(0, label.size() - 1);
        msg_buf.push_back("Banks in Root ID " + asString(rootNum));
        msg_buf.push_back("    " + label);
        for (int idx = 0; idx < MAX_BANKS_IN_ROOT; ++ idx)
        {
            if (!bank.roots [rootNum].banks [idx].dirname.empty())
                msg_buf.push_back("    ID " + asString(idx) + "    "
                                + bank.roots [rootNum].banks [idx].dirname);
        }
    }
    else
        msg_buf.push_back("No Root ID " + asString(rootNum));
}


void SynthEngine::ListInstruments(int bankNum, list<string>& msg_buf)
{
    int root = bank.currentRootID;
    string label;

    if (bankNum < 0 || bankNum >= MAX_BANKS_IN_ROOT)
        bankNum = bank.currentBankID;
    if (bank.roots.count(root) > 0
        && !bank.roots [root].path.empty())
    {
        if (!bank.roots [root].banks [bankNum].instruments.empty())
        {
            label = bank.roots [root].path;
            if (label.at(label.size() - 1) == '/')
                label = label.substr(0, label.size() - 1);
            msg_buf.push_back("Instruments in Root ID " + asString(root)
                            + ", Bank ID " + asString(bankNum));
            msg_buf.push_back("    " + label
                            + "/" + bank.roots [root].banks [bankNum].dirname);
            for (int idx = 0; idx < BANK_SIZE; ++ idx)
            {
                if (!bank.emptyslotWithID(root, bankNum, idx))
                {
                    string suffix = "";
                    if (bank.roots [root].banks [bankNum].instruments [idx].ADDsynth_used)
                        suffix += "A";
                    if (bank.roots [root].banks [bankNum].instruments [idx].SUBsynth_used)
                        suffix += "S";
                    if (bank.roots [root].banks [bankNum].instruments [idx].PADsynth_used)
                        suffix += "P";
                    msg_buf.push_back("    ID " + asString(idx) + "    "
                                    + bank.roots [root].banks [bankNum].instruments [idx].name
                                    + "  (" + suffix + ")");
                }
            }
        }
        else
            msg_buf.push_back("No Bank ID " + asString(bankNum)
                      + " in Root " + asString(root));
    }
    else
                msg_buf.push_back("No Root ID " + asString(root));
}


void SynthEngine::ListCurrentParts(list<string>& msg_buf)
{
    int dest;
    string name;
    int avail = Runtime.NumAvailableParts;

    msg_buf.push_back(asString(avail) + " parts available");
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if ((part[npart]->Pname) != "Simple Sound" || (partonoffRead(npart)))
        {
            name = "  " + asString(npart);
            dest = part[npart]->Paudiodest;
            if (!partonoffRead(npart) || npart >= avail)
                name += " -";
            else if(dest == 1)
                name += " M";
            else if(dest == 2)
                name += " P";
            else
                name += " B";
            name +=  " " + part[npart]->Pname;
            msg_buf.push_back(name);
        }
    }
}


void SynthEngine::ListVectors(list<string>& msg_buf)
{
    bool found = false;

    for (int chan = 0; chan < NUM_MIDI_CHANNELS; ++chan)
    {
        if(SingleVector(msg_buf, chan))
            found = true;
    }
    if (!found)
        msg_buf.push_back("No vectors enabled");
}


bool SynthEngine::SingleVector(list<string>& msg_buf, int chan)
{
    if (!Runtime.nrpndata.vectorEnabled[chan])
        return false;

    int Xfeatures = Runtime.nrpndata.vectorXfeatures[chan];
    string Xtext = "Features =";
    if (Xfeatures == 0)
        Xtext = "No Features :(";
    else
    {
        if (Xfeatures & 1)
            Xtext += " 1";
        if (Xfeatures & 2)
            Xtext += " 2";
        if (Xfeatures & 4)
            Xtext += " 3";
        if (Xfeatures & 8)
            Xtext += " 4";
    }
    msg_buf.push_back("Channel " + asString(chan));
    msg_buf.push_back("  X CC = " + asString((int)  Runtime.nrpndata.vectorXaxis[chan]) + ",  " + Xtext);
    msg_buf.push_back("    L = " + part[chan]->Pname + ",  R = " + part[chan + 16]->Pname);

    if (Runtime.nrpndata.vectorYaxis[chan] > 0x7f
        || Runtime.NumAvailableParts < NUM_MIDI_CHANNELS * 4)
        msg_buf.push_back("  Y axis disabled");
    else
    {
        int Yfeatures = Runtime.nrpndata.vectorYfeatures[chan];
        string Ytext = "Features =";
        if (Yfeatures == 0)
            Ytext = "No Features :(";
        else
        {
            if (Yfeatures & 1)
                Ytext += " 1";
            if (Yfeatures & 2)
                Ytext += " 2";
            if (Yfeatures & 4)
                Ytext += " 3";
            if (Yfeatures & 8)
                Ytext += " 4";
        }
        msg_buf.push_back("  Y CC = " + asString((int) Runtime.nrpndata.vectorYaxis[chan]) + ",  " + Ytext);
        msg_buf.push_back("    U = " + part[chan + 32]->Pname + ",  D = " + part[chan + 48]->Pname);
    }
    return true;
}


void SynthEngine::ListSettings(list<string>& msg_buf)
{
    int root;
    string label;

    msg_buf.push_back("Settings");
    msg_buf.push_back("  Master volume " + asString((int) Pvolume));
    msg_buf.push_back("  Master key shift " + asString(Pkeyshift - 64));

    root = bank.currentRootID;
    if (bank.roots.count(root) > 0 && !bank.roots [root].path.empty())
    {
        label = bank.roots [root].path;
        if (label.at(label.size() - 1) == '/')
            label = label.substr(0, label.size() - 1);
        msg_buf.push_back("  Current Root ID " + asString(root)
                        + "    " + label);
        msg_buf.push_back("  Current Bank ID " + asString(bank.currentBankID)
                        + "    " + bank.roots [root].banks [bank.currentBankID].dirname);
    }
    else
        msg_buf.push_back("  No paths set");

    msg_buf.push_back("  Number of available parts "
                    + asString(Runtime.NumAvailableParts));

    msg_buf.push_back("  Current part " + asString(Runtime.currentPart));

    msg_buf.push_back("  Current part's channel " + asString((int)part[Runtime.currentPart]->Prcvchn));

    if (Runtime.midi_bank_root > 119)
        msg_buf.push_back("  MIDI Root Change off");
    else
        msg_buf.push_back("  MIDI Root CC " + asString(Runtime.midi_bank_root));

    if (Runtime.midi_bank_C > 119)
        msg_buf.push_back("  MIDI Bank Change off");
    else
        msg_buf.push_back("  MIDI Bank CC " + asString(Runtime.midi_bank_C));

    if (Runtime.EnableProgChange)
    {
        msg_buf.push_back("  MIDI Program Change on");
        if (Runtime.enable_part_on_voice_load)
            msg_buf.push_back("  MIDI Program Change enables part");
        else
            msg_buf.push_back("  MIDI Program Change doesn't enable part");
    }
    else
        msg_buf.push_back("  MIDI program change off");

    if (Runtime.midi_upper_voice_C > 119)
        msg_buf.push_back("  MIDI extended Program Change off");
    else
        msg_buf.push_back("  MIDI extended Program Change CC "
                        + asString(Runtime.midi_upper_voice_C));
    switch (Runtime.midiEngine)
    {
        case 2:
            label = "ALSA";
            break;

        case 1:
            label = "jack";
            break;

        default:
            label = "None";
            break;
    }
    msg_buf.push_back("  Preferred MIDI " + label);
    switch (Runtime.audioEngine)
    {
        case 2:
            label = "ALSA";
            break;

        case 1:
            label = "jack";
            break;

        default:
            label = "None";
            break;
    }
    msg_buf.push_back("  Preferred audio " + label);
    msg_buf.push_back("  ALSA MIDI " + Runtime.alsaMidiDevice);
    msg_buf.push_back("  ALSA audio " + Runtime.alsaAudioDevice);
    msg_buf.push_back("  jack MIDI " + Runtime.jackMidiDevice);
    msg_buf.push_back("  Jack server " + Runtime.jackServer);
    if (Runtime.connectJackaudio)
        label = "on";
    else
        label = "off";
    msg_buf.push_back("  Jack autoconnect " + label);

    if (Runtime.toConsole)
    {
        msg_buf.push_back("  Reports sent to console window");
    }
    else
        msg_buf.push_back("  Reports sent to stderr");
}


/* Provides a way of setting dynamic system variables
 * from sources other than the gui
 */
void SynthEngine::SetSystemValue(int type, int value)
{
    list<string> msg;
    string label;
    label = "";

    switch (type)
    {
        case 2: // master key shift
            if (value > MAX_KEY_SHIFT + 64)
                value = MAX_KEY_SHIFT + 64;
            else if (value < MIN_KEY_SHIFT + 64) // 3 octaves is enough for anybody :)
                value = MIN_KEY_SHIFT + 64;
            setPkeyshift(value);
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateMaster, 0);
            Runtime.Log("Master key shift set to " + asString(value - 64));
            break;

        case 7: // master volume
            setPvolume(value);
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateMaster, 0);
            Runtime.Log("Master volume set to " + asString(value));
            break;

        case 64:
        case 65:
        case 66:
        case 67:
        case 68:
        case 69:
        case 70:
        case 71:
        case 72:
        case 73:
        case 74:
        case 75:
        case 76:
        case 77:
        case 78:
        case 79:
            for (int npart = 0; npart < Runtime.NumAvailableParts; ++ npart)
                if (part[npart]->Penabled && part[npart]->Prcvchn == (type - 64))
                    SetPartShift(npart, value);
            break;

        case 100: // reports destination
            if (value > 63)
            {
                Runtime.toConsole = true;
                Runtime.Log("Sending reports to console window");
                // we need the next line in case someone is working headless
                cout << "Sending reports to console window\n";
            }
            else
            {
                Runtime.toConsole = false;
                Runtime.Log("Sending reports to stderr");
            }
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateMaster, 0);
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 1);
            break;

        case 108: // list vector parameters
            ListVectors(msg);
            cliOutput(msg, 255);
            break;

        case 109: // list dynamics
            ListSettings(msg);
            cliOutput(msg, 255);
            break;

        case 110 : // list paths
            ListPaths(msg);
            cliOutput(msg, 255);
            break;

        case 111 : // list banks
            ListBanks(value, msg);
            cliOutput(msg, 255);
            break;

        case 112: // list instruments
            ListInstruments(value, msg);
            cliOutput(msg, 255);
            break;

        case 113: // root
            if (value > 119)
                value = 128;
            if (value != Runtime.midi_bank_root) // don't mess about if it's the same
            {
                label = Runtime.testCCvalue(value);
                if (label > "")
                {
                    Runtime.Log("CC" + asString(value) + " in use by " + label);
                    value = -1;
                }
                else
                {
                    Runtime.midi_bank_root = value;
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
                }
            }
            if (value == 128) // but still report the setting
                Runtime.Log("MIDI Root Change disabled");
            else if (value > -1)
                Runtime.Log("Root CC set to " + asString(value));
            break;

        case 114: // bank
            if (value != 0 && value != 32)
                value = 128;
            if (value != Runtime.midi_bank_C)
            {
                label = Runtime.testCCvalue(value);
                if (label > "")
                {
                    Runtime.Log("CC" + asString(value) + " in use by " + label);
                    value = -1;
                }
                else
                {
                    Runtime.midi_bank_C = value;
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
                }
            }
            if (value == 0)
                Runtime.Log("Bank CC set to MSB (0)");
            else if (value == 32)
                Runtime.Log("Bank CC set to LSB (32)");
            else if (value > -1)
                Runtime.Log("MIDI Bank Change disabled");
            break;

        case 115: // program change
            value = (value > 63);
            if (value)
                Runtime.Log("MIDI Program Change enabled");
            else
                Runtime.Log("MIDI Program Change disabled");
            if (value != Runtime.EnableProgChange)
            {
                Runtime.EnableProgChange = value;
                GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
            }
            break;

        case 116: // enable on program change
            value = (value > 63);
            if (value)
                Runtime.Log("MIDI Program Change will enable part");
            else
                Runtime.Log("MIDI Program Change doesn't enable part");
            if (value != Runtime.enable_part_on_voice_load)
            {
                Runtime.enable_part_on_voice_load = value;
                GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
            }
            break;

        case 117: // extended program change
            if (value > 119)
                value = 128;
            if (value != Runtime.midi_upper_voice_C) // don't mess about if it's the same
            {
                label = Runtime.testCCvalue(value);
                if (label > "")
                {
                    Runtime.Log("CC" + asString(value) + " in use by " + label);
                    value = -1;
                }
                else
                {
                    Runtime.midi_upper_voice_C = value;
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
                }
            }
            if (value == 128) // but still report the setting
                Runtime.Log("MIDI extended Program Change disabled");
            else if (value > -1)
                Runtime.Log("Extended Program Change CC set to " + asString(value));
            break;

        case 118: // active parts
            if (value == 16 || value == 32 || value == 64)
            {
                Runtime.NumAvailableParts = value;
                Runtime.Log("Available parts set to " + asString(value));
                GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePart,0);
            }
            else
                Runtime.Log("Out of range");
            break;

        case 119: // obvious!
            Runtime.saveConfig();
            Runtime.Log("Settings saved");
            break;

        case 128: // shortform NRPN channel switch

            if (value >= NUM_MIDI_PARTS) // single row
            {
                int start = value & 0xc;
                value &= 0x3f;
                for (int i = start; i < start + 4; ++i)
                {
                    if (i != value)
                        part[i]->Prcvchn = start | NUM_MIDI_CHANNELS;
                    else
                        part[i]->Prcvchn = start;
                }
            }
            else // columns
            {
                int chan = value & 0xf;
                for (int i = chan; i < NUM_MIDI_PARTS; i += NUM_MIDI_CHANNELS)
                {
                    if (i != value)
                        part[i]->Prcvchn = chan | NUM_MIDI_CHANNELS;
                    else
                       part[i]->Prcvchn = chan;
                }
            }
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePart,0);
            break;
    }
}


void SynthEngine::writeRBP(char type, char data0, char data1)
{
    struct RBP_data block;
    unsigned int writesize = sizeof(RBP_data);
    block.data[0] = type;
    block.data[1] = data0;
    block.data[2] = data1;
    char *point = (char*)&block;
    unsigned int towrite = writesize;
    unsigned int wrote = 0;
    unsigned int found;
    unsigned int tries = 0;

    if (jack_ringbuffer_write_space(RBPringbuf) >= writesize)
    {
        while (towrite && tries < 3)
        {
            found = jack_ringbuffer_write(RBPringbuf, point, towrite);
            wrote += found;
            point += found;
            towrite -= found;
            ++tries;
        }
        if (towrite)
            Runtime.Log("Unable to write data to Root/bank/Program");
    }
    else
        Runtime.Log("Root/bank/Program buffer full!");
}


bool SynthEngine::vectorInit(int dHigh, unsigned char chan, int par)
{
    string name = "";

    if (dHigh < 2)
    {
        int parts = Runtime.NumAvailableParts;
        if ((dHigh == 0) && (parts < NUM_MIDI_CHANNELS * 2))
        {
            SetSystemValue(118, NUM_MIDI_CHANNELS * 2);
            partonoffLock(chan, 1);
            partonoffLock(chan + NUM_MIDI_CHANNELS, 1);
        }
        else if ((dHigh == 1) && (parts < NUM_MIDI_CHANNELS * 4))
        {
            SetSystemValue(118, NUM_MIDI_CHANNELS * 4);
            partonoffLock(chan + NUM_MIDI_CHANNELS * 2, 1);
            partonoffLock(chan + NUM_MIDI_CHANNELS * 3, 1);
        }
        name = Runtime.testCCvalue(par);
    }
    else if (!Runtime.nrpndata.vectorEnabled[chan])
    {
        Runtime.Log("Vector control must be enabled first");
        return true;
    }
    else if (dHigh > 7)
        name = Runtime.masterCCtest(par);

    if (name > "")
    {
        Runtime.Log("CC " + asString(par) + " in use for " + name);
        return true;
    }
    return false;
}


void SynthEngine::vectorSet(int dHigh, unsigned char chan, int par)
{
    string featureList = "";

    if (dHigh == 2 || dHigh == 3)
    {
        if (bitTest(par, 0))
        {
            featureList += "1 en  ";
        }
        if (bitTest(par, 1))
        {
            if (!bitTest(par, 4))
                featureList += "2 en  ";
            else
                featureList += "2 rev  ";
        }
        if (bitTest(par, 2))
        {
            if (!bitTest(par, 5))
                featureList += "3 en  ";
            else
                featureList += "3 rev  ";
        }
         if (bitTest(par, 3))
        {
            if (!bitTest(par, 6))
                featureList += "4 en";
            else
                featureList += "4 rev";
        }
    }

    switch (dHigh)
    {
        case 0:
            Runtime.nrpndata.vectorXaxis[chan] = par;
            if (!Runtime.nrpndata.vectorEnabled[chan])
            {
                Runtime.nrpndata.vectorEnabled[chan] = true;
                Runtime.Log("Vector control enabled");
                // enabling is only done with a valid X CC
            }
            SetPartChan(chan, chan);
            SetPartChan(chan | 16, chan);
            Runtime.nrpndata.vectorXcc2[chan] = C_panning;
            Runtime.nrpndata.vectorXcc4[chan] = C_filtercutoff;
            Runtime.nrpndata.vectorXcc8[chan] = C_modwheel;
            Runtime.Log("Vector " + asString((int) chan) + " X CC set to " + asString(par));
            break;

        case 1:
            if (!Runtime.nrpndata.vectorEnabled[chan])
                Runtime.Log("Vector X axis must be set before Y");
            else
            {
                SetPartChan(chan | 32, chan);
                SetPartChan(chan | 48, chan);
                Runtime.nrpndata.vectorYaxis[chan] = par;
                Runtime.nrpndata.vectorYcc2[chan] = C_panning;
                Runtime.nrpndata.vectorYcc4[chan] = C_filtercutoff;
                Runtime.nrpndata.vectorYcc8[chan] = C_modwheel;
                Runtime.Log("Vector " + asString((int) chan) + " Y CC set to " + asString(par));
            }
            break;

        case 2:
            Runtime.nrpndata.vectorXfeatures[chan] = par;
            Runtime.Log("Set X features " + featureList);
            break;

        case 3:
            if (Runtime.NumAvailableParts > NUM_MIDI_CHANNELS * 2)
            {
                Runtime.nrpndata.vectorYfeatures[chan] = par;
                Runtime.Log("Set Y features " + featureList);
            }
            break;

        /*
         * If this came from the command line thread
         * we don't need to worry about blocking
         * with these program changes as it is only
         * the command line thread that's blocked.
         * The MIDI NRPN thread deals with them separately.
         */
        case 4:
            SetProgram(chan | 0x80, par);
            break;

        case 5:
            SetProgram(chan | 0x90, par);
            break;

        case 6:
            SetProgram(chan | 0xa0, par);
            break;

        case 7:
            SetProgram(chan | 0xb0, par);
            break;

        case 8:
            Runtime.nrpndata.vectorXcc2[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " X feature 2 set to " + asString(par));
            break;

        case 9:
            Runtime.nrpndata.vectorXcc4[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " X feature 3 set to " + asString(par));
            break;

        case 10:
            Runtime.nrpndata.vectorXcc8[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " X feature 4 set to " + asString(par));
            break;

        case 11:
            Runtime.nrpndata.vectorYcc2[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " Y feature 2 set to " + asString(par));
            break;

        case 12:
            Runtime.nrpndata.vectorYcc4[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " Y feature 3 set to " + asString(par));
            break;

        case 13:
            Runtime.nrpndata.vectorYcc8[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " Y feature 4 set to " + asString(par));
            break;

        default:
            Runtime.nrpndata.vectorEnabled[chan] = false;
            Runtime.nrpndata.vectorXaxis[chan] = 0xff;
            Runtime.nrpndata.vectorYaxis[chan] = 0xff;
            Runtime.nrpndata.vectorXfeatures[chan] = 0;
            Runtime.nrpndata.vectorYfeatures[chan] = 0;
            Runtime.Log("Channel " + asString((int) chan) + " vector control disabled");
            break;
    }
}


void SynthEngine::ClearNRPNs(void)
{
    Runtime.nrpnL = 127;
    Runtime.nrpnH = 127;
    Runtime.nrpnActive = false;

    for (int chan = 0; chan < NUM_MIDI_CHANNELS; ++chan)
    {
        Runtime.nrpndata.vectorEnabled[chan] = false;
        Runtime.nrpndata.vectorXaxis[chan] = 0xff;
        Runtime.nrpndata.vectorYaxis[chan] = 0xff;
        Runtime.nrpndata.vectorXfeatures[chan] = 0;
        Runtime.nrpndata.vectorYfeatures[chan] = 0;
    }
}


void SynthEngine::resetAll(void)
{
    actionLock(lockmute);
    defaults();
    ClearNRPNs();
    actionLock(unlock);
    Runtime.Log("All dynamic values set to defaults.");
}


// Enable/Disable a part
void SynthEngine::partonoffLock(int npart, int what)
{
    sem_wait (&partlock);
    partonoffWrite(npart, what);
    sem_post (&partlock);
}


void SynthEngine::partonoffWrite(int npart, int what)
{
    if (npart >= Runtime.NumAvailableParts)
        return;

    if (what)
    {
        VUpeak.values.parts[npart] = 1e-9f;
        part[npart]->Penabled = 1;
    }
    else
    {   // disabled part
        part[npart]->Penabled = 0;
        part[npart]->cleanup();
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
            if (Pinsparts[nefx] == npart)
                insefx[nefx]->cleanup();
        VUpeak.values.parts[npart] = -0.2;
    }
}


bool SynthEngine::partonoffRead(int npart)
{
    return (part[npart]->Penabled != 0);
}


// Master audio out (the final sound)
int SynthEngine::MasterAudio(float *outl [NUM_MIDI_PARTS + 1], float *outr [NUM_MIDI_PARTS + 1], int to_process)
{
    float *mainL = outl[NUM_MIDI_PARTS]; // tiny optimisation
    float *mainR = outr[NUM_MIDI_PARTS]; // makes code clearer

    p_buffersize = buffersize;
    p_bufferbytes = bufferbytes;
    p_buffersize_f = buffersize_f;

    if ((to_process > 0) && (to_process < buffersize))
    {
        p_buffersize = to_process;
        p_bufferbytes = p_buffersize * sizeof(float);
        p_buffersize_f = p_buffersize;
    }

    int npart;

    memset(mainL, 0, p_bufferbytes);
    memset(mainR, 0, p_bufferbytes);

    if (isMuted())
    {
        for (npart = 0; npart < (Runtime.NumAvailableParts); ++npart)
        {
            if (partonoffRead(npart))
            {
                memset(outl[npart], 0, p_bufferbytes);
                memset(outr[npart], 0, p_bufferbytes);
            }
        }
    }
/* Normally the above is unnecessary, as we later do a copy to just the parts
 * that have a direct output. This completely overwrites the buffers.
 * Only these are sent to jack, so it doesn't matter what the unused ones contain.
 * However, this doesn't happen when muted, so the buffers then need to be zeroed.
 */
    else
    {
        actionLock(lock);

        // Compute part samples and store them ->partoutl,partoutr
        for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
            if (partonoffRead(npart))
                part[npart]->ComputePartSmps();

        // Insertion effects
        int nefx;
        for (nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        {
            if (Pinsparts[nefx] >= 0)
            {
                int efxpart = Pinsparts[nefx];
                if (part[efxpart]->Penabled)
                    insefx[nefx]->out(part[efxpart]->partoutl, part[efxpart]->partoutr);
            }
        }

        // Apply the part volumes and pannings (after insertion effects)
        for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (!partonoffRead(npart))
                continue;

            float oldvol_l = part[npart]->oldvolumel;
            float oldvol_r = part[npart]->oldvolumer;
            float newvol_l = part[npart]->pannedVolLeft();
            float newvol_r = part[npart]->pannedVolRight();
            if (aboveAmplitudeThreshold(oldvol_l, newvol_l) || aboveAmplitudeThreshold(oldvol_r, newvol_r))
            {   // the volume or the panning has changed and needs interpolation
                for (int i = 0; i < p_buffersize; ++i)
                {
                    float vol_l = interpolateAmplitude(oldvol_l, newvol_l, i, p_buffersize);
                    float vol_r = interpolateAmplitude(oldvol_r, newvol_r, i, p_buffersize);
                    part[npart]->partoutl[i] *= vol_l;
                    part[npart]->partoutr[i] *= vol_r;
                }
                part[npart]->oldvolumel = newvol_l;
                part[npart]->oldvolumer = newvol_r;
            }
            else
            {
                for (int i = 0; i < p_buffersize; ++i)
                {   // the volume did not change
                    part[npart]->partoutl[i] *= newvol_l;
                    part[npart]->partoutr[i] *= newvol_r;
                }
            }
        }
        // System effects
        for (nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        {
            if (!sysefx[nefx]->geteffect())
                continue; // is disabled

            // Clean up the samples used by the system effects
            memset(tmpmixl, 0, p_bufferbytes);
            memset(tmpmixr, 0, p_bufferbytes);

            // Mix the channels according to the part settings about System Effect
            for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
            {
                if (partonoffRead(npart)        // it's enabled
                 && Psysefxvol[nefx][npart]      // it's sending an output
                 && part[npart]->Paudiodest & 1) // it's connected to the main outs
                {
                    // the output volume of each part to system effect
                    float vol = sysefxvol[nefx][npart];
                    for (int i = 0; i < p_buffersize; ++i)
                    {
                        tmpmixl[i] += part[npart]->partoutl[i] * vol;
                        tmpmixr[i] += part[npart]->partoutr[i] * vol;
                    }
                }
            }

            // system effect send to next ones
            for (int nefxfrom = 0; nefxfrom < nefx; ++nefxfrom)
            {
                if (Psysefxsend[nefxfrom][nefx])
                {
                    float v = sysefxsend[nefxfrom][nefx];
                    for (int i = 0; i < p_buffersize; ++i)
                    {
                        tmpmixl[i] += sysefx[nefxfrom]->efxoutl[i] * v;
                        tmpmixr[i] += sysefx[nefxfrom]->efxoutr[i] * v;
                    }
                }
            }
            sysefx[nefx]->out(tmpmixl, tmpmixr);

            // Add the System Effect to sound output
            float outvol = sysefx[nefx]->sysefxgetvolume();
            for (int i = 0; i < p_buffersize; ++i)
            {
                mainL[i] += tmpmixl[i] * outvol;
                mainR[i] += tmpmixr[i] * outvol;
            }
        }

        for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (part[npart]->Paudiodest & 2){    // Copy separate parts

                for (int i = 0; i < p_buffersize; ++i)
                {
                    outl[npart][i] = part[npart]->partoutl[i];
                    outr[npart][i] = part[npart]->partoutr[i];
                }
            }
            if (part[npart]->Paudiodest & 1)    // Mix wanted parts to mains
            {
                for (int i = 0; i < p_buffersize; ++i)
                {   // the volume did not change
                    mainL[i] += part[npart]->partoutl[i];
                    mainR[i] += part[npart]->partoutr[i];
                }
            }
        }

        // Insertion effects for Master Out
        for (nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        {
            if (Pinsparts[nefx] == -2)
                insefx[nefx]->out(mainL, mainR);
        }

        LFOtime++; // update the LFO's time

        // Master volume, and all output fade
        for (int idx = 0; idx < p_buffersize; ++idx)
        {
            mainL[idx] *= volume; // apply Master Volume
            mainR[idx] *= volume;
            if (shutup) // fade-out - fadeLevel must also have been set
            {
                for (npart = 0; npart < (Runtime.NumAvailableParts); ++npart)
                {
                    if (part[npart]->Paudiodest & 2)
                    {
                        outl[npart][idx] *= fadeLevel;
                        outr[npart][idx] *= fadeLevel;
                    }
                }
                mainL[idx] *= fadeLevel;
                mainR[idx] *= fadeLevel;
                fadeLevel -= fadeStep;
            }
        }

        actionLock(unlock);

        // Peak calculation for mixed outputs
        VUpeak.values.vuRmsPeakL = 1e-12f;
        VUpeak.values.vuRmsPeakR = 1e-12f;
        float absval;
        for (int idx = 0; idx < p_buffersize; ++idx)
        {
            if ((absval = fabsf(mainL[idx])) > VUpeak.values.vuOutPeakL)
                VUpeak.values.vuOutPeakL = absval;
            if ((absval = fabsf(mainR[idx])) > VUpeak.values.vuOutPeakR)
                VUpeak.values.vuOutPeakR = absval;

            // RMS Peak
            VUpeak.values.vuRmsPeakL += mainL[idx] * mainL[idx];
            VUpeak.values.vuRmsPeakR += mainR[idx] * mainR[idx];
        }

       if (shutup && fadeLevel <= 0.001f)
            ShutUp();

        // Peak computation for part vu meters
        for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (partonoffRead(npart))
            {
                for (int idx = 0; idx < p_buffersize; ++idx)
                {
                    if ((absval = fabsf(part[npart]->partoutl[idx])) > VUpeak.values.parts[npart])
                        VUpeak.values.parts[npart] = absval;
                    if ((absval = fabsf(part[npart]->partoutr[idx])) > VUpeak.values.parts[npart])
                        VUpeak.values.parts[npart] = absval;
                }
            }
        }

        VUpeak.values.p_buffersize = p_buffersize;

        if (jack_ringbuffer_write_space(vuringbuf) >= sizeof(VUtransfer))
        {
            jack_ringbuffer_write(vuringbuf, ( char*)VUpeak.bytes, sizeof(VUtransfer));
            VUpeak.values.vuOutPeakL = 1e-12f;
            VUpeak.values.vuOutPeakR = 1e-12f;
            for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
            {
                if (partonoffRead(npart))
                    VUpeak.values.parts[npart] = 1.0e-9;
                else if (VUpeak.values.parts[npart] < -2.2) // fake peak is a negative value
                    VUpeak.values.parts[npart]+= 2;
            }
        }
    }
    return p_buffersize;
}


bool SynthEngine::fetchMeterData(VUtransfer *VUdata)
{
    if (jack_ringbuffer_read_space(vuringbuf) >= sizeof(VUtransfer))
    {

        jack_ringbuffer_read(vuringbuf, ( char*)VUdata->bytes, sizeof(VUtransfer));
        VUdata->values.vuRmsPeakL = sqrt(VUdata->values.vuRmsPeakL / VUdata->values.p_buffersize);
        VUdata->values.vuRmsPeakR = sqrt(VUdata->values.vuRmsPeakR / VUdata->values.p_buffersize);
        return true;
    }
    return false;
}

// Parameter control
void SynthEngine::setPvolume(char control_value)
{
    Pvolume = control_value;
    volume  = dB2rap((Pvolume - 96.0f) / 96.0f * 40.0f);
}


void SynthEngine::setPkeyshift(int Pkeyshift_)
{
    Pkeyshift = Pkeyshift_;
    keyshift = Pkeyshift - 64;
}


void SynthEngine::setPsysefxvol(int Ppart, int Pefx, char Pvol)
{
    Psysefxvol[Pefx][Ppart] = Pvol;
    sysefxvol[Pefx][Ppart]  = powf(0.1f, (1.0f - Pvol / 96.0f) * 2.0f);
}


void SynthEngine::setPsysefxsend(int Pefxfrom, int Pefxto, char Pvol)
{
    Psysefxsend[Pefxfrom][Pefxto] = Pvol;
    sysefxsend[Pefxfrom][Pefxto]  = powf(0.1f, (1.0f - Pvol / 96.0f) * 2.0f);
}

void SynthEngine::setPaudiodest(int value)
{
    Paudiodest = value;
}


// Panic! (Clean up all parts and effects)
void SynthEngine::ShutUp(void)
{
    VUpeak.values.vuOutPeakL = 1e-12f;
    VUpeak.values.vuOutPeakR = 1e-12f;

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart]->cleanup();
        VUpeak.values.parts[npart] = -0.2;
    }
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx]->cleanup();
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx]->cleanup();
    shutup = false;
    fadeLevel = 0.0f;
}


void SynthEngine::allStop()
{
    actionLock(lockmute);
    shutup = 1;
    fadeLevel = 1.0f;
    actionLock(unlock);
}


bool SynthEngine::actionLock(lockset request)
{
    int chk  = -1;

    switch (request)
    {
        case trylock:
            chk = pthread_mutex_trylock(processLock);
            break;

        case lock:
            chk = pthread_mutex_lock(processLock);
            break;

        case unlock:
            Unmute();
            chk = pthread_mutex_unlock(processLock);
            break;

        case lockmute:
            Mute();
            chk = pthread_mutex_lock(processLock);
            break;

        default:
            break;
    }
    return (chk == 0) ? true : false;
}


void SynthEngine::applyparameters(void)
{
    ShutUp();
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart]->applyparameters();
}


int SynthEngine::loadParameters(string fname)
{
    int result = 0;
    Runtime.SimpleCheck = false;

    actionLock(lockmute);
    defaults(); // clear all parameters
    if (loadXML(fname)) // load the data
    {
        result = 1; // this is messy, but can't trust bool to int conversions
        if (Runtime.SimpleCheck)
            result = 3;
    }
    actionLock(unlock);
    return result;
}


int SynthEngine::loadPatchSetAndUpdate(string fname)
{
    int result = loadParameters(fname);
    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateMaster, 0);
    return result;
}


bool SynthEngine::installBanks(int instance)
{
    bool banksFound = true;
    string branch;
    string name = Runtime.ConfigDir + '/' + YOSHIMI;

    if (instance > 0)
        name += ("-" + asString(instance));
    string bankname = name + ".banks";
//    Runtime.Log(bankname);
    if (!isRegFile(bankname))
    {
        banksFound = false;
        Runtime.Log("Missing bank file");
        bankname = name + ".config";
        if (isRegFile(bankname))
            Runtime.Log("Copying data from config");
        else
        {
            Runtime.Log("Scanning for banks");
            bank.rescanforbanks();
            return false;
        }
    }
    if (banksFound)
        branch = "BANKLIST";
    else
        branch = "CONFIGURATION";
    XMLwrapper *xml = new XMLwrapper(this);
    if (!xml)
    {
        Runtime.Log("loadConfig failed XMLwrapper allocation");
        return false;
    }
    xml->loadXMLfile(bankname);
    if (!xml->enterbranch(branch))
    {
        Runtime. Log("extractConfigData, no " + branch + " branch");
        return false;
    }
    bank.parseConfigFile(xml);
    xml->exitbranch();
    delete xml;
    return true;
}


bool SynthEngine::saveBanks(int instance)
{
    string name = Runtime.ConfigDir + '/' + YOSHIMI;
    if (instance > 0)
        name += ("-" + asString(instance));
    string bankname = name + ".banks";
    Runtime.xmlType = XML_BANK;

    XMLwrapper *xmltree = new XMLwrapper(this);
    if (!xmltree)
    {
        Runtime.Log("saveBanks failed xmltree allocation");
        return false;
    }
    xmltree->beginbranch("BANKLIST");
    bank.saveToConfigFile(xmltree);
    xmltree->endbranch();

    if (!xmltree->saveXMLfile(bankname))
        Runtime.Log("Failed to save config to " + bankname);

    delete xmltree;

    return true;
}


void SynthEngine::addHistory(string name, int group)
{
    unsigned int name_start = name.rfind("/");
    unsigned int name_end = name.rfind(".");

    if (name_start == string::npos || name_end == string::npos
            || (name_start - 1) >= name_end)
        return;
    unsigned int offset = 0;
    bool copy = false;
    vector<string> &listType = *getHistory(group);
    if (listType.size() > MAX_HISTORY)
        offset = listType.size() - MAX_HISTORY;
     // don't test against names that will be dropped when saving
    for (vector<string>::iterator it = listType.begin() + offset; it != listType.end(); ++it)
    {
        if (*it == name)
            copy = true;
    }
    if (!copy)
        listType.push_back(name);
    return;
}


vector<string> * SynthEngine::getHistory(int group)
{
    if (group == 5)
        return &VectorHistory;
    else if (group == 4)
        return &StateHistory;
    else if (group == 3)
        return &ScaleHistory;
    return &ParamsHistory;
}


bool SynthEngine::loadHistory()
{
    string name = Runtime.ConfigDir + '/' + YOSHIMI;
    string historyname = name + ".history";
    if (!isRegFile(historyname))
    {
        Runtime.Log("Missing history file");
        return false;
    }
    XMLwrapper *xml = new XMLwrapper(this);
    if (!xml)
    {
        Runtime.Log("loadHistory failed XMLwrapper allocation");
        return false;
    }
    xml->loadXMLfile(historyname);
    if (!xml->enterbranch("HISTORY"))
    {
        Runtime. Log("extractHistoryData, no HISTORY branch");
        return false;
    }
    int hist_size;
    string filetype;
    string type;
    string extension;
    for (int count = 2; count < 6; ++count)
    {
        switch (count)
        {
            case 2:
                type = "XMZ_PATCH_SETS";
                extension = "xmz_file";
                break;
            case 3:
                type = "XMZ_SCALE";
                extension = "xsz_file";
                break;
            case 4:
                type = "XMZ_STATE";
                extension = "state_file";
                break;
            case 5:
                type = "XMZ_VECTOR";
                extension = "xvy_file";
                break;
        }
        if (xml->enterbranch(type))
        { // should never exceed max history as size trimmed on save
            hist_size = xml->getpar("history_size", 0, 0, MAX_HISTORY);
            for (int i = 0; i < hist_size; ++i)
            {
                if (xml->enterbranch("XMZ_FILE", i))
                {
                    filetype = xml->getparstr(extension);
                    if (filetype.size() && isRegFile(filetype))
                        addHistory(filetype, count);
                    xml->exitbranch();
                }
            }
            xml->exitbranch();
        }
    }
    xml->exitbranch();
    delete xml;
    return true;
}


bool SynthEngine::saveHistory()
{
    string name = Runtime.ConfigDir + '/' + YOSHIMI;
    string historyname = name + ".history";
    Runtime.xmlType = XML_HISTORY;

    XMLwrapper *xmltree = new XMLwrapper(this);
    if (!xmltree)
    {
        Runtime.Log("saveHistory failed xmltree allocation");
        return false;
    }
    xmltree->beginbranch("HISTORY");
    {
        unsigned int offset;
        int x;
        string type;
        string extension;
        for (int count = 2; count < 6; ++count)
        {
            switch (count)
            {
                case 2:
                    type = "XMZ_PATCH_SETS";
                    extension = "xmz_file";
                    break;
                case 3:
                    type = "XMZ_SCALE";
                    extension = "xsz_file";
                    break;
                case 4:
                    type = "XMZ_STATE";
                    extension = "state_file";
                    break;
                case 5:
                    type = "XMZ_VECTOR";
                    extension = "xvy_file";
                    break;
            }
            vector<string> listType = *getHistory(count);
            if (listType.size())
            {
                offset = 0;
                x = 0;
                xmltree->beginbranch(type);
                    xmltree->addpar("history_size", listType.size());
                    if (listType.size() > MAX_HISTORY)
                        offset = listType.size() - MAX_HISTORY;
                    for (vector<string>::iterator it = listType.begin() + offset; it != listType.end(); ++it)
                    {
                        xmltree->beginbranch("XMZ_FILE", x);
                            xmltree->addparstr(extension, *it);
                        xmltree->endbranch();
                        ++x;
                    }
                xmltree->endbranch();
            }
        }
    }
    xmltree->endbranch();
    if (!xmltree->saveXMLfile(historyname))
        Runtime.Log("Failed to save data to " + historyname);
    delete xmltree;
    return true;
}


bool SynthEngine::loadVector(unsigned char baseChan, string name, bool full)
{
    /*if (baseChan >= NUM_MIDI_CHANNELS)
    {
        Runtime.Log("Invalid channel number");
        return false;
    }*/
    if (name.empty())
    {
        Runtime.Log("No filename");
        return false;
    }
    string file = setExtension(name, "xvy");
    legit_pathname(file);
    if (!isRegFile(file))
    {
        Runtime.Log("Can't find " + file);
        return false;
    }
    XMLwrapper *xml = new XMLwrapper(this);
    if (!xml)
    {
        Runtime.Log("Load Vector failed XMLwrapper allocation");
        return false;
    }
    xml->loadXMLfile(file);
    if (!xml->enterbranch("VECTOR"))
    {
        Runtime. Log("Extract Data, no VECTOR branch");
        return false;
    }
    addHistory(file, 5);
    int lastPart = NUM_MIDI_PARTS;
    if (baseChan >= NUM_MIDI_CHANNELS)
        baseChan = xml->getpar255("Source_channel", 0);
    Runtime.nrpndata.vectorXaxis[baseChan] = xml->getpar255("X_sweep_CC", 0xff);
    if (Runtime.nrpndata.vectorXaxis[baseChan] < 0x7f)
        Runtime.nrpndata.vectorEnabled[baseChan] = true;;
    Runtime.nrpndata.vectorYaxis[baseChan] = xml->getpar255("Y_sweep_CC", 0xff);

    int x_feat = 0;
    int y_feat = 0;
    if (xml->getparbool("X_feature_1", false))
        x_feat |= 1;
    if (xml->getparbool("X_feature_2", false))
        x_feat |= 2;
    if (xml->getparbool("X_feature_2_R", false))
        x_feat |= 0x10;
    if (xml->getparbool("X_feature_4", false))
        x_feat |= 4;
    if (xml->getparbool("X_feature_4_R", false))
        x_feat |= 0x20;
    if (xml->getparbool("X_feature_8", false))
        x_feat |= 8;
    if (xml->getparbool("X_feature_8_R", false))
        x_feat |= 0x40;
    Runtime.nrpndata.vectorXcc2[baseChan] = xml->getpar255("X_CCout_2", 10);
    Runtime.nrpndata.vectorXcc4[baseChan] = xml->getpar255("X_CCout_4", 74);
    Runtime.nrpndata.vectorXcc8[baseChan] = xml->getpar255("X_CCout_8", 1);
    if (Runtime.nrpndata.vectorYaxis[baseChan] > 0x7f)
    {
        lastPart /= 2;
    }
    else
    {
        if (xml->getparbool("Y_feature_1", false))
            y_feat |= 1;
        if (xml->getparbool("Y_feature_2", false))
            y_feat |= 2;
        if (xml->getparbool("Y_feature_2_R", false))
            y_feat |= 0x10;
        if (xml->getparbool("Y_feature_4", false))
            y_feat |= 4;
        if (xml->getparbool("Y_feature_4_R", false))
            y_feat |= 0x20;
        if (xml->getparbool("Y_feature_8", false))
            y_feat |= 8;
        if (xml->getparbool("Y_feature_8_R", false))
            y_feat |= 0x40;
        Runtime.nrpndata.vectorYcc2[baseChan] = xml->getpar255("Y_CCout_2", 10);
        Runtime.nrpndata.vectorYcc4[baseChan] = xml->getpar255("Y_CCout_4", 74);
        Runtime.nrpndata.vectorYcc8[baseChan] = xml->getpar255("Y_CCout_8", 1);
    }
    Runtime.nrpndata.vectorXfeatures[baseChan] = x_feat;
    Runtime.nrpndata.vectorYfeatures[baseChan] = y_feat;

    if (full)
    {
        Runtime.NumAvailableParts = xml->getpar255("current_midi_parts", Runtime.NumAvailableParts);
        for (int npart = 0; npart < lastPart; npart += NUM_MIDI_CHANNELS)
        {
            if (!xml->enterbranch("PART", npart))
                continue;
            part[npart + baseChan]->getfromXML(xml);
            part[npart + baseChan]->Prcvchn = baseChan;
            xml->exitbranch();
            if (partonoffRead(npart + baseChan) && (part[npart + baseChan]->Paudiodest & 2))
                GuiThreadMsg::sendMessage(this, GuiThreadMsg::RegisterAudioPort, npart + baseChan);
        }
        GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePart,0);
    // need to ensure thread safety here.
    }
    xml->exitbranch();
    delete xml;
    return true;
}


bool SynthEngine::saveVector(unsigned char baseChan, string name, bool full)
{
    bool ok = true;

    if (baseChan >= NUM_MIDI_CHANNELS)
    {
        Runtime.Log("Invalid channel number");
        return false;
    }
    if (name.empty())
    {
        Runtime.Log("No filename");
        return false;
    }
    if (Runtime.nrpndata.vectorEnabled[baseChan] == false)
    {
        Runtime.Log("No vector data on this channel");
        return false;
    }

    string file = setExtension(name, "xvy");
    legit_pathname(file);

    Runtime.xmlType = XML_VECTOR;
    XMLwrapper *xml = new XMLwrapper(this);
    if (!xml)
    {
        Runtime.Log("Save Vector failed xmltree allocation");
        return false;
    }
    int lastPart = NUM_MIDI_PARTS;
    int x_feat = Runtime.nrpndata.vectorXfeatures[baseChan];
    int y_feat = Runtime.nrpndata.vectorYfeatures[baseChan];
    xml->beginbranch("VECTOR");
        xml->addpar("Source_channel", baseChan);
        xml->addpar("X_sweep_CC", Runtime.nrpndata.vectorXaxis[baseChan]);
        xml->addpar("Y_sweep_CC", Runtime.nrpndata.vectorYaxis[baseChan]);
        xml->addparbool("X_feature_1", (x_feat & 1) > 0);
        xml->addparbool("X_feature_2", (x_feat & 2) > 0);
        xml->addparbool("X_feature_2_R", (x_feat & 0x10) > 0);
        xml->addparbool("X_feature_4", (x_feat & 4) > 0);
        xml->addparbool("X_feature_4_R", (x_feat & 0x20) > 0);
        xml->addparbool("X_feature_8", (x_feat & 8) > 0);
        xml->addparbool("X_feature_8_R", (x_feat & 0x40) > 0);
        xml->addpar("X_CCout_2",Runtime.nrpndata.vectorXcc2[baseChan]);
        xml->addpar("X_CCout_4",Runtime.nrpndata.vectorXcc4[baseChan]);
        xml->addpar("X_CCout_8",Runtime.nrpndata.vectorXcc8[baseChan]);
        if (Runtime.nrpndata.vectorYaxis[baseChan] > 0x7f)
        {
            lastPart /= 2;
        }
        else
        {
            xml->addparbool("Y_feature_1", (y_feat & 1) > 0);
            xml->addparbool("Y_feature_2", (y_feat & 2) > 0);
            xml->addparbool("Y_feature_2_R", (y_feat & 0x10) > 0);
            xml->addparbool("Y_feature_4", (y_feat & 4) > 0);
            xml->addparbool("Y_feature_4_R", (y_feat & 0x20) > 0);
            xml->addparbool("Y_feature_8", (y_feat & 8) > 0);
            xml->addparbool("Y_feature_8_R", (y_feat & 0x40) > 0);
            xml->addpar("Y_CCout_2",Runtime.nrpndata.vectorYcc2[baseChan]);
            xml->addpar("Y_CCout_4",Runtime.nrpndata.vectorYcc4[baseChan]);
            xml->addpar("Y_CCout_8",Runtime.nrpndata.vectorYcc8[baseChan]);
        }
        if (full)
        {
            xml->addpar("current_midi_parts", lastPart);
            for (int npart = 0; npart < lastPart; npart += NUM_MIDI_CHANNELS)
            {
                xml->beginbranch("PART",npart);
                    part[npart + baseChan]->add2XML(xml);
                xml->endbranch();
            }
        }
        if (xml->saveXMLfile(file))
            addHistory(file, 5);
        else
        {
            Runtime.Log("Failed to save data to " + file);
            ok = false;
        }
        xml->endbranch();
    delete xml;
    return ok;
}


void SynthEngine::add2XML(XMLwrapper *xml)
{
    xml->beginbranch("MASTER");
    actionLock(lockmute);
    xml->addpar("current_midi_parts", Runtime.NumAvailableParts);
    xml->addpar("volume", Pvolume);
    xml->addpar("key_shift", Pkeyshift);

    xml->beginbranch("MICROTONAL");
    microtonal.add2XML(xml);
    xml->endbranch();

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        xml->beginbranch("PART",npart);
        part[npart]->add2XML(xml);
        xml->endbranch();
    }

    xml->beginbranch("SYSTEM_EFFECTS");
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        xml->beginbranch("SYSTEM_EFFECT", nefx);
        xml->beginbranch("EFFECT");
        sysefx[nefx]->add2XML(xml);
        xml->endbranch();

        for (int pefx = 0; pefx < NUM_MIDI_PARTS; ++pefx)
        {
            xml->beginbranch("VOLUME", pefx);
            xml->addpar("vol", Psysefxvol[nefx][pefx]);
            xml->endbranch();
        }

        for (int tonefx = nefx + 1; tonefx < NUM_SYS_EFX; ++tonefx)
        {
            xml->beginbranch("SENDTO", tonefx);
            xml->addpar("send_vol", Psysefxsend[nefx][tonefx]);
            xml->endbranch();
        }
        xml->endbranch();
    }
    xml->endbranch();

    xml->beginbranch("INSERTION_EFFECTS");
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        xml->beginbranch("INSERTION_EFFECT", nefx);
        xml->addpar("part", Pinsparts[nefx]);

        xml->beginbranch("EFFECT");
        insefx[nefx]->add2XML(xml);
        xml->endbranch();
        xml->endbranch();
    }
    xml->endbranch(); // INSERTION_EFFECTS
    actionLock(unlock);
    xml->endbranch(); // MASTER
}


int SynthEngine::getalldata(char **data)
{
    XMLwrapper *xml = new XMLwrapper(this);
    add2XML(xml);
    *data = xml->getXMLdata();
    delete xml;
    return strlen(*data) + 1;
}


void SynthEngine::putalldata(const char *data, int size)
{
    XMLwrapper *xml = new XMLwrapper(this);
    if (!xml->putXMLdata(data))
    {
        Runtime.Log("SynthEngine: putXMLdata failed");
        delete xml;
        return;
    }
    //if (xml->enterbranch("MASTER"))
    //{
        actionLock(lock);
        defaults();
        getfromXML(xml);
        actionLock(unlock);
        xml->exitbranch();
    //}
    //else
        //Runtime.Log("Master putAllData failed to enter MASTER branch");
    delete xml;
}


bool SynthEngine::saveXML(string filename)
{
    Runtime.xmlType = XML_PARAMETERS;
    XMLwrapper *xml = new XMLwrapper(this);
    add2XML(xml);
    bool result = xml->saveXMLfile(filename);
    delete xml;
    return result;
}


bool SynthEngine::loadXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper(this);
    if (NULL == xml)
    {
        Runtime.Log("Failed to init xml tree");
        return false;
    }
    if (!xml->loadXMLfile(filename))
    {
        delete xml;
        return false;
    }
    defaults();
    bool isok = getfromXML(xml);
    delete xml;
    return isok;
}


bool SynthEngine::getfromXML(XMLwrapper *xml)
{

    if (!xml->enterbranch("BASE_PARAMETERS"))
    {
        Runtime.Log("SynthEngine getfromXML, no BASE branch");
        Runtime.NumAvailableParts = NUM_MIDI_CHANNELS; // set default to be safe
        return false;
    }
    Runtime.NumAvailableParts = xml->getpar("max_midi_parts", NUM_MIDI_CHANNELS, NUM_MIDI_CHANNELS, NUM_MIDI_PARTS); // for backwards compatibility
    xml->exitbranch();
    if (!xml->enterbranch("MASTER"))
    {
        Runtime.Log("SynthEngine getfromXML, no MASTER branch");
        return false;
    }
    Runtime.NumAvailableParts = xml->getpar("current_midi_parts", NUM_MIDI_CHANNELS, NUM_MIDI_CHANNELS, NUM_MIDI_PARTS);
    setPvolume(xml->getpar127("volume", Pvolume));
    setPkeyshift(xml->getpar("key_shift", Pkeyshift, MIN_KEY_SHIFT + 64, MAX_KEY_SHIFT + 64));

    part[0]->Penabled = 0;
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (!xml->enterbranch("PART", npart))
            continue;
        part[npart]->getfromXML(xml);
        xml->exitbranch();
        if (partonoffRead(npart) && (part[npart]->Paudiodest & 2))
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::RegisterAudioPort, npart);
    }

    if (xml->enterbranch("MICROTONAL"))
    {
        microtonal.getfromXML(xml);
        xml->exitbranch();
    }

    sysefx[0]->changeeffect(0);
    if (xml->enterbranch("SYSTEM_EFFECTS"))
    {
        for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        {
            if (!xml->enterbranch("SYSTEM_EFFECT", nefx))
                continue;
            if (xml->enterbranch("EFFECT"))
            {
                sysefx[nefx]->getfromXML(xml);
                xml->exitbranch();
            }

            for (int partefx = 0; partefx < NUM_MIDI_PARTS; ++partefx)
            {
                if (!xml->enterbranch("VOLUME", partefx))
                    continue;
                setPsysefxvol(partefx, nefx,xml->getpar127("vol", Psysefxvol[partefx][nefx]));
                xml->exitbranch();
            }

            for (int tonefx = nefx + 1; tonefx < NUM_SYS_EFX; ++tonefx)
            {
                if (!xml->enterbranch("SENDTO", tonefx))
                    continue;
                setPsysefxsend(nefx, tonefx, xml->getpar127("send_vol", Psysefxsend[nefx][tonefx]));
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if (xml->enterbranch("INSERTION_EFFECTS"))
    {
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        {
            if (!xml->enterbranch("INSERTION_EFFECT", nefx))
                continue;
            Pinsparts[nefx] = xml->getpar("part", Pinsparts[nefx], -2, NUM_MIDI_PARTS);
            if (xml->enterbranch("EFFECT"))
            {
                insefx[nefx]->getfromXML(xml);
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }
    xml->exitbranch(); // MASTER
    return true;
}


float SynthHelper::getDetune(unsigned char type, unsigned short int coarsedetune,
                             unsigned short int finedetune) const
{
    float det = 0.0f;
    float octdet = 0.0f;
    float cdet = 0.0f;
    float findet = 0.0f;
    int octave = coarsedetune / 1024; // get Octave

    if (octave >= 8)
        octave -= 16;
    octdet = octave * 1200.0f;

    int cdetune = coarsedetune % 1024; // coarse and fine detune
    if (cdetune > 512)
        cdetune -= 1024;
    int fdetune = finedetune - 8192;

    switch (type)
    {
        // case 1 is used for the default (see below)
        case 2:
            cdet = fabs(cdetune * 10.0f);
            findet = fabs(fdetune / 8192.0f) * 10.0f;
            break;

        case 3:
            cdet = fabsf(cdetune * 100.0f);
            findet = powf(10.0f, fabs(fdetune / 8192.0f) * 3.0f) / 10.0f - 0.1f;
            break;

        case 4:
            cdet = fabs(cdetune * 701.95500087f); // perfect fifth
            findet = (powf(2.0f, fabs(fdetune / 8192.0f) * 12.0f) - 1.0f) / 4095.0f * 1200.0f;
            break;

            // case ...: need to update N_DETUNE_TYPES, if you'll add more
        default:
            cdet = fabs(cdetune * 50.0f);
            findet = fabs(fdetune / 8192.0f) * 35.0f; // almost like "Paul's Sound Designer 2"
            break;
    }
    if (finedetune < 8192)
        findet = -findet;
    if (cdetune < 0)
        cdet = -cdet;
    det = octdet + cdet + findet;
    return det;
}


MasterUI *SynthEngine::getGuiMaster(bool createGui)
{
    if (guiMaster == NULL && createGui)
        guiMaster = new MasterUI(this);
    return guiMaster;
}


void SynthEngine::guiClosed(bool stopSynth)
{
    if (stopSynth && !isLV2Plugin)
        Runtime.runSynth = false;
    if (guiClosedCallback != NULL)
        guiClosedCallback(guiCallbackArg);
}


void SynthEngine::closeGui()
{
    if (guiMaster != NULL)
    {
        delete guiMaster;
        guiMaster = NULL;
        Runtime.showGui = false;
    }
}


string SynthEngine::makeUniqueName(string name)
{
    string result = "Yoshimi";
    if (uniqueId > 0)
        result += ("-" + asString(uniqueId));
    result += " : " + name;
    return result;
}


void SynthEngine::setWindowTitle(string _windowTitle)
{
    if (!_windowTitle.empty())
        windowTitle = _windowTitle;
}

