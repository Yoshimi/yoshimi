/*
    Phaser.cpp - Phaser effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
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

    This file is a derivative of a ZynAddSubFX original.

*/

#include "Misc/SynthEngine.h"
#include "Effects/Phaser.h"
#include "Misc/NumericFuncs.h"

#include <algorithm>


using func::limit;
using func::invSignal;


#define PHASER_LFO_SHAPE 2
#define ONE_  0.99999f        // To prevent LFO ever reaching 1.0f for filter stability purposes
#define ZERO_ 0.00001f        // Same idea as above.

namespace {
    const int PRESET_SIZE = 15;
    const int NUM_PRESETS = 12;
    unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        // Phaser
        // 0   1    2    3  4   5     6   7   8    9 10   11 12  13 14
        {64, 64, 36,  0,   0, 64,  110, 64,  1,  0,   0, 20, 0, 0,  0 },
        {64, 64, 35,  0,   0, 88,  40,  64,  3,  0,   0, 20, 0,  0, 0 },
        {64, 64, 31,  0,   0, 66,  68,  107, 2,  0,   0, 20, 0,  0, 0 },
        {39, 64, 22,  0,   0, 66,  67,  10,  5,  0,   1, 20, 0,  0, 0 },
        {64, 64, 20,  0,   1, 110, 67,  78,  10, 0,   0, 20, 0,  0, 0 },
        {64, 64, 53,  100, 0, 58,  37,  78,  3,  0,   0, 20, 0,  0, 0 },
        // APhaser
        // 0   1    2   3   4   5     6   7   8    9 10   11 12  13 14
        {64, 64, 14,  0,   1, 64,  64,  40,  4,  10,  0, 110,1,  20, 1 },
        {64, 64, 14,  5,   1, 64,  70,  40,  6,  10,  0, 110,1,  20, 1 },
        {64, 64, 9,   0,   0, 64,  60,  40,  8,  10,  0, 40, 0,  20, 1 },
        {64, 64, 14,  10,  0, 64,  45,  80,  7,  10,  1, 110,1,  20, 1 },
        {25, 64, 127, 10,  0, 64,  25,  16,  8,  100, 0, 25, 0,  20, 1 },
        {64, 64, 1,   10,  1, 64,  70,  40,  12, 10,  0, 110,1,  20, 1 }
    };
}


Phaser::Phaser(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0),
    lfo(_synth),
    oldl(NULL),
    oldr(NULL),
    xn1l(NULL),
    xn1r(NULL),
    yn1l(NULL),
    yn1r(NULL),
    synth(_synth)
{
    analog_setup();
    setpreset(Ppreset);
    Pchanged = false;
    cleanup();
}


void Phaser::analog_setup()
{
    //model mismatch between JFET devices
    offset[0]  = -0.2509303f;
    offset[1]  = 0.9408924f;
    offset[2]  = 0.998f;
    offset[3]  = -0.3486182f;
    offset[4]  = -0.2762545f;
    offset[5]  = -0.5215785f;
    offset[6]  = 0.2509303f;
    offset[7]  = -0.9408924f;
    offset[8]  = -0.998f;
    offset[9]  = 0.3486182f;
    offset[10] = 0.2762545f;
    offset[11] = 0.5215785f;

    barber = 0;  //Deactivate barber pole phasing by default

    mis       = 1.0f;
    Rmin      = 625.0f; // 2N5457 typical on resistance at Vgs = 0
    Rmax      = 22000.0f; // Resistor parallel to FET
    Rmx       = Rmin / Rmax;
    Rconst    = 1.0f + Rmx; // Handle parallel resistor relationship
    C         = 0.00000005f; // 50 nF
    CFs       = 2.0f * synth->samplerate_f * C;
    invperiod = 1.0f / synth->buffersize_f;
}


Phaser::~Phaser()
{
    if (oldl != NULL)
        delete [] oldl;
    if (oldr != NULL)
        delete [] oldr;

    if (xn1l)
        delete[] xn1l;
    if (yn1l)
        delete[] yn1l;
    if (xn1r)
        delete[] xn1r;
    if (yn1r)
        delete[] yn1r;
}


// Effect output
void Phaser::out(float *smpsl, float *smpsr)
{
    if (Panalog)
        AnalogPhase(smpsl, smpsr);
    else
        NormalPhase(smpsl, smpsr);
}


