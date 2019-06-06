/*
    SynthEngine.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2014-2019, Will Godfrey & others

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
    Street, Fifth Floor, Boston, MA 02110-1301, USA.

    This file is derivative of original ZynAddSubFX code.

    Modified May 2019
*/

#include <sys/types.h>
#include <stdio.h>
#include <sys/time.h>
#include <set>

using namespace std;

#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif

#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "Params/Controller.h"
#include "Misc/Part.h"
#include "Effects/EffectMgr.h"
#include "Misc/XMLwrapper.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

extern void mainRegisterAudioPort(SynthEngine *s, int portnum);
extern std::string runGui;
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
    unifiedpresets(),
    Runtime(this, argc, argv),
    presetsstore(this),
    legatoPart(0),
    masterMono(false),
    fadeAll(0),
    fadeStep(0),
    fadeLevel(0),
    samplerate(48000),
    samplerate_f(samplerate),
    halfsamplerate_f(samplerate / 2),
    buffersize(128),
    buffersize_f(buffersize),
    bufferbytes(buffersize * sizeof(float)),
    oscilsize(512),
    oscilsize_f(oscilsize),
    halfoscilsize(oscilsize / 2),
    halfoscilsize_f(halfoscilsize),
    TransVolume(0.0),
    Pvolume(90),
    ControlStep(0.0),
    Pkeyshift(64),
    syseffnum(0),
    inseffnum(0),
    ctl(NULL),
    microtonal(this),
    fft(NULL),
    VUcount(0),
    VUready(false),
    muted(0),
    volume(0.0),
    keyshift(0),
#ifdef GUI_FLTK
    guiMaster(NULL),
    guiClosedCallback(NULL),
    guiCallbackArg(NULL),
#endif
    LFOtime(0),
    windowTitle("Yoshimi" + asString(uniqueId))
{
    union {
        uint32_t u32 = 0x11223344;
        uint8_t arr[4];
    } x;
    //std::cout << "byte " << int(x.arr[0]) << std::endl;
    Runtime.isLittleEndian = (x.arr[0] == 0x44);

    if (bank.roots.empty())
        bank.addDefaultRootDirs();

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

    // seed the shared master random number generator
    prng.init(time(NULL));

    //TestFunc(123); // just for testing
}


SynthEngine::~SynthEngine()
{
#ifdef GUI_FLTK
    closeGui();
#endif

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

    sem_destroy(&partlock);
    sem_destroy(&mutelock);
    if (ctl)
        delete ctl;
    getRemoveSynthId(true, uniqueId);
}


