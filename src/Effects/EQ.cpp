/*
    EQ.cpp - EQ effect

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
#include "Effects/EQ.h"

EQ::EQ(bool insertion_, float *efxoutl_, float *efxoutr_) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0)
{
    for (int i = 0; i < MAX_EQ_BANDS; ++i)
    {
        filter[i].Ptype = 0;
        filter[i].Pfreq = 64;
        filter[i].Pgain = 64;
        filter[i].Pq = 64;
        filter[i].Pstages = 0;
        filter[i].l = new AnalogFilter(6, 1000.0, 1.0, 0);
        filter[i].r = new AnalogFilter(6, 1000.0, 1.0, 0);
    }
    // default values
    Pvolume = 50;
    setpreset(Ppreset);
    cleanup();
}


// Cleanup the effect
void EQ::cleanup(void)
{
    for (int i = 0; i < MAX_EQ_BANDS; ++i)
    {
        filter[i].l->cleanup();
        filter[i].r->cleanup();
    }
}

// Effect output
void EQ::out(float *smpsl, float *smpsr)
{
    int buffersize = zynMaster->getBuffersize();
    memcpy(efxoutl, smpsl, buffersize * sizeof(float));
    memcpy(efxoutr, smpsr, buffersize * sizeof(float));
    for (int i = 0; i < buffersize; ++i)
    {
        efxoutl[i] *= volume;
        efxoutr[i] *= volume;
        //efxoutl[i] = smpsl[i] * volume;
        //efxoutr[i] = smpsr[i] * volume;
    }
    for (int i = 0; i < MAX_EQ_BANDS; ++i)
    {
        if (filter[i].Ptype == 0)
            continue;
        filter[i].l->filterout(efxoutl);
        filter[i].r->filterout(efxoutr);
    }
}


// Parameter control

void EQ::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    outvolume = powf(0.005f, (1.0 - Pvolume / 127.0)) * 10.0;
    volume = (!insertion) ? 1.0 : outvolume;
}


void EQ::setpreset(unsigned char npreset)
{
    const int PRESET_SIZE = 1;
    const int NUM_PRESETS = 2;
    unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        // EQ 1
        { 67 },
        // EQ 2
        { 67 }
    };

    if (npreset >= NUM_PRESETS)
        npreset = NUM_PRESETS - 1;
    for (int n = 0; n < PRESET_SIZE; ++n)
        changepar(n, presets[npreset][n]);
    Ppreset = npreset;
}


void EQ::changepar(int npar, unsigned char value)
{
    switch (npar)
    {
        case 0:
            setvolume(value);
            break;
    }
    if (npar < 10)
        return;

    int nb = (npar - 10) / 5; // number of the band (filter)
    if (nb >= MAX_EQ_BANDS)
        return;
    int bp = npar % 5; // band paramenter

    float tmp;
    switch (bp)
    {
        case 0:
            filter[nb].Ptype = value;
            if (value > 9)
                filter[nb].Ptype = 0; // has to be changed if more filters will be added
            if (filter[nb].Ptype != 0)
            {
                filter[nb].l->settype(value - 1);
                filter[nb].r->settype(value - 1);
            }
            break;
        case 1:
            filter[nb].Pfreq = value;
            tmp = 600.0 * powf(30.0f, (value - 64.0) / 64.0);
            filter[nb].l->setfreq(tmp);
            filter[nb].r->setfreq(tmp);
            break;
        case 2:
            filter[nb].Pgain = value;
            tmp = 30.0 * (value - 64.0) / 64.0;
            filter[nb].l->setgain(tmp);
            filter[nb].r->setgain(tmp);
            break;
        case 3:
            filter[nb].Pq = value;
            tmp = powf(30.0f, (value - 64.0) / 64.0);
            filter[nb].l->setq(tmp);
            filter[nb].r->setq(tmp);
            break;
        case 4:
            filter[nb].Pstages = value;
            if (value >= MAX_FILTER_STAGES)
                filter[nb].Pstages = MAX_FILTER_STAGES - 1;
            filter[nb].l->setstages(value);
            filter[nb].r->setstages(value);
            break;
    }
}

unsigned char EQ::getpar(int npar)
{
    switch (npar)
    {
        case 0:
            return Pvolume;
            break;
    }
    if (npar < 10)
        return 0;

    int nb = (npar - 10) / 5; // number of the band (filter)
    if (nb >= MAX_EQ_BANDS)
        return 0;
    int bp = npar % 5; // band paramenter
    switch (bp)
    {
        case 0:
            return(filter[nb].Ptype);
            break;
        case 1:
            return(filter[nb].Pfreq);
            break;
        case 2:
            return(filter[nb].Pgain);
            break;
        case 3:
            return(filter[nb].Pq);
            break;
        case 4:
            return(filter[nb].Pstages);
            break;
    }
    return 0; // in case of bogus parameter number
}


float EQ::getfreqresponse(float freq)
{
    float resp = 1.0;
    for (int i = 0; i < MAX_EQ_BANDS; ++i)
    {
        if (filter[i].Ptype == 0)
            continue;
        resp *= filter[i].l->H(freq);
    }
    return rap2dB(resp * outvolume);
}
