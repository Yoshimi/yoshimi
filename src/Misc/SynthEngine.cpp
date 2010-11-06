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

    This file is derivative of ZynAddSubFX original code, modified 2010
*/

#include <iostream>

#include "MasterUI.h"
#include "Sql/ProgramBanks.h"
#include "Misc/SynthEngine.h"

SynthEngine *synth = NULL;

char SynthEngine::random_state[256] = { 0, };
struct random_data SynthEngine::random_buf;

SynthEngine::SynthEngine() :
    shutup(false),
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
    lockwait(boost::posix_time::microsec(666u))
{
    ctl = new Controller();
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
        delete [] tmpmixl;
    if (tmpmixr)
        delete [] tmpmixr;
    if (fft)
        delete fft;
    if (ctl)
        delete ctl;
}


bool SynthEngine::Init(unsigned int audiosrate, int audiobufsize)
{
    if (initstate_r(samplerate + buffersize + oscilsize, random_state,
                    sizeof(random_state), &random_buf))
        Runtime.Log("SynthEngine Init failed on general randomness");

    samplerate_f = samplerate = audiosrate;
    halfsamplerate_f = samplerate / 2;
    buffersize_f = buffersize = audiobufsize;
    bufferbytes = buffersize * sizeof(float);
    oscilsize_f = oscilsize = Runtime.Oscilsize;
    halfoscilsize_f = halfoscilsize = oscilsize / 2;

    if (oscilsize < (buffersize / 2))
    {
        Runtime.Log("Enforcing oscilsize to half buffersize, "
                    + asString(oscilsize) + " -> " + asString(buffersize / 2));
        oscilsize = buffersize / 2;
    }
    
    if ((fft = new FFTwrapper(oscilsize)) == NULL)
    {
        Runtime.Log("SynthEngine failed to allocate fft");
        goto bail_out;
    }

    tmpmixl = new float[buffersize];
    tmpmixr = new float[buffersize];
    if (tmpmixl == NULL || tmpmixr == NULL)
    {
        Runtime.Log("SynthEngine tmpmix allocations failed");
        goto bail_out;
    }

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart] = new Part(&microtonal, fft);
        if (NULL == part[npart])
        {
            Runtime.Log("Failed to allocate new Part");
            goto bail_out;
        }
        vuoutpeakpart[npart] = 1e-9f;
        fakepeakpart[npart] = 0;
    }

    // Insertion Effects init
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        insefx[nefx] = new EffectMgr(1);
        if (NULL == insefx[nefx])
        {
            Runtime.Log("Failed to allocate new Insertion EffectMgr");
            goto bail_out;
        }
    }

    // System Effects init
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        sysefx[nefx] = new EffectMgr(0);
        if (NULL == sysefx[nefx])
        {
            Runtime.Log("Failed to allocate new System Effects EffectMgr");
            goto bail_out;
        }
    }
    defaults();
    if (Runtime.doRestoreJackSession)
    {
        if (!Runtime.restoreJsession(this))
        {
            Runtime.Log("Restore jack session failed");
            goto bail_out;
        }
    }
    else if (Runtime.doRestoreState)
    {
        if (!Runtime.restoreState(this))
         {
             Runtime.Log("Restore state failed");
             goto bail_out;
         }
    }
    else
    {
        if (Runtime.paramsLoad.size())
        {
            if (loadXML(Runtime.paramsLoad) >= 0)
            {
                applyparameters();
                Runtime.paramsLoad = Runtime.addParamHistory(Runtime.paramsLoad);
                Runtime.Log("Loaded " + Runtime.paramsLoad + " parameters");
            }
            else
            {
                Runtime.Log("Failed to load parameters " + Runtime.paramsLoad);
                goto bail_out;
            }
        }
        if (!Runtime.instrumentLoad.empty())
        {
            int loadtopart = 0;
            if (!part[loadtopart]->loadXMLinstrument(Runtime.instrumentLoad))
            {
                Runtime.Log("Failed to load instrument file " + Runtime.instrumentLoad);
                goto bail_out;
            }
            else
                Runtime.Log("Instrument file " + Runtime.instrumentLoad + " loaded");
        }
    }
    return true;

bail_out:
    if (fft != NULL)
        delete fft;
    fft = NULL;
    if (tmpmixl != NULL)
        delete tmpmixl;
    tmpmixl = NULL;
    if (tmpmixr != NULL)
        delete tmpmixr;
    tmpmixr = NULL;
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (NULL != part[npart])
            delete part[npart];
        part[npart] = NULL;
    }
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        if (NULL != insefx[nefx])
            delete insefx[nefx];
        insefx[nefx] = NULL;
    }
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        if (NULL != sysefx[nefx])
            delete sysefx[nefx];
        sysefx[nefx] = NULL;
    }
    return false;
}


