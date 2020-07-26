/*
    FilterParams.cpp - Parameters for filter

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2019, Will Godfrey

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

    Modified May 2019
*/

#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Misc/NumericFuncs.h"
#include "Params/FilterParams.h"

using func::rap2dB;


FilterParams::FilterParams(unsigned char Ptype_, unsigned char Pfreq_, unsigned  char Pq_, unsigned char Pfreqtrackoffset_, SynthEngine *_synth) :
    Presets(_synth),
    changed(false),
    Dtype(Ptype_),
    Dfreq(Pfreq_),
    Dq(Pq_),
    Dfreqtrackoffset(Pfreqtrackoffset_)
{
    setpresettype("Pfilter");
    defaults();
}


void FilterParams::defaults(void)
{
    Ptype = Dtype;
    Pfreq = Dfreq;
    Pq = Dq;

    Pstages = 0;
    Pfreqtrack = 64;
    Pfreqtrackoffset = Dfreqtrackoffset;
    Pgain = 64;
    Pcategory = 0;

    Pnumformants = 3;
    Pformantslowness = 64;
    for (int j = 0; j < FF_MAX_VOWELS; ++j)
        defaults(j);

    Psequencesize = 3;
    for (int i = 0; i < FF_MAX_SEQUENCE; ++i)
        Psequence[i].nvowel = i % FF_MAX_VOWELS;

    Psequencestretch = 40;
    Psequencereversed = 0;
    Pcenterfreq = 64; // 1 kHz
    Poctavesfreq = 64;
    Pvowelclearness = 64;
}


void FilterParams::defaults(int n)
{
    int j = n;
    for (int i = 0; i < FF_MAX_FORMANTS; ++i)
    {
        Pvowels[j].formants[i].freq = synth->randomINT() >> 24; // some random freqs
        Pvowels[j].formants[i].q = 64;
        Pvowels[j].formants[i].amp = 127;
    }
}


// Get the parameters from other FilterParams
void FilterParams::getfromFilterParams(FilterParams *pars)
{
    defaults();
    if (pars == NULL)
        return;

    Ptype = pars->Ptype;
    Pfreq = pars->Pfreq;
    Pq = pars->Pq;

    Pstages = pars->Pstages;
    Pfreqtrack = pars->Pfreqtrack;
    Pgain = pars->Pgain;
    Pcategory = pars->Pcategory;

    Pnumformants = pars->Pnumformants;
    Pformantslowness = pars->Pformantslowness;
    for (int j = 0; j < FF_MAX_VOWELS; ++j)
    {
        for (int i = 0; i < FF_MAX_FORMANTS; ++i)
        {
            Pvowels[j].formants[i].freq = pars->Pvowels[j].formants[i].freq;
            Pvowels[j].formants[i].q = pars->Pvowels[j].formants[i].q;
            Pvowels[j].formants[i].amp = pars->Pvowels[j].formants[i].amp;
        }
    }

    Psequencesize = pars->Psequencesize;
    for (int i = 0; i < FF_MAX_SEQUENCE; ++i)
        Psequence[i].nvowel = pars->Psequence[i].nvowel;

    Psequencestretch = pars->Psequencestretch;
    Psequencereversed = pars->Psequencereversed;
    Pcenterfreq = pars->Pcenterfreq;
    Poctavesfreq = pars->Poctavesfreq;
    Pvowelclearness = pars->Pvowelclearness;
}


// Parameter control
float FilterParams::getfreq(void)
{
    return (Pfreq / 64.0f - 1.0f) * 5.0f;
}


float FilterParams::getq(void)
{
    return expf(powf(Pq / 127.0f, 2.0f) * logf(1000.0f)) - 0.9f;
}


