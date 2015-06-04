/*
    Master.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert
    Copyright 2009, James Morris

    This file is part of zynminus, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    zynminus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with zynminus.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>

using namespace std;

#include "Misc/Master.h"

bool Pexitprogram = false;  // if the UI sets this true, the program will exit

Master *zynMaster;

float *denormalkillbuf;

Master::Master() :
    shutup(0),
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
    pthread_mutex_destroy(&mutex);
}


bool Master::Init(unsigned int sample_rate, int buffer_size, int oscil_size)
{
    samplerate = Runtime.settings.Samplerate = sample_rate;
    buffersize = Runtime.settings.Buffersize = buffer_size;
    oscilsize = oscil_size;

    shutup = 0;

    if (!actionLock(init))
    {
        cerr << "Error, actionLock init failed" << endl;
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

    if (NULL == (denormalkillbuf = new float[buffersize]))
    {
        cerr << "Failed to allocate denormalkillbuf" << endl;
        goto bail_out;
    }
    for (int i = 0; i < buffersize; ++i)
        denormalkillbuf[i] = (RND - 0.5) * 1e-16;

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
    defaults();

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
        delete part[npart];
        part[npart] = NULL;
    }
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
    {
        delete insefx[nefx];
        insefx[nefx] = NULL;
    }
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
    {
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
        if (!(chk = pthread_mutex_init(&mutex, NULL)))
            processLock = &mutex;
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
                chk = pthread_mutex_lock(processLock);
                break;

            case lock:
                chk = pthread_mutex_lock(processLock);
                break;

            case unlock:
                chk = pthread_mutex_unlock(processLock);
                break;
            default:
                break;
        }
    }
    return (chk == 0) ? true : false;
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
        {
            //if (nefx==0) setPsysefxvol(npart,nefx,64);
            //else
            setPsysefxvol(npart, nefx, 0);
        }
        for (int nefxto = 0; nefxto < NUM_SYS_EFX; ++nefxto)
            setPsysefxsend(nefx, nefxto, 0);
    }

//	sysefx[0]->changeeffect(1);
    microtonal.defaults();
    ShutUp();
}


// Note On Messages (velocity = 0 for NoteOff)
void Master::NoteOn(unsigned char chan, unsigned char note, unsigned char velocity)
{
    if (!velocity)
        this->NoteOff(chan, note);
    else
        for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
            if (chan == part[npart]->Prcvchn)
            {
                fakepeakpart[npart] = velocity * 2;
                if (part[npart]->Penabled != 0)
                    part[npart]->NoteOn(note, velocity, keyshift);
            }
}


// Note Off Messages
void Master::NoteOff(unsigned char chan, unsigned char note)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
    {
        if (chan == part[npart]->Prcvchn && part[npart]->Penabled != 0)
            part[npart]->NoteOff(note);
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
void Master::partOnOff(int npart, int what)
{
    if (npart >= NUM_MIDI_PARTS)
        return;
    if (what == 0) // disable part
    {
        fakepeakpart[npart] = 0;
        part[npart]->Penabled = 0;
        part[npart]->cleanup();
        for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
            if (Pinsparts[nefx] == npart)
                insefx[nefx]->cleanup();
    } else { // part enabled
        part[npart]->Penabled = 1;
        fakepeakpart[npart] = 0;
    }
}


// Master audio out (the final sound)
bool Master::MasterAudio(float *outl, float *outr, bool lockrequired)
{
    if (!actionLock((lockrequired) ? lock : trylock))
        return false;

    // Clean up the output samples
    memset(outl, 0, buffersize * sizeof(float));
    memset(outr, 0, buffersize * sizeof(float));

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
        if (!sysefx[nefx]->geteffect())
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
        float outvol = sysefx[nefx]->sysefxgetvolume();
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

    vuoutpeakl = 1e-12;
    vuoutpeakr = 1e-12;
    vurmspeakl = 1e-12;
    vurmspeakr = 1e-12;
    float sample;
    for (int i = 0; i < buffersize; ++i)
    {
        sample = outl[i] * volume; // Master Volume
        sample = (sample > 1.0f) ? 1.0f : sample;
        outl[i] = (sample < -1.0f) ? -1.0f : sample;
        sample = outr[i] * volume;
        sample = (sample > 1.0f) ? 1.0f : sample;
        outr[i] = (sample < -1.0f) ? -1.0f : sample;
        if (fabsf(outl[i]) > vuoutpeakl) // Peak computation (for vumeters)
            vuoutpeakl = fabsf(outl[i]);
        if (fabsf(outr[i]) > vuoutpeakr)
            vuoutpeakr = fabsf(outr[i]);
        vurmspeakl += outl[i] * outl[i]; // RMS Peak computation (for vumeters)
        vurmspeakr += outr[i] * outr[i];
    }

    if (vuoutpeakl > 1.0 || vuoutpeakr > 1.0)
        vuclipped = 1;
    if (vumaxoutpeakl < vuoutpeakl)
        vumaxoutpeakl = vuoutpeakl;
    if (vumaxoutpeakr < vuoutpeakr)
        vumaxoutpeakr = vuoutpeakr;

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
        }
        else if (fakepeakpart[npart] > 1)
            fakepeakpart[npart]--;
    }
    // Shutup if it is asked (with fade-out)
    if (shutup)
    {
        for (int i = 0; i < buffersize; ++i)
        {
            float tmp = (float)(buffersize - i)
                               / (float)buffersize;
            outl[i] *= tmp;
            outr[i] *= tmp;
        }
        ShutUp();
    }
    actionLock(unlock);
    // update the LFO's time
    LFOParams::time++;
    return true;
}


// Parameter control
void Master::setPvolume(char control_value)
{
    Pvolume = control_value;
    volume = volControl->Level(control_value);
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
        part[npart]->cleanup();
        fakepeakpart[npart] = 0;
    }
    for (int nefx = 0; nefx < NUM_INS_EFX; ++nefx)
        insefx[nefx]->cleanup();
    for (int nefx = 0; nefx < NUM_SYS_EFX; ++nefx)
        sysefx[nefx]->cleanup();
    vuresetpeaks();
    shutup = 0;
}


// Reset peaks and clear the "clipped" flag (for VU-meter)
void Master::vuresetpeaks(void)
{
    vuoutpeakl = vuoutpeakr = vumaxoutpeakl = vumaxoutpeakr = 1e-9;
    vuclipped = 0;
}


void Master::applyparameters(void)
{
    for (int npart = 0; npart < NUM_MIDI_PARTS; ++npart)
        part[npart]->applyparameters();
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


int Master::getalldata(char **data)
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


void Master::putalldata(char *data, int size)
{
    XMLwrapper *xml=new XMLwrapper();
    if (!xml->putXMLdata(data)) {
        delete(xml);
        return;
    }

    if (xml->enterbranch("MASTER")==0) return;

    actionLock(lock);
    getfromXML(xml);
    actionLock(unlock);
    xml->exitbranch();
    delete(xml);
}


int Master::saveXML(const char *filename)
{
    XMLwrapper *xml=new XMLwrapper();
    xml->beginbranch("MASTER");
    add2XML(xml);
    xml->endbranch();
    int result=xml->saveXMLfile(filename);
    delete (xml);
    return(result);
}


int Master::loadXML(const char *filename)
{
    XMLwrapper *xml=new XMLwrapper();
    if (xml->loadXMLfile(filename)<0) {
        delete(xml);
        return(-1);
    }
    if (xml->enterbranch("MASTER")==0) return(-10);
    getfromXML(xml);
    xml->exitbranch();
    delete(xml);
    return 0;
}


void Master::getfromXML(XMLwrapper *xml)
{
    setPvolume(xml->getpar127("volume",Pvolume));
    setPkeyshift(xml->getpar127("key_shift",Pkeyshift));
    ctl.NRPN.receive=xml->getparbool("nrpn_receive",ctl.NRPN.receive);

    part[0]->Penabled=0;
    for (int npart=0;npart<NUM_MIDI_PARTS;npart++) {
        if (xml->enterbranch("PART",npart)==0) continue;
        part[npart]->getfromXML(xml);
        xml->exitbranch();
    }

    if (xml->enterbranch("MICROTONAL")) {
        microtonal.getfromXML(xml);
        xml->exitbranch();
    }

    sysefx[0]->changeeffect(0);
    if (xml->enterbranch("SYSTEM_EFFECTS")) {
        for (int nefx=0;nefx<NUM_SYS_EFX;nefx++) {
            if (xml->enterbranch("SYSTEM_EFFECT",nefx)==0) continue;
            if (xml->enterbranch("EFFECT")) {
                sysefx[nefx]->getfromXML(xml);
                xml->exitbranch();
            }

            for (int partefx=0;partefx<NUM_MIDI_PARTS;partefx++) {
                if (xml->enterbranch("VOLUME",partefx)==0) continue;
                setPsysefxvol(partefx,nefx,xml->getpar127("vol",Psysefxvol[partefx][nefx]));
                xml->exitbranch();
            }

            for (int tonefx=nefx+1;tonefx<NUM_SYS_EFX;tonefx++) {
                if (xml->enterbranch("SENDTO",tonefx)==0) continue;
                setPsysefxsend(nefx,tonefx,xml->getpar127("send_vol",Psysefxsend[nefx][tonefx]));
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if (xml->enterbranch("INSERTION_EFFECTS")) {
        for (int nefx=0;nefx<NUM_INS_EFX;nefx++) {

            if (xml->enterbranch("INSERTION_EFFECT",nefx)==0) continue;
            Pinsparts[nefx]=xml->getpar("part",Pinsparts[nefx],-2,NUM_MIDI_PARTS);
            if (xml->enterbranch("EFFECT")) {
                insefx[nefx]->getfromXML(xml);
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }
}


