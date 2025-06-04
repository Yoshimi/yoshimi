/*
    SynthEngine.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2014-2020, Will Godfrey & others

    Copyright 2022-2023, Will Godfrey, Rainer Hans Liffers
    Copyright 2024-2025, Will Godfrey, Ichthyostega, Kristian Amlie & others

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
    Street, Fifth Floor, Boston, MA 02110-1301, USA.

    This file is derivative of original ZynAddSubFX code.

*/

#include <cassert>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <set>

#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif

#include "Misc/Alloc.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "Params/Controller.h"
#include "Misc/Part.h"
#include "Effects/EffectMgr.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/NumericFuncs.h"
#include "Misc/FormatFuncs.h"
#include "Misc/XMLStore.h"
#include "Synth/OscilGen.h"
#include "Params/ADnoteParameters.h"
#include "Params/PADnoteParameters.h"
#include "Interface/InterfaceAnchor.h"

using file::isRegularFile;
using file::setExtension;
using file::findLeafName;
using file::createEmptyFile;
using file::deleteFile;
using file::make_legit_filename;

using func::decibel;
using func::bitTest;
using func::asString;
using func::string2int;

using std::this_thread::sleep_for;
using std::chrono_literals::operator ""us;
using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::time_point;

using std::to_string;
using std::ofstream;
using std::ios_base;
using std::string;
using std::set;




namespace { // Global implementation internal history data
    static vector<string> InstrumentHistory;
    static vector<string> ParamsHistory;
    static vector<string> ScaleHistory;
    static vector<string> StateHistory;
    static vector<string> VectorHistory;
    static vector<string> MidiLearnHistory;
    static vector<string> PresetHistory;
    static vector<string> PadHistory;
    static vector<string> TuningHistory;
    static vector<string> KeymapHistory;

    static vector<string> historyLastSeen(TOPLEVEL::XML::ScalaMap + 1, ""); // don't really understand this :(
}



SynthEngine::SynthEngine(uint instanceID)
    : uniqueId{instanceID}
    , Runtime{*this}
    , bank{*this}
    , interchange{*this}
    , midilearn{*this}
    , mididecode{this}
    , vectorcontrol{this}
    , audioOut{}
    , partlock{}
    , legatoPart{0}
    , masterMono{false}
    // part[]
    , fadeAll{0}
    , fadeStep{0}
    , fadeStepShort{0}
    , fadeLevel{0}
    , samplerate{48000}
    , samplerate_f{float(samplerate)}
    , halfsamplerate_f{float(samplerate / 2)}
    , buffersize{512}
    , buffersize_f{float(buffersize)}
    , bufferbytes{int(buffersize*sizeof(float))}
    , oscilsize{1024}
    , oscilsize_f{float(oscilsize)}
    , halfoscilsize{oscilsize / 2}
    , halfoscilsize_f{float(halfoscilsize)}
    , oscil_sample_step_f{1.0}
    , oscil_norm_factor_pm{1.0}
    , oscil_norm_factor_fm{1.0}
    , sent_buffersize{0}
    , sent_bufferbytes{0}
    , sent_buffersize_f{0}
    , fixed_sample_step_f{0}
    , TransVolume{0}
    , Pvolume{0}
    , ControlStep{0}
    , Paudiodest{0}
    , Pkeyshift{0}
    , PbpmFallback{0}
    // Psysefxvol[][]
    // Psysefxsend[][]
    , syseffnum{0}
    // syseffEnable[]
    , inseffnum{0}
    // sysefx[]
    // insefx[]
    // Pinsparts[]
    , sysEffectUiCon{interchange.guiDataExchange.createConnection<EffectDTO>()}
    , insEffectUiCon{interchange.guiDataExchange.createConnection<EffectDTO>()}
    , partEffectUiCon{interchange.guiDataExchange.createConnection<EffectDTO>()}
    , sysEqGraphUiCon{interchange.guiDataExchange.createConnection<EqGraphDTO>()}
    , insEqGraphUiCon{interchange.guiDataExchange.createConnection<EqGraphDTO>()}
    , partEqGraphUiCon{interchange.guiDataExchange.createConnection<EqGraphDTO>()}
    , ctl{NULL}
    , microtonal{this}
    , fft{}
    , textMsgBuffer{TextMsgBuffer::instance()}
    , VUpeak{}
    , VUcopy{}
    , VUdata{}
    , VUcount{0}
    , VUready{false}
    , volume{0.0}
    // sysefxvol[][]
    // sysefxsend[][]
    , keyshift{0}
    , callbackGuiClosed{}
    , windowTitle{"Yoshimi" + asString(uniqueId)}
    , needsSaving{false}
    , channelTimer{0}
    , LFOtime{0}
    , songBeat{0.0}
    , monotonicBeat{0.0}
    , bpm{90}
    , bpmAccurate{false}
{
    union {
        uint32_t u32 = 0x11223344;
        uint8_t arr[4];
    } x;
    Runtime.isLittleEndian = (x.arr[0] == 0x44);
    ctl = new Controller(this);
    for (int i = 0; i < NUM_MIDI_CHANNELS; ++ i)
        Runtime.vectordata.Name[i] = "No Name " + std::to_string(i + 1);
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
    shutdownGui();
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

    sem_destroy(&partlock);
    if (ctl)
        delete ctl;
}


bool SynthEngine::Init(uint audiosrate, int audiobufsize)
{
    Runtime.init();
    audioOutStore(_SYS_::mute::Active);
    samplerate_f = samplerate = audiosrate;
    halfsamplerate_f = samplerate_f / 2;

    buffersize = Runtime.buffersize;
    if (buffersize > audiobufsize)
        buffersize = audiobufsize;
    buffersize_f = buffersize;
    fixed_sample_step_f = buffersize_f / samplerate_f;
    bufferbytes = buffersize * sizeof(float);

    oscilsize_f = oscilsize = Runtime.oscilsize;
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

    fft.reset(new fft::Calc(oscilsize));

    sem_init(&partlock, 0, 1);

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart] = new Part(npart, &microtonal, *fft, *this);
        if (!part[npart])
        {
            Runtime.Log("Failed to allocate new Part");
            goto bail_out;
        }
    }

    // Insertion Effects init
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        if (!(insefx[nefx] = new EffectMgr(1, *this)))
        {
            Runtime.Log("Failed to allocate new Insertion EffectMgr");
            goto bail_out;
        }
    }

    // System Effects init
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        if (!(sysefx[nefx] = new EffectMgr(0, *this)))
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
    Runtime.genTmp1.reset(buffersize);
    Runtime.genTmp2.reset(buffersize);
    Runtime.genTmp3.reset(buffersize);
    Runtime.genTmp4.reset(buffersize);

    // similar to above but for parts
    Runtime.genMixl.reset(buffersize);
    Runtime.genMixr.reset(buffersize);

    defaults();
    ClearNRPNs();

    if (Runtime.sessionStage == _SYS_::type::Default || Runtime.sessionStage == _SYS_::type::StartupSecond || Runtime.sessionStage == _SYS_::type::JackSecond)
        Runtime.restoreSessionData(Runtime.stateFile);
    if (Runtime.paramsLoad.size())
    {
        string filename = setExtension(Runtime.paramsLoad, EXTEN::patchset);
        ShutUp();
        if (!loadXML(filename))
        {
            Runtime.Log("Failed to load parameters " + filename);
            Runtime.paramsLoad = "";
        }
    }
    if (Runtime.instrumentLoad.size())
    {
        string filename = Runtime.instrumentLoad;
        if (part[Runtime.load2part]->loadXML(filename))
        {
            part[Runtime.load2part]->Penabled = 1;
            Runtime.Log("Instrument file " + filename + " loaded");
        }
        else
        {
            Runtime.Log("Failed to load instrument file " + filename);
            Runtime.instrumentLoad = "";
        }
    }
    if (Runtime.midiLearnLoad.size())
    {
        string filename = Runtime.midiLearnLoad;
        if (midilearn.loadList(filename))
        {
#ifdef GUI_FLTK
            midilearn.updateGui(); // does nothing if --no-gui
#endif
            Runtime.Log("midiLearn file " + filename + " loaded");
        }
        else
        {
            Runtime.Log("Failed to load midiLearn file " + filename);
            Runtime.midiLearnLoad = "";
        }
    }
    /*
     * put here so its threads don't run until everything else is ready
     */
    if (!interchange.Init())
    {
        Runtime.Log("[ERROR] interChange init failed", _SYS_::LogError);
        goto bail_out;
    }

    // we seem to need this here only for first time startup :(
    bank.setCurrentBankID(Runtime.tempBank, false);
    return true;


