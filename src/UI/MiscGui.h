/*
    MiscGui.h - common link between GUI and synth

    Copyright 2016-2023 Will Godfrey & others

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

*/

#ifndef MISCGUI_H
#define MISCGUI_H

#include <string>

#include "Misc/SynthEngine.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/FormatFuncs.h"

using file::saveText;
using file::loadText;
using func::string2int;

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
    VC_LFOfreqBPM,
    VC_LFOdepthFreq,
    VC_LFOdepthAmp,
    VC_LFOdepthFilter,
    VC_LFOdelay,
    VC_LFOstartphase,
    VC_LFOstartphaseRand,
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
    VC_PartHumaniseDetune,
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
    VC_FXlfofreqBPM,
    VC_FXdefaultDW,
    VC_FXEQfreq,
    VC_FXEQq,
    VC_FXEQgain,
    VC_FXEQfilterGain,
    VC_FXDistVol,
    VC_FXDistLevel,
    VC_FXDistLowPass,
    VC_FXDistHighPass,
    VC_XFadeUpdate,
    VC_Retrigger,
    VC_RandWalkSpread,
};

float collect_readData(SynthEngine *synth, float value, unsigned char control, unsigned char part, unsigned char kititem = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char offset = 0xff, unsigned char miscmsg = 0xff, unsigned char request = 0xff);

void collect_data(SynthEngine *synth, float value, unsigned char action, unsigned char type, unsigned char control, unsigned char part, unsigned char kititem = 0xff, unsigned char engine = 0xff, unsigned char insert = 0xff, unsigned char parameter = 0xff, unsigned char offset = 0xff, unsigned char miscmsg = 0xff);

void alert(SynthEngine *synth, string message);
int choice(SynthEngine *synth, string one, string two, string three, string message);
string setfiler(SynthEngine *synth, string title, string name, bool save, int extension);
string input_text(SynthEngine *synth, string label, string text);

string convert_value(ValueType type, float val);

string variable_prec_units(float v, const string& u, int maxPrec, bool roundup = false);
string custom_value_units(float v, const string& u, int prec=0);
void  custom_graph_dimensions(ValueType vt, int& w, int& h);
void custom_graphics(ValueType vt, float val,int W,int H);
ValueType getLFOdepthType(int group);
ValueType getLFOFreqType(int bpmEnabled);
ValueType getFilterFreqType(int type);
ValueType getFilterFreqTrackType(int offset);

int millisec2logDial(unsigned int);
unsigned int logDial2millisec(int);

class GuiUpdates {

public:
    void read_updates(SynthEngine *synth);

private:
    void decode_envelope(SynthEngine *synth, CommandBlock *getData);
    void decode_updates(SynthEngine *synth, CommandBlock *getData);
};

inline void saveWin(SynthEngine *synth, int w, int h, int x, int y, int o, std::string filename)
{
    std::string ID = std::to_string(synth->getUniqueId()) + "-";
    std::string values =  std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(w) + " " + std::to_string(h) + " " + std::to_string(o);
    //std::cout << values << std::endl;
    saveText(values, file::configDir() + "/windows/" + ID + filename);
}

inline void loadWin(SynthEngine *synth, int& w, int& h, int& x, int& y, int& o, std::string filename)
{
    std::string ID = std::to_string(synth->getUniqueId()) + "-";
    std::string values = loadText(file::configDir() + "/windows/" + ID + filename);
    //std::cout << file::configDir() << "/windows/" << ID << filename << std::endl;
    w = h = o = 0;
    if (values == "")
    {
        x = y = 80;
    }
    else
    {
        size_t pos = values.find(' ');
        if (pos == string::npos)
        {
            x = y = 80;
        }
        else
        {
            x = string2int(values.substr(0, pos));
            if (x < 4)
                x = 4;

            y = string2int(values.substr(pos));
            if (y < 10)
                y = 10;

            pos = values.find(' ', pos + 1);
            if (pos == string::npos)
                return;
            w = string2int(values.substr(pos));

            pos = values.find(' ', pos + 1);
            if (pos == string::npos)
                return;
            h = string2int(values.substr(pos));

            pos = values.find(' ', pos + 1);
            if (pos == string::npos)
                return;
            o = string2int(values.substr(pos));
            //std::cout << "x " << x << "   y " << y  << "   w " << w << "   h " << h <<  "   o " << o << "  " << filename << std::endl;
        }
    }
}

