/*
    SynthEngine.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2014-2017, Will Godfrey & others

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

    This file is derivative of original ZynAddSubFX code.

    Modified September 2017
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

#define MUTEX

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
static vector<string> MidiLearnHistory;


SynthEngine::SynthEngine(int argc, char **argv, bool _isLV2Plugin, unsigned int forceId) :
    uniqueId(getRemoveSynthId(false, forceId)),
    isLV2Plugin(_isLV2Plugin),
    needsSaving(false),
    bank(this),
    interchange(this),
    midilearn(this),
    mididecode(this),
    Runtime(this, argc, argv),
    presetsstore(this),
    fadeAll(0),
    fadeLevel(0),
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
    muted(0),
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
    for (int i = 0; i < NUM_MIDI_CHANNELS; ++ i)
        Runtime.vectordata.Name[i] = "No Name " + to_string(i + 1);
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart] = NULL;
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx] = NULL;
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx] = NULL;
    fadeAll = 0;
}


SynthEngine::~SynthEngine()
{
    closeGui();

    if (RBPthreadHandle)
        pthread_join(RBPthreadHandle, NULL);

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

    if (Runtime.genTmp1)
        fftwf_free(Runtime.genTmp1);
    if (Runtime.genTmp2)
        fftwf_free(Runtime.genTmp2);
    if (Runtime.genTmp3)
        fftwf_free(Runtime.genTmp3);
    if (Runtime.genTmp4)
        fftwf_free(Runtime.genTmp4);

    if (tmpmixl)
        fftwf_free(tmpmixl);
    if (tmpmixr)
        fftwf_free(tmpmixr);
    if (fft)
        delete fft;
    pthread_mutex_destroy(&processMutex);
    sem_destroy(&partlock);
    sem_destroy(&mutelock);
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

    /*
     * These replace local memory allocations that
     * were being made every time an add or sub note
     * was processed. Now global so treat with care!
     */
    Runtime.genTmp1 = (float*)fftwf_malloc(bufferbytes);
    Runtime.genTmp2 = (float*)fftwf_malloc(bufferbytes);
    Runtime.genTmp3 = (float*)fftwf_malloc(bufferbytes);
    Runtime.genTmp4 = (float*)fftwf_malloc(bufferbytes);

    oscilsize_f = oscilsize = Runtime.Oscilsize;
    halfoscilsize_f = halfoscilsize = oscilsize / 2;
    fadeStep = 10.0f / samplerate; // 100mS fade
    ControlStep = (127.0f / samplerate) * 5.0f; // 200mS for 0 to 127
    int found = 0;

    if (!interchange.Init(this))
    {
        Runtime.LogError("interChange init failed");
        goto bail_out;
    }

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
        Runtime.Log("SynthEngine failed to create vuringbuf");
        goto bail_out;
    }
    if (jack_ringbuffer_mlock(vuringbuf))
    {
        Runtime.Log("Failed to lock vuringbuf memory");
        goto bail_out;
    }

    if (!(RBPringbuf = jack_ringbuffer_create(512)))
    {
        Runtime.Log("SynthEngine failed to create RBPringbuf");
        goto bail_out;
    }
    if (jack_ringbuffer_mlock(RBPringbuf))
    {
        Runtime.Log("Failed to lock RBPringbuf memory");
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
    sem_init(&mutelock, 0, 1);

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
            string file = setExtension(Runtime.paramsLoad, "xmz");
            if (loadXML(file))
            {
                applyparameters();
                addHistory(file, 2);
                Runtime.Log("Loaded " + file + " parameters");
            }
            else
            {
                Runtime.Log("Failed to load parameters " + file);
                Runtime.paramsLoad = "";
            }
        }
        else if (Runtime.instrumentLoad.size())
        {
            string feli = setExtension(Runtime.instrumentLoad, "xiz");
            int loadtopart = 0;
            if (part[loadtopart]->loadXMLinstrument(feli))
                Runtime.Log("Instrument file " + feli + " loaded");
            else
            {
                Runtime.Log("Failed to load instrument file " + feli);
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


    if (!Runtime.startThread(&RBPthreadHandle, _RBPthread, this, false, 0, false, "RBP"))
    {
        Runtime.Log("Failed to start RBP thread");
        goto bail_out;
    }

    // we seem to need this here only for first time startup :(
    bank.setCurrentBankID(Runtime.tempBank);

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
    string name;
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
                    case 1: // load root
                        SetBankRoot(block.data[1]);
                        break;

                    case 2: // load bank
                        SetBank(block.data[1]);
                        break;

                    case 3: // load standard instrument
                        SetProgram(block.data[1], block.data[2]);
                        break;

                    case 4: // load upper set instrument
                        SetProgram(block.data[1], (block.data[2] + 128));
                        break;

                    case 5: // load file named instrument via miscMsg
                        SetProgramToPart(block.data[1], -1, miscMsgPop(block.data[2]));
                        break;

                    case 6: // cease all sound
                        switch(block.data[1] & 0xff)
                        {
                            case 1:
                                ShutUp();
                                Unmute();
                                break;
                        }
                        break;
                    case 10: // set global fine detune
                        microtonal.Pglobalfinedetune = block.data[1];
                        setAllPartMaps();
                        break;

                    case 11: // set global key shift
                        setPkeyshift(block.data[1]);
                        setAllPartMaps();
                        break;

                    case 12: // set part keyshift
                        part[(unsigned char)block.data[1]]->Pkeyshift = block.data[2];
                        setPartMap(block.data[1]);
                        break;
                }
                // in case it was called from CLI
                Runtime.finishedCLI = true;
            }
            else
                Runtime.Log("Unable to read data from Root/Bank/Program");
        }
        else
            usleep(120); // yes it's a hack but seems reliable
    }
    return NULL;
}


void SynthEngine::defaults(void)
{
    setPvolume(90);
    TransVolume = Pvolume - 1; // ensure it is always set
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
    setAllPartMaps();
    Runtime.currentPart = 0;
    Runtime.channelSwitchType = 0;
    Runtime.channelSwitchCC = 128;
    Runtime.channelSwitchValue = 0;
    //CmdInterface.defaults(); // **** need to work out how to call this
    Runtime.NumAvailableParts = NUM_MIDI_CHANNELS;
    ShutUp();
}


void SynthEngine::setPartMap(int npart)
{
    part[npart]->setNoteMap(part[npart]->Pkeyshift - 64);
    part[npart]->PmapOffset = 128 - part[npart]->PmapOffset;
}


