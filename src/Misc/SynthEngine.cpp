/*
    SynthEngine.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris

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

    This file is derivative of original ZynAddSubFX code, modified March 2011
*/

using namespace std;

#include "MasterUI.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"

SynthEngine *synth = NULL;

char SynthEngine::random_state[256] = { 0, };
struct random_data SynthEngine::random_buf;

SynthEngine::SynthEngine() :
    shutup(false),
    samplerate(48000),
    samplerate_f(samplerate),
    halfsamplerate_f(samplerate / 2),
    buffersize(256),
    buffersize_f(buffersize),
    oscilsize(1024),
    oscilsize_f(oscilsize),
    halfoscilsize(oscilsize / 2),
    halfoscilsize_f(halfoscilsize),
    ctl(NULL),
    fft(NULL),
    muted(0xFF),
    tmpmixl(NULL),
    tmpmixr(NULL),
    processLock(NULL),
    meterLock(NULL),
    stateXMLtree(NULL)
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
        fftwf_free(tmpmixl);
    if (tmpmixr)
        fftwf_free(tmpmixr);
    if (fft)
        delete fft;
    pthread_mutex_destroy(&processMutex);
    pthread_mutex_destroy(&meterMutex);
    if (ctl)
        delete ctl;
}


bool SynthEngine::Init(unsigned int audiosrate, int audiobufsize)
{
    samplerate_f = samplerate = audiosrate;
    halfsamplerate_f = samplerate / 2;
    buffersize_f = buffersize = audiobufsize;
    bufferbytes = buffersize * sizeof(float);
    oscilsize_f = oscilsize = Runtime.Oscilsize;
    halfoscilsize_f = halfoscilsize = oscilsize / 2;

    if (!pthread_mutex_init(&processMutex, NULL))
        processLock = &processMutex;
    else
    {
        Runtime.Log("SynthEngine actionLock init fails :-(");
        processLock = NULL;
        goto bail_out;
    }

    if (!pthread_mutex_init(&meterMutex, NULL))
        meterLock = &meterMutex;
    else
    {
        Runtime.Log("SynthEngine meterLock init fails :-(");
        meterLock = NULL;
        goto bail_out;
    }

    if (initstate_r(samplerate + buffersize + oscilsize, random_state,
                    sizeof(random_state), &random_buf))
        Runtime.Log("SynthEngine Init failed on general randomness");

    if (oscilsize < (buffersize / 2))
    {
        Runtime.Log("Enforcing oscilsize to half buffersize, "
                    + asString(oscilsize) + " -> " + asString(buffersize / 2));
        oscilsize = buffersize / 2;
    }

    if (!(fft = new FFTwrapper(oscilsize)))
    {
        Runtime.Log("SynthEngine failed to allocate fft");
        goto bail_out;
    }

     tmpmixl = (float*)fftwf_malloc(synth->bufferbytes);
     tmpmixr = (float*)fftwf_malloc(synth->bufferbytes);
    if (!tmpmixl || !tmpmixr)
    {
        Runtime.Log("SynthEngine tmpmix allocations failed");
        goto bail_out;
    }

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart] = new Part(&microtonal, fft);
        if (!part[npart])
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
        if (!(insefx[nefx] = new EffectMgr(1)))
        {
            Runtime.Log("Failed to allocate new Insertion EffectMgr");
            goto bail_out;
        }
    }

    // System Effects init
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        if (!(sysefx[nefx] = new EffectMgr(0)))
        {
            Runtime.Log("Failed to allocate new System Effects EffectMgr");
            goto bail_out;
        }
    }
    defaults();
    if (Runtime.restoreJackSession)
    {
        if (!Runtime.restoreJsession())
        {
            Runtime.Log("Restore jack session failed");
            goto bail_out;
        }
    }
    else if (Runtime.restoreState)
    {
        if (!Runtime.stateRestore())
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
    if (fft)
        delete fft;
    fft = NULL;
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


void SynthEngine::defaults(void)
{
    setPvolume(90);
    setPkeyshift(64);
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart]->defaults();
        part[npart]->Prcvchn = npart % NUM_MIDI_CHANNELS;
    }
    partonoff(0, 1); // enable the first part
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


// Note On Messages (velocity == 0 => NoteOff)
void SynthEngine::NoteOn(unsigned char chan, unsigned char note, unsigned char velocity)
{
    if (!velocity)
        this->NoteOff(chan, note);
    else if (!isMuted())
        for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        {
            if (chan == part[npart]->Prcvchn)
            {
                fakepeakpart[npart] = velocity * 2;
                if (part[npart]->Penabled)
                {
                    actionLock(lock);
                    part[npart]->NoteOn(note, velocity, keyshift);
                    actionLock(unlock);
                }
            }
        }
}


