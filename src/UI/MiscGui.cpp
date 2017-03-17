/*
    MiscGui.cpp - common link between GUI and synth

    Copyright 2016-2017 Will Godfrey & others

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

    Modified March 2017
*/

#include "Misc/SynthEngine.h"
#include "MiscGui.h"
#include "MasterUI.h"

#include <iostream>

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <cairo.h>
#include <cairo-xlib.h>

SynthEngine *synth;

void collect_data(SynthEngine *synth, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kititem, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2)
{
    int typetop = type & 0xc0;
    if ( part == 0xf1 && insert == 16)
        type |= 8; // this is a hack :(

    if (part != 0xd8)
    {

        if ((type & 3) == 3)
        { // value type is now irrelevant
            if(Fl::event_state(FL_CTRL) != 0)
            {
                if (type & 8)
                    type = 3;
                // identifying this for button 3 as MIDI learn
                else
                {
                    synth->getGuiMaster()->midilearnui->words->copy_label("Can't learn this control");
                    synth->getGuiMaster()->midilearnui->message->show();
                    synth->getGuiMaster()->midilearnui->message->position(Fl::event_x_root() + 16, Fl::event_y_root());
                    synth->getRuntime().Log("Can't MIDI-learn this control");
                    /* can't use fl_alert here.
                     * For some reason it goes into a loop on spin boxes
                     * and runs menus up to their max value.
                     */
                    return;
                }
            }
            else
            {
                //type = 0;
                type = 0x40;
                // identifying this for button 3 as set default
            }
        }
        else if((type & 7) > 2)
            type = 1 | typetop;
            // change scroll wheel to button 1

    }
    type |= (typetop & 0x80);

    CommandBlock putData;
    size_t commandSize = sizeof(putData);
    putData.data.value = value;
    putData.data.type = type | 0x20; // 0x20 = from GUI
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kititem;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;

    if (jack_ringbuffer_write_space(synth->interchange.fromGUI) >= commandSize)
        jack_ringbuffer_write(synth->interchange.fromGUI, (char*) putData.bytes, commandSize);
}


void read_updates(SynthEngine *synth)
{
    CommandBlock getData;
    size_t commandSize = sizeof(getData);

    while(jack_ringbuffer_read_space(synth->interchange.toGUI) >= commandSize)
    {
        int toread = commandSize;
        char *point = (char*) &getData.bytes;
        jack_ringbuffer_read(synth->interchange.toGUI, point, toread);
        decode_updates(synth, &getData);
    }
}


