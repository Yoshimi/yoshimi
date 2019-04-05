/*
    Master.cpp

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

    This file is a derivative of the ZynAddSubFX original, modified January 2010
*/

//#include <iostream>

using namespace std;

#include "GuiThreadUI.h"
#include "Misc/Master.h"

Master *zynMaster = NULL;

int Master::muted = 0;

char Master::random_state[256] = { 0, };
struct random_data Master::random_buf;

Master::Master() :
    shutup(false),
    fft(NULL),
    recordPending(false),
    samplerate(0),
    buffersize(0),
    oscilsize(0),
    processLock(NULL),
    tmpmixl(NULL),
    tmpmixr(NULL),
    stateXMLtree(NULL)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart] = NULL;
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx] = NULL;
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx] = NULL;
    shutup = false;
}


Master::~Master()
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if (NULL != part[npart])
            delete part[npart];
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        if (NULL != insefx[nefx])
            delete insefx[nefx];
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        if (NULL != sysefx[nefx])
            delete sysefx[nefx];

    if (NULL != tmpmixl)
        delete [] tmpmixl;
    if (NULL != tmpmixr)
        delete [] tmpmixr;
    if (NULL != fft)
        delete fft;
    pthread_mutex_destroy(&processMutex);
    pthread_mutex_destroy(&meterMutex);
}


