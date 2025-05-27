/*
    Controller.cpp - (Midi) Controllers implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2018, Will Godfrey

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

    Modified October 2018
*/

#include <cmath>
#include <iostream>

#include "Misc/XMLStore.h"
#include "Misc/SynthEngine.h"
#include "Misc/SynthHelper.h"
#include "Params/Controller.h"

using func::power;
using func::powFrac;


Controller::Controller(SynthEngine *_synth):
    synth(_synth)
{
    defaults();
    resetall();
}


void Controller::defaults()
{
    setpitchwheelbendrange(200); // 2 halftones
    expression.receive = 1;
    panning.depth = 64;
    filtercutoff.depth = 64;
    filterq.depth = 64;
    bandwidth.depth = 64;
    bandwidth.exponential = 0;
    modwheel.depth = 80;
    modwheel.exponential = 0;
    fmamp.receive = 1;
    volume.receive = 1;
    volume.data = 96;
    volume.volume = 96.0f/127.0f;
    sustain.receive = 1;
    portamentosetup();
    resonancecenter.depth = 64;
    resonancebandwidth.depth = 64;

    initportamento(440.0f, 440.0f, false);
    setportamento(0);
}


void Controller::resetall()
{
    setpitchwheelbendrange(200); // 2 halftones
    setpitchwheel(0); // center
    expression.receive = 1;
    setexpression(127);
    setPanDepth(64);
    filtercutoff.depth = 64;
    setfiltercutoff(64);
    filterq.depth = 64;
    setfilterq(64);
    bandwidth.depth = 64;
    bandwidth.exponential = 0;
    setbandwidth(64);
    modwheel.depth = 80;
    modwheel.exponential = 0;
    setmodwheel(64);
    fmamp.receive = 1;
    setfmamp(127);
    volume.receive = 1;
    volume.data = 96;
    volume.volume = 96.0f/127.0f;
    setvolume(96);
    sustain.receive = 1;
    setsustain(0);
    portamentosetup();
    initportamento(440.0f, 440.0f, false);
    setportamento(0);
    resonancecenter.depth = 64;
    setresonancecenter(64);
    resonancebandwidth.depth = 64;
    setresonancebw(64);
}


void  Controller::portamentosetup()
{
    portamento.portamento = 0;
    portamento.used = 0;
    portamento.proportional = 0;
    portamento.propRate     = 80;
    portamento.propDepth    = 90;
    portamento.receive = 1;
    portamento.time = 64;
    portamento.updowntimestretch = 64;
    portamento.pitchthresh = 3;
    portamento.pitchthreshtype = 1;
    portamento.noteusing = -1;
}


void Controller::setpitchwheel(int value)
{
    pitchwheel.data = value;
    float cents = value / 8192.0f;
    cents *= pitchwheel.bendrange;
    pitchwheel.relfreq = power<2>(cents / 1200.0f);
    // original comment
    //fprintf(stderr,"%ld %ld -> %.3f\n",pitchwheel.bendrange,pitchwheel.data,pitchwheel.relfreq);fflush(stderr);
}


void Controller::setpitchwheelbendrange(ushort value)
{
    pitchwheel.bendrange = value;
}


void Controller::setexpression(int value)
{
    expression.data = value;
    if (expression.receive && value >= 0 && value < 128)
         expression.relvolume = value / 127.0f;
    else
        expression.relvolume = 1.0f;
}


void Controller::setfiltercutoff(int value)
{
    filtercutoff.data = value;
    filtercutoff.relfreq = (value - 64.0f) * filtercutoff.depth / 4096.0f
                           * 3.321928f; // 3.3219.. = ln2(10)
}


void Controller::setfilterq(int value)
{
    filterq.data = value;
    filterq.relq = power<30>((value - 64.0f) / 64.0f * (filterq.depth / 64.0f));
}