void SynthEngine::defaults(void)
{
    setPvolume(90);
    setPkeyshift(64);
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart]->defaults();
        part[npart]->midichannel = npart % NUM_MIDI_CHANNELS;
    }
    partOnOff(0, 1); // enable the first part
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
    ShutUp();
}


void SynthEngine::noteOn(unsigned char chan, unsigned char note, unsigned char velocity)
{
    if (!velocity)
        noteOff(chan, note); // velocity 0 -> NoteOff
    else
    {
        if (recordPending && musicClient->recordTrigger())
            guiMaster->record_activated();
        for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        {
            if (part[npart]->Penabled && chan == part[npart]->midichannel)
            {
                lockSharable();
                part[npart]->NoteOn(note, velocity, keyshift);
                unlockSharable();
            }
        }
    }
}


void SynthEngine::noteOff(unsigned char chan, unsigned char note)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (part[npart]->Penabled && chan == part[npart]->midichannel)
        {
            lockSharable();
            part[npart]->NoteOff(note);
            unlockSharable();
        }
    }
}


void SynthEngine::setPitchwheel(unsigned char chan, short int par)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if (part[npart]->Penabled && chan == part[npart]->midichannel)
            part[npart]->ctl->setpitchwheel(par);
}


void SynthEngine::setController(unsigned char ctrltype, unsigned char channel, unsigned char val)
{
    switch (ctrltype)
    {
        case C_modwheel:             //   1
        case C_volume:               //   7
        case C_pan:                  //  10
        case C_expression:           //  11
        case C_sustain:              //  64
        case C_portamento:           //  65
        case C_filterq:              //  71
        case C_filtercutoff:         //  74
        case C_soundcontroller6:     //  75 bandwidth
        case C_soundcontroller7:     //  76 fmamp
        case C_soundcontroller8:     //  77 resonance center
        case C_soundcontroller9:     //  78 resonance bandwidth
        case C_allsoundsoff:         // 120
        case C_resetallcontrollers:  // 121
        case C_allnotesoff:          // 123
            for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
                if (part[npart]->Penabled && channel == part[npart]->midichannel)
                    part[npart]->SetController(ctrltype, val);
                    // Send the controller to all active parts assigned to the channel
            break;

        case C_bankselectmsb:
            midiBankMSB = val;
            break;

        case C_bankselectlsb:
            midiBankLSB = val;
            break;

        default:
            Runtime.Log(string("Ignoring midi control change type ") + asString(ctrltype), true);
            break;
    }

    if (ctrltype == C_allsoundsoff)
    {   // cleanup insertion/system FX
        synth->lockSharable();
        for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
            sysefx[nefx]->cleanup();
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
            insefx[nefx]->cleanup();
        synth->unlockSharable();
    }
}


void SynthEngine::applyMidi(unsigned char* bytes)
{
    unsigned char channel = bytes[0] & 0x0f;
    char selectbank = -1;

    switch (bytes[0] & 0xf0)
    {
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
            selectbank = (midiBankLSB < 0) ? midiBankMSB : midiBankLSB;
            if (selectbank < 0)
                Runtime.Log("Invalid bank selection for midi program");
            else
            {
                if (!part[channel]->loadProgram(selectbank, bytes[1]))
                    Runtime.Log("Midi program change failed");
            }
            midiBankMSB = midiBankLSB = -1;
            break;

        case MSG_pitchwheel_control: // 224
            for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
                if (part[npart]->Penabled && channel == part[npart]->midichannel)
                    part[npart]->ctl->setpitchwheel(((bytes[2] << 7) | bytes[1]) - 8192);
            break;

        default: // too difficult or just uninteresting
            break;
    }
}


// Enable/Disable a part
void SynthEngine::partOnOff(int npart, int what)
{
    if (npart >= NUM_MIDI_PARTS)
        return;
    fakepeakpart[npart] = 0;
    lockSharable();
    if (what)
        part[npart]->Penabled = 1;
    else
    {   // disabled part
        part[npart]->Penabled = 0;
        part[npart]->cleanup();
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
            if (Pinsparts[nefx] == npart)
                insefx[nefx]->cleanup();
    }
    unlockSharable();
}