bail_out:
    fft.reset();

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


/**
 * Prepare and wire a communication anchor, allowing the GUI to establish
 * data connections with this SynthEngine. This InstanceAnchor record is
 * pushed through the GuiDataExchange (maintained within InterChange),
 * and the corresponding notification is placed into the toGUI ringbuffer,
 * where it typically is the very first message, since this function is
 * invoked from SynthEngine::Init().
 */
InterfaceAnchor SynthEngine::buildGuiAnchor()
{
    InterfaceAnchor anchorRecord;
    anchorRecord.synth = this;
    anchorRecord.synthID = uniqueId;

    ///////////////////TODO 1/2024 : connect all routing-Tags used by embedded sub-components of the Synth
    anchorRecord.sysEffectParam  = sysEffectUiCon;
    anchorRecord.sysEffectEQ     = sysEqGraphUiCon;
    anchorRecord.insEffectParam  = insEffectUiCon;
    anchorRecord.insEffectEQ     = insEqGraphUiCon;
    anchorRecord.partEffectParam = partEffectUiCon;
    anchorRecord.partEffectEQ    = partEqGraphUiCon;

    return anchorRecord;
}


/**
 * This callback is triggered whenever a new SynthEngine instance becomes fully operational.
 * If running with GUI, the GuiMaster has been created and communication via GuiDataExchange
 * has been primed. The LV2-plugin calls this later when the GUI is opened, with `isFirstInit==false`
 */
void SynthEngine::postBootHook(bool isFirstInit)
{
    if (isFirstInit)
    { /* nothing special for first init to do currently */ }
    maybePublishEffectsToGui();
    // more initial push-updates will be added here...
    //
}// note InterChange::commandMain() will also push into GUI: (control=control::dataExchange, part=section::main)


#ifdef GUI_FLTK
MasterUI* SynthEngine::getGuiMaster()
{
    return interchange.guiMaster.get();
}

void SynthEngine::shutdownGui()
{
    interchange.shutdownGui();
}
#endif /*GUI_FLTK*/

void SynthEngine::signalGuiWindowClosed()
{
    if (not Runtime.isLV2)
        Runtime.runSynth.store(false, std::memory_order_release);
    if (callbackGuiClosed)
        callbackGuiClosed(); // if defined, invoke it
}


string SynthEngine::manualname()
{
    string manfile = "yoshimi-user-manual-";
    manfile += YOSHIMI_VERSION;
    manfile = manfile.substr(0, manfile.find(" ")); // remove M or rc suffix
    int pos = 0;
    int count = 0;
    for (uint i = 0; i < manfile.length(); ++i)
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


void SynthEngine::defaults()
{
    for (int i = 0; i <NUM_MIDI_PARTS; ++i)
        partonoffLock(i, 0); // ensure parts are disabled
    setPvolume(90);
    TransVolume = Pvolume - 1; // ensure it is always set
    setPkeyshift(64);
    PbpmFallback = 120;

    VUpeak.values.vuOutPeakL = 0;
    VUpeak.values.vuOutPeakR = 0;
    VUpeak.values.vuRmsPeakL = 0;
    VUpeak.values.vuRmsPeakR = 0;

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart]->defaults(npart);

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
    // avoid direct GUI push-update from here, since it's already covered in
    // getfromXML() and resetAll() -- which happens to cover all relevant cases
    // see SynthEngine::maybePublishEffectsToGui()

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
    Runtime.numAvailableParts = NUM_MIDI_CHANNELS;
    Runtime.panLaw = MAIN::panningType::normal;
    ShutUp();
    Runtime.lastfileseen.clear();
    for (int i = 0; i <= TOPLEVEL::XML::ScalaMap; ++i)
    {
        Runtime.lastfileseen.push_back(file::userHome());
        Runtime.sessionSeen[i] = false;
    }

#ifdef REPORT_NOTES_ON_OFF
    Runtime.noteOnSent = 0; // note test
    Runtime.noteOnSeen = 0;
    Runtime.noteOffSent = 0;
    Runtime.noteOffSeen = 0;
#endif

    partonoffLock(0, 1); // enable the first part
}


void SynthEngine::setPartMap(int npart)
{
    part[npart]->setNoteMap(part[npart]->Pkeyshift - 64);
}


void SynthEngine::setAllPartMaps()
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++ npart)
        setPartMap(npart);
}

void SynthEngine::audioOutStore(uint8_t num)
{
    audioOut.store(num);
    interchange.spinSortResultsThread();
}


/* for automated testing: brings all existing pseudo random generators
 * within this SyntEngine into a reproducible state, based on given seed;
 * also resets long lived procedural state and rebuilds PAD wavetables */
void SynthEngine::setReproducibleState(int seed)
{
    ShutUp();
    LFOtime = 0;
    monotonicBeat = songBeat = 0.0f;
    prng.init(seed);
    for (int p = 0; p < NUM_MIDI_PARTS; ++p)
        if (part[p] and part[p]->Penabled)
            for (int i = 0; i < NUM_KIT_ITEMS; ++i)
            {
                Part::KitItem& kitItem = part[p]->kit[i];
                if (!kitItem.Penabled) continue; // reseed only enabled items
                if (kitItem.adpars and kitItem.Padenabled)
                    for (int v = 0; v < NUM_VOICES; ++v)
                    {
                        if (not kitItem.adpars->VoicePar[v].Enabled) continue;
                        kitItem.adpars->VoicePar[v].OscilSmp->reseed(randomINT());
                        kitItem.adpars->VoicePar[v].FMSmp->reseed(randomINT());
                    }
                if (kitItem.padpars and kitItem.Ppadenabled)
                    {
                        kitItem.padpars->reseed(randomINT());
                        kitItem.padpars->oscilgen->forceUpdate(); // rebuild Spectrum
                        // synchronously rebuild PADSynth wavetable with new randseed
                        kitItem.padpars->buildNewWavetable(true);
                        kitItem.padpars->activate_wavetable();
                    }
            }
    Runtime.Log("SynthEngine("+to_string(uniqueId)+"): reseeded with "+to_string(seed));
}


namespace {
    // helper to support automated testing of PADSynth wavetable swap
    inline PADnoteParameters* findFirstPADSynth(Part *part[NUM_MIDI_PARTS])
    {
        for (int p = 0; p < NUM_MIDI_PARTS; ++p)
            if (part[p] and part[p]->Penabled)
                for (int i = 0; i < NUM_KIT_ITEMS; ++i)
                {
                    Part::KitItem& kitItem = part[p]->kit[i];
                    if (kitItem.padpars and kitItem.Ppadenabled)
                        return kitItem.padpars;
                }
        return nullptr;
    }
}

/* for automated testing: stash aside the wavetable of one PADSynth and possibly swap in another.
 * Works together with the CLI command test/swapWave. See TestInvoker::swapPadTable() */
