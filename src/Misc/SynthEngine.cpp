/*
    SynthEngine.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2014-2018, Will Godfrey & others

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

    Modified May 2018
*/

#define NOLOCKS

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

extern void mainRegisterAudioPort(SynthEngine *s, int portnum);
map<SynthEngine *, MusicClient *> synthInstances;
SynthEngine *firstSynth = NULL;

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
//
// histories
static vector<string> InstrumentHistory;
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
    sent_buffersize(0),
    sent_bufferbytes(0),
    sent_buffersize_f(0),
    ctl(NULL),
    microtonal(this),
    fft(NULL),
    muted(0),
    processLock(NULL),
    //stateXMLtree(NULL),
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

    if (Runtime.genMixl)
        fftwf_free(Runtime.genMixl);
    if (Runtime.genMixr)
        fftwf_free(Runtime.genMixr);

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
    sent_all_buffersize_f = buffersize_f;

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

    // similar to above but for parts
    Runtime.genMixl = (float*)fftwf_malloc(bufferbytes);
    Runtime.genMixr = (float*)fftwf_malloc(bufferbytes);

    oscilsize_f = oscilsize = Runtime.Oscilsize;
    halfoscilsize_f = halfoscilsize = oscilsize / 2;
    fadeStep = 10.0f / samplerate; // 100mS fade
    ControlStep = (127.0f / samplerate) * 5.0f; // 200mS for 0 to 127
    int found = 0;

    if (!interchange.Init())
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
#if (HAVE_RANDOM_R)
    memset(&random_buf, 0, sizeof(random_buf));

    if (initstate_r(samplerate + buffersize + oscilsize, random_state,
                    sizeof(random_state), &random_buf))
        Runtime.Log("SynthEngine Init failed on general randomness");
#else
    if (!initstate(samplerate + buffersize + oscilsize, random_state, sizeof(random_state)))
        Runtime.Log("SynthEngine Init failed on general randomness");