void SynthEngine::setAllPartMaps(void)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++ npart)
        part[npart]->setNoteMap(part[npart]->Pkeyshift - 64);

    // we swap all maps together after they've been changed
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++ npart)
        part[npart]->PmapOffset = 128 - part[npart]->PmapOffset;
}

// Note On Messages (velocity == 0 => NoteOff)
void SynthEngine::NoteOn(unsigned char chan, unsigned char note, unsigned char velocity)
{
#ifdef REPORT_NOTEON
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
#endif
    if (!velocity)
        this->NoteOff(chan, note);
    else if (!isMuted())
        for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (chan == part[npart]->Prcvchn)
            {
               if (partonoffRead(npart))
                {
#ifdef MUTEX
                    actionLock(lock);
#endif
                    part[npart]->NoteOn(note, velocity, keyshift);
#ifdef MUTEX
                    actionLock(unlock);
#endif
                }
                else if (VUpeak.values.parts[npart] > (-velocity))
                    VUpeak.values.parts[npart] = -(0.2 + velocity); // ensure fake is always negative
            }
        }
#ifdef REPORT_NOTEON
    if (Runtime.showTimes)
    {
        gettimeofday(&tv2, NULL);
        if (tv1.tv_usec > tv2.tv_usec)
        {
            tv2.tv_sec--;
            tv2.tv_usec += 1000000;
            }
        int actual = (tv2.tv_sec - tv1.tv_sec) *1000000 + (tv2.tv_usec - tv1.tv_usec);
        Runtime.Log("Note time " + to_string(actual) + "uS");
    }
#endif
}


// Note Off Messages
void SynthEngine::NoteOff(unsigned char chan, unsigned char note)
{
    for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
    {
        // mask values 16 - 31 to still allow a note off
        if (chan == (part[npart]->Prcvchn & 0xef) && partonoffRead(npart))
        {
#ifdef MUTEX
            actionLock(lock);
#endif
            part[npart]->NoteOff(note);
#ifdef MUTEX
            actionLock(unlock);
#endif
        }
    }
}


// Controllers
void SynthEngine::SetController(unsigned char chan, int type, short int par)
{
    if (type == Runtime.midi_bank_C)
    {
        SetBank(par); //shouldn't get here. Banks are set directly via SetBank method from MusicIO class
        return;
    }
    if (type == Runtime.channelSwitchCC)
    {
        SetSystemValue(128, par);
        return;
    }
    int npart;
    if (chan < NUM_MIDI_CHANNELS)
    {
        for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {   // Send the controller to all part assigned to the channel
            if (chan == part[npart]->Prcvchn && partonoffRead(npart))
            {
                part[npart]->SetController(type, par);
                if (type == 7 || type == 10) // only a few
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePanelItem, npart);
                else if (type == 1 || type == 11 || type == 71 || type == 74)
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateControllers, npart);
            }
        }
    }
    else
    {
        npart = chan & 0x7f;
        if (npart < Runtime.NumAvailableParts)
        {
            part[npart]->SetController(type, par);
            if (type == 7 || type == 10) // only a few
                GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePanelItem, npart);
            else if (type == 1 || type == 11 || type == 71 || type == 74)
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateControllers, npart);
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
                Mute();
                insefx[effnum]->changeeffect(value);
                Unmute();
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
    int foundRoot;
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
    int originalRoot = bank.getCurrentRootID();
    int originalBank = bank.getCurrentBankID();
    if (bank.setCurrentRootID(rootnum))
    {
        foundRoot = bank.getCurrentRootID();
        if (foundRoot != rootnum)
        { // abort and recover old settings
            bank.setCurrentRootID(originalRoot);
            bank.setCurrentBankID(originalBank);
            foundRoot = originalRoot;
        }
        if (Runtime.showGui)
        {
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateBankRootDirs, 0);
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::RescanForBanks, 0);
        }
        name = asString(foundRoot) + " \"" + bank.getRootPath(foundRoot) + "\"";
        if (rootnum != foundRoot)
            name = "Cant find ID " + asString(rootnum) + ". Current root is " + name;
        else
        {
            if (Runtime.showTimes)
            {
                gettimeofday(&tv2, NULL);
                if (tv1.tv_usec > tv2.tv_usec)
                {
                    tv2.tv_sec--;
                    tv2.tv_usec += 1000000;
                }
                int actual = (tv2.tv_sec - tv1.tv_sec) *1000000 + (tv2.tv_usec - tv1.tv_usec);
                name += ("  Time " + to_string(actual) + "uS");
            }
            name = "Root set to " + name;
        }
        Runtime.Log(name);
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
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
    if (bank.setCurrentBankID(banknum, true))
    {
        string name = "Bank set to " + asString(banknum) + " \"" + bank.roots [bank.currentRootID].banks [banknum].dirname + "\"";
        if (Runtime.showTimes)
        {
            gettimeofday(&tv2, NULL);
            if (tv1.tv_usec > tv2.tv_usec)
            {
                tv2.tv_sec--;
                tv2.tv_usec += 1000000;
            }
            int actual = (tv2.tv_sec - tv1.tv_sec) *1000000 + (tv2.tv_usec - tv1.tv_usec);
            name += ("  Time " + to_string(actual) + "uS");
        }
        Runtime.Log(name);
        if (Runtime.showGui)
        {
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::RefreshCurBank, 0);
        }
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


/*
 * If we want to change a *specific* part regardless of what MIDI chanel it
 * is listening to we use 0 - 63 ORed with 128
 *
 * Values 0 - 15 will change 'standard' parts listening to that channel
 */
