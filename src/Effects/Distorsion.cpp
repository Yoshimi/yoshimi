/*
    Distorsion.cpp - Distorsion effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

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

    This file is a derivative of a ZynAddSubFX original, modified October 2010
*/

#include "Misc/SynthEngine.h"
#include "Effects/Distorsion.h"

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
    float l, r, lout, rout;

    float inputvol = powf(5.0f, (Pdrive - 32.0f) / 127.0f);
    if (Pnegate != 0)
        inputvol *= -1.0f;

    if (Pstereo != 0)
    {   //Stereo
        for (int i = 0; i < synth->buffersize; ++i)
        {
            efxoutl[i] = smpsl[i] * inputvol * (1.0f - panning);
            efxoutr[i] = smpsr[i] * inputvol * panning;
        }
    } else {
        for (int i = 0; i < synth->buffersize; ++i)
        {
            efxoutl[i] = (smpsl[i] * panning + smpsr[i] * (1.0f - panning)) * inputvol;
        }
    }

    if (Pprefiltering != 0)
        applyfilters(efxoutl, efxoutr);

    // no optimised, yet (no look table)
    waveShapeSmps(synth->buffersize, efxoutl, Ptype + 1, Pdrive);
    if (Pstereo != 0)
        waveShapeSmps(synth->buffersize, efxoutr, Ptype + 1, Pdrive);

    if (Pprefiltering == 0)
        applyfilters(efxoutl, efxoutr);

    if (Pstereo == 0)
        memcpy(efxoutr, efxoutl, synth->bufferbytes);
        //for (i = 0; i < synth->buffersize; ++i)
        //    efxoutr[i] = efxoutl[i];

    float level = dB2rap(60.0f * Plevel / 127.0f - 40.0f);
    for (int i = 0; i < synth->buffersize; ++i)
    {
        lout = efxoutl[i];
        rout = efxoutr[i];
        l = lout * (1.0f - lrcross) + rout * lrcross;
        r = rout * (1.0f - lrcross) + lout * lrcross;
        lout = l;
        rout = r;
        efxoutl[i] = lout * 2.0f * level;
        efxoutr[i] = rout * 2.0f *level;
    }
}


// Parameter control
void Distorsion::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    if(insertion == 0)
    {
        outvolume = pow(0.01f, (1.0f - Pvolume / 127.0f)) * 4.0f;
        volume = 1.0f;
    }
    else
         volume = outvolume = Pvolume / 127.0f;
    if (Pvolume == 0.0f)
        cleanup();
}


void Distorsion::setpanning(unsigned char Ppanning_)
{
    Ppanning = Ppanning_;
    panning = (Ppanning + 0.5f) / 127.0f;
}


void Distorsion::setlrcross(unsigned char Plrcross_)
{
    Plrcross = Plrcross_;
    lrcross = Plrcross / 127.0f;
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