void decode_updates(SynthEngine *synth, CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char insertParam = getData->data.parameter;
    //unsigned char insertPar2 = getData->data.par2;

    if (npart >= 0xc0 && npart < 0xd0) // vector
    {
        return; // todo
    }
    if (npart == 0xd8)
    {
        synth->getGuiMaster()->midilearnui->returns_update(getData);
        return;
    }

    Part *part;
    part = synth->part[npart];

    if (kititem >= 0x80 && kititem != 0xff) // effects
    {
        if (npart == 0xf1)
            synth->getGuiMaster()->syseffectui->returns_update(getData);
        else if (npart == 0xf2)
            synth->getGuiMaster()->inseffectui->returns_update(getData);
        else if (npart < 0x40)
            synth->getGuiMaster()->partui->inseffectui->returns_update(getData);
        return;
    }

    if (npart >= 0xf0) // main / sys / ins
    {
        if (npart == 0xf0 && (control == 80 || control == 96))
            return; // gui in undefined state at this point
        synth->getGuiMaster()->returns_update(getData);
        return;
    }

    if (npart >= 0x40)
        return; // invalid part number

    if ((kititem & engine & insert) == 0xff && control == 96) // special case for part clear
    {
        synth->getGuiMaster()->returns_update(getData);
        return;
    }

    if (kititem != 0 && engine != 255 && control != 8 && part->kit[kititem & 0x1f].Penabled == false)
        return; // attempt to access non existant kititem

    if (kititem == 0xff || (kititem & 0x20)) // part
    {
        if (control != 58 && kititem < 0xff && part->Pkitmode == 0)
            return; // invalid access
        synth->getGuiMaster()->partui->returns_update(getData);
        return;
    }

    if (kititem > 0 && kititem < 0xff && part->Pkitmode == 0)
        return; // invalid access

    if (engine == 2) // padsynth
    {
        if(synth->getGuiMaster()->partui->padnoteui)
        {
            switch (insert)
            {
                case 0xff:
                    synth->getGuiMaster()->partui->padnoteui->returns_update(getData);
                    break;
                case 0:
                    switch(insertParam)
                    {
                        case 0:
                            if (synth->getGuiMaster()->partui->padnoteui->amplfo)
                                synth->getGuiMaster()->partui->padnoteui->amplfo->returns_update(getData);
                            break;
                        case 1:
                            if (synth->getGuiMaster()->partui->padnoteui->freqlfo)
                                synth->getGuiMaster()->partui->padnoteui->freqlfo->returns_update(getData);
                            break;
                        case 2:
                            if (synth->getGuiMaster()->partui->padnoteui->filterlfo)
                                synth->getGuiMaster()->partui->padnoteui->filterlfo->returns_update(getData);
                            break;
                    }
                    break;
                case 1:
                    if (synth->getGuiMaster()->partui->padnoteui->filterui)
                        synth->getGuiMaster()->partui->padnoteui->filterui->returns_update(getData);
                    break;
                case 2:
                    switch(insertParam)
                    {
                        case 0:
                            if (synth->getGuiMaster()->partui->padnoteui->ampenv)
                                synth->getGuiMaster()->partui->padnoteui->ampenv->returns_update(getData);
                            break;
                        case 1:
                            if (synth->getGuiMaster()->partui->padnoteui->freqenv)
                                synth->getGuiMaster()->partui->padnoteui->freqenv->returns_update(getData);
                            break;
                        case 2:
                            if (synth->getGuiMaster()->partui->padnoteui->filterenv)
                                synth->getGuiMaster()->partui->padnoteui->filterenv->returns_update(getData);
                            break;
                    }
                    break;

                case 5:
                case 6:
                case 7:
                    if(synth->getGuiMaster()->partui->padnoteui->oscui)
                        synth->getGuiMaster()->partui->padnoteui->oscui->returns_update(getData);
                    break;
                case 8:
                case 9:
                    if(synth->getGuiMaster()->partui->padnoteui->resui)
                        synth->getGuiMaster()->partui->padnoteui->resui->returns_update(getData);
                    break;
            }
        }
        return;
    }

    if (engine == 1) // subsynth
    {
        if (synth->getGuiMaster()->partui->subnoteui)
            switch (insert)
            {
                case 1:
                    if (synth->getGuiMaster()->partui->subnoteui->filterui)
                        synth->getGuiMaster()->partui->subnoteui->filterui->returns_update(getData);
                    break;
                case 2:
                    switch(insertParam)
                    {
                        case 0:
                            if (synth->getGuiMaster()->partui->subnoteui->ampenv)
                                synth->getGuiMaster()->partui->subnoteui->ampenv->returns_update(getData);
                            break;
                        case 1:
                            if (synth->getGuiMaster()->partui->subnoteui->freqenvelopegroup)
                                synth->getGuiMaster()->partui->subnoteui->freqenvelopegroup->returns_update(getData);
                            break;
                        case 2:
                            if (synth->getGuiMaster()->partui->subnoteui->filterenv)
                                synth->getGuiMaster()->partui->subnoteui->filterenv->returns_update(getData);
                            break;
                        case 3:
                            if (synth->getGuiMaster()->partui->subnoteui->bandwidthenvelopegroup)
                                synth->getGuiMaster()->partui->subnoteui->bandwidthenvelopegroup->returns_update(getData);
                            break;
                    }
                    break;
                case 0xff:
                case 6:
                case 7:
                    synth->getGuiMaster()->partui->subnoteui->returns_update(getData);
                    break;
            }
        return;
    }

    if (engine >= 0x80) // addsynth voice / modulator
    {
        if (synth->getGuiMaster()->partui->adnoteui)
        {
            if (synth->getGuiMaster()->partui->adnoteui->advoice)
            {
                switch (insert)
                {
                    case 0xff:
                        synth->getGuiMaster()->partui->adnoteui->advoice->returns_update(getData);
                        break;
                    case 0:
                        switch(insertParam)
                        {
                            case 0:
                                if (synth->getGuiMaster()->partui->adnoteui->advoice->voiceamplfogroup)
                                    synth->getGuiMaster()->partui->adnoteui->advoice->voiceamplfogroup->returns_update(getData);
                                break;
                            case 1:
                                if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqlfogroup)
                                    synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqlfogroup->returns_update(getData);
                                break;
                            case 2:
                                if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterlfogroup)
                                    synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterlfogroup->returns_update(getData);
                                break;
                        }
                        break;
                    case 1:
                        if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefilter)
                            synth->getGuiMaster()->partui->adnoteui->advoice->voicefilter->returns_update(getData);
                        break;
                    case 2:
                        if (engine >= 0xC0)
                            switch(insertParam)
                            {
                                case 0:
                                    if (synth->getGuiMaster()->partui->adnoteui->advoice->voiceFMampenvgroup)
                                        synth->getGuiMaster()->partui->adnoteui->advoice->voiceFMampenvgroup->returns_update(getData);
                                    break;
                                case 1:
                                    if (synth->getGuiMaster()->partui->adnoteui->advoice->voiceFMfreqenvgroup)
                                        synth->getGuiMaster()->partui->adnoteui->advoice->voiceFMfreqenvgroup->returns_update(getData);
                                    break;
                            }
                        else
                        {
                            switch(insertParam)
                            {
                                case 0:
                                    if (synth->getGuiMaster()->partui->adnoteui->advoice->voiceampenvgroup)
                                        synth->getGuiMaster()->partui->adnoteui->advoice->voiceampenvgroup->returns_update(getData);
                                    break;
                                case 1:
                                    if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqenvgroup)
                                        synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqenvgroup->returns_update(getData);
                                    break;
                                case 2:
                                    if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterenvgroup)
                                        synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterenvgroup->returns_update(getData);
                                    break;
                            }
                        break;
                        }
                    case 5:
                    case 6:
                    case 7:
                        if (synth->getGuiMaster()->partui->adnoteui->advoice->oscedit)
                            synth->getGuiMaster()->partui->adnoteui->advoice->oscedit->returns_update(getData);
                        break;
                }
            }
        }
        return;
    }

    if (engine == 0) // addsynth base
    {
        if (synth->getGuiMaster()->partui->adnoteui)
            switch (insert)
            {
                case 0xff:
                    synth->getGuiMaster()->partui->adnoteui->returns_update(getData);
                    break;
                case 0:
                    switch(insertParam)
                    {
                        case 0:
                            if (synth->getGuiMaster()->partui->adnoteui->amplfo)
                                synth->getGuiMaster()->partui->adnoteui->amplfo->returns_update(getData);
                            break;
                        case 1:
                            if (synth->getGuiMaster()->partui->adnoteui->freqlfo)
                                synth->getGuiMaster()->partui->adnoteui->freqlfo->returns_update(getData);
                            break;
                        case 2:
                            if (synth->getGuiMaster()->partui->adnoteui->filterlfo)
                                synth->getGuiMaster()->partui->adnoteui->filterlfo->returns_update(getData);
                            break;
                    }
                    break;
                case 1:
                    if (synth->getGuiMaster()->partui->adnoteui->filterui)
                        synth->getGuiMaster()->partui->adnoteui->filterui->returns_update(getData);
                    break;
                case 2:
                    switch(insertParam)
                    {
                        case 0:
                            if (synth->getGuiMaster()->partui->adnoteui->ampenv)
                                synth->getGuiMaster()->partui->adnoteui->ampenv->returns_update(getData);
                            break;
                        case 1:
                            if (synth->getGuiMaster()->partui->adnoteui->freqenv)
                                synth->getGuiMaster()->partui->adnoteui->freqenv->returns_update(getData);
                            break;
                        case 2:
                            if (synth->getGuiMaster()->partui->adnoteui->filterenv)
                                synth->getGuiMaster()->partui->adnoteui->filterenv->returns_update(getData);
                            break;
                    }
                    break;

                case 8:
                case 9:
                    if (synth->getGuiMaster()->partui->adnoteui->resui)
                        synth->getGuiMaster()->partui->adnoteui->resui->returns_update(getData);
                    break;
            }
        return;
    }
}