float FilterParams::getfreqtracking(float notefreq)
{
    if (Pfreqtrackoffset != 0)
    {
        // In this setting freq.tracking's range is: 0% to 198%
        // 100% for value 64
        return logf(notefreq / 440.0f) * Pfreqtrack / (64.0f * LOG_2);
    }
    else
    {
        // In this original setting freq.tracking's range is: -100% to +98%
        // It does not reach up to 100% because the maximum value of
        // Pfreqtrack is 127. Pfreqtrack==128 would give 100%
        return logf(notefreq / 440.0f) * (Pfreqtrack - 64.0f) / (64.0f * LOG_2);
    }
}


float FilterParams::getgain(void)
{
    return (Pgain / 64.0f - 1.0f) * 30.0f; // -30..30dB
}


// Get the center frequency of the formant's graph
float FilterParams::getcenterfreq(void)
{
    return 10000.0f * powf(10.0f, -(1.0f - Pcenterfreq / 127.0f) * 2.0f);
}


// Get the number of octave that the formant functions applies to
float FilterParams::getoctavesfreq(void)
{
    return 0.25f + 10.0f * Poctavesfreq / 127.0f;
}


// Get the frequency from x, where x is [0..1]
float FilterParams::getfreqx(float x)
{
    if (x > 1.0f)
        x = 1.0f;
    float octf = powf(2.0f, getoctavesfreq());
    return getcenterfreq() / sqrtf(octf) * powf(octf, x);
}


// Get the x coordinate from frequency (used by the UI)
float FilterParams::getfreqpos(float freq)
{
    return (logf(freq) - logf(getfreqx(0.0f))) / logf(2.0f) / getoctavesfreq();
}


// Get the freq. response of the formant filter
void FilterParams::formantfilterH(int nvowel, int nfreqs, float *freqs)
{
    float c[3], d[3];
    float filter_freq, filter_q, filter_amp;
    float omega, sn, cs, alpha;

    for (int i = 0; i < nfreqs; ++i)
        freqs[i] = 0.0;

    // for each formant...
    for (int nformant = 0; nformant < Pnumformants; ++nformant)
    {
        // compute formant parameters(frequency,amplitude,etc.)
        filter_freq = getformantfreq(Pvowels[nvowel].formants[nformant].freq);
        filter_q = getformantq(Pvowels[nvowel].formants[nformant].q) * getq();
        if (Pstages > 0)
            filter_q = (filter_q > 1.0)
                        ? powf(filter_q, (1.0f / (Pstages + 1)))
                        : filter_q;

        filter_amp = getformantamp(Pvowels[nvowel].formants[nformant].amp);

        if (filter_freq <= (synth->halfsamplerate_f - 100.0f))
        {
            omega = TWOPI * filter_freq / synth->samplerate_f;
            sn = sinf(omega);
            cs = cosf(omega);
            alpha = sn / (2 * filter_q);
            float tmp = 1 + alpha;
            c[0] = alpha / tmp * sqrtf(filter_q + 1);
            c[1] = 0;
            c[2] = -alpha / tmp * sqrtf(filter_q + 1);
            d[1] = -2.0f * cs / tmp * (-1);
            d[2] = (1 - alpha) / tmp * (-1);
        } else
            continue;

        for (int i = 0; i < nfreqs; ++i)
        {
            float freq = getfreqx(i / (float)nfreqs);
            if (freq > synth->halfsamplerate_f)
            {
                for (int tmp = i; tmp < nfreqs; ++tmp)
                    freqs[tmp] = 0.0f;
                break;
            }
            float fr = freq / synth->samplerate_f * TWOPI;
            float x = c[0], y = 0.0f;
            for (int n = 1; n < 3; ++n)
            {
                x += cosf(n * fr) * c[n];
                y -= sinf(n * fr) * c[n];
            }
            float h = x * x + y * y;
            x = 1.0f;
            y = 0.0f;
            for (int n = 1; n < 3; ++n)
            {
                x -= cosf(n * fr) * d[n];
                y += sinf(n * fr) * d[n];
            }
            h = h / (x * x + y * y);

            freqs[i] += powf(h, ((Pstages + 1.0f) / 2.0f)) * filter_amp;
        }
    }
    for (int i = 0; i < nfreqs; ++i)
    {
        if (freqs[i] > 0.000000001f)
            freqs[i] = rap2dB(freqs[i]) + getgain();
        else
            freqs[i] = -90.0f;
    }
}


