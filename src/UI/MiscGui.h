/*
    MiscGui.h - common link between GUI and synth

    Copyright 2016 Will Godfrey

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

*/

#ifndef MISCGUI_H
#define MISCGUI_H

#include "Misc/SynthEngine.h"

enum ValueType {
    VC_plainValue,
    VC_percent127,
    VC_percent255,
    VC_GlobalFineDetune,
    VC_MasterVolume,
    VC_LFOfreq,
    VC_LFOdepth0,  // freq   LFO
    VC_LFOdepth1,  // ampl   LFO
    VC_LFOdepth2,  // filter LFO
    VC_LFOdelay,
    VC_LFOstartphase,
    VC_EnvelopeDT,
    VC_EnvelopeFreqVal,
    VC_EnvelopeFilterVal,
    VC_EnvelopeAmpSusVal,
    VC_FilterFreq0,
    VC_FilterFreq1,
    VC_FilterFreq2,
    VC_FilterFreqTrack0,
    VC_FilterFreqTrack1,
    VC_FilterQ,
    VC_InstrumentVolume,
    VC_ADDVoiceVolume,
    VC_PartVolume,
    VC_PanningRandom,
    VC_PanningStd,
    VC_EnvStretch,
    VC_LFOStretch,
    VC_FreqOffsetHz,
    VC_FilterGain,
    VC_AmpVelocitySense,
    VC_FilterQAnalogUnused,
    VC_BandWidth
};

void collect_data(SynthEngine *synth, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kititem = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char par2 = 0xff);

void read_updates(SynthEngine *synth);
void decode_updates(SynthEngine *synth, CommandBlock *getData);

string convert_value(ValueType type, float val);

string custom_value_units(float v, string u, int prec=0);
ValueType getLFOdepthType(LFOParams *pars);
ValueType getFilterFreqType(FilterParams *pars);
ValueType getFilterFreqTrackType(FilterParams *pars);

#endif