void SynthEngine::SetProgram(unsigned char chan, unsigned short pgm)
{
    bool partOK = true;
    int npart;
    string fname = bank.getfilename(pgm);

    if ((fname == "") || (bank.getname(pgm) < "!")) // can't get a program name less than this
        Runtime.Log("No Program " + asString(pgm + 1) + " in this bank");
    else
    {
        if (chan <  NUM_MIDI_CHANNELS) // a normal program change
        {
            for (npart = 0; npart < NUM_MIDI_CHANNELS; ++npart)
                // we don't want upper parts (16 - 63) activiated!
                if (chan == part[npart]->Prcvchn)
                {
                    // ensure all listening parts succeed
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
    unsigned char enablestate;
    string loaded;

    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);

    sem_wait (&partlock);

    if (Runtime.enable_part_on_voice_load)
        enablestate = 1; // always on
    else
        enablestate = 2; // on if it was before
    partonoffWrite(npart, -1); // more off than before :)
    if (part[npart]->loadXMLinstrument(fname))
    {
        partonoffWrite(npart, enablestate); // must be here to update gui
        loadOK = true;
        // show file instead of program if we got here from Instruments -> Load External...
        loaded = "Loaded " +
                    ((pgm == -1) ? fname : to_string(pgm + 1)
                    + " \"" + bank.getname(pgm) + "\"")
                    + " to Part " + to_string(npart + 1);
        if (Runtime.showTimes)
        {
            gettimeofday(&tv2, NULL);
            if (tv1.tv_usec > tv2.tv_usec)
            {
                tv2.tv_sec--;
                tv2.tv_usec += 1000000;
            }
            int actual = ((tv2.tv_sec - tv1.tv_sec) *1000 + (tv2.tv_usec - tv1.tv_usec)/ 1000.0f) + 0.5f;
            loaded += ("  Time " + to_string(actual) + "mS");
        }
    }
    else
        partonoffWrite(npart, enablestate); // also here to restore failed load state.

    sem_post (&partlock);

    if (!loadOK)
        GuiThreadMsg::sendMessage(this, GuiThreadMsg::GuiAlert,miscMsgPush("Could not load " + fname));
    else
    {
        if (part[npart]->Pname == "Simple Sound")
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::GuiAlert,miscMsgPush("Instrument is called 'Simple Sound', Yoshimi's basic sound name. You should change this if you wish to re-save."));
        Runtime.Log(loaded);
        GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePartProgram, npart);
    }
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
    setPartMap(npart);
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

    if (Runtime.toConsole)
    {
        for (it = msg_buf.begin(); it != msg_buf.end(); ++it)
            Runtime.Log(*it);
            // we need this in case someone is working headless
        cout << "\nReports sent to console window\n\n";
    }
    else if (msg_buf.size() < lines) // Output will fit the screen
    {
        string text = "";
        for (it = msg_buf.begin(); it != msg_buf.end(); ++it)
        {
            text += *it;
            text += "\n";
        }
        Runtime.Log(text);
    }
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
    string prefix;
    unsigned int idx;
    msg_buf.push_back("Root Paths");

    for (idx = 0; idx < MAX_BANK_ROOT_DIRS; ++ idx)
    {
        if (bank.roots.count(idx) > 0 && !bank.roots [idx].path.empty())
        {
            if (idx == bank.getCurrentRootID())
                prefix = " *";
            else
                prefix = "  ";
            label = bank.roots [idx].path;
            if (label.at(label.size() - 1) == '/')
                label = label.substr(0, label.size() - 1);
            msg_buf.push_back(prefix + " ID " + asString(idx) + "     " + label);
        }
    }
}


void SynthEngine::ListBanks(int rootNum, list<string>& msg_buf)
{
    string label;
    string prefix;
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
        for (unsigned int idx = 0; idx < MAX_BANKS_IN_ROOT; ++ idx)
        {
            if (bank.roots [rootNum].banks.count(idx))
            {
                if (idx == bank.getCurrentBankID())
                    prefix = " *";
                else
                    prefix = "  ";
                msg_buf.push_back(prefix + " ID " + asString(idx) + "    "
                                + bank.roots [rootNum].banks [idx].dirname);
            }
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
                    msg_buf.push_back("    ID " + asString(idx + 1) + "    "
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
            name = "  " + asString(npart + 1);
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
    if (!Runtime.vectordata.Enabled[chan])
        return false;

    int Xfeatures = Runtime.vectordata.Xfeatures[chan];
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
    msg_buf.push_back("Channel " + asString(chan + 1));
    msg_buf.push_back("  X CC = " + asString((int)  Runtime.vectordata.Xaxis[chan]) + ",  " + Xtext);
    msg_buf.push_back("  L = " + part[chan]->Pname + ",  R = " + part[chan + 16]->Pname);

    if (Runtime.vectordata.Yaxis[chan] > 0x7f
        || Runtime.NumAvailableParts < NUM_MIDI_CHANNELS * 4)
        msg_buf.push_back("  Y axis disabled");
    else
    {
        int Yfeatures = Runtime.vectordata.Yfeatures[chan];
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
        msg_buf.push_back("  Y CC = " + asString((int) Runtime.vectordata.Yaxis[chan]) + ",  " + Ytext);
        msg_buf.push_back("  U = " + part[chan + 32]->Pname + ",  D = " + part[chan + 48]->Pname);
        msg_buf.push_back("  Name = " + Runtime.vectordata.Name[chan]);
    }
    return true;
}


void SynthEngine::ListSettings(list<string>& msg_buf)
{
    int root;
    string label;

    msg_buf.push_back("Configuration:");
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
            label = "JACK";
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
            label = "JACK";
            break;

        default:
            label = "None";
            break;
    }
    msg_buf.push_back("  Preferred audio " + label);
    msg_buf.push_back("  ALSA MIDI " + Runtime.alsaMidiDevice);
    msg_buf.push_back("  ALSA audio " + Runtime.alsaAudioDevice);
    msg_buf.push_back("  JACK MIDI " + Runtime.jackMidiDevice);
    msg_buf.push_back("  JACK server " + Runtime.jackServer);
    if (Runtime.connectJackaudio)
        label = "on";
    else
        label = "off";
    msg_buf.push_back("  JACK autoconnect " + label);

    if (Runtime.toConsole)
    {
        msg_buf.push_back("  Reports sent to console window");
    }
    else
        msg_buf.push_back("  Reports sent to stdout");
    if (Runtime.loadDefaultState)
        msg_buf.push_back("  Autostate on");
    else
        msg_buf.push_back("  Autostate off");

    if (Runtime.showTimes)
        msg_buf.push_back("  Times on");
    else
        msg_buf.push_back("  Times off");
}


/* Provides a way of setting/reading dynamic system variables
 * from sources other than the gui
 */
int SynthEngine::SetSystemValue(int type, int value)
{
    list<string> msg;
    string label;
    label = "";
    int pos;

    switch (type)
    {
        case 2: // master key shift
            if (value > MAX_KEY_SHIFT + 64)
                value = MAX_KEY_SHIFT + 64;
            else if (value < MIN_KEY_SHIFT + 64) // 3 octaves is enough for anybody :)
                value = MIN_KEY_SHIFT + 64;
            setPkeyshift(value);
            setAllPartMaps();
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
                if (partonoffRead(npart) && part[npart]->Prcvchn == (type - 64))
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
                Runtime.Log("Sending reports to stdout");
            }
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateMaster, 0);
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 1);
            break;

        case 101:
            if (value > 63)
                Runtime.loadDefaultState = true;
            else
                Runtime.loadDefaultState = false;
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 5);
            break;

        case 102:
            if (value > 63)
            {
                Runtime.showTimes = true;
                label = "Enabled";
            }
            else
            {
                Runtime.showTimes = false;
                label = "Disabled";
            }
            Runtime.Log("Part load time " + label);
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 5);
            break;

        case 103:
            if (value > 63)
                Runtime.hideErrors = true;
            else
                Runtime.hideErrors = false;
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 5);

        case 106:
        {
            pos = 0;
            vector<string> &listtype = *getHistory(6);
            vector<string>::iterator it = listtype.begin();
            while (it != listtype.end() && pos != value)
            {
                ++ it;
                ++ pos;
            }
            if (it == listtype.end())
                return -1;
            else
                return miscMsgPush(*it);
        }

        case 107: // list midi learned lines
            if (value <= 0)
                midilearn.listLine(-value);
            else
            {
                midilearn.listAll(msg);
                cliOutput(msg, value);
            }
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
            Runtime.Log("Config saved");
            break;

        case 128: // channel switch
            if (Runtime.channelSwitchType == 1 || Runtime.channelSwitchType == 3) // single row / loop
            {
                if (Runtime.channelSwitchType == 1)
                {
                    if (value >= NUM_MIDI_CHANNELS)
                        return 1; // out of range
                }
                else if (value > 0)
                    value = (Runtime.channelSwitchValue + 1) % NUM_MIDI_CHANNELS; // loop
                else
                    return 0; // do nothing if it's a switch off

                Runtime.channelSwitchValue = value;
                for (int ch = 0; ch < NUM_MIDI_CHANNELS; ++ch)
                {
                    bool isVector = Runtime.vectordata.Enabled[ch];
                    if (ch != value)
                    {
                        part[ch]->Prcvchn = NUM_MIDI_CHANNELS;
                        if (isVector)
                        {
                            part[ch + NUM_MIDI_CHANNELS]->Prcvchn = NUM_MIDI_CHANNELS;
                            part[ch + NUM_MIDI_CHANNELS * 2]->Prcvchn = NUM_MIDI_CHANNELS;
                            part[ch + NUM_MIDI_CHANNELS * 3]->Prcvchn = NUM_MIDI_CHANNELS;
                        }
                    }
                    else
                    {
                        part[ch]->Prcvchn = 0;
                        if (isVector)
                        {
                            part[ch + NUM_MIDI_CHANNELS]->Prcvchn = 0;
                            part[ch + NUM_MIDI_CHANNELS * 2]->Prcvchn = 0;
                            part[ch + NUM_MIDI_CHANNELS * 3]->Prcvchn = 0;
                        }
                    }
                }
            }
            else if (Runtime.channelSwitchType == 2) // columns
            {
                if (value >= NUM_MIDI_PARTS)
                    return 1; // out of range
                int chan = value & 0xf;
                for (int i = chan; i < NUM_MIDI_PARTS; i += NUM_MIDI_CHANNELS)
                {
                    if (i != value)
                        part[i]->Prcvchn = chan | NUM_MIDI_CHANNELS;
                    else
                       part[i]->Prcvchn = chan;
                }
            }
            else
                return 2; // unrecognised
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePart,0);
            break;
    }
    return 0;
}