void FilterParams::add2XMLsection(XMLwrapper *xml,int n)
{
    int nvowel = n;
    for (int nformant = 0; nformant < FF_MAX_FORMANTS; ++nformant)
    {
        xml->beginbranch("FORMANT",nformant);
        xml->addpar("freq",Pvowels[nvowel].formants[nformant].freq);
        xml->addpar("amp",Pvowels[nvowel].formants[nformant].amp);
        xml->addpar("q",Pvowels[nvowel].formants[nformant].q);
        xml->endbranch();
    }
}


void FilterParams::add2XML(XMLwrapper *xml)
{
    //filter parameters
    xml->addpar("category",Pcategory);
    xml->addpar("type",Ptype);
    xml->addpar("freq",Pfreq);
    xml->addpar("q",Pq);
    xml->addpar("stages",Pstages);
    xml->addpar("freq_track",Pfreqtrack);
    xml->addparbool("freqtrackoffset",Pfreqtrackoffset);
    xml->addpar("gain",Pgain);

    //formant filter parameters
    if ((Pcategory==1)||(!xml->minimal))
    {
        xml->beginbranch("FORMANT_FILTER");
        xml->addpar("num_formants",Pnumformants);
        xml->addpar("formant_slowness",Pformantslowness);
        xml->addpar("vowel_clearness",Pvowelclearness);
        xml->addpar("center_freq",Pcenterfreq);
        xml->addpar("octaves_freq",Poctavesfreq);
        for (int nvowel=0;nvowel<FF_MAX_VOWELS;nvowel++)
        {
            xml->beginbranch("VOWEL",nvowel);
            add2XMLsection(xml,nvowel);
            xml->endbranch();
        }
        xml->addpar("sequence_size",Psequencesize);
        xml->addpar("sequence_stretch",Psequencestretch);
        xml->addparbool("sequence_reversed",Psequencereversed);
        for (int nseq=0;nseq<FF_MAX_SEQUENCE;nseq++)
        {
            xml->beginbranch("SEQUENCE_POS",nseq);
            xml->addpar("vowel_id",Psequence[nseq].nvowel);
            xml->endbranch();
        }
        xml->endbranch();
    }
}


void FilterParams::getfromXMLsection(XMLwrapper *xml,int n)
{
    int nvowel=n;
    for (int nformant = 0; nformant < FF_MAX_FORMANTS; nformant++)
    {
        if (xml->enterbranch("FORMANT",nformant) == 0)
            continue;
        Pvowels[nvowel].formants[nformant].freq =
            xml->getpar127("freq",Pvowels[nvowel].formants[nformant].freq);
        Pvowels[nvowel].formants[nformant].amp =
            xml->getpar127("amp",Pvowels[nvowel].formants[nformant].amp);
        Pvowels[nvowel].formants[nformant].q =
            xml->getpar127("q",Pvowels[nvowel].formants[nformant].q);
        xml->exitbranch();
    }
}