bool SynthEngine::Init(unsigned int audiosrate)
{
    samplerate_f = samplerate = audiosrate;
    halfsamplerate_f = samplerate_f / 2;
    buffersize = Runtime.Buffersize;
    buffersize_f = float(buffersize);
    bufferbytes = buffersize * sizeof(float);

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

    if (Runtime.paramsLoad.size())
    {
        string file = setExtension(Runtime.paramsLoad, EXTEN::patchset);
        ShutUp();
        if (!loadXML(file))
        {
            Runtime.Log("Failed to load parameters " + file);
            Runtime.paramsLoad = "";
        }
    }
    if (Runtime.instrumentLoad.size())
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
    if (Runtime.midiLearnLoad.size())
    {
        string feml = Runtime.midiLearnLoad;
        if (midilearn.loadList(feml))
        {
#ifdef GUI_FLTK
            midilearn.updateGui();
#endif
            Runtime.Log("midiLearn file " + feml + " loaded");
        }
        else
        {
            Runtime.Log("Failed to load midiLearn file " + feml);
            Runtime.midiLearnLoad = "";
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

    // just to make sure we're in sync
    if (uniqueId == 0)
    {
        if (Runtime.showGui)
            createEmptyFile(runGui);
        else
            deleteFile(runGui);
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
    manfile = manfile.substr(0, manfile.find(" ")); // remove M suffix
    int pos = 0;
    int count = 0;
    for (unsigned i = 0; i < manfile.length(); ++i)
    {
        if (manfile.at(i) == '.')
        {
            pos = i;
            ++count;
        }
    }
    if (count == 3)
        manfile = manfile.substr(0, pos); // remove bugfix number
    return manfile;
}


void SynthEngine::defaults(void)
{
    setPvolume(90);
    TransVolume = Pvolume - 1; // ensure it is always set
    setPkeyshift(64);

    VUpeak.values.vuOutPeakL = 0;
    VUpeak.values.vuOutPeakR = 0;
    VUpeak.values.vuRmsPeakL = 0;
    VUpeak.values.vuRmsPeakR = 0;

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart]->defaults();
        part[npart]->Prcvchn = npart % NUM_MIDI_CHANNELS;
    }
    VUpeak.values.parts[0] = -1.0f;
    VUpeak.values.partsR[0] = -1.0f;
    VUdata.values.parts[0] = -1.0f;
    VUdata.values.partsR[0] = -1.0f;
    VUcopy.values.parts[0]= -1.0f;
    VUcopy.values.partsR[0]= -1.0f;

    partonoffLock(0, 1); // enable the first part

    inseffnum = 0;
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        insefx[nefx]->defaults();
        Pinsparts[nefx] = -1;
    }
    masterMono = false;

    // System Effects init
    syseffnum = 0;
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
                part[npart]->NoteOn(note, velocity);
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
            part[npart]->NoteOff(note);
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
void SynthEngine::SetController(unsigned char chan, int CCtype, short int par)
{
    if (CCtype == Runtime.midi_bank_C)
    {
        //shouldn't get here. Banks are set directly
        return;
    }
    if (CCtype <= 119 && CCtype == Runtime.channelSwitchCC)
    {
        RunChannelSwitch(par);
        return;
    }
    if (CCtype == MIDI::CC::allSoundOff)
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
    //std::cout << "  min " << minPart<< "  max " << maxPart << "  Rec " << int(part[npart]->Prcvchn) << "  Chan " << int(chan) <<std::endl;
    for (npart = minPart; npart < maxPart; ++ npart)
    {   // Send the controller to all part assigned to the channel
        part[npart]->legatoFading = 0;
        if (chan == part[npart]->Prcvchn)
        {
            if (CCtype == part[npart]->PbreathControl) // breath
            {
                part[npart]->SetController(MIDI::CC::volume, 64 + par / 2);
                part[npart]->SetController(MIDI::CC::filterCutoff, par);
            }
            else if (CCtype == 0x44) // legato switch
            {
                int mode = (ReadPartKeyMode(npart) & 3);
                if (par < 64)
                    SetPartKeyMode(npart, mode & 3); // normal
                else
                    SetPartKeyMode(npart, mode | 4); // temporary legato
            }
            else
            {
                //std::cout << "CCtype " << int(CCtype) << "  par " << int(par) << std::endl;
                part[npart]->SetController(CCtype, par);
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
    putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
    // TODO the next line is wrong, it should really be
    // handled by MIDI
    putData.data.source |= TOPLEVEL::action::fromCLI;

    if (group == 0x24)
    {
        putData.data.part = TOPLEVEL::section::systemEffects;
        if (efftype == 0x40)
            putData.data.control = 1;
        //else if (efftype == 0x60) // not done yet
            //putData.data.control = 2;
        else
        {
            putData.data.kit = EFFECT::type::none + sysefx[effnum]->geteffect();
            putData.data.control = parnum;
        }
    }
    else
    {
        putData.data.part = TOPLEVEL::section::insertEffects;
        //std::cout << "efftype " << int(efftype) << std::endl;
        if (efftype == 0x40)
            putData.data.control = 1;
        else if (efftype == 0x60)
            putData.data.control = 2;
        else
        {
            putData.data.kit = EFFECT::type::none + insefx[effnum]->geteffect();
            putData.data.control = parnum;
        }
    }
    putData.data.engine = effnum;

    if (in_place)
        interchange.commandEffects(&putData);
    else // TODO next line is a hack!
        midilearn.writeMidi(&putData, false);
}


int SynthEngine::setRootBank(int root, int banknum, bool notinplace)
{
    string name = "";
    int foundRoot;
    int originalRoot = Runtime.currentRoot;
    int originalBank = Runtime.currentBank;
    bool ok = true;

    if (root < 0x80)
    {
        if (bank.setCurrentRootID(root))
        {
            foundRoot = Runtime.currentRoot;
            if (foundRoot != root)
            { // abort and recover old settings
                bank.setCurrentRootID(originalRoot);
                bank.setCurrentBankID(originalBank);
            }
            else
            {
                originalRoot = foundRoot;
                originalBank = Runtime.currentBank;
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
                if (root < UNUSED)
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
                if(root < UNUSED)
                    name += " in root " + to_string(root) + ".";
                else
                    name += " in this root.";
                name += " Current bank is " + asString(ReadBank());
            }
        }
    }

    int msgID = NO_MSG;
    if (notinplace)
        msgID = miscMsgPush(name);
    if (!ok)
        msgID |= 0xFF0000;
    return msgID;
}


int SynthEngine::setProgramByName(CommandBlock *getData)
{
    int msgID = NO_MSG;
    bool ok = true;
    int npart = int(getData->data.kit);
    string fname = miscMsgPop(getData->data.par2);
    fname = setExtension(fname, EXTEN::yoshInst);
    if (!isRegFile(fname.c_str()))
        fname = setExtension(fname, EXTEN::zynInst);
    string name = findleafname(fname);
    if (name < "!")
    {
        name = "Invalid instrument name " + name;
        ok = false;
    }
    if (ok && !isRegFile(fname.c_str()))
    {
        name = "Can't find " + fname;
        ok = false;
    }
    if (ok)
    {
        ok = setProgram(fname, npart);
        if (!ok)
            name = "File " + name + "unrecognised or corrupted";
    }

    msgID = miscMsgPush(name);
    if (!ok)
    {
        msgID |= 0xFF0000;
        partonoffLock(npart, 2); // as it was
    }
    else
    {
        addHistory(setExtension(fname, EXTEN::zynInst), TOPLEVEL::historyList::Instrument);
        partonoffLock(npart, 2 - Runtime.enable_part_on_voice_load); // always on if enabled
    }
    return msgID;
}


int SynthEngine::setProgramFromBank(CommandBlock *getData, bool notinplace)
{
    struct timeval tv1, tv2;
    if (notinplace && Runtime.showTimes)
        gettimeofday(&tv1, NULL);

    int instrument = int(getData->data.value);
    int banknum = getData->data.engine;
    if (banknum == UNUSED)
        banknum = Runtime.currentBank;
    int npart = getData->data.kit;
    int root = getData->data.insert;
    if (root == UNUSED)
        root = Runtime.currentRoot;

    bool ok;

    string fname = bank.getFullPath(root, banknum, instrument);
    string name = findleafname(fname);
    if (name < "!")
    {
        ok = false;
        if (notinplace)
            name = "No instrument at " + to_string(instrument + 1) + " in this bank";
    }
    else
    {
        ok = setProgram(fname, npart);
        if (notinplace)
        {
            if (!ok)
                name = "Instrument " + name + "missing or corrupted";
        }
    }

    int msgID = NO_MSG;
    if (notinplace)
    {
        if (ok && Runtime.showTimes)
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
    {
        msgID |= 0xFF0000;
        partonoffLock(npart, 2); // as it was
    }
    else
        partonoffLock(npart, 2 - Runtime.enable_part_on_voice_load); // always on if enabled
    return msgID;
}


bool SynthEngine::setProgram(string fname, int npart)
{
    bool ok = true;
    part[npart]->legatoFading = 0;
    if (!part[npart]->loadXMLinstrument(fname))
        ok = false;
    return ok;
}


int SynthEngine::ReadBankRoot(void)
{
    return Runtime.currentRoot;
}


int SynthEngine::ReadBank(void)
{
    return Runtime.currentBank;
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
            if (idx == Runtime.currentRoot)
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
        rootNum = Runtime.currentRoot;
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
                if (idx == Runtime.currentBank)
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
    int root = Runtime.currentRoot;
    string label;

    if (bankNum < 0 || bankNum >= MAX_BANKS_IN_ROOT)
        bankNum = Runtime.currentBank;
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

    root = Runtime.currentRoot;
    if (bank.roots.count(root) > 0 && !bank.roots [root].path.empty())
    {
        label = bank.roots [root].path;
        if (label.at(label.size() - 1) == '/')
            label = label.substr(0, label.size() - 1);
        msg_buf.push_back("  Current Root ID " + asString(root)
                        + "    " + label);
        msg_buf.push_back("  Current Bank ID " + asString(Runtime.currentBank)
                        + "    " + bank.roots [root].banks [Runtime.currentBank].dirname);
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
#ifdef GUI_FLTK
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateMaster, 0);
#endif
            Runtime.Log("Master key shift set to " + asString(value - 64));
            break;

        case 7: // master volume
            setPvolume(value);
#ifdef GUI_FLTK
            GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateMaster, 0);
#endif
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
#ifdef GUI_FLTK
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePart, 0);
#endif
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
#ifdef GUI_FLTK
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
#endif
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
#ifdef GUI_FLTK
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
#endif
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
#ifdef GUI_FLTK
                GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
#endif
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
#ifdef GUI_FLTK
                GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
#endif
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
#ifdef GUI_FLTK
                    GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdateConfig, 4);
#endif
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
#ifdef GUI_FLTK
                GuiThreadMsg::sendMessage(this, GuiThreadMsg::UpdatePart,0);
#endif
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
            Runtime.vectordata.Xcc2[chan] = MIDI::CC::panning;
            Runtime.vectordata.Xcc4[chan] = MIDI::CC::filterCutoff;
            Runtime.vectordata.Xcc8[chan] = MIDI::CC::modulation;
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
                Runtime.vectordata.Ycc2[chan] = MIDI::CC::panning;
                Runtime.vectordata.Ycc4[chan] = MIDI::CC::filterCutoff;
                Runtime.vectordata.Ycc8[chan] = MIDI::CC::modulation;
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
        putData.data.source = TOPLEVEL::action::fromMIDI | TOPLEVEL::action::muteAndLoop;
        putData.data.control = 8;
        putData.data.part = TOPLEVEL::section::midiIn;
        putData.data.kit = part;
        midilearn.writeMidi(&putData, true);
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
    defaults();
    ClearNRPNs();
    if (Runtime.loadDefaultState && isRegFile(Runtime.defaultStateName+ ".state"))
    {
        Runtime.StateFile = Runtime.defaultStateName;
        Runtime.stateRestore();
    }
    if (andML)
        midilearn.generalOpps(0, 0, MIDILEARN::control::clearAll, TOPLEVEL::section::midiLearn, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED);
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
 * Intelligent switch for unknown part status that always
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
    {
        VUpeak.values.parts[npart] = 1e-9f;
        VUpeak.values.partsR[npart] = 1e-9f;
    }
    else if (tmp != 1 && original == 1) // disable if it wasn't already off
    {
        part[npart]->cleanup();
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        {
            if (Pinsparts[nefx] == npart)
                insefx[nefx]->cleanup();
        }
        VUpeak.values.parts[npart] = -1.0f;
        VUpeak.values.partsR[npart] = -1.0f;
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
    putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
    putData.data.control = TOPLEVEL::control::textMessage;
    putData.data.part = TOPLEVEL::section::main;
#ifdef GUI_FLTK
    if (interchange.fromGUI ->write(putData.bytes))
    {
        while(isMuted() == 0)
            usleep (1000);
    }
#endif
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
 * Intelligent switch for unknown mute status that always
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
int SynthEngine::MasterAudio(float *outl [NUM_MIDI_PARTS + 1], float *outr [NUM_MIDI_PARTS + 1])
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
    memset(mainL, 0, bufferbytes);
    memset(mainR, 0, bufferbytes);

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
                memset(outl[npart], 0, bufferbytes);
                memset(outr[npart], 0, bufferbytes);
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
            for (int i = 0; i < buffersize; ++i)
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
            memset(tmpmixl, 0, bufferbytes);
            memset(tmpmixr, 0, bufferbytes);

            // Mix the channels according to the part settings about System Effect
            for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
            {
                if (partLocal[npart]        // it's enabled
                 && Psysefxvol[nefx][npart]        // it's sending an output
                 && (part[npart]->Paudiodest & 1)) // it's connected to the main outs
                {
                    // the output volume of each part to system effect
                    float vol = sysefxvol[nefx][npart];
                    for (int i = 0; i < buffersize; ++i)
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
                    for (int i = 0; i < buffersize; ++i)
                    {
                        tmpmixl[i] += sysefx[nefxfrom]->efxoutl[i] * v;
                        tmpmixr[i] += sysefx[nefxfrom]->efxoutr[i] * v;
                    }
                }
            }
            sysefx[nefx]->out(tmpmixl, tmpmixr);

            // Add the System Effect to sound output
            float outvol = sysefx[nefx]->sysefxgetvolume();
            for (int i = 0; i < buffersize; ++i)
            {
                mainL[i] += tmpmixl[i] * outvol;
                mainR[i] += tmpmixr[i] * outvol;
            }
        }

        for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (part[npart]->Paudiodest & 2){    // Copy separate parts

                for (int i = 0; i < buffersize; ++i)
                {
                    outl[npart][i] = part[npart]->partoutl[i];
                    outr[npart][i] = part[npart]->partoutr[i];
                }
            }
            if (part[npart]->Paudiodest & 1)    // Mix wanted parts to mains
            {
                for (int i = 0; i < buffersize; ++i)
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
        for (int idx = 0; idx < buffersize; ++idx)
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
            if (masterMono)
                mainL[idx] = mainR[idx] = (mainL[idx] + mainR[idx]) / 2.0;
        }

        // Peak calculation for mixed outputs
        float absval;
        for (int idx = 0; idx < buffersize; ++idx)
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
                for (int idx = 0; idx < buffersize; ++idx)
                {
                    if ((absval = fabsf(part[npart]->partoutl[idx])) > VUpeak.values.parts[npart])
                        VUpeak.values.parts[npart] = absval;
                    if ((absval = fabsf(part[npart]->partoutr[idx])) > VUpeak.values.partsR[npart])
                        VUpeak.values.partsR[npart] = absval;
                }
            }
            else
            {
                VUpeak.values.parts[npart] = -1.0f;
                VUpeak.values.partsR[npart] = -1.0f;
            }
        }

        VUcount += buffersize;
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
                {
                    VUpeak.values.parts[npart] = 1.0e-9f;
                    VUpeak.values.partsR[npart] = 1.0e-9f;
                }
                else
                {
                    VUpeak.values.parts[npart] = -1.0f;
                    VUpeak.values.partsR[npart] = -1.0f;
                }

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
            interchange.flagsWrite(fadeAll);
            fadeAll = 0;
        }
    }
    return buffersize;
}


