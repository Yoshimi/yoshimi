/*
    FilterParams.cpp - Parameters for filter

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2019-2023, Will Godfrey
    Copyringt 2024 Kristian Amlie

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

#include "Misc/XMLStore.h"
#include "Misc/SynthEngine.h"
#include "Misc/NumericFuncs.h"
#include "Params/FilterParams.h"

using func::asDecibel;
using func::power;


FilterParams::FilterParams(uchar Ptype_, float Pfreq_, float Pq_, uchar Pfreqtrackoffset_, SynthEngine& _synth)
    : ParamBase{_synth}
    , changed{false}
    , Dtype{Ptype_}
    , Dfreq{Pfreq_}
    , Dq{Pq_}
    , Dfreqtrackoffset{Pfreqtrackoffset_}
{
    defaults();
}


void FilterParams::defaults()
{
    Ptype = Dtype;
    Pfreq = Dfreq;
    Pq = Dq;

    Pstages = FILTDEF::stages.def;
    Pfreqtrack = FILTDEF::freqTrack.def;
    Pfreqtrackoffset = Dfreqtrackoffset;
    Pgain = FILTDEF::gain.def;
    Pcategory = FILTDEF::category.def;

    Pnumformants = FILTDEF::formCount.def;
    Pformantslowness = FILTDEF::formSpeed.def;
    for (int j = 0; j < FF_MAX_VOWELS; ++j)
        defaults(j);

    Psequencesize = FILTDEF::sequenceSize.def;
    for (int i = 0; i < FF_MAX_SEQUENCE; ++i)
        Psequence[i].nvowel = i % FF_MAX_VOWELS;

    Psequencestretch = FILTDEF::formStretch.def;
    Psequencereversed = FILTSWITCH::sequenceReverse;
    Pcenterfreq = FILTDEF::formCentre.def; // 1 kHz
    Poctavesfreq = FILTDEF::formOctave.def;
    Pvowelclearness = FILTDEF::formClear.def;
}


void FilterParams::defaults(int n)
{
    int j = n;
    for (int i = 0; i < FF_MAX_FORMANTS; ++i)
    {
        Pvowels[j].formants[i].freq = synth.randomINT() >> 24; // some random freqs
        Pvowels[j].formants[i].firstF = Pvowels[j].formants[i].freq; // the only time we set this
        Pvowels[j].formants[i].q = FILTDEF::formQ.def;
        Pvowels[j].formants[i].amp = FILTDEF::formAmp.def;
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
float FilterParams::getfreq()
{
    return (Pfreq / 64.0f - 1.0f) * 5.0f;
}


float FilterParams::getq()
{
    return expf(powf(Pq / 127.0f, 2.0f) * logf(1000.0f)) - 0.9f;
}


float FilterParams::getfreqtracking(float notefreq)
{
    if (Pfreqtrackoffset != FILTSWITCH::trackRange)
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


float FilterParams::getgain()
{
    return (Pgain / 64.0f - 1.0f) * 30.0f; // -30..30dB
}


// Get the center frequency of the formant's graph
float FilterParams::getcenterfreq()
{
    return 10000.0f * power<10>(-(1.0f - Pcenterfreq / FILTDEF::formCentre.max) * 2.0f);
}


// Get the number of octave that the formant functions applies to
float FilterParams::getoctavesfreq()
{
    return 0.25f + 10.0f * Poctavesfreq / FILTDEF::formOctave.max;
}


// Get the frequency from x, where x is [0..1]
float FilterParams::getfreqx(float x)
{
    if (x > 1.0f)
        x = 1.0f;
    float octf = power<2>(getoctavesfreq());
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

        if (filter_freq <= (synth.halfsamplerate_f - 100.0f))
        {
            omega = TWOPI * filter_freq / synth.samplerate_f;
            sn = sinf(omega);
            cs = cosf(omega);
            alpha = sn / (2 * filter_q);
            float tmp = 1 + alpha;
            c[0] = alpha / tmp * sqrtf(filter_q + 1);
            c[1] = 0;
            c[2] = -alpha / tmp * sqrtf(filter_q + 1);
            d[1] = -2.0f * cs / tmp * (-1);
            d[2] = (1 - alpha) / tmp * (-1);
        }
        else
            continue;

        for (int i = 0; i < nfreqs; ++i)
        {
            float freq = getfreqx(i / (float)nfreqs);
            if (freq > synth.halfsamplerate_f)
            {
                for (int tmp = i; tmp < nfreqs; ++tmp)
                    freqs[tmp] = 0.0f;
                break;
            }
            float fr = freq / synth.samplerate_f * TWOPI;
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
            freqs[i] = asDecibel(freqs[i]) + getgain();
        else
            freqs[i] = -90.0f;
    }
}


void FilterParams::add2XML(XMLtree& xmlFilter)
{
    //filter parameters
    xmlFilter.addPar_int ("category"       , Pcategory);
    xmlFilter.addPar_int ("type"           , Ptype);
    xmlFilter.addPar_frac("freq"           , Pfreq);
    xmlFilter.addPar_frac("q"              , Pq);
    xmlFilter.addPar_int ("stages"         , Pstages);
    xmlFilter.addPar_frac("freq_track"     , Pfreqtrack);
    xmlFilter.addPar_bool("freqtrackoffset", Pfreqtrackoffset);
    xmlFilter.addPar_frac("gain"           , Pgain);

    //formant filter parameters
    if ((Pcategory==1) or (synth.getRuntime().xmlmax))
    {
        XMLtree xmlFormant = xmlFilter.addElm("FORMANT_FILTER");
            xmlFormant.addPar_int ("num_formants"    , Pnumformants);
            xmlFormant.addPar_frac("formant_slowness", Pformantslowness);
            xmlFormant.addPar_frac("vowel_clearness" , Pvowelclearness);
            xmlFormant.addPar_int ("center_freq"     , Pcenterfreq);
            xmlFormant.addPar_int ("octaves_freq"    , Poctavesfreq);
            for (uint nvowel=0; nvowel < FF_MAX_VOWELS; nvowel++)
            {
                XMLtree xmlVowel = xmlFormant.addElm("VOWEL",nvowel);
                add2XML_vowel(xmlVowel,nvowel);
            }
            xmlFormant.addPar_int ("sequence_size"    , Psequencesize);
            xmlFormant.addPar_frac("sequence_stretch" , Psequencestretch);
            xmlFormant.addPar_bool("sequence_reversed", Psequencereversed);
            for (uint nseq=0; nseq < FF_MAX_SEQUENCE; nseq++)
            {
                XMLtree xmlSeq = xmlFormant.addElm("SEQUENCE_POS",nseq);
                xmlSeq.addPar_int ("vowel_id", Psequence[nseq].nvowel);
            }
    }
}


void FilterParams::add2XML_vowel(XMLtree& xmlVowel, const uint nvowel)
{
    for (uint nformant = 0; nformant < FF_MAX_FORMANTS; ++nformant)
    {
        XMLtree xmlFormant = xmlVowel.addElm("FORMANT",nformant);
            xmlFormant.addPar_frac("freq", Pvowels[nvowel].formants[nformant].freq);
            xmlFormant.addPar_frac("amp" , Pvowels[nvowel].formants[nformant].amp);
            xmlFormant.addPar_frac("q"   , Pvowels[nvowel].formants[nformant].q);
    }
}


void FilterParams::getfromXML(XMLtree& xmlFilter)
{
    // filter parameters
    Pcategory        = xmlFilter.getPar_127 ("category"       ,Pcategory);
    Ptype            = xmlFilter.getPar_127 ("type"           ,Ptype);
    Pfreq            = xmlFilter.getPar_frac("freq"           ,Pfreq,     FILTDEF::addFreq.min,  FILTDEF::addFreq.max);
    Pq               = xmlFilter.getPar_frac("q"              ,Pq,        FILTDEF::qVal.min,     FILTDEF::qVal.max);
    Pstages          = xmlFilter.getPar_127 ("stages"         ,Pstages);
    Pfreqtrack       = xmlFilter.getPar_frac("freq_track"     ,Pfreqtrack,FILTDEF::freqTrack.min,FILTDEF::freqTrack.max);
    Pfreqtrackoffset = xmlFilter.getPar_bool("freqtrackoffset",Pfreqtrackoffset);
    Pgain            = xmlFilter.getPar_frac("gain"           ,Pgain,     FILTDEF::gain.min,     FILTDEF::gain.max);

    // formant filter parameters
    if (XMLtree xmlFormant = xmlFilter.getElm("FORMANT_FILTER"))
    {
        Pnumformants      = xmlFormant.getPar_127 ("num_formants"     ,Pnumformants);
        Pformantslowness  = xmlFormant.getPar_frac("formant_slowness" ,Pformantslowness,FILTDEF::formSpeed.min, FILTDEF::formSpeed.max);
        Pvowelclearness   = xmlFormant.getPar_frac("vowel_clearness"  ,Pvowelclearness, FILTDEF::formClear.min, FILTDEF::formClear.max);
        Pcenterfreq       = xmlFormant.getPar_127 ("center_freq"      ,Pcenterfreq);
        Poctavesfreq      = xmlFormant.getPar_127 ("octaves_freq"     ,Poctavesfreq);

        for (uint nvowel = 0; nvowel < FF_MAX_VOWELS; nvowel++)
            if (XMLtree xmlVowel = xmlFormant.getElm("VOWEL",nvowel))
                getfromXML_vowel(xmlVowel,nvowel);

        Psequencesize     = xmlFormant.getPar_127 ("sequence_size"    ,Psequencesize);
        Psequencestretch  = xmlFormant.getPar_frac("sequence_stretch" ,Psequencestretch,FILTDEF::formStretch.min,FILTDEF::formStretch.max);
        Psequencereversed = xmlFormant.getPar_bool("sequence_reversed",Psequencereversed);
        for (uint nseq = 0; nseq < FF_MAX_SEQUENCE;nseq++)
            if (XMLtree xmlSeq = xmlFormant.getElm("SEQUENCE_POS",nseq))
                Psequence[nseq].nvowel = xmlSeq.getPar_int("vowel_id", Psequence[nseq].nvowel, 0,FF_MAX_VOWELS-1);
    }
}


void FilterParams::getfromXML_vowel(XMLtree& xmlVowel, const uint nvowel)
{
    for (uint nformant = 0; nformant < FF_MAX_FORMANTS; nformant++)
        if (XMLtree xmlFormant = xmlVowel.getElm("FORMANT",nformant))
        {
            Pvowels[nvowel].formants[nformant].freq =
                xmlFormant.getPar_frac("freq",Pvowels[nvowel].formants[nformant].freq,FILTDEF::formFreq.min,FILTDEF::formFreq.max);
            Pvowels[nvowel].formants[nformant].firstF =Pvowels[nvowel].formants[nformant].freq;
            // the saved setting becomes the new pseudo default value.

            Pvowels[nvowel].formants[nformant].amp =
                xmlFormant.getPar_frac("amp",Pvowels[nvowel].formants[nformant].amp,FILTDEF::formAmp.min,FILTDEF::formAmp.max);
            Pvowels[nvowel].formants[nformant].q =
                xmlFormant.getPar_frac("q",  Pvowels[nvowel].formants[nformant].q,FILTDEF::formQ.min,FILTDEF::formQ.max);
        }
}


float filterLimit::getFilterLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;
    int effType = getData->data.kit;
    int engine = getData->data.engine;
    int offset = getData->data.offset;
    int dynPreset = 0;

    if (effType == EFFECT::type::dynFilter)
    {
        dynPreset = offset << 4;
    }
    unsigned char type = 0;

    // filter defaults
    int min = 0;
    int max = 127;
    float def = 64;
    unsigned char learnable = TOPLEVEL::type::Learnable;
    type |= learnable;

    switch (control)
    {
        case FILTERINSERT::control::centerFrequency:
            if (effType == EFFECT::type::dynFilter)
            {
                switch (dynPreset)
                {
                    case 0:
                        def = FILTDEF::dynFreq0.def;
                        break;
                    case 1:
                        def = FILTDEF::dynFreq1.def;
                        break;
                    case 2:
                        def = FILTDEF::dynFreq2.def;
                        break;
                    case 3:
                        def = FILTDEF::dynFreq3.def;
                        break;
                    case 4:
                        def = FILTDEF::dynFreq4.def;
                        break;
                }
            }
            else if (engine == PART::engine::subSynth)
                def = FILTDEF::subFreq.def;
            else if (engine >= PART::engine::addVoice1)
                def = FILTDEF::voiceFreq.def;
            else
                def = FILTDEF::padFreq.def;
            type &= ~TOPLEVEL::type::Integer;
            break;
        case FILTERINSERT::control::Q:
            if (effType == EFFECT::type::dynFilter)
            {
                switch (dynPreset)
                {
                    case 0:
                        def = FILTDEF::dynQval0.def;
                        break;
                    case 1:
                        def = FILTDEF::dynQval1.def;
                        break;
                    case 2:
                        def = FILTDEF::dynQval2.def;
                        break;
                    case 3:
                        def = FILTDEF::dynQval3.def;
                        break;
                    case 4:
                        def = FILTDEF::dynQval4.def;
                        break;

                }
            }
            else if (engine >= PART::engine::addVoice1)
                def = FILTDEF::voiceQval.def;

            else
                def = FILTDEF::qVal.def;
            type &= ~TOPLEVEL::type::Integer;
            break;
        case FILTERINSERT::control::frequencyTracking:
            def = FILTDEF::freqTrack.def;
            break;
        case FILTERINSERT::control::velocitySensitivity:
            if (engine >= PART::engine::addVoice1)
                def = FILTDEF::voiceVelSense.def;
            else
                def = FILTDEF::velSense.def;
            break;
        case FILTERINSERT::control::velocityCurve:
            def = FILTDEF::velFuncSense.def;
            break;
        case FILTERINSERT::control::gain:
            def = FILTDEF::gain.def;
            break;
        case FILTERINSERT::control::stages:
            type |= TOPLEVEL::type::Integer;
            if (effType == EFFECT::type::dynFilter)
                def = FILTDEF::dynStages.def;
            else
                def = FILTDEF::stages.def;
            max = FILTDEF::stages.max;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::baseType:
            type |= TOPLEVEL::type::Integer;
            max = FILTDEF::category.max;
            def = FILTDEF::category.def;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::analogType:
            type |= TOPLEVEL::type::Integer;
            max = FILTDEF::analogType.max;
            def = FILTDEF::analogType.def;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::stateVariableType:
            type |= TOPLEVEL::type::Integer;
            max = FILTDEF::stVarfType.max;
            def = FILTDEF::stVarfType.def;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::frequencyTrackingRange:
            type |= TOPLEVEL::type::Integer;
            max = true;
            def = FILTSWITCH::trackRange;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::formantSlowness:
            def = FILTDEF::formSpeed.def;
            break;
        case FILTERINSERT::control::formantClearness:
            def = FILTDEF::formClear.def;
            break;
        case FILTERINSERT::control::formantFrequency:
            if (request == TOPLEVEL::type::Default)
                type |= TOPLEVEL::type::Error;
            // it's pseudo random so inhibit default *** change this!
            type &= ~TOPLEVEL::type::Integer;
            break;
        case FILTERINSERT::control::formantQ:
            def = FILTDEF::formQ.def;
            type &= ~TOPLEVEL::type::Integer;
            break;
        case FILTERINSERT::control::formantAmplitude:
            def = FILTDEF::formAmp.def;
            break;
        case FILTERINSERT::control::formantStretch:
            def = FILTDEF::formStretch.def;
            break;
        case FILTERINSERT::control::formantCenter:
            def = FILTDEF::formCentre.def;
            type &= ~TOPLEVEL::type::Integer;
            break;
        case FILTERINSERT::control::formantOctave:
            def = FILTDEF::formOctave.def;
            break;
        case FILTERINSERT::control::numberOfFormants:
            type |= TOPLEVEL::type::Integer;
            min = FILTDEF::formCount.min;
            max = FILTDEF::formCount.max;
            def = FILTDEF::formCount.def;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::vowelNumber:
            type |= TOPLEVEL::type::Integer;
            max = FILTDEF::formVowel.max;
            def = FILTDEF::formVowel.def;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::formantNumber:
            type |= TOPLEVEL::type::Integer;
            max = FILTDEF::formCount.max;
            def = FILTDEF::formCount.def;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::sequenceSize:
            type |= TOPLEVEL::type::Integer;
            min = FILTDEF::sequenceSize.min;
            max = FILTDEF::sequenceSize.max;
            def = FILTDEF::sequenceSize.def;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::sequencePosition:
            type |= TOPLEVEL::type::Integer;
            def = 0;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::vowelPositionInSequence:
            type |= TOPLEVEL::type::Integer;
            max = 5;
            type &= ~learnable;
            break;
        case FILTERINSERT::control::negateInput:
            type |= TOPLEVEL::type::Integer;
            max = true;
            def =FILTSWITCH::sequenceReverse;
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