void FilterParams::getfromXML(XMLwrapper *xml)
{
    // filter parameters
    Pcategory = xml->getpar127("category",Pcategory);
    Ptype = xml->getpar127("type",Ptype);
    Pfreq = xml->getpar127("freq",Pfreq);
    Pq = xml->getpar127("q",Pq);
    Pstages = xml->getpar127("stages",Pstages);
    Pfreqtrack = xml->getpar127("freq_track",Pfreqtrack);
    Pfreqtrackoffset = xml->getparbool("freqtrackoffset", Pfreqtrackoffset);
    Pgain = xml->getpar127("gain",Pgain);

    // formant filter parameters
    if (xml->enterbranch("FORMANT_FILTER"))
    {
        Pnumformants = xml->getpar127("num_formants",Pnumformants);
        Pformantslowness = xml->getpar127("formant_slowness",Pformantslowness);
        Pvowelclearness = xml->getpar127("vowel_clearness",Pvowelclearness);
        Pcenterfreq = xml->getpar127("center_freq",Pcenterfreq);
        Poctavesfreq = xml->getpar127("octaves_freq",Poctavesfreq);

        for (int nvowel = 0; nvowel < FF_MAX_VOWELS;nvowel++)
        {
            if (xml->enterbranch("VOWEL",nvowel) == 0)
                continue;
            getfromXMLsection(xml,nvowel);
            xml->exitbranch();
        }
        Psequencesize = xml->getpar127("sequence_size",Psequencesize);
        Psequencestretch = xml->getpar127("sequence_stretch",Psequencestretch);
        Psequencereversed = xml->getparbool("sequence_reversed",Psequencereversed);
        for (int nseq = 0; nseq < FF_MAX_SEQUENCE;nseq++)
        {
            if (xml->enterbranch("SEQUENCE_POS",nseq) == 0)
                continue;
            Psequence[nseq].nvowel = xml->getpar("vowel_id",
                                          Psequence[nseq].nvowel, 0,
                                          FF_MAX_VOWELS-1);
            xml->exitbranch();
        }
        xml->exitbranch();
    }
}

float filterLimit::getFilterLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int kitItem = getData->data.kit;
    int engine = getData->data.engine;

    unsigned char type = 0;

    // filter defaults
    int min = 0;
    int max = 127;
    float def = 64;
    type |= TOPLEVEL::type::Integer;
    unsigned char learnable = TOPLEVEL::type::Learnable;
    type |= learnable;

    switch (control)
    {
        case FILTERINSERT::control::centerFrequency:
            if (kitItem == EFFECT::type::dynFilter)
                def = 45;
            else if (engine == PART::engine::subSynth)
                def = 80;
            else if (engine >= PART::engine::addVoice1)
                def = 50;
            else
                def = 94;
            break;
        case FILTERINSERT::control::Q:
            if (engine >= PART::engine::addVoice1)
                def = 60;
            else if (kitItem != EFFECT::type::dynFilter)
                def = 40;
            break; // for dynFilter it's the default 64
        case FILTERINSERT::control::frequencyTracking:
            break;
        case FILTERINSERT::control::velocitySensitivity:
            if (engine >= PART::engine::addVoice1)
                def = 0;
            break;
        case FILTERINSERT::control::velocityCurve:
            break;
        case FILTERINSERT::control::gain:
            break;
        case FILTERINSERT::control::stages:
            if (kitItem == EFFECT::type::dynFilter)
                def = 1;
            else
                def = 0;
            max = 4;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::baseType:
            max = 2;
            def = 0;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::analogType:
            max = 8;
            def = 1;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::stateVariableType:
            max = 3;
            def = 0;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::frequencyTrackingRange:
            max = 1;
            def = 0;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::formantSlowness:
            break;
        case FILTERINSERT::control::formantClearness:
            break;
        case FILTERINSERT::control::formantFrequency:
            if (request == TOPLEVEL::type::Default)
                type |= TOPLEVEL::type::Error;
            // it's random so inhibit default
            break;
        case FILTERINSERT::control::formantQ:
            break;
        case FILTERINSERT::control::formantAmplitude:
            def = 127;
            break;
        case FILTERINSERT::control::formantStretch:
            def = 40;
            break;
        case FILTERINSERT::control::formantCenter:
            break;
        case FILTERINSERT::control::formantOctave:
            break;
        case FILTERINSERT::control::numberOfFormants:
            min = 1;
            max = 12;
            def = 3;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::vowelNumber:
            max = 5;
            def = 0;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::formantNumber:
            max = 11;
            def = 0;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::sequenceSize:
            min = 1;
            max = 8;
            def = 3;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::sequencePosition:
            def = 0;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::vowelPositionInSequence:
            max = 5;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::negateInput:
            max = 1;
            def = 0;
            type &= ~learnable;
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