void SynthEngine::writeRBP(char type, char data0, char data1, char data2)
{
    struct RBP_data block;
    unsigned int writesize = sizeof(RBP_data);
    block.data[0] = type;
    block.data[1] = data0;
    block.data[2] = data1;
    block.data[3] = data2;
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
            Runtime.Log("Unable to write data to Root/Bank/Program");
    }
    else
        Runtime.Log("Root/Bank/Program buffer full!");
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
    else if (!Runtime.vectordata.Enabled[chan])
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
            Runtime.vectordata.Xaxis[chan] = par;
            if (!Runtime.vectordata.Enabled[chan])
            {
                Runtime.vectordata.Enabled[chan] = true;
                Runtime.Log("Vector control enabled");
                // enabling is only done with a valid X CC
            }
            SetPartChan(chan, chan);
            SetPartChan(chan | 16, chan);
            Runtime.vectordata.Xcc2[chan] = C_panning;
            Runtime.vectordata.Xcc4[chan] = C_filtercutoff;
            Runtime.vectordata.Xcc8[chan] = C_modwheel;
            //Runtime.Log("Vector " + asString((int) chan) + " X CC set to " + asString(par));
            break;

        case 1:
            if (!Runtime.vectordata.Enabled[chan])
                Runtime.Log("Vector X axis must be set before Y");
            else
            {
                SetPartChan(chan | 32, chan);
                SetPartChan(chan | 48, chan);
                Runtime.vectordata.Yaxis[chan] = par;
                Runtime.vectordata.Ycc2[chan] = C_panning;
                Runtime.vectordata.Ycc4[chan] = C_filtercutoff;
                Runtime.vectordata.Ycc8[chan] = C_modwheel;
                //Runtime.Log("Vector " + asString(int(chan) + 1) + " Y CC set to " + asString(par));
            }
            break;

        case 2:
            Runtime.vectordata.Xfeatures[chan] = par;
            Runtime.Log("Set X features " + featureList);
            break;

        case 3:
            if (Runtime.NumAvailableParts > NUM_MIDI_CHANNELS * 2)
            {
                Runtime.vectordata.Yfeatures[chan] = par;
                Runtime.Log("Set Y features " + featureList);
            }
            break;

        case 4:
            writeRBP(3, chan | 0x80, par);
            break;

        case 5:
            writeRBP(3, chan | 0x90, par);
            break;

        case 6:
            writeRBP(3, chan | 0xa0, par);
            break;

        case 7:
            writeRBP(3, chan | 0xb0, par);
            break;

        case 8:
            Runtime.vectordata.Xcc2[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " X feature 2 set to " + asString(par));
            break;

        case 9:
            Runtime.vectordata.Xcc4[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " X feature 3 set to " + asString(par));
            break;

        case 10:
            Runtime.vectordata.Xcc8[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " X feature 4 set to " + asString(par));
            break;

        case 11:
            Runtime.vectordata.Ycc2[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " Y feature 2 set to " + asString(par));
            break;

        case 12:
            Runtime.vectordata.Ycc4[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " Y feature 3 set to " + asString(par));
            break;

        case 13:
            Runtime.vectordata.Ycc8[chan] = par;
            Runtime.Log("Channel " + asString((int) chan) + " Y feature 4 set to " + asString(par));
            break;

        default:
            Runtime.vectordata.Enabled[chan] = false;
            Runtime.vectordata.Xaxis[chan] = 0xff;
            Runtime.vectordata.Yaxis[chan] = 0xff;
            Runtime.vectordata.Xfeatures[chan] = 0;
            Runtime.vectordata.Yfeatures[chan] = 0;
            Runtime.Log("Channel " + asString(int(chan) + 1) + " Vector control disabled");
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
        Runtime.vectordata.Enabled[chan] = false;
        Runtime.vectordata.Xaxis[chan] = 0xff;
        Runtime.vectordata.Yaxis[chan] = 0xff;
        Runtime.vectordata.Xfeatures[chan] = 0;
        Runtime.vectordata.Yfeatures[chan] = 0;
        Runtime.vectordata.Name[chan] = "No Name " + to_string (chan + 1);
    }
}


