/*
    DynamicFilter.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2021, Will Godfrey

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
#include "Effects/DynamicFilter.h"

static const int PRESET_SIZE = 10;
static const int NUM_PRESETS = 5;
static const char presets[NUM_PRESETS][PRESET_SIZE] = {
        // WahWah
        { 110, 64, 80, 0, 0, 64, 0, 90, 0, 60 },
        // AutoWah
        {110, 64, 70, 0, 0, 80, 70, 0, 0, 60 },
        // Sweep
        {100, 64, 30, 0, 0, 50, 80, 0, 0, 60 },
        // VocalMorph1
        { 110, 64, 80, 0, 0, 64, 0, 64, 0, 60 },
        // VocalMorph1
        {127, 64, 50, 0, 0, 96, 64, 0, 0, 60 }
};

DynamicFilter::DynamicFilter(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth) :
    Effect(insertion_, efxoutl_, efxoutr_, new FilterParams(0, 64, 64, 0, _synth), 0),
    lfo(_synth),
    Pdepth(0),
    Pampsns(90),
    Pampsnsinv(0),
    Pampsmooth(60),
    filterl(NULL),
    filterr(NULL),
    synth(_synth)
{
    setvolume(110);
    setpreset(Ppreset);
    changepar(1, 64); // pan
    Pchanged = false;
    cleanup();
}


DynamicFilter::~DynamicFilter()
{
    delete filterpars;
    delete filterl;
    delete filterr;
}


// Apply the effect
void DynamicFilter::out(float *smpsl, float *smpsr)
{
    if (filterpars->changed)
    {
        filterpars->changed = false;
        cleanup();
    }

    float lfol, lfor;
    lfo.effectlfoout(&lfol, &lfor);
    lfol *= depth * 5.0f;
    lfor *= depth * 5.0f;
    float freq = filterpars->getfreq();
    float q = filterpars->getq();

    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
        memcpy(efxoutl, smpsl, synth->sent_bufferbytes);
        memcpy(efxoutr, smpsr, synth->sent_bufferbytes);
        float x = (fabsf(smpsl[i]) + fabsf(smpsr[i])) * 0.5f;
        ms1 = ms1 * (1.0f - ampsmooth) + x * ampsmooth + 1e-10f;
    }

    float ampsmooth2 = powf(ampsmooth, 0.2f) * 0.3f;
    ms2 = ms2 * (1.0f - ampsmooth2) + ms1 * ampsmooth2;
    ms3 = ms3 * (1.0f - ampsmooth2) + ms2 * ampsmooth2;
    ms4=ms4 * (1.0f - ampsmooth2) + ms3 * ampsmooth2;
    float rms = (sqrtf(ms4)) * ampsns;

    float frl = filterl->getrealfreq(freq + lfol + rms);
    float frr = filterr->getrealfreq(freq + lfor + rms);

    filterl->setfreq_and_q(frl, q);
    filterr->setfreq_and_q(frr, q);

    filterl->filterout(efxoutl);
    filterr->filterout(efxoutr);

    // panning
    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
        efxoutl[i] *= pangainL.getAndAdvanceValue();
        efxoutr[i] *= pangainR.getAndAdvanceValue();
    }
}


// Cleanup the effect
void DynamicFilter::cleanup(void)
{
    reinitfilter();
    ms1 = ms2 = ms3 = ms4 = 0.0f;
}


// Parameter control
void DynamicFilter::setdepth(unsigned char Pdepth_)
{
    Pdepth = Pdepth_;
    depth = powf(Pdepth / 127.0f, 2.0f);
}


void DynamicFilter::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    float tmp = Pvolume / 127.0f;
    outvolume.setTargetValue(tmp);
    if (!insertion)
        volume.setTargetValue(1.0f);
    else
        volume.setTargetValue(tmp);
}


void DynamicFilter::setampsns(unsigned char Pampsns_)
{
    Pampsns = Pampsns_;
    ampsns = powf(Pampsns / 127.0f, 2.5f) * 10.0f;
    if (Pampsnsinv)
        ampsns = -ampsns;
    ampsmooth = expf(-Pampsmooth / 127.0f * 10.0f) * 0.99f;
}


void DynamicFilter::reinitfilter(void)
{
    if (filterl != NULL)
        delete filterl;
    if (filterr != NULL)
        delete filterr;
    filterl = new Filter(filterpars, synth);
    filterr = new Filter(filterpars, synth);
}


void DynamicFilter::setpreset(unsigned char npreset)
{


if (npreset < 0xf)
    {
        if (npreset >= NUM_PRESETS)
            npreset = NUM_PRESETS - 1;
        for (int n = 0; n < PRESET_SIZE; ++n)
            changepar(n, presets[npreset][n]);

        filterpars->defaults();

        switch (npreset)
        {
            case 0:
                filterpars->Pcategory = 0;
                filterpars->Ptype = 2;
                filterpars->Pfreq = 45;
                filterpars->Pq = 64;
                filterpars->Pstages = 1;
                filterpars->Pgain = 64;
                break;

            case 1:
                filterpars->Pcategory = 2;
                filterpars->Ptype = 0;
                filterpars->Pfreq = 72;
                filterpars->Pq = 64;
                filterpars->Pstages = 0;
                filterpars->Pgain = 64;
                break;

            case 2:
                filterpars->Pcategory = 0;
                filterpars->Ptype = 4;
                filterpars->Pfreq = 64;
                filterpars->Pq = 64;
                filterpars->Pstages = 2;
                filterpars->Pgain = 64;
                break;

            case 3:
                filterpars->Pcategory = 1;
                filterpars->Ptype = 0;
                filterpars->Pfreq = 50;
                filterpars->Pq = 70;
                filterpars->Pstages = 1;
                filterpars->Pgain = 64;

                filterpars->Psequencesize = 2;
                // "I"
                filterpars->Pvowels[0].formants[0].freq = 34;
                filterpars->Pvowels[0].formants[0].amp = 127;
                filterpars->Pvowels[0].formants[0].q = 64;
                filterpars->Pvowels[0].formants[1].freq = 99;
                filterpars->Pvowels[0].formants[1].amp = 122;
                filterpars->Pvowels[0].formants[1].q = 64;
                filterpars->Pvowels[0].formants[2].freq = 108;
                filterpars->Pvowels[0].formants[2].amp = 112;
                filterpars->Pvowels[0].formants[2].q = 64;
                // "A"
                filterpars->Pvowels[1].formants[0].freq = 61;
                filterpars->Pvowels[1].formants[0].amp = 127;
                filterpars->Pvowels[1].formants[0].q = 64;
                filterpars->Pvowels[1].formants[1].freq = 71;
                filterpars->Pvowels[1].formants[1].amp = 121;
                filterpars->Pvowels[1].formants[1].q = 64;
                filterpars->Pvowels[1].formants[2].freq = 99;
                filterpars->Pvowels[1].formants[2].amp = 117;
                filterpars->Pvowels[1].formants[2].q = 64;
                break;

            case 4:
                filterpars->Pcategory = 1;
                filterpars->Ptype = 0;
                filterpars->Pfreq = 64;
                filterpars->Pq = 70;
                filterpars->Pstages = 1;
                filterpars->Pgain = 64;

                filterpars->Psequencesize = 2;
                filterpars->Pnumformants = 2;
                filterpars->Pvowelclearness = 0;

                filterpars->Pvowels[0].formants[0].freq = 70;
                filterpars->Pvowels[0].formants[0].amp = 127;
                filterpars->Pvowels[0].formants[0].q = 64;
                filterpars->Pvowels[0].formants[1].freq = 80;
                filterpars->Pvowels[0].formants[1].amp = 122;
                filterpars->Pvowels[0].formants[1].q = 64;

                filterpars->Pvowels[1].formants[0].freq = 20;
                filterpars->Pvowels[1].formants[0].amp = 127;
                filterpars->Pvowels[1].formants[0].q = 64;
                filterpars->Pvowels[1].formants[1].freq = 100;
                filterpars->Pvowels[1].formants[1].amp = 121;
                filterpars->Pvowels[1].formants[1].q = 64;
                break;
        }

        if (insertion == 0)
            changepar(0, presets[npreset][0] * 0.5f); // lower the volume if this is
                                                  // system effect
        Ppreset = npreset;
        reinitfilter();
    }
    else
    {
        unsigned char preset = npreset & 0xf;
        unsigned char param = npreset >> 4;
        if (param == 0xf)
            param = 0;
        changepar(param, presets[preset][param]);
        if ((insertion == 0) && (param == 0))
            changepar(0, presets[preset][0] * 0.5f);
    }
    Pchanged = false;
}


void DynamicFilter::changepar(int npar, unsigned char value)
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
            setampsns(value);
            break;

        case 8:
            Pampsnsinv = value;
            setampsns(Pampsns);
            break;

        case 9:
            Pampsmooth = value;
            setampsns(Pampsns);
            break;
    }
    Pchanged = true;
}


unsigned char DynamicFilter::getpar(int npar)
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
        case 7:  return Pampsns;
        case 8:  return Pampsnsinv;
        case 9:  return Pampsmooth;
        default: break;
    }
    return 0;
}


float Dynamlimit::getlimits(CommandBlock *getData)
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
            if (npart == TOPLEVEL::section::systemEffects) // system effects
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
            max = 1;
            canLearn = 0;
            break;
        case 9:
            break;
        case 16:
            max = 4;
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

