/*
    Distorsion.cpp - Distorsion effect

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

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Effects/Distorsion.h"

// Waveshape (this is called by OscilGen::waveshape and Distorsion::process)
void waveshapesmps(int n, float *smps, unsigned char type, unsigned char drive)
{
    int i;
    float ws = drive / 127.0;
    float tmpv;

    switch (type)
    {
        case 1:
            ws = powf( 10.0f, ws * ws * 3.0f) - 1.0 + 0.001; // Arctangent
            for (i = 0; i < n; ++i)
                smps[i] = atanf(smps[i] * ws) / atanf(ws);
            break;
        case 2:
            ws = ws * ws * 32.0 + 0.0001; // Asymmetric
            tmpv = (ws < 1.0) ? sinf(ws) + 0.1 : 1.1;
            for (i = 0; i < n; ++i)
                smps[i] = sinf(smps[i] * (0.1 + ws - ws * smps[i])) / tmpv;
            break;
        case 3:
            ws = ws * ws * ws * 20.0 + 0.0001; // Pow
            for (i = 0; i < n; ++i)
            {
                smps[i] *= ws;
                if (fabsf(smps[i]) < 1.0)
                {
                    smps[i] = (smps[i] - powf(smps[i], 3.0)) * 3.0;
                    if (ws < 1.0)
                        smps[i] /= ws;
                } else
                    smps[i] = 0.0;
            }
            break;
        case 4:
            ws = ws * ws * ws * 32.0 + 0.0001; // Sine
            tmpv = (ws < 1.57) ? sinf(ws) : 1.0;
            for (i = 0; i < n; ++i)
                smps[i] = sinf(smps[i] * ws) / tmpv;
            break;
        case 5:
            ws = ws * ws + 0.000001; // Quantisize
            for (i = 0; i < n; ++i)
                smps[i] = floorf(smps[i] / ws + 0.5) * ws;
            break;
        case 6:
            ws = ws * ws * ws * 32 + 0.0001; // Zigzag
            tmpv = (ws < 1.0) ? sinf(ws) : 1.0;
            for (i = 0; i < n; ++i)
                smps[i] = asinf(sinf(smps[i] * ws)) / tmpv;
            break;
        case 7:
            ws = powf(2.0, -ws * ws * 8.0); // Limiter
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i];
                if (fabsf(tmp) > ws)
                {
                    smps[i] = (tmp >= 0.0) ? 1.0 : -1.0;
                }
                else
                    smps[i] /= ws;
            }
            break;
        case 8:
            ws = powf(2.0f, -ws * ws * 8.0); // Upper Limiter
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i];
                if (tmp > ws)
                    smps[i] = ws;
                smps[i] *= 2.0;
            }
            break;
        case 9:
            ws = powf(2.0f, -ws * ws * 8.0); // Lower Limiter
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i];
                if (tmp < -ws)
                    smps[i] = -ws;
                smps[i] *= 2.0;
            }
            break;
        case 10:
            ws = (powf(2.0f, ws * 6.0) - 1.0) / powf(2.0, 6.0); // Inverse Limiter
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i];
                if (fabsf(tmp) > ws)
                {
                    smps[i] = (tmp >= 0.0) ? (tmp - ws) : (tmp + ws);
                }
                else
                    smps[i] = 0;
            }
            break;
        case 11:
            ws = powf(5.0f, ws * ws * 1.0) - 1.0; // Clip
            for (i = 0; i < n; ++i)
                smps[i] = smps[i] * (ws + 0.5) * 0.9999 - floorf(0.5f + smps[i] * (ws + 0.5) * 0.9999);
            break;
        case 12:
            ws = ws * ws * ws * 30 + 0.001; // Asym2
            tmpv = (ws < 0.3) ? ws : 1.0;
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i] * ws;
                if (tmp > -2.0 && tmp < 1.0)
                    smps[i] = tmp * (1.0 - tmp) * (tmp + 2.0) / tmpv;
                else
                    smps[i] = 0.0;
            }
            break;
        case 13:
            ws = ws * ws * ws * 32.0 + 0.0001; // Pow2
            tmpv = (ws < 1.0) ? (ws * (1 + ws) / 2.0) : 1.0;
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i] * ws;
                if (tmp >- 1.0 && tmp < 1.618034)
                    smps[i] = tmp * (1.0 - tmp) / tmpv;
                else if (tmp > 0.0)
                    smps[i] = -1.0;
                else
                    smps[i] = -2.0;
            }
            break;
        case 14:
            ws = powf(ws, 5.0f) * 80.0 + 0.0001; // sigmoid
            tmpv = (ws > 10.0) ? 0.5 : 0.5-1.0 / (expf(ws) + 1.0);
            for (i = 0; i < n; ++i)
            {
                float tmp = smps[i] * ws;
                if (tmp < -10.0)
                    tmp = -10.0;
                else if (tmp > 10.0)
                    tmp = 10.0;
                tmp = 0.5 - 1.0 / (expf(tmp) + 1.0);
                smps[i] = tmp / tmpv;
            }
            break;
        // todo update to Distorsion::changepar (Ptype max) if there is added
        // more waveshapings functions
    }
}


Distorsion::Distorsion(bool insertion_, float *efxoutl_, float *efxoutr_) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0)
{
    lpfl = new AnalogFilter(2, 22000, 1, 0);
    lpfr = new AnalogFilter(2, 22000, 1, 0);
    hpfl = new AnalogFilter(3, 20, 1, 0);
    hpfr = new AnalogFilter(3, 20, 1, 0);

    // default values
    Pvolume = 50;
    Plrcross = 40;
    Pdrive = 90;
    Plevel = 64;
    Ptype = 0;
    Pnegate = 0;
    Plpf = 127;
    Phpf = 0;
    Pstereo = 0;
    Pprefiltering = 0;

    setpreset(Ppreset);
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
void Distorsion::cleanup()
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
    if (Pstereo != 0)
    {   // stereo
        lpfr->filterout(efxoutr);
        hpfr->filterout(efxoutr);
    }
}


// Effect output
void Distorsion::out(float *smpsl, float *smpsr)
{
    int buffersize = zynMaster->getBuffersize();
    float l, r, lout, rout;

    float inputvol = powf(5.0, (Pdrive - 32.0) / 127.0);
    if (Pnegate != 0)
        inputvol *= -1.0;

    if (Pstereo != 0)
    {   //Stereo
        for (int i = 0; i < buffersize; ++i)
        {
            efxoutl[i] = smpsl[i] * inputvol * (1.0 - panning);
            efxoutr[i] = smpsr[i] * inputvol * panning;
        }
    } else {
        for (int i = 0; i < buffersize; ++i)
        {
            efxoutl[i] = (smpsl[i] * panning + smpsr[i] * (1.0 - panning)) * inputvol;
        }
    }

    if (Pprefiltering != 0)
        applyfilters(efxoutl, efxoutr);

    // no optimised, yet (no look table)
    waveshapesmps(buffersize, efxoutl, Ptype + 1, Pdrive);
    if (Pstereo != 0)
        waveshapesmps(buffersize, efxoutr, Ptype + 1, Pdrive);

    if (Pprefiltering == 0)
        applyfilters(efxoutl, efxoutr);

    if (Pstereo == 0)
        memcpy(efxoutr, efxoutl, buffersize * sizeof(float));
        //for (i = 0; i < buffersize; ++i)
        //    efxoutr[i] = efxoutl[i];

    float level = dB2rap(60.0 * Plevel / 127.0 - 40.0);
    for (int i = 0; i < buffersize; ++i)
    {
        lout = efxoutl[i];
        rout = efxoutr[i];
        l = lout * (1.0 - lrcross) + rout * lrcross;
        r = rout * (1.0 - lrcross) + lout * lrcross;
        lout = l;
        rout = r;
        efxoutl[i] = lout * 2.0 * level;
        efxoutr[i] = rout * 2.0 *level;
    }
}


// Parameter control
void Distorsion::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    if(insertion == 0)
    {
        outvolume = pow(0.01f, (1.0 - Pvolume / 127.0)) * 4.0;
        volume = 1.0;
    }
    else
         volume = outvolume = Pvolume / 127.0;
    if (Pvolume == 0.0)
        cleanup();
}


void Distorsion::setpanning(unsigned char Ppanning_)
{
    Ppanning = Ppanning_;
    panning = (Ppanning + 0.5) / 127.0;
}


void Distorsion::setlrcross(unsigned char Plrcross_)
{
    Plrcross = Plrcross_;
    lrcross = Plrcross / 127.0;
}


void Distorsion::setlpf(unsigned char Plpf_)
{
    Plpf = Plpf_;
    float fr = expf(powf(Plpf / 127.0, 0.5f) * logf(25000.0f)) + 40;
    lpfl->setfreq(fr);
    lpfr->setfreq(fr);
}


void Distorsion::sethpf(unsigned char Phpf_)
{
    Phpf = Phpf_;
    float fr = expf(powf(Phpf / 127.0, 0.5f) * logf(25000.0f)) + 20.0;
    hpfl->setfreq(fr);
    hpfr->setfreq(fr);
}


void Distorsion::setpreset(unsigned char npreset)
{
    const int PRESET_SIZE = 11;
    const int NUM_PRESETS = 6;
    unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
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
        changepar(0, (int)(presets[npreset][0] / 1.5)); // lower the volume if this is system effect
    Ppreset = npreset;
    cleanup();
}


void Distorsion::changepar(int npar, unsigned char value)
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
            setlrcross(value);
            break;
        case 3:
            Pdrive = value;
            break;
        case 4:
            Plevel = value;
            break;
        case 5:
            if (value > 13)
                Ptype = 13; // this must be increased if more distorsion types are added
            else
                Ptype = value;
            break;
        case 6:
            if (value > 1)
                Pnegate = 1;
            else
                Pnegate = value;
            break;
        case 7:
            setlpf(value);
            break;
        case 8:
            sethpf(value);
            break;
        case 9:
            if (value > 1)
                Pstereo = 1;
            else
                Pstereo = value;
            break;
        case 10:
            Pprefiltering = value;
            break;
    }
}


unsigned char Distorsion::getpar(int npar)
{
    switch (npar)
    {
        case 0:  return Pvolume;
        case 1:  return Ppanning;
        case 2:  return Plrcross;
        case 3:  return Pdrive;
        case 4:  return Plevel;
        case 5:  return Ptype;
        case 6:  return Pnegate;
        case 7:  return Plpf;
        case 8:  return Phpf;
        case 9:  return Pstereo;
        case 10: return Pprefiltering;
        default: break;
    }
    return 0; // in case of bogus parameter number
}