string convert_value(ValueType type, float val)
{
    float f;
    int i;
    string s;
    switch(type)
    {
        case VC_percent127:
            return(custom_value_units(val / 127.0f * 100.0f+0.05f,"%",1));

        case VC_percent128:
            return(custom_value_units(val / 128.0f * 100.0f+0.05f,"%",1));

        case VC_percent255:
            return(custom_value_units(val / 255.0f * 100.0f+0.05f,"%",1));

        case VC_percent64_127:
            return(custom_value_units((val-64) / 63.0f * 100.0f+0.05f,"%",1));

        case VC_GlobalFineDetune:
            return(custom_value_units((val-64),"cents",1));

        case VC_MasterVolume:
            return(custom_value_units((val-96.0f)/96.0f*40.0f,"dB",1));

        case VC_LFOfreq:
            f = (powf(2.0f, val * 10.0f) - 1.0f) / 12.0f;
            return variable_prec_units(f, "Hz", 3);

        case VC_LFOdepthFreq: // frequency LFO
            f=powf(2.0f,(int)val/127.0f*11.0f)-1.0f;
            return variable_prec_units(f, "cents", 2);

        case VC_LFOdepthAmp: // amplitude LFO
            return(custom_value_units(val / 127.0f * 200.0f,"%",1));

        case VC_LFOdepthFilter: // filter LFO
            f=(int)val / 127.0f * 4800.0f; // 4 octaves
            return variable_prec_units(f, "cents", 2);

        case VC_LFOdelay:
            f = ((int)val) / 127.0f * 4.0f + 0.005f;
            return(custom_value_units(f,"s",2));

        case VC_LFOstartphase:
            if((int)val == 0)
                return("random");
            else
                return(custom_value_units(((int)val - 64.0f) / 127.0f
                                      * 360.0f, "°"));
        case VC_EnvelopeDT:
            // unfortunately converttofree() is not called in time for us to
            // be able to use env->getdt(), so we have to compute ourselves
            f = (powf(2.0f, ((int)val) / 127.0f * 12.0f) - 1.0f) * 10.0f;
            if(f >= 1000)
                return variable_prec_units(f/1000.0f, "s", 2);
            else
                return variable_prec_units(f, "ms", 2);

        case VC_EnvelopeFreqVal:
            f=(powf(2.0f, 6.0f * fabsf((int)val - 64.0f) / 64.0f) -1.0f) * 100.0f;
            if((int)val<64) f = -f;
            return variable_prec_units(f, "cents", 2);

        case VC_EnvelopeFilterVal:
            f=((int)val - 64.0f) / 64.0f * 7200.0f; // 6 octaves
            return variable_prec_units(f, "cents", 2);

        case VC_EnvelopeAmpSusVal:
            return(custom_value_units((1.0f - (int)val / 127.0f)
                                      * MIN_ENVELOPE_DB, "dB", 1));

        case VC_EnvelopeLinAmpSusVal:
            f = 20.0f * log10f((int)val / 127.0f);
            return variable_prec_units(f, "dB", 2);

        case VC_EnvelopeBandwidthVal:
	    f = powf(2.0f, 10.0f * ((int)val - 64) / 64.0f);
            return variable_prec_units(f, "x", 4);

        case VC_FilterFreq0: // AnalogFilter
            f=powf(2.0f, (val / 64.0f - 1.0f) * 5.0f + 9.96578428f);
            if (f >= 1000.0f)
                return variable_prec_units(f/1000.0f, "kHz", 2);
            else
                return variable_prec_units(f, "Hz", 2);

        case VC_FilterFreq2: // SVFilter
            f=powf(2.0f, (val / 64.0f - 1.0f) * 5.0f + 9.96578428f);
            // We have to adjust the freq because of this line
            // in method SVFilter::computefiltercoefs() (file SVFilter.cpp)
            //
            //   par.f = freq / synth->samplerate_f * 4.0f;
            //
            // Using factor 4.0 instead of the usual 2.0*PI leads to a
            // different effective cut-off freq, which we will be showing
            f *= 4.0 / (2.0 * PI);
            if (f >= 1000.0f)
                return variable_prec_units(f/1000.0f, "kHz", 2);
            else
                return variable_prec_units(f, "Hz", 2);

        case VC_FilterFreq1: // ToDo
            return(custom_value_units(val,""));

        case VC_FilterQ:
        case VC_FilterQAnalogUnused:
            s.clear();
            s += "Q = ";
            f = expf(powf((int)val / 127.0f, 2.0f) * logf(1000.0f)) - 0.9f;
            s += variable_prec_units(f, "", 4, true);
            if (type == VC_FilterQAnalogUnused)
                s += "(This filter does not use Q)";
            return(s);

        case VC_FilterVelocityAmp:
            f = (int)val / 127.0 * -6.0;
            f = powf(2.0f,f + log(1000.0f)/log(2.0f)); // getrealfreq
            f = log(f/1000.0f)/log(powf(2.0f,1.0f/12.0f))*100.0f; // in cents
            return(custom_value_units(f-0.5, "cents"));

        case VC_FilterFreqTrack0:
            s.clear();
            s += "standard range is -100 .. +98%\n";
            f = (val - 64.0f) / 64.0f * 100.0f;
            s += custom_value_units(f, "%", 1);
            return(s);

        case VC_FilterFreqTrack1:
            s.clear();
            s += "0/+ checked: range is 0 .. 198%\n";
            f = val /64.0f * 100.0f;
            s += custom_value_units(f, "%", 1);
            return(s);

        case VC_InstrumentVolume:
            return(custom_value_units(-60.0f*(1.0f-(int)val/96.0f),"dB",1));

        case VC_ADDVoiceVolume:
            return(custom_value_units(-60.0f*(1.0f-lrint(val)/127.0f),"dB",1));

        case VC_ADDVoiceDelay:
            if((int) val == 0)
                return "No delay";
            f = (expf((val/127.0f) * logf(50.0f)) - 1) / 10;
            if(f >= 1)
                return variable_prec_units(f, "s", 2, true);
            else
                return variable_prec_units(f * 1000, "ms", 1);

        case VC_PitchBend:
            if ((int) val == 64)
                return "Off - no pitch bend";
            f = (val - 64) / 24;
            s = string(f > 0 ? "" : "\n(reversed)");
            f = fabsf(f);
                return custom_value_units(f, "x bend range " + s, 2) +
                    "\n(default: +/- " + custom_value_units(200 * f, "cents )");

        case VC_PartVolume:
            return(custom_value_units((val-96.0f)/96.0f*40.0f,"dB",1));

        case VC_PanningRandom:
            i = lrint(val);
            if(i==0)
                return("random");
            else if(i==64)
                return("centered");
            else if(i<64)
                return(custom_value_units((64.0f - i) / 63.0f * 100.0f,"% left"));
            else
                return(custom_value_units((i - 64.0f)/63.0f*100.0f,"% right"));

        case VC_PanningStd:
            i = lrint(val);
            if(i==64)
                return("centered");
            else if(i<64)
                return(custom_value_units((64.0f - i) / 64.0f * 100.0f,"% left"));
            else
                return(custom_value_units((i - 64.0f)/63.0f*100.0f,"% right"));

        case VC_EnvStretch:
            s.clear();
            f = powf(2.0f,(int)val/64.0f);
            s += custom_value_units((int)val/127.0f*100.0f+0.05f,"%",1);
            if ((int)val!=0)
            {
                s += ", ( x";
                s += custom_value_units(f+0.005f,"/octave down)",2);
            }
            return s;

        case VC_LFOStretch:
            s.clear();
            i = val;
            i = (i == 0) ? 1 : (i); // val == 0 is not allowed
            f = powf(2.0f,(i-64.0)/63.0f);
            s += custom_value_units((i-64.0f)/63.0f*100.0f,"%");
            if (i != 64)
            {
                s += ", ( x";
                s += custom_value_units(f+((f<0) ? (-0.005f) : (0.005f)),
                                    "/octave up)",2);
            }
            return s;

        case VC_FreqOffsetHz:
            f = ((int)val-64.0f)/64.0f;
            f = 15.0f*(f * sqrtf(fabsf(f)));
            return(custom_value_units(f+((f<0) ? (-0.005f) : (0.005f)),"Hz",2));

        case VC_FixedFreqET:
            f = powf(2.0f, (lrint(val) - 1) / 63.0f) - 1.0f;
            if(lrint(val) <= 1) /* 0 and 1 are both fixed */
                return "Fixed";
            else if(lrint(val) <= 64)
                return custom_value_units(powf(2.0f,f),"x /octave up",2);
            else
                return custom_value_units(powf(3.0f,f),"x /octave up",2);

        case VC_FilterGain:
            f = ((int)val / 64.0f -1.0f) * 30.0f; // -30..30dB
            f += (f<0) ? -0.05 : 0.05;
            return(custom_value_units(f, "dB", 1));

        case VC_AmpVelocitySense:
            i = val;
            s.clear();
            if (i==127)
            {
                s += "Velocity sensing disabled.";
                return(s);
            }
            f = powf(8.0f, (64.0f - (float)i) / 64.0f);
            // Max dB range for vel=1 compared to vel=127
            s += "Velocity Dynamic Range ";
            f = -20.0f * logf(powf((1.0f / 127.0f), f)) / log(10.0f);
            s += variable_prec_units(f, "dB", 2);
            s += "\nVelocity/2 = ";
            s += variable_prec_units(f/(-1 * log2(127)), "dB", 2);
            return(s);

        case VC_BandWidth:
            f = powf((int)val / 1000.0f, 1.1f);
            f = powf(10.0f, f * 4.0f) * 0.25f;
            return variable_prec_units(f, "cents", 2);

        case VC_SubBandwidth:
            /* This is only an approximation based on observation.
               Considering the variability of the synthesis depending
               on number of filter stages, it seems accurate enough.
             */
	    f = powf(10.0f, (val - 127.0f) / 127.0f * 4.0f) * 4800;
            return variable_prec_units(f, "cents", 3);

        case VC_SubBandwidthRel:
	    f = powf(100.0f, (63 - (int)val) / 64.0f);
            return variable_prec_units(f, "x", 3);

        case VC_SubBandwidthScale:
            if((int)val == 0)
                return "Constant";
	    f = val / 64.0f * 3.0f;
            return "Factor (100,10k): " +
                variable_prec_units(powf(10,f), "", 4) + ", " +
                variable_prec_units(powf(0.1,f), "x", 4);

        case VC_FilterVelocitySense: // this is also shown graphically
            if((int)val==127)
                return("off");
            else
                return(custom_value_units(val,""));
            break;

        case VC_FXSysSend:
            if((int)val==0)
                return("-inf dB");
            else
                return(custom_value_units((val-96.0f)/96.0f*40.0f,"dB",1));

        case VC_FXEchoVol:
            // initial volume is set in Echo::setvolume like this
            f = powf(0.01f, (1.0f - (int)val / 127.0f)) * 4.0f;
            // in Echo::out this is multiplied by a panning value
            // which is 0.707 for centered and by 2.0
            // in EffectMgr::out it is multiplied by 2.0 once more
            // so in the end we get
            f *= 2.828f; // 0.707 * 4
            f = 20.0f * logf(f) / logf(10.0f);
            // Here we are finally
            return(custom_value_units(f,"dB",1));

        case VC_FXEchoDelay:
            // delay is 0 .. 1.5 sec
            f = (int)val / 127.0f * 1.5f;
            return(custom_value_units(f+0.005f,"s",2));

        case VC_FXEchoLRdel:
            s.clear();
            // ToDo: It would be nice to calculate the ratio between left
            // and right. We would need to know the delay time however...
            f = (powf(2.0f, fabsf((int)val-64.0f)/64.0f*9.0f)-1.0); // ms
            if ((int)val < 64)
            {
                s+="left +"+custom_value_units(f+0.05,"ms",1)+" / ";
                s+=custom_value_units(-f-0.05,"ms",1)+" right";
            }
            else
            {
                s+="left "+custom_value_units(-f-0.05,"ms",1)+" / ";
                s+="+"+custom_value_units(f+0.05,"ms",1)+" right";
            }
            return(s);

        case VC_FXEchoDW:
            s.clear();
            f = (int)val / 127.0f;
            if(f < 0.5f)
            {
                f = f * 2.0f;
                f *= f;  // for Reverb and Echo
                f *= 1.414; // see VC_FXEchoVol for 0.707 * 2.0
                f = 20.0f * logf(f) / logf(10.0f);
                s += "Dry: -0 dB, Wet: "
                    +custom_value_units(f,"dB",1);
            }
            else
            {
                f = (1.0f - f) * 2.0f;
                f = 20.0f * logf(f) / logf(10.0f);
                s += "Dry: "
                    +custom_value_units(f,"dB",1)+", Wet: +3.0 dB";
            }
            return(s);

        case VC_FXReverbVol:
            f = powf(0.01f, (1.0f - (int)val / 127.0f)) * 4.0f;
            f = 20.0f * logf(f) / logf(10.0f);
            return(custom_value_units(f,"dB",1));

        case VC_FXReverbTime:
            f = powf(60.0f, (int)val / 127.0f) - 0.97f; // s
            return variable_prec_units(f, "s", 2, true);

        case VC_FXReverbIDelay:
            f = powf(50.0f * (int)val / 127.0f, 2.0f) - 1.0f; // ms
            if ((int)f > 0)
            {
                if (f<1000.0f)
                    return(custom_value_units(f+0.5f,"ms"));
                else
                    return(custom_value_units(f/1000.0+0.005f,"s",2));
            }
            else
                return("0 ms");

        case VC_FXReverbHighPass:
            f = expf(powf((int)val / 127.0f, 0.5f) * logf(10000.0f)) + 20.0f;
            if ((int)val == 0)
                return("no high pass");
            else if (f<1000.0f)
                return(custom_value_units(f+0.5f,"Hz"));
            else
                return(custom_value_units(f/1000.0f+0.005f,"kHz",2));

        case VC_FXReverbLowPass:
            f = expf(powf((int)val / 127.0f, 0.5f) * logf(25000.0f)) + 40.0f;
            if ((int)val == 127)
                return("no low pass");
            else if (f<1000.0f)
                return(custom_value_units(f+0.5f,"Hz"));
            else
                return(custom_value_units(f/1000.0f+0.005f,"kHz",2));

        case VC_FXReverbDW:
            s.clear();
            f = (int)val / 127.0f;
            if(f < 0.5f)
            {
                f = f * 2.0f;
                f *= f;  // for Reverb and Echo
                f = 20.0f * logf(f) / logf(10.0f);
                s += "Dry: -0 dB, Wet: "
                    +custom_value_units(f,"dB",1);
            }
            else
            {
                f = (1.0f - f) * 2.0f;
                f = 20.0f * logf(f) / logf(10.0f);
                s += "Dry: "
                    +custom_value_units(f,"dB",1)+", Wet: -0 dB";
            }
            return(s);

        case VC_FXReverbBandwidth:
            f = powf((int)val / 127.0f, 2.0f) * 200.0f; // cents
            return variable_prec_units(f, "cents", 2, true);

        case VC_FXdefaultVol:
            f = ((int)val / 127.0f)*1.414f;
            f = 20.0f * logf(f) / logf(10.0f);
            return(custom_value_units(f,"dB",1));

        case VC_FXlfofreq:
            f = (powf(2.0f, (int)val / 127.0f * 10.0f) - 1.0f) * 0.03f;
            return variable_prec_units(f, "Hz", 3);

        case VC_FXChorusDepth:
            f = powf(8.0f, ((int)val / 127.0f) * 2.0f) -1.0f; //ms
            return variable_prec_units(f, "ms", 2, true);

        case VC_FXChorusDelay:
            f = powf(10.0f, ((int)val / 127.0f) * 2.0f) -1.0f; //ms
            return variable_prec_units(f, "ms", 2, true);

        case VC_FXdefaultFb:
            f = (((int)val - 64.0f) / 64.1f) * 100.0f;
            return(custom_value_units(f,"%"));

        case VC_FXlfoStereo:
            f = ((int)val - 64.0f) / 127.0 * 360.0f;
            if ((int)val == 64)
                return("equal");
            else if (f < 0.0f)
                return("left +"+custom_value_units(-f,"°"));
            else
                return("right +"+custom_value_units(f,"°"));

        case VC_FXdefaultDW:
            s.clear();
            f = (int)val / 127.0f;
            if(f < 0.5f)
            {
                f = f * 2.0f;
                f = 20.0f * logf(f) / logf(10.0f);
                s += "Dry: -0 dB, Wet: "
                    +custom_value_units(f,"dB",1);
            }
            else
            {
                f = (1.0f - f) * 2.0f;
                f = 20.0f * logf(f) / logf(10.0f);
                s += "Dry: "
                    +custom_value_units(f,"dB",1)+", Wet: -0 dB";
            }
            return(s);

        case VC_FXEQfreq:
            f = 600.0f * powf(30.0f, ((int)val - 64.0f) / 64.0f);
            if (f >= 1000)
                return variable_prec_units(f/1000.f, "kHz", 2);
            else
                return variable_prec_units(f, "Hz", 2, true);

        case VC_FXEQq:
            f = powf(30.0f, ((int)val - 64.0f) / 64.0f);
            return variable_prec_units(f, "", 3, true);

        case VC_FXEQgain:
            f = 20.0f - 46.02f*(1.0f - ((int)val / 127.0f));
            // simplification of
            // powf(0.005f, (1.0f - Pvolume / 127.0f)) * 10.0f;
            // by approximating 0.005^x ~= 10^(-2.301*x)    | log10(200)=2.301
            // Max. error is below 0.01 which is less than displayed precision
            return(custom_value_units(f,"dB",1));

        case VC_FXEQfilterGain:
            f = 30.0f * ((int)val - 64.0f) / 64.0f;
            return(custom_value_units(f,"dB",1));

        case VC_plainValue:
        {
            /* Avoid trailing space */
            ostringstream oss;
            oss.setf(std::ios_base::fixed);
            oss.precision(0);
            oss << val;
            return string(oss.str());
        }
        case VC_FXDistVol:
            f = -40.0f * (1.0f - ((int)val / 127.0f)) + 15.05f;
            return(custom_value_units(f,"dB",1));

        case VC_FXDistLevel:
            f = 60.0f * (int)val / 127.0f - 40.0f;
            return(custom_value_units(f,"dB",1));

        case VC_FXDistLowPass:
            f = expf(powf((int)val / 127.0f, 0.5f) * logf(25000.0f)) + 40.0f;
            if (f<1000.0f)
                return(custom_value_units(f+0.5f,"Hz"));
            else
                return(custom_value_units(f/1000.0f+0.005f,"kHz",2));

        case VC_FXDistHighPass:
            f = expf(powf((int)val / 127.0f, 0.5f) * logf(25000.0f)) + 20.0f;
            if (f<1000.0f)
                return(custom_value_units(f+0.5f,"Hz"));
            else
                return(custom_value_units(f/1000.0f+0.005f,"kHz",2));
    }
    // avoid compiler warning
    return(custom_value_units(val,""));
}

