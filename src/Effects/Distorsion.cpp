/*
    Distorsion.cpp - Distorsion effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified April 2011
*/

#include "Misc/SynthEngine.h"
#include "Effects/Distorsion.h"

Distorsion::Distorsion(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0),
    Pvolume(50),
    Pdrive(90),
    Plevel(64),
    Ptype(0),
    Pnegate(0),
    Plpf(127),
    Phpf(0),
    Pstereo(1),
    Pprefiltering(0),
    synth(_synth)
{
    lpfl = new AnalogFilter(2, 22000, 1, 0, synth);
    lpfr = new AnalogFilter(2, 22000, 1, 0, synth);
    hpfl = new AnalogFilter(3, 20, 1, 0, synth);
    hpfr = new AnalogFilter(3, 20, 1, 0, synth);
    setpreset(Ppreset);
    changepar(2, 40);
    cleanup();
}

Distorsion::~Distorsion()
{
    delete lpfl;
    delete lpfr;
    delete hpfl;
    delete hpfr;

}

// Cleanup the effect
void Distorsion::cleanup(void)
{
    lpfl->cleanup();
    hpfl->cleanup();
    lpfr->cleanup();
    hpfr->cleanup();
}


// Apply the filters
void Distorsion::applyfilters(float *efxoutl, float *efxoutr)
{
    lpfl->filterout(efxoutl);
    hpfl->filterout(efxoutl);
    lpfr->filterout(efxoutr);
    hpfr->filterout(efxoutr);
}


// Effect output
void Distorsion::out(float *smpsl, float *smpsr)
{
    float inputdrive = powf(5.0f, (Pdrive - 32.0f) / 127.0f);
    if (Pnegate)
        inputdrive *= -1.0f;

    if (Pstereo) // Stereo
    {
        for (int i = 0; i < synth->p_buffersize; ++i)
        {
            efxoutl[i] = smpsl[i] * inputdrive * pangainL;
            efxoutr[i] = smpsr[i] * inputdrive* pangainR;
        }
    }
    else // Mono
        for (int i = 0; i < synth->p_buffersize; ++i)
            efxoutl[i] = inputdrive * (smpsl[i]* pangainL + smpsr[i]* pangainR) * 0.7f;

    if (Pprefiltering)
        applyfilters(efxoutl, efxoutr);

    waveShapeSmps(synth->p_buffersize, efxoutl, Ptype + 1, Pdrive);
    if (Pstereo)
        waveShapeSmps(synth->p_buffersize, efxoutr, Ptype + 1, Pdrive);

    if (!Pprefiltering)
        applyfilters(efxoutl, efxoutr);
    if (!Pstereo)
        memcpy(efxoutr, efxoutl, synth->p_bufferbytes);

    float level = dB2rap(60.0f * Plevel / 127.0f - 40.0f);
    for (int i = 0; i < synth->p_buffersize; ++i)
    {
        float lout = efxoutl[i];
        float rout = efxoutr[i];
        float l = lout * (1.0f - lrcross) + rout * lrcross;
        float r = rout * (1.0f - lrcross) + lout * lrcross;
        lout = l;
        rout = r;
        efxoutl[i] = lout * 2.0f * level;
        efxoutr[i] = rout * 2.0f * level;
    }
}


// Parameter control
void Distorsion::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    if(insertion == 0)
    {
        outvolume = powf(0.01f, (1.0f - Pvolume / 127.0f)) * 4.0f;
        volume = 1.0f;
    }
    else
         volume = outvolume = Pvolume / 127.0f;
    if (Pvolume == 0.0f)
        cleanup();
}

void Distorsion::setlpf(unsigned char Plpf_)
{
    Plpf = Plpf_;
    float fr = expf(powf(Plpf / 127.0f, 0.5f) * logf(25000.0f)) + 40.0f;
    lpfl->setfreq(fr);
    lpfr->setfreq(fr);
}


void Distorsion::sethpf(unsigned char Phpf_)
{
    Phpf = Phpf_;
    float fr = expf(powf(Phpf / 127.0f, 0.5f) * logf(25000.0f)) + 20.0f;
    hpfl->setfreq(fr);
    hpfr->setfreq(fr);
}


void Distorsion::setpreset(unsigned char npreset)
{
    const int PRESET_SIZE = 11;
    const int NUM_PRESETS = 6;
    int presets[NUM_PRESETS][PRESET_SIZE] = {
        // Overdrive 1
        { 127, 64, 35, 56, 70, 0, 0, 96, 0, 0, 0 },
        // Overdrive 2
        { 127, 64, 35, 29, 75, 1, 0, 127, 0, 0, 0 },
        // A. Exciter 1
        { 64, 64, 35, 75, 80, 5, 0, 127, 105, 1, 0 },
        // A. Exciter 2
        { 64, 64, 35, 85, 62, 1, 0, 127, 118, 1, 0 },
        // Guitar Amp
        { 127, 64, 35, 63, 75, 2, 0, 55, 0, 0, 0 },
        // Quantisize
        { 127, 64, 35, 88, 75, 4, 0, 127, 0, 1, 0 }
    };

    if (npreset >= NUM_PRESETS)
        npreset = NUM_PRESETS - 1;
    for (int n = 0; n < PRESET_SIZE; ++n)
        changepar(n, presets[npreset][n]);
    if (!insertion)
        // lower the volume if this is system effect
        changepar(0, (int)roundf(((float)presets[npreset][0] * 0.7f)));
    Ppreset = npreset;
    cleanup();
}

// Parameters Control
void Distorsion::settype(unsigned char Ptype_){
    if(Ptype_ > 13)
        Ptype = 13;
    else
        Ptype = Ptype_;
}

void Distorsion::setdrive(unsigned char Pdrive_){ 
    Pdrive = Pdrive_;
}
void Distorsion::setlevel(unsigned char Plevel_){ 
    Plevel = Plevel_;
}

void Distorsion::changepar(int npar, unsigned char value)
{
    switch (npar)
    {
        case c_Pvolume:
            setvolume(value);
            break;
        case c_Ppanning:
            setpanning(value);
            break;
        case c_Plrcross:
            setlrcross(value);
            break;
        case c_Pdrive:
            Pdrive = value;
            break;
        case c_Plevel:
            Plevel = value;
            break;
        case c_Ptype:
            settype(value);
            break;
        case c_Pnegate:
            if (value > 1)
                Pnegate = 1;
            else
                Pnegate = value;
            break;
        case c_Plpf:
            setlpf(value);
            break;
        case c_Phpf:
            sethpf(value);
            break;
        case c_Pstereo:
            Pstereo = (value > 0) ? 1 : 0; 
            break;
        case c_Pprefiltering:
            Pprefiltering = value;
            break;
    }
}


unsigned char Distorsion::getpar(int npar)
{
    switch (npar)
    {
        case c_Pvolume:  return Pvolume;
        case c_Ppanning:  return Ppanning;
        case c_Plrcross:  return Plrcross;
        case c_Pdrive:  return Pdrive;
        case c_Plevel:  return Plevel;
        case c_Ptype:  return Ptype;
        case c_Pnegate:  return Pnegate;
        case c_Plpf:  return Plpf;
        case c_Phpf:  return Phpf;
        case c_Pstereo:  return Pstereo;
        case c_Pprefiltering: return Pprefiltering;
        default: break;
    }
    return 0; // in case of bogus parameter number
}
