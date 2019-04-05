/*
    Alienwah.cpp - "AlienWah" effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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

using namespace std;

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Effects/Alienwah.h"

Alienwah::Alienwah(bool insertion_, float *efxoutl_, float *efxoutr_) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0),
    oldl(NULL),
    oldr(NULL)
{
    setpreset(Ppreset);
    cleanup();
    oldclfol = complex<float>(fb, 0.0);
    oldclfor = complex<float>(fb, 0.0);
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
    float lfol;
    float lfor; // Left/Right LFOs
    complex<float> clfol, clfor, out, tmp;
    lfo.effectlfoout(&lfol, &lfor);
    lfol *= depth * PI * 2.0;
    lfor *= depth * PI * 2.0;
    clfol = complex<float>(cosf(lfol + phase) * fb, sinf(lfol + phase) * fb); //rework
    clfor = complex<float>(cosf(lfor + phase) * fb, sinf(lfor + phase) * fb); //rework

    int buffersize = zynMaster->getBuffersize();
    for (int i = 0; i < buffersize; ++i)
    {
        float x = (float)i / (float)buffersize;
        float x1 = 1.0 - x;
        // left
        tmp = clfol * x + oldclfol * x1;

        out = tmp * oldl[oldk];
        // was: out.real() += (1 - abs(fb)) * smpsr[i] * (1.0 - panning);
        out.real() += (1 - abs(fb)) * smpsl[i] * (1.0 - panning);

        oldl[oldk] = out;
        float l = out.real() * 10.0 * (fb + 0.1);

        // right
        tmp = clfor * x + oldclfor * x1;

        out = tmp * oldr[oldk];
        // was: out.real() += (1 - abs(fb)) * smpsr[i] * (1.0 - panning);
        out.real() += (1 - abs(fb)) * smpsr[i] * panning;

        oldr[oldk] = out;
        float r = out.real() * 10.0 * (fb + 0.1);

        if (++oldk >= Pdelay)
            oldk = 0;
        // LRcross
        efxoutl[i] = l * (1.0 - lrcross) + r * lrcross;
        efxoutr[i] = r * (1.0 - lrcross) + l * lrcross;
    }
    oldclfol = clfol;
    oldclfor = clfor;
}


// Cleanup the effect
void Alienwah::cleanup(void)
{
    for (int i = 0; i < Pdelay; ++i)
    {
        oldl[i] = complex<float>(0.0, 0.0);
        oldr[i] = complex<float>(0.0, 0.0);
    }
    oldk = 0;
}


// Parameter control
void Alienwah::setdepth(unsigned char _depth)
{
    Pdepth = _depth;
    depth = Pdepth / 127.0;
}

void Alienwah::setfb(unsigned char _fb)
{
    Pfb = _fb;
    fb = fabs((Pfb - 64.0) / 64.1);
    fb = sqrtf(fb);
    if (fb < 0.4)
        fb = 0.4;
    if (Pfb < 64)
        fb = -fb;
}


void Alienwah::setvolume(unsigned char _volume)
{
    Pvolume = _volume;
    outvolume = Pvolume / 127.0;
    if (insertion == 0)
        volume = 1.0;
    else
        volume = outvolume;
}


void Alienwah::setpanning(unsigned char _panning)
{
    Ppanning = _panning;
    panning = Ppanning / 127.0;
}


void Alienwah::setlrcross(unsigned char _lrcross)
{
    Plrcross = _lrcross;
    lrcross = Plrcross / 127.0;
}


void Alienwah::setphase(unsigned char _phase)
{
    Pphase = _phase;
    phase = (Pphase - 64.0) / 64.0 * PI;
}


void Alienwah::setdelay(unsigned char _delay)
{
    if (oldl != NULL)
        delete [] oldl;
    if (oldr != NULL)
        delete [] oldr;
    Pdelay = (_delay >= MAX_ALIENWAH_DELAY) ? MAX_ALIENWAH_DELAY : _delay;
    oldl = new complex<float>[Pdelay];
    oldr = new complex<float>[Pdelay];
    cleanup();
}


void Alienwah::setpreset(unsigned char npreset)
{
    const int PRESET_SIZE = 11;
    const int NUM_PRESETS = 4;
    unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        // AlienWah1
        { 127, 64, 70, 0, 0, 62, 60, 105, 25, 0, 64 },
        // AlienWah2
        { 127, 64, 73, 106, 0, 101, 60, 105, 17, 0, 64 },
        // AlienWah3
        { 127, 64, 63, 0, 1, 100, 112, 105, 31, 0, 42 },
        // AlienWah4
        { 93, 64, 25, 0, 1, 66, 101, 11, 47, 0, 86 }
    };

    if (npreset >= NUM_PRESETS)
        npreset = NUM_PRESETS - 1;
    for (int n = 0; n < PRESET_SIZE; ++n)
        changepar(n, presets[npreset][n]);
    if (insertion == 0)
        changepar(0, presets[npreset][0] / 2); // lower the volume if this is system effect
    Ppreset = npreset;
}


void Alienwah::changepar(int npar, unsigned char value)
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
}


unsigned char Alienwah::getpar(int npar)
{
    switch (npar)
    {
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
