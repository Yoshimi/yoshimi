/*
    SynthEngine.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2009, James Morris

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of ZynAddSubFX original code, modified November 2010
*/

#include <boost/shared_ptr.hpp>

#include "MasterUI.h"
#include "Sql/ProgramBanks.h"
#include "Misc/SynthEngine.h"

SynthEngine *synth = NULL;

char SynthEngine:: random_state[256] = { 0, };
struct random_data SynthEngine:: random_buf;

SynthEngine::SynthEngine()
    :shutup(false),
      samplerate(48000),
      samplerate_f(samplerate),
      halfsamplerate_f(samplerate / 2),
      buffersize(0),
      buffersize_f(buffersize),
      oscilsize(1024),
      oscilsize_f(oscilsize),
      halfoscilsize(oscilsize / 2),
      halfoscilsize_f(halfoscilsize),
      synthperiodStartFrame(0u),
      ctl(NULL),
      fft(NULL),
      recordPending(false),
      stateXMLtree(NULL),
      tmpmixl(NULL),
      tmpmixr(NULL),
      midiBankLSB(-1),
      midiBankMSB(-1),
      synthMuted(0)
{
    ctl = new Controller();
    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart] = NULL;
    for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx] = NULL;
    for(int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx] = NULL;
    shutup = false;
}


SynthEngine::~SynthEngine()
{
    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if(part[npart])
            delete part[npart];
    for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        if(insefx[nefx])
            delete insefx[nefx];
    for(int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        if(sysefx[nefx])
            delete sysefx[nefx];

    if(tmpmixl)
        delete [] tmpmixl;
    if(tmpmixr)
        delete [] tmpmixr;
    if(fft)
        delete fft;
    if(ctl)
        delete ctl;
}


bool SynthEngine::Init(unsigned int audiosrate, int audiobufsize)
{
    if(initstate_r(samplerate + buffersize + oscilsize, random_state,
                   sizeof(random_state), &random_buf))
        Runtime.Log("SynthEngine randomness fails, outcome unpredictable!",
                    true);

    samplerate_f     = samplerate = audiosrate;
    halfsamplerate_f = samplerate / 2;
    buffersize_f     = buffersize = audiobufsize;
    bufferbytes      = buffersize * sizeof(float);
    oscilsize_f      = oscilsize = Runtime.Oscilsize;
    halfoscilsize_f  = halfoscilsize = oscilsize / 2;
    lockgrace =
        boost::posix_time::microsec(roundf(333000.0f * synth->buffersize_f
                                           / synth->samplerate_f));
    // 1/3 period time?

    if(oscilsize < (buffersize / 2)) {
        Runtime.Log("Enforcing oscilsize to half buffersize, "
                    + asString(oscilsize) + " -> " + asString(buffersize / 2));
        oscilsize = buffersize / 2;
    }

    if((fft = new FFTwrapper(oscilsize)) == NULL) {
        Runtime.Log("SynthEngine failed to allocate fft");
        goto bail_out;
    }

    tmpmixl = new float[buffersize];
    tmpmixr = new float[buffersize];
    if((tmpmixl == NULL) || (tmpmixr == NULL)) {
        Runtime.Log("SynthEngine tmpmix allocations failed");
        goto bail_out;
    }

    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart) {
        part[npart] = new Part(&microtonal, fft);
        if(NULL == part[npart]) {
            Runtime.Log("Failed to allocate new Part");
            goto bail_out;
        }
        vuoutpeakpart[npart] = 1e-9f;
        fakepeakpart[npart]  = 0;
    }

    // Insertion Effects init
    for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx) {
        insefx[nefx] = new EffectMgr(1);
        if(NULL == insefx[nefx]) {
            Runtime.Log("Failed to allocate new Insertion EffectMgr");
            goto bail_out;
        }
    }

    // System Effects init
    for(int nefx = 0; nefx < NUM_SYS_EFX; ++nefx) {
        sysefx[nefx] = new EffectMgr(0);
        if(NULL == sysefx[nefx]) {
            Runtime.Log("Failed to allocate new System Effects EffectMgr");
            goto bail_out;
        }
    }
    Defaults();
    if(Runtime.doRestoreJackSession) {
        if(!Runtime.restoreJsession(this)) {
            Runtime.Log("Restore jack session failed");
            goto bail_out;
        }
    }
    else
    if(Runtime.doRestoreState) {
        if(!Runtime.restoreState(this)) {
            Runtime.Log("Restore state failed");
            goto bail_out;
        }
    }
    else {
        if(Runtime.paramsLoad.size()) {
            if(loadXML(Runtime.paramsLoad) >= 0) {
                Runtime.paramsLoad = Runtime.addParamHistory(Runtime.paramsLoad);
                Runtime.Log("Loaded " + Runtime.paramsLoad + " parameters");
            }
            else {
                Runtime.Log("Failed to load parameters " + Runtime.paramsLoad);
                goto bail_out;
            }
        }
        if(!Runtime.instrumentLoad.empty()) {
            int loadtopart = 0;
            if(!part[loadtopart]->loadXMLinstrument(Runtime.instrumentLoad)) {
                Runtime.Log(
                    "Failed to load instrument file " + Runtime.instrumentLoad);
                goto bail_out;
            }
            else
                Runtime.Log(
                    "Instrument file " + Runtime.instrumentLoad + " loaded");
        }
    }
    return true;