void SynthEngine::resetAll(void)
{
    defaults();
    ClearNRPNs();
    Unmute();
}


// Enable/Disable a part
void SynthEngine::partonoffLock(int npart, int what)
{
    sem_wait (&partlock);
    partonoffWrite(npart, what);
    sem_post (&partlock);
}

/*
 * Intellegent switch for unknown part status that always
 * switches off and later returns original unknown state
 */
void SynthEngine::partonoffWrite(int npart, int what)
{
    if (npart >= Runtime.NumAvailableParts)
        return;
    unsigned char original = part[npart]->Penabled;
    unsigned char tmp = original;
    switch (what)
    {
        case 0: // always off
            tmp = 0;
            break;
        case 1: // always on
            tmp = 1;
            break;
        case -1: // further from on
            tmp -= 1;
            break;
        case 2:
            if (tmp != 1) // nearer to on
                tmp += 1;
            break;
        default:
            return;
    }

    part[npart]->Penabled = tmp;
    if (tmp == 1 && original != 1) // enable if it wasn't already on
        VUpeak.values.parts[npart] = 1e-9f;
    else if (tmp != 1 && original == 1) // disable if it wasn't already off
    {
        part[npart]->cleanup();
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        {
            if (Pinsparts[nefx] == npart)
                insefx[nefx]->cleanup();
        }
        VUpeak.values.parts[npart] = -0.2;
    }
}


char SynthEngine::partonoffRead(int npart)
{
    return (part[npart]->Penabled == 1);
}


void SynthEngine::Unmute()
{
    sem_wait (&mutelock);
    mutewrite(2);
    sem_post (&mutelock);
}

void SynthEngine::Mute()
{
    sem_wait (&mutelock);
    mutewrite(-1);
    sem_post (&mutelock);
}

/*
 * Intellegent switch for unknown mute status that always
 * switches off and later returns original unknown state
 */
void SynthEngine::mutewrite(int what)
{
    unsigned char original = muted;
    unsigned char tmp = original;
    switch (what)
    {
        case 0: // always off
            tmp = 0;
            break;
        case 1: // always on
            tmp = 1;
            break;
        case -1: // further from on
            tmp -= 1;
            break;
        case 2:
            if (tmp != 1) // nearer to on
                tmp += 1;
            break;
        default:
            return;
    }
    muted = tmp;
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

    memset(mainL, 0, p_bufferbytes);
    memset(mainR, 0, p_bufferbytes);

    interchange.mediate();

    int npart;
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
#ifdef MUTEX
        actionLock(lock);
#endif
        // Compute part samples and store them ->partoutl,partoutr
        char partLocal[NUM_MIDI_PARTS]; // isolates loop from possible change
        for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            partLocal[npart] = partonoffRead(npart);
            if (partLocal[npart])
                part[npart]->ComputePartSmps();
        }
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
            if (!partLocal[npart])
                continue;

            float Step = ControlStep;
            for (int i = 0; i < p_buffersize; ++i)
            {
                if (part[npart]->Ppanning - part[npart]->TransPanning > Step)
                    part[npart]->checkPanning(Step);
                else if (part[npart]->TransPanning - part[npart]->Ppanning > Step)
                    part[npart]->checkPanning(-Step);
                if (part[npart]->Pvolume - part[npart]->TransVolume > Step)
                    part[npart]->checkVolume(Step);
                else if (part[npart]->TransVolume - part[npart]->Pvolume > Step)
                    part[npart]->checkVolume(-Step);
                part[npart]->partoutl[i] *= (part[npart]->pannedVolLeft() * part[npart]->ctl->expression.relvolume);
                part[npart]->partoutr[i] *= (part[npart]->pannedVolRight() * part[npart]->ctl->expression.relvolume);
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
                if (partLocal[npart]        // it's enabled
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
        float cStep = ControlStep;
        for (int idx = 0; idx < p_buffersize; ++idx)
        {
            if (Pvolume - TransVolume > cStep)
            {
                TransVolume += cStep;
                volume = dB2rap((TransVolume - 96.0f) / 96.0f * 40.0f);
            }
            else if (TransVolume - Pvolume > cStep)
            {
                TransVolume -= cStep;
                volume = dB2rap((TransVolume - 96.0f) / 96.0f * 40.0f);
            }
            mainL[idx] *= volume; // apply Master Volume
            mainR[idx] *= volume;
            if (fadeAll) // fadeLevel must also have been set
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
#ifdef MUTEX
        actionLock(unlock);
#endif
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

        // Peak computation for part vu meters
        for (npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (partLocal[npart])
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
                if (partLocal[npart])
                    VUpeak.values.parts[npart] = 1.0e-9;
                else if (VUpeak.values.parts[npart] < -2.2) // fake peak is a negative value
                    VUpeak.values.parts[npart]+= 2;
            }
        }
/*
 * This has to be at the end of the audio loop to
 * ensure no contention with VU updates etc.
 */
        if (fadeAll && fadeLevel <= 0.001f)
        {
            Mute();
            fadeLevel = 0; // just to be sure
            if ((fadeAll & 0xff) == 1)
                writeRBP(6, fadeAll); // stop
            else
                interchange.returnsDirect(fadeAll);
            fadeAll = 0;
        }
    }
    return p_buffersize;
}


