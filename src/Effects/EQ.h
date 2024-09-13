/*
  EQ.h - Equalizer Effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2018-2019, Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original.

    Modified March 2019
*/

#ifndef EQ_H
#define EQ_H

#include "globals.h"
#include "Misc/SynthEngine.h"
#include "DSP/AnalogFilter.h"
#include "Effects/Effect.h"

#include <algorithm>
#include <memory>

const uchar EQmaster_def = 67;
const uchar EQfreq_def = 64;
const uchar EQgain_def = 64;
const uchar EQq_def = 64;

class EQ : public Effect
{
    public:
        EQ(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine&);
       ~EQ() = default;

        void out(float *smpsl, float *smpr)   override;
        void setpreset(uchar npreset)         override;
        void changepar(int npar, uchar value) override;
        uchar getpar(int npar)          const override;
        void getAllPar(EffectParArray&) const override;
        void cleanup()                        override;

        /** render transfer function for UI */
        void renderResponse(EQGraphArray & lut) const;

        /** scale helpers for the response diagram */
        static float xScaleFreq(float fac);
        static float xScaleFac(float freq);
        static float yScaleFac(float dB);

    private:
        constexpr static auto GRAPH_MIN_FREQ = 20;
        constexpr static auto GRAPH_MAX_dB   = 30;

        void setvolume(uchar Pvolume_);
        float calcResponse(float freq) const;

        // Parameters
        bool Pchanged;
        uchar Pvolume;
        uchar Pband;

        struct FilterParam
        {
            uchar Ptype, Pfreq, Pgain, Pq, Pstages; // parameters
            synth::InterpolatedValue<float> freq, gain, q;
            std::unique_ptr<AnalogFilter> l; // internal values
            std::unique_ptr<AnalogFilter> r; // internal values

            FilterParam(SynthEngine& synth)
                :Ptype{0}
                ,Pfreq{EQfreq_def}
                ,Pgain{EQgain_def}
                ,Pq{EQq_def}
                ,Pstages{0}
                ,freq{0, synth.samplerate}
                ,gain{0, synth.samplerate}
                ,q   {0, synth.samplerate}
                ,l{new AnalogFilter(synth, TOPLEVEL::filter::Peak2, 1000.0, 1.0, 0)}
                ,r{new AnalogFilter(synth, TOPLEVEL::filter::Peak2, 1000.0, 1.0, 0)}
            { }
        };

        FilterParam filter[MAX_EQ_BANDS];

        class FilterSnapshot;
        mutable std::unique_ptr<FilterSnapshot> filterSnapshot;
};

class EQlimit
{
    public:
        float getlimits(CommandBlock *getData);
};




inline float EQ::xScaleFreq(float fac)
{
    if (fac > 1.0)
        fac = 1.0;
    return GRAPH_MIN_FREQ * power<1000>(fac);
}

inline float EQ::xScaleFac(float freq)
{
    if (freq < GRAPH_MIN_FREQ)
        freq = GRAPH_MIN_FREQ;
    return logf(freq / GRAPH_MIN_FREQ) / logf(1000.0);
}

inline float EQ::yScaleFac(float dB)
{
    return (1 + dB / GRAPH_MAX_dB) / 2.0;
}


#endif /*EQ.h*/