bail_out:
    if(fft != NULL)
        delete fft;
    fft = NULL;
    if(tmpmixl != NULL)
        delete tmpmixl;
    tmpmixl = NULL;
    if(tmpmixr != NULL)
        delete tmpmixr;
    tmpmixr = NULL;
    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart) {
        if(NULL != part[npart])
            delete part[npart];
        part[npart] = NULL;
    }
    for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx) {
        if(NULL != insefx[nefx])
            delete insefx[nefx];
        insefx[nefx] = NULL;
    }
    for(int nefx = 0; nefx < NUM_SYS_EFX; ++nefx) {
        if(NULL != sysefx[nefx])
            delete sysefx[nefx];
        sysefx[nefx] = NULL;
    }
    return false;
}


void SynthEngine::Defaults(void)
{
    bool wasmuted = (__sync_fetch_and_or(&synthMuted, 0xFF)) ? true : false;
    setPvolume(90);
    setPkeyshift(64);
    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart) {
        part[npart]->defaults();
        part[npart]->midichannel = npart % NUM_MIDI_CHANNELS;
    }
    partOnOff(0, 1); // enable the first part
    for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx) {
        insefx[nefx]->defaults();
        Pinsparts[nefx] = -1;
    }
    // System Effects init
    for(int nefx = 0; nefx < NUM_SYS_EFX; ++nefx) {
        sysefx[nefx]->defaults();
        for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
            setPsysefxvol(npart, nefx, 0);
        for(int nefxto = 0; nefxto < NUM_SYS_EFX; ++nefxto)
            setPsysefxsend(nefx, nefxto, 0);
    }
    microtonal.defaults();
    cleanUp();
    if(!wasmuted)
        __sync_and_and_fetch(&synthMuted, 0);
}


void SynthEngine::noteOn(unsigned char chan,
                         unsigned char note,
                         unsigned char velocity)
{
    if(!velocity)
        noteOff(chan, note); // velocity 0 -> NoteOff
    else {
        if(recordPending && musicClient->recordTrigger())
            guiMaster->record_activated();
        for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
            if(part[npart]->Penabled && (chan == part[npart]->midichannel)) {
                lockSharable();
                part[npart]->NoteOn(note, velocity, keyshift);
                unlockSharable();
            }
    }
}


void SynthEngine::noteOff(unsigned char chan, unsigned char note)
{
    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if(part[npart]->Penabled && (chan == part[npart]->midichannel)) {
            lockSharable();
            part[npart]->NoteOff(note);
            unlockSharable();
        }
}