bool SynthEngine::fetchMeterData(VUtransfer *VUdata)
{
    bool isOK = false;
    if (jack_ringbuffer_read_space(vuringbuf) >= sizeof(VUtransfer))
    {

        jack_ringbuffer_read(vuringbuf, ( char*)VUdata->bytes, sizeof(VUtransfer));
        VUdata->values.vuRmsPeakL = sqrt(VUdata->values.vuRmsPeakL / VUdata->values.p_buffersize);
        VUdata->values.vuRmsPeakR = sqrt(VUdata->values.vuRmsPeakR / VUdata->values.p_buffersize);
        isOK = true;
    }
    return isOK;
}


// Parameter control
void SynthEngine::setPvolume(float control_value)
{
    Pvolume = control_value;
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
}


void SynthEngine::allStop(unsigned int stopType)
{
    fadeAll = stopType;
    if (fadeLevel < 0.001)
        fadeLevel = 1.0f;
    // don't reset if it's already fading.
}


bool SynthEngine::actionLock(lockset request)
{
    int chk  = -1;

    switch (request)
    {
        case lock:
            chk = pthread_mutex_lock(processLock);
            break;

        case unlock:
            chk = pthread_mutex_unlock(processLock);
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


bool SynthEngine::loadStateAndUpdate(string filename)
{
    bool result = Runtime.loadState(filename);
    if (result)
        addHistory(filename, 4);
    ShutUp();
    Unmute();
    return result;
}


bool SynthEngine::saveState(string filename)
{
    filename = setExtension(filename, "state");
    bool result = Runtime.saveState(filename);
    if (result)
        addHistory(filename, 4);
    return result;
}


bool SynthEngine::loadPatchSetAndUpdate(string fname)
{
    bool result;
    fname = setExtension(fname, "xmz");
    result = loadXML(fname); // load the data
    Unmute();
    if (result)
    {
        setAllPartMaps();
        addHistory(fname, 2);
    }
    return result;
}


bool SynthEngine::loadMicrotonal(string fname)
{
    bool ok = true;
    microtonal.defaults();
    if (microtonal.loadXML(setExtension(fname, "xsz")))
        addHistory(fname, 3);
    else
        ok = false;
    return ok;
}

bool SynthEngine::saveMicrotonal(string fname)
{
    bool ok = true;
    if (microtonal.saveXML(setExtension(fname, "xsz")))
        addHistory(fname, 3);
    else
        ok = false;
    return ok;
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
        Runtime.Log("extractConfigData, no " + branch + " branch");
        return false;
    }
    bank.parseConfigFile(xml);
    xml->exitbranch();
    delete xml;
    SetBankRoot(Runtime.tempRoot);
    SetBank(Runtime.tempBank);
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
    if (findleafname(name) < "!")
        return;
    unsigned int offset = 0;
    bool copy = false;
    vector<string> &listType = *getHistory(group);
    if (!listType.size())
    {
        listType.push_back(name);
        Runtime.lastPatchSet = 0;
        return;
    }
    if (listType.size() > MAX_HISTORY)
        offset = listType.size() - MAX_HISTORY;
     // don't test against names that will be dropped when saving
    int lineNo = offset;

    for (vector<string>::iterator it = listType.begin() + offset; it != listType.end(); ++it)
    {
        if (*it == name)
        {
            copy = true;
            break;
        }
        ++ lineNo;
    }
    if (!copy)
        listType.push_back(name);
    if (group == 2)
        Runtime.lastPatchSet = lineNo;
    return;
}


vector<string> * SynthEngine::getHistory(int group)
{
    switch(group)
    {
        case 2:
            return &ParamsHistory;
            break;
        case 3:
            return &ScaleHistory;
            break;
        case 4:
            return &StateHistory;
            break;
        case 5:
            return &VectorHistory;
            break;
        case 6:
            return &MidiLearnHistory;
            break;
        default:
            Runtime.Log("Unrecognised group " + to_string(group) + "\nUsing patchset history");
            return &ParamsHistory;
    }
}


string SynthEngine::lastPatchSetSeen()
{
    if (Runtime.lastPatchSet == -1)
        return "";
    int count = 0;
    vector<string> &listType = *getHistory(2);
    vector<string>::iterator it = listType.begin();
    while (it != listType.end() && count < Runtime.lastPatchSet)
    {
        ++ it;
        ++ count;
    }
    if (it == listType.end())
        return "";
    else
        return *it;
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
    for (int count = 2; count < 7; ++count)
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
            case 6:
                type = "XMZ_MIDILEARN";
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
        for (int count = 2; count < 7; ++count)
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
                case 6:
                    type = "XMZ_MIDILEARN";
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


unsigned char SynthEngine::loadVectorAndUpdate(unsigned char baseChan, string name)
{
    unsigned char result = loadVector(baseChan, name, true);
    if (result < 255)
        addHistory(name, 5);
    ShutUp();
    Unmute();
    return result;
}


unsigned char SynthEngine::loadVector(unsigned char baseChan, string name, bool full)
{
    unsigned char actualBase = 255; // error!
    if (name.empty())
    {
        Runtime.Log("No filename", 2);
        return actualBase;
    }
    string file = setExtension(name, "xvy");
    legit_pathname(file);
    if (!isRegFile(file))
    {
        Runtime.Log("Can't find " + file, 2);
        return actualBase;
    }
    XMLwrapper *xml = new XMLwrapper(this);
    if (!xml)
    {
        Runtime.Log("Load Vector failed XMLwrapper allocation", 2);
        return actualBase;
    }
    xml->loadXMLfile(file);
    if (!xml->enterbranch("VECTOR"))
            Runtime. Log("Extract Data, no VECTOR branch", 2);
    else
    {
        actualBase = extractVectorData(baseChan, xml, findleafname(name));
        int lastPart = NUM_MIDI_PARTS;
        if (Runtime.vectordata.Yaxis[actualBase] >= 0x7f)
            lastPart = NUM_MIDI_CHANNELS * 2;
        for (int npart = 0; npart < lastPart; npart += NUM_MIDI_CHANNELS)
        {
            if (xml->enterbranch("PART", npart))
            {
                part[npart + actualBase]->getfromXML(xml);
                part[npart + actualBase]->Prcvchn = actualBase;
                xml->exitbranch();
                setPartMap(npart + actualBase);
            }
        }
        xml->endbranch(); // VECTOR
    }
    delete xml;
    return actualBase;
}


unsigned char SynthEngine::extractVectorData(unsigned char baseChan, XMLwrapper *xml, string name)
{
    int lastPart = NUM_MIDI_PARTS;
    unsigned char tmp;
    string newname = xml->getparstr("name");

    if (baseChan >= NUM_MIDI_CHANNELS)
        baseChan = xml->getpar255("Source_channel", 0);

    if (newname > "!" && newname.find("No Name") != 1)
        Runtime.vectordata.Name[baseChan] = newname;
    else if (!name.empty())
        Runtime.vectordata.Name[baseChan] = name;
    else
        Runtime.vectordata.Name[baseChan] = "No Name " + to_string(baseChan);

    tmp = xml->getpar255("X_sweep_CC", 0xff);
    if (tmp >= 0x0e && tmp  < 0x7f)
    {
        Runtime.vectordata.Xaxis[baseChan] = tmp;
        Runtime.vectordata.Enabled[baseChan] = true;
    }
    else
    {
        Runtime.vectordata.Xaxis[baseChan] = 0x7f;
        Runtime.vectordata.Enabled[baseChan] = false;
    }

    // should exit here if not enabled

    tmp = xml->getpar255("Y_sweep_CC", 0xff);
    if (tmp >= 0x0e && tmp  < 0x7f)
        Runtime.vectordata.Yaxis[baseChan] = tmp;
    else
    {
        lastPart = NUM_MIDI_CHANNELS * 2;
        Runtime.vectordata.Yaxis[baseChan] = 0x7f;
        partonoffWrite(baseChan + NUM_MIDI_CHANNELS * 2, 0);
        partonoffWrite(baseChan + NUM_MIDI_CHANNELS * 3, 0);
        // disable these - not in current vector definition
    }

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
    Runtime.vectordata.Xcc2[baseChan] = xml->getpar255("X_CCout_2", 10);
    Runtime.vectordata.Xcc4[baseChan] = xml->getpar255("X_CCout_4", 74);
    Runtime.vectordata.Xcc8[baseChan] = xml->getpar255("X_CCout_8", 1);
    if (lastPart == NUM_MIDI_PARTS)
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
        Runtime.vectordata.Ycc2[baseChan] = xml->getpar255("Y_CCout_2", 10);
        Runtime.vectordata.Ycc4[baseChan] = xml->getpar255("Y_CCout_4", 74);
        Runtime.vectordata.Ycc8[baseChan] = xml->getpar255("Y_CCout_8", 1);
    }
    Runtime.vectordata.Xfeatures[baseChan] = x_feat;
    Runtime.vectordata.Yfeatures[baseChan] = y_feat;
    if (Runtime.NumAvailableParts < lastPart)
        Runtime.NumAvailableParts = xml->getpar255("current_midi_parts", Runtime.NumAvailableParts);

    for (int npart = 0; npart < lastPart; npart += NUM_MIDI_CHANNELS)
    {
        partonoffWrite(npart + baseChan, 1);
        if (part[npart + baseChan]->Paudiodest & 2)
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::RegisterAudioPort, npart + baseChan);
    }
    return baseChan;
}


