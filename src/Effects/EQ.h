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

#include <memory>

const unsigned char EQmaster_def = 67;
const unsigned char EQfreq_def = 64;
const unsigned char EQgain_def = 64;
const unsigned char EQq_def = 64;

class EQ : public Effect
{
    public:
        EQ(bool insertion_, float *efxoutl_, float *efxoutr_, SynthEngine *_synth);
        ~EQ() { };
        void out(float *smpsl, float *smpr);
        void setpreset(unsigned char npreset);
        void changepar(int npar, unsigned char value);
        unsigned char getpar(int npar);
        void cleanup(void);
        float getfreqresponse(float freq);

    private:
        void setvolume(unsigned char Pvolume_);

        // Parameters
        bool Pchanged;
        unsigned char Pvolume;
        unsigned char Pband;

        struct FilterParam
        {
            unsigned char Ptype, Pfreq, Pgain, Pq, Pstages; // parameters
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
                ,l{new AnalogFilter(TOPLEVEL::filter::Peak2, 1000.0, 1.0, 0, &synth)}
                ,r{new AnalogFilter(TOPLEVEL::filter::Peak2, 1000.0, 1.0, 0, &synth)}
            { }
        };

        FilterParam filter[MAX_EQ_BANDS];
};

class EQlimit
{
    public:
        float getlimits(CommandBlock *getData);
};

#endif