void custom_graph_dimensions(ValueType vt, int& w, int& h)
{
    switch(vt)
    {
    case VC_FilterVelocitySense:
        w = 128;
        h = 64;
        break;
    case VC_SubBandwidthScale:
        w = 256;
        h = 128;
        break;
    default:
        w = 0;
        h = 0;
    }
}

void custom_graphics(ValueType vt, float val,int W,int H)
{
    int x0,y0,i;
    int _w, _h;
    float x,y,p;
    custom_graph_dimensions(vt, _w, _h);
    x0 = W / 2 - (_w / 2);
    y0 = H;

    switch(vt)
    {
    case VC_FilterVelocitySense:
    {
        p = powf(8.0f,(64.0f-(int)val)/64.0f);

        /* Grid */
        fl_color(FL_GRAY);

        int j = 1;
        int gDist = _h / 4;
        for(; j < 4; j++) /* Vertical */
        {
            fl_line(x0, y0 - gDist * j, x0 + _w, y0 - gDist * j);
        }

        gDist = _w / 4;
        for(j = 1; j < 4; j++) /* Horizontal */
        {
            fl_line(x0 + gDist * j, y0, x0 + gDist * j, y0 - _h);
        }

        /*Function curve*/
        fl_color(FL_BLUE);
        if ((int)val == 127)
        {   // in this case velF will always return 1.0
            y = y0 - _h;
            fl_line(x0, y, x0 + _w, y);
        }
        else
        {
            fl_begin_line();
            for(i = 0; i < _w; i++)
            {
                x = (float)i / (float)_w;
                y = powf(x,p) * _h;
                fl_vertex((float)x0 + i, (float)y0 - y);
            }
            fl_end_line();
        }
        break;
    }
    case VC_SubBandwidthScale:
    {
        /* The scale centers around the factor 1 vertically
           and is logarithmic in both dimensions. */

        int margin = 28;
        _h -= margin;
        _w -= margin * 2;
        x0 += margin * 1.25;
        y0 -= margin * 0.75;

        float cy = y0 - _h / 2;

        int j = 1;
        const float lg1020 = log10(20); /* Lower bound = 20hz*/
        const float rx = _w / (log10(20000) - lg1020); /* log. width ratio */
        const float ry = (_h / 2) / log10(100000);

        string hzMarkers[] = {"20", "100", "1k", "10k"};
        string xMarkers[] = {"x10","x100","x1k","x10k","10%","1%","0.1%","0.01%"};

        /* Scale lines */

        fl_font(fl_font(),8);
        fl_line_style(0);
        for(i = 0; i < 4; i++) /* 10x / 10%, 100x / 1% ... */
        {
            y = ry * (i + 1);
            fl_color(169,169,169);
            fl_line(x0, cy - y, x0 + _w, cy - y);
            fl_line(x0, cy + y, x0 + _w, cy + y);
            fl_color(0,0,0);
            fl_draw(xMarkers[i].c_str(), x0 - 28, (cy - y - 4), 24, 12,
                    Fl_Align(FL_ALIGN_RIGHT));
            fl_draw(xMarkers[i + 4].c_str(), x0 - 28, (cy + y - 4), 24, 12,
                    Fl_Align(FL_ALIGN_RIGHT));
        }

        /* Hz lines */

        fl_color(196,196,196); /* Lighter inner lines*/

        for(i = 10;i != 0; i *= 10)
        {
            for(j = 2; j < 10; j++)
            {
                x = x0 + rx * (log10(i * j) - lg1020) + 1;
                fl_line(x, y0, x, y0 - _h);
                if(i * j >= 20000)
                {
                    i = 0;
                    break;
                }
            }
        }

        fl_font(fl_font(),10);
        for(i = 0; i < 4; i++) /* 20, 100, 1k, 10k */
        {
            x = x0 + (i == 0 ?  0 : ((float)i + 1 - lg1020) * rx);
            fl_color(127,127,127); /* Darker boundary lines */
            fl_line(x, y0, x, y0 - _h);
            fl_color(FL_BLACK);
            fl_draw(hzMarkers[i].c_str(), x - 20, y0 + 4, 40, 12,
                    Fl_Align(FL_ALIGN_CENTER));
        }
        /* Unit marker at the lower right of the graph */
        fl_draw("Hz", x0 + _w, y0 + 4, 20, 12, Fl_Align(FL_ALIGN_LEFT));

        /* Vertical center line */
        fl_color(64,64,64);
        fl_line(x0 - margin, cy, x0 + _w, cy);

        /* Function curve */
        fl_color(FL_BLUE);
        if((int)val == 0)
        {
            fl_line(x0, cy, x0 + _w, cy);
        }
        else
        {
            const float p = ((int)val / 64.0f) * 3.0;

            /* Cairo not necessary, but makes it easier to read the graph */
            cairo_t *cr;
            cairo_surface_t* Xsurface = cairo_xlib_surface_create
                (fl_display, fl_window, fl_visual->visual,
                 Fl_Window::current()->w(), Fl_Window::current()->h());
            cr = cairo_create (Xsurface);

            cairo_set_source_rgb(cr, 1, 0, 0);
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, x0, cy - ry * log10(powf(50, p)));
            cairo_line_to(cr, x0 + _w, cy - ry * log10(powf(0.05, p)));
            cairo_stroke(cr);

            cairo_surface_destroy(Xsurface);  cairo_destroy(cr);
        }
        break;
    }
    default:
        break;
    }
}