unsigned char SynthEngine::saveVector(unsigned char baseChan, string name, bool full)
{
    unsigned char result = 0xff; // ok

    if (baseChan >= NUM_MIDI_CHANNELS)
        return miscMsgPush("Invalid channel number");
    if (name.empty())
        return miscMsgPush("No filename");
    if (Runtime.vectordata.Enabled[baseChan] == false)
        return miscMsgPush("No vector data on this channel");

    string file = setExtension(name, "xvy");
    legit_pathname(file);

    Runtime.xmlType = XML_VECTOR;
    XMLwrapper *xml = new XMLwrapper(this);
    if (!xml)
    {
        Runtime.Log("Save Vector failed xmltree allocation", 2);
        return miscMsgPush("FAIL");
    }
    xml->beginbranch("VECTOR");
        insertVectorData(baseChan, true, xml, findleafname(file));
    xml->endbranch();

    if (xml->saveXMLfile(file))
        addHistory(file, 5);
    else
    {
        Runtime.Log("Failed to save data to " + file, 2);
        result = miscMsgPush("FAIL");
    }
    delete xml;
    return result;
}


bool SynthEngine::insertVectorData(unsigned char baseChan, bool full, XMLwrapper *xml, string name)
{
    int lastPart = NUM_MIDI_PARTS;
    int x_feat = Runtime.vectordata.Xfeatures[baseChan];
    int y_feat = Runtime.vectordata.Yfeatures[baseChan];

    if (Runtime.vectordata.Name[baseChan].find("No Name") != 1)
        xml->addparstr("name", Runtime.vectordata.Name[baseChan]);
    else
        xml->addparstr("name", name);

    xml->addpar("Source_channel", baseChan);
    xml->addpar("X_sweep_CC", Runtime.vectordata.Xaxis[baseChan]);
    xml->addpar("Y_sweep_CC", Runtime.vectordata.Yaxis[baseChan]);
    xml->addparbool("X_feature_1", (x_feat & 1) > 0);
    xml->addparbool("X_feature_2", (x_feat & 2) > 0);
    xml->addparbool("X_feature_2_R", (x_feat & 0x10) > 0);
    xml->addparbool("X_feature_4", (x_feat & 4) > 0);
    xml->addparbool("X_feature_4_R", (x_feat & 0x20) > 0);
    xml->addparbool("X_feature_8", (x_feat & 8) > 0);
    xml->addparbool("X_feature_8_R", (x_feat & 0x40) > 0);
    xml->addpar("X_CCout_2",Runtime.vectordata.Xcc2[baseChan]);
    xml->addpar("X_CCout_4",Runtime.vectordata.Xcc4[baseChan]);
    xml->addpar("X_CCout_8",Runtime.vectordata.Xcc8[baseChan]);
    if (Runtime.vectordata.Yaxis[baseChan] > 0x7f)
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
        xml->addpar("Y_CCout_2",Runtime.vectordata.Ycc2[baseChan]);
        xml->addpar("Y_CCout_4",Runtime.vectordata.Ycc4[baseChan]);
        xml->addpar("Y_CCout_8",Runtime.vectordata.Ycc8[baseChan]);
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
    return true;
}