inline int lastSeen(SynthEngine *synth, std::string filename)
{
    std::string ID = std::to_string(synth->getUniqueId()) + "-";
    std::string values = loadText(file::configDir() + "/windows/" + ID + filename);
    //std::cout << values << " " << filename << std::endl;
    size_t pos = values.rfind(' ');
    if (pos == string::npos)
        return false;
    ++pos;
    return string2int(values.substr(pos));
}

inline void setVisible(SynthEngine *synth, bool v, std::string filename)
{
    std::string ID = std::to_string(synth->getUniqueId()) + "-";
    std::string values = loadText(file::configDir() + "/windows/" + ID + filename);
    size_t pos = values.rfind(' ');
    if (pos == string::npos)
        return;
    ++ pos;
    bool vis = string2int(values.substr(pos));
    if (vis == v)
        return;
    values.replace(pos, 1, std::to_string(v));
    //std::cout << v << " " << values << " " << filename << std::endl;
    saveText(values, file::configDir() + "/windows/" + ID + filename);
}

inline void checkSane(int& x, int& y, int& w, int& h, int defW, int defH, bool halfsize = false)
{
    int maxW = Fl::w() - 5; // wiggle room
    int maxH = Fl::h() - 30; // space for minimal titlebar

    if ((w / defW) != (h / defH)) // ratio
        w = h / defH * defW; // doesn't matter which one we pick!

    int adjustW;
    int adjustH;
    if(halfsize)
    {
        adjustW = maxW / 2;
        adjustH = maxH / 2;
    }
    else
    {
        adjustW = maxW;
        adjustH = maxH;
    }
    if (w > maxW || h > maxH) // size
    {
        h = adjustH;
        w = adjustW;
        if (h / defH > w / defW)
        {
            h = w / defW * defH;
        }
        else
        {
            w = h / defH * defW;
        }
    }

    if ((x + w) > maxW) // position
    {
        x = maxW - w;
        if (x < 5)
            x = 5;
    }
    if ((y + h) > maxH)
    {
        y = maxH - h;
        if (y < 30)
            y = 30;
    }
    //std::cout << "x " << x << "  y " << y << "  w " << w << "  h " << h << std::endl;
}
const int gen_text = 64;
const int graph_back = 57;
const int graph_grid = 217;
const int graph_Harmonics_grid = 216;
const int graph_line = 63;
const int EQ_back = 96;
const int EQ_back_off = 97;
const int EQ_line =  95;
const int EQ_line_off =  94;
const int yoshi_ins_typ = 219;
const int instr_back = 17;
const int add_back = 214;
const int sub_back = 236;
const int pad_back = 158;
const int pad_prof_fill = 231;
const int ext_voice = 157;
const int ext_osc = 238;

const int COLOURLIST = 17;
const unsigned char colourNumbers [COLOURLIST] = {
    gen_text,
    graph_back,
    graph_grid,
    graph_Harmonics_grid,
    graph_line,
    EQ_back,
    EQ_back_off,
    EQ_line,
    EQ_line_off,
    yoshi_ins_typ,
    instr_back,
    add_back,
    sub_back,
    pad_back,
    pad_prof_fill,
    ext_voice,
    ext_osc,
};

static std::string colourData [] = {
    "0,0,0, General text",
    "0,0,0, Common graph background",
    "40,120,190, Common graph grid",
    "30,70,255, Grid for harmonics graph",
    "0,255,0, Common graph line",
    "0,70,150, EQ graph background",
    "80,120,160, EQ background disabled",
    "255,255,0, EQ graph line",
    "200,200,80, EQ graph line disabled",
    "0,0,225, Yoshimi instrument type",
    "253,246,230, Instrument background",
    "223,175,191, AddSynth background",
    "175,207,223, SubSynth background",
    "205,221,173, PadSynth background",
    "180,210,240, PadSynth profile fill",
    "159,223,143, Ext voice",
    "143,191,223, Ext osc",
    "R,G,B, (no spaces)",
    "Do not edit this. It may be overwritten.",
    "Copy as template for other named themes.",
};

#endif