void SynthEngine::fetchMeterData()
{ // overload protection below shouldn't be needed :(
    static int delay = 20;
    if (!VUready)
        return;
    if (delay > 0)
    {
        --delay;
        VUdata.values.vuOutPeakL = 0.0f;
        VUdata.values.vuOutPeakR = 0.0f;
        VUdata.values.vuRmsPeakL = 0.0f;
        VUdata.values.vuRmsPeakR = 0.0f;
        VUready = true;
        return;
    }
    float fade;
    float root;
    int buffsize;
    buffsize = VUcopy.values.buffersize;
    root = sqrt(VUcopy.values.vuRmsPeakL / buffsize);
    if (VUdata.values.vuRmsPeakL >= 1.0f) // overload protection
        VUdata.values.vuRmsPeakL = root;
    else
        VUdata.values.vuRmsPeakL = ((VUdata.values.vuRmsPeakL * 7) + root) / 8;

    root = sqrt(VUcopy.values.vuRmsPeakR / buffsize);
    if (VUdata.values.vuRmsPeakR >= 1.0f) // overload protection
        VUdata.values.vuRmsPeakR = root;
    else
        VUdata.values.vuRmsPeakR = ((VUdata.values.vuRmsPeakR * 7) + root) / 8;

    fade = VUdata.values.vuOutPeakL * 0.92f;// mult;
    if (fade >= 1.0f) // overload protection
        fade = 0.0f;
    if (VUcopy.values.vuOutPeakL > 1.8f) // overload protection
        VUcopy.values.vuOutPeakL = fade;
    else
    {
        if (VUcopy.values.vuOutPeakL > fade)
            VUdata.values.vuOutPeakL = VUcopy.values.vuOutPeakL;
        else
            VUdata.values.vuOutPeakL = fade;
    }

    fade = VUdata.values.vuOutPeakR * 0.92f;// mult;
    if (fade >= 1.0f) // overload protection
        fade = 00.f;
    if (VUcopy.values.vuOutPeakR > 1.8f) // overload protection
        VUcopy.values.vuOutPeakR = fade;
    else
    {
        if (VUcopy.values.vuOutPeakR > fade)
            VUdata.values.vuOutPeakR = VUcopy.values.vuOutPeakR;
        else
            VUdata.values.vuOutPeakR = fade;
    }

    for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
    {
        if (VUpeak.values.parts[npart] < 0.0)
            VUdata.values.parts[npart] = -1.0f;
        else
        {
            fade = VUdata.values.parts[npart];
            if (VUcopy.values.parts[npart] > fade)
                VUdata.values.parts[npart] = VUcopy.values.parts[npart];
            else
                VUdata.values.parts[npart] = fade * 0.85f;
        }
        if (VUpeak.values.partsR[npart] < 0.0)
            VUdata.values.partsR[npart] = -1.0f;
        else
        {
            fade = VUdata.values.partsR[npart];
            if (VUcopy.values.partsR[npart] > fade)
                VUdata.values.partsR[npart] = VUcopy.values.partsR[npart];
            else
                VUdata.values.partsR[npart] = fade * 0.85f;
        }
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


// Panic! (Clean up all parts and effects)
void SynthEngine::ShutUp(void)
{
    VUpeak.values.vuOutPeakL = 1e-12f;
    VUpeak.values.vuOutPeakR = 1e-12f;

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart]->legatoFading = 0;
        part[npart]->cleanup();
        VUpeak.values.parts[npart] = -1.0f;
        VUpeak.values.partsR[npart] = -1.0f;
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


bool SynthEngine::loadStateAndUpdate(string filename)
{
    defaults();
    bool result = Runtime.loadState(filename);
    ShutUp();
    Unmute();
    return result;
}


bool SynthEngine::saveState(string filename)
{
    return Runtime.saveState(filename);
}


bool SynthEngine::loadPatchSetAndUpdate(string fname)
{
    bool result;
    fname = setExtension(fname, EXTEN::patchset);
    result = loadXML(fname); // load the data
    Unmute();
    if (result)
        setAllPartMaps();
    return result;
}


bool SynthEngine::loadMicrotonal(string fname)
{
    return microtonal.loadXML(setExtension(fname, EXTEN::scale));
}

bool SynthEngine::saveMicrotonal(string fname)
{
    return microtonal.saveXML(setExtension(fname, EXTEN::scale));
}


bool SynthEngine::installBanks()
{
    bool banksFound = true;
    string branch;
    string name = Runtime.ConfigDir + '/' + YOSHIMI;

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
    Runtime.Log("\nFound " + asString(bank.InstrumentsInBanks) + " instruments in " + asString(bank.BanksInRoots) + " banks");
    Runtime.Log(miscMsgPop(setRootBank(Runtime.tempRoot, Runtime.tempBank)& 0xff));
#ifdef GUI_FLTK
    GuiThreadMsg::sendMessage((this), GuiThreadMsg::RefreshCurBank, 1);
#endif
    return true;
}


bool SynthEngine::saveBanks()
{
    string name = Runtime.ConfigDir + '/' + YOSHIMI;
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
    if (group == TOPLEVEL::historyList::Instrument && (name.rfind(EXTEN::yoshInst) != string::npos))
        name = setExtension(name, EXTEN::zynInst);
    vector<string> &listType = *getHistory(group);
    listType.push_back(name);
}


void SynthEngine::addHistory(string name, int group)
{
    if (findleafname(name) < "!")
        return;
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
        delete xml;
        return false;
    }
    int hist_size;
    string filetype;
    string type;
    string extension;
    for (int count = TOPLEVEL::historyList::Instrument; count <= TOPLEVEL::historyList::MLearn; ++count)
    {
        switch (count)
        {
            case TOPLEVEL::historyList::Instrument:
                type = "XMZ_INSTRUMENTS";
                extension = "xiz_file";
                break;
            case TOPLEVEL::historyList::Patch:
                type = "XMZ_PATCH_SETS";
                extension = "xmz_file";
                break;
            case TOPLEVEL::historyList::Scale:
                type = "XMZ_SCALE";
                extension = "xsz_file";
                break;
            case TOPLEVEL::historyList::State:
                type = "XMZ_STATE";
                extension = "state_file";
                break;
            case TOPLEVEL::historyList::Vector:
                type = "XMZ_VECTOR";
                extension = "xvy_file";
                break;
            case TOPLEVEL::historyList::MLearn:
                type = "XMZ_MIDILEARN";
                extension = "xly_file";
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
                        if (filetype.rfind(EXTEN::zynInst) != string::npos)
                            filetype = setExtension(filetype, EXTEN::yoshInst);
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
        for (int count = TOPLEVEL::historyList::Instrument; count <= TOPLEVEL::historyList::MLearn; ++count)
        {
            switch (count)
            {
                case TOPLEVEL::historyList::Instrument:
                    type = "XMZ_INSTRUMENTS";
                    extension = "xiz_file";
                    break;
                case TOPLEVEL::historyList::Patch:
                    type = "XMZ_PATCH_SETS";
                    extension = "xmz_file";
                    break;
                case TOPLEVEL::historyList::Scale:
                    type = "XMZ_SCALE";
                    extension = "xsz_file";
                    break;
                case TOPLEVEL::historyList::State:
                    type = "XMZ_STATE";
                    extension = "state_file";
                    break;
                case TOPLEVEL::historyList::Vector:
                    type = "XMZ_VECTOR";
                    extension = "xvy_file";
                    break;
                case TOPLEVEL::historyList::MLearn:
                    type = "XMZ_MIDILEARN";
                    extension = "xly_file";
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
    ShutUp();
    Unmute();
    return result;
}


unsigned char SynthEngine::loadVector(unsigned char baseChan, string name, bool full)
{
    bool a = full; full = a; // suppress warning
    unsigned char actualBase = NO_MSG; // error!
    if (name.empty())
    {
        Runtime.Log("No filename", 2);
        return actualBase;
    }
    string file = setExtension(name, EXTEN::vector);
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
    {
            Runtime. Log("Extract Data, no VECTOR branch", 2);
            delete xml;
    }
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
    unsigned char result = NO_MSG; // ok

    if (baseChan >= NUM_MIDI_CHANNELS)
        return miscMsgPush("Invalid channel number");
    if (name.empty())
        return miscMsgPush("No filename");
    if (Runtime.vectordata.Enabled[baseChan] == false)
        return miscMsgPush("No vector data on this channel");

    string file = setExtension(name, EXTEN::vector);
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

    if (!xml->saveXMLfile(file))
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
    filename = setExtension(filename, EXTEN::patchset);
    Runtime.xmlType = XML_PARAMETERS;
    XMLwrapper *xml = new XMLwrapper(this, true);
    add2XML(xml);
    bool result = xml->saveXMLfile(filename);
    delete xml;
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
    Runtime.channelSwitchType = xml->getpar("channel_switch_type", Runtime.channelSwitchType, 0, 4);
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
#ifdef GUI_FLTK
MasterUI *SynthEngine::getGuiMaster(bool createGui)
{
    if (guiMaster == NULL && createGui)
        guiMaster = new MasterUI(this);
    return guiMaster;
}
#endif

void SynthEngine::guiClosed(bool stopSynth)
{
    if (stopSynth && !isLV2Plugin)
        Runtime.runSynth = false;
#ifdef GUI_FLTK
    if (guiClosedCallback != NULL)
        guiClosedCallback(guiCallbackArg);
#endif
}

#ifdef GUI_FLTK
void SynthEngine::closeGui()
{
    if (guiMaster != NULL)
    {
        delete guiMaster;
        guiMaster = NULL;
    }
}
#endif


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
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    unsigned char type = 0;

    // defaults
    int min = 0;
    float def = 64;
    int max = 127;
    type |= TOPLEVEL::type::Integer;
    unsigned char learnable = TOPLEVEL::type::Learnable;

    switch (control)
    {
        case MAIN::control::volume:
            def = 90;
            type &= ~TOPLEVEL::type::Integer;
            type |= learnable;
            break;

        case MAIN::control::partNumber:
            def = 0;
            max = Runtime.NumAvailableParts -1;
            break;

        case MAIN::control::availableParts:
            min = 16;
            def = 16;
            max = 64;
            break;

        case MAIN::control::detune:
            break;

        case MAIN::control::keyShift:
            min = -36;
            def = 0;
            max = 36;
            break;

        case MAIN::control::mono:
            def = 0; // off
            max = 1;
            break;

        case MAIN::control::soloType:
            def = 0; // Off
            max = 4;
            break;

        case MAIN::control::soloCC:
            min = 14;
            def = 115;
            max = 119;
            break;

        case MAIN::control::masterReset:
        case MAIN::control::masterResetAndMlearn:
        case MAIN::control::stopSound:
            def = 0;
            max = 0;
            break;

        default:
            type |= TOPLEVEL::type::Error;
            break;

    }
    getData->data.type = type;
    if (type & TOPLEVEL::type::Error)
        return 1;

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


float SynthEngine::getVectorLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    unsigned char type = 0;

    // vector defaults
    type |= TOPLEVEL::type::Integer;
    int min = 0;
    float def = 0;
    int max = 1;

    switch (control)
    {
        case VECTOR::control::undefined:
            break;
        case VECTOR::control::name:
            break;
        case VECTOR::control::Xcontroller:
            max = 119;
            break;
        case VECTOR::control::XleftInstrument:
            max = 159;
            break;
        case VECTOR::control::XrightInstrument:
            max = 159;
            break;
        case VECTOR::control::Xfeature0:
            break;
        case VECTOR::control::Xfeature1:
            max = 2;
            break;
        case VECTOR::control::Xfeature2:
            max = 2;
            break;
        case VECTOR::control::Xfeature3:
            max = 2;
            break;
        case VECTOR::control::Ycontroller:
            max = 119;
            break;
        case VECTOR::control::YupInstrument:
            max = 159;
            break;
        case VECTOR::control::YdownInstrument:
            max = 159;
            break;
        case VECTOR::control::Yfeature0:
            break;
        case VECTOR::control::Yfeature1:
            max = 2;
            break;
        case VECTOR::control::Yfeature2:
            max = 2;
            break;
        case VECTOR::control::Yfeature3:
            max = 2;
            break;
        case VECTOR::control::erase:
            break;

        default: // TODO
            type |= TOPLEVEL::type::Error;
            break;
    }
    getData->data.type = type;
    if (type & TOPLEVEL::type::Error)
        return 1;

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


float SynthEngine::getConfigLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    unsigned char type = 0;

    // config defaults
    int min = 0;
    float def = 0;
    int max = 1;
    type |= TOPLEVEL::type::Integer;

    switch (control)
    {
        case CONFIG::control::oscillatorSize:
            min = MIN_OSCIL_SIZE;
            def = 1024;
            max = MAX_OSCIL_SIZE;
            break;
        case CONFIG::control::bufferSize:
            min = MIN_BUFFER_SIZE;
            def = 512;
            max = MAX_BUFFER_SIZE;
           break;
        case CONFIG::control::padSynthInterpolation:
            break;
        case CONFIG::control::virtualKeyboardLayout:
            max = 3;
            break;
        case CONFIG::control::XMLcompressionLevel:
            def = 3;
            max = 9;
            break;
        case CONFIG::control::reportsDestination:
            break;
        case CONFIG::control::savedInstrumentFormat:
            max = 3;
            break;

        case CONFIG::control::defaultStateStart:
            break;
        case CONFIG::control::hideNonFatalErrors:
            break;
        case CONFIG::control::showSplash:
            def = 1;
            break;
        case CONFIG::control::logInstrumentLoadTimes:
            break;
        case CONFIG::control::logXMLheaders:
            break;
        case CONFIG::control::saveAllXMLdata:
            break;
        case CONFIG::control::enableGUI:
            def = 1;
            break;
        case CONFIG::control::enableCLI:
            def = 1;
            break;
        case CONFIG::control::enableAutoInstance:
            def = 1;
            break;
        case CONFIG::control::exposeStatus:
            def = 1;
            max = 2;
            break;

        case CONFIG::control::jackMidiSource:
            min = 3; // anything greater than max
            def = miscMsgPush("default");
            break;
        case CONFIG::control::jackPreferredMidi:
            def = 1;
            break;
        case CONFIG::control::jackServer:
            min = 3;
            def = miscMsgPush("default");
            break;
        case CONFIG::control::jackPreferredAudio:
            def = 1;
            break;
        case CONFIG::control::jackAutoConnectAudio:
            def = 1;
            break;

        case CONFIG::control::alsaMidiSource:
            min = 3;
            def = miscMsgPush("default");
            break;
        case CONFIG::control::alsaPreferredMidi:
            def = 1;
            break;
        case CONFIG::control::alsaAudioDevice:
            min = 3;
            def = miscMsgPush("default");
            break;
        case CONFIG::control::alsaPreferredAudio:
            break;
        case CONFIG::control::alsaSampleRate:
            def = 2;
            max = 3;
            break;

        case CONFIG::control::bankRootCC: // runtime midi checked elsewhere
            def = 0;
            max = 119;
            break;
        case CONFIG::control::bankCC: // runtime midi checked elsewhere
            def = 32;
            max = 119;
            break;
        case CONFIG::control::enableProgramChange:
            break;
        case CONFIG::control::instChangeEnablesPart:
            def = 1;
            break;
        case CONFIG::control::extendedProgramChangeCC: // runtime midi checked elsewhere
            def = 110;
            max = 119;
            break;
        case CONFIG::control::ignoreResetAllCCs:
            break;
        case CONFIG::control::logIncomingCCs:
            break;
        case CONFIG::control::showLearnEditor:
            def = 1;
            break;
        case CONFIG::control::enableNRPNs:
            def = 1;
            break;

        case CONFIG::control::saveCurrentConfig:
            break;

        default:
            type |= TOPLEVEL::type::Error;
            break;
    }
    getData->data.type = type;
    if (type & TOPLEVEL::type::Error)
        return 1;

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