void SynthEngine::setPitchwheel(unsigned char chan, short int par)
{
    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if(part[npart]->Penabled && (chan == part[npart]->midichannel))
            part[npart]->ctl->setpitchwheel(par);
}


void SynthEngine::setController(unsigned char channel,
                                unsigned char ctrltype,
                                unsigned char par)
{
    switch(ctrltype) {
        case C_bankselectmsb:
            midiBankMSB = par;
            break;

        case C_bankselectlsb:
            midiBankLSB = par;
            break;

        default:
            for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
                // send controller to all active parts assigned to the channel
                if((channel == part[npart]->midichannel)
                   && part[npart]->Penabled)
                    part[npart]->SetController(ctrltype, par);
            break;
    }

    if(ctrltype == C_allsoundsoff) { // cleanup insertion/system FX
        synth->lockSharable();
        for(int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
            sysefx[nefx]->cleanup();
        for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
            insefx[nefx]->cleanup();
        synth->unlockSharable();
    }
}


void SynthEngine::applyMidi(unsigned char *bytes)
{
    unsigned char channel = bytes[0] & 0x0f;
    switch(bytes[0] & 0xf0) {
        case MSG_noteoff: // 128
            noteOff(channel, bytes[1]);
            break;

        case MSG_noteon: // 144
            noteOn(channel, bytes[1], bytes[2]);
            break;

        case MSG_control_change: // 176
            setController(channel, bytes[1], bytes[2]);
            break;

        case MSG_program_change: // 224
        {
            char bankselect = (midiBankLSB < 0) ? midiBankMSB : midiBankLSB;
            bankselect = (bankselect < 0) ? 0 : bankselect;
            for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
                if((channel == part[npart]->midichannel)
                   && part[npart]->Penabled
                   && !part[channel]->loadProgram(bankselect,
                                                  bytes[1] - 1))
                    Runtime.Log("Midi program change failed");
        }
        break;

        case MSG_pitchwheel_control: // 224
            for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
                if(part[npart]->Penabled
                   && (channel == part[npart]->midichannel))
                    part[npart]->ctl->setpitchwheel(
                        ((bytes[2] << 7) | bytes[1]) - 8192);
            break;

        default: // too difficult or just uninteresting
            break;
    }
}


// Enable/Disable a part
void SynthEngine::partOnOff(int npart, int what)
{
    fakepeakpart[npart] = 0;
    if(what)
        part[npart]->Penabled = 1;
    else {
        part[npart]->partDisable();
        for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
            if(Pinsparts[nefx] == npart)
                insefx[nefx]->cleanup();
    }
}


// Enable/Disable a part
void SynthEngine::partEnable(unsigned char npart, bool maybe)
{
    if(npart <= NUM_MIDI_PARTS)
        if(!(part[npart]->Penabled = (maybe) ? 1 : 0)) { // disabled part
            part[npart]->cleanup();
            for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
                if(Pinsparts[nefx] == npart)
                    insefx[nefx]->cleanup();
        }
}


// Master audio out (the final sound)
void SynthEngine::MasterAudio(float *outl, float *outr)
{
    memset(outl, 0, bufferbytes);
    memset(outr, 0, bufferbytes);
    if(synthMuted)
        return;
    // Compute part samples in part=>partoutl, partoutr
    int npart;
    for(npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if(part[npart]->Active()) {
            if(timedlockExclusive()) {
                part[npart]->ComputePartSmps();
                unlockExclusive();
            }
            else {
                Runtime.Log("MasterAudio skips ComputePartSmps");
                memset(part[npart]->partoutl, 0, bufferbytes);
                memset(part[npart]->partoutr, 0, bufferbytes);
            }
        }

    // Insertion effects
    int nefx;
    for(nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        if(Pinsparts[nefx] >= 0) {
            int efxpart = Pinsparts[nefx];
            if(part[efxpart]->Active())
                insefx[nefx]->out(part[efxpart]->partoutl,
                                  part[efxpart]->partoutr);
        }

    // Apply the part volumes and pannings (after insertion effects)
    for(npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if(part[npart]->Active()) {
            float newvol_l = part[npart]->volume;
            float newvol_r = part[npart]->volume;
            float oldvol_l = part[npart]->oldvolumel;
            float oldvol_r = part[npart]->oldvolumer;
            float pan      = part[npart]->panning;
            if(pan < 0.5)
                newvol_l *= (1.0 - pan) * 2.0;
            else
                newvol_r *= pan * 2.0;

            if(aboveAmplitudeThreshold(oldvol_l,
                                       newvol_l)
               || aboveAmplitudeThreshold(oldvol_r, newvol_r)) { // the volume or the panning has changed and needs interpolation
                for(int i = 0; i < buffersize; ++i) {
                    float vol_l = interpolateAmplitude(oldvol_l,
                                                       newvol_l,
                                                       i,
                                                       buffersize);
                    float vol_r = interpolateAmplitude(oldvol_r,
                                                       newvol_r,
                                                       i,
                                                       buffersize);
                    part[npart]->partoutl[i] *= vol_l;
                    part[npart]->partoutr[i] *= vol_r;
                }
                part[npart]->oldvolumel = newvol_l;
                part[npart]->oldvolumer = newvol_r;
            }
            else
                for(int i = 0; i < buffersize; ++i) { // the volume did not change
                    part[npart]->partoutl[i] *= newvol_l;
                    part[npart]->partoutr[i] *= newvol_r;
                }
        }

    // System effects
    for(nefx = 0; nefx < NUM_SYS_EFX; ++nefx) {
        if(!sysefx[nefx]->geteffect())
            continue; // is disabled

        // Clean up the samples used by the system effects
        memset(tmpmixl, 0, bufferbytes);
        memset(tmpmixr, 0, bufferbytes);

        // Mix the channels according to the part settings about System Effect
        for(npart = 0; npart < NUM_MIDI_PARTS; ++npart)
            if(part[npart]->Active() && Psysefxvol[nefx][npart]) { // skip if part is disabled or has no output to effect
                                                                   // the output volume of each part to system effect
                float vol = sysefxvol[nefx][npart];
                for(int i = 0; i < buffersize; ++i) {
                    tmpmixl[i] += part[npart]->partoutl[i] * vol;
                    tmpmixr[i] += part[npart]->partoutr[i] * vol;
                }
            }

        // system effect send to next ones
        for(int nefxfrom = 0; nefxfrom < nefx; ++nefxfrom)
            if(Psysefxsend[nefxfrom][nefx]) {
                float v = sysefxsend[nefxfrom][nefx];
                for(int i = 0; i < buffersize; ++i) {
                    tmpmixl[i] += sysefx[nefxfrom]->efxoutl[i] * v;
                    tmpmixr[i] += sysefx[nefxfrom]->efxoutr[i] * v;
                }
            }

        sysefx[nefx]->out(tmpmixl, tmpmixr);

        // Add the System Effect to sound output
        float outvol = sysefx[nefx]->sysefxgetvolume();
        for(int i = 0; i < buffersize; ++i) {
            outl[i] += tmpmixl[i] * outvol;
            outr[i] += tmpmixr[i] * outvol;
        }
    }

    // Mix all parts
    for(npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if(part[npart]->Active())
            for(int i = 0; i < buffersize; ++i) { // the volume did not change
                outl[i] += part[npart]->partoutl[i];
                outr[i] += part[npart]->partoutr[i];
            }

    // Insertion effects for Master Out
    for(nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        if(Pinsparts[nefx] == -2)
//            lockSharable();
            insefx[nefx]->out(outl, outr);
//            unlockSharable();


    meterMutex.lock();
    vuoutpeakl = 1e-12f;
    vuoutpeakr = 1e-12f;
    vurmspeakl = 1e-12f;
    vurmspeakr = 1e-12f;
    meterMutex.unlock();

    float absval;
    for(int idx = 0; idx < buffersize; ++idx) {
        outl[idx] *= volume; // apply Master Volume
        outr[idx] *= volume;

        if((absval = fabsf(outl[idx])) > vuoutpeakl)  // Peak computation (for vumeters)
            vuoutpeakl = absval;
        if((absval = fabsf(outr[idx])) > vuoutpeakr)
            vuoutpeakr = absval;
        vurmspeakl += outl[idx] * outl[idx];  // RMS Peak
        vurmspeakr += outr[idx] * outr[idx];

        if(outl[idx] > 1.0f)
            clippedL = true;
        else
        if(outl[idx] < -1.0f)
            clippedL = true;
        if(outr[idx] > 1.0f)
            clippedR = true;
        else
        if(outr[idx] < -1.0f)
            clippedR = true;

        if(shutup) { // fade-out
            float fade = (float)(buffersize - idx) / (float)buffersize;
            outl[idx] *= fade;
            outr[idx] *= fade;
        }
    }
    if(shutup)
        cleanUp();
    synthperiodStartFrame += buffersize;
    ++LFOParams::time; // update the LFO's time

    meterMutex.lock();
    if(vumaxoutpeakl < vuoutpeakl)
        vumaxoutpeakl = vuoutpeakl;
    if(vumaxoutpeakr < vuoutpeakr)
        vumaxoutpeakr = vuoutpeakr;

    vurmspeakl = sqrtf(vurmspeakl / buffersize);
    vurmspeakr = sqrtf(vurmspeakr / buffersize);

    // Part Peak computation (for Part vu meters/fake part vu meters)
    for(npart = 0; npart < NUM_MIDI_PARTS; ++npart) {
        vuoutpeakpart[npart] = 1.0e-12f;
        if(part[npart]->Active()) {
            float *outl = part[npart]->partoutl;
            float *outr = part[npart]->partoutr;
            for(int i = 0; i < buffersize; ++i) {
                float tmp = fabsf(outl[i] + outr[i]);
                if(tmp > vuoutpeakpart[npart])
                    vuoutpeakpart[npart] = tmp;
            }
            vuoutpeakpart[npart] *= volume; // how is part peak related to master volume??
        }
        else
        if(fakepeakpart[npart] > 1)
            fakepeakpart[npart]--;
    }
    vuOutPeakL    = vuoutpeakl;
    vuOutPeakR    = vuoutpeakr;
    vuMaxOutPeakL = vumaxoutpeakl;
    vuMaxOutPeakR = vumaxoutpeakr;
    vuRmsPeakL    = vurmspeakl;
    vuRmsPeakR    = vurmspeakr;
    vuClippedL    = clippedL;
    vuClippedR    = clippedR;
    meterMutex.unlock();
}


// Parameter control
void SynthEngine::setPvolume(char control_value)
{
    Pvolume = control_value;
    volume  = dB2rap((Pvolume - 96.0f) / 96.0f * 40.0f);
}


void SynthEngine::setPkeyshift(char Pkeyshift_)
{
    Pkeyshift = Pkeyshift_;
    keyshift  = (int)Pkeyshift - 64;
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
void SynthEngine::cleanUp(void)
{
    bool wasmuted = (__sync_fetch_and_or(&synthMuted, 0xFF)) ? true : false;
    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart) {
        part[npart]->cleanup();
        fakepeakpart[npart] = 0;
    }
    for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx]->cleanup();
    for(int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx]->cleanup();
    vuresetpeaks();
    shutup = false;
    if(!wasmuted)
        __sync_and_and_fetch(&synthMuted, 0);
}


// Reset peaks and clear the "clipped" flag (for VU-meter)
void SynthEngine::vuresetpeaks(void)
{
    meterMutex.lock();
    vuOutPeakL    = vuoutpeakl = 1e-12f;
    vuOutPeakR    = vuoutpeakr = 1e-12f;
    vuMaxOutPeakL = vumaxoutpeakl = 1e-12f;
    vuMaxOutPeakR = vumaxoutpeakr = 1e-12f;
    vuRmsPeakL    = vurmspeakl = 1e-12f;
    vuRmsPeakR    = vurmspeakr = 1e-12f;
    vuClippedL    = vuClippedL = clippedL = clippedR = false;
    meterMutex.unlock();
}



void SynthEngine::lockExclusive(void)
{
    using namespace boost::interprocess;
    try {
        synthMutex.lock();
    }
    catch(interprocess_exception &ex) {
        Runtime.Log("SynthEngine::lockExclusive throws exception!");
    }
}


void SynthEngine::unlockExclusive(void)
{
    using namespace boost::interprocess;
    try {
        synthMutex.unlock();
    }
    catch(interprocess_exception &ex) {
        Runtime.Log("SynthEngine::unlockExclusive throws exception!");
    }
}


bool SynthEngine::trylockExclusive(void)
{
    bool ok = false;
    using namespace boost::interprocess;
    try {
        ok = synthMutex.try_lock();
    }
    catch(interprocess_exception &ex) {
        Runtime.Log("SynthEngine::trylockExclusive throws exception!");
    }
    return ok;
}


bool SynthEngine::timedlockExclusive(void)
{
    bool ok = false;
    boost::posix_time::ptime endtime =
        boost::posix_time::microsec_clock::local_time() + lockgrace;
    using namespace boost::interprocess;
    try {
        ok = synthMutex.timed_lock(endtime);
    }
    catch(interprocess_exception &ex) {
        Runtime.Log("SynthEngine::timedlockExclusive throws exception!");
        ok = false;
    }
    return ok;
}


void SynthEngine::lockSharable(void)
{
    using namespace boost::interprocess;
    try {
        synthMutex.lock_sharable();
    }
    catch(interprocess_exception &ex) {
        Runtime.Log("SynthEngine::lockSharable throws exception!");
    }
}


void SynthEngine::unlockSharable(void)
{
    using namespace boost::interprocess;
    try {
        synthMutex.unlock_sharable();
    }
    catch(interprocess_exception &ex) {
        Runtime.Log("SynthEngine::unlockSharable throws exception!");
    }
}

void SynthEngine::add2XML(XMLwrapper *xml)
{
    xml->beginbranch("MASTER");
    xml->addpar("volume", Pvolume);
    xml->addpar("key_shift", Pkeyshift);

    xml->beginbranch("MICROTONAL");
    microtonal.add2XML(xml);
    xml->endbranch();

    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart) {
        xml->beginbranch("PART", npart);
        part[npart]->add2XML(xml);
        xml->endbranch();
    }

    xml->beginbranch("SYSTEM_EFFECTS");
    for(int nefx = 0; nefx < NUM_SYS_EFX; ++nefx) {
        xml->beginbranch("SYSTEM_EFFECT", nefx);
        xml->beginbranch("EFFECT");
        sysefx[nefx]->add2XML(xml);
        xml->endbranch();

        for(int pefx = 0; pefx < NUM_MIDI_PARTS; ++pefx) {
            xml->beginbranch("VOLUME", pefx);
            xml->addpar("vol", Psysefxvol[nefx][pefx]);
            xml->endbranch();
        }

        for(int tonefx = nefx + 1; tonefx < NUM_SYS_EFX; ++tonefx) {
            xml->beginbranch("SENDTO", tonefx);
            xml->addpar("send_vol", Psysefxsend[nefx][tonefx]);
            xml->endbranch();
        }
        xml->endbranch();
    }
    xml->endbranch();

    xml->beginbranch("INSERTION_EFFECTS");
    for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx) {
        xml->beginbranch("INSERTION_EFFECT", nefx);
        xml->addpar("part", Pinsparts[nefx]);

        xml->beginbranch("EFFECT");
        insefx[nefx]->add2XML(xml);
        xml->endbranch(); // EFFECT
        xml->endbranch(); // INSERTION_EFFECT
    }
    xml->endbranch();     // INSERTION_EFFECTS
    xml->endbranch();     // MASTER
}


void SynthEngine::putalldata(char *data, int size)
{
    boost::shared_ptr<XMLwrapper> xmlwrap = boost::shared_ptr<XMLwrapper>(
        new XMLwrapper());
    if(!xmlwrap->putXMLdata(data)) {
        Runtime.Log("SynthEngine putXMLdata failed");
        return;
    }
    if(xmlwrap->enterbranch("MASTER")) {
        getfromXML(xmlwrap.get());
        xmlwrap->exitbranch();
    }
    else
        Runtime.Log("Master putAllData failed to enter MASTER branch");
}


bool SynthEngine::saveXML(string filename)
{
    boost::shared_ptr<XMLwrapper> xmlwrap = boost::shared_ptr<XMLwrapper>(
        new XMLwrapper());
    add2XML(xmlwrap.get());
    return xmlwrap->saveXMLfile(filename);
}


bool SynthEngine::loadXML(string filename)
{
    boost::shared_ptr<XMLwrapper> xmlwrap = boost::shared_ptr<XMLwrapper>(
        new XMLwrapper());
    if(!xmlwrap->loadXMLfile(filename))
        return false;
    return getfromXML(xmlwrap.get());
}


bool SynthEngine::getfromXML(XMLwrapper *xml)
{
    __sync_or_and_fetch(&synthMuted, 0xFF);
    Defaults();
    if(!xml->enterbranch("MASTER")) {
        Runtime.Log("SynthEngine getfromXML, no MASTER branch");
        return false;
    }
    setPvolume(xml->getpar127("volume", Pvolume));
    setPkeyshift(xml->getpar127("key_shift", Pkeyshift));

    part[0]->Penabled = 0;
    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if(xml->enterbranch("PART", npart)) {
            part[npart]->getfromXML(xml);
            xml->exitbranch();
        }

    if(xml->enterbranch("MICROTONAL")) {
        microtonal.getfromXML(xml);
        xml->exitbranch();
    }

    sysefx[0]->changeeffect(0);
    if(xml->enterbranch("SYSTEM_EFFECTS")) {
        for(int nefx = 0; nefx < NUM_SYS_EFX; ++nefx) {
            if(!xml->enterbranch("SYSTEM_EFFECT", nefx))
                continue;
            if(xml->enterbranch("EFFECT")) {
                sysefx[nefx]->getfromXML(xml);
                xml->exitbranch();
            }

            for(int partefx = 0; partefx < NUM_MIDI_PARTS; ++partefx) {
                if(!xml->enterbranch("VOLUME", partefx))
                    continue;
                setPsysefxvol(partefx, nefx,
                              xml->getpar127("vol", Psysefxvol[partefx][nefx]));
                xml->exitbranch();
            }

            for(int tonefx = nefx + 1; tonefx < NUM_SYS_EFX; ++tonefx) {
                if(!xml->enterbranch("SENDTO", tonefx))
                    continue;
                setPsysefxsend(nefx, tonefx,
                               xml->getpar127("send_vol",
                                              Psysefxsend[nefx][tonefx]));
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if(xml->enterbranch("INSERTION_EFFECTS")) {
        for(int nefx = 0; nefx < NUM_INS_EFX; ++nefx) {
            if(!xml->enterbranch("INSERTION_EFFECT", nefx))
                continue;
            Pinsparts[nefx] = xml->getpar("part",
                                          Pinsparts[nefx],
                                          -2,
                                          NUM_MIDI_PARTS);
            if(xml->enterbranch("EFFECT")) {
                insefx[nefx]->getfromXML(xml);
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }
    xml->exitbranch(); // MASTER
    __sync_and_and_fetch(&synthMuted, 0);
    return true;
}
