/*
    Echo.cpp - Echo effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified March 2011
*/

#include "Misc/SynthEngine.h"
#include "Effects/Echo.h"

Echo::Echo(bool insertion_, float* efxoutl_, float* efxoutr_) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0),
    Pvolume(50),
    Pdelay(60),
    Plrdelay(100),
    Pfb(40),
    Phidamp(60),
    lrdelay(0),
    ldelay(NULL),
    rdelay(NULL)
{
    setpreset(Ppreset);
    changepar(4, 100); // lrcross
    cleanup();
}

Echo::~Echo()
{
    delete [] ldelay;
    delete [] rdelay;
}


// Cleanup the effect
void Echo::cleanup(void)
{
    memset(ldelay, 0, dl * sizeof(float));
    memset(rdelay, 0, dr * sizeof(float));
    oldl = oldr = 0.0f;
}


// Initialize the delays
void Echo::initdelays(void)
{
    // todo: make this adjust insted of destroy old delays
    kl = kr = 0;
    dl = delay - lrdelay;
    if (dl < 1)
        dl = 1;
    dr = delay + lrdelay;
    if (dr < 1)
        dr = 1;

    if (ldelay != NULL)
        delete [] ldelay;
    if (rdelay != NULL)
        delete [] rdelay;
    ldelay = new float[dl];
    rdelay = new float[dr];
    cleanup();
}


// Effect output
void Echo::out(float* smpsl, float* smpsr)
{
    float l, r;
    float ldl = ldelay[kl];
    float rdl = rdelay[kr];
    for (int i = 0; i < synth->buffersize; ++i)
    {
        ldl = ldelay[kl];
        rdl = rdelay[kr];
        l = ldl * (1.0 - lrcross) + rdl * lrcross;
        r = rdl * (1.0 - lrcross) + ldl * lrcross;
        ldl = l;
        rdl = r;

        efxoutl[i] = ldl * 2.0f - 1e-20f; // anti-denormal - a very, very, very
        efxoutr[i] = rdl * 2.0f - 1e-20f; // small dc bias

        ldl = smpsl[i] * pangainL - ldl * fb;
        rdl = smpsr[i] * pangainR - rdl * fb;

        // LowPass Filter
        ldelay[kl] = ldl = ldl * hidamp + oldl * (1.0f - hidamp);
        rdelay[kr] = rdl = rdl * hidamp + oldr * (1.0f - hidamp);
        oldl = ldl;
        oldr = rdl;

        if (++kl >= dl)
            kl = 0;
        if (++kr >= dr)
            kr = 0;
    }
}


// Parameter control
void Echo::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    if (insertion == 0)
    {
        outvolume = powf(0.01f, (1.0f - Pvolume / 127.0f)) * 4.0f;
        volume = 1.0f;
    }
    else
        volume = outvolume = Pvolume / 127.0f;
    if (Pvolume == 0)
        cleanup();
}


void Echo::setdelay(const unsigned char Pdelay_)
{
    Pdelay = Pdelay_;
    delay = 1 + lrintf(Pdelay / 127.0f * synth->samplerate_f * 1.5f); // 0 .. 1.5 sec
    initdelays();
}


void Echo::setlrdelay(unsigned char Plrdelay_)
{
    float tmp;
    Plrdelay = Plrdelay_;
    tmp = (powf(2.0f, fabsf(Plrdelay - 64.0f) / 64.0f * 9.0f) -1.0f) / 1000.0f * synth->samplerate_f;
    if (Plrdelay < 64.0f)
        tmp = -tmp;
    lrdelay = (int)tmp;
    initdelays();
}


void Echo::setfb(unsigned char Pfb_)
{
    Pfb = Pfb_;
    fb = Pfb / 128.0f;
}


void Echo::sethidamp(unsigned char Phidamp_)
{
    Phidamp = Phidamp_;
    hidamp = 1.0 - Phidamp / 127.0f;
}


void Echo::setpreset(unsigned char npreset)
{
    const int PRESET_SIZE = 7;
    const int NUM_PRESETS = 9;
    unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {

        { 67, 64, 35, 64, 30, 59, 0 },     // Echo 1
        { 67, 64, 21, 64, 30, 59, 0 },     // Echo 2
        { 67, 75, 60, 64, 30, 59, 10 },    // Echo 3
        { 67, 60, 44, 64, 30, 0, 0 },      // Simple Echo
        { 67, 60, 102, 50, 30, 82, 48 },   // Canyon
        { 67, 64, 44, 17, 0, 82, 24 },     // Panning Echo 1
        { 81, 60, 46, 118, 100, 68, 18 },  // Panning Echo 2
        { 81, 60, 26, 100, 127, 67, 36 },  // Panning Echo 3
        { 62, 64, 28, 64, 100, 90, 55 }    // Feedback Echo
    };

    if (npreset >= NUM_PRESETS)
        npreset = NUM_PRESETS - 1;
    for (int n = 0; n < PRESET_SIZE; ++n)
        changepar(n, presets[npreset][n]);
    if (insertion != 0)
        setvolume(presets[npreset][0] / 2); // lower the volume if this is insertion effect
    Ppreset = npreset;
}


void Echo::changepar(int npar, unsigned char value)
{
    switch (npar)
    {
        case 0:
            setvolume(value);
            break;
        case 1:
            setpanning(value);
            break;
        case 2:
            setdelay(value);
            break;
        case 3:
            setlrdelay(value);
            break;
        case 4:
            setlrcross(value);
            break;
        case 5:
            setfb(value);
            break;
        case 6:
            sethidamp(value);
            break;
    }
}


unsigned char Echo::getpar(int npar)
{
    switch (npar)
    {
        case 0: return Pvolume;
        case 1: return Ppanning;
        case 2: return Pdelay;
        case 3: return Plrdelay;
        case 4: return Plrcross;
        case 5: return Pfb;
        case 6: return Phidamp;
        default: break;
    }
    return 0; // in case of bogus parameter number
}
