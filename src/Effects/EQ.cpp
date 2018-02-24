/*
    EQ.cpp - EQ effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2018, Will Godfrey

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

    Modified February 2018
*/

#include "Misc/SynthEngine.h"
#include "Effects/EQ.h"

EQ::EQ(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0),
    synth(_synth)
{
    for (int i = 0; i < MAX_EQ_BANDS; ++i)
    {
        filter[i].Ptype = 0;
        filter[i].Pfreq = 64;
        filter[i].Pgain = 64;
        filter[i].Pq = 64;
        filter[i].Pstages = 0;
        filter[i].l = new AnalogFilter(6, 1000.0, 1.0, 0, synth);
        filter[i].r = new AnalogFilter(6, 1000.0, 1.0, 0, synth);
    }
    // default values
    setvolume(50);
    Pband = 0;
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
    memcpy(efxoutl, smpsl, synth->sent_bufferbytes);
    memcpy(efxoutr, smpsr, synth->sent_bufferbytes);
    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
        efxoutl[i] *= volume.getValue();
        efxoutr[i] *= volume.getValue();
        volume.advanceValue();
    }
    for (int i = 0; i < MAX_EQ_BANDS; ++i)
    {
        if (filter[i].Ptype == 0)
            continue;

        float oldval = filter[i].freq.getValue();
        filter[i].freq.advanceValue(synth->sent_buffersize);
        float newval = filter[i].freq.getValue();
        if (oldval != newval) {
            filter[i].l->interpolatenextbuffer();
            filter[i].l->setfreq(newval);
            filter[i].r->interpolatenextbuffer();
            filter[i].r->setfreq(newval);
        }

        oldval = filter[i].gain.getValue();
        filter[i].gain.advanceValue(synth->sent_buffersize);
        newval = filter[i].gain.getValue();
        if (oldval != newval) {
            filter[i].l->interpolatenextbuffer();
            filter[i].l->setgain(newval);
            filter[i].r->interpolatenextbuffer();
            filter[i].r->setgain(newval);
        }

        oldval = filter[i].q.getValue();
        filter[i].q.advanceValue(synth->sent_buffersize);
        newval = filter[i].q.getValue();
        if (oldval != newval) {
            filter[i].l->interpolatenextbuffer();
            filter[i].l->setq(newval);
            filter[i].r->interpolatenextbuffer();
            filter[i].r->setq(newval);
        }

        filter[i].l->filterout(efxoutl);
        filter[i].r->filterout(efxoutr);
    }
}


// Parameter control

void EQ::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    float tmp = powf(0.005f, (1.0f - Pvolume / 127.0f)) * 10.0f;
    outvolume.setTargetValue(tmp);
    volume.setTargetValue((!insertion) ? 1.0f : tmp);
}


void EQ::setpreset(unsigned char npreset)
{
    const int PRESET_SIZE = 1;
    const int NUM_PRESETS = 2;
    unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        { 67 }, // EQ 1
        { 67 }  // EQ 2
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
        case 1:
            Pband = value;
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
            tmp = 600.0f * powf(30.0f, (value - 64.0f) / 64.0f);
            filter[nb].freq.setTargetValue(tmp);
            break;

        case 2:
            filter[nb].Pgain = value;
            tmp = 30.0f * (value - 64.0f) / 64.0f;
            filter[nb].gain.setTargetValue(tmp);
            break;

        case 3:
            filter[nb].Pq = value;
            tmp = powf(30.0f, (value - 64.0f) / 64.0f);
            filter[nb].q.setTargetValue(tmp);
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
        case 1:
            return Pband;
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
    float resp = 1.0f;
    for (int i = 0; i < MAX_EQ_BANDS; ++i)
    {
        if (filter[i].Ptype == 0)
            continue;
        resp *= filter[i].l->H(freq);
    }
    // Only for UI purposes, use target value.
    return rap2dB(resp * outvolume.getTargetValue());
}


float EQlimit::getlimits(CommandBlock *getData)
{
    int value = getData->data.value;
    int control = getData->data.control;
    int request = getData->data.type & 3; // clear upper bits

    int min = 0;
    int max = 127;
    int def = 0;
    bool canLearn = true;
    bool isInteger = true;

    switch (control)
    {
        case 0:
            def = 64;
            break;
        case 1:
            max = 7;
            canLearn = false;
            break;
        case 10:
            max = 9;
            canLearn = false;
            break;
        case 11:
        case 12:
        case 13:
            def = 64;
            break;
        case 14:
            max = 4;
            canLearn = false;
            break;
        default:
            getData->data.type |= 4; // error
            return 1.0f;
            break;
    }

    switch(request)
    {
        case 0:
            if(value < min)
                value = min;
            else if(value > max)
                value = max;
            break;
        case 1:
            value = min;
            break;
        case 2:
            value = max;
            break;
        case 3:
            value = def;
            break;
    }
    getData->data.type |= (canLearn * 64 + isInteger * 128);
    return float(value);
}