void SynthEngine::swapTestPADtable()
{
    static unique_ptr<PADTables> testWavetable{nullptr};
    // find the first enabled PADSynth to work on
    auto padSynth = findFirstPADSynth(part);
    if (not padSynth) return;

    if (not testWavetable) // init with empty (muted) wavetable
        testWavetable.reset(new PADTables{padSynth->Pquality});

    using std::swap;
    swap(padSynth->waveTable, *testWavetable);
    padSynth->paramsChanged();
    if (padSynth->PxFadeUpdate)
    {// rig a cross-fade for ongoing notes to pick up
        PADTables copy4fade{padSynth->Pquality};
        copy4fade.cloneDataFrom(*testWavetable);
        padSynth->xFade.startXFade(copy4fade);
    }
}



// Note On Messages
void SynthEngine::NoteOn(uchar chan, uchar note, uchar velocity)
{
#ifdef REPORT_NOTES_ON_OFF
    ++Runtime.noteOnSeen; // note test
    if (Runtime.noteOnSeen != Runtime.noteOnSent)
        Runtime.Log("Note on diff " + to_string(Runtime.noteOnSent - Runtime.noteOnSeen));
#endif

#ifdef REPORT_NOTE_ON_TIME
    steady_clock::time_point noteTime;
    noteTime = steady_clock::now();
#endif
    for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
    {
        if (chan == part[npart]->Prcvchn or part[npart]->isOmni())
        {
            if (partonoffRead(npart))
                part[npart]->NoteOn(note, velocity);
        }
    }
#ifdef REPORT_NOTE_ON_TIME
    if (Runtime.showTimes)
    {
        using Microsec = std::chrono::duration<int, std::micro>;
        auto duration = duration_cast<Microsec>(steady_clock::now() - noteTime);
        Runtime.Log("Note start time " + to_string(duration.count()) + "us");
    }
#endif
}


// Note Off Messages
void SynthEngine::NoteOff(uchar chan, uchar note)
{
#ifdef REPORT_NOTES_ON_OFF
    ++Runtime.noteOffSeen; // note test
    if (Runtime.noteOffSeen != Runtime.noteOffSent)
        Runtime.Log("Note off diff " + to_string(Runtime.noteOffSent - Runtime.noteOffSeen));
#endif

    for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
    {
        // mask values 16 - 31 to still allow a note off
        if ((chan == (part[npart]->Prcvchn & 0xef) or part[npart]->isOmni()) and partonoffRead(npart))
            part[npart]->NoteOff(note);
    }
}


int SynthEngine::RunChannelSwitch(uchar chan, int value)
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
            timespec now_struct;
            clock_gettime(CLOCK_MONOTONIC, &now_struct);
            int64_t now_ms = int64_t(now_struct.tv_sec) * 1000 + int64_t(now_struct.tv_nsec) / 1000000;
            if ((now_ms - channelTimer) > 60)
                channelTimer = now_ms;
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


static inline bool isOmniCC(uchar CCtype)
{
    // This works specifically for omni messages 124 and 125 by exploiting that
    // they differ only in the last bit.
    static_assert(MIDI::CC::omniOff == 124 and MIDI::CC::omniOn == 125);
    return (CCtype & 126) == 124;
}

// Controllers
void SynthEngine::SetController(uchar chan, int CCtype, short int par)
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
        maxPart = Runtime.numAvailableParts;
    }
    else
    {
        bool vector = (chan >= 0x80);
        chan &= 0x3f;
        if (chan >= Runtime.numAvailableParts)
            return; // shouldn't be possible
        minPart = chan;
        maxPart = chan + 1;
        if (vector)
            chan &= 0xf;
    }


    for (int npart = minPart; npart < maxPart; ++ npart)
    {
        // Send the controller to all enabled parts assigned to the channel, or
        // that have omni enabled. If the message is itself an omni message, it
        // only goes to the former.
        if ((part[npart]->Prcvchn == chan or (part[npart]->isOmni() and
            not isOmniCC(CCtype))) and part[npart]->Penabled == 1)
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
     * for insert effect only, | 96 LSB sets destination
     * for system only, &3 sets destination LSB value
     *
     * Data LSB param value
     */

    uchar group = Runtime.nrpnH | 0x20;
    uchar effnum = Runtime.nrpnL;
    uchar parnum = Runtime.dataH;
    uchar value = Runtime.dataL;
    uchar efftype = (parnum & 0x60);
    Runtime.dataL = 0xff; // use once then clear it out

    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));
    putData.data.value = value;
    putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
    putData.data.source = TOPLEVEL::action::fromMIDI | TOPLEVEL::action::forceUpdate;

    if (group == 0x24) // sys
    {
        putData.data.part = TOPLEVEL::section::systemEffects;
        if (efftype == 0x40)
        {
            putData.data.control = EFFECT::sysIns::effectType;
        }
        else if (efftype == 0x60) // send eff to
        {
            putData.data.control = (parnum & 3);
            putData.data.insert = 16;
        }
        else
        {
            putData.data.kit = EFFECT::type::none + sysefx[effnum]->geteffect();
            putData.data.control = parnum;
        }
    }
    else // ins
    {
        putData.data.part = TOPLEVEL::section::insertEffects;
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
        interchange.commandEffects(putData);
    else // TODO next line is a hack!
        midilearn.writeMidi(putData, false);
}


