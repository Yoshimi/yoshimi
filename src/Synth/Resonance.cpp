/*
    Resonance.cpp - Resonance

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2018-2019 Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original

*/

#include <sys/types.h>
#include <cmath>
#include <iostream>

using namespace std;

#include "Misc/SynthEngine.h"
#include "Synth/Resonance.h"

Resonance::Resonance(SynthEngine *_synth) : Presets(_synth)
{
    setpresettype("Presonance");
    defaults();
}


void Resonance::defaults(void)
{
    Penabled = 0;
    PmaxdB = 20;
    Pcenterfreq = 64; // 1 kHz
    Poctavesfreq = 64;
    Pprotectthefundamental = 0;
    ctlcenter = 1.0;
    ctlbw = 1.0;
    for (int i = 0; i < MAX_RESONANCE_POINTS; ++i)
        Prespoints[i] = 64;
}


// Set a point of resonance function with a value
void Resonance::setpoint(int n, unsigned char p)
{
    if (n < 0 || n >= MAX_RESONANCE_POINTS)
        return;
    Prespoints[n] = p;
}


// Apply the resonance to FFT data
void Resonance::applyres(int n, FFTFREQS fftdata, float freq)
{
    if (Penabled == 0)
        return; // if the resonance is disabled
    float sum = 0.0;
    float l1 = logf(getfreqx(0.0) * ctlcenter);
    float l2 = logf(2.0f) * getoctavesfreq() * ctlbw;

    for (int i = 0; i < MAX_RESONANCE_POINTS; ++i)
        if (sum < Prespoints[i])
            sum = Prespoints[i];
    if (sum < 1.0)
        sum = 1.0;

    for (int i = 1; i < n; ++i)
    {
        // compute where the n-th hamonics fits to the graph
        float x = (logf(freq * i) - l1) / l2;
        if (x < 0.0)
            x = 0.0;

        x *= MAX_RESONANCE_POINTS;
        float dx = x - floorf(x);
        x = floorf(x);
        int kx1 = (int)x;
        if (kx1 >= MAX_RESONANCE_POINTS)
            kx1 = MAX_RESONANCE_POINTS -1;
        int kx2 = kx1 + 1;
        if (kx2 >= MAX_RESONANCE_POINTS)
            kx2 = MAX_RESONANCE_POINTS - 1;
        float y = (Prespoints[kx1] * (1.0 - dx) + Prespoints[kx2] * dx) / 127.0 - sum / 127.0;
        y = powf(10.0f, y * PmaxdB / 20.0);
        if (Pprotectthefundamental != 0 && i == 1)
            y = 1.0;
        fftdata.c[i] *= y;
        fftdata.s[i] *= y;
    }
}


// Gets the response at the frequency "freq"
float Resonance::getfreqresponse(float freq)
{
    float l1 = logf(getfreqx(0.0) * ctlcenter);
    float l2 = logf(2.0f) * getoctavesfreq() * ctlbw, sum = 0.0;

    for (int i = 0; i < MAX_RESONANCE_POINTS; ++i)
        if (sum < Prespoints[i])
            sum = Prespoints[i];
    if (sum < 1.0)
        sum = 1.0;
    // compute where the n-th hamonics fits to the graph
    float x = (logf(freq) - l1) / l2;
    if (x < 0.0)
        x = 0.0;

    x *= MAX_RESONANCE_POINTS;
    float dx = x - floorf(x);
    x = floorf(x);
    int kx1 = (int)x;
    if (kx1 >= MAX_RESONANCE_POINTS)
        kx1 = MAX_RESONANCE_POINTS - 1;
    int kx2 = kx1 + 1;
    if (kx2 >= MAX_RESONANCE_POINTS)
        kx2 = MAX_RESONANCE_POINTS - 1;
    float result = (Prespoints[kx1] * (1.0 - dx) + Prespoints[kx2] * dx)
                         / 127.0 - sum / 127.0;
    result = powf(10.0f, result * PmaxdB / 20.0);
    return result;
}


// Smooth the resonance function
void Resonance::smooth()
{
    float old = Prespoints[0];
    for (int i = 0; i < MAX_RESONANCE_POINTS; ++i)
    {
        old = old * 0.4 + Prespoints[i] * 0.6;
        Prespoints[i] = (int) old;
    }
    old = Prespoints[MAX_RESONANCE_POINTS - 1];
    for (int i = MAX_RESONANCE_POINTS - 1; i > 0; i--)
    {
        old = old * 0.4 + Prespoints[i] * 0.6;
        Prespoints[i] = (int) old + 1;
        if (Prespoints[i] > 127)
            Prespoints[i] = 127;
    }
}