// Enable/Disable a part
void SynthEngine::partEnable(unsigned char npart, bool maybe)
{
    if (npart <= NUM_MIDI_PARTS)
    {
        if (!(part[npart]->Penabled = (maybe) ? 1 : 0))
        {   // disabled part
            part[npart]->cleanup();
            for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
                if (Pinsparts[nefx] == npart)
                    insefx[nefx]->cleanup();
        }
    }
}


// Master audio out (the final sound)
void SynthEngine::MasterAudio(float *outl, float *outr)
{
    memset(outl, 0, bufferbytes);
    memset(outr, 0, bufferbytes);
    // Compute part samples and store them npart]->partoutl, partoutr
    int npart;
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if (part[npart]->Active())
        {
            //if (trylockExclusive())
            if (timedlockExclusive())
            {
                part[npart]->ComputePartSmps();
                unlockExclusive();
            }
            else
            {
                memset(part[npart]->partoutl, 0, bufferbytes);
                memset(part[npart]->partoutr, 0, bufferbytes);
            }
        }

    // Insertion effects
    int nefx;
    for (nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        if (Pinsparts[nefx] >= 0)
        {
            int efxpart = Pinsparts[nefx];
            if (part[efxpart]->Active())
                insefx[nefx]->out(part[efxpart]->partoutl, part[efxpart]->partoutr);
        }
    }

    // Apply the part volumes and pannings (after insertion effects)
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if (part[npart]->Active())
        {
            float newvol_l = part[npart]->volume;
            float newvol_r = part[npart]->volume;
            float oldvol_l = part[npart]->oldvolumel;
            float oldvol_r = part[npart]->oldvolumer;
            float pan = part[npart]->panning;
            if (pan < 0.5)
                newvol_l *= (1.0 - pan) * 2.0;
            else
                newvol_r *= pan * 2.0;

            if (aboveAmplitudeThreshold(oldvol_l, newvol_l) || aboveAmplitudeThreshold(oldvol_r, newvol_r))
            {   // the volume or the panning has changed and needs interpolation
                for (int i = 0; i < buffersize; ++i)
                {
                    float vol_l = interpolateAmplitude(oldvol_l, newvol_l, i, buffersize);
                    float vol_r = interpolateAmplitude(oldvol_r, newvol_r, i, buffersize);
                    part[npart]->partoutl[i] *= vol_l;
                    part[npart]->partoutr[i] *= vol_r;
                }
                part[npart]->oldvolumel = newvol_l;
                part[npart]->oldvolumer = newvol_r;
            }
            else
            {
                for (int i = 0; i < buffersize; ++i)
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
        memset(tmpmixl, 0, bufferbytes);
        memset(tmpmixr, 0, bufferbytes);

        // Mix the channels according to the part settings about System Effect
        for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
            if (part[npart]->Active() && Psysefxvol[nefx][npart])
            {   // skip if part is disabled or has no output to effect
                // the output volume of each part to system effect
                float vol = sysefxvol[nefx][npart];
                for (int i = 0; i < buffersize; ++i)
                {
                    tmpmixl[i] += part[npart]->partoutl[i] * vol;
                    tmpmixr[i] += part[npart]->partoutr[i] * vol;
                }
            }

        // system effect send to next ones
        for (int nefxfrom = 0; nefxfrom < nefx; ++nefxfrom)
            if (Psysefxsend[nefxfrom][nefx])
            {
                float v = sysefxsend[nefxfrom][nefx];
                for (int i = 0; i < buffersize; ++i)
                {
                    tmpmixl[i] += sysefx[nefxfrom]->efxoutl[i] * v;
                    tmpmixr[i] += sysefx[nefxfrom]->efxoutr[i] * v;
                }
            }

        sysefx[nefx]->out(tmpmixl, tmpmixr);

        // Add the System Effect to sound output
        float outvol = sysefx[nefx]->sysefxgetvolume();
        for (int i = 0; i < buffersize; ++i)
        {
            outl[i] += tmpmixl[i] * outvol;
            outr[i] += tmpmixr[i] * outvol;
       }
    }

    // Mix all parts
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
         if (part[npart]->Active())
             for (int i = 0; i < buffersize; ++i)
             {   // the volume did not change
                 outl[i] += part[npart]->partoutl[i];
                 outr[i] += part[npart]->partoutr[i];
             }

    // Insertion effects for Master Out
    for (nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        if (Pinsparts[nefx] == -2)
        {
            lockSharable();
            insefx[nefx]->out(outl, outr);
            unlockSharable();
        }

    meterMutex.lock();
    vuoutpeakl = 1e-12f;
    vuoutpeakr = 1e-12f;
    vurmspeakl = 1e-12f;
    vurmspeakr = 1e-12f;
    meterMutex.unlock();

    float absval;
    for (int idx = 0; idx < buffersize; ++idx)
    {
        outl[idx] *= volume; // apply Master Volume
        outr[idx] *= volume;

        if ((absval = fabsf(outl[idx])) > vuoutpeakl) // Peak computation (for vumeters)
            vuoutpeakl = absval;
        if ((absval = fabsf(outr[idx])) > vuoutpeakr)
            vuoutpeakr = absval;
        vurmspeakl += outl[idx] * outl[idx];  // RMS Peak
        vurmspeakr += outr[idx] * outr[idx];

        if (outl[idx] > 1.0f)
            clippedL = true;
         else if (outl[idx] < -1.0f)
            clippedL = true;
        if (outr[idx] > 1.0f)
            clippedR = true;
         else if (outr[idx] < -1.0f)
            clippedR = true;

        if (shutup) // fade-out
        {
            float fade = (float)(buffersize - idx) / (float)buffersize;
            outl[idx] *= fade;
            outr[idx] *= fade;
        }
    }
    if (shutup)
        ShutUp();
    synthperiodStartFrame += buffersize;
    LFOParams::time++; // update the LFO's time

    meterMutex.lock();
    if (vumaxoutpeakl < vuoutpeakl)  vumaxoutpeakl = vuoutpeakl;
    if (vumaxoutpeakr < vuoutpeakr)  vumaxoutpeakr = vuoutpeakr;

    vurmspeakl = sqrtf(vurmspeakl / buffersize);
    vurmspeakr = sqrtf(vurmspeakr / buffersize);

    // Part Peak computation (for Part vu meters/fake part vu meters)
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        vuoutpeakpart[npart] = 1.0e-12f;
        if (part[npart]->Active())
        {
            float *outl = part[npart]->partoutl;
            float *outr = part[npart]->partoutr;
            for (int i = 0; i < buffersize; ++i)
            {
                float tmp = fabsf(outl[i] + outr[i]);
                if (tmp > vuoutpeakpart[npart])
                    vuoutpeakpart[npart] = tmp;
            }
            vuoutpeakpart[npart] *= volume; // how is part peak related to master volume??
        }
        else if (fakepeakpart[npart] > 1)
            fakepeakpart[npart]--;
    }
    vuOutPeakL =    vuoutpeakl;
    vuOutPeakR =    vuoutpeakr;
    vuMaxOutPeakL = vumaxoutpeakl;
    vuMaxOutPeakR = vumaxoutpeakr;
    vuRmsPeakL =    vurmspeakl;
    vuRmsPeakR =    vurmspeakr;
    vuClippedL =    clippedL;
    vuClippedR =    clippedR;
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
    keyshift = (int)Pkeyshift - 64;
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
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart]->cleanup();
        fakepeakpart[npart] = 0;
    }
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx]->cleanup();
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx]->cleanup();
    vuresetpeaks();
    shutup = false;
}


