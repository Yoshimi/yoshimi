/*
    Alienwah.cpp - "AlienWah" effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018-2021, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code.

*/

#include "Misc/SynthEngine.h"
#include "Effects/Alienwah.h"

using namespace std;

static const int PRESET_SIZE = 11;
static const int NUM_PRESETS = 4;
static unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        // AlienWah1
        { 127, 64, 70, 0, 0, 62, 60, 105, 25, 0, 64 },
        // AlienWah2
        { 127, 64, 73, 106, 0, 101, 60, 105, 17, 0, 64 },
        // AlienWah3
        { 127, 64, 63, 0, 1, 100, 112, 105, 31, 0, 42 },
        // AlienWah4
        { 93, 64, 25, 0, 1, 66, 101, 11, 47, 0, 86 }
};

Alienwah::Alienwah(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0),
    lfo(_synth),
    oldl(NULL),
    oldr(NULL),
    synth(_synth)
{
    setpreset(Ppreset);
    cleanup();
    oldclfol = complex<float>(fb, 0.0);
    oldclfor = complex<float>(fb, 0.0);
    Pchanged = false;
}


Alienwah::~Alienwah()
{
    if (oldl != NULL)
        delete [] oldl;
    if (oldr != NULL)
        delete [] oldr ;
}


// Apply the effect
void Alienwah::out(float *smpsl, float *smpsr)
{
    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
            smpsl[i] += float(1e-20); // anti-denormal
            smpsr[i] += float(1e-20); // anti-denormal
    }
    float lfol;
    float lfor; // Left/Right LFOs
    complex<float> clfol, clfor, out, tmp;
    lfo.effectlfoout(&lfol, &lfor);
    lfol *= depth * TWOPI;
    lfor *= depth * TWOPI;
    clfol = complex<float>(cosf(lfol + phase) * fb, sinf(lfol + phase) * fb); //rework
    clfor = complex<float>(cosf(lfor + phase) * fb, sinf(lfor + phase) * fb); //rework

    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
        float x = (float)i / synth->sent_buffersize_f;
        float x1 = 1.0f - x;
        // left
        tmp = clfol * x + oldclfol * x1;

        out = tmp * oldl[oldk];
        out += (1 - abs(fb)) * smpsl[i] * pangainL.getAndAdvanceValue();

        oldl[oldk] = out;
        float l = out.real() * 10.0f * (fb + 0.1f);

        // right
        tmp = clfor * x + oldclfor * x1;

        out = tmp * oldr[oldk];
        out += (1 - abs(fb)) * smpsr[i] * pangainR.getAndAdvanceValue();

        oldr[oldk] = out;
        float r = out.real() * 10.0f * (fb + 0.1f);

        if (++oldk >= Pdelay)
            oldk = 0;
        // LRcross
        efxoutl[i] = l * (1.0f - lrcross.getValue()) + r * lrcross.getValue();
        efxoutr[i] = r * (1.0f - lrcross.getValue()) + l * lrcross.getValue();
        lrcross.advanceValue();
    }
    oldclfol = clfol;
    oldclfor = clfor;
}


// Cleanup the effect
void Alienwah::cleanup(void)
{
    for (int i = 0; i < Pdelay; ++i)
    {
        oldl[i] = complex<float>(0.0f, 0.0f);
        oldr[i] = complex<float>(0.0f, 0.0f);
    }
    oldk = 0;
}


// Parameter control
void Alienwah::setdepth(unsigned char _depth)
{
    Pdepth = _depth;
    depth = Pdepth / 127.0f;
}

void Alienwah::setfb(unsigned char _fb)
{
    Pfb = _fb;
    fb = fabs((Pfb - 64.0f) / 64.1f);
    fb = sqrtf(fb);
    if (fb < 0.4f)
        fb = 0.4f;
    if (Pfb < 64)
        fb = -fb;
}


void Alienwah::setvolume(unsigned char _volume)
{
    Pvolume = _volume;
    float tmp = Pvolume / 127.0f;
    outvolume.setTargetValue(tmp);
    if (insertion == 0)
        volume.setTargetValue(1.0f);
    else
        volume.setTargetValue(tmp);
}


