/*
    Chorus.cpp - Chorus and Flange effects

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

#include <iostream>

using namespace std;

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Effects/Chorus.h"

Chorus::Chorus(bool insertion_, float *const efxoutl_, float *efxoutr_) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0)
{
    dlk = drk = 0;
    maxdelay = (int)(MAX_CHORUS_DELAY / 1000.0 * zynMaster->getSamplerate());
    delayl = new float[maxdelay];
    delayr = new float[maxdelay];

    setPreset(Ppreset);

    lfo.effectLfoOut(&lfol, &lfor);
    dl2 = getDelay(lfol);
    dr2 = getDelay(lfor);
    Cleanup();
}


// get the delay value in samples; xlfo is the current lfo value
float Chorus::getDelay(float xlfo)
{
    float result = (Pflangemode) ? 0 : (delay + xlfo * depth) * zynMaster->getSamplerate();

    //check if it is too big delay (caused bu erroneous setDelay() and setDepth()
    if ((result + 0.5) >= maxdelay)
    {
        cerr << "WARNING: Chorus.C::getDelay(..) too big delay (see setdelay and setdepth funcs.)\n";
        result = maxdelay - 1.0;
    }
    return result;
}


// Apply the effect
void Chorus::out(float *smpsl, float *smpsr)
{
    const float one = 1.0;
    dl1 = dl2;
    dr1 = dr2;
    lfo.effectLfoOut(&lfol, &lfor);

    dl2 = getDelay(lfol);
    dr2 = getDelay(lfor);

    float inL, inR, tmpL, tmpR, tmp;
    int buffersize = zynMaster->getBuffersize();
    for (int i = 0; i < buffersize; ++i)
    {
        inL = tmpL = smpsl[i];
        inR = tmpR = smpsr[i];
        // LRcross
        inL = tmpL * (1.0 - lrcross) + tmpR * lrcross;
        inR = tmpR * (1.0 - lrcross) + tmpL * lrcross;

        // Left channel

        // compute the delay in samples using linear interpolation between the lfo delays
        mdel = (dl1 * (buffersize - i) + dl2 * i) / buffersize;
        if (++dlk >= maxdelay)
            dlk = 0;
        tmp = dlk - mdel + maxdelay * 2.0; // where should I get the sample from

        F2I(tmp, dlhi);
        dlhi %= maxdelay;

        dlhi2 = (dlhi - 1 + maxdelay) % maxdelay;
        dllo = 1.0 - fmodf(tmp, one);
        efxoutl[i] = delayl[dlhi2] * dllo + delayl[dlhi] * (1.0 - dllo);
        delayl[dlk] = inL + efxoutl[i] * fb;

        // Right channel

        // compute the delay in samples using linear interpolation between the lfo delays
        mdel = (dr1 * (buffersize - i) + dr2 * i) / buffersize;
        if (++drk >= maxdelay)
            drk = 0;
        tmp = drk * 1.0 - mdel + maxdelay * 2.0; // where should I get the sample from

        F2I(tmp, dlhi);
        dlhi %= maxdelay;

        dlhi2 = (dlhi - 1 + maxdelay) % maxdelay;
        dllo = 1.0 - fmodf(tmp, one);
        efxoutr[i] = delayr[dlhi2] * dllo + delayr[dlhi] * (1.0 - dllo);
        delayr[dlk] = inR + efxoutr[i] * fb;
    }

    if (Poutsub)
        for (int i = 0; i < buffersize; ++i)
        {
            efxoutl[i] *= -1.0;
            efxoutr[i] *= -1.0;
        }

    for (int i = 0; i < buffersize; ++i)
    {
        efxoutl[i] *= panning;
        efxoutr[i] *= (1.0 - panning);
    }
}

// Cleanup the effect
void Chorus::Cleanup(void)
{
    for (int i = 0; i < maxdelay; ++i)
        delayl[i] = delayr[i] = 0.0;
}


// Parameter control
void Chorus::setDepth(unsigned char Pdepth)
{
    this->Pdepth = Pdepth;
    depth = (powf(8.0, (Pdepth / 127.0) * 2.0) - 1.0) / 1000.0; // seconds
}


void Chorus::setDelay(unsigned char Pdelay)
{
    this->Pdelay=Pdelay;
    delay = (powf(10.0, (Pdelay / 127.0) * 2.0) - 1.0) / 1000.0; // seconds
}


void Chorus::setFb(unsigned char Pfb)
{
    this->Pfb = Pfb;
    fb = (Pfb - 64.0) / 64.1;
}


void Chorus::setVolume(unsigned char Pvolume)
{
    this->Pvolume = Pvolume;
    outvolume = Pvolume / 127.0;
    if (insertion == 0)
        volume = 1.0;
    else
        volume = outvolume;
}


void Chorus::setPanning(unsigned char Ppanning)
{
    this->Ppanning = Ppanning;
    panning = Ppanning / 127.0;
}


void Chorus::setLrCross(unsigned char Plrcross)
{
    this->Plrcross = Plrcross;
    lrcross = Plrcross / 127.0;
}


void Chorus::setPreset(unsigned char npreset)
{
    const int PRESET_SIZE = 12;
    const int NUM_PRESETS = 10;
    unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        // Chorus1
        { 64, 64, 50, 0, 0, 90, 40, 85, 64, 119, 0, 0 },
        // Chorus2
        {64, 64, 45, 0, 0, 98, 56, 90, 64, 19, 0, 0 },
        // Chorus3
        {64, 64, 29, 0, 1, 42, 97, 95, 90, 127, 0, 0 },
        // Celeste1
        {64, 64, 26, 0, 0, 42, 115, 18, 90, 127, 0, 0 },
        // Celeste2
        {64, 64, 29, 117, 0, 50, 115, 9, 31, 127, 0, 1 },
        // Flange1
        {64, 64, 57, 0, 0, 60, 23, 3, 62, 0, 0, 0 },
        // Flange2
        {64, 64, 33, 34, 1, 40, 35, 3, 109, 0, 0, 0 },
        // Flange3
        {64, 64, 53, 34, 1, 94, 35, 3, 54, 0, 0, 1 },
        // Flange4
        {64, 64, 40, 0, 1, 62, 12, 19, 97, 0, 0, 0 },
        // Flange5
        {64, 64, 55, 105, 0, 24, 39, 19, 17, 0, 0, 1 }
    };

    if (npreset >= NUM_PRESETS)
        npreset = NUM_PRESETS - 1;
    for (int n = 0; n < PRESET_SIZE; ++n)
        changePar(n, presets[npreset][n]);
    Ppreset = npreset;
}


void Chorus::changePar(int npar, unsigned char value)
{
    switch (npar)
    {
        case 0:
            setVolume(value);
            break;
        case 1:
            setPanning(value);
            break;
        case 2:
            lfo.Pfreq = value;
            lfo.updateParams();
            break;
        case 3:
            lfo.Prandomness = value;
            lfo.updateParams();
            break;
        case 4:
            lfo.PLFOtype = value;
            lfo.updateParams();
            break;
        case 5:
            lfo.Pstereo = value;
            lfo.updateParams();
            break;
        case 6:
            setDepth(value);
            break;
        case 7:
            setDelay(value);
            break;
        case 8:
            setFb(value);
            break;
        case 9:
            setLrCross(value);
            break;
        case 10:
            Pflangemode = (value > 1) ? 1 : value;
            break;
        case 11:
            Poutsub = (value > 1) ? 1 : value;
            break;
    }
}


unsigned char Chorus::getPar(int npar) const
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
        case 7:  return Pdelay;
        case 8:  return Pfb;
        case 9:  return Plrcross;
        case 10: return Pflangemode;
        case 11: return Poutsub;
        default: return 0;
    }
}
