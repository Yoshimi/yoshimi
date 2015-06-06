/*
    FilterParams.cpp - Parameters for filter

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

#include <cmath>
//#include <iostream>

//using namespace std;

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Params/FilterParams.h"

FilterParams::FilterParams(unsigned char Ptype_, unsigned char Pfreq_,
                           unsigned  char Pq_) :
    Presets(),
    changed(false),
    Dtype(Ptype_),
    Dfreq(Pfreq_),
    Dq(Pq_)
{
    setPresetType("Pfilter");
    setDefaults();
}


void FilterParams::setDefaults(void)
{
    Ptype = Dtype;
    Pfreq = Dfreq;
    Pq = Dq;

    Pstages = 0;
    Pfreqtrack = 64;
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
        Pvowels[j].formants[i].freq = (int)(RND * 127.0); // some random freqs
        Pvowels[j].formants[i].q = 64;
        Pvowels[j].formants[i].amp = 127;
    }
}


// Get the parameters from other FilterParams
void FilterParams::getfromFilterParams(FilterParams *pars)
{
    setDefaults();
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
float FilterParams::getFreq(void)
{
    return (Pfreq / 64.0 - 1.0) * 5.0;
}


float FilterParams::getQ(void)
{
    return expf(powf(Pq / 127.0, 2) * logf(1000.0)) - 0.9;
}


float FilterParams::getFreqTracking(float notefreq)
{
    return logf(notefreq / 440.0) * (Pfreqtrack - 64.0) / (64.0 * LOG_2);
}


float FilterParams::getGain(void)
{
    return (Pgain / 64.0 - 1.0) * 30.0; // -30..30dB
}


// Get the center frequency of the formant's graph
float FilterParams::getCenterFreq(void)
{
    return 10000.0 * powf(10, -(1.0 - Pcenterfreq / 127.0) * 2.0);
}


// Get the number of octave that the formant functions applies to
float FilterParams::getOctavesFreq(void)
{
    return 0.25 + 10.0 * Poctavesfreq / 127.0;
}


// Get the frequency from x, where x is [0..1]
float FilterParams::getFreqX(float x)
{
    if (x > 1.0)
        x = 1.0;
    float octf = powf(2.0, getOctavesFreq());
    return getCenterFreq() / sqrtf(octf) * powf(octf, x);
}


// Get the x coordinate from frequency (used by the UI)
float FilterParams::getFreqPos(float freq)
{
    return (logf(freq) - logf(getFreqX(0.0))) / logf(2.0) / getOctavesFreq();
}


// Get the freq. response of the formant filter
void FilterParams::formantFilterH(int nvowel, int nfreqs, float *freqs)
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
        filter_freq = getFormantFreq(Pvowels[nvowel].formants[nformant].freq);
        filter_q = getFormantQ(Pvowels[nvowel].formants[nformant].q) * getQ();
        if (Pstages > 0)
            filter_q = (filter_q > 1.0)
                        ? powf(filter_q, (1.0 / (Pstages + 1)))
                        : filter_q;

        filter_amp = getFormantAmp(Pvowels[nvowel].formants[nformant].amp);

        if (filter_freq <= (samplerate / 2 - 100.0))
        {
            omega = 2 * PI * filter_freq / samplerate;
            sn = sinf(omega);
            cs = cosf(omega);
            alpha = sn / (2 * filter_q);
            float tmp = 1 + alpha;
            c[0] = alpha / tmp * sqrtf(filter_q + 1);
            c[1] = 0;
            c[2] = -alpha / tmp * sqrtf(filter_q + 1);
            d[1] = -2 * cs / tmp * (-1);
            d[2] = (1 - alpha) / tmp * (-1);
        } else
            continue;

        for (int i = 0; i < nfreqs; ++i)
        {
            float freq = getFreqX(i / (float)nfreqs);
            if (freq > samplerate / 2)
            {
                for (int tmp = i; tmp < nfreqs; ++tmp)
                    freqs[tmp] = 0.0;
                break;
            }
            float fr = freq / samplerate * PI * 2.0;
            float x = c[0], y = 0.0;
            for (int n = 1; n < 3; ++n)
            {
                x += cosf(n * fr) * c[n];
                y -= sinf(n * fr) * c[n];
            }
            float h = x * x + y * y;
            x = 1.0;
            y = 0.0;
            for (int n = 1; n < 3; ++n)
            {
                x -= cosf(n * fr) * d[n];
                y += sinf(n * fr) * d[n];
            }
            h = h / (x * x + y * y);

            freqs[i] += powf(h, ((Pstages + 1.0) / 2.0)) * filter_amp;
        }
    }
    for (int i = 0; i < nfreqs; ++i)
    {
        if (freqs[i] > 0.000000001)
            freqs[i] = rap2dB(freqs[i]) + getGain();
        else
            freqs[i] = -90.0;
    }
}


// Transforms a parameter to the real value
float FilterParams::getFormantFreq(unsigned char freq)
{
    float result = getFreqX(freq / 127.0);
    return result;
}


float FilterParams::getFormantAmp(unsigned char amp)
{
    float result = powf(0.1, (1.0 - amp / 127.0) * 4.0);
    return result;
}


float FilterParams::getFormantQ(unsigned char q)
{
    // temp
    float result = powf(25.0, (q - 32.0) / 64.0);
    return result;
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
    xml->addpar("gain",Pgain);

    //formant filter parameters
    if ((Pcategory==1)||(!xml->minimal)) {
        xml->beginbranch("FORMANT_FILTER");
        xml->addpar("num_formants",Pnumformants);
        xml->addpar("formant_slowness",Pformantslowness);
        xml->addpar("vowel_clearness",Pvowelclearness);
        xml->addpar("center_freq",Pcenterfreq);
        xml->addpar("octaves_freq",Poctavesfreq);
        for (int nvowel=0;nvowel<FF_MAX_VOWELS;nvowel++) {
            xml->beginbranch("VOWEL",nvowel);
            add2XMLsection(xml,nvowel);
            xml->endbranch();
        }
        xml->addpar("sequence_size",Psequencesize);
        xml->addpar("sequence_stretch",Psequencestretch);
        xml->addparbool("sequence_reversed",Psequencereversed);
        for (int nseq=0;nseq<FF_MAX_SEQUENCE;nseq++) {
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
    for (int nformant=0;nformant<FF_MAX_FORMANTS;nformant++) {
        if (xml->enterbranch("FORMANT",nformant)==0) continue;
        Pvowels[nvowel].formants[nformant].freq=xml->getpar127("freq",Pvowels[nvowel].formants[nformant].freq);
        Pvowels[nvowel].formants[nformant].amp=xml->getpar127("amp",Pvowels[nvowel].formants[nformant].amp);
        Pvowels[nvowel].formants[nformant].q=xml->getpar127("q",Pvowels[nvowel].formants[nformant].q);
        xml->exitbranch();
    }
}


void FilterParams::getfromXML(XMLwrapper *xml)
{
    // filter parameters
    Pcategory=xml->getpar127("category",Pcategory);
    Ptype=xml->getpar127("type",Ptype);
    Pfreq=xml->getpar127("freq",Pfreq);
    Pq=xml->getpar127("q",Pq);
    Pstages=xml->getpar127("stages",Pstages);
    Pfreqtrack=xml->getpar127("freq_track",Pfreqtrack);
    Pgain=xml->getpar127("gain",Pgain);

    // formant filter parameters
    if (xml->enterbranch("FORMANT_FILTER")) {
        Pnumformants=xml->getpar127("num_formants",Pnumformants);
        Pformantslowness=xml->getpar127("formant_slowness",Pformantslowness);
        Pvowelclearness=xml->getpar127("vowel_clearness",Pvowelclearness);
        Pcenterfreq=xml->getpar127("center_freq",Pcenterfreq);
        Poctavesfreq=xml->getpar127("octaves_freq",Poctavesfreq);

        for (int nvowel=0;nvowel<FF_MAX_VOWELS;nvowel++) {
            if (xml->enterbranch("VOWEL",nvowel)==0) continue;
            getfromXMLsection(xml,nvowel);
            xml->exitbranch();
        }
        Psequencesize=xml->getpar127("sequence_size",Psequencesize);
        Psequencestretch=xml->getpar127("sequence_stretch",Psequencestretch);
        Psequencereversed=xml->getparbool("sequence_reversed",Psequencereversed);
        for (int nseq=0;nseq<FF_MAX_SEQUENCE;nseq++) {
            if (xml->enterbranch("SEQUENCE_POS",nseq)==0) continue;
            Psequence[nseq].nvowel=xml->getpar("vowel_id",Psequence[nseq].nvowel,0,FF_MAX_VOWELS-1);
            xml->exitbranch();
        }
        xml->exitbranch();
    }
}