// Note Off Messages
void SynthEngine::NoteOff(unsigned char chan, unsigned char note)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (chan == part[npart]->Prcvchn && part[npart]->Penabled)
        {
            actionLock(lock);
            part[npart]->NoteOff(note);
            actionLock(unlock);
        }
    }
}


// Controllers
void SynthEngine::SetController(unsigned char chan, unsigned int type, short int par)
{
    if (type == Runtime.midi_bank_C) {
        /*  we use either msb or lsb for bank changes
	  128 banks is enough for anybody :-)
        this is configurable to suit different hardware synths
        */
        Runtime.Log("SynthEngine setController bankselect: " + asString(par));
        bank.msb = (unsigned char)par + 1; //Bank indexes start from 1       
        if(bank.msb <= MAX_NUM_BANKS) {
            bank.loadbank(bank.banks[bank.msb].dir);
            Runtime.Log("SynthEngine setController bankselect: Bank loaded " + bank.banks[bank.msb].name);
        } else
            Runtime.Log("SynthEngine setProgram: Value is out of range!");
        return;        
    }
    else{ // bank change doesn't directly affect parts.
	for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
	{   // Send the controller to all part assigned to the channel
	    if (chan == part[npart]->Prcvchn && part[npart]->Penabled)
		part[npart]->SetController(type, par);
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

void SynthEngine::SetProgram(unsigned char chan, unsigned char pgm)
{
    for(int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if(chan == part[npart]->Prcvchn)
        {
            if (part[npart]->Penabled == 0 and Runtime.enable_part_on_voice_load != 0)
            {
                partonoff(npart, 1);
            }
            bank.loadfromslot(pgm, part[npart]); //Programs indexes start from 0
        }
    Runtime.Log("SynthEngine setProgram: Program loaded " + bank.getname(pgm));
    //update UI
    guiMaster->updatepanel();
    if (guiMaster->partui && guiMaster->partui->instrumentlabel && guiMaster->partui->part) {
        guiMaster->partui->instrumentlabel->copy_label(guiMaster->partui->part->Pname.c_str());
    }
}

// Enable/Disable a part
void SynthEngine::partonoff(int npart, int what)
{
    if (npart >= NUM_MIDI_PARTS)
        return;
    fakepeakpart[npart] = 0;
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
}


// Master audio out (the final sound)
void SynthEngine::MasterAudio(float *outl [NUM_MIDI_PARTS], float *outr [NUM_MIDI_PARTS])
{
    int npart;
    for (npart = 0; npart < (NUM_MIDI_PARTS + 1); ++npart)
    {
        memset(outl[npart], 0, bufferbytes);
        memset(outr[npart], 0, bufferbytes);
    }
    if (isMuted())
        return;

    actionLock(lock);

    // Compute part samples and store them ->partoutl,partoutr
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if (part[npart]->Penabled)
        {
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
            {
                insefx[nefx]->out(part[efxpart]->partoutl, part[efxpart]->partoutr);
            }
        }
    }

    // Apply the part volumes and pannings (after insertion effects)
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (!part[npart]->Penabled)
            continue;

        float oldvol_l = part[npart]->oldvolumel;
        float oldvol_r = part[npart]->oldvolumer;
        float newvol_l = part[npart]->pannedVolLeft();
        float newvol_r = part[npart]->pannedVolRight();
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
        {
            // skip if part is disabled or has no output to effect
            if (part[npart]->Penabled && Psysefxvol[nefx][npart])
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
            outl[NUM_MIDI_PARTS][i] += tmpmixl[i] * outvol;
            outr[NUM_MIDI_PARTS][i] += tmpmixr[i] * outvol;
        }
    }

    // Copy all parts
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        for (int i = 0; i < buffersize; ++i)
        {
            outl[npart][i] = part[npart]->partoutl[i];
            outr[npart][i] = part[npart]->partoutr[i];
        }
    }

    // Mix all parts to mixed outputs
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        for (int i = 0; i < buffersize; ++i)
        {   // the volume did not change
            outl[NUM_MIDI_PARTS][i] += outl[npart][i];
            outr[NUM_MIDI_PARTS][i] += outr[npart][i];
        }
    }

    // Insertion effects for Master Out
    for (nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        if (Pinsparts[nefx] == -2)
        {
            insefx[nefx]->out(outl[NUM_MIDI_PARTS], outr[NUM_MIDI_PARTS]);
        }
    }

    actionLock(unlock);

    LFOParams::time++; // update the LFO's time

    vupeakLock(lock);
    vuoutpeakl = 1e-12f;
    vuoutpeakr = 1e-12f;
    vurmspeakl = 1e-12f;
    vurmspeakr = 1e-12f;
    vupeakLock(unlock);

    float absval;
    //Per output master volume and fade
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        for (int idx = 0; idx < buffersize; ++idx)
        {
            outl[npart][idx] *= volume; // apply Master Volume
            outr[npart][idx] *= volume;

            if (shutup) // fade-out
            {
                float fade = (float) (buffersize - idx) / (float) buffersize;
                outl[npart][idx] *= fade;
                outr[npart][idx] *= fade;
            }
        }
    }

    //Master volume and clip calculation for mixed outputs
    for (int idx = 0; idx < buffersize; ++idx)
    {
        outl[NUM_MIDI_PARTS][idx] *= volume; // apply Master Volume
        outr[NUM_MIDI_PARTS][idx] *= volume;

        if ((absval = fabsf(outl[NUM_MIDI_PARTS][idx])) > vuoutpeakl) // Peak computation (for vumeters)
            vuoutpeakl = absval;
        if ((absval = fabsf(outr[NUM_MIDI_PARTS][idx])) > vuoutpeakr)
            vuoutpeakr = absval;
        vurmspeakl += outl[NUM_MIDI_PARTS][idx] * outl[NUM_MIDI_PARTS][idx]; // RMS Peak
        vurmspeakr += outr[NUM_MIDI_PARTS][idx] * outr[NUM_MIDI_PARTS][idx];

        // check for clips
        if (outl[NUM_MIDI_PARTS][idx] > 1.0f)
            clippedL = true;
        else if (outl[NUM_MIDI_PARTS][idx] < -1.0f)
            clippedL = true;
        if (outr[NUM_MIDI_PARTS][idx] > 1.0f)
            clippedR = true;
        else if (outr[NUM_MIDI_PARTS][idx] < -1.0f)
            clippedR = true;

        if (shutup) // fade-out
        {
            float fade = (float) (buffersize - idx) / (float) buffersize;
            outl[NUM_MIDI_PARTS][idx] *= fade;
            outr[NUM_MIDI_PARTS][idx] *= fade;
        }
    }
    if (shutup)
        ShutUp();

    vupeakLock(lock);
    if (vumaxoutpeakl < vuoutpeakl)
        vumaxoutpeakl = vuoutpeakl;
    if (vumaxoutpeakr < vuoutpeakr)
        vumaxoutpeakr = vuoutpeakr;

    vurmspeakl = sqrtf(vurmspeakl / buffersize);
    vurmspeakr = sqrtf(vurmspeakr / buffersize);

    // Part Peak computation (for Part vu meters/fake part vu meters)
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        vuoutpeakpart[npart] = 1.0e-12;
        if (part[npart]->Penabled)
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
    vuOutPeakL = vuoutpeakl;
    vuOutPeakR = vuoutpeakr;
    vuMaxOutPeakL = vumaxoutpeakl;
    vuMaxOutPeakR = vumaxoutpeakr;
    vuRmsPeakL = vurmspeakl;
    vuRmsPeakR = vurmspeakr;
    vuClippedL = clippedL;
    vuClippedR = clippedR;
    vupeakLock(unlock);
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


