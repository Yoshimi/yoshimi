/*
    Master.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert
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

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/

#include <iostream>

using namespace std;

#include "GuiThreadUI.h"
#include "Misc/Master.h"

bool Pexitprogram = false;  // if the UI sets this true, the program will exit

Master *zynMaster = NULL;

Master::Master() :
    shutup(false),
    fft(NULL),
    samplerate(0),
    buffersize(0),
    oscilsize(0),
    processLock(NULL),
    tmpmixl(NULL),
    tmpmixr(NULL),
    volControl(new Fader(2.0)) // 2.0 => 0 .. +6db gain)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart] = NULL;
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx] = NULL;
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx] = NULL;
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
    actionLock(destroy);
}


bool Master::Init(unsigned int sample_rate, int buffer_size, int oscil_size,
                  string params_file, string instrument_file)
{
    samplerate = Runtime.settings.Samplerate = sample_rate;
    buffersize = Runtime.settings.Buffersize = buffer_size;
    if (oscil_size < (buffersize / 2))
    {
        cerr << "Enforcing oscilsize adjustment to half buffersize, "
             << oscilsize << " -> " << buffersize / 2 << endl;
        oscilsize = buffersize / 2;
    }
    else
        oscilsize = oscil_size;

    shutup = false;

    if (!actionLock(init))
    {
        cerr << "Error, actionLock init failed" << endl;
        goto bail_out;
    }
    if (Runtime.settings.showGui && !vupeakLock(init))
    {
        cerr << "Error, meterLock init failed" << endl;
        goto bail_out;
    }

    if ((fft = new FFTwrapper(oscilsize)) == NULL)
    {
        cerr << "Error, Master failed to allocate fft" << endl;
        goto bail_out;
    }

    tmpmixl = new float[buffersize];
    tmpmixr = new float[buffersize];
    if (tmpmixl == NULL || tmpmixr == NULL)
    {
        cerr << "Error, Master tmpmix allocations failed" << endl;
        goto bail_out;
    }

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart] = new Part(&microtonal, fft);
        if (NULL == part[npart])
        {
            cerr << "Failed to allocate new Part" << endl;
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
            cerr << "Failed to allocate new Insertion EffectMgr" << endl;
            goto bail_out;
        }
    }
    // System Effects init
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        sysefx[nefx] = new EffectMgr(0);
        if (NULL == sysefx[nefx])
        {
            cerr << "Failed to allocate new System Effects EffectMgr" << endl;
            goto bail_out;
        }
    }
    if (NULL == volControl)
    {
        cerr << "Failed to allocate new VolumeControl" << endl;
        goto bail_out;
    }
    setDefaults();

    if (!params_file.empty())
    {
        if (!zynMaster->loadXML(params_file))
        {
            cerr << "Error, failed to load master file: " << params_file << endl;
            goto bail_out;
        }
        else
        {
            zynMaster->applyParameters();
            if (Runtime.settings.verbose)
                cerr << "Master file " << params_file << " loaded" << endl;
        }
    }
    if (!instrument_file.empty())
    {
        int loadtopart = 0;
        if (!zynMaster->part[loadtopart]->loadXMLinstrument(instrument_file))
        {
            cerr << "Error, failed to load instrument file: " << instrument_file << endl;
            goto bail_out;
        }
        else
        {
            zynMaster->part[loadtopart]->applyParameters();
            if (Runtime.settings.verbose)
                cerr << "Instrument file " << instrument_file << " loaded" << endl;
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

bool Master::actionLock(lockset request)
{
    int chk  = -1;
    if (request == init)
    {
        if (!(chk = pthread_mutex_init(&processMutex, NULL)))
            processLock = &processMutex;
        else
        {
            cerr << "Error, Master actionLock init fails :-(" << endl;
            processLock = NULL;
        }
    }
    else if (NULL != processLock)
    {
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
                musicClient->unMute();
                break;

            case lockmute:
                musicClient->Mute();
                chk = pthread_mutex_lock(processLock);
                break;

            case destroy:
                pthread_mutex_destroy(&processMutex);
                chk = 0;
                break;

            default:
                break;
        }
    }
    return (chk == 0) ? true : false;
}


bool Master::vupeakLock(lockset request)
{
    int chk  = -1;
    if (request == init)
    {
        if (!(chk = pthread_mutex_init(&meterMutex, NULL)))
            meterLock = &meterMutex;
        else
        {
            cerr << "Error, Master meterLock init fails :-(" << endl;
            meterLock = NULL;
        }
    }
    else if (NULL != meterLock)
    {
        switch (request)
        {
            case trylock:
                chk = pthread_mutex_trylock(meterLock);
                break;

            case lock:
                chk = pthread_mutex_lock(meterLock);
                break;

            case unlock:
                chk = pthread_mutex_unlock(meterLock);
                break;

            case lockmute:
                musicClient->Mute();
                chk = pthread_mutex_lock(processLock);
                break;

            case destroy:
                pthread_mutex_destroy(&meterMutex);
                chk = 0;
                break;

            default:
                break;
        }
    }
    return (chk == 0) ? true : false;
}


void Master::setDefaults(void)
{
    setPvolume(90);
    setPkeyshift(64);

    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart]->setDefaults();
        part[npart]->Prcvchn = npart % NUM_MIDI_CHANNELS;
    }

    partOnOff(0, 1); // enable the first part

    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        insefx[nefx]->setDefaults();
        Pinsparts[nefx] = -1;
    }

    // System Effects init
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        sysefx[nefx]->setDefaults();
        for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        {
            //if (nefx==0) setPsysefxvol(npart,nefx,64);
            //else
            setPsysefxvol(npart, nefx, 0);
        }
        for (int nefxto = 0; nefxto < NUM_SYS_EFX; ++nefxto)
            setPsysefxsend(nefx, nefxto, 0);
    }

//	sysefx[0]->changeeffect(1);
    microtonal.setDefaults();
    ShutUp();
}


// Note On Messages (velocity = 0 for NoteOff)
void Master::NoteOn(unsigned char chan, unsigned char note,
                    unsigned char velocity, bool record_trigger)
{
    if (!velocity)
        this->NoteOff(chan, note);
    else
    {
        if (Runtime.settings.showGui)
        {
            if (record_trigger && guiMaster->autorecordbutton->value())
            {

                cerr << "auto start record" << endl;

                zynMaster->actionLock(lock);
                musicClient->startRecord();
                zynMaster->actionLock(unlock);
                guiMaster->record_activated();
            }
        }
        zynMaster->actionLock(lock);
        for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        {
            if (chan == part[npart]->Prcvchn)
            {
                fakepeakpart[npart] = velocity * 2;
                if (part[npart]->Penabled)
                    part[npart]->NoteOn(note, velocity, keyshift);
            }
        }
        zynMaster->actionLock(unlock);
    }
}


// Note Off Messages
void Master::NoteOff(unsigned char chan, unsigned char note)
{
    zynMaster->actionLock(lock);
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (chan == part[npart]->Prcvchn && part[npart]->Penabled)
            part[npart]->NoteOff(note);
    }
    zynMaster->actionLock(unlock);
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
        ctl.setParameterNumber(type, par);

        int parhi = -1, parlo = -1, valhi = -1, vallo = -1;
        if (!ctl.getNrpn(&parhi, &parlo, &valhi, &vallo))
        {   // this is NRPN
            // fprintf(stderr,"rcv. NRPN: %d %d %d %d\n",parhi,parlo,valhi,vallo);
            switch (parhi)
            {
                case 0x04: // System Effects
                    if (parlo < NUM_SYS_EFX)
                        sysefx[parlo]->setEffectPar_nolock(valhi, vallo);
                    break;
            case 0x08: // Insertion Effects
                if (parlo < NUM_INS_EFX)
                    insefx[parlo]->setEffectPar_nolock(valhi, vallo);
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
                sysefx[nefx]->Cleanup();
            for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
                insefx[nefx]->Cleanup();
        }
    }
}


// Enable/Disable a part
void Master::partOnOff(int npart, int what)
{
    if (npart >= NUM_MIDI_PARTS)
        return;
    fakepeakpart[npart] = 0;
    if (what)
        part[npart]->Penabled = 1; // part enabled
    else
    {   // disabled part
        part[npart]->Penabled = 0;
        part[npart]->Cleanup();
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
            if (Pinsparts[nefx] == npart)
                insefx[nefx]->Cleanup();
    }
}


// Master audio out (the final sound)
void Master::MasterAudio(jsample_t *outl, jsample_t *outr)
{
    // Clean up the output samples
    memset(outl, 0, buffersize * sizeof(jsample_t));
    memset(outr, 0, buffersize * sizeof(jsample_t));
    actionLock(lock);

    // Compute part samples and store them npart]->partoutl,partoutr
    int npart;
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        if (part[npart]->Penabled)
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
            newvol_l *= pan * 2.0;
        else
            newvol_r *= (1.0 - pan) * 2.0;

        if (ABOVE_AMPLITUDE_THRESHOLD(oldvol_l, newvol_l)
            || ABOVE_AMPLITUDE_THRESHOLD(oldvol_r, newvol_r))
        {   // the volume or the panning has changed and needs interpolation
            for (int i = 0; i < buffersize; ++i)
            {
                float vol_l = INTERPOLATE_AMPLITUDE(oldvol_l, newvol_l, i,
                                                          buffersize);
                float vol_r = INTERPOLATE_AMPLITUDE(oldvol_r, newvol_r, i,
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
            {   // the volume did not changed
                part[npart]->partoutl[i] *= newvol_l;
                part[npart]->partoutr[i] *= newvol_r;
            }
        }
    }
    // System effects
    for (nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
        if (!sysefx[nefx]->getEffect())
            continue; // the effect is disabled

        // Clean up the samples used by the system effects
        memset(tmpmixl, 0, buffersize * sizeof(float));
        memset(tmpmixr, 0, buffersize * sizeof(float));

        // Mix the channels according to the part settings about System Effect
        for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        {
            if (!Psysefxvol[nefx][npart]   // skip, part has no output to effect
                || !part[npart]->Penabled) // skip, part is disabled
                continue;

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
        float outvol = sysefx[nefx]->sysefxGetVolume();
        for (int i = 0; i < buffersize; ++i)
        {
            outl[i] += tmpmixl[i] * outvol;
            outr[i] += tmpmixr[i] * outvol;
        }
    }

    // Mix all parts
    for (npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        for (int i = 0; i < buffersize; ++i)
        {   // the volume did not changed
            outl[i] += part[npart]->partoutl[i];
            outr[i] += part[npart]->partoutr[i];
        }
    }

    // Insertion effects for Master Out
    for (nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        if (Pinsparts[nefx] == -2)
            insefx[nefx]->out(outl, outr);
    }

    LFOParams::time++; // update the LFO's time
    actionLock(unlock);

    if (Runtime.settings.showGui)
    {
        zynMaster->vupeakLock(lock);
        vuoutpeakl = 1e-9;
        vuoutpeakr = 1e-9;
        vurmspeakl = 1e-9;
        vurmspeakr = 1e-9;
        zynMaster->vupeakLock(unlock);
    }
    float sample;
    float absval;
    for (int idx = 0; idx < buffersize; ++idx)
    {
        // left
        sample = outl[idx] * volume; // Master Volume
        if (Runtime.settings.showGui)
        {
            absval = fabsf(sample);
            if (absval > vuoutpeakl) // Peak computation (for vumeters)
                if ((vuoutpeakl = absval) > 1.0f)
                    vuclipped = true;
            vurmspeakl += sample * sample;  // RMS Peak computation (for vumeters)
        }
        if (sample > 1.0f)
            sample = 1.0f;
        else if (sample > 1.0f)
            sample = -1.0f;
        outl[idx] = sample;

        // right
        sample = outr[idx] * volume;
        if (Runtime.settings.showGui)
        {
            absval = fabsf(sample);
            if (absval > vuoutpeakr)  // Peak computation (for vumeters)
                if ((vuoutpeakr = absval) > 1.0f)
                    vuclipped = true;
            vurmspeakr += sample * sample;   // RMS Peak computation (for vumeters)
        }
        if (sample > 1.0f)
            sample = 1.0f;
        else if (sample < -1.0f)
            sample = -1.0f;
        outr[idx] = sample;
        if (shutup) // Shutup fade-out
        {
            float fade = (float)(buffersize - idx) / (float)buffersize;
            outl[idx] *= fade;
            outr[idx] *= fade;
        }

    }
    // Shutup if it is asked
    if (shutup)
        ShutUp();

    if (Runtime.settings.showGui)
    {
        zynMaster->vupeakLock(lock);
        if (vumaxoutpeakl < vuoutpeakl)  vumaxoutpeakl = vuoutpeakl;
        if (vumaxoutpeakr < vuoutpeakr)  vumaxoutpeakr = vuoutpeakr;

        vurmspeakl = sqrtf(vurmspeakl / buffersize);
        vurmspeakr = sqrtf(vurmspeakr / buffersize);

        // Part Peak computation (for Part vumeters or fake part vumeters)
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
                vuoutpeakpart[npart] *= volume;
                // how is part peak related to master volume??
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
        vuClipped =     vuclipped;
        zynMaster->vupeakLock(unlock);
    }
}


// Parameter control
void Master::setPvolume(char control_value)
{
    Pvolume = control_value;
    volume = volControl->Level(Pvolume);
}


void Master::setPkeyshift(char Pkeyshift_)
{
    Pkeyshift = Pkeyshift_;
    keyshift = (int)Pkeyshift - 64;
}


void Master::setPsysefxvol(int Ppart, int Pefx, char Pvol)
{
    Psysefxvol[Pefx][Ppart] = Pvol;
    //sysefxvol[Pefx][Ppart] = powf(0.1, (1.0 - Pvol / 96.0) * 2.0);
    sysefxvol[Pefx][Ppart] = volControl->Level(Pvol);
}


void Master::setPsysefxsend(int Pefxfrom, int Pefxto, char Pvol)
{
    Psysefxsend[Pefxfrom][Pefxto] = Pvol;
    //sysefxsend[Pefxfrom][Pefxto] = powf(0.1, (1.0 - Pvol / 96.0) * 2.0);
    sysefxsend[Pefxfrom][Pefxto] = volControl->Level(Pvol);
}


// Panic! (Clean up all parts and effects)
void Master::ShutUp(void)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        part[npart]->Cleanup();
        fakepeakpart[npart] = 0;
    }
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx]->Cleanup();
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx]->Cleanup();
    vuResetpeaks();
    shutup = false;
}


// Reset peaks and clear the "clipped" flag (for VU-meter)
void Master::vuResetpeaks(void)
{
    zynMaster->vupeakLock(lock);
    vuOutPeakL = vuoutpeakl = 1e-12;
    vuOutPeakR = vuoutpeakr =  1e-12;
    vuMaxOutPeakL = vumaxoutpeakl = 1e-12;
    vuMaxOutPeakR = vumaxoutpeakr = 1e-12;
    vuRmsPeakL = vurmspeakl = 1e-12;
    vuRmsPeakR = vurmspeakr = 1e-12;
    vuClipped = vuclipped = false;
    zynMaster->vupeakLock(unlock);
}


void Master::applyParameters(void)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart]->applyParameters();
}


void Master::add2XML(XMLwrapper *xml)
{
    xml->addpar("volume",Pvolume);
    xml->addpar("key_shift",Pkeyshift);
    xml->addparbool("nrpn_receive",ctl.NRPN.receive);

    xml->beginbranch("MICROTONAL");
    microtonal.add2XML(xml);
    xml->endbranch();

    for (int npart=0;npart<NUM_MIDI_PARTS;npart++) {
        xml->beginbranch("PART",npart);
        part[npart]->add2XML(xml);
        xml->endbranch();
    }

    xml->beginbranch("SYSTEM_EFFECTS");
    for (int nefx=0;nefx<NUM_SYS_EFX;nefx++) {
        xml->beginbranch("SYSTEM_EFFECT",nefx);
        xml->beginbranch("EFFECT");
        sysefx[nefx]->add2XML(xml);
        xml->endbranch();

        for (int pefx=0;pefx<NUM_MIDI_PARTS;pefx++) {
            xml->beginbranch("VOLUME",pefx);
            xml->addpar("vol", Psysefxvol[nefx][pefx]);
            xml->endbranch();
        }

        for (int tonefx=nefx+1;tonefx<NUM_SYS_EFX;tonefx++) {
            xml->beginbranch("SENDTO",tonefx);
            xml->addpar("send_vol", Psysefxsend[nefx][tonefx]);
            xml->endbranch();
        }
        xml->endbranch();
    }
    xml->endbranch();

    xml->beginbranch("INSERTION_EFFECTS");
    for (int nefx=0;nefx<NUM_INS_EFX;nefx++) {
        xml->beginbranch("INSERTION_EFFECT", nefx);
        xml->addpar("part", Pinsparts[nefx]);

        xml->beginbranch("EFFECT");
        insefx[nefx]->add2XML(xml);
        xml->endbranch();
        xml->endbranch();
    }
    xml->endbranch();
}


int Master::getAllData(char **data)
{
    XMLwrapper *xml=new XMLwrapper();

    xml->beginbranch("MASTER");

    actionLock(lock);
    add2XML(xml);
    actionLock(unlock);
    xml->endbranch();

    *data=xml->getXMLdata();
    delete (xml);
    return strlen(*data) + 1;
}


void Master::putAllData(char *data, int size)
{
    XMLwrapper *xml=new XMLwrapper();
    if (!xml->putXMLdata(data))
    {
        cerr << "Error, Master putXMLdata failed" << endl;
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
        cerr << "Error, putAllData failed to enter MASTER branch" << endl;
    delete xml;
}


bool Master::saveXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper();
    xml->beginbranch("MASTER");
    add2XML(xml);
    xml->endbranch();
    bool result = xml->saveXMLfile(filename);
    delete xml;
    return result;
}


bool Master::loadXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper();
    if (!xml->loadXMLfile(filename))
    {
        delete xml;
        return false;
    }
    if (xml->enterbranch("MASTER") == 0)
    {
        cerr << "Error, no MASTER branch found" << endl;
        return false;
    }
    getfromXML(xml);
    xml->exitbranch();
    delete xml;
    return true;
}


void Master::getfromXML(XMLwrapper *xml)
{
    setPvolume(xml->getpar127("volume",Pvolume));
    setPkeyshift(xml->getpar127("key_shift",Pkeyshift));
    ctl.NRPN.receive=xml->getparbool("nrpn_receive",ctl.NRPN.receive);

    part[0]->Penabled = 0;
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (xml->enterbranch("PART",npart) == 0)
            continue;
        part[npart]->getfromXML(xml);
        xml->exitbranch();
    }

    if (xml->enterbranch("MICROTONAL")) {
        microtonal.getfromXML(xml);
        xml->exitbranch();
    }

    sysefx[0]->changeEffect(0);
    if (xml->enterbranch("SYSTEM_EFFECTS"))
    {
        for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        {
            if (xml->enterbranch("SYSTEM_EFFECT",nefx) == 0)
                continue;
            if (xml->enterbranch("EFFECT")) {
                sysefx[nefx]->getfromXML(xml);
                xml->exitbranch();
            }

            for (int partefx = 0; partefx < NUM_MIDI_PARTS; ++partefx)
            {
                if (xml->enterbranch("VOLUME", partefx) == 0)
                    continue;
                setPsysefxvol(partefx, nefx,xml->getpar127("vol", Psysefxvol[partefx][nefx]));
                xml->exitbranch();
            }

            for (int tonefx = nefx + 1; tonefx < NUM_SYS_EFX; ++tonefx)
            {
                if (xml->enterbranch("SENDTO", tonefx) == 0)
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
            if (xml->enterbranch("INSERTION_EFFECT", nefx) == 0)
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
}
