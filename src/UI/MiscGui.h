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
#include "Misc/MirrorData.h"

#include "UI/Themes.h"

#include "Interface/InterfaceAnchor.h"
using RoutingTag = GuiDataExchange::RoutingTag;

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

float collect_readData(SynthEngine *synth, float value, unsigned char control, unsigned char part, unsigned char kititem = UNUSED, unsigned char engine = UNUSED, unsigned char insert = UNUSED, unsigned char parameter = UNUSED, unsigned char offset = UNUSED, unsigned char miscmsg = UNUSED, unsigned char request = UNUSED);

void collect_writeData(SynthEngine *synth, float value, unsigned char action, unsigned char type, unsigned char control, unsigned char part, unsigned char kititem = UNUSED, unsigned char engine = UNUSED, unsigned char insert = UNUSED, unsigned char parameter = UNUSED, unsigned char offset = UNUSED, unsigned char miscmsg = UNUSED);

void alert(SynthEngine *synth, string message);
int choice(SynthEngine *synth, string one, string two, string three, string message);
string setfiler(SynthEngine *synth, string title, string name, bool save, int extension);
string input_text(SynthEngine *synth, string label, string text);

int setSlider(float current, float normal);
int setKnob(float current, float normal);

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


/**
 * Base class mixed into MasterUI, which is the root of the Yoshimi FLTK UI.
 * Provides functions to establish communication with the Core.
 */
class GuiUpdates
{
protected:
    GuiUpdates(InterChange&, InterfaceAnchor&&);

    // must not be copied nor moved
    GuiUpdates(GuiUpdates &&)                =delete;
    GuiUpdates(GuiUpdates const&)            =delete;
    GuiUpdates& operator=(GuiUpdates &&)     =delete;
    GuiUpdates& operator=(GuiUpdates const&) =delete;

    void read_updates(SynthEngine *synth);

public:
    InterChange& interChange;
    InterfaceAnchor anchor;

    auto connectSysEffect() { return GuiDataExchange::Connection<EffectDTO>(interChange.guiDataExchange, anchor.sysEffectParam); }
    auto connectInsEffect() { return GuiDataExchange::Connection<EffectDTO>(interChange.guiDataExchange, anchor.insEffectParam); }
    auto connectPartEffect(){ return GuiDataExchange::Connection<EffectDTO>(interChange.guiDataExchange, anchor.partEffectParam);}

private:
    void decode_envelope(SynthEngine *synth, CommandBlock *getData);
    void decode_updates(SynthEngine *synth, CommandBlock *getData);
};

inline void saveWin(SynthEngine *synth, int w, int h, int x, int y, int o, std::string filename)
{
    std::string ID = std::to_string(synth->getUniqueId()) + "-";
    std::string values =  std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(w) + " " + std::to_string(h) + " " + std::to_string(o);
    saveText(values, file::configDir() + "/windows/" + ID + filename);
}

inline void loadWin(SynthEngine *synth, int& w, int& h, int& x, int& y, int& o, std::string filename)
{
    std::string ID = std::to_string(synth->getUniqueId()) + "-";
    std::string values = loadText(file::configDir() + "/windows/" + ID + filename);
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
        }
    }
}

inline int lastSeen(SynthEngine *synth, std::string filename)
{
    std::string ID = std::to_string(synth->getUniqueId()) + "-";
    std::string values = loadText(file::configDir() + "/windows/" + ID + filename);
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
    saveText(values, file::configDir() + "/windows/" + ID + filename);
}

inline void checkSane(int& x, int& y, int& w, int& h, int defW, int defH, bool halfsize = false, int priority = 0)
{
    if (!priority) return;
    int minX, minY, maxW, maxH;
    Fl::screen_xywh(minX, minY, maxW, maxH, x, y, w, h);

    // Pretend that this is the center screen, which makes calculations easier.
    // We will reverse this at the bottom.
    //x -= minX;
    //y -= minY;

    maxW -= 5; // wiggle room
    maxH -= 30; // space for minimal titlebar
    //std::cout <<"  dw " << defW << "  dh " << defH << std::endl;
    //std::cout << "pri " << priority <<"  w " << w << "  h " << h << std::endl;

    //std::cout << "difference " << abs((w/defW) - (h/defH)) << std::endl;

    float dx = x;
    float dy = y;
    float dh = h;
    float dw = w;
    // Only needed when manually handling sizes
    {
        if (priority == 1)
            dh = (dw*defH) / defW;
        else if (priority == 2)
            dw = (dh*defW) / defH;
    }
    //std::cout <<"  dw " << dw << "  dh " << dh << std::endl;

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
    if (dw > maxW || dh > maxH) // size
    {
        dh = adjustH;
        dw = adjustW;
        if (dh / defH > dw / defW)
        {
            dh = dw / defW * defH;
        }
        else
        {
            dw = dh / defH * defW;
        }
    }

    if ((dx + dw) > maxW) // position
    {
        dx = maxW - dw;
        if (dx < 5)
            dx = 5;
    }
    if ((dy + dh) > maxH)
    {
        dy = maxH - dh;
        if (dy < 30)
            dy = 30;
    }

    // Restore position relative to screen position.
    //dx += minX;
    //dy += minY;

    //std::cout << "X " << x << "  " << dx << std::endl;
    //std::cout << "Y " << y << "  " << dy << std::endl;
    //std::cout << "W " << w << "  " << dw << std::endl;
    //std::cout << "H " << h << "  " << dh << std::endl;
    x = int(dx + 0.4);
    y = int(dy + 0.4);
    w = int(dw + 0.4);
    h = int(dh + 0.4);

}

inline void voiceOscUpdate(SynthEngine *synth_, int npart, int kititem, int nvoice, int &nvs, int &nvp)
{
        SynthEngine *synth = synth_;
        int extOsc= collect_readData(synth,0,ADDVOICE::control::voiceOscillatorSource, npart, kititem, PART::engine::addVoice1 + nvoice);
        if (collect_readData(synth,0,ADDVOICE::control::externalOscillator, npart, kititem, PART::engine::addVoice1 + nvoice) >= 0)
        {
            while (collect_readData(synth,0,ADDVOICE::control::externalOscillator, npart, kititem, PART::engine::addVoice1 + nvs) >= 0)
                nvp = nvs = collect_readData(synth,0,ADDVOICE::control::externalOscillator, npart, kititem, PART::engine::addVoice1 + nvs);
        }
        else if (extOsc >= 0)
            nvs = extOsc;

        return;

        // the original code

        /*if (pars->VoicePar[nvoice].PVoice  >= 0)
        {
            while (pars->VoicePar[nvs].PVoice  >= 0)
                nvp = nvs = pars->VoicePar[nvs].PVoice;
        }
        else if (pars->VoicePar[nvoice].Pextoscil  >= 0)
            nvs = pars->VoicePar[nvoice].Pextoscil;
        oscil->changeParams(pars->VoicePar[nvs].POscil);
        osc->init(oscil,0,pars->VoicePar[nvp].Poscilphase, synth);
        */
}

#endif /*MISCGUI_H*/