string variable_prec_units(float v, string u, int maxPrec, bool roundup)
{
    int digits = 0, lim = (int) pow(10,maxPrec);
    float _v = fabsf(v);
    while(maxPrec > digits)
    {
        if(_v >= lim)
            break;
        digits++;
        lim /= 10;
    }
    if (roundup)
    {
        v += 5 * powf(10,-(digits + 1));
    }
    return custom_value_units(v, u, digits);
}

string custom_value_units(float v, string u, int prec)
{
    ostringstream oss;
    oss.setf(std::ios_base::fixed);
    oss.precision(prec);
    oss << v << " " << u;
    return(string(oss.str()));
}

ValueType getLFOdepthType(int group)
{
    switch(group)
    {
        case 0: return(VC_LFOdepthAmp);
        case 1: return(VC_LFOdepthFreq);
        case 2: return(VC_LFOdepthFilter);
    }
    return(VC_plainValue);
}

ValueType getFilterFreqType(int type)
{
    switch(type)
    {
        case 0: return(VC_FilterFreq0);
        case 1: return(VC_FilterFreq1);
        case 2: return(VC_FilterFreq2);
    }
    return(VC_plainValue);
}

ValueType getFilterFreqTrackType(int offset)
{
    switch(offset)
    {
        case 0: return(VC_FilterFreqTrack0);
        default: return(VC_FilterFreqTrack1);
    }
}
