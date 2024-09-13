/*
    EQ.cpp - Equalizer effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
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

#include "Misc/NumericFuncs.h"
#include "Misc/SynthEngine.h"
#include "Misc/Util.h"
#include "Effects/EQ.h"

#include <cstddef>
#include <cassert>

using func::power;
using func::powFrac;
using func::asDecibel;
using util::unConst;
using util::max;

using std::make_unique;


EQ::EQ(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine& _synth)
    : Effect(insertion_, efxoutl_, efxoutr_, NULL, 0, _synth)
    , Pchanged{false}
    , Pvolume{}
    , Pband{0}
    , filter{synth,synth,synth,synth,synth,synth,synth,synth} // MAX_EQ_BANDS
    , filterSnapshot{}
{
    // default values
    setvolume(50);
    setpreset(Ppreset);
    cleanup();
}


// Cleanup the effect
void EQ::cleanup()
{
    Effect::cleanup();
    for (int i = 0; i < MAX_EQ_BANDS; ++i)
    {
        filter[i].l->cleanup();
        filter[i].r->cleanup();
    }
}


// Effect output
void EQ::out(float *smpsl, float *smpsr)
{
    outvolume.advanceValue(synth.sent_buffersize);

    memcpy(efxoutl, smpsl, synth.sent_bufferbytes);
    memcpy(efxoutr, smpsr, synth.sent_bufferbytes);
    for (int i = 0; i < synth.sent_buffersize; ++i)
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
        filter[i].freq.advanceValue(synth.sent_buffersize);
        float newval = filter[i].freq.getValue();
        if (oldval != newval)
        {
            filter[i].l->interpolatenextbuffer();
            filter[i].l->setfreq(newval);
            filter[i].r->interpolatenextbuffer();
            filter[i].r->setfreq(newval);
        }

        oldval = filter[i].gain.getValue();
        filter[i].gain.advanceValue(synth.sent_buffersize);
        newval = filter[i].gain.getValue();
        if (oldval != newval)
        {
            filter[i].l->interpolatenextbuffer();
            filter[i].l->setgain(newval);
            filter[i].r->interpolatenextbuffer();
            filter[i].r->setgain(newval);
        }

        oldval = filter[i].q.getValue();
        filter[i].q.advanceValue(synth.sent_buffersize);
        newval = filter[i].q.getValue();
        if (oldval != newval)
        {
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

void EQ::setvolume(uchar Pvolume_)
{
    Pvolume = Pvolume_;
    float tmp = 10.0f * powFrac<200>(1.0f - Pvolume / 127.0f);
    outvolume.setTargetValue(tmp);
    volume.setTargetValue((!insertion) ? 1.0f : tmp);
}


void EQ::setpreset(uchar npreset)
{
    const int PRESET_SIZE = 1;
    const int NUM_PRESETS = 2;
    uchar presets[NUM_PRESETS][PRESET_SIZE] = {
        { EQmaster_def }, // EQ 1
        { EQmaster_def }  // EQ 2
    };

    if (npreset >= NUM_PRESETS)
        npreset = NUM_PRESETS - 1;
    for (int n = 0; n < PRESET_SIZE; ++n)
        changepar(n, presets[npreset][n]);
    Ppreset = npreset;
    Pchanged = true;
}


void EQ::changepar(int npar, uchar value)
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
            Pband = value;
            break;
    }
    if (npar < 10)
        return;

    int nb = (npar - 10) / 5; // number of the band (filter)
    if (nb >= MAX_EQ_BANDS)
        return;
    int bp = npar % 5; // band parameter

    float tmp;
    switch (bp)
    {
        case 0:
            filter[nb].Ptype = value;
            if (value > AnalogFilter::MAX_TYPES)
                filter[nb].Ptype = 0;
            if (filter[nb].Ptype != 0)
            {
                filter[nb].l->settype(value - 1);
                filter[nb].r->settype(value - 1);
            }
            break;

        case 1:
            filter[nb].Pfreq = value;
            tmp = 600.0f * power<30>((value - 64.0f) / 64.0f);
            filter[nb].freq.setTargetValue(tmp);
            break;

        case 2:
            filter[nb].Pgain = value;
            tmp = 30.0f * (value - 64.0f) / 64.0f;
            filter[nb].gain.setTargetValue(tmp);
            break;

        case 3:
            filter[nb].Pq = value;
            tmp = power<30>((value - 64.0f) / 64.0f);
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
    Pchanged = true;
}


uchar EQ::getpar(int npar) const
{
    switch (npar)
    {
        case -1: return Pchanged;
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
    int bp = npar % 5; // band parameter
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


/**
 * Special implementation, since only EQ uses the high number of parameters.
 */