#endif

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
            ShutUp();
            if (!loadXML(file))
            {
                Runtime.Log("Failed to load parameters " + file);
                Runtime.paramsLoad = "";
            }
        }
        else if (Runtime.instrumentLoad.size())
        {
            string feli = Runtime.instrumentLoad;
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

    // we seem to need this here only for first time startup :(
    bank.setCurrentBankID(Runtime.tempBank);

    return true;


bail_out:
    if (fft)
        delete fft;
    fft = NULL;

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


string SynthEngine::manualname(void)
{
    string manfile = "yoshimi-user-manual-";
    manfile += YOSHIMI_VERSION;
    return manfile.substr(0, manfile.find(" "));
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

    partonoffLock(0, 1); // enable the first part
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
    VUcount = 0;
    VUready = false;
    Runtime.currentPart = 0;
    Runtime.VUcount = 0;
    Runtime.channelSwitchType = 0;
    Runtime.channelSwitchCC = 128;
    Runtime.channelSwitchValue = 0;
    //CmdInterface.defaults(); // **** need to work out how to call this
    Runtime.NumAvailableParts = NUM_MIDI_CHANNELS;
    ShutUp();
    Runtime.lastfileseen.clear();
    for (int i = 0; i < 7; ++i)
        Runtime.lastfileseen.push_back(Runtime.userHome);

#ifdef REPORT_NOTES_ON_OFF
    Runtime.noteOnSent = 0; // note test
    Runtime.noteOnSeen = 0;
    Runtime.noteOffSent = 0;
    Runtime.noteOffSeen = 0;
#endif

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

// Note On Messages
void SynthEngine::NoteOn(unsigned char chan, unsigned char note, unsigned char velocity)
{
#ifdef REPORT_NOTES_ON_OFF
    ++Runtime.noteOnSeen; // note test
    if (Runtime.noteOnSeen != Runtime.noteOnSent)
        Runtime.Log("Note on diff " + to_string(Runtime.noteOnSent - Runtime.noteOnSeen));
#endif

#ifdef REPORT_NOTEON
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
#endif
    for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
    {
        if (chan == part[npart]->Prcvchn)
        {
            if (partonoffRead(npart))
            {
                actionLock(lockType);
                part[npart]->NoteOn(note, velocity);
                actionLock(unlockType);
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
#ifdef REPORT_NOTES_ON_OFF
    ++Runtime.noteOffSeen; // note test
    if (Runtime.noteOffSeen != Runtime.noteOffSent)
        Runtime.Log("Note off diff " + to_string(Runtime.noteOffSent - Runtime.noteOffSeen));
#endif

    for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
    {
        // mask values 16 - 31 to still allow a note off
        if (chan == (part[npart]->Prcvchn & 0xef) && partonoffRead(npart))
        {
            actionLock(lockType);
            part[npart]->NoteOff(note);
            actionLock(unlockType);
        }
    }
}


int SynthEngine::RunChannelSwitch(int value)
{
    static unsigned int timer = 0;
    if ((interchange.tick - timer) > 511) // approx 60mS
        timer = interchange.tick;
    else if (Runtime.channelSwitchType > 2)
        return 0; // de-bounced

    switch (Runtime.channelSwitchType)
    {
        case 1: // single row
            if (value >= NUM_MIDI_CHANNELS)
                return 1; // out of range
            break;
        case 2: // columns
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
            Runtime.channelSwitchValue = value;
            return 0; // all OK
            break;
        }
        case 3: // loop
            if (value == 0)
                return 0; // do nothing - it's a switch off
            value = (Runtime.channelSwitchValue + 1) % NUM_MIDI_CHANNELS;
            break;
        case 4: // twoway
            if (value == 0)
                return 0; // do nothing - it's a switch off
            if (value >= 64)
                value = (Runtime.channelSwitchValue + 1) % NUM_MIDI_CHANNELS;
            else
                value = (Runtime.channelSwitchValue + NUM_MIDI_CHANNELS - 1) % NUM_MIDI_CHANNELS;
            // add in NUM_MIDI_CHANNELS so always positive
            break;
        default:
            return 2; // unknown
    }
    // vvv column mode never gets here vvv
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
    return 0; // all OK
}


// Controllers
void SynthEngine::SetController(unsigned char chan, int type, short int par)
{
    if (type == Runtime.midi_bank_C)
    {
        //shouldn't get here. Banks are set directly via SetBank method from MusicIO class
        return;
    }
    if (type <= 119 && type == Runtime.channelSwitchCC)
    {
        RunChannelSwitch(par);
        return;
    }
    if (type == C_allsoundsoff)
    {   // cleanup insertion/system FX
        for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
            sysefx[nefx]->cleanup();
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
            insefx[nefx]->cleanup();
        return;
    }

    int minPart, maxPart;

    if (chan < NUM_MIDI_CHANNELS)
    {
        minPart = 0;
        maxPart = Runtime.NumAvailableParts;
    }
    else
    {
        bool vector = (chan >= 0x80);
        chan &= 0x3f;
        if (chan >= Runtime.NumAvailableParts)
            return; // shouldn't be possible
        minPart = chan;
        maxPart = chan + 1;
        if (vector)
            chan &= 0xf;
    }

    int npart;
    //cout << "  min " << minPart<< "  max " << maxPart << "  Rec " << int(part[npart]->Prcvchn) << "  Chan " << int(chan) << endl;
    for (npart = minPart; npart < maxPart; ++ npart)
    {   // Send the controller to all part assigned to the channel
        part[npart]->legatoFading = 0;
        if (chan == part[npart]->Prcvchn)// && partonoffRead(npart))
        {
            if (type == part[npart]->PbreathControl) // breath
            {
                part[npart]->SetController(C_volume, 64 + par / 2);
                part[npart]->SetController(C_filtercutoff, par);
            }
            else if (type == 0x44) // legato switch
            {
                int mode = (ReadPartKeyMode(npart) & 3);
                if (par < 64)
                    SetPartKeyMode(npart, mode & 3); // normal
                else
                    SetPartKeyMode(npart, mode | 4); // temporary legato
            }
            else
            {
                //cout << "type " << int(type) << "  par " << int(par) << endl;
                part[npart]->SetController(type, par);
            }
        }
    }
}


void SynthEngine::SetZynControls(bool in_place)
{
    /*
     * NRPN MSB system / insertion
     * NRPN LSB effect number
     * Data MSB param to change
     * if | 64 LSB sets eff type
     * for insert effect only | 96 LSB sets destination
     *
     * Data LSB param value
     */

    unsigned char group = Runtime.nrpnH | 0x20;
    unsigned char effnum = Runtime.nrpnL;
    unsigned char parnum = Runtime.dataH;
    unsigned char value = Runtime.dataL;
    unsigned char efftype = (parnum & 0x60);
    Runtime.dataL = 0xff; // use once then clear it out

    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));
    putData.data.value = value;
    putData.data.type = 0xd0;

    if (group == 0x24) // system
    {
        putData.data.part = 0xf1;
        if (efftype == 0x40)
            putData.data.control = 1;
        //else if (efftype == 0x60) // not done yet
            //putData.data.control = 2;
        else
        {
            putData.data.kit = 0x80 + sysefx[effnum]->geteffect();
            putData.data.control = parnum;
        }
    }
    else // insertion
    {
        putData.data.part = 0xf2;
        //cout << "efftype " << int(efftype) << endl;
        if (efftype == 0x40)
            putData.data.control = 1;
        else if (efftype == 0x60)
            putData.data.control = 2;
        else
        {
            putData.data.kit = 0x80 + insefx[effnum]->geteffect();
            putData.data.control = parnum;
        }
    }
    putData.data.engine = effnum;

    if (in_place)
        interchange.commandEffects(&putData);
    else
        midilearn.writeMidi(&putData, sizeof(putData), false);
}


int  SynthEngine::RootBank(int rootnum, int banknum)
{
    CommandBlock getData;
    memset(&getData, 0xff, sizeof(getData));
    getData.data.value = 0xff;
    getData.data.engine = banknum;
    getData.data.insert = rootnum;
    return SetRBP(&getData);
}


int SynthEngine::SetRBP(CommandBlock *getData, bool notinplace)
{
    int program = lrint(getData->data.value);
    int npart = getData->data.kit;
    int banknum = getData->data.engine;
    int root = getData->data.insert;
    int par2 = getData->data.par2;

    string name = "";
    int foundRoot;
    int originalRoot = bank.getCurrentRootID();
    int originalBank = bank.getCurrentBankID();
    bool ok = true;
    bool hasProgChange = (program < 0xff || par2 != NO_MSG);

    struct timeval tv1, tv2;
    if (notinplace && Runtime.showTimes && hasProgChange)
        gettimeofday(&tv1, NULL);

    if (root < 0x80)
    {
        if (bank.setCurrentRootID(root))
        {
            foundRoot = bank.getCurrentRootID();
            if (foundRoot != root)
            { // abort and recover old settings
                bank.setCurrentRootID(originalRoot);
                bank.setCurrentBankID(originalBank);
            }
            else
            {
                originalRoot = foundRoot;
                originalBank = bank.getCurrentBankID();
            }
            name = asString(foundRoot) + " \"" + bank.getRootPath(originalRoot) + "\"";
            if (root != foundRoot)
            {
                ok = false;
                if (notinplace)
                    name = "Cant find ID " + asString(root) + ". Current root is " + name;
            }
            else
            {
                name = "Root set to " + name;
            }
        }
        else
        {
            ok = false;
            if (notinplace)
                name = "No match for root ID " + asString(root);
        }
    }

    if (ok && (banknum < 0x80))
    {
        if (bank.setCurrentBankID(banknum, true))
        {
            if (notinplace)
            {
                if (root < 0xff)
                    name = "Root " + to_string(root) + ". ";
                name = name + "Bank set to " + asString(banknum) + " \"" + bank.roots [originalRoot].banks [banknum].dirname + "\"";
            }
            originalBank = banknum;
        }
        else
        {
            ok = false;
            bank.setCurrentBankID(originalBank);
            if (notinplace)
            {
                name = "No bank " + asString(banknum);
                if(root < 0xff)
                    name += " in root " + to_string(root) + ".";
                else
                    name += " in this root.";
                name += " Current bank is " + asString(ReadBank());
            }
        }
    }
    if (hasProgChange)
    {
        part[npart]->legatoFading = 0;
        if (ok)
        {
            string fname;
            if (program < 0xff)
                fname = bank.getfilename(program);
            else
                fname = miscMsgPop(par2);
            if (findleafname(fname) < "!") // can't get a program name less than this
            {
                if (notinplace)
                {
                    name = "Can't find instrument ";
                    if (program < 0xff)
                    name = findleafname(name) + asString(program + 1) + " in this bank";
                    else
                        name += fname;
                }
                ok = false;
            }
            else
            {
                if (notinplace)
                {
                    name = "";
                    if (program < 0xff)
                    {
                        if (root < 0xff)
                            name = "Root " + to_string(originalRoot) + ". ";
                        if (banknum < 0xff)
                            name = name + "Bank " + to_string(originalBank) + ". ";
                    }
                }
                if (part[npart]->loadXMLinstrument(fname))
                {
                    if (notinplace)
                        name += "Loaded ";
                }
                else
                {
                    if (notinplace)
                        name += "Failed to load ";
                    ok = false;
                }
                if (notinplace)
                {
                    if (program < 0xff)
                        name += bank.getname(program);
                    else
                        name += fname;
                    if (ok)
                    {
                        if (par2 < 0xff)
                            addHistory(setExtension(fname, "xiz"), 1);
                        name = name + " to Part " + to_string(npart + 1);
                    }
                }
            }
            if (!ok)
                partonoffLock(npart, 2); // as it was
            else
                partonoffLock(npart, 2 - Runtime.enable_part_on_voice_load); // always on if enabled
        }
        else
            partonoffLock(npart, 2); // as it was
    }

    int msgID = 0xff;
    if (notinplace)
    {
        if (ok && Runtime.showTimes && hasProgChange)
        {
            gettimeofday(&tv2, NULL);
            if (tv1.tv_usec > tv2.tv_usec)
            {
                tv2.tv_sec--;
                tv2.tv_usec += 1000000;
            }
            int actual = ((tv2.tv_sec - tv1.tv_sec) *1000 + (tv2.tv_usec - tv1.tv_usec)/ 1000.0f) + 0.5f;
            name += ("  Time " + to_string(actual) + "mS");
        }
        msgID = miscMsgPush(name);
    }
    if (!ok)
        msgID |= 0xFF0000;
    return msgID;
}


int SynthEngine::ReadBankRoot(void)
{
    return bank.currentRootID; // this is private so handle with care
}


int SynthEngine::ReadBank(void)
{
    return bank.currentBankID; // this is private so handle with care
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
    }
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
    part[npart]->Pkeymode = mode;

/*    if (mode > 2)
        mode = 2;
    switch(mode)
    {
        case 2:
            part[npart]->Ppolymode = 0;
            part[npart]->Plegatomode = 1;
            break;
        case 1:
            part[npart]->Ppolymode = 0;
            part[npart]->Plegatomode = 0;
            break;
        case 0:
        default:
            part[npart]->Ppolymode = 1;
            part[npart]->Plegatomode = 0;
            break;
    }*/
}


int SynthEngine::ReadPartKeyMode(int npart)
{
    return part[npart]->Pkeymode;
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


/*
 * Provides a way of setting dynamic system variables
 * via NRPNs
 */
int SynthEngine::SetSystemValue(int type, int value)
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
            setAllPartMaps();
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateMaster, 0);
            Runtime.Log("Master key shift set to " + asString(value - 64));
            break;

        case 7: // master volume
            setPvolume(value);
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateMaster, 0);
            Runtime.Log("Master volume set to " + asString(value));
            break;

        case 64: // part key shifts
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
                {
                    if (value < MIN_KEY_SHIFT + 64)
                        value = MIN_KEY_SHIFT + 64;
                    else if(value > MAX_KEY_SHIFT + 64)
                        value = MAX_KEY_SHIFT + 64;
                    part[npart]->Pkeyshift = value;
                    setPartMap(npart);
                    Runtime.Log("Part " +asString((int) npart) + "  key shift set to " + asString(value - 64));
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePart, 0);
                }
            break;

        case 80: // root CC
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

        case 81: // bank CC
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

        case 82: // enable program change
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

        case 83: // enable part on program change
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

        case 84: // extended program change CC
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

        case 85: // active parts
            if (value == 16 || value == 32 || value == 64)
            {
                Runtime.NumAvailableParts = value;
                Runtime.Log("Available parts set to " + asString(value));
                GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePart,0);
            }
            else
                Runtime.Log("Out of range");
            break;

        case 86: // obvious!
            Runtime.saveConfig();
            Runtime.Log("Config saved");
            break;

    }
    return 0;
}