void Phaser::AnalogPhase(float *smpsl, float *smpsr)
{
    float lfoVall;
    float lfoValr;
    float modl;
    float modr;
    float gl;
    float gr;
    float hpfl = 0;
    float hpfr = 0;

    lfo.effectlfoout(&lfoVall, &lfoValr);
    modl = lfoVall * width + (depth - 0.5f);
    modr = lfoValr * width + (depth - 0.5f);

    modl = limit(modl, ZERO_, ONE_);
    modr = limit(modr, ZERO_, ONE_);

    if (Phyper)
    {
        // Triangle wave squared is approximately sine on bottom, triangle on top
        // Result is exponential sweep more akin to filter in synth with
        // exponential generator circuitry.
        modl *= modl;
        modr *= modr;
    }

    // g.,g. is Vp - Vgs. Typical FET drain-source resistance follows constant/[1-sqrt(Vp - Vgs)]
    modl = sqrtf(1.0f - modl);
    modr = sqrtf(1.0f - modr);

    diffr = (modr - oldrgain) * invperiod;
    diffl = (modl - oldlgain) * invperiod;

    gl = oldlgain;
    gr = oldrgain;
    oldlgain = modl;
    oldrgain = modr;

   for (int i = 0; i < synth->sent_buffersize; ++i)
   {
        gl += diffl; // Linear interpolation between LFO samples
        gr += diffr;

        float xnl(smpsl[i] * pangainL.getAndAdvanceValue());
        float xnr(smpsr[i] * pangainR.getAndAdvanceValue());

        if (barber)
        {
            gl = fmodf((gl + 0.25f), ONE_);
            gr = fmodf((gr + 0.25f), ONE_);
        }

        xnl = applyPhase(xnl, gl, fbl, hpfl, yn1l, xn1l);
        xnr = applyPhase(xnr, gr, fbr, hpfr, yn1r, xn1r);


        fbl = xnl * fb;
        fbr = xnr * fb;
        efxoutl[i] = xnl;
        efxoutr[i] = xnr;
    }

    if (Poutsub)
    {
        invSignal(efxoutl, synth->sent_buffersize);
        invSignal(efxoutr, synth->sent_buffersize);
    }
}


float Phaser::applyPhase(float x, float g, float fb,
                         float &hpf, float *yn1, float *xn1)
{
    for (int j = 0; j < Pstages; ++j)
    { //Phasing routine
        mis = 1.0f + offsetpct * offset[j];

        // This is symmetrical.
        // FET is not, so this deviates slightly, however sym dist. is
        // better sounding than a real FET.
        float d = (1.0f + 2.0f * (0.25f + g) * hpf * hpf * distortion) * mis;
        Rconst = 1.0f + mis * Rmx;

        // This is 1/R. R is being modulated to control filter fc.
        float b    = (Rconst - g) / (d * Rmin);
        float gain = (CFs - b) / (CFs + b);
        yn1[j] = (gain * (x + yn1[j]) - xn1[j]) + 1e-12; // anti-denormal

        // high pass filter:
        // Distortion depends on the high-pass part of the AP stage.
        hpf = yn1[j] + (1.0f - gain) * xn1[j];

        xn1[j] = x;
        x = yn1[j];
        if (j == 1)
            x += fb; // Insert feedback after first phase stage
    }
    return x;
}


void Phaser::NormalPhase(float *smpsl, float *smpsr)
{
    float lfol, lfor, lgain, rgain, tmp;

    lfo.effectlfoout(&lfol, &lfor);
    lgain = lfol;
    rgain = lfor;
    lgain = (expf(lgain * PHASER_LFO_SHAPE) - 1)
            / (expf(PHASER_LFO_SHAPE) - 1.0f);
    rgain = (expf(rgain * PHASER_LFO_SHAPE) - 1)
            / (expf(PHASER_LFO_SHAPE) - 1.0f);

    lgain = 1.0f - phase * (1.0f - depth) - (1.0f - phase) * lgain * depth;
    lgain = limit(lgain,ZERO_,ONE_);//(lgain > 1.0f) ? 1.0f : lgain;
    rgain = 1.0f - phase * (1.0f - depth) - (1.0f - phase) * rgain * depth;
    rgain = limit(rgain,ZERO_,ONE_);//(rgain > 1.0f) ? 1.0f : rgain;

    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
        float x = (float)i / synth->sent_buffersize_f;
        float x1 = 1.0f - x;
        float gl = lgain * x + oldlgain * x1;
        float gr = rgain * x + oldrgain * x1;
        float inl = smpsl[i] * pangainL.getAndAdvanceValue() + fbl;
        float inr = smpsr[i] * pangainR.getAndAdvanceValue() + fbr;

        // Phasing routine
        for (int j = 0; j < Pstages * 2; ++j)
        {
            // Left channel
            tmp = oldl[j];
            oldl[j] = gl * tmp + inl;
            inl = (tmp - gl * oldl[j]) + 1e-12; // anti-denormal
            // Right channel
            tmp = oldr[j];
            oldr[j] = gr * tmp + inr;
            inr = (tmp - gr * oldr[j]) + 1e-12; // anti-denormal
        }

        // Left/Right crossing
        float l = inl;
        float r = inr;
        inl = l * (1.0f - lrcross.getValue()) + r * lrcross.getValue();
        inr = r * (1.0f - lrcross.getValue()) + l * lrcross.getValue();
        lrcross.advanceValue();
        fbl = inl * fb;
        fbr = inr * fb;
        efxoutl[i] = inl;
        efxoutr[i] = inr;
    }
    oldlgain = lgain;
    oldrgain = rgain;
    if (Poutsub)
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            efxoutl[i] *= -1.0f;
            efxoutr[i] *= -1.0f;
        }
}