void Controller::setbandwidth(int value)
{
    bandwidth.data = value;
    if (!bandwidth.exponential)
    {
        float tmp = power<25>(powf(bandwidth.depth / 127.0f, 1.5f)) - 1.0f;
        if (value < 64 && bandwidth.depth >= 64)
            tmp = 1.0f;
        bandwidth.relbw = (value / 64.0f - 1.0f) * tmp + 1.0f;
        if (bandwidth.relbw < 0.01f)
            bandwidth.relbw = 0.01f;
    }
    else
    {
        bandwidth.relbw = power<25>((value - 64.0f) / 64.0f * (bandwidth.depth / 64.0f));
    }
}


void Controller::setmodwheel(int value)
{
    modwheel.data = value;
    if (!modwheel.exponential)
    {
        float tmp = power<25>(powf(modwheel.depth / 127.0f, 1.5f) * 2.0f) / 25.0f;
        if (value < 64 && modwheel.depth >= 64)
            tmp = 1.0f;
        modwheel.relmod = (value / 64.0f - 1.0f) * tmp + 1.0f;
        if (modwheel.relmod < 0.0f)
            modwheel.relmod = 0.0f;
    }
    else
        modwheel.relmod = power<25>((value - 64.0f) / 64.0f * (modwheel.depth / 80.0f));
}


void Controller::setfmamp(int value)
{
    fmamp.data = value;
    fmamp.relamp = value / 127.0f;
    if (fmamp.receive)
        fmamp.relamp = value / 127.0f;
    else
        fmamp.relamp = 1.0f;
}


void Controller::setvolume(int value) // range is 64 to 127
{
    if (value < 64)
        value = 96; // set invalid to default
    volume.data = value;
    volume.volume = value / 127.0f;
}


void Controller::setsustain(int value)
{
    sustain.data = value;
    if (sustain.receive)
        sustain.sustain = (value < 64) ? 0 : 1;
    else
        sustain.sustain = 0;
}


void Controller::setportamento(int value)
{
    portamento.data = value;
    if (portamento.receive)
        portamento.portamento = (value < 64) ? 0 : 1;
}


// returns true if portamento's preconditions are met
bool Controller::initportamento(float oldfreq, float newfreq, bool in_progress)
{
    portamento.x = 0.0f;

    if (in_progress)
    {   // Legato in progress
        if (!portamento.portamento)
            return false;
    }
    else
    {   // No legato, do the original if...return
        if (portamento.used != 0 || !portamento.portamento)
            return false;
    }

    float portamentotime = power<100>(portamento.time / 127.0f) / 50.0f; // portamento time in seconds

    if (portamento.proportional)
    {
        //If there is a min(float,float) and a max(float,float) then they
        //could be used here
        //Linear functors could also make this nicer
        if (oldfreq > newfreq) //2 is the center of propRate
            portamentotime *=
                powf(oldfreq / newfreq
                     / (portamento.propRate / 127.0f * 3 + .05),
                     (portamento.propDepth / 127.0f * 1.6f + .2));
        else                  //1 is the center of propDepth
            portamentotime *=
                powf(newfreq / oldfreq
                     / (portamento.propRate / 127.0f * 3 + .05),
                     (portamento.propDepth / 127.0f * 1.6f + .2));
    }

    if (portamento.updowntimestretch >= 64 && newfreq < oldfreq)
    {
        if (portamento.updowntimestretch == 127)
            return false;
        portamentotime *= powFrac<10>((portamento.updowntimestretch - 64) / 63.0f);
    }
    if (portamento.updowntimestretch < 64 && newfreq > oldfreq)
    {
        if (portamento.updowntimestretch == 0)
            return false;
        portamentotime *= powFrac<10>((64.0f - portamento.updowntimestretch) / 64.0f);
    }

    portamento.dx = synth->fixed_sample_step_f / portamentotime;
    portamento.origfreqrap = oldfreq / newfreq;

    float tmprap = (portamento.origfreqrap > 1.0f)
                          ? portamento.origfreqrap
                          : 1.0 / portamento.origfreqrap ;

    float thresholdrap = power<2>(portamento.pitchthresh / 12.0f);
    if (portamento.pitchthreshtype == 0 && (tmprap - 0.00001f) > thresholdrap)
        return false;
    if (portamento.pitchthreshtype == 1 && (tmprap + 0.00001f) < thresholdrap)
        return false;

    portamento.used = 1;
    portamento.freqrap = portamento.origfreqrap;
    return true;
}