bool SynthEngine::vectorInit(int dHigh, unsigned char chan, int par)
{
    string name = "";

    if (dHigh < 2)
    {
        string name = Runtime.masterCCtest(par);
        if (name != "")
        {
            name = "CC " + to_string(par) + " in use for " + name;
            Runtime.Log(name);
            return true;
        }
        int parts = 2* NUM_MIDI_CHANNELS * (dHigh + 1);
        if (parts > Runtime.NumAvailableParts)
            Runtime.NumAvailableParts = parts;
        if (dHigh == 0)
        {
            partonoffLock(chan, 1);
            partonoffLock(chan + NUM_MIDI_CHANNELS, 1);
        }
        else
        {
            partonoffLock(chan + NUM_MIDI_CHANNELS * 2, 1);
            partonoffLock(chan + NUM_MIDI_CHANNELS * 3, 1);
        }
    }
    else if (!Runtime.vectordata.Enabled[chan])
    {
        name = "Vector control must be enabled first";
        return true;
    }

    if (name != "" )
        Runtime.Log(name);
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

    unsigned char part = 0;
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
            part = chan;
            break;

        case 5:
            part = chan | 0x10;
            break;

        case 6:
            part = chan | 0x20;
            break;

        case 7:
            part = chan | 0x30;
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
    if (dHigh >= 4 && dHigh <= 7)
    {
        CommandBlock putData;
        memset(&putData, 0xff, sizeof(putData));
        putData.data.value = par;
        putData.data.type = 0xd0;
        putData.data.control = 8;
        putData.data.part = 0xd9;
        putData.data.kit = part;
        putData.data.parameter = 0xc0;
        midilearn.writeMidi(&putData, sizeof(putData), true);
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


void SynthEngine::resetAll(bool andML)
{
    __sync_and_and_fetch(&interchange.blockRead, 0);
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++ npart)
        part[npart]->busy = false;
    if (Runtime.loadDefaultState && isRegFile(Runtime.defaultStateName+ ".state"))
    {
        Runtime.StateFile = Runtime.defaultStateName;
        Runtime.stateRestore();
    }
    else
    {
        defaults();
        ClearNRPNs();
    }
    if (andML)
        midilearn.generalOpps(0, 0, 96, 240, 255, 255, 255, 255, 255);
    Unmute();
}


// Enable/Disable a part
void SynthEngine::partonoffLock(int npart, int what)
{
    sem_wait(&partlock);
    partonoffWrite(npart, what);
    sem_post(&partlock);
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


void SynthEngine::SetMuteAndWait(void)
{
    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));
    putData.data.value = 0;
    putData.data.type = 0xc0;
    putData.data.control = 0xfe;
    putData.data.part = 0xf0;
    if (jack_ringbuffer_write_space(interchange.fromGUI) >= sizeof(putData))
    {
        jack_ringbuffer_write(interchange.fromGUI, (char*) putData.bytes, sizeof(putData));
        while(isMuted() == 0)
            usleep (1000);
    }
}