// Randomize the resonance function
void Resonance::randomize(int type)
{
    uint32_t r = synth->randomINT() >> 24;
    for (int i = 0; i < MAX_RESONANCE_POINTS; ++i)
    {
        Prespoints[i] = r;
        if (type == 0 && synth->numRandom() < 0.1f)   // draw new random only for 10% of all slots
            r = synth->randomINT() >> 24;
        if (type == 1 && synth->numRandom() < 0.3f)   // ...only for 30% of all slots
            r = synth->randomINT() >> 24;
        if (type == 2)
            r = synth->randomINT() >> 24;
    }
    smooth();
}


// Interpolate the peaks
void Resonance::interpolatepeaks(int type)
{
    int x1 = 0, y1 = Prespoints[0];
    for (int i = 1; i < MAX_RESONANCE_POINTS; ++i)
    {
        if (Prespoints[i] != 64 || (i + 1) == MAX_RESONANCE_POINTS)
        {
            int y2 = Prespoints[i];
            for (int k = 0; k < i - x1; ++k)
            {
                float x = (float) k / (i - x1);
                if (type == 0)
                    x = (1 - cosf(x * PI)) * 0.5;
                Prespoints[x1 + k] = (int)(y1 * (1.0 - x) + y2 * x);
            }
            x1 = i;
            y1 = y2;
        }
    }
}


// Get the frequency from x, where x is [0..1]; x is the x coordinate
float Resonance::getfreqx(float x)
{
    if (x > 1.0)
        x = 1.0;
    float octf = powf(2.0f, getoctavesfreq());
    return (getcenterfreq() / sqrtf(octf) * powf(octf, x));
}


// Get the x coordinate from frequency (used by the UI)
float Resonance::getfreqpos(float freq)
{
    return (logf(freq) - logf(getfreqx(0.0))) / logf(2.0f) / getoctavesfreq();
}


// Get the center frequency of the resonance graph
float Resonance::getcenterfreq(void)
{
    return 10000.0 * powf(10.0f, -(1.0f - Pcenterfreq / 127.0f) * 2.0f);
}


// Get the number of octave that the resonance functions applies to
float Resonance::getoctavesfreq(void)
{
    return 0.25 + 10.0 * Poctavesfreq / 127.0;
}


void Resonance::sendcontroller(unsigned short int ctl, float par)
{
    if (ctl == MIDI::CC::resonanceCenter)
        ctlcenter = par;
    else
        ctlbw = par;
}


void Resonance::add2XML(XMLwrapper *xml)
{
    xml->addparbool("enabled",Penabled);

    if ((Penabled==0)&&(xml->minimal)) return;

    xml->addpar("max_db",PmaxdB);
    xml->addpar("center_freq",Pcenterfreq);
    xml->addpar("octaves_freq",Poctavesfreq);
    xml->addparbool("protect_fundamental_frequency",Pprotectthefundamental);
    xml->addpar("resonance_points",MAX_RESONANCE_POINTS);
    for (int i=0;i<MAX_RESONANCE_POINTS;i++)
    {
        xml->beginbranch("RESPOINT",i);
        xml->addpar("val",Prespoints[i]);
        xml->endbranch();
    }
}


void Resonance::getfromXML(XMLwrapper *xml)
{
    Penabled=xml->getparbool("enabled",Penabled);

    PmaxdB=xml->getpar127("max_db",PmaxdB);
    Pcenterfreq=xml->getpar127("center_freq",Pcenterfreq);
    Poctavesfreq=xml->getpar127("octaves_freq",Poctavesfreq);
    Pprotectthefundamental=xml->getparbool("protect_fundamental_frequency",Pprotectthefundamental);
    for (int i=0;i<MAX_RESONANCE_POINTS;i++)
    {
        if (xml->enterbranch("RESPOINT",i)==0) continue;
        Prespoints[i]=xml->getpar127("val",Prespoints[i]);
        xml->exitbranch();
    }
}


float ResonanceLimits::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int insert = getData->data.insert;

    unsigned char type = 0;

    // resonance defaults
    int min = 0;
    int max = 1;
    int def = 0;
    type |= TOPLEVEL::type::Integer;

    if (insert == TOPLEVEL::insert::resonanceGraphInsert)
    {
        min = 1;
        max = 127;
        def = 64;

        getData->data.type = type;

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
        return value;
    }

    switch (control)
    {
        case RESONANCE::control::maxDb:
            min = 1;
            max = 90;
            def = 20;
            break;
        case RESONANCE::control::centerFrequency:
            max = 127;
            def = 64;
            break;
        case RESONANCE::control::octaves:
            max = 127;
            def = 64;
            break;
        case RESONANCE::control::enableResonance:
            break;
        case RESONANCE::control::randomType:
            max = 2;
            break;
        case RESONANCE::control::interpolatePeaks:
            break;
        case RESONANCE::control::protectFundamental:
            break;
        case RESONANCE::control::clearGraph:
            max = 0;
            break;
        case RESONANCE::control::smoothGraph:
            max = 0;
            break;
        default:
            type |= TOPLEVEL::type::Error;
            break;
    }
    getData->data.type = type;
    if (type & TOPLEVEL::type::Error)
        return 1;

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
    return value;
}