bool Master::Init(void)
{
    samplerate = Runtime.Samplerate;
    buffersize = Runtime.Buffersize;
    oscilsize = Runtime.Oscilsize;

    int chk;
    if (!(chk = pthread_mutex_init(&processMutex, NULL)))
        processLock = &processMutex;
    else
    {
        Runtime.Log("Master actionLock init fails :-(");
        processLock = NULL;
        goto bail_out;
    }
    if (!(chk = pthread_mutex_init(&meterMutex, NULL)))
        meterLock = &meterMutex;
    else
    {
        Runtime.Log("Master meterLock init fails :-(");
        meterLock = NULL;
        goto bail_out;
    }
    
    if (initstate_r(samplerate + buffersize + oscilsize, random_state,
                    sizeof(random_state), &random_buf))
        Runtime.Log("Master Init failed on general randomness");

    if (oscilsize < (buffersize / 2))
    {
        Runtime.Log("Enforcing oscilsize to half buffersize, "
                    + asString(oscilsize) + " -> " + asString(buffersize / 2));
        oscilsize = buffersize / 2;
    }

    if ((fft = new FFTwrapper(oscilsize)) == NULL)
    {
        Runtime.Log("Master failed to allocate fft");
        goto bail_out;
    }

    if (Runtime.restoreState)
    {
        if (NULL == (stateXMLtree = Runtime.RestoreRuntimeState()))
        {
            Runtime.Log("Restore runtime state failed");
            goto bail_out;
        }
    }

    tmpmixl = new float[buffersize];
    tmpmixr = new float[buffersize];
    if (tmpmixl == NULL || tmpmixr == NULL)
    {
        Runtime.Log("Master tmpmix allocations failed");
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
        vuoutpeakpart[npart] = 1e-9;
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

    if (Runtime.restoreState)
    {
        if (!getfromXML(stateXMLtree))
        {
            delete stateXMLtree;
            goto bail_out;
        }
        delete stateXMLtree;
    }
    else
    {
        if (Runtime.paramsLoad.size())
        {
            if (zynMaster->loadXML(Runtime.paramsLoad) >= 0)
            {
                zynMaster->applyparameters();
                Runtime.paramsLoad = Runtime.AddParamHistory(Runtime.paramsLoad);
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
            if (!zynMaster->part[loadtopart]->loadXMLinstrument(Runtime.instrumentLoad))
            {
                Runtime.Log("Failed to load instrument file " + Runtime.instrumentLoad);
                goto bail_out;
            }
            else
            {
                zynMaster->part[loadtopart]->applyparameters();
                Runtime.Log("Instrument file " + Runtime.instrumentLoad + " loaded");
            }
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


void Master::defaults(void)
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
void Master::NoteOn(unsigned char chan, unsigned char note,
                    unsigned char velocity, bool record_trigger)
{
    if (!velocity)
        this->NoteOff(chan, note);
    else if (!muted)
    {
        if (recordPending && record_trigger)
            guiMaster->record_activated();
        for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        {
            if (chan == part[npart]->Prcvchn)
            {
                fakepeakpart[npart] = velocity * 2;
                if (part[npart]->Penabled)
                {
                    zynMaster->actionLock(lock);
                    part[npart]->NoteOn(note, velocity, keyshift);
                    zynMaster->actionLock(unlock);
                }
            }
        }
    }
}


// Note Off Messages
void Master::NoteOff(unsigned char chan, unsigned char note)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (chan == part[npart]->Prcvchn && part[npart]->Penabled)
        {
            zynMaster->actionLock(lock);
            part[npart]->NoteOff(note);
            zynMaster->actionLock(unlock);
        }
    }
}


// Controllers
void Master::SetController(unsigned char chan, unsigned int type, short int par)
{
    if (type == C_dataentryhi
        || type == C_dataentrylo
        || type == C_nrpnhi
        || type == C_nrpnlo)
    {
        // Process RPN and NRPN by the Master (ignore the chan)
        ctl.setparameternumber(type, par);

        int parhi = -1, parlo = -1, valhi = -1, vallo = -1;
        if (!ctl.getnrpn(&parhi, &parlo, &valhi, &vallo))
        {   // this is NRPN
            // fprintf(stderr,"rcv. NRPN: %d %d %d %d\n",parhi,parlo,valhi,vallo);
            switch (parhi)
            {
                case 0x04: // System Effects
                    if (parlo < NUM_SYS_EFX)
                        sysefx[parlo]->seteffectpar_nolock(valhi, vallo);
                    break;
            case 0x08: // Insertion Effects
                if (parlo < NUM_INS_EFX)
                    insefx[parlo]->seteffectpar_nolock(valhi, vallo);
                break;

            }
        }
    }
    else
    {   // other controllers
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


// Enable/Disable a part
void Master::partonoff(int npart, int what)
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
void Master::MasterAudio(jsample_t *outl, jsample_t *outr)
{
    memset(outl, 0, buffersize * sizeof(jsample_t));
    memset(outr, 0, buffersize * sizeof(jsample_t));

    if (muted)
        return;

    // Compute part samples and store them npart]->partoutl,partoutr
    int npart;
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if (part[npart]->Penabled)
        {
            actionLock(lock);
            part[npart]->ComputePartSmps();
            actionLock(unlock);
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
                actionLock(lock);
                insefx[nefx]->out(part[efxpart]->partoutl, part[efxpart]->partoutr);
                actionLock(unlock);
            }
        }
    }

    // Apply the part volumes and pannings (after insertion effects)
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (!part[npart]->Penabled)
            continue;

        float newvol_l = part[npart]->volume;
        float newvol_r = part[npart]->volume;
        float oldvol_l = part[npart]->oldvolumel;
        float oldvol_r = part[npart]->oldvolumer;
        float pan = part[npart]->panning;
        if (pan < 0.5)
            newvol_l *= (1.0 - pan) * 2.0;
        else
            newvol_r *= pan * 2.0;

        actionLock(lock);
        if (AboveAmplitudeThreshold(oldvol_l, newvol_l)
            || AboveAmplitudeThreshold(oldvol_r, newvol_r))
        {   // the volume or the panning has changed and needs interpolation
            for (int i = 0; i < buffersize; ++i)
            {
                float vol_l = InterpolateAmplitude(oldvol_l, newvol_l, i,
                                                   buffersize);
                float vol_r = InterpolateAmplitude(oldvol_r, newvol_r, i,
                                                   buffersize);
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
        actionLock(unlock);
    }
    // System effects
    for (nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        if (!sysefx[nefx]->geteffect())
            continue; // is disabled

        // Clean up the samples used by the system effects
        memset(tmpmixl, 0, buffersize * sizeof(float));
        memset(tmpmixr, 0, buffersize * sizeof(float));

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
                    actionLock(lock);
                    tmpmixl[i] += part[npart]->partoutl[i] * vol;
                    tmpmixr[i] += part[npart]->partoutr[i] * vol;
                    actionLock(unlock);
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
                    actionLock(lock);
                    tmpmixl[i] += sysefx[nefxfrom]->efxoutl[i] * v;
                    tmpmixr[i] += sysefx[nefxfrom]->efxoutr[i] * v;
                    actionLock(unlock);
                }
            }
        }
        sysefx[nefx]->out(tmpmixl, tmpmixr);

        // Add the System Effect to sound output
        float outvol = sysefx[nefx]->sysefxgetvolume();
        for (int i = 0; i < buffersize; ++i)
        {
            actionLock(lock);
            outl[i] += tmpmixl[i] * outvol;
            outr[i] += tmpmixr[i] * outvol;
            actionLock(unlock);
        }
    }

    // Mix all parts
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        for (int i = 0; i < buffersize; ++i)
        {   // the volume did not change
            actionLock(lock);
            outl[i] += part[npart]->partoutl[i];
            outr[i] += part[npart]->partoutr[i];
            actionLock(unlock);
        }
    }

    // Insertion effects for Master Out
    for (nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        if (Pinsparts[nefx] == -2)
        {
            actionLock(lock);
            insefx[nefx]->out(outl, outr);
            actionLock(unlock);
        }
    }

    LFOParams::time++; // update the LFO's time

    zynMaster->vupeakLock(lock);
    vuoutpeakl = 1e-12;
    vuoutpeakr = 1e-12;
    vurmspeakl = 1e-12;
    vurmspeakr = 1e-12;
    zynMaster->vupeakLock(unlock);

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

        // Clip as necessary
        if (outl[idx] > 1.0f)
        {
            outl[idx] = 1.0f;
            clippedL = true;
        }
        else if (outl[idx] < -1.0f)
        {
            outl[idx] = -1.0f;
            clippedL = true;
        }
        if (outr[idx] > 1.0f)
        {
            outr[idx] = 1.0f;
            clippedR = true;
        }
        else if (outr[idx] < -1.0f)
        {
            outr[idx] = -1.0f;
            clippedR = true;
        }

        if (shutup) // fade-out
        {
            float fade = (float)(buffersize - idx) / (float)buffersize;
            outl[idx] *= fade;
            outr[idx] *= fade;
        }
    }
    if (shutup)
        ShutUp();

    zynMaster->vupeakLock(lock);
    if (vumaxoutpeakl < vuoutpeakl)  vumaxoutpeakl = vuoutpeakl;
    if (vumaxoutpeakr < vuoutpeakr)  vumaxoutpeakr = vuoutpeakr;

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
    vuOutPeakL =    vuoutpeakl;
    vuOutPeakR =    vuoutpeakr;
    vuMaxOutPeakL = vumaxoutpeakl;
    vuMaxOutPeakR = vumaxoutpeakr;
    vuRmsPeakL =    vurmspeakl;
    vuRmsPeakR =    vurmspeakr;
    vuClippedL =    clippedL;
    vuClippedR =    clippedR;
    zynMaster->vupeakLock(unlock);
}