int SynthEngine::setRootBank(int root, int banknum, bool inplace)
{
                      ///////TODO this function should be in class Bank (makes SyntEngine needlessly complex)
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
                if (not inplace)
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
            if (not inplace)
                name = "No match for root ID " + asString(root);
        }
    }

    if (ok && (banknum < 0x80))
    {
        if (bank.setCurrentBankID(banknum))
        {
            if (not inplace)
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
            if (not inplace)
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
    if (not inplace)
        msgID = textMsgBuffer.push(name);
    if (!ok)
        msgID |= 0xFF0000;
    return msgID;
}


int SynthEngine::setProgramByName(CommandBlock& cmd)
{
    steady_clock::time_point startTime;
    if (Runtime.showTimes)
        startTime = steady_clock::now();

    int msgID = NO_MSG;
    bool ok = true;
    int npart = int(cmd.data.kit);
    string fname{textMsgBuffer.fetch(cmd.data.miscmsg)};
    fname = setExtension(fname, EXTEN::yoshInst);
    if (not isRegularFile(fname))
        fname = setExtension(fname, EXTEN::zynInst);
    string name = findLeafName(fname);
    if (name < "!")
    {
        name = "Invalid instrument name " + name;
        ok = false;
    }
    if (ok and not isRegularFile(fname))
    {
        name = "Can't find " + fname;
        ok = false;
    }
    if (ok)
    {
        ok = setProgram(fname, npart);
        if (not ok)
            name = "File " + name + "unrecognised or corrupted";
    }

    if (ok and Runtime.showTimes)
    {
        using Millisec = std::chrono::duration<int, std::milli>;
        auto duration = duration_cast<Millisec>(steady_clock::now() - startTime);
        name += ("  Time " + to_string(duration.count()) + "ms");
    }

    msgID = textMsgBuffer.push(name);
    if (not ok)
    {
        msgID |= 0xFF0000;
        partonoffLock(npart, 2); // as it was
    }
    else
    {
        Runtime.sessionSeen[TOPLEVEL::XML::Instrument] = true;
        addHistory(setExtension(fname, EXTEN::zynInst), TOPLEVEL::XML::Instrument);
        partonoffLock(npart, 1);
    }
    return msgID;
}


int SynthEngine::setProgramFromBank(CommandBlock& cmd, bool inplace)
{
    steady_clock::time_point startTime;
    if (not inplace and Runtime.showTimes)
        startTime = steady_clock::now();

    int instrument = int(cmd.data.value);
    int banknum = cmd.data.engine;
    if (banknum == UNUSED)
        banknum = Runtime.currentBank;
    int npart = cmd.data.kit;
    int root  = cmd.data.insert;
    if (root == UNUSED)
        root = Runtime.currentRoot;

    bool ok;

    string fname = bank.getFullPath(root, banknum, instrument);
    string name = findLeafName(fname);
    if (name < "!")
    {
        ok = false;
        if (not inplace)
            name = "No instrument at " + to_string(instrument + 1) + " in this bank";
    }
    else
    {
        ok = setProgram(fname, npart);
        if (not inplace)
        {
            if (not ok)
                name = "Instrument " + name + " missing or corrupted";
        }
    }

    int msgID = NO_MSG;
    if (not inplace)
    {
        if (ok and Runtime.showTimes)
        {
            using Millisec = std::chrono::duration<int, std::milli>;
            auto duration = duration_cast<Millisec>(steady_clock::now() - startTime);
            name += ("  Time " + to_string(duration.count()) + "ms");
        }
        msgID = textMsgBuffer.push(name);
    }
    if (not ok)
    {
        msgID |= 0xFF0000;
        partonoffLock(npart, 2); // as it was
    }
    else
        partonoffLock(npart, 1);
    return msgID;
}


bool SynthEngine::setProgram(string const& fname, int npart)
{
    // switch active part (UI will do the same on returns_update)
    getRuntime().currentPart = npart;
    interchange.undoRedoClear();
    bool ok = true;
    if (!part[npart]->loadXML(fname))
        ok = false;
    return ok;
}


int SynthEngine::ReadBankRoot()
{
    return Runtime.currentRoot;
}


int SynthEngine::ReadBank()
{
    return Runtime.currentBank;
}


// Set part's channel number
void SynthEngine::SetPartChanForVector(uchar npart, uchar nchan)
{
    if (npart < Runtime.numAvailableParts)
    {
        /* We allow direct controls to set out of range channel numbers.
         * This gives us a way to disable all channel messages to a part.
         * Values 16 to 31 will still allow a note off but values greater
         * than that allow a drone to be set.
         * Sending a valid channel number will restore normal operation
         * as will using the GUI controls.
         */
        part[npart]->Prcvchn =  nchan;
        // Disable Omni by default for newly initialized Vectors.
        part[npart]->Pomni = false;
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
void SynthEngine::cliOutput(list<string>& msg_buf, uint lines)
{
    list<string>::iterator it;

    if (Runtime.toConsole)
    {
        for (it = msg_buf.begin(); it != msg_buf.end(); ++it)
            Runtime.Log(*it);
            // we need this in case someone is working headless
        std::cout << "\nReports sent to console window\n\n";
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
        ofstream fout(page_filename,(ios_base::out | ios_base::trunc));
        for (it = msg_buf.begin(); it != msg_buf.end(); ++it)
            fout << *it << std::endl;
        fout.close();
        string cmd = "less -X -i -M -PM\"q=quit /=search PgUp/PgDown=scroll (line %lt of %L)\" " + page_filename;
        system(cmd.c_str());
        unlink(page_filename.c_str());
    }
    msg_buf.clear();
}


void SynthEngine::ListPaths(list<string>& msg_buf)
{
                      ///////TODO this function should be in class Bank (makes SyntEngine needlessly complex)
    string label;
    string prefix;
    msg_buf.push_back("Root Paths");

    for (uint idx = 0; idx < MAX_BANK_ROOT_DIRS; ++ idx)
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
                      ///////TODO this function should be in class Bank (makes SyntEngine needlessly complex)
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
        for (uint idx = 0; idx < MAX_BANKS_IN_ROOT; ++ idx)
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
                      ///////TODO this function should be in class Bank (makes SyntEngine needlessly complex)
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
    { // build a text list of the enabled 'X' features
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
        || Runtime.numAvailableParts < NUM_MIDI_CHANNELS * 4)
        msg_buf.push_back("  Y axis disabled");
    else
    {
        int Yfeatures = Runtime.vectordata.Yfeatures[chan];
        string Ytext = "Features =";
        if (Yfeatures == 0)
            Ytext = "No Features :(";
        else
        { // build a text list of the enabled 'Y' features
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

                      ///////TODO the following block should delegate to class Bank (makes SyntEngine needlessly complex)
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
                    + asString(Runtime.numAvailableParts));

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

    if (Runtime.enableProgChange)
        msg_buf.push_back("  MIDI Program Change on");
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
    switch (Runtime.alsaMidiType)
    {
        case 2:
            label = "External";
            break;

        case 1:
            label = "Search";
            break;

        default:
            label = "Fixed";
            break;
    }
    msg_buf.push_back("  ALSA MIDI connection " + label);
    msg_buf.push_back("  ALSA MIDI source " + Runtime.alsaMidiDevice);
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
 * Provides a way of setting dynamic system variables via NRPNs
 */
int SynthEngine::SetSystemValue(int type, int value)
{
    list<string> msg;
    string label;
    label = "";
    bool to_send = false;
    uchar  action = 0;
    uchar  cmd = UNUSED;
    uchar  setpart;
    uchar  parameter = UNUSED;

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

                for (uint i = 0; i < Runtime.numAvailableParts; ++ i)
                {
                    if (partonoffRead(i) and (part[i]->Prcvchn == (type - 64) or part[i]->isOmni()))
                    {
                        putData.data.part = i;
                        int tries = 0;
                        bool ok = true;
                        do
                        {
                            ++ tries;
                            ok = interchange.fromMIDI.write(putData.bytes);
                            if (!ok)
                                sleep_for(1us);
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

        case 84: // extended program change CC
            if (value > 119)
                value = 128;
            else
            { // this is far from ideal !!!
                string label = Runtime.testCCvalue(value);if (label != "")
                {
                    parameter = textMsgBuffer.push(label);
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
            sleep_for(1us);
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


int SynthEngine::LoadNumbered(uchar group, uchar entry)
{
    vector<string> const& listType{getHistory(group)};
    if (size_t(entry) >= listType.size())
        return (textMsgBuffer.push(" FAILED: List entry " + to_string(int(entry)) + " out of range") | 0xFF0000);
    string const& filename{listType.at(entry)};
    return textMsgBuffer.push(filename);
}


bool SynthEngine::vectorInit(int dHigh, uchar chan, int par)
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
        uint parts = 2* NUM_MIDI_CHANNELS * (dHigh + 1);
        if (parts > Runtime.numAvailableParts)
            Runtime.numAvailableParts = parts;
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


void SynthEngine::vectorSet(int dHigh, uchar chan, int par)
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

    uchar part = 0;
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
            SetPartChanForVector(chan, chan);
            SetPartChanForVector(chan | 16, chan);
            Runtime.vectordata.Xcc2[chan] = MIDI::CC::panning;
            Runtime.vectordata.Xcc4[chan] = MIDI::CC::filterCutoff;
            Runtime.vectordata.Xcc8[chan] = MIDI::CC::modulation;
            break;

        case 1:
            if (!Runtime.vectordata.Enabled[chan])
                Runtime.Log("Vector X axis must be set before Y");
            else
            {
                SetPartChanForVector(chan | 32, chan);
                SetPartChanForVector(chan | 48, chan);
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
            if (Runtime.numAvailableParts > NUM_MIDI_CHANNELS * 2)
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
        midilearn.writeMidi(putData, true);
    }
}


void SynthEngine::ClearNRPNs()
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
    interchange.undoRedoClear();
    interchange.syncWrite = false;
    interchange.lowPrioWrite = false;
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++ npart)
        part[npart]->busy = false;
    defaults();
    ClearNRPNs();
    if (Runtime.loadDefaultState)
    {
        string filename = Runtime.defaultSession;
        if (isRegularFile(filename))
        {
            Runtime.stateFile = filename;
            Runtime.restoreSessionData(Runtime.stateFile);
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
        midilearn.generalOperations(putData);
        textMsgBuffer.clear();
    }
    // possibly push changed effect state to GUI
    maybePublishEffectsToGui();
}


// Enable/Disable a part
void SynthEngine::partonoffLock(uint npart, int what)
{
    sem_wait(&partlock);
    partonoffWrite(npart, what);
    sem_post(&partlock);
}

/*
 * Intelligent switch for unknown part status that always
 * switches off and later returns original unknown state
 */
void SynthEngine::partonoffWrite(uint npart, int what)
{
    if (npart >= uint(Runtime.numAvailableParts))
        return;
    uchar original = part[npart]->Penabled;
    if (original > 1)
        original = 1;
    uchar tmp = original;
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
            if (Pinsparts[nefx] == int(npart))
                insefx[nefx]->cleanup();
        }
        VUpeak.values.parts[npart] = -1.0f;
        VUpeak.values.partsR[npart] = -1.0f;
    }
}


char SynthEngine::partonoffRead(uint npart)
{
    return (part[npart]->Penabled == 1);
}


// Master audio out (the final sound)
int SynthEngine::MasterAudio(float *outl [NUM_MIDI_PARTS + 1], float *outr [NUM_MIDI_PARTS + 1], int to_process)
{
    static uint VUperiod = samplerate / 20;
    /*
     * The above line gives a VU refresh of at least 50mS
     * but it may be longer depending on the buffer size
     */
    float *mainL = outl[NUM_MIDI_PARTS]; // tiny optimisation
    float *mainR = outr[NUM_MIDI_PARTS]; // makes code clearer

    Samples& tmpmixl = Runtime.genMixl;
    Samples& tmpmixr = Runtime.genMixr;
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

    uchar sound = audioOut.load();
    switch (sound)
    {
        case _SYS_::mute::Pending:
            // set by resolver
            fadeLevel = 1.0f;
            audioOutStore(_SYS_::mute::Fading);
            sound = _SYS_::mute::Fading;
            break;
        case _SYS_::mute::Fading:
            if (fadeLevel < 0.001f)
            {
                audioOutStore(_SYS_::mute::Active);
                sound = _SYS_::mute::Active;
                fadeLevel = 0;
            }
            break;
        case _SYS_::mute::Active:
            // cleared by resolver
            break;
        case _SYS_::mute::Complete:
            // set by resolver and paste
            audioOutStore(_SYS_::mute::Idle);
            break;
        case _SYS_::mute::Request:
            // set by paste routine
            audioOutStore(_SYS_::mute::Immediate);
            sound = _SYS_::mute::Active;
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
    for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
            partLocal[npart] = partonoffRead(npart);

    if (sound == _SYS_::mute::Active)
    {
        for (uint npart = 0; npart < (Runtime.numAvailableParts); ++npart)
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
        for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
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
                    insefx[nefx]->out(part[efxpart]->partoutl.get(),
                                      part[efxpart]->partoutr.get());
            }
        }

        // Apply the part volumes and pannings (after insertion effects)
        uchar panLaw = Runtime.panLaw;
        for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
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
            memset(tmpmixl.get(), 0, sent_bufferbytes);
            memset(tmpmixr.get(), 0, sent_bufferbytes);
            if (!syseffEnable[nefx])
                continue; // is off

            // Mix the channels according to the part settings about System Effect
            for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
            {
                if (partLocal[npart]               // it's enabled
                 && Psysefxvol[nefx][npart]        // it's sending an output
                 && (part[npart]->Paudiodest & 1)) // it's connected to the main outs
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
            sysefx[nefx]->out(tmpmixl.get(), tmpmixr.get());

            // Add the System Effect to sound output
            float outvol = sysefx[nefx]->sysefxgetvolume();
            for (int i = 0; i < sent_buffersize; ++i)
            {
                mainL[i] += tmpmixl[i] * outvol;
                mainR[i] += tmpmixr[i] * outvol;
            }
        }

        for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
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
                volume = decibel<-40>(1.0f - TransVolume/96.0f);
            }
            else if (TransVolume - Pvolume > cStep)
            {
                TransVolume -= cStep;
                volume = decibel<-40>(1.0f - TransVolume/96.0f);
            }
            mainL[idx] *= volume; // apply Master Volume
            mainR[idx] *= volume;
            if (sound == _SYS_::mute::Fading) // fadeLevel must also have been set
            {
                for (uint npart = 0; npart < (Runtime.numAvailableParts); ++npart)
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
        for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
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
            for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
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
{
    if (!VUready)
        return;

    float fade;
    float root;
    int buffsize = VUcopy.values.buffersize;

    root = sqrt(VUcopy.values.vuRmsPeakL / buffsize);
    VUdata.values.vuRmsPeakL = ((VUdata.values.vuRmsPeakL * 7) + root) / 8;
    root = sqrt(VUcopy.values.vuRmsPeakR / buffsize);
    VUdata.values.vuRmsPeakR = ((VUdata.values.vuRmsPeakR * 7) + root) / 8;

    fade = VUdata.values.vuOutPeakL * 0.92f;// mult;
    if (fade >= 1.0f) // overload protection
        fade = 0.0f;
    if (VUcopy.values.vuOutPeakL > fade)
        VUdata.values.vuOutPeakL = VUcopy.values.vuOutPeakL;
    else
        VUdata.values.vuOutPeakL = fade;

    fade = VUdata.values.vuOutPeakR * 0.92f;// mult;
    if (VUcopy.values.vuOutPeakR > fade)
        VUdata.values.vuOutPeakR = VUcopy.values.vuOutPeakR;
    else
        VUdata.values.vuOutPeakR = fade;

    for (uint npart = 0; npart < Runtime.numAvailableParts; ++npart)
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


void SynthEngine::setPaudiodest(int value)
{
    Paudiodest = value;
}


void SynthEngine::setPsysefxvol(int Ppart, int Pefx, char Pvol)
{
    Psysefxvol[Pefx][Ppart] = Pvol;
    sysefxvol[Pefx][Ppart]  = decibel<-40>(1.0f - Pvol / 96.0f);  // Pvol=0..127 => -40dB .. +12.9166dB
}


void SynthEngine::setPsysefxsend(int Pefxfrom, int Pefxto, char Pvol)
{
    Psysefxsend[Pefxfrom][Pefxto] = Pvol;
    sysefxsend[Pefxfrom][Pefxto]  = decibel<-40>(1.0f - Pvol / 96.0f);
}

/**
 * Triggered by Param change or general init;
 * Collect current state of complex effect data and push an update towards GUI.
 * The GuiDataExchange system (located in InterChange) is used to publish the
 * Data Transfer Objects into the GUI, activated by sending a notification through
 * the toGUI ringbuffer. When receiving such a push, the GUI invokes EffUI::refresh().
 */
void SynthEngine::pushEffectUpdate(uchar partNum)
{
    bool isPart{partNum < NUM_MIDI_PARTS};
    bool isInsert{partNum != TOPLEVEL::section::systemEffects};

    assert(isPart
        || partNum == TOPLEVEL::section::systemEffects
        || partNum == TOPLEVEL::section::insertEffects);
    assert(part[getRuntime().currentPart]);
    Part& currPart{*part[getRuntime().currentPart]};
    // the "current" effect as selected / exposed in the GUI
    uchar effnum = isPart? currPart.Peffnum
                 : isInsert? inseffnum : syseffnum;
    assert(effnum < (isPart? NUM_PART_EFX : isInsert? NUM_INS_EFX : NUM_SYS_EFX));

    EffectMgr** effInstance = isPart? currPart.partefx
                            : isInsert? insefx : sysefx;

    EffectDTO dto;
    dto.effNum = effnum;
    dto.effType = effInstance[effnum]->geteffect();
    dto.isInsert = isInsert;
    dto.enabled  = (0 != dto.effType && ((isPart && !currPart.Pefxbypass[effnum])
                                        ||(isInsert && Pinsparts[effnum] != -1)
                                        ||(!isInsert && syseffEnable[effnum])));
    dto.changed = effInstance[effnum]->geteffectpar(-1);
    dto.currPreset = effInstance[effnum]->getpreset();
    dto.insertFxRouting = isPart || !isInsert? -1 : Pinsparts[effnum];
    dto.partFxRouting = !isPart?    +1 : currPart.Pefxroute[effnum];
    dto.partFxBypass  = !isPart? false : currPart.Pefxbypass[effnum];
    effInstance[effnum]->getAllPar(dto.param);
    //////////////////////////////////////////////////TODO 2/24 as partial workaround until all further direct core accesses are addressed
    dto.eff_in_core_TODO_deprecated = effInstance[effnum];

    if (isPart)
        partEffectUiCon.publish(dto);
    else if (isInsert)
        insEffectUiCon.publish(dto);
    else
        sysEffectUiCon.publish(dto);

    if (dto.effType == (EFFECT::type::eq - EFFECT::type::none))
    {// cascading update for the embedded EQ graph
        EqGraphDTO graphDto;
        effInstance[effnum]->renderEQresponse(graphDto.response);
        if (isPart)
            partEqGraphUiCon.publish(graphDto);
        else if (isInsert)
            insEqGraphUiCon.publish(graphDto);
        else
            sysEqGraphUiCon.publish(graphDto);
    }
}


/**
 * Push a complete update of Effect state, in case the GUI is active.
 * There are three distinct EffUI modules, each receiving the state of "the current"
 * selected effect. Calling this function is only required when effect state changes
 * are _not_ propagated via InterChange::commandSysIns(), commandEffects() or commandPart().
 * Especially it must be invoked after loading or pasting state, and this is covered by getfromXML().
 * Init() and defaults() do not call this function; either it is covered otherwise
 * or because the default constructed GUI widgets do not need an initial push
 * Thus, the only other situation to cover is a call to SynthEngine::resetAll().
 */
void SynthEngine::maybePublishEffectsToGui()
{
#ifdef GUI_FLTK
    if (not interchange.guiMaster)
        return; // publish only while GUI is active

    pushEffectUpdate(TOPLEVEL::section::systemEffects);
    pushEffectUpdate(TOPLEVEL::section::insertEffects);
    pushEffectUpdate(getRuntime().currentPart);
#endif
}


// Panic! (Clean up all parts and effects)
void SynthEngine::ShutUp()
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


bool SynthEngine::loadStateAndUpdate(string const& filename)
{
    interchange.undoRedoClear();
    Runtime.sessionStage = _SYS_::type::InProgram;
    bool success = Runtime.restoreSessionData(filename);
    if (!success)
        defaults();
    return success;
}


bool SynthEngine::saveState(string const& filename)
{
    return Runtime.saveSessionData(filename);
}


bool SynthEngine::loadPatchSetAndUpdate(string fname)
{
    interchange.undoRedoClear();
    bool result;
    fname = setExtension(fname, EXTEN::patchset);
    result = loadXML(fname); // load the data
    if (result)
        setAllPartMaps();
    return result;
}


bool SynthEngine::installBanks()
{
                      ///////TODO this function should be in class Bank (makes SyntEngine needlessly complex)
    string bankFile = file::configDir() + '/' + YOSHIMI + ".banks";
    bool newBanks = false;
    if (isRegularFile(bankFile))
    {
        newBanks = bank.establishBanks(bankFile);
    }
    else
    {
       newBanks = bank.establishBanks(std::nullopt);
       Runtime.currentRoot = 5;
    }
    Runtime.Log("\nFound " + asString(bank.instrumentsInBanks) + " instruments in " + asString(bank.banksInRoots) + " banks");

    if (newBanks)
        Runtime.Log(textMsgBuffer.fetch(setRootBank(5, 5) & 0xff));
    else
        Runtime.Log(textMsgBuffer.fetch(setRootBank(Runtime.tempRoot, Runtime.tempBank) & 0xff));
    return true;
}


bool SynthEngine::saveBanks()
{
    string name = file::configDir() + '/' + YOSHIMI;
    string bankname = name + ".banks";

    XMLStore xml{TOPLEVEL::XML::Bank};
    XMLtree xmlInfo = xml.accessTop().getElm("INFORMATION");
    // Info-node is added automatically together with the metadata for root
    // for banks we store the current Bank-Version there
    xmlInfo.addPar_int("Banks_Version", bank.getVersion());

    XMLtree xmlBankList = xml.addElm("BANKLIST");
    bank.saveToConfigFile(xmlBankList);

    if (not xml.saveXMLfile(bankname, getRuntime().getLogger(), getRuntime().gzipCompression))
        Runtime.Log("Failed to save config to " + bankname);

    return true;
}


void SynthEngine::newHistory(string name, uint group)
{
    if (findLeafName(name) < "!")
        return;
    if (group == TOPLEVEL::XML::Instrument and name.rfind(EXTEN::yoshInst) != string::npos)
        name = setExtension(name, EXTEN::zynInst);
    getHistory(group).push_back(name);
}


void SynthEngine::addHistory(string const& name, uint group)
{
    if (findLeafName(name) < "!")
    {
        return;
    }
    if (group > TOPLEVEL::XML::ScalaMap)
        return; // last seen not stored for these.

    historyLastSeen.at(group) = name;

    if (Runtime.historyLock[group])
    {
        return;
    }

    vector<string>& historyData{getHistory(group)};
    auto it = historyData.begin();
    historyData.erase(std::remove(it, historyData.end(), name), historyData.end()); // remove all matches
    historyData.insert(historyData.begin(), name);
    while(historyData.size() > MAX_HISTORY)
        historyData.pop_back();
}


vector<string>& SynthEngine::getHistory(uint group)
{
    switch(group)
    {
        case TOPLEVEL::XML::Instrument: // 0
            return InstrumentHistory;
            break;
        case TOPLEVEL::XML::Patch: // 1
            return ParamsHistory;
            break;
        case TOPLEVEL::XML::Scale: // 2
            return ScaleHistory;
            break;
        case TOPLEVEL::XML::State: // 3
            return StateHistory;
            break;
        case TOPLEVEL::XML::Vector: // 4
            return VectorHistory;
            break;
        case TOPLEVEL::XML::MLearn: // 5
            return MidiLearnHistory;
            break;
        case TOPLEVEL::XML::Presets: // 6
            return PresetHistory;
            break;

        case TOPLEVEL::XML::PadSample: // 7
            return PadHistory;
            break;
        case TOPLEVEL::XML::ScalaTune: // 8
            return TuningHistory;
            break;
        case TOPLEVEL::XML::ScalaMap: // 9
            return KeymapHistory;
            break;
        default:
            // can't identify what is calling this.
            // It's connected with opening the filer on presets
            Runtime.Log("Unrecognised history group " + to_string(group) + "\nUsing patchset history");
            return ParamsHistory;
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
    if (group > TOPLEVEL::XML::ScalaMap)
        return ""; // last seen not stored for these.
    if (group == TOPLEVEL::XML::Instrument && Runtime.sessionSeen[group] == false)
        return "";

    return historyLastSeen.at(group);
}


bool SynthEngine::loadHistory()
{
    string historyFile{file::localDir()  + "/recent"};
    if (not isRegularFile(historyFile))
    {   // second attempt at legacy location...
        historyFile = file::configDir() + '/' + YOSHIMI + ".history";
        if (not isRegularFile(historyFile))
        {
            Runtime.Log("Missing recent history file");
            return false;
        }
    }
    XMLStore xml{historyFile, Runtime.getLogger()};
    XMLtree xmlHistory = xml.getElm("HISTORY");
    if (not xmlHistory)
    {
        Runtime. Log("Failed to extract history data, no <HISTORY> branch in \""+historyFile+"\"");
        return false;
    }
    uint typeID;
    string historyKind;
    string fileTypeID;
    for (typeID = TOPLEVEL::XML::Instrument; typeID <= TOPLEVEL::XML::ScalaMap; ++typeID)
    {
        switch (typeID)
        {
            case TOPLEVEL::XML::Instrument:
                historyKind = "XMZ_INSTRUMENTS";
                fileTypeID = "xiz_file";
                break;
            case TOPLEVEL::XML::Patch:
                historyKind = "XMZ_PATCH_SETS";
                fileTypeID = "xmz_file";
                break;
            case TOPLEVEL::XML::Scale:
                historyKind = "XMZ_SCALE";
                fileTypeID = "xsz_file";
                break;
            case TOPLEVEL::XML::State:
                historyKind = "XMZ_STATE";
                fileTypeID = "state_file";
                break;
            case TOPLEVEL::XML::Vector:
                historyKind = "XMZ_VECTOR";
                fileTypeID = "xvy_file";
                break;
            case TOPLEVEL::XML::MLearn:
                historyKind = "XMZ_MIDILEARN";
                fileTypeID = "xly_file";
                break;
            case TOPLEVEL::XML::Presets:
                historyKind = "XMZ_PRESETS";
                fileTypeID = "xpz_file";
                break;

            case TOPLEVEL::XML::PadSample:
                historyKind = "XMZ_PADSAMPLE";
                fileTypeID = "wav_file";
                break;
            case TOPLEVEL::XML::ScalaTune:
                historyKind = "XMZ_TUNING";
                fileTypeID = "scl_file";
                break;
            case TOPLEVEL::XML::ScalaMap:
                historyKind = "XMZ_KEYMAP";
                fileTypeID = "kbm_file";
                break;
        }
        if (XMLtree xmlHistoryType = xmlHistory.getElm(historyKind))
        {
            Runtime.historyLock[typeID] = xmlHistoryType.getPar_bool("lock_status", false);
            uint hist_size = xmlHistoryType.getPar_int("history_size", 0, 0, MAX_HISTORY);
            if (hist_size > 0)
            {// should never exceed max history
                assert (hist_size <= MAX_HISTORY);
                for (uint i = 0; i < hist_size; ++i)
                {
                    if (XMLtree xmlFile = xmlHistoryType.getElm("XMZ_FILE", i))
                    {
                        string histFileName = xmlFile.getPar_str(fileTypeID);
                        if (fileTypeID == "xiz_file" and not isRegularFile(histFileName))
                        {
                            if (histFileName.rfind(EXTEN::zynInst) != string::npos)
                                histFileName = setExtension(histFileName, EXTEN::yoshInst);
                        }
                        if (not histFileName.empty() and isRegularFile(histFileName))
                            newHistory(histFileName, typeID);
                    }

                }

                string tryRecent = xmlHistoryType.getPar_str("most_recent");
                if (not tryRecent.empty())
                    historyLastSeen.at(typeID) = tryRecent;
            }
        }
    }// for
    return true;
}


bool SynthEngine::saveHistory()
{
    XMLStore xml{TOPLEVEL::XML::History};
    XMLtree xmlHistory = xml.addElm("HISTORY");
    {
        uint typeID;
        string historyKind;
        string fileTypeID;
        for (typeID = TOPLEVEL::XML::Instrument; typeID <= TOPLEVEL::XML::ScalaMap; ++typeID)
        {
            switch (typeID)
            {
                case TOPLEVEL::XML::Instrument:
                    historyKind = "XMZ_INSTRUMENTS";
                    fileTypeID = "xiz_file";
                    break;
                case TOPLEVEL::XML::Patch:
                    historyKind = "XMZ_PATCH_SETS";
                    fileTypeID = "xmz_file";
                    break;
                case TOPLEVEL::XML::Scale:
                    historyKind = "XMZ_SCALE";
                    fileTypeID = "xsz_file";
                    break;
                case TOPLEVEL::XML::State:
                    historyKind = "XMZ_STATE";
                    fileTypeID = "state_file";
                    break;
                case TOPLEVEL::XML::Vector:
                    historyKind = "XMZ_VECTOR";
                    fileTypeID = "xvy_file";
                    break;
                case TOPLEVEL::XML::MLearn:
                    historyKind = "XMZ_MIDILEARN";
                    fileTypeID = "xly_file";
                    break;
                case TOPLEVEL::XML::Presets:
                    historyKind = "XMZ_PRESETS";
                    fileTypeID = "xpz_file";
                    break;

                case TOPLEVEL::XML::PadSample:
                    historyKind = "XMZ_PADSAMPLE";
                    fileTypeID = "wav_file";
                    break;
                case TOPLEVEL::XML::ScalaTune:
                    historyKind = "XMZ_TUNING";
                    fileTypeID = "scl_file";
                    break;
                case TOPLEVEL::XML::ScalaMap:
                    historyKind = "XMZ_KEYMAP";
                    fileTypeID = "kbm_file";
                    break;
            }
            vector<string> const& historyData{getHistory(typeID)};
            if (not historyData.empty())
            {
                uint i{0};
                XMLtree xmlHistoryType = xmlHistory.addElm(historyKind);
                    xmlHistoryType.addPar_bool("lock_status", Runtime.historyLock[typeID]);
                    xmlHistoryType.addPar_int ("history_size", historyData.size());
                    assert(historyData.size() <= MAX_HISTORY);
                    for (auto const& historyEntry : historyData)
                    {
                        XMLtree xmlFile = xmlHistoryType.addElm("XMZ_FILE", i);
                            xmlFile.addPar_str(fileTypeID, historyEntry);
                        ++i;
                    }
                    xmlHistoryType.addPar_str("most_recent", historyLastSeen.at(typeID));
            }
        }// for
    }
    string historyFile = file::localDir()  + "/recent";
    bool success = xml.saveXMLfile(historyFile, Runtime.getLogger(), Runtime.gzipCompression);
    if (not success)
        Runtime.Log("Failed to save history index to \""+ historyFile+"\"");
    return success;
}


void SynthEngine::add2XML(XMLStore& xml)
{
    XMLtree xmlMaster = xml.addElm("MASTER");

    xmlMaster.addPar_int ("current_midi_parts" , Runtime.numAvailableParts);
    xmlMaster.addPar_int ("panning_law"        , Runtime.panLaw);
    xmlMaster.addPar_frac("volume"             , Pvolume);
    xmlMaster.addPar_int ("key_shift"          , Pkeyshift);
    xmlMaster.addPar_real("bpm_fallback"       , PbpmFallback);
    xmlMaster.addPar_int ("channel_switch_type", Runtime.channelSwitchType);
    xmlMaster.addPar_int ("channel_switch_CC"  , Runtime.channelSwitchCC);

    XMLtree xmlMicrotonal = xmlMaster.addElm("MICROTONAL");
    microtonal.add2XML(xmlMicrotonal);

    for (uint npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        XMLtree xmlPart = xmlMaster.addElm("PART",npart);
        part[npart]->add2XML_YoshimiPartSetup(xmlPart);
    }

    XMLtree xmlSysEffects = xmlMaster.addElm("SYSTEM_EFFECTS");
    for (uint nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        XMLtree xmlSysfx = xmlSysEffects.addElm("SYSTEM_EFFECT", nefx);
            XMLtree xmlEffectSetting = xmlSysfx.addElm("EFFECT");
            sysefx[nefx]->add2XML(xmlEffectSetting);

            for (uint pefx = 0; pefx < NUM_MIDI_PARTS; ++pefx)
            {
                XMLtree xmlMixVol = xmlSysfx.addElm("VOLUME", pefx);
                xmlMixVol.addPar_int("vol", Psysefxvol[nefx][pefx]);
            }

            for (int tonefx = nefx + 1; tonefx < NUM_SYS_EFX; ++tonefx)
            {
                XMLtree xmlSendVol = xmlSysfx.addElm("SENDTO", tonefx);
                xmlSendVol.addPar_int("send_vol", Psysefxsend[nefx][tonefx]);
            }
    }

    XMLtree xmlInsEffects = xmlMaster.addElm("INSERTION_EFFECTS");
    for (uint nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        XMLtree xmlInsfx = xmlInsEffects.addElm("INSERTION_EFFECT", nefx);
        xmlInsfx.addPar_int("part", Pinsparts[nefx]);

            XMLtree xmlEffectSetting = xmlInsfx.addElm("EFFECT");
            insefx[nefx]->add2XML(xmlEffectSetting);
    }

    for (uint i = 0; i < NUM_MIDI_CHANNELS; ++i)
        if (Runtime.vectordata.Xaxis[i] < 127)
        {
            XMLtree xmlVector = xmlMaster.addElm("VECTOR", i);
            vectorcontrol.insertVectorData(i, false, xmlVector, "");
        }
}


bool SynthEngine::savePatchesXML(string filename)
{
    filename = setExtension(filename, EXTEN::patchset);
    XMLStore xml{TOPLEVEL::XML::Patch};
    this->add2XML(xml);
    return xml.saveXMLfile(filename
                          ,Runtime.getLogger()
                          ,Runtime.gzipCompression);
}


bool SynthEngine::loadXML(string const& filename)
{
    XMLStore xml{filename, Runtime.getLogger()};
    postLoadCheck(xml,*this);
    if (not xml)
        return false;
    defaults();
    bool success = getfromXML(xml);
    setAllPartMaps();
    return success;
}


bool SynthEngine::getfromXML(XMLStore& xml)
{
    XMLtree xmlMaster = xml.getElm("MASTER");
    if (not xmlMaster)
    {
        Runtime.Log("SynthEngine getfromXML: no <MASTER> branch found in XML");
        return false;
    }
    Runtime.numAvailableParts = xmlMaster.getPar_int("current_midi_parts", NUM_MIDI_CHANNELS, NUM_MIDI_CHANNELS, NUM_MIDI_PARTS);
    Runtime.panLaw = xmlMaster.getPar_int("panning_law", Runtime.panLaw, MAIN::panningType::cut, MAIN::panningType::boost);
    setPvolume(xmlMaster.getPar_frac("volume", Pvolume, 0, 127));
    setPkeyshift(xmlMaster.getPar_int("key_shift", Pkeyshift, MIN_KEY_SHIFT + 64, MAX_KEY_SHIFT + 64));
    PbpmFallback = xmlMaster.getPar_real("bpm_fallback", PbpmFallback, BPM_FALLBACK_MIN, BPM_FALLBACK_MAX);
    Runtime.channelSwitchType = xmlMaster.getPar_int("channel_switch_type", Runtime.channelSwitchType, 0, 5);
    Runtime.channelSwitchCC   = xmlMaster.getPar_int("channel_switch_CC", Runtime.channelSwitchCC, 0, 128);
    Runtime.channelSwitchValue = 0;
    for (uint npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if (XMLtree xmlPart = xmlMaster.getElm("PART", npart))
        {
            part[npart]->getfromXML(xmlPart);
            if (partonoffRead(npart) && (part[npart]->Paudiodest & 2))
                Config::instances().registerAudioPort(getUniqueId(), npart);
        }

    if (XMLtree xmlMicrotonal = xmlMaster.getElm("MICROTONAL"))
        microtonal.getfromXML(xmlMicrotonal);

    sysefx[0]->defaults();
    if (XMLtree xmlSysEffects = xmlMaster.getElm("SYSTEM_EFFECTS"))
        for (uint nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
            if (XMLtree xmlSysfx = xmlSysEffects.getElm("SYSTEM_EFFECT", nefx))
            {
                if (XMLtree xmlEffectSetting = xmlSysfx.getElm("EFFECT"))
                    sysefx[nefx]->getfromXML(xmlEffectSetting);

                for (uint partefx = 0; partefx < NUM_MIDI_PARTS; ++partefx)
                    if (XMLtree xmlMixVol = xmlSysfx.getElm("VOLUME", partefx))
                        setPsysefxvol(partefx, nefx, xmlMixVol.getPar_127("vol", Psysefxvol[partefx][nefx]));

                for (uint tonefx = nefx + 1; tonefx < NUM_SYS_EFX; ++tonefx)
                    if (XMLtree xmlSendVol = xmlSysfx.getElm("SENDTO", tonefx))
                        setPsysefxsend(nefx, tonefx, xmlSendVol.getPar_127("send_vol", Psysefxsend[nefx][tonefx]));
            }

    if (XMLtree xmlInsEffects = xmlMaster.getElm("INSERTION_EFFECTS"))
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
            if (XMLtree xmlInsfx = xmlInsEffects.getElm("INSERTION_EFFECT", nefx))
            {
                Pinsparts[nefx] = xmlInsfx.getPar_int("part", Pinsparts[nefx], -2, NUM_MIDI_PARTS);
                if (XMLtree xmlEffectSetting = xmlInsfx.getElm("EFFECT"))
                    insefx[nefx]->getfromXML(xmlEffectSetting);
            }

    for (uchar i = 0; i < NUM_MIDI_CHANNELS; ++i)
        if (XMLtree xmlVector = xmlMaster.getElm("VECTOR", i))
            vectorcontrol.extractVectorData(i, xmlVector, "");

    // possibly push changed effect state to GUI
    maybePublishEffectsToGui();
    return true;
}


string SynthEngine::makeUniqueName(string const& name)
{
    string result = "Yoshimi";
    if (uniqueId > 0)
        result += ("-" + asString(uniqueId));
    result += " : " + name;
    return result;
}


float SynthEngine::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    uchar type = 0;

    // defaults
    int min = 0;
    float def = 64;
    int max = 127;
    type |= TOPLEVEL::type::Integer;
    uchar learnable = TOPLEVEL::type::Learnable;

    switch (control)
    {
        case MAIN::control::volume:
            def = 90;
            type &= ~TOPLEVEL::type::Integer;
            type |= learnable;
            break;

        case MAIN::control::startInstance:
        case MAIN::control::stopInstance:
            min = 0;
            def = 1;
            max = 31;
            break;

        case MAIN::control::partNumber:
            def = 0;
            max = Runtime.numAvailableParts -1;
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
            type &= ~TOPLEVEL::type::Integer;
            break;

        case MAIN::control::keyShift:
            min = -36;
            def = 0;
            max = 36;
            break;

        case MAIN::control::bpmFallback:
            min = BPM_FALLBACK_MIN;
            def = 120;
            max = BPM_FALLBACK_MAX;
            type &= ~TOPLEVEL::type::Integer;
            break;

        case MAIN::control::mono:
            def = 0; // off
            max = 1;
            type |= learnable;
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
        case MAIN::control::defaultInstrument:
            def = 0;
            max = Runtime.numAvailableParts -1;
            break;
        case MAIN::control::masterReset:
        case MAIN::control::masterResetAndMlearn:
        case MAIN::control::stopSound:
            def = 0;
            max = 0;
            break;

        case MAIN::control::loadInstrumentFromBank:
            return value; // this is just a workaround :(
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


void SynthEngine::CBtest(CommandBlock *candidate, bool miscmsg) // default - don't read message
{
    std::cout << "\n value "     << candidate->data.value
              << "\n type "      << int(candidate->data.type)
              << "\n source "    << int(candidate->data.source)
              << "\n cont "      << int(candidate->data.control)
              << "\n part "      << int(candidate->data.part)
              << "\n kit "       << int(candidate->data.kit)
              << "\n engine "    << int(candidate->data.engine)
              << "\n insert "    << int(candidate->data.insert)
              << "\n parameter " << int(candidate->data.parameter)
              << "\n offset "    << int(candidate->data.offset)
              << std::endl;
    if (miscmsg) // read this *without* deleting it
        std::cout << ">" << textMsgBuffer.fetch(candidate->data.miscmsg, false) << "<" << std::endl;
    else
        std::cout << " miscmsg " << int(candidate->data.miscmsg) << std::endl;
}