bool SynthEngine::isMuted(void)
{
    return (muted < 1);
}


void SynthEngine::Unmute()
{
    sem_wait(&mutelock);
    mutewrite(2);
    sem_post(&mutelock);
}

void SynthEngine::Mute()
{
    sem_wait(&mutelock);
    mutewrite(-1);
    sem_post(&mutelock);
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
    static unsigned int VUperiod = samplerate / 20;
    /*
     * The above line gives a VU refresh of at least 50mS
     * but it may be longer depending on the buffer size
     */
    float *mainL = outl[NUM_MIDI_PARTS]; // tiny optimisation
    float *mainR = outr[NUM_MIDI_PARTS]; // makes code clearer

    float *tmpmixl = Runtime.genMixl;
    float *tmpmixr = Runtime.genMixr;
    sent_buffersize = buffersize;
    sent_bufferbytes = bufferbytes;
    sent_buffersize_f = buffersize_f;

    if ((to_process > 0) && (to_process < buffersize))
    {
        sent_buffersize = to_process;
        sent_bufferbytes = sent_buffersize * sizeof(float);
        sent_buffersize_f = sent_buffersize;
        //Runtime.Log("Short Buffer");
    }

    memset(mainL, 0, sent_bufferbytes);
    memset(mainR, 0, sent_bufferbytes);

    interchange.mediate();
    char partLocal[NUM_MIDI_PARTS]; // isolates loop from possible change
    for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
            partLocal[npart] = partonoffRead(npart);

    if (isMuted())
    {
        for (int npart = 0; npart < (Runtime.NumAvailableParts); ++npart)
        {
            if (partLocal[npart])
            {
                memset(outl[npart], 0, sent_bufferbytes);
                memset(outr[npart], 0, sent_bufferbytes);
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
        actionLock(lockType);
        // Compute part samples and store them ->partoutl,partoutr
        for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (partLocal[npart])
            {
                legatoPart = npart;
                part[npart]->ComputePartSmps();
            }
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
        for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (!partLocal[npart])
                continue;

            float Step = ControlStep;
            for (int i = 0; i < sent_buffersize; ++i)
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

            // Clear the samples used by the system effects
            memset(tmpmixl, 0, sent_bufferbytes);
            memset(tmpmixr, 0, sent_bufferbytes);

            // Mix the channels according to the part settings about System Effect
            for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
            {
                if (partLocal[npart]        // it's enabled
                 && Psysefxvol[nefx][npart]      // it's sending an output
                 && part[npart]->Paudiodest & 1) // it's connected to the main outs
                {
                    // the output volume of each part to system effect
                    float vol = sysefxvol[nefx][npart];
                    for (int i = 0; i < sent_buffersize; ++i)
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
                    for (int i = 0; i < sent_buffersize; ++i)
                    {
                        tmpmixl[i] += sysefx[nefxfrom]->efxoutl[i] * v;
                        tmpmixr[i] += sysefx[nefxfrom]->efxoutr[i] * v;
                    }
                }
            }
            sysefx[nefx]->out(tmpmixl, tmpmixr);

            // Add the System Effect to sound output
            float outvol = sysefx[nefx]->sysefxgetvolume();
            for (int i = 0; i < sent_buffersize; ++i)
            {
                mainL[i] += tmpmixl[i] * outvol;
                mainR[i] += tmpmixr[i] * outvol;
            }
        }

        for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (part[npart]->Paudiodest & 2){    // Copy separate parts

                for (int i = 0; i < sent_buffersize; ++i)
                {
                    outl[npart][i] = part[npart]->partoutl[i];
                    outr[npart][i] = part[npart]->partoutr[i];
                }
            }
            if (part[npart]->Paudiodest & 1)    // Mix wanted parts to mains
            {
                for (int i = 0; i < sent_buffersize; ++i)
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
        for (int idx = 0; idx < sent_buffersize; ++idx)
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
                for (int npart = 0; npart < (Runtime.NumAvailableParts); ++npart)
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
        actionLock(unlockType);

        // Peak calculation for mixed outputs
        float absval;
        for (int idx = 0; idx < sent_buffersize; ++idx)
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
        for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (partLocal[npart])
            {
                for (int idx = 0; idx < sent_buffersize; ++idx)
                {
                    if ((absval = fabsf(part[npart]->partoutl[idx])) > VUpeak.values.parts[npart])
                        VUpeak.values.parts[npart] = absval;
                    if ((absval = fabsf(part[npart]->partoutr[idx])) > VUpeak.values.parts[npart])
                        VUpeak.values.parts[npart] = absval;
                }
            }
        }

        VUcount += sent_buffersize;
        if ((VUcount >= VUperiod && !VUready) || VUcount > (samplerate << 2))
        // ensure this eventually clears if VUready fails
        {
            VUpeak.values.buffersize = VUcount;
            VUcount = 0;
            memcpy(&VUcopy, &VUpeak, sizeof(VUpeak));
            VUready = true;
            VUpeak.values.vuOutPeakL = 1e-12f;
            VUpeak.values.vuOutPeakR = 1e-12f;
            VUpeak.values.vuRmsPeakL = 1e-12f;
            VUpeak.values.vuRmsPeakR = 1e-12f;
            for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
            {
                if (partLocal[npart])
                    VUpeak.values.parts[npart] = 1.0e-9f;
                else if (VUpeak.values.parts[npart] < -2.2f) // fake peak is a negative value
                    VUpeak.values.parts[npart]+= 2.0f;
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
            interchange.returnsDirect(fadeAll);
            fadeAll = 0;
        }
    }
    return sent_buffersize;
}


void SynthEngine::fetchMeterData()
{
    if (!VUready)
        return;
    float fade;
    float root;
    int buffsize;
    buffsize = VUcopy.values.buffersize;
    root = sqrt(VUcopy.values.vuRmsPeakL / buffsize);
    VUdata.values.vuRmsPeakL = ((VUdata.values.vuRmsPeakL * 7) + root) / 8;
    root = sqrt(VUcopy.values.vuRmsPeakR / buffsize);
    VUdata.values.vuRmsPeakR = ((VUdata.values.vuRmsPeakR * 7) + root) / 8;

    fade = VUdata.values.vuOutPeakL * 0.92f;//mult;
    if (VUcopy.values.vuOutPeakL > fade)
        VUdata.values.vuOutPeakL = VUcopy.values.vuOutPeakL;
    else
        VUdata.values.vuOutPeakL = fade;

    fade = VUdata.values.vuOutPeakR * 0.92f;//mult;
    if (VUcopy.values.vuOutPeakR > fade)
        VUdata.values.vuOutPeakR = VUcopy.values.vuOutPeakR;
    else
        VUdata.values.vuOutPeakR = fade;

    for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
    {
        fade = VUdata.values.parts[npart];
        if (VUcopy.values.parts[npart] > fade || VUcopy.values.parts[npart] < -0.1f)
            VUdata.values.parts[npart] = VUcopy.values.parts[npart];
        else
            VUdata.values.parts[npart] = fade * 0.85f;
    }
    VUready = false;
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
        part[npart]->legatoFading = 0;
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
#ifdef NOLOCKS
    lockset a = request; request = a; // suppress warning
    return 0;
#else
    int chk  = -1;

    switch (request)
    {
        case lockType:
            chk = pthread_mutex_lock(processLock);
            break;

        case unlockType:
            chk = pthread_mutex_unlock(processLock);
            break;

        default:
            break;
    }
    return (chk == 0) ? true : false;
#endif
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
    string name = Runtime.ConfigDir + "/yoshimi";
    if (uniqueId > 0)
        name += ("-" + to_string(uniqueId));
    name += ".state";
    if (result && filename != name) // never list default state
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
    Runtime.Log(miscMsgPop(RootBank(Runtime.tempRoot, Runtime.tempBank)& 0xff));
    return true;
}


bool SynthEngine::saveBanks(int instance)
{
    string name = Runtime.ConfigDir + '/' + YOSHIMI;
    if (instance > 0)
        name += ("-" + asString(instance));
    string bankname = name + ".banks";
    Runtime.xmlType = XML_BANK;

    XMLwrapper *xmltree = new XMLwrapper(this, true);
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


void SynthEngine::newHistory(string name, int group)
{
    if (findleafname(name) < "!")
        return;
    if (group == 1 && (name.rfind(".xiy") != string::npos))
        name = setExtension(name, "xiz");
    vector<string> &listType = *getHistory(group);
    listType.push_back(name);
}


void SynthEngine::addHistory(string name, int group)
{
    if (findleafname(name) < "!")
        return;
    if (group == 1 && (name.rfind(".xiy") != string::npos))
        name = setExtension(name, "xiz");
    vector<string> &listType = *getHistory(group);
    vector<string>::iterator itn = listType.begin();
    listType.insert(itn, name);

    for (vector<string>::iterator it = listType.begin() + 1; it < listType.end(); ++ it)
    {
        if (*it == name)
            listType.erase(it);
    }
    setLastfileAdded(group, name);
    return;
}


vector<string> * SynthEngine::getHistory(int group)
{
    switch(group)
    {
        case 1:
            return &InstrumentHistory;
            break;
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


string SynthEngine::lastItemSeen(int group)
{
    vector<string> &listType = *getHistory(group);
    vector<string>::iterator it = listType.begin();
    if (it == listType.end())
        return "";
    else
        return *it;
}


void SynthEngine::setLastfileAdded(int group, string name)
{
    if (name == "")
        name = Runtime.userHome;
    list<string>::iterator it = Runtime.lastfileseen.begin();
    int count = 0;
    while (count < group && it != miscList.end())
    {
        ++it;
        ++count;
    }
    if (it != miscList.end())
        *it = name;
}


string SynthEngine::getLastfileAdded(int group)
{
    list<string>::iterator it = Runtime.lastfileseen.begin();
    int count = 0;
    while ( count < group && it != miscList.end())
    {
        ++it;
        ++count;
    }
    if (it == miscList.end())
        return "";
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
    XMLwrapper *xml = new XMLwrapper(this, true);
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
    for (int count = 1; count < 7; ++count)
    {
        switch (count)
        {
            case 1:
                type = "XMZ_INSTRUMENTS";
                extension = "xiz_file";
                break;
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
                    if (extension == "xiz_file" && !isRegFile(filetype))
                    {
                        if (filetype.rfind(".xiz") != string::npos)
                            filetype = setExtension(filetype, "xiy");
                    }
                    if (filetype.size() && isRegFile(filetype))
                        newHistory(filetype, count);
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

    XMLwrapper *xmltree = new XMLwrapper(this, true);
    if (!xmltree)
    {
        Runtime.Log("saveHistory failed xmltree allocation");
        return false;
    }
    xmltree->beginbranch("HISTORY");
    {
        string type;
        string extension;
        for (int count = 1; count < 7; ++count)
        {
            switch (count)
            {
                case 1:
                    type = "XMZ_INSTRUMENTS";
                    extension = "xiz_file";
                    break;
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
                unsigned int offset = 0;
                int x = 0;
                xmltree->beginbranch(type);
                    xmltree->addpar("history_size", listType.size());
                    if (listType.size() > MAX_HISTORY)
                        offset = listType.size() - MAX_HISTORY;
                    for (vector<string>::iterator it = listType.begin(); it != listType.end() - offset; ++it)
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
    bool a = full; full = a; // suppress warning
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
    XMLwrapper *xml = new XMLwrapper(this, true);
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

                partonoffWrite(npart + baseChan, 1);
                if (part[npart + actualBase]->Paudiodest & 2)
                    mainRegisterAudioPort(this, npart + actualBase);
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
    return baseChan;
}


unsigned char SynthEngine::saveVector(unsigned char baseChan, string name, bool full)
{
    bool a = full; full = a; // suppress warning
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
    XMLwrapper *xml = new XMLwrapper(this, true);
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
    XMLwrapper *xml = new XMLwrapper(this, true);
    add2XML(xml);
    midilearn.insertMidiListData(false, xml);
    *data = xml->getXMLdata();
    delete xml;
    return strlen(*data) + 1;
}


void SynthEngine::putalldata(const char *data, int size)
{
    int a = size; size = a; // suppress warning (may be used later)
    XMLwrapper *xml = new XMLwrapper(this, true);
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
    XMLwrapper *xml = new XMLwrapper(this, true);
    add2XML(xml);
    bool result = xml->saveXMLfile(filename);
    delete xml;
    if (result)
        addHistory(filename,2);
    return result;
}


bool SynthEngine::loadXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper(this, true);
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
    Runtime.channelSwitchCC = xml->getpar("channel_switch_CC", Runtime.channelSwitchCC, 0, 128);
    Runtime.channelSwitchValue = 0;
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (!xml->enterbranch("PART", npart))
            continue;
        part[npart]->getfromXML(xml);
        xml->exitbranch();
        if (partonoffRead(npart) && (part[npart]->Paudiodest & 2))
            mainRegisterAudioPort(this, npart);
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

SynthEngine *SynthEngine::getSynthFromId(unsigned int uniqueId)
{
    map<SynthEngine *, MusicClient *>::iterator itSynth;
    SynthEngine *synth;
    for (itSynth = synthInstances.begin(); itSynth != synthInstances.end(); ++ itSynth)
    {
        synth = itSynth->first;
        if (synth->getUniqueId() == uniqueId)
            return synth;
    }
    synth = synthInstances.begin()->first;
    return synth;
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

float SynthEngine::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & 3);
    int control = getData->data.control;

    // defaults
    int type = (getData->data.type & 0x3f) | 0x80; // set as integer
    int min = 0;
    float def = 64;
    int max = 127;
    //cout << "master control " << to_string(control) << endl;
    switch (control)
    {
        case 0:
            def = 90;
            type = (type &0x3f) | 0x40; // float, learnable
            break;

        case 14:
            min = 1;
            def = 1;
            max = Runtime.NumAvailableParts;;
            break;

        case 15:
            min = 16;
            def = 16;
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
            def = 115;
            max = 119;
            break;

        case 96:
        case 128:
            min = 0;
            def = 0;
            max = 0;
            break;

    }
    getData->data.type = type;

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


float SynthEngine::getVectorLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & 3);
    int control = getData->data.control;

    // defaults
    int type = (getData->data.type & 0x3f) | 0x80; // set as integer
    int min = 0;
    float def = 0;
    int max = NUM_MIDI_CHANNELS;
    //cout << "config control " << to_string(control) << endl;
    switch (control)
    {
        default: // TODO
            //min = -1;
            //def = -1;
            //max = -1;
            //type |= 4; // error
            break;
    }
    getData->data.type = type;
    if (type & 4)
        return 1;

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


float SynthEngine::getConfigLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & 3);
    int control = getData->data.control;

    // defaults
    int type = (getData->data.type & 0x3f) | 0x80; // set as integer
    int min = 0;
    float def = 0;
    int max = 1;
    //cout << "config control " << to_string(control) << endl;
    switch (control)
    {
        case 0:
            min = 256;
            def = 1024;
            max = 16384;
            break;
        case 1:
            min = 16;
            def = 512;
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
            def = 1;
            break;
        case 19:
            break;
        case 20:
            break;
        case 21:
            break;
        case 22:
            def = 1;
            break;
        case 23:
            def = 1;
            break;

        case 32:
            min = 3; // anything greater than max
            def = miscMsgPush("default");
            break;
        case 33:
            def = 1;
            break;
        case 34:
            min = 3;
            def = miscMsgPush("default");
            break;
        case 35:
            def = 1;
            break;
        case 36:
            def = 1;
            break;

        case 48:
            min = 3;
            def = miscMsgPush("default");
            break;
        case 49:
            def = 1;
            break;
        case 50:
            min = 3;
            def = miscMsgPush("default");
            break;
        case 51:
            break;
        case 52:
            def = 2;
            max = 3;
            break;

        case 64:
            break;
        case 65: // runtime midi checked elsewhere
            max = 119;
            break;
        case 67: // runtime midi checked elsewhere
            def = 32;
            max = 119;
            break;
        case 68:
            break;
        case 69:
            def = 1;
            break;
        case 70:
            break;
        case 71: // runtime midi checked elsewhere
            def = 110;
            max = 119;
            break;
        case 72:
            break;
        case 73:
            break;
        case 74:
            def = 1;
            break;

        case 80:
            break;

        default:
            type |= 4; // error
            return 2;
            break;
    }
    getData->data.type = type;
    if (type & 4)
        return 1;
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