void EQ::getAllPar(EffectParArray& target) const
{
    for (uint i=0; i<target.size(); ++i)
        target[i] = this->getpar(i);
}


float EQlimit::getlimits(CommandBlock *getData)
{
    int value = getData->data.value;
    int control = getData->data.control;
    int request = getData->data.type & TOPLEVEL::type::Default; // clear flags

    int min = 0;
    int max = 127;
    int def = 0;
    uchar canLearn = TOPLEVEL::type::Learnable;
    uchar isInteger = TOPLEVEL::type::Integer;

    switch (control)
    {
        case 0:
            def = EQmaster_def;
            break;
        case 1:
            max = 7;
            canLearn = 0;
            break;
        case 10:
            max = 9;
            canLearn = 0;
            break;
        case 11:
            def = EQfreq_def;
            break;
        case 12:
            def = EQgain_def;
            break;
        case 13:
            def = EQq_def;
            break;
        case 14:
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



/**
 * Helper: an inline buffer to maintain a temporary copy of an AnalogFilter,
 * used to compute the filter coefficients and then the response for GUI presentation.
 * We can not use the actual filters, since their values will be interpolated gradually.
 * Rather, we need to work from the pristine FilterParameter settings of this EQ.
 */
class EQ::FilterSnapshot
{
    // a chunk of raw uninitialised storage of suitable size
    alignas(AnalogFilter)
        std::byte buffer_[sizeof(AnalogFilter)];

    EQ const& eq;

public:
   ~FilterSnapshot()
    { destroy(); }

    FilterSnapshot(EQ const& outer)
        : eq{outer}
    {
        emplaceFilter(0, 1000, 1.0, 1, 1.0);
    }// ensure there is always a dummy object emplaced


    void captureBand(uint idx)
    {
        assert(idx < MAX_EQ_BANDS);
        FilterParam const& par{eq.filter[idx]};
        destroy();
        emplaceFilter(max(0, par.Ptype-1)       // Ptype == 0 means disabled -- skipped in calcResponse()
                     ,par.freq.getTargetValue()
                     ,par.q.getTargetValue()
                     ,par.Pstages
                     ,par.gain.getTargetValue()
                     );
    }

    AnalogFilter& access()
    {
        return * std::launder (reinterpret_cast<AnalogFilter*> (&buffer_));
    }

private:
    void emplaceFilter(uchar type, float freq, float q, uchar stages, float dBgain)
    {
        new(&buffer_) AnalogFilter(unConst(eq).synth, type,freq,q,stages,dBgain);
    }

    void destroy()
    {
        access().~AnalogFilter();
    }
};


/**
 * Prepare the Lookup-Table used by the EQGraph-UI to display the
 * gain response as function of the frequency. The number of step points in the LUT
 * is defined by EQ_GRAPH_STEPS; these »slots« span an X-axis running from [0.0 ... 1.0].
 * The translation of these scale points into actual frequencies is defined by xScaleFac(freq),
 * where 0.0 corresponds to 20Hz and 1.0 corresponds to 20kHz. This render calculation is
 * invoked on each push-update for an EQ -- see SynthEngine::pushEffectUpdate(part);
 * this is unconditionally invoked on each parameter change (yet seems to be fast enough).
 */
void EQ::renderResponse(EQGraphArray & lut) const
{
    auto subNyquist = [this](float f){ return f <= synth.halfsamplerate_f; };
    for (uint i=0; i<lut.size(); ++i)
    {
        float gridFactor = float(i) / (lut.size()-1);  // »fence post problem« : both 0.0 and 1.0 included
        float slotFreq = xScaleFreq(gridFactor);
        lut[i] = subNyquist(slotFreq)? yScaleFac(calcResponse(slotFreq))
                                     : -1.0f;
    }
}

float EQ::calcResponse(float freq) const
{
    if (not filterSnapshot)
        filterSnapshot = make_unique<FilterSnapshot>(*this);

    float response{1.0};
    for (int i = 0; i < MAX_EQ_BANDS; ++i)
    {
        if (filter[i].Ptype == 0)
            continue;
        filterSnapshot->captureBand(i);
        response *= filterSnapshot->access().calcFilterResponse(freq);
    }
    // Only for UI purposes, use target value.
    return asDecibel(response * outvolume.getTargetValue());
}
