/*
    DynamicFilter.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
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

    This file is a derivative of the ZynAddSubFX original, modified January 2010
*/

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Effects/DynamicFilter.h"

DynamicFilter::DynamicFilter(bool insertion_, float *efxoutl_, float *efxoutr_) :
    Effect(insertion_, efxoutl_, efxoutr_, new FilterParams(0, 64, 64), 0),
    Pvolume(110),
    Ppanning(64),
    Pdepth(0),
    Pampsns(90),
    Pampsnsinv(0),
    Pampsmooth(60),
    filterl(NULL),
    filterr(NULL)
{
    setpreset(Ppreset);
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
    lfol *= depth * 5.0;
    lfor *= depth * 5.0;
    float freq = filterpars->getfreq();
    float q = filterpars->getq();

    int buffersize = zynMaster->getBuffersize();
    for (int i = 0; i < buffersize; ++i)
    {
        memcpy(efxoutl, smpsl, buffersize * sizeof(float));
        memcpy(efxoutr, smpsr, buffersize * sizeof(float));
        float x = (fabsf(smpsl[i]) + fabsf(smpsr[i])) * 0.5;
        ms1 = ms1 * (1.0 - ampsmooth) + x * ampsmooth + 1e-10;
    }

    float ampsmooth2 = powf(ampsmooth, 0.2) * 0.3;
    ms2 = ms2 * (1.0 - ampsmooth2) + ms1 * ampsmooth2;
    ms3 = ms3 * (1.0 - ampsmooth2) + ms2 * ampsmooth2;
    ms4=ms4 * (1.0 - ampsmooth2) + ms3 * ampsmooth2;
    float rms = (sqrtf(ms4)) * ampsns;

    float frl = filterl->getrealfreq(freq + lfol + rms);
    float frr = filterr->getrealfreq(freq + lfor + rms);

    filterl->setfreq_and_q(frl, q);
    filterr->setfreq_and_q(frr, q);

    filterl->filterout(efxoutl);
    filterr->filterout(efxoutr);

    // panning
    for (int i = 0; i < buffersize; ++i)
    {
        efxoutl[i] *= (1.0 - panning);
        efxoutr[i] *= panning;
    }
}

// Cleanup the effect
void DynamicFilter::cleanup(void)
{
    reinitfilter();
    ms1 = ms2 = ms3 = ms4 = 0.0;
}


// Parameter control
void DynamicFilter::setdepth(unsigned char Pdepth_)
{
    Pdepth = Pdepth_;
    depth = powf(Pdepth / 127.0, 2.0f);
}


void DynamicFilter::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    outvolume = Pvolume / 127.0;
    if (!insertion)
        volume = 1.0;
    else
        volume = outvolume;
}

void DynamicFilter::setpanning(unsigned char Ppanning_)
{
    Ppanning = Ppanning_;
    panning = Ppanning / 127.0;
}


void DynamicFilter::setampsns(unsigned char Pampsns_)
{
    Pampsns = Pampsns_;
    ampsns = powf(Pampsns / 127.0, 2.5f) * 10.0;
    if (Pampsnsinv)
        ampsns = -ampsns;
    ampsmooth = expf(-Pampsmooth / 127.0 * 10.0) * 0.99;
}

void DynamicFilter::reinitfilter(void)
{
    if (filterl != NULL)
        delete filterl;
    if (filterr != NULL)
        delete filterr;
    filterl = new Filter(filterpars);
    filterr = new Filter(filterpars);
}

void DynamicFilter::setpreset(unsigned char npreset)
{
    const int PRESET_SIZE = 10;
    const int NUM_PRESETS = 5;
    unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
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

//	    for (int i=0;i<5;i++){
//		printf("freq=%d  amp=%d  q=%d\n",filterpars->Pvowels[0].formants[i].freq,filterpars->Pvowels[0].formants[i].amp,filterpars->Pvowels[0].formants[i].q);
//	    };
    if (insertion == 0)
        changepar(0, presets[npreset][0] / 2); // lower the volume if this is system effect
    Ppreset = npreset;
    reinitfilter();
}


void DynamicFilter::changepar(int npar, unsigned char value)
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
}

unsigned char DynamicFilter::getpar(int npar)
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
        case 7:  return Pampsns;
        case 8:  return Pampsnsinv;
        case 9:  return Pampsmooth;
        default: break;
    }
    return 0;
}