void Controller::updateportamento()
{
    if (portamento.used)
    {
        portamento.x += portamento.dx;
        if (portamento.x > 1.0f)
        {
            portamento.x = 1.0f;
            portamento.used = 0;
        }
        portamento.freqrap = (1.0f - portamento.x) * portamento.origfreqrap + portamento.x;
    }
}


void Controller::setresonancecenter(int value)
{
    resonancecenter.data = value;
    resonancecenter.relcenter = power<3>((value - 64.0f) / 64.0f * (resonancecenter.depth / 64.0f));
}


namespace {
    static const float LN_BASE1_5 = log(1.5);
    inline float power1_5(float exponent) { return expf(LN_BASE1_5 * exponent); } // 1.5^exponent
}

void Controller::setresonancebw(int value)
{
    resonancebandwidth.data = value;
    resonancebandwidth.relbw = power1_5((value - 64.0f) / 64.0f * (resonancebandwidth.depth / 127.0f));
}


void Controller::add2XML(XMLtree& xml)
{
    xml.addPar_int("pitchwheel_bendrange", pitchwheel.bendrange);

    xml.addPar_bool("expression_receive"    , expression.receive);
    xml.addPar_int ("panning_depth"         , panning.depth);
    xml.addPar_int ("filter_cutoff_depth"   , filtercutoff.depth);
    xml.addPar_int ("filter_q_depth"        , filterq.depth);
    xml.addPar_int ("bandwidth_depth"       , bandwidth.depth);
    xml.addPar_int ("mod_wheel_depth"       , modwheel.depth);
    xml.addPar_bool("mod_wheel_exponential" , modwheel.exponential);
    xml.addPar_bool("fm_amp_receive"        , fmamp.receive);
    xml.addPar_bool("volume_receive"        , volume.receive);
    xml.addPar_int ("volume_range"          , volume.data);
    xml.addPar_bool("sustain_receive"       , sustain.receive);

    xml.addPar_bool("portamento_receive"          , portamento.receive);
    xml.addPar_int ("portamento_time"             , portamento.time);
    xml.addPar_int ("portamento_pitchthresh"      , portamento.pitchthresh);
    xml.addPar_int ("portamento_pitchthreshtype"  , portamento.pitchthreshtype);
    xml.addPar_int ("portamento_portamento"       , portamento.portamento);
    xml.addPar_int ("portamento_updowntimestretch", portamento.updowntimestretch);
    xml.addPar_int ("portamento_proportional"     , portamento.proportional);
    xml.addPar_int ("portamento_proprate"         , portamento.propRate);
    xml.addPar_int ("portamento_propdepth"        , portamento.propDepth);

    xml.addPar_int ("resonance_center_depth"      , resonancecenter.depth);
    xml.addPar_int ("resonance_bandwidth_depth"   , resonancebandwidth.depth);
}