// Reset peaks and clear the "clipped" flag (for VU-meter)
void SynthEngine::vuresetpeaks(void)
{
    meterMutex.lock();
    vuOutPeakL = vuoutpeakl = 1e-12f;
    vuOutPeakR = vuoutpeakr =  1e-12f;
    vuMaxOutPeakL = vumaxoutpeakl = 1e-12f;
    vuMaxOutPeakR = vumaxoutpeakr = 1e-12f;
    vuRmsPeakL = vurmspeakl = 1e-12f;
    vuRmsPeakR = vurmspeakr = 1e-12f;
    vuClippedL = vuClippedL = clippedL = clippedR = false;
    meterMutex.unlock();
}


/**
bool SynthEngine::loadProgram(int partnum, unsigned char bk, unsigned char prog)
{
    return part[partnum]->loadProgram(bk, prog);
}
**/


void SynthEngine::lockUpgradable(void)
{
    using namespace boost::interprocess;
    try { synthMutex. lock_upgradable(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::lockUpgradable throws exception!");
    }
}


void SynthEngine::unlockUpgradable(void)
{
    using namespace boost::interprocess;
    try { synthMutex.unlock_upgradable(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::unlockUpgradable throws exception!");
    }
}


void SynthEngine::upgradeLockExclusive(void)
{
    using namespace boost::interprocess;
    try { synthMutex.unlock_upgradable_and_lock(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::lockUpgradeExclusive throws exception!");
    }
}


void SynthEngine::downgradeLockUpgradable(void)
{
    using namespace boost::interprocess;
    try { synthMutex.unlock_and_lock_upgradable(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::lockUpgradeExclusive throws exception!");
    }
}


bool SynthEngine::timedlockUpgradable(void)
{
    bool ok = false;
    boost::posix_time::ptime endtime = boost::posix_time::microsec_clock::local_time() + lockwait;
    using namespace boost::interprocess;
    try { ok = synthMutex.timed_lock_upgradable(endtime); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::timedlockUpgradable throws exception!");
        ok = false;
    }
    return ok;
}


bool SynthEngine::timedUpgradeLockExclusive(void)
{
    bool ok = false;
    boost::posix_time::ptime endtime = boost::posix_time::microsec_clock::local_time() + lockwait;
    using namespace boost::interprocess;
    try { ok = synthMutex.timed_lock_upgradable(endtime); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::timedlockUpgradable throws exception!");
        ok = false;
    }
    return ok;
}


void SynthEngine::lockExclusive(void)
{
    using namespace boost::interprocess;
    try { synthMutex.lock(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::lockExclusive throws exception!");
    }
}


void SynthEngine::unlockExclusive(void)
{
    using namespace boost::interprocess;
    try { synthMutex.unlock(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::unlockExclusive throws exception!");
    }
}


bool SynthEngine::trylockExclusive(void)
{
    bool ok = false;
    using namespace boost::interprocess;
    try { ok = synthMutex.try_lock(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::trylockExclusive throws exception!");
    }
    return ok;
}


bool SynthEngine::timedlockExclusive(void)
{
    bool ok = false;
    boost::posix_time::ptime endtime = boost::posix_time::microsec_clock::local_time() + lockwait;
    using namespace boost::interprocess;
    try { ok = synthMutex.timed_lock(endtime); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::timedlockExclusive throws exception!");
        ok = false;
    }
    return ok;
}


void SynthEngine::lockSharable(void)
{
    using namespace boost::interprocess;
    try { synthMutex.lock_sharable(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::lockSharable throws exception!");
    }
}


void SynthEngine::unlockSharable(void)
{
    using namespace boost::interprocess;
    try { synthMutex.unlock_sharable(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::unlockSharable throws exception!");
    }
}


bool SynthEngine::trylockSharable(void)
{
    bool ok = false;
    using namespace boost::interprocess;
    try {ok = synthMutex.try_lock_sharable(); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::trylockSharable throws exception!");
    }
    return ok;
}


bool SynthEngine::timedlockSharable(void)
{
    bool ok = false;
    boost::posix_time::ptime endtime = boost::posix_time::microsec_clock::local_time() + lockwait;
    using namespace boost::interprocess;
    try { ok = synthMutex.timed_lock_sharable(endtime); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("SynthEngine::timedlockSharable throws exception!");
        ok = false;
    }
    return ok;
}


void SynthEngine::applyparameters(void)
{
    ShutUp();
}


void SynthEngine::add2XML(XMLwrapper *xml)
{
    xml->beginbranch("MASTER");
    lockSharable();
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
    unlockSharable();
    xml->endbranch(); // MASTER
}


int SynthEngine::getalldata(char **data)
{
    XMLwrapper *xml = new XMLwrapper();
    add2XML(xml);
    *data = xml->getXMLdata();
    delete xml;
    return strlen(*data) + 1;
}


void SynthEngine::putalldata(char *data, int size)
{
    XMLwrapper *xml = new XMLwrapper();
    if (!xml->putXMLdata(data))
    {
        Runtime.Log("SynthEngine putXMLdata failed");
        delete xml;
        return;
    }
    if (xml->enterbranch("MASTER"))
    {
        lockSharable();
        getfromXML(xml);
        unlockSharable();
        xml->exitbranch();
    }
    else
        Runtime.Log("Master putAllData failed to enter MASTER branch");
    delete xml;
}


bool SynthEngine::saveXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper();
    add2XML(xml);
    bool result = xml->saveXMLfile(filename);
    delete xml;
    return result;
}


bool SynthEngine::loadXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper();
    if (NULL == xml)
    {
        Runtime.Log("failed to init xml tree");
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
    if (!xml->enterbranch("MASTER"))
    {
        Runtime.Log("SynthEngine getfromXML, no MASTER branch");
        return false;
    }
    setPvolume(xml->getpar127("volume", Pvolume));
    setPkeyshift(xml->getpar127("key_shift", Pkeyshift));

    part[0]->Penabled = 0;
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (!xml->enterbranch("PART", npart))
            continue;
        part[npart]->getfromXML(xml);
        xml->exitbranch();
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