void Alienwah::setphase(unsigned char _phase)
{
    Pphase = _phase;
    phase = (Pphase - 64.0f) / 64.0f * PI;
}


void Alienwah::setdelay(unsigned char _delay)
{
    if (oldl != NULL)
        delete [] oldl;
    if (oldr != NULL)
        delete [] oldr;
    Pdelay = _delay;
    oldl = new complex<float>[Pdelay];
    oldr = new complex<float>[Pdelay];
    cleanup();
}


void Alienwah::setpreset(unsigned char npreset)
{
    if (npreset < 0xf)
    {
        if (npreset >= NUM_PRESETS)
            npreset = NUM_PRESETS - 1;
        for (int n = 0; n < PRESET_SIZE; ++n)
            changepar(n, presets[npreset][n]);
        if (insertion)
            changepar(0, presets[npreset][0] / 2); // lower the volume if this is insertion effect
        Ppreset = npreset;
    }
    else
    {
        unsigned char preset = npreset & 0xf;
        unsigned char param = npreset >> 4;
        if (param == 0xf)
            param = 0;
        changepar(param, presets[preset][param]);
        if (insertion && (param == 0))
            changepar(0, presets[preset][0] / 2);
    }
    Pchanged = false;
}


void Alienwah::changepar(int npar, unsigned char value)
{
    if (npar == -1)
    {
        Pchanged = (value != 0);
        return;
    }
    switch (npar)
    {
        case 0:
            setvolume(value);
            break;
        case 1:
            setpanning(value);
            break;
        case 2:
            lfo.Pfreq = value;
            lfo.updateparams();
            break;
        case 3:
            lfo.Prandomness = value;
            lfo.updateparams();
            break;
        case 4:
            lfo.PLFOtype = value;
            lfo.updateparams();
            break;
        case 5:
            lfo.Pstereo = value;
            lfo.updateparams();
            break;
        case 6:
            setdepth(value);
            break;
        case 7:
            setfb(value);
            break;
        case 8:
            setdelay(value);
            break;
        case 9:
            setlrcross(value);
            break;
        case 10:
            setphase(value);
            break;
    }
    Pchanged = true;
}


unsigned char Alienwah::getpar(int npar)
{
    switch (npar)
    {
        case -1: return Pchanged;
        case 0:  return Pvolume;
        case 1:  return Ppanning;
        case 2:  return lfo.Pfreq;
        case 3:  return lfo.Prandomness;
        case 4:  return lfo.PLFOtype;
        case 5:  return lfo.Pstereo;
        case 6:  return Pdepth;
        case 7:  return Pfb;
        case 8:  return Pdelay;
        case 9:  return Plrcross;
        case 10: return Pphase;
        default: break;
    }
    return 0;
}


float Alienlimit::getlimits(CommandBlock *getData)
{
    int value = getData->data.value;
    int control = getData->data.control;
    int request = getData->data.type & TOPLEVEL::type::Default; // clear flags
    int npart = getData->data.part;
    int presetNum = getData->data.engine;
    int min = 0;
    int max = 127;

    int def = presets[presetNum][control];
    unsigned char canLearn = TOPLEVEL::type::Learnable;
    unsigned char isInteger = TOPLEVEL::type::Integer;
    switch (control)
    {
        case 0:
            if (npart != TOPLEVEL::section::systemEffects) // system effects
                def /= 2;
            break;
        case 1:
            break;
        case 2:
            break;
        case 3:
            break;
        case 4:
            max = 1;
            canLearn = 0;
            break;
        case 5:
            break;
        case 6:
            break;
        case 7:
            break;
        case 8:
            max = 100;
            canLearn = 0;
            break;
        case 9:
            break;
        case 10:
            break;
        case 16:
            max = 3;
            canLearn = 0;
            break;
        default:
            getData->data.type |= TOPLEVEL::type::Error; // error
            return 1.0f;
            break;
    }

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
    getData->data.type |= (canLearn + isInteger);
    return float(value);
}

