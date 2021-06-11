/*
    SynthEngine.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2014-2020, Will Godfrey & others

    Copyright 2021, Will Godfrey, Rainer Hans Liffers

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

*/

#include <sys/time.h>
#include <set>
#include <iostream>
#include <string>
#include <algorithm>
#include <iterator>

#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif

#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "Params/Controller.h"
#include "Misc/Part.h"
#include "Effects/EffectMgr.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/NumericFuncs.h"
#include "Misc/FormatFuncs.h"
#include "Misc/XMLwrapper.h"

using file::isRegularFile;
using file::setExtension;
using file::findLeafName;
using file::createEmptyFile;
using file::deleteFile;
using file::make_legit_pathname;

using func::dB2rap;
using func::bitTest;
using func::asString;
using func::string2int;

using std::set;


extern void mainRegisterAudioPort(SynthEngine *s, int portnum);

map<SynthEngine *, MusicClient *> synthInstances;
SynthEngine *firstSynth = NULL;

namespace { // Global implementation internal history data
    static vector<string> InstrumentHistory;
    static vector<string> ParamsHistory;
    static vector<string> ScaleHistory;
    static vector<string> StateHistory;
    static vector<string> PresetHistory;
    static vector<string> VectorHistory;
    static vector<string> MidiLearnHistory;
    static vector<string> PadHistory;
    static vector<string> TuningHistory;
    static vector<string> KeymapHistory;
}


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


SynthEngine::SynthEngine(int argc, char **argv, bool _isLV2Plugin, unsigned int forceId) :
    uniqueId(getRemoveSynthId(false, forceId)),
    isLV2Plugin(_isLV2Plugin),
    needsSaving(false),
    bank(this),
    interchange(this),
    midilearn(this),
    mididecode(this),
    //unifiedpresets(this),
    Runtime(this, argc, argv),
    presetsstore(this),
    textMsgBuffer(TextMsgBuffer::instance()),
    fadeAll(0),
    fadeStepShort(0),
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
#ifdef GUI_FLTK
    guiMaster(NULL),
    guiClosedCallback(NULL),
    guiCallbackArg(NULL),
#endif
    CHtimer(0),
    LFOtime(0),
    songBeat(0.0f),
    monotonicBeat(0.0f),
    windowTitle("Yoshimi" + asString(uniqueId))
{
    union {
        uint32_t u32 = 0x11223344;
        uint8_t arr[4];
    } x;
    Runtime.isLittleEndian = (x.arr[0] == 0x44);
    meterDelay = 20;
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

    for (int i = 0; i <= TOPLEVEL::XML::ScalaMap; ++i)
        Runtime.historyLock[i] = false;

    // seed the shared master random number generator
    prng.init(time(NULL));
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
    if (ctl)
        delete ctl;
    getRemoveSynthId(true, uniqueId);
}


