/*
    Phaser.cpp - Phaser effect

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

#include "../Misc/Util.h"
#include "../Misc/Master.h"
#include "Phaser.h"

#define PHASER_LFO_SHAPE 2

Phaser::Phaser(bool insertion_, float *efxoutl_, float *efxoutr_) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0),
    oldl(NULL),
    oldr(NULL)
{
    setpreset(Ppreset);
    cleanup();
};


Phaser::~Phaser()
{
    if (oldl != NULL)
        delete [] oldl;
    if (oldr != NULL)
        delete [] oldr;
}


// Effect output
void Phaser::out(float *smpsl, float *smpsr)
{
    //int j;
    float lfol, lfor, lgain, rgain, tmp;

    lfo.effectlfoout(&lfol, &lfor);
    lgain = lfol;
    rgain = lfor;
    lgain = (expf(lgain * PHASER_LFO_SHAPE) - 1)
            / (expf(PHASER_LFO_SHAPE) - 1.0);
    rgain = (expf(rgain * PHASER_LFO_SHAPE) - 1)
            / (expf(PHASER_LFO_SHAPE) - 1.0);

    lgain = 1.0 - phase * (1.0 - depth) - (1.0 - phase) * lgain * depth;
    lgain = (lgain > 1.0) ? 1.0 : lgain;
    rgain = 1.0 - phase * (1.0 - depth) - (1.0 - phase) * rgain * depth;
    rgain = (rgain > 1.0) ? 1.0 : rgain;

    int buffersize = zynMaster->getBuffersize();
    float bufsize_f = buffersize;
    for (int i = 0; i < buffersize; ++i)
    {
        float x = (float)i / bufsize_f;
        float x1 = 1.0 - x;
        float gl = lgain * x + oldlgain * x1;
        float gr = rgain * x + oldrgain * x1;
        float inl = smpsl[i] * panning + fbl;
        float inr = smpsr[i] * (1.0 - panning) + fbr;

        // Phasing routine
        for (int j = 0; j < Pstages * 2; ++j)
        {
            // Left channel
            tmp = oldl[j];
            oldl[j] = gl * tmp + inl;
            inl = tmp - gl * oldl[j];
            // Right channel
            tmp = oldr[j];
            oldr[j] = gr * tmp + inr;
            inr = tmp - gr * oldr[j];
        }

        // Left/Right crossing
        float l = inl;
        float r = inr;
        inl = l * (1.0 - lrcross) + r * lrcross;
        inr = r * (1.0 - lrcross) + l * lrcross;
        fbl = inl * fb;
        fbr = inr * fb;
        efxoutl[i] = inl;
        efxoutr[i] = inr;
    }
    oldlgain = lgain;
    oldrgain = rgain;
    if (Poutsub)
        for (int i = 0; i < buffersize; ++i)
        {
            efxoutl[i] *= -1.0;
            efxoutr[i] *= -1.0;
        }
}


// Cleanup the effect
void Phaser::cleanup(void)
{
    fbl = fbr = oldlgain = oldrgain = 0.0;
    for (int i = 0; i < Pstages * 2; ++i)
        oldl[i] = oldr[i] = 0.0;
}


// Parameter control
void Phaser::setdepth(unsigned char _depth)
{
    Pdepth = _depth;
    depth = Pdepth / 127.0;
}


void Phaser::setfb(unsigned char _fb)
{
    Pfb = _fb;
    fb = (Pfb - 64.0) / 64.1;
}


void Phaser::setvolume(unsigned char _volume)
{
    Pvolume = _volume;
    outvolume = Pvolume / 127.0;
    volume = (!insertion) ? 1.0 : outvolume;
}


void Phaser::setpanning(unsigned char _panning)
{
    Ppanning = _panning;
    panning = Ppanning / 127.0;
}

void Phaser::setlrcross(unsigned char _lrcross)
{
    Plrcross = _lrcross;
    lrcross = Plrcross / 127.0;
}


void Phaser::setstages(unsigned char _stages)
{
    if (oldl != NULL)
        delete [] oldl;
    if (oldr != NULL)
        delete [] oldr;
    Pstages = (_stages >= MAX_PHASER_STAGES) ? MAX_PHASER_STAGES - 1 : _stages;
    oldl = new float[Pstages * 2];
    oldr = new float[Pstages * 2];
    cleanup();
}


void Phaser::setphase(unsigned char _phase)
{
    Pphase = _phase;
    phase = Pphase / 127.0;
}


void Phaser::setpreset(unsigned char npreset)
{
    const int PRESET_SIZE = 12;
    const int NUM_PRESETS = 6;
    unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        // Phaser1
        { 64, 64, 36, 0, 0, 64, 110, 64, 1, 0, 0, 20 },
        // Phaser2
        { 64, 64, 35, 0, 0, 88, 40, 64, 3, 0, 0, 20 },
        // Phaser3
        { 64, 64, 31, 0, 0, 66, 68, 107, 2, 0, 0, 20 },
        // Phaser4
        { 39, 64, 22, 0, 0, 66, 67, 10, 5, 0, 1, 20 },
        // Phaser5
        { 64, 64, 20, 0, 1, 110, 67, 78, 10, 0, 0, 20 },
        // Phaser6
        { 64, 64, 53, 100, 0, 58, 37, 78, 3, 0, 0, 20 }
    };
    if (npreset >= NUM_PRESETS)
        npreset = NUM_PRESETS - 1;
    for (int n = 0; n < PRESET_SIZE; ++n)
        changepar(n, presets[npreset][n]);
    Ppreset = npreset;
}


void Phaser::changepar(int npar, unsigned char value)
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
            setstages(value);
            break;
        case 9:
            setlrcross(value);
            break;
        case 10:
            Poutsub = (value > 1) ? 1 : value;
            break;
        case 11:
            setphase(value);
            break;
    }
}


unsigned char Phaser::getpar(int npar) const
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
        case 8:  return Pstages;
        case 9:  return Plrcross;
        case 10: return Poutsub;
        case 11: return Pphase;
        default: break;
    }
    return 0;
}