void SynthEngine::add2XML(XMLwrapper *xml)
{
    xml->beginbranch("MASTER");
    xml->addpar("current_midi_parts", Runtime.NumAvailableParts);
    xml->addpar("volume", Pvolume);
    xml->addpar("key_shift", Pkeyshift);
    xml->addpar("channel_switch_type", Runtime.channelSwitchType);
    xml->addpar("channel_switch_CC", Runtime.channelSwitchCC);

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
    for (int i = 0; i < NUM_MIDI_CHANNELS; ++i)
    {
        if (Runtime.vectordata.Xaxis[i] < 127)
        {
            xml->beginbranch("VECTOR", i);
            insertVectorData(i, false, xml, "");
            xml->endbranch(); // VECTOR
        }
    }
    xml->endbranch(); // MASTER
}


int SynthEngine::getalldata(char **data)
{
    XMLwrapper *xml = new XMLwrapper(this);
    add2XML(xml);
    midilearn.insertMidiListData(false, xml);
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
    defaults();
    getfromXML(xml);
    midilearn.extractMidiListData(false, xml);
    setAllPartMaps();
    delete xml;
}


bool SynthEngine::savePatchesXML(string filename)
{
    filename = setExtension(filename, "xmz");
    Runtime.xmlType = XML_PARAMETERS;
    XMLwrapper *xml = new XMLwrapper(this);
    add2XML(xml);
    bool result = xml->saveXMLfile(filename);
    delete xml;
    if (result)
        addHistory(filename,2);
    return result;
}


bool SynthEngine::loadXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper(this);
    if (NULL == xml)
    {
        Runtime.Log("Failed to init xml tree", 2);
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
    setAllPartMaps();
    return isok;
}


bool SynthEngine::getfromXML(XMLwrapper *xml)
{
    if (!xml->enterbranch("MASTER"))
    {
        Runtime.Log("SynthEngine getfromXML, no MASTER branch");
        return false;
    }
    Runtime.NumAvailableParts = xml->getpar("current_midi_parts", NUM_MIDI_CHANNELS, NUM_MIDI_CHANNELS, NUM_MIDI_PARTS);
    setPvolume(xml->getpar127("volume", Pvolume));
    setPkeyshift(xml->getpar("key_shift", Pkeyshift, MIN_KEY_SHIFT + 64, MAX_KEY_SHIFT + 64));
    Runtime.channelSwitchType = xml->getpar("channel_switch_type", Runtime.channelSwitchType, 0, 3);
    Runtime.channelSwitchCC = xml->getpar127("channel_switch_CC", Runtime.channelSwitchCC);
    Runtime.channelSwitchValue = 0;

    partonoffWrite(0, 0); // why?;
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
    for (unsigned char i = 0; i < NUM_MIDI_CHANNELS; ++i)
    {
        if (xml->enterbranch("VECTOR", i))
        {
            extractVectorData(i, xml, "");
            xml->endbranch();
        }
    }
    xml->endbranch(); // MASTER
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
        //Runtime.showGui = false;
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

void SynthEngine::getLimits(CommandBlock *getData)
{
    int control = getData->data.control;
    // defaults
    int type = (getData->data.type & 0x3f) | 0x80; // set as integer
    int min = 0;
    int def = 640;
    int max = 127;
    //cout << "master control " << to_string(control) << endl;
    switch (control)
    {
        case 0:
            def = 900;
            type = (type &0x3f) | 0x40; // float, learnable
            break;

        case 14:
            min = 1;
            def = 10;
            max = Runtime.NumAvailableParts;;
            break;

        case 15:
            min = 16;
            def = 160;
            max = 64;
            break;

        case 32:
            type |= 0x40;
            break;

        case 35:
            min = -36;
            def = 0;
            max = 36;
            break;

        case 48:
            def = 0;
            max = 3;
            break;

        case 49:
            min = 14;
            def = 1150;
            max = 119;
            break;

        case 96:
        case 128:
            min = 0;
            def = 0;
            max = 0;
            break;

        default:
            min = -1;
            def = -10;
            max = -1;
            break;
    }
    getData->data.type = type;
    getData->limits.min = min;
    getData->limits.def = def;
    getData->limits.max = max;
}


void SynthEngine::getVectorLimits(CommandBlock *getData)
{
    int control = getData->data.control;
    // defaults
    int type = (getData->data.type & 0x3f) | 0x80; // set as integer
    int min = 0;
    int def = 0;
    int max = NUM_MIDI_CHANNELS;
    //cout << "config control " << to_string(control) << endl;
    switch (control)
    {
        default:
            break;
    }
    getData->data.type = type;
    getData->limits.min = min;
    getData->limits.def = def;
    getData->limits.max = max;
}


void SynthEngine::getConfigLimits(CommandBlock *getData)
{
    int control = getData->data.control;
    // defaults
    int type = (getData->data.type & 0x3f) | 0x80; // set as integer
    int min = 0;
    int def = 0;
    int max = 1;
    //cout << "config control " << to_string(control) << endl;
    switch (control)
    {
        case 0:
            min = 256;
            def = 10240;
            max = 16384;
            break;
        case 1:
            min = 16;
            def = 5120;
            max = 4096;
           break;
        case 2:
            break;
        case 3:
            max = 3;
            break;
        case 4:
            def = 3;
            max = 9;
            break;
        case 5:
            break;

        case 16:
            break;
        case 17:
            break;
        case 18:
            def = 10;
            break;
        case 19:
            break;
        case 20:
            break;
        case 21:
            break;
        case 22:
            def = 10;
            break;
        case 23:
            def = 10;
            break;

        case 32:
            min = 3; // anything greater than max
            def = miscMsgPush("default");
            break;
        case 33:
            def = 10;
            break;
        case 34:
            min = 3;
            def = miscMsgPush("default");
            break;
        case 35:
            def = 10;
            break;
        case 36:
            def = 10;
            break;

        case 48:
            min = 3;
            def = miscMsgPush("default");
            break;
        case 49:
            def = 10;
            break;
        case 50:
            min = 3;
            def = miscMsgPush("default");
            break;
        case 51:
            break;
        case 52:
            def = 20;
            max = 3;
            break;

        case 64:
            break;
        case 65: // runtime midi checked elsewhere
            max = 119;
            break;
        case 67: // runtime midi checked elsewhere
            def = 320;
            max = 119;
            break;
        case 68:
            break;
        case 69:
            def = 10;
            break;
        case 70:
            break;
        case 71: // runtime midi checked elsewhere
            def = 1100;
            max = 119;
            break;
        case 72:
            break;
        case 73:
            break;
        case 74:
            def = 10;
            break;

        case 80:
            break;

    }
    getData->data.type = type;
    getData->limits.min = min;
    getData->limits.def = def;
    getData->limits.max = max;
}