bool SynthEngine::Init(unsigned int audiosrate, int audiobufsize)
{
    audioOut.store(_SYS_::mute::Active);
    samplerate_f = samplerate = audiosrate;
    halfsamplerate_f = samplerate_f / 2;

    buffersize = Runtime.Buffersize;
    if (buffersize > audiobufsize)
        buffersize = audiobufsize;
    buffersize_f = buffersize;
    fixed_sample_step_f = buffersize_f / samplerate_f;
    bufferbytes = buffersize * sizeof(float);

    oscilsize_f = oscilsize = Runtime.Oscilsize;
    if (oscilsize < (buffersize / 2))
    {
        Runtime.Log("Enforcing oscilsize to half buffersize, "
                    + asString(oscilsize) + " -> " + asString(buffersize / 2));
        oscilsize_f = oscilsize = buffersize / 2;
    }
    halfoscilsize_f = halfoscilsize = oscilsize / 2;
    oscil_sample_step_f = oscilsize_f / samplerate_f;

    // Phase and frequency modulation are calculated in terms of samples, not
    // angle/frequency, so modulation must be normalized to reference values of
    // angle/sample and time/sample.

    // oscilsize is one wavelength worth of samples, so
    // phase modulation should scale proportionally
    oscil_norm_factor_pm = oscilsize_f / oscilsize_ref_f;
    // FM also depends on samples/wavelength as well as samples/time,
    // so scale FM inversely with the sample rate.
    oscil_norm_factor_fm =
        oscil_norm_factor_pm * (samplerate_ref_f / samplerate_f);

    // distance / duration / second = distance / (duration * second)
    // While some might prefer to write this as the latter, when distance and
    // duration are constants the latter incurs two roundings while the former
    // brings the constants together, allowing constant-folding. -ffast-math
    // produces the same assembly in both cases, and we normally compile with it
    // enabled, but it's probably a bad habit to rely on non-IEEE float math too
    // much. If we were doing integer division, even -ffast-math wouldn't save
    // us, and the rounding behaviour would actually be important.
    fadeStep = 1.0f / 0.1f / samplerate_f; // 100ms for 0 to 1
    fadeStepShort = 1.0f / 0.005f / samplerate_f; // 5ms for 0 to 1
    ControlStep = 127.0f / 0.2f / samplerate_f; // 200ms for 0 to 127

    if (!(fft = new FFTwrapper(oscilsize)))
    {
        Runtime.Log("SynthEngine failed to allocate fft");
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

    if (Runtime.sessionStage == _SYS_::type::Default || Runtime.sessionStage == _SYS_::type::StartupSecond || Runtime.sessionStage == _SYS_::type::JackSecond)
        Runtime.restoreSessionData(Runtime.StateFile);
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
    /*
     * put here so its threads don't run until everthing else is ready
     */
    if (!interchange.Init())
    {
        Runtime.LogError("interChange init failed");
        goto bail_out;
    }

    // we seem to need this here only for first time startup :(
    bank.setCurrentBankID(Runtime.tempBank, false);
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
    manfile = manfile.substr(0, manfile.find(" ")); // remove M or rc suffix
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
    for (int i = 0; i <NUM_MIDI_PARTS; ++i)
        partonoffLock(i, 0); // ensure parts are disabled
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

    inseffnum = 0;
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        insefx[nefx]->defaults();
        Pinsparts[nefx] = -1;
    }
    masterMono = false;
    fileCompatible = true;
    usingYoshiType = false;

    // System Effects init
    syseffnum = 0;
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        syseffEnable[nefx] = true;
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
    Runtime.channelSwitchType = MIDI::SoloType::Disabled;
    Runtime.channelSwitchCC = 128;
    Runtime.channelSwitchValue = 0;
    //CmdInterface.defaults(); // **** need to work out how to call this
    Runtime.NumAvailableParts = NUM_MIDI_CHANNELS;
    Runtime.panLaw = MAIN::panningType::normal;
    ShutUp();
    Runtime.lastfileseen.clear();
    for (int i = 0; i <= TOPLEVEL::XML::ScalaMap; ++i)
    {
        Runtime.lastfileseen.push_back(Runtime.userHome);
        Runtime.sessionSeen[i] = false;
    }

#ifdef REPORT_NOTES_ON_OFF
    Runtime.noteOnSent = 0; // note test
    Runtime.noteOnSeen = 0;
    Runtime.noteOffSent = 0;
    Runtime.noteOffSeen = 0;
#endif

    Runtime.effectChange = UNUSED; // temporary fix
    partonoffLock(0, 1); // enable the first part
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


int SynthEngine::RunChannelSwitch(unsigned char chan, int value)
{
    int switchtype = Runtime.channelSwitchType;
    if (switchtype > MIDI::SoloType::Channel)
        return 2; // unknown

    if (switchtype >= MIDI::SoloType::Loop)
    {
        if (switchtype != MIDI::SoloType::Channel)
        {
            if (value == 0)
                return 0; // we ignore switch off for these
    /*
     * loop and twoway are increment counters
     * we assume nobody can repeat a switch press within 60mS!
     */
            if ((interchange.tick - CHtimer) > 511) // approx 60mS
                CHtimer = interchange.tick;
            else
                return 0; // de-bounced
        }
        if (value >= 64)
            value = 1;
        else if (switchtype == MIDI::SoloType::TwoWay)
            value = -1;
        else
            value = 0;
    }
    if ((switchtype <= MIDI::SoloType::Column || switchtype == MIDI::SoloType::Channel) && value == Runtime.channelSwitchValue)
        return 0; // nothing changed

    switch (switchtype)
    {
        case MIDI::SoloType::Row:
            if (value >= NUM_MIDI_CHANNELS)
                return 1; // out of range
            break;
        case MIDI::SoloType::Column:
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
        case MIDI::SoloType::Loop:
            value = (Runtime.channelSwitchValue + 1) % NUM_MIDI_CHANNELS;
            break;

        case MIDI::SoloType::TwoWay:
            value = (Runtime.channelSwitchValue + NUM_MIDI_CHANNELS + value) % NUM_MIDI_CHANNELS;
            // we add in NUM_MIDI_CHANNELS so it's always positive
            break;

        case MIDI::SoloType::Channel:
            // if the CC value is 64-127 Solo Parts on the Channel of the CC
            if (value)
            {
                for (int p = 0; p < NUM_MIDI_PARTS; ++p)
                {
                    if ((part[p]->Prcvchn & (NUM_MIDI_CHANNELS - 1)) == chan)
                        part[p]->Prcvchn &= (NUM_MIDI_CHANNELS - 1);
                    else
                        part[p]->Prcvchn = part[p]->Prcvchn | NUM_MIDI_CHANNELS;
                }
            }
            else // if the CC value is 0-63 un-Solo Parts on all Channels
            {
                for (int p = 0; p < NUM_MIDI_PARTS; ++p)
                {
                    if (part[p]->Prcvchn >= NUM_MIDI_CHANNELS)
                        part[p]->Prcvchn &= (NUM_MIDI_CHANNELS - 1);
                }
            }
            Runtime.channelSwitchValue = value;
            return 0; // all ok
            break;
    }
    // vvv column and channel modes never get here vvv
    for (int ch = 0; ch < NUM_MIDI_CHANNELS; ++ch)
    {
        Runtime.channelSwitchValue = value;
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
        RunChannelSwitch(chan, par);
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


    for (int npart = minPart; npart < maxPart; ++ npart)
    {   // Send the controller to all part assigned to the channel

        //std::cout << "  min " << minPart<< "  max " << maxPart << "  Rec " << int(part[npart]->Prcvchn) << "  Chan " << int(chan) <<std::endl;
        if (part[npart]->Prcvchn == chan)
        {
            if (CCtype == part[npart]->PbreathControl) // breath
            {
                part[npart]->SetController(MIDI::CC::volume, 64 + par / 2);
                part[npart]->SetController(MIDI::CC::filterCutoff, par);
            }
            else if (CCtype == MIDI::CC::legato)
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
        if (bank.setCurrentBankID(banknum))
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
                if (root < UNUSED)
                    name += " in root " + to_string(root) + ".";
                else
                    name += " in this root.";
                name += " Current bank is " + asString(ReadBank());
            }
        }
    }

    int msgID = NO_MSG;
    if (notinplace)
        msgID = textMsgBuffer.push(name);
    if (!ok)
        msgID |= 0xFF0000;
    return msgID;
}


int SynthEngine::setProgramByName(CommandBlock *getData)
{
    struct timeval tv1, tv2;
    if (Runtime.showTimes)
        gettimeofday(&tv1, NULL);
    int msgID = NO_MSG;
    bool ok = true;
    int npart = int(getData->data.kit);
    string fname = textMsgBuffer.fetch(getData->data.miscmsg);
    fname = setExtension(fname, EXTEN::yoshInst);
    if (!isRegularFile(fname.c_str()))
        fname = setExtension(fname, EXTEN::zynInst);
    string name = findLeafName(fname);
    if (name < "!")
    {
        name = "Invalid instrument name " + name;
        ok = false;
    }
    if (ok && !isRegularFile(fname.c_str()))
    {
        name = "Can't find " + fname;
        ok = false;
    }
    if (ok)
    {
        ok = setProgram(fname, npart);
        if (ok && part[npart]->Poriginal == UNTITLED)
            part[npart]->Poriginal = "";
        if (!ok)
            name = "File " + name + "unrecognised or corrupted";
    }

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

    msgID = textMsgBuffer.push(name);
    if (!ok)
    {
        msgID |= 0xFF0000;
        partonoffLock(npart, 2); // as it was
    }
    else
    {
        Runtime.sessionSeen[TOPLEVEL::XML::Instrument] = true;
        addHistory(setExtension(fname, EXTEN::zynInst), TOPLEVEL::XML::Instrument);
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
    string name = findLeafName(fname);
    if (name < "!")
    {
        ok = false;
        if (notinplace)
            name = "No instrument at " + to_string(instrument + 1) + " in this bank";
    }
    else
    {
        ok = setProgram(fname, npart);
        if (ok && part[npart]->Poriginal == UNTITLED)
            part[npart]->Poriginal = "";
        if (notinplace)
        {
            if (!ok)
                name = "Instrument " + name + " missing or corrupted";
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
        msgID = textMsgBuffer.push(name);
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


bool SynthEngine::setProgram(const string& fname, int npart)
{
    bool ok = true;
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
            for (int idx = 0; idx < MAX_INSTRUMENTS_IN_BANK; ++ idx)
            {
                if (!bank.emptyslot(root, bankNum, idx))
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
        if (SingleVector(msg_buf, chan))
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

    msg_buf.push_back("  Current part " + asString(Runtime.currentPart + 1));

    msg_buf.push_back("  Current part's channel " + asString((int)part[Runtime.currentPart]->Prcvchn + 1));

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
    bool to_send = false;
    unsigned char  action = 0;
    unsigned char  cmd = UNUSED;
    unsigned char  setpart;
    unsigned char  parameter = UNUSED;

    switch (type)
    {
        case 2: // master key shift
            value -=64;
            if (value > MAX_KEY_SHIFT)
                value = MAX_KEY_SHIFT;
            else if (value < MIN_KEY_SHIFT) // 3 octaves is enough for anybody :)
                value = MIN_KEY_SHIFT;
            cmd = MAIN::control::keyShift;
            setpart = TOPLEVEL::section::main;
            action = TOPLEVEL::action::lowPrio;
            to_send = true;
            break;

        case 7: // master volume
            cmd = MAIN::control::volume;
            setpart = TOPLEVEL::section::main;
            to_send = true;
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
            {
                value -= 64;
                if (value < MIN_KEY_SHIFT)
                        value = MIN_KEY_SHIFT;
                    else if (value > MAX_KEY_SHIFT)
                        value = MAX_KEY_SHIFT;

                CommandBlock putData;
                memset(&putData, 0xff, sizeof(putData));
                putData.data.value = value;
                putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
                putData.data.source = TOPLEVEL::action::fromCLI | TOPLEVEL::action::lowPrio;
                putData.data.control = PART::control::keyShift;

                for (int i = 0; i < Runtime.NumAvailableParts; ++ i)
                {
                    if (partonoffRead(i) && part[i]->Prcvchn == (type - 64))
                    {
                        putData.data.part = i;
                        int tries = 0;
                        bool ok = true;
                        do
                        {
                            ++ tries;
                            ok = interchange.fromMIDI.write(putData.bytes);
                            if (!ok)
                                usleep(1);
                        // we can afford a short delay for buffer to clear
                        }
                        while (!ok && tries < 3);
                        if (!ok)
                        {
                            Runtime.Log("Midi buffer full!");
                            ok = false;
                        }
                    }
                }
            }
            return 0;
            break;

        case 80: // root CC
            if (value > 119)
                value = 128;
            if (value != Runtime.midi_bank_root) // don't mess about if it's the same
            {
                // this is not ideal !!!
                if (value == Runtime.midi_bank_C)
                {
                    parameter = textMsgBuffer.push("in use by bank CC");
                    value = 128;
                }
            }
            cmd = CONFIG::control::bankRootCC;
            setpart = TOPLEVEL::section::config;
            to_send = true;
            break;

        case 81: // bank CC
            if (value != 0 && value != 32)
                value = 128;
            else if (value != Runtime.midi_bank_C) // not already set!
            {
                // nor this !
                if (value == Runtime.midi_bank_root)
                {
                    parameter = textMsgBuffer.push("in use by bank root CC");
                    value = 128;
                }
            }
            cmd = CONFIG::control::bankCC;
            setpart = TOPLEVEL::section::config;
            to_send = true;
            break;

        case 82: // enable program change
            value = (value > 63);
            cmd = CONFIG::control::enableProgramChange;
            setpart = TOPLEVEL::section::config;
            to_send = true;
            break;

        case 83: // enable part on program change
            value = (value > 63);
            cmd = CONFIG::control::instChangeEnablesPart;
            setpart = TOPLEVEL::section::config;
            to_send = true;
            break;

        case 84: // extended program change CC
            if (value > 119)
                value = 128;
            else
            { // this is far from ideal !!!
                string label = Runtime.testCCvalue(value);if (label != "")
                {
                    parameter = textMsgBuffer.push(label);;
                    value = 128;
                }
                cmd = CONFIG::control::extendedProgramChangeCC;
                setpart = TOPLEVEL::section::config;
                to_send = true;
            }
            break;

        case 85: // active parts
            if (value <= 16)
                value = 16;
            else if (value <= 32)
                value = 32;
            else
                value = 64;
            cmd = MAIN::control::availableParts;
            setpart = TOPLEVEL::section::main;
            to_send = true;
            break;

        case 86: // obvious!
            value = 0;
            cmd = CONFIG::control::saveCurrentConfig;
            setpart = TOPLEVEL::section::config;
            action = TOPLEVEL::action::lowPrio;
            to_send = true;
            break;
    }
    if (!to_send)
        return 0;

    /*
     * This is only ever called from the MIDI NRPN thread so is safe.
     * In fact we will probably move it there once all the routines
     * have been converted.
     * We fake a CLI message so that we get reporting and GUI update.
     */

    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));
    putData.data.value = value;
    putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
    putData.data.source = TOPLEVEL::action::fromCLI | action;
    putData.data.control = cmd;
    putData.data.part = setpart;
    putData.data.parameter = parameter;

    int tries = 0;
    bool ok = true;
    do
    {
        ++ tries;
        ok = interchange.fromMIDI.write(putData.bytes);
        if (!ok)
            usleep(1);
    // we can afford a short delay for buffer to clear
    }
    while (!ok && tries < 3);
    if (!ok)
    {
        Runtime.Log("Midi buffer full!");
        ok = false;
    }
    return 0;
}


int SynthEngine::LoadNumbered(unsigned char group, unsigned char entry)
{
    string filename;
    vector<string> &listType = *getHistory(group);
    if (size_t(entry) >= listType.size())
        return (textMsgBuffer.push(" FAILED: List entry " + to_string(int(entry)) + " out of range") | 0xFF0000);
    filename = listType.at(entry);
    return textMsgBuffer.push(filename);
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
            part = chan | NUM_MIDI_CHANNELS;
            break;

        case 6:
            part = chan | (NUM_MIDI_CHANNELS * 2);
            break;

        case 7:
            part = chan | (NUM_MIDI_CHANNELS * 3);
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
        putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
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
    interchange.syncWrite = false;
    interchange.lowPrioWrite = false;
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++ npart)
        part[npart]->busy = false;
    defaults();
    ClearNRPNs();
    if (Runtime.loadDefaultState)
    {
        string filename = Runtime.defaultStateName + ("-" + to_string(this->getUniqueId()));
        if (isRegularFile(filename + ".state"))
        {
            Runtime.StateFile = filename;
            Runtime.restoreSessionData(Runtime.StateFile);
        }
    }
    if (andML)
    {
        CommandBlock putData;
        memset(&putData, 0xff, sizeof(putData));
        putData.data.value = 0;
        putData.data.type = 0;
        putData.data.control = MIDILEARN::control::clearAll;
        putData.data.part = TOPLEVEL::section::midiLearn;
        midilearn.generalOperations(&putData);
    }
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
    if (original > 1)
        original = 1;
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
            if (tmp < 1) // nearer to on
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
    else if (tmp < 1 && original == 1) // disable if it wasn't already off
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


// Master audio out (the final sound)
int SynthEngine::MasterAudio(float *outl [NUM_MIDI_PARTS + 1], float *outr [NUM_MIDI_PARTS + 1], int to_process)
{
    //if (to_process < 64)
        //Runtime.Log("Process " + to_string(to_process));
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
    }

    memset(mainL, 0, sent_bufferbytes);
    memset(mainR, 0, sent_bufferbytes);

    unsigned char sound = audioOut.load();
    switch (sound)
    {
        case _SYS_::mute::Pending:
            // set by resolver
            fadeLevel = 1.0f;
            audioOut.store(_SYS_::mute::Fading);
            sound = _SYS_::mute::Fading;
            //std::cout << "here fading" << std:: endl;
            break;
        case _SYS_::mute::Fading:
            if (fadeLevel < 0.001f)
            {
                audioOut.store(_SYS_::mute::Active);
                sound = _SYS_::mute::Active;
                fadeLevel = 0;
            }
            break;
        case _SYS_::mute::Active:
            // cleared by resolver
            break;
        case _SYS_::mute::Complete:
            // set by resolver and paste
            audioOut.store(_SYS_::mute::Idle);
            //std::cout << "here complete" << std:: endl;
            break;
        case _SYS_::mute::Request:
            // set by paste routine
            audioOut.store(_SYS_::mute::Immediate);
            sound = _SYS_::mute::Active;
            //std::cout << "here requesting" << std:: endl;
            break;
        case _SYS_::mute::Immediate:
            // cleared by paste routine
            sound = _SYS_::mute::Active;
            break;
        default:
            break;
    }


    interchange.mediate();
    char partLocal[NUM_MIDI_PARTS];
    /*
     * This isolates the loop from part changes so that when a low
     * prio thread completes and re-enables the part, it will not
     * actually be seen until the start of the next period.
     */
    for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
            partLocal[npart] = partonoffRead(npart);

    if (sound == _SYS_::mute::Active)
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
        unsigned char panLaw = Runtime.panLaw;
        for (int npart = 0; npart < Runtime.NumAvailableParts; ++npart)
        {
            if (!partLocal[npart])
                continue;

            float Step = ControlStep;
            for (int i = 0; i < sent_buffersize; ++i)
            {
                if (part[npart]->Ppanning - part[npart]->TransPanning > Step)
                    part[npart]->checkPanning(Step, panLaw);
                else if (part[npart]->TransPanning - part[npart]->Ppanning > Step)
                    part[npart]->checkPanning(-Step, panLaw);
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
            if (!syseffEnable[nefx])
                continue; // is off

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
                if (!syseffEnable[nefxfrom])
                    continue; // is off
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
            if (sound == _SYS_::mute::Fading) // fadeLevel must also have been set
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

        LFOtime += sent_buffersize; // update the LFO's time
    }
    return sent_buffersize;
}


void SynthEngine::fetchMeterData()
{ // overload protection below shouldn't be needed :(
    if (!VUready)
        return;
    if (meterDelay > 0)
    {
        --meterDelay;
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
        VUpeak.values.parts[npart] = -1.0f;
        VUpeak.values.partsR[npart] = -1.0f;
    }
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx]->cleanup();
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx]->cleanup();
}


bool SynthEngine::loadStateAndUpdate(const string& filename)
{
    defaults();
    Runtime.sessionStage = _SYS_::type::InProgram;
    Runtime.stateChanged = true;
    bool result = Runtime.restoreSessionData(filename);
    ShutUp();
    return result;
}


bool SynthEngine::saveState(const string& filename)
{
    return Runtime.saveSessionData(filename);
}


bool SynthEngine::loadPatchSetAndUpdate(string fname)
{
    bool result;
    fname = setExtension(fname, EXTEN::patchset);
    result = loadXML(fname); // load the data
    if (result)
        setAllPartMaps();
    return result;
}


bool SynthEngine::loadMicrotonal(const string& fname)
{
    return microtonal.loadXML(setExtension(fname, EXTEN::scale));
}

bool SynthEngine::saveMicrotonal(const string& fname)
{
    return microtonal.saveXML(setExtension(fname, EXTEN::scale));
}

bool SynthEngine::installBanks()
{
    string name = Runtime.ConfigDir + '/' + YOSHIMI;
    string bankname = name + ".banks";
    bool banksGood = false;
    bool newBanks = false;
    if (isRegularFile(bankname))
    {
        XMLwrapper *xml = new XMLwrapper(this);
        if (xml)
        {
            banksGood = true;
            xml->loadXMLfile(bankname);
            newBanks = bank.parseBanksFile(xml);
            delete xml;
        }
    }
    if (!banksGood){
       newBanks = bank.parseBanksFile(NULL);
       Runtime.currentRoot = 5;
    }

    Runtime.Log("\nFound " + asString(bank.InstrumentsInBanks) + " instruments in " + asString(bank.BanksInRoots) + " banks");
    if (newBanks)
        Runtime.Log(textMsgBuffer.fetch(setRootBank(5, 5) & 0xff));
    else
        Runtime.Log(textMsgBuffer.fetch(setRootBank(Runtime.tempRoot, Runtime.tempBank) & 0xff));
    return true;
}


bool SynthEngine::saveBanks()
{
    string name = Runtime.ConfigDir + '/' + YOSHIMI;
    string bankname = name + ".banks";
    Runtime.xmlType = TOPLEVEL::XML::Bank;

    XMLwrapper *xml = new XMLwrapper(this, true);
    if (!xml)
    {
        Runtime.Log("saveBanks failed xml allocation");
        return false;
    }
    xml->beginbranch("BANKLIST");
    bank.saveToConfigFile(xml);
    xml->endbranch();

    if (!xml->saveXMLfile(bankname))
        Runtime.Log("Failed to save config to " + bankname);

    delete xml;

    return true;
}


void SynthEngine::newHistory(string name, int group)
{
    if (findLeafName(name) < "!")
        return;
    if (group == TOPLEVEL::XML::Instrument && (name.rfind(EXTEN::yoshInst) != string::npos))
        name = setExtension(name, EXTEN::zynInst);
    vector<string> &listType = *getHistory(group);
    listType.push_back(name);
}


void SynthEngine::addHistory(const string& name, int group)
{
    //std::cout << "history name " << name << "  group " << group << std::endl;
    if (Runtime.historyLock[group])
    {
        //std::cout << "history locked" << std::endl;
        return;
    }
    if (findLeafName(name) < "!")
    {
        //std::cout << "failed leafname" << std::endl;
        return;
    }

    vector<string> &listType = *getHistory(group);
    vector<string>::iterator itn = listType.begin();
    listType.erase(std::remove(itn, listType.end(), name), listType.end()); // remove all matches
    listType.insert(listType.begin(), name);
    setLastfileAdded(group, name);
}


vector<string> * SynthEngine::getHistory(int group)
{
    //std::cout << "group " << group << std::endl;
    switch(group)
    {
        case TOPLEVEL::XML::Instrument: // 0
            return &InstrumentHistory;
            break;
        case TOPLEVEL::XML::Patch: // 1
            return &ParamsHistory;
            break;
        case TOPLEVEL::XML::Scale: // 2
            return &ScaleHistory;
            break;
        case TOPLEVEL::XML::State: // 3
            return &StateHistory;
            break;
        case TOPLEVEL::XML::Vector: // 4
            return &VectorHistory;
            break;
        case TOPLEVEL::XML::MLearn: // 5
            return &MidiLearnHistory;
            break;
        case TOPLEVEL::XML::Presets: // 6
            return &PresetHistory;
            break;

        case TOPLEVEL::XML::PadSample: // 7
            return &PadHistory;
            break;
        case TOPLEVEL::XML::ScalaTune: // 8
            return &TuningHistory;
            break;
        case TOPLEVEL::XML::ScalaMap: // 9
            return &KeymapHistory;
            break;
        default:
            // can't identify what is calling this.
            // It's connected with opening the filer on presets
            Runtime.Log("Unrecognised group " + to_string(group) + "\nUsing patchset history");
            return &ParamsHistory;
    }
}


void SynthEngine::setHistoryLock(int group, bool status)
{
    Runtime.historyLock[group] = status;
}


bool SynthEngine::getHistoryLock(int group)
{
    return Runtime.historyLock[group];
}


string SynthEngine::lastItemSeen(int group)
{
    if (group == TOPLEVEL::XML::Instrument && Runtime.sessionSeen[group] == false)
        return "";

    vector<string> &listType = *getHistory(group);
    if (listType.empty())
        return "";
    return *listType.begin();
}


void SynthEngine::setLastfileAdded(int group, string name)
{
    if (name == "")
        name = Runtime.userHome;
    list<string>::iterator it = Runtime.lastfileseen.begin();
    int count = 0;
    while (count < group && it != Runtime.lastfileseen.end())
    {
        ++it;
        ++count;
    }
    if (it != Runtime.lastfileseen.end())
        *it = name;
}


string SynthEngine::getLastfileAdded(int group)
{
    list<string>::iterator it = Runtime.lastfileseen.begin();
    int count = 0;
    while (count < group && it != Runtime.lastfileseen.end())
    {
        ++it;
        ++count;
    }
    if (it == Runtime.lastfileseen.end())
        return "";
    return *it;
}


bool SynthEngine::loadHistory()
{
    string historyname = Runtime.localDir  + "/recent";
    if (!isRegularFile(historyname))
    {   // recover old version
        historyname = Runtime.ConfigDir + '/' + string(YOSHIMI) + ".history";
        if (!isRegularFile(historyname))
        {
            Runtime.Log("Missing recent history file");
            return false;
        }
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
    int count;
    string filetype;
    string type;
    string extension;
    for (count = TOPLEVEL::XML::Instrument; count <= TOPLEVEL::XML::ScalaMap; ++count)
    {
        switch (count)
        {
            case TOPLEVEL::XML::Instrument:
                type = "XMZ_INSTRUMENTS";
                extension = "xiz_file";
                break;
            case TOPLEVEL::XML::Patch:
                type = "XMZ_PATCH_SETS";
                extension = "xmz_file";
                break;
            case TOPLEVEL::XML::Scale:
                type = "XMZ_SCALE";
                extension = "xsz_file";
                break;
            case TOPLEVEL::XML::State:
                type = "XMZ_STATE";
                extension = "state_file";
                break;
            case TOPLEVEL::XML::Vector:
                type = "XMZ_VECTOR";
                extension = "xvy_file";
                break;
            case TOPLEVEL::XML::MLearn:
                type = "XMZ_MIDILEARN";
                extension = "xly_file";
                break;
            case TOPLEVEL::XML::Presets:
                type = "XMZ_PRESETS";
                extension = "xpz_file";
                break;

            case TOPLEVEL::XML::PadSample:
                type = "XMZ_PADSAMPLE";
                extension = "wav_file";
                break;
            case TOPLEVEL::XML::ScalaTune:
                type = "XMZ_TUNING";
                extension = "scl_file";
                break;
            case TOPLEVEL::XML::ScalaMap:
                type = "XMZ_KEYMAP";
                extension = "kbm_file";
                break;
        }
        if (xml->enterbranch(type))
        { // should never exceed max history as size trimmed on save
            Runtime.historyLock[count] = xml->getparbool("lock_status", false);
            hist_size = xml->getpar("history_size", 0, 0, MAX_HISTORY);
            for (int i = 0; i < hist_size; ++i)
            {
                if (xml->enterbranch("XMZ_FILE", i))
                {
                    filetype = xml->getparstr(extension);
                    if (extension == "xiz_file" && !isRegularFile(filetype))
                    {
                        if (filetype.rfind(EXTEN::zynInst) != string::npos)
                            filetype = setExtension(filetype, EXTEN::yoshInst);
                    }
                    if (filetype.size() && isRegularFile(filetype))
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
    string historyname = Runtime.localDir  + "/recent";
    Runtime.xmlType = TOPLEVEL::XML::History;

    XMLwrapper *xml = new XMLwrapper(this, true);
    if (!xml)
    {
        Runtime.Log("saveHistory failed xml allocation");
        return false;
    }
    xml->beginbranch("HISTORY");
    {
        int count;
        string type;
        string extension;
        for (count = TOPLEVEL::XML::Instrument; count <= TOPLEVEL::XML::ScalaMap; ++count)
        {
            switch (count)
            {
                case TOPLEVEL::XML::Instrument:
                    type = "XMZ_INSTRUMENTS";
                    extension = "xiz_file";
                    break;
                case TOPLEVEL::XML::Patch:
                    type = "XMZ_PATCH_SETS";
                    extension = "xmz_file";
                    break;
                case TOPLEVEL::XML::Scale:
                    type = "XMZ_SCALE";
                    extension = "xsz_file";
                    break;
                case TOPLEVEL::XML::State:
                    type = "XMZ_STATE";
                    extension = "state_file";
                    break;
                case TOPLEVEL::XML::Vector:
                    type = "XMZ_VECTOR";
                    extension = "xvy_file";
                    break;
                case TOPLEVEL::XML::MLearn:
                    type = "XMZ_MIDILEARN";
                    extension = "xly_file";
                    break;
                case TOPLEVEL::XML::Presets:
                    type = "XMZ_PRESETS";
                    extension = "xpz_file";
                    break;

                case TOPLEVEL::XML::PadSample:
                type = "XMZ_PADSAMPLE";
                extension = "wav_file";
                break;
                case TOPLEVEL::XML::ScalaTune:
                    type = "XMZ_TUNING";
                    extension = "scl_file";
                    break;
                case TOPLEVEL::XML::ScalaMap:
                    type = "XMZ_KEYMAP";
                    extension = "kbm_file";
                    break;
            }
            vector<string> listType = *getHistory(count);
            if (listType.size())
            {
                unsigned int offset = 0;
                int x = 0;
                xml->beginbranch(type);
                    xml->addparbool("lock_status", Runtime.historyLock[count]);
                    xml->addpar("history_size", listType.size());
                    if (listType.size() > MAX_HISTORY)
                        offset = listType.size() - MAX_HISTORY;
                    for (vector<string>::iterator it = listType.begin(); it != listType.end() - offset; ++it)
                    {
                        xml->beginbranch("XMZ_FILE", x);
                            xml->addparstr(extension, *it);
                        xml->endbranch();
                        ++x;
                    }
                xml->endbranch();
            }
        }
    }
    xml->endbranch();
    if (!xml->saveXMLfile(historyname))
        Runtime.Log("Failed to save data to " + historyname);
    delete xml;
    return true;
}


unsigned char SynthEngine::loadVectorAndUpdate(unsigned char baseChan, const string& name)
{
    unsigned char result = loadVector(baseChan, name, true);
    ShutUp();
    return result;
}


unsigned char SynthEngine::loadVector(unsigned char baseChan, const string& name, bool full)
{
    bool a = full; full = a; // suppress warning
    unsigned char actualBase = NO_MSG; // error!
    if (name.empty())
    {
        Runtime.Log("No filename", 2);
        return actualBase;
    }
    string file = setExtension(name, EXTEN::vector);
    make_legit_pathname(file);
    if (!isRegularFile(file))
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
    }
    else
    {
        actualBase = extractVectorData(baseChan, xml, findLeafName(name));
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


unsigned char SynthEngine::extractVectorData(unsigned char baseChan, XMLwrapper *xml, const string& name)
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


unsigned char SynthEngine::saveVector(unsigned char baseChan, const string& name, bool full)
{
    bool a = full; full = a; // suppress warning
    unsigned char result = NO_MSG; // ok

    if (baseChan >= NUM_MIDI_CHANNELS)
        return textMsgBuffer.push("Invalid channel number");
    if (name.empty())
        return textMsgBuffer.push("No filename");
    if (Runtime.vectordata.Enabled[baseChan] == false)
        return textMsgBuffer.push("No vector data on this channel");

    string file = setExtension(name, EXTEN::vector);
    make_legit_pathname(file);

    Runtime.xmlType = TOPLEVEL::XML::Vector;
    XMLwrapper *xml = new XMLwrapper(this, true);
    if (!xml)
    {
        Runtime.Log("Save Vector failed xml allocation", 2);
        return textMsgBuffer.push("FAIL");
    }
    xml->beginbranch("VECTOR");
        insertVectorData(baseChan, true, xml, findLeafName(file));
    xml->endbranch();

    if (!xml->saveXMLfile(file))
    {
        Runtime.Log("Failed to save data to " + file, 2);
        result = textMsgBuffer.push("FAIL");
    }
    delete xml;
    return result;
}


bool SynthEngine::insertVectorData(unsigned char baseChan, bool full, XMLwrapper *xml, const string& name)
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
    xml->addpar("panning_law", Runtime.panLaw);
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
    bool oldFormat = usingYoshiType;
    usingYoshiType = true; // make sure everything is saved
    XMLwrapper *xml = new XMLwrapper(this, true);
    add2XML(xml);
    midilearn.insertMidiListData(xml);
    *data = xml->getXMLdata();
    delete xml;
    usingYoshiType = oldFormat;
    return strlen(*data) + 1;
}


void SynthEngine::putalldata(const char *data, int size)
{
    while (isspace(*data))
        ++data;
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
    bool oldFormat = usingYoshiType;
    usingYoshiType = true; // make sure everything is saved
    filename = setExtension(filename, EXTEN::patchset);
    Runtime.xmlType = TOPLEVEL::XML::Patch;
    XMLwrapper *xml = new XMLwrapper(this, true);
    add2XML(xml);
    bool result = xml->saveXMLfile(filename);
    delete xml;
    usingYoshiType = oldFormat;
    return result;
}


bool SynthEngine::loadXML(const string& filename)
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
    Runtime.panLaw = xml->getpar("panning_law", Runtime.panLaw, MAIN::panningType::cut, MAIN::panningType::boost);
    setPvolume(xml->getpar127("volume", Pvolume));
    setPkeyshift(xml->getpar("key_shift", Pkeyshift, MIN_KEY_SHIFT + 64, MAX_KEY_SHIFT + 64));
    Runtime.channelSwitchType = xml->getpar("channel_switch_type", Runtime.channelSwitchType, 0, 5);
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


string SynthEngine::makeUniqueName(const string& name)
{
    string result = "Yoshimi";
    if (uniqueId > 0)
        result += ("-" + asString(uniqueId));
    result += " : " + name;
    return result;
}


void SynthEngine::setWindowTitle(const string& _windowTitle)
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
        case MAIN::control::panLawType:
            min = MAIN::panningType::cut;
            def = MAIN::panningType::normal;
            max = MAIN::panningType::boost;
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
            max = 5;
            break;

        case MAIN::control::soloCC:
            min = 14;
            def = 115;
            max = 119;
            break;
        case MAIN::control::defaultPart:
            def = 0;
            max = Runtime.NumAvailableParts -1;
            break;
        case MAIN::control::masterReset:
        case MAIN::control::masterResetAndMlearn:
        case MAIN::control::stopSound:
            def = 0;
            max = 0;
            break;

        case MAIN::control::loadInstrumentFromBank:
            return value; // this is just a workround :(
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
        case CONFIG::control::enableHighlight:
            break;

        case CONFIG::control::jackMidiSource:
            min = 3; // anything greater than max
            def = textMsgBuffer.push("default");
            break;
        case CONFIG::control::jackPreferredMidi:
            def = 1;
            break;
        case CONFIG::control::jackServer:
            min = 3;
            def = textMsgBuffer.push("default");
            break;
        case CONFIG::control::jackPreferredAudio:
            def = 1;
            break;
        case CONFIG::control::jackAutoConnectAudio:
            def = 1;
            break;

        case CONFIG::control::alsaMidiSource:
            min = 3;
            def = textMsgBuffer.push("default");
            break;
        case CONFIG::control::alsaPreferredMidi:
            def = 1;
            break;
        case CONFIG::control::alsaAudioDevice:
            min = 3;
            def = textMsgBuffer.push("default");
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