void Controller::getfromXML(XMLtree& xml)
{
    pitchwheel.bendrange= xml.getPar_int ("pitchwheel_bendrange",pitchwheel.bendrange,-6400,6400);

    expression.receive  = xml.getPar_bool("expression_receive"   ,expression.receive);
    panning.depth       = xml.getPar_127 ("panning_depth"        ,panning.depth);
    filtercutoff.depth  = xml.getPar_127 ("filter_cutoff_depth"  ,filtercutoff.depth);
    filterq.depth       = xml.getPar_127 ("filter_q_depth"       ,filterq.depth);
    bandwidth.depth     = xml.getPar_127 ("bandwidth_depth"      ,bandwidth.depth);
    modwheel.depth      = xml.getPar_127 ("mod_wheel_depth"      ,modwheel.depth);
    modwheel.exponential= xml.getPar_bool("mod_wheel_exponential",modwheel.exponential);
    fmamp.receive       = xml.getPar_bool("fm_amp_receive"       ,fmamp.receive);
    volume.receive      = xml.getPar_bool("volume_receive"       ,volume.receive);
    setvolume(xml.getPar_127("volume_range",volume.data));

    sustain.receive     = xml.getPar_bool("sustain_receive",sustain.receive);

    portamento.receive          = xml.getPar_bool("portamento_receive"         ,portamento.receive);
    portamento.time             = xml.getPar_127("portamento_time"             ,portamento.time);
    portamento.pitchthresh      = xml.getPar_127("portamento_pitchthresh"      ,portamento.pitchthresh);
    portamento.pitchthreshtype  = xml.getPar_127("portamento_pitchthreshtype"  ,portamento.pitchthreshtype);
    portamento.portamento       = xml.getPar_127("portamento_portamento"       ,portamento.portamento);
    portamento.updowntimestretch= xml.getPar_127("portamento_updowntimestretch",portamento.updowntimestretch);
    portamento.proportional     = xml.getPar_127("portamento_proportional"     ,portamento.proportional);
    portamento.propRate         = xml.getPar_127("portamento_proprate"         ,portamento.propRate);
    portamento.propDepth        = xml.getPar_127("portamento_propdepth"        ,portamento.propDepth);

    resonancecenter.depth    = xml.getPar_127("resonance_center_depth"   ,resonancecenter.depth);
    resonancebandwidth.depth = xml.getPar_127("resonance_bandwidth_depth",resonancebandwidth.depth);
}


float Controller::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    unsigned char type = getData->data.type;
    int request = type & TOPLEVEL::type::Default;
    int control = getData->data.control;

    // controller defaults
    int min = 0;
    float def = 64;
    int max = 127;
    type |= TOPLEVEL::type::Integer;
    unsigned char learnable = TOPLEVEL::type::Learnable;

    switch (control)
    {
        case PART::control::volumeRange:
            min = 64;
            def = 96;
            break;
        case PART::control::volumeEnable:
            def = 1;
            max = 1;
            break;
        case PART::control::panningWidth:
            type |= learnable;
            max = 64;
            break;
        case PART::control::modWheelDepth:
            def = 80;
            break;
        case PART::control::exponentialModWheel:
            def = 0;
            max = 1;
            break;
        case PART::control::bandwidthDepth:
            type |= learnable;
            break;
        case PART::control::exponentialBandwidth:
            def = 0;
            max = 1;
            break;
        case PART::control::expressionEnable:
            def = 1;
            max = 1;
            break;
        case PART::control::FMamplitudeEnable:
            def = 1;
            max = 1;
            break;
        case PART::control::sustainPedalEnable:
            def = 1;
            max = 1;
            break;
        case PART::control::pitchWheelRange:
            type |= learnable;
            min = -6400;
            def = 200;
            max = 6400;
            break;
        case PART::control::filterQdepth:
            break;
        case PART::control::filterCutoffDepth:
            break;
        case PART::control::breathControlEnable:
            max = 1;
            def = 1;
            break;
        case PART::control::resonanceCenterFrequencyDepth:
            break;
        case PART::control::resonanceBandwidthDepth:
            break;
        case PART::control::portamentoTime:
            type |= learnable;
            min = 0;
            break;
        case PART::control::portamentoTimeStretch:
            type |= learnable;
            break;
        case PART::control::portamentoThreshold:
            type |= learnable;
            def = 3;
            break;
        case PART::control::portamentoThresholdType:
            min = 0;
            max = 1;
            def = 1;
            break;
        case PART::control::enableProportionalPortamento:
            def = 0;
            max = 1;
            break;
        case PART::control::proportionalPortamentoRate:
            type |= learnable;
            def = 80;
            break;
        case PART::control::proportionalPortamentoDepth:
            type |= learnable;
            def = 90;
            break;
        case PART::control::receivePortamento:
            def = 1;
            max = 1;
            break;
        case PART::control::resetAllControllers:
            def = 0;
            max = 0;
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