bool SynthEngine::vupeakLock(lockset request)
{
    int chk  = -1;
    switch (request)
    {
        case lock:
            chk = pthread_mutex_lock(meterLock);
            break;

        case unlock:
            chk = pthread_mutex_unlock(meterLock);
            break;

        default:
            break;
    }
    return (chk == 0) ? true : false;
}


// Reset peaks and clear the "clipped" flag (for VU-meter)
void SynthEngine::vuresetpeaks(void)
{
    vupeakLock(lock);
    vuOutPeakL = vuoutpeakl = 1e-12f;
    vuOutPeakR = vuoutpeakr =  1e-12f;
    vuMaxOutPeakL = vumaxoutpeakl = 1e-12f;
    vuMaxOutPeakR = vumaxoutpeakr = 1e-12f;
    vuRmsPeakL = vurmspeakl = 1e-12f;
    vuRmsPeakR = vurmspeakr = 1e-12f;
    vuClippedL = false;
    vuClippedL = false;
    clippedL = false;
    clippedR = false;
    vupeakLock(unlock);
}


void SynthEngine::applyparameters(void)
{
    ShutUp();
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart]->applyparameters();
}


void SynthEngine::add2XML(XMLwrapper *xml)
{
    xml->beginbranch("MASTER");
    actionLock(lockmute);
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
        actionLock(lock);
        getfromXML(xml);
        actionLock(unlock);
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