// Parameter control
void Master::setPvolume(char control_value)
{
    Pvolume = control_value;
    volume  = dB2rap((Pvolume - 96.0) / 96.0 * 40.0);
}


void Master::setPkeyshift(char Pkeyshift_)
{
    Pkeyshift = Pkeyshift_;
    keyshift = (int)Pkeyshift - 64;
}


void Master::setPsysefxvol(int Ppart, int Pefx, char Pvol)
{
    Psysefxvol[Pefx][Ppart] = Pvol;
    sysefxvol[Pefx][Ppart]  = powf(0.1f, (1.0f - Pvol / 96.0f) * 2.0f);
}


void Master::setPsysefxsend(int Pefxfrom, int Pefxto, char Pvol)
{
    Psysefxsend[Pefxfrom][Pefxto] = Pvol;
    sysefxsend[Pefxfrom][Pefxto]  = powf(0.1f, (1.0f - Pvol / 96.0f) * 2.0f);
}


// Panic! (Clean up all parts and effects)
void Master::ShutUp(void)
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


bool Master::actionLock(lockset request)
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
            chk = pthread_mutex_unlock(processLock);
            if (muted)
                __sync_sub_and_fetch (&muted, 1);
            break;

        case lockmute:
            __sync_add_and_fetch (&muted, 1);
            chk = pthread_mutex_lock(processLock);
            break;

        default:
            break;
    }
    return (chk == 0) ? true : false;
}


bool Master::vupeakLock(lockset request)
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
void Master::vuresetpeaks(void)
{
    zynMaster->vupeakLock(lock);
    vuOutPeakL = vuoutpeakl = 1e-12;
    vuOutPeakR = vuoutpeakr =  1e-12;
    vuMaxOutPeakL = vumaxoutpeakl = 1e-12;
    vuMaxOutPeakR = vumaxoutpeakr = 1e-12;
    vuRmsPeakL = vurmspeakl = 1e-12;
    vuRmsPeakR = vurmspeakr = 1e-12;
    vuClippedL = vuClippedL = clippedL = clippedR = false;
    zynMaster->vupeakLock(unlock);
}


void Master::applyparameters(void)
{
    ShutUp();
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart]->applyparameters();
}


void Master::add2XML(XMLwrapper *xml)
{
    xml->beginbranch("MASTER");
    actionLock(lockmute);
    xml->addpar("volume", Pvolume);
    xml->addpar("key_shift", Pkeyshift);
    xml->addparbool("nrpn_receive", ctl.NRPN.receive);

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


int Master::getalldata(char **data)
{
    XMLwrapper *xml = new XMLwrapper();
    add2XML(xml);
    *data = xml->getXMLdata();
    delete xml;
    return strlen(*data) + 1;
}


void Master::putalldata(char *data, int size)
{
    XMLwrapper *xml = new XMLwrapper();
    if (!xml->putXMLdata(data))
    {
        Runtime.Log("Master putXMLdata failed");
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


bool Master::saveXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper();
    add2XML(xml);
    bool result = xml->saveXMLfile(filename);
    delete xml;
    return result;
}


bool Master::loadXML(string filename)
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


bool Master::getfromXML(XMLwrapper *xml)
{
    if (!xml->enterbranch("MASTER"))
    {
        Runtime.Log("Master getfromXML, no MASTER branch");
        return false;
    }
    setPvolume(xml->getpar127("volume", Pvolume));
    setPkeyshift(xml->getpar127("key_shift", Pkeyshift));
    ctl.NRPN.receive = xml->getparbool("nrpn_receive", ctl.NRPN.receive);

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
