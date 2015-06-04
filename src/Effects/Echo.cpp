/*
    Echo.cpp - Echo effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Misc/AntiDenormals.h"
#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Echo.h"

Echo::Echo(bool insertion_, float* efxoutl_, float* efxoutr_) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0),
    Pvolume(50),
    Ppanning(64),
    Pdelay(60),
    Plrdelay(100),
    Plrcross(100),
    Pfb(40),
    Phidamp(60),
    lrdelay(0),
    ldelay(NULL),
    rdelay(NULL),
    fader6db(new Fader(2.0))
{
    setpreset(Ppreset);
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
    oldl = oldr = 0.0;
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
    int buffersize = zynMaster->getBuffersize();
    for (int i = 0; i < buffersize; ++i)
    {
        ldl = ldelay[kl];
        rdl = rdelay[kr];
        l = ldl * (1.0 - lrcross) + rdl * lrcross;
        r = rdl * (1.0 - lrcross) + ldl * lrcross;
        ldl = l;
        rdl = r;

        efxoutl[i] = ldl * 2.0;
        efxoutr[i] = rdl * 2.0;

        ldl = smpsl[i] * panning - ldl * fb;
        rdl = smpsr[i] * (1.0 - panning) - rdl * fb;

        // LowPass Filter
        ldelay[kl] = ldl = ldl * hidamp + oldl * (1.0 - hidamp);
        rdelay[kr] = rdl = rdl * hidamp + oldr * (1.0 - hidamp);
        oldl = ldl;
        oldr = rdl;

        if (++kl >= dl)
            kl = 0;
        if (++kr >= dr)
            kr = 0;
    }
}

// Parameter control
void Echo::setvolume(unsigned char value)
{
    Pvolume = value;
    if (insertion == 0)
    {
        if (NULL != fader6db)
            outvolume = fader6db->Level(Pvolume);
        else
            outvolume = powf(0.01, (1.0 - Pvolume / 127.0)) * 4.0;
        volume = 1.0;
    }
    else
        volume = outvolume = Pvolume / 127.0;
    if (Pvolume == 0)
        cleanup();
}

void Echo::setpanning(unsigned char _panning)
{
    Ppanning = _panning;
    panning = (Ppanning + 0.5) / 127.0;
}

void Echo::setdelay(const unsigned char _delay)
{
    Pdelay = _delay;
    delay = 1 + (int)(Pdelay / 127.0 * zynMaster->getSamplerate() * 1.5); // 0 .. 1.5 sec
    initdelays();
}

void Echo::setlrdelay(unsigned char _lrdelay)
{
    float tmp;
    Plrdelay = _lrdelay;
    tmp = (powf(2, fabsf(Plrdelay - 64.0) / 64.0 * 9) -1.0) / 1000.0 * zynMaster->getSamplerate();
    if (Plrdelay < 64.0)
        tmp = -tmp;
    lrdelay = (int)tmp;
    initdelays();
}

void Echo::setlrcross(unsigned char _lrcross)
{
    Plrcross = _lrcross;
    lrcross = Plrcross / 127.0 * 1.0;
}

void Echo::setfb(unsigned char _fb)
{
    Pfb = _fb;
    fb = Pfb / 128.0;
}

void Echo::sethidamp(unsigned char _hidamp)
{
    Phidamp = _hidamp;
    hidamp = 1.0 - Phidamp / 127.0;
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


unsigned char Echo::getpar(int npar) const
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
