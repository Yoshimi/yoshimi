/*
    MiscGui.h - common link between GUI and synth

    Copyright 2016-2019 Will Godfrey & others

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

#include <string>

#include "Misc/SynthEngine.h"
#include "Misc/FileMgrFuncs.h"

using file::saveText;
using file::loadText;

enum ValueType {
    VC_plainValue,
    VC_plainReverse,
    VC_pitchWheel,
    VC_percent127,
    VC_percent128,
    VC_percent255,
    VC_percent64_127,
    VC_PhaseOffset,
    VC_WaveHarmonicMagnitude,
    VC_GlobalFineDetune,
    VC_MasterVolume,
    VC_LFOfreq,
    VC_LFOdepthFreq,
    VC_LFOdepthAmp,
    VC_LFOdepthFilter,
    VC_LFOdelay,
    VC_LFOstartphase,
    VC_EnvelopeDT,
    VC_EnvelopeFreqVal,
    VC_EnvelopeFilterVal,
    VC_EnvelopeAmpSusVal,
    VC_EnvelopeLinAmpSusVal,
    VC_EnvelopeBandwidthVal,
    VC_FilterFreq0, // Analog
    VC_FilterFreq1, // Formant
    VC_FilterFreq2, // StateVar
    VC_FilterFreqTrack0,
    VC_FilterFreqTrack1,
    VC_FilterQ,
    VC_FilterVelocityAmp,
    VC_FilterVelocitySense,
    VC_FormFilterClearness,
    VC_FormFilterSlowness,
    VC_FormFilterStretch,
    VC_InstrumentVolume,
    VC_ADDVoiceVolume,
    VC_ADDVoiceDelay,
    VC_PitchBend,
    VC_PartVolume,
    VC_PartHumaniseVelocity,
    VC_PanningRandom,
    VC_PanningStd,
    VC_EnvStretch,
    VC_LFOStretch,
    VC_FreqOffsetHz,
    VC_FixedFreqET,
    VC_FilterGain,
    VC_AmpVelocitySense,
    VC_FilterQAnalogUnused,
    VC_BandWidth,
    VC_SubBandwidth,
    VC_SubBandwidthScale,
    VC_SubBandwidthRel,
    VC_SubHarmonicMagnitude,
    VC_FXSysSend,
    VC_FXEchoVol,
    VC_FXEchoDelay,
    VC_FXEchoLRdel,
    VC_FXEchoDW,
    VC_FXReverbVol,
    VC_FXReverbTime,
    VC_FXReverbIDelay,
    VC_FXReverbHighPass,
    VC_FXReverbLowPass,
    VC_FXReverbDW,
    VC_FXReverbBandwidth,
    VC_FXdefaultVol,
    VC_FXdefaultFb,
    VC_FXChorusDepth,
    VC_FXChorusDelay,
    VC_FXlfoStereo,
    VC_FXlfofreq,
    VC_FXdefaultDW,
    VC_FXEQfreq,
    VC_FXEQq,
    VC_FXEQgain,
    VC_FXEQfilterGain,
    VC_FXDistVol,
    VC_FXDistLevel,
    VC_FXDistLowPass,
    VC_FXDistHighPass
};

float collect_readData(SynthEngine *synth, float value, unsigned char control, unsigned char part, unsigned char kititem = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char offset = 0xff, unsigned char miscmsg = 0xff, unsigned char request = 0xff);

void collect_data(SynthEngine *synth, float value, unsigned char action, unsigned char type, unsigned char control, unsigned char part, unsigned char kititem = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char offset = 0xff, unsigned char miscmsg = 0xff);

string convert_value(ValueType type, float val);

string variable_prec_units(float v, const string& u, int maxPrec, bool roundup = false);
string custom_value_units(float v, const string& u, int prec=0);
void  custom_graph_dimensions(ValueType vt, int& w, int& h);
void custom_graphics(ValueType vt, float val,int W,int H);
ValueType getLFOdepthType(int group);
ValueType getFilterFreqType(int type);
ValueType getFilterFreqTrackType(int offset);

class GuiUpdates {

public:
    void read_updates(SynthEngine *synth);
private:
    void decode_envelope(SynthEngine *synth, CommandBlock *getData);
    void decode_updates(SynthEngine *synth, CommandBlock *getData);
};

inline void saveWin(SynthEngine *synth, int x, int y, int o, std::string filename)
{
    std::string ID = std::to_string(synth->getUniqueId()) + "-";
    std::string values =  std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(o);
    //std::cout << values << std::endl;
    saveText(values, synth->getRuntime().ConfigDir + "/windows/" + ID + filename);
}

inline void loadWin(SynthEngine *synth, int& x, int& y, int& o, std::string filename)
{
    std::string ID = std::to_string(synth->getUniqueId()) + "-";;
    std::string values = loadText(synth->getRuntime().ConfigDir + "/windows/" + ID + filename);
    //std::cout << synth->getRuntime().ConfigDir << "/windows/" << ID << filename << std::endl;
    if (values == "")
    {
        x = y = 80;
        o = 0;
    }
    else
    {
        size_t pos = values.find(' ');
        if (pos == string::npos)
            x = y = 80;
        else
        {
            x = stoi(values.substr(0, pos));
            if (x < 4)
                x = 4;
            y = stoi(values.substr(pos));
            if (y < 10)
                y = 10;
            o = stoi(values.substr(values.rfind(' ')));
            //std::cout << "x " << x << "   y " << y <<  "   o " << o << std::endl;
        }
    }
}


#endif