// Cleanup the effect
void Phaser::cleanup(void)
{
    fbl = fbr = oldlgain = oldrgain = 0.0f;
    memset(oldl, 0, sizeof(float)*Pstages * 2);
    memset(oldr, 0, sizeof(float)*Pstages * 2);
    memset(xn1l, 0, sizeof(float)*Pstages);
    memset(xn1r, 0, sizeof(float)*Pstages);
    memset(yn1l, 0, sizeof(float)*Pstages);
    memset(yn1r, 0, sizeof(float)*Pstages);
}


// Parameter control
void Phaser::setdepth(unsigned char Pdepth_)
{
    Pdepth = Pdepth_;
    depth = Pdepth / 127.0f;
}


void Phaser::setwidth(unsigned char Pwidth_)
{
    Pwidth = Pwidth_;
    width = Pwidth / 127.0f;
}


void Phaser::setfb(unsigned char Pfb_)
{
    Pfb = Pfb_;
    fb = (Pfb - 64.0f) / 64.1f;
}


void Phaser::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    float tmp = Pvolume / 127.0f;
    outvolume.setTargetValue(tmp);
    volume.setTargetValue((!insertion) ? 1.0f : tmp);
}


void Phaser::setdistortion(unsigned char Pdistortion_)
{
    Pdistortion = Pdistortion_;
    distortion = (float)Pdistortion / 127.0f;
}


void Phaser::setoffset(unsigned char Poffset_)
{
    Poffset = Poffset_;
    offsetpct     = (float)Poffset / 127.0f;
}


void Phaser::setstages(unsigned char Pstages_)
{
    if (oldl != NULL)
        delete [] oldl;
    if (xn1l)
        delete[] xn1l;
    if (yn1l)
        delete[] yn1l;
    if (oldr != NULL)
        delete [] oldr;
    if (xn1r)
        delete[] xn1r;
    if (yn1r)
        delete[] yn1r;

    Pstages = Pstages_;
    oldl = new float[Pstages * 2];
    oldr = new float[Pstages * 2];
    xn1l = new float[Pstages];
    xn1r = new float[Pstages];
    yn1l = new float[Pstages];
    yn1r = new float[Pstages];
    cleanup();
}


void Phaser::setphase(unsigned char Pphase_)
{
    Pphase = Pphase_;
    phase = Pphase / 127.0;
}


void Phaser::setpreset(unsigned char npreset)
{
    if (npreset < 0xf)
    {
        if (npreset >= NUM_PRESETS)
            npreset = NUM_PRESETS - 1;
        for (int n = 0; n < PRESET_SIZE; ++n)
            changepar(n, presets[npreset][n]);
        Ppreset = npreset;
    }
    else
    {
        unsigned char preset = npreset & 0xf;
        unsigned char param = npreset >> 4;
        if (param == 0xf)
            param = 0;
        changepar(param, presets[preset][param]);
    }
    Pchanged = false;
}


void Phaser::changepar(int npar, unsigned char value)
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
            barber = (2 == value);
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
            setoffset(value);
            break;

        case 10:
            Poutsub = (value > 1) ? 1 : value;
            break;

        case 11:
            setphase(value);
            setwidth(value);
            break;

        case 12:
            Phyper = std::min(int(value), 1);
            break;

        case 13:
            setdistortion(value);
            break;

        case 14:
            Panalog = value;
            break;
    }
    Pchanged = true;
}


unsigned char Phaser::getpar(int npar)
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
        case 8:  return Pstages;
        case 9:  return Plrcross;
            return Poffset;      // same
        case 10: return Poutsub;
        case 11: return Pphase;
            return Pwidth;      // same
        case 12: return Phyper;
        case 13: return Pdistortion;
        case 14: return Panalog;
        default: break;
    }
    return 0;
}


float Phaserlimit::getlimits(CommandBlock *getData)
{
    int value = getData->data.value;
    int control = getData->data.control;
    int request = getData->data.type & TOPLEVEL::type::Default; // clear flags
    int presetNum = getData->data.engine;
    int min = 0;
    int max = 127;

    int def = presets[presetNum][control];
    unsigned char canLearn = TOPLEVEL::type::Learnable;
    unsigned char isInteger = TOPLEVEL::type::Integer;
    switch (control)
    {
        case 0:
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
            min = 1;
            max = 12;
            canLearn = 0;
            break;
        case 9:
            break;
        case 10:
            canLearn = 0;
            max = 1;
            break;
        case 11:
            break;
        case 12:
            canLearn = 0;
            max = 1;
            break;
        case 13:
            break;
        case 14:
            max = 1;
            canLearn = 0;
            break;
        case 16:
            max = 11;
            canLearn = 0;
            break;
        default:
            getData->data.type |= TOPLEVEL::type::Error;
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

