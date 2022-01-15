/*
    Reverb.cpp - Reverberation effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018-2021, Will Godfrey

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
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

#include <cmath>

#include "DSP/Unison.h"
#include "DSP/AnalogFilter.h"
#include "Misc/SynthEngine.h"
#include "Misc/SynthHelper.h"
#include "Effects/Reverb.h"

using func::power;
using func::powFrac;

static const int PRESET_SIZE = 13;
static const int NUM_PRESETS = 13;
static unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        // Cathedral1
        {80,  64,  63,  24,  0,  0,  0, 85,  5,  83,   1,  64,  20 },
        // Cathedral2
        {80,  64,  69,  35,  0,  0,  0, 127, 0,  71,   0,  64,  20 },
        // Cathedral3
        {80,  64,  69,  24,  0,  0,  0, 127, 75, 78,   1,  85,  20 },
        // Hall1
        {90,  64,  51,  10,  0,  0,  0, 127, 21, 78,   1,  64,  20 },
        // Hall2
        {90,  64,  53,  20,  0,  0,  0, 127, 75, 71,   1,  64,  20 },
        // Room1
        {100, 64,  33,  0,   0,  0,  0, 127, 0,  106,  0,  30,  20 },
        // Room2
        {100, 64,  21,  26,  0,  0,  0, 62,  0,  77,   1,  45,  20 },
        // Basement
        {110, 64,  14,  0,   0,  0,  0, 127, 5,  71,   0,  25,  20 },
        // Tunnel
        {85,  80,  84,  20,  42, 0,  0, 51,  0,  78,   1,  105, 20 },
        // Echoed1
        {95,  64,  26,  60,  71, 0,  0, 114, 0,  64,   1,  64,  20 },
        // Echoed2
        {90,  64,  40,  88,  71, 0,  0, 114, 0,  88,   1,  64,  20 },
        // VeryLong1
        {90,  64,  93,  15,  0,  0,  0, 114, 0,  77,   0,  95,  20 },
        // VeryLong2
        {90,  64,  111, 30,  0,  0,  0, 114, 90, 74,   1,  80,  20 }
};

// todo: EarlyReflections, Prdelay, Perbalance

Reverb::Reverb(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0, _synth),
    // defaults
//    Pvolume(48),
    Ptime(64),
    Pidelay(40),
    Pidelayfb(0),
//    Prdelay(0), // **** RHL ****
//    Perbalance(64), // **** RHL ****
    Plpf(127),
    Phpf(0),
    Plohidamp(80),
    Ptype(1),
    Proomsize(64),
    Pbandwidth(20),
    roomsize(1.0f),
    rs(1.0f),
    bandwidth(NULL),
    idelay(NULL),
    lpf(NULL),
    hpf(NULL), // no filter
    lpffr(0, synth->samplerate),
    hpffr(0, synth->samplerate),
    inputbuf(_synth->buffersize)
{
    setvolume(48);
    for (int i = 0; i < REV_COMBS * 2; ++i)
    {

        comblen[i] = 800 + synth->randomINT() / (INT32_MAX/1400);
        combk[i] = 0;
        lpcomb[i] = 0;
        combfb[i] = -0.97f;
        comb[i] = NULL;
    }

    for (int i = 0; i < REV_APS * 2; ++i)
    {
        aplen[i] = 500 + synth->randomINT() / (INT32_MAX/500);
        apk[i] = 0;
        ap[i] = NULL;
    }
    setpreset(Ppreset);
    Pchanged = false;
    cleanup(); // do not call this before the comb initialisation
}


Reverb::~Reverb()
{
    int i;
    if (idelay)
        delete [] idelay;
    if (hpf)
        delete hpf;
    if (lpf)
        delete lpf;
    for (i = 0; i < REV_APS * 2; ++i)
        delete [] ap[i];
    for (i = 0; i < REV_COMBS * 2; ++i)
        delete [] comb[i];

    if (bandwidth)
        delete bandwidth;
}


void Reverb::clearBuffers()
{
    for (size_t j = 0; j < REV_COMBS * 2; ++j)
    {
        combk[j] = 0;
        lpcomb[j] = 0.0;
        for (size_t smp = 0; smp < comblen[j]; ++smp)
            comb[j][smp] = 0.0f;
    }
    for (size_t j = 0; j < REV_APS * 2; ++j)
    {
        apk[j] = 0;
        for (size_t smp = 0; smp < aplen[j]; ++smp)
            ap[j][smp] = 0.0f;
    }

    if (idelay)
        memset(idelay, 0, sizeof(float) * idelaylen);
}


// Process one channel; 0 = left, 1 = right
void Reverb::calculateReverb(size_t ch, Samples& inputFeed, float *output)
{
    ////TODO: implement the high part from lohidamp    (comment probably from original author, before 2010)

    for (size_t j = REV_COMBS * ch; j < REV_COMBS * (ch + 1); ++j)
    {
        size_t offset = combk[j];
        size_t combLen = comblen[j];
        float lowpassj = lpcomb[j];
        for (size_t smp = 0; smp < size_t(synth->sent_buffersize); ++smp)
        {
            float feedback = comb[j][offset] * combfb[j];
            feedback = feedback * (1.0f - lohifb) + lowpassj * lohifb;
            lowpassj = feedback;

            comb[j][offset] = inputFeed[smp] + feedback;
            output[smp] += feedback;

            if ((++offset) >= combLen)
                offset = 0;
        }

        combk[j] = offset;
        lpcomb[j] = lowpassj;
    }

    // feed result of comb filters into AllPass filters
    for (size_t j = REV_APS * ch; j < REV_APS * (1 + ch); ++j)
    {
        size_t offset = apk[j];
        size_t allpassLen = aplen[j];
        for (size_t smp = 0; smp < size_t(synth->sent_buffersize); ++smp)
        {
            float feedback = ap[j][offset];
            ap[j][offset] = 0.7f * feedback + output[smp];
            output[smp] = feedback - 0.7f * ap[j][offset] + 1e-20f; // anti-denormal - a very, very, very small dc bias
            if ((++offset) >= allpassLen)
                offset = 0;
        }
        apk[j] = offset;
    }
}


namespace { //Helper: detect change above rounding errors for frequency interpolation

    const float FREQUENCY_EPSILON = 1e-3;

    bool significantChange(float newVal, float oldVal)
    {
        return std::fabs(newVal - oldVal) > FREQUENCY_EPSILON;
    }
}


void Reverb::preprocessInput(float *rawL, float *rawR, Samples& inputFeed)
{
    for (size_t i = 0; i < size_t(synth->sent_buffersize); ++i)
    {
        inputFeed[i] = float(1e-20) + ((rawL[i] + rawR[i]) / 2.0f); // includes anti-denormal

        if (idelay)
        {// shift input by pre-delay
            float tmp = inputFeed[i] + idelay[idelayk] * idelayfb;
            inputFeed[i] = idelay[idelayk];
            idelay[idelayk] = tmp;
            idelayk++;
            if (idelayk >= idelaylen)
                idelayk = 0;
        }
    }

    if (bandwidth)
        bandwidth->process(synth->sent_buffersize, inputFeed.get());

    if (lpf)
    {
        float currFreq = lpf->getFreq();
        lpffr.advanceValue(synth->sent_buffersize);
        if (significantChange(currFreq, lpffr.getValue()))
        {
            lpf->interpolatenextbuffer();
            lpf->setfreq(lpffr.getValue());
        }
        lpf->filterout(inputFeed.get());
    }
     if (hpf)
    {
        float currFreq = hpf->getFreq();
        hpffr.advanceValue(synth->sent_buffersize);
        if (significantChange(currFreq, hpffr.getValue()))
        {
            hpf->interpolatenextbuffer();
            hpf->setfreq(hpffr.getValue());
        }
         hpf->filterout(inputFeed.get());
    }
}



// Effect output
void Reverb::out(float *rawL, float *rawR)
{
    outvolume.advanceValue(synth->sent_buffersize);

    if (!Pvolume && insertion)
        return;

    preprocessInput(rawL,rawR, inputbuf);

    calculateReverb(0, inputbuf, efxoutl); // inputbuf -> left
    calculateReverb(1, inputbuf, efxoutr); // inputbuf -> right

    float lvol = rs / REV_COMBS * pangainL.getAndAdvanceValue();
    float rvol = rs / REV_COMBS * pangainR.getAndAdvanceValue();
    if (insertion != 0)
    {
        lvol *= 2.0f;
        rvol *= 2.0f;
    }
    for (size_t i = 0; i < size_t(synth->sent_buffersize); ++i)
    {
        efxoutl[i] *= lvol;
        efxoutr[i] *= rvol;
    }
}


// Reset the effect to pristine state
void Reverb::cleanup()
{
    Effect::cleanup();
    lpffr.pushToTarget();
    hpffr.pushToTarget();
    setupPipelines();
    settime(Ptime);
    clearBuffers();
    if (hpf)
        hpf->cleanup();
    if (lpf)
        lpf->cleanup();
}


// Parameter control
void Reverb::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    if (!insertion)
    {
        outvolume.setTargetValue(4.0f * powFrac<100>(1.0f - Pvolume / 127.0f));
        volume.setTargetValue(1.0f);
    }
    else
    {
        float tmp = Pvolume / 127.0f;
        volume.setTargetValue(tmp);
        outvolume.setTargetValue(tmp);
        if (Pvolume == 0.0f)
            cleanup();
    }
}


void Reverb::settime(unsigned char Ptime_)
{
    Ptime = Ptime_;
    float t = power<60>(Ptime / 127.0f) - 0.97f;
    for (int i = 0; i < REV_COMBS * 2; ++i)
        combfb[i] = -expf(float(comblen[i]) / synth->samplerate_f * logf(0.001f) / t);
        // the feedback is negative because it removes the DC
}


void Reverb::setlohidamp(unsigned char Plohidamp_)
{
    Plohidamp = (Plohidamp_ < 64) ? 64 : Plohidamp_;
                       // remove this when the high part from lohidamp is added
    if (Plohidamp == 64)
    {
        lohidamptype = 0;
        lohifb = 0.0f;
    }
    else
    {
        if (Plohidamp < 64)
            lohidamptype = 1;
        if (Plohidamp > 64)
            lohidamptype = 2;
        float x = fabsf((float)(Plohidamp - 64) / 64.1f);
        lohifb = x * x;
    }
}


void Reverb::setidelay(unsigned char Pidelay_)
{
    Pidelay = Pidelay_;
    float delay = powf(50.0f * Pidelay / 127.0f, 2.0f) - 1.0f;

    if (idelay)
        delete [] idelay;
    idelay = NULL;

    idelaylen = lrint(synth->samplerate_f * delay / 1000.0f);
    if (idelaylen > 1)
    {
        idelayk = 0;
        idelay = new float[idelaylen];
        memset(idelay, 0, idelaylen * sizeof(float));
    }
}


void Reverb::setidelayfb(unsigned char Pidelayfb_)
{
    Pidelayfb = Pidelayfb_;
    idelayfb = Pidelayfb / 128.0f;
}


void Reverb::sethpf(unsigned char Phpf_)
{
    Phpf = Phpf_;
    if (Phpf == 0)
    {   // No HighPass
        if (hpf)
            delete hpf;
        hpf = NULL;
    } else {
        hpffr.setTargetValue(expf(powf(Phpf / 127.0f, 0.5f) * logf(10000.0f)) + 20.0f);
        if (hpf == NULL)
            hpf = new AnalogFilter(3, hpffr.getValue(), 1, 0, synth);
    }
}


void Reverb::setlpf(unsigned char Plpf_)
{
    Plpf = Plpf_;
    if (Plpf == 127)
    {   // No LowPass
        if (lpf)
            delete lpf;
        lpf = NULL;
    } else {
        lpffr.setTargetValue(expf(powf(Plpf / 127.0f, 0.5f) * logf(25000.0f)) + 40.0f);
        if (!lpf)
            lpf = new AnalogFilter(2, lpffr.getValue(), 1, 0, synth);
    }
}


void Reverb::settype(unsigned char Ptype_)
{
    Ptype = Ptype_;
    if (Ptype >= NUM_TYPES)
        Ptype = NUM_TYPES - 1;

    cleanup(); // invokes setupPipelines()
}


void Reverb::setupPipelines()
{
    int combtunings[NUM_TYPES][REV_COMBS] = {
        { 0, 0, 0, 0, 0, 0, 0, 0 }, // this is unused (for random)

        // Freeverb by Jezar at Dreampoint
        { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 },
        { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }
    };

    int aptunings[NUM_TYPES][REV_APS] = {
        { 0, 0, 0, 0 },         // this is unused (for random)
        { 225, 341, 441, 556 }, // Freeverb by Jezar at Dreampoint
        { 225, 341, 441, 556 }
    };

    float samplerate_adjust = synth->samplerate_f / 44100.0f;

    // adjust the combs according to samplerate and room size
    for (int i = 0; i < REV_COMBS * 2; ++i)
    {
        float tmp;
        if (Ptype == 0)
            tmp = 800.0f + synth->numRandom() * 1400.0f;
        else
            tmp = combtunings[Ptype][i % REV_COMBS];
        tmp *= roomsize;
        if (i > REV_COMBS)
            tmp += 23.0f;
        tmp *= samplerate_adjust; // adjust the combs according to the samplerate
        comblen[i] = size_t(tmp);
        if (comblen[i] < 10)
            comblen[i] = 10;
        combk[i] = 0;
        lpcomb[i] = 0;
        if (comb[i])
            delete [] comb[i];
        comb[i] = new float[comblen[i]];
        memset(comb[i], 0, comblen[i] * sizeof(float));
    }

    for (int i = 0; i < REV_APS * 2; ++i)
    {
        float tmp;
        if (Ptype == 0)
        {
            tmp = 500.0f + synth->numRandom() * 500.0f;
        }
        else
            tmp = aptunings[Ptype][i % REV_APS];
        tmp *= roomsize;
        if (i > REV_APS)
            tmp += 23.0f;
        tmp *= samplerate_adjust; // adjust the combs according to the samplerate
        aplen[i] = size_t(tmp);
        if (aplen[i] < 10)
            aplen[i] = 10;
        apk[i] = 0;
        if (ap[i])
            delete [] ap[i];
        ap[i] = new float[aplen[i]];
        memset(ap[i], 0, aplen[i] * sizeof(float));
    }
    if (NULL != bandwidth)
        delete bandwidth;
    bandwidth = NULL;
    if (Ptype == 2)
    { // bandwidth
        bandwidth = new Unison(synth->buffersize / 4 + 1, 2.0f, synth);
        bandwidth->setSize(50);
        bandwidth->setBaseFrequency(1.0f);
        //TODO the size of the unison buffer may be too small, though this has
        //not been verified yet.
        //As this cannot be resized in a RT context, a good upper bound should
        //be found
    }
}


void Reverb::setroomsize(unsigned char Proomsize_)
{
    Proomsize = Proomsize_;
    if (!Proomsize)
        this->Proomsize = 64; // this is because the older versions consider roomsize=0
    roomsize = (this->Proomsize - 64.0f) / 64.0f;
    if (roomsize > 0.0f)
        roomsize *= 2.0f;
    roomsize = power<10>(roomsize);
    rs = sqrtf(roomsize);
    settype(Ptype);
}


void Reverb::setbandwidth(unsigned char Pbandwidth_)
{
    Pbandwidth = Pbandwidth_;
    float v = Pbandwidth / 127.0f;
    if (bandwidth)
        bandwidth->setBandwidth(powf(v, 2.0f) * 200.0f);
}


void Reverb::setpreset(unsigned char npreset)
{
    if (npreset < 0xf)
    {
        if (npreset >= NUM_PRESETS)
            npreset = NUM_PRESETS - 1;
        for (int n = 0; n < PRESET_SIZE; ++n)
            changepar(n, presets[npreset][n]);
        if (insertion)
            changepar(0, presets[npreset][0] / 2); // lower the volume if this is insertion effect
        Ppreset = npreset;
    }
    else
    {
        unsigned char preset = npreset & 0xf;
        unsigned char param = npreset >> 4;
        if (param == 0xf)
            param = 0;
        changepar(param, presets[preset][param]);
        if (insertion && (param == 0))
            changepar(0, presets[preset][0] / 2);
    }
    Pchanged = false;
}


void Reverb::changepar(int npar, unsigned char value)
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
            settime(value);
            break;
        case 3:
            setidelay(value);
            break;
        case 4:
            setidelayfb(value);
            break;
    //  case 5: setrdelay(value);
    //      break;
    //  case 6: seterbalance(value);
    //      break;
        case 7:
            setlpf(value);
            break;
        case 8:
            sethpf(value);
            break;
        case 9:
            setlohidamp(value);
            break;
        case 10:
            settype(value);
            if (value == 2)
                setbandwidth(20); // TODO use defaults
            break;
        case 11:
            setroomsize(value);
            break;
        case 12:
            setbandwidth(value);
            break;
    }
    Pchanged = true;
}


unsigned char Reverb::getpar(int npar)
{
    switch (npar)
    {
        case -1: return Pchanged;
        case 0:  return Pvolume;
        case 1:  return Ppanning;
        case 2:  return Ptime;
        case 3:  return Pidelay;
        case 4:  return Pidelayfb;
    //  case 5: return(Prdelay);
    //      break;
    //  case 6: return(Perbalance);
    //      break;
        case 7:  return Plpf;
        case 8:  return Phpf;
        case 9:  return Plohidamp;
        case 10: return Ptype;
        case 11: return Proomsize;
        case 12: return Pbandwidth;
        default: break;
    }
    return 0; // in case of bogus "parameter"
}


float Revlimit::getlimits(CommandBlock *getData)
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
            if (npart != TOPLEVEL::section::systemEffects) // system effects
                def /=2;
            break;
        case 1:
            break;
        case 2:
            break;
        case 3:
            break;
        case 4:
            break;
        case 7:
            break;
        case 8:
            break;
        case 9:
            min = 64;
            break;
        case 10:
            max = 2;
            canLearn = 0;
            break;
        case 11:
            canLearn = 0;
            break;
        case 12:
            break;
        case 16:
            max = 12;
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
