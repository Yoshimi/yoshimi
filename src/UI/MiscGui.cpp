/*
    MiscGui.cpp - common link between GUI and synth

    Copyright 2016-2021 Will Godfrey & others

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

#include "Misc/SynthEngine.h"
#include "Misc/TextMsgBuffer.h"
#include "MiscGui.h"
#include "MasterUI.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <cairo.h>
#include <cairo-xlib.h>


namespace { // Implementation details...

    TextMsgBuffer& textMsgBuffer = TextMsgBuffer::instance();
}

float collect_readData(SynthEngine *synth, float value, unsigned char control, unsigned char part, unsigned char kititem, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset, unsigned char miscmsg, unsigned char request)
{
    unsigned char type = 0;
    unsigned char action = TOPLEVEL::action::fromGUI;
    if (request < TOPLEVEL::type::Limits)
        type = request | TOPLEVEL::type::Limits; // its a limit test
    else if (request != UNUSED)
        action |= request;
    CommandBlock putData;

    putData.data.value = value;
    putData.data.type = type;
    putData.data.source = action;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kititem;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.offset = offset;
    putData.data.miscmsg = miscmsg;
    float result;
    if (miscmsg != NO_MSG)
    {
        synth->interchange.readAllData(&putData);
        result = putData.data.miscmsg;
    }
    else
        result = synth->interchange.readAllData(&putData);
    return result;

}

void collect_data(SynthEngine *synth, float value, unsigned char action, unsigned char type, unsigned char control, unsigned char part, unsigned char kititem, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char offset, unsigned char miscmsg)
{
    if (part < NUM_MIDI_PARTS && engine == PART::engine::padSynth)
    {
        if (collect_readData(synth, 0, PART::control::partBusy, part))
        {
            alert(synth, "Part " + to_string(part + 1) + " is busy");
            return;
        }
    }
    CommandBlock putData;
    putData.data.value = value;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kititem;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.offset = offset;
    putData.data.miscmsg = miscmsg;
    if (action == TOPLEVEL::action::fromMIDI)
        type = type | 1; // faking MIDI from virtual keyboard
    else
    {
        if (part != TOPLEVEL::section::midiLearn)
        { // midilearn UI must pass though un-modified
            unsigned char typetop = type & (TOPLEVEL::type::Write | TOPLEVEL::type::Integer);
            unsigned char buttons = Fl::event_button();
            if (part == TOPLEVEL::section::main && (control != MAIN::control::volume &&  control  != MAIN::control::detune))
                type = 1;

            if (buttons == 3 && Fl::event_is_click())
            {
                // check range & if learnable
                float newValue;
                putData.data.type = 3 | TOPLEVEL::type::Limits;
                newValue = synth->interchange.readAllData(&putData);
                //if (newValue != value)
                    //std::cout << "Gui limits " << value <<" to " << newValue << std::endl;
                if (Fl::event_state(FL_CTRL) != 0)
                {
                    if (putData.data.type & TOPLEVEL::type::Learnable)
                    {
                        // identifying this for button 3 as MIDI learn
                        type = TOPLEVEL::type::LearnRequest;
                    }
                    else
                    {
                        alert(synth, "Can't learn this control");
                        synth->getRuntime().Log("Can't MIDI-learn this control");
                        type = TOPLEVEL::type::Learnable;
                    }
                }
                else
                {
                    putData.data.value = newValue;
                    type = TOPLEVEL::type::Write;
                    action |= TOPLEVEL::action::forceUpdate;
                    // has to be write as it's 'set default'
                }
            }
            else if (buttons > 2)
                type = 1; // change scroll wheel to button 1
            type |= typetop;
            action |= TOPLEVEL::action::fromGUI;
        }
    }

    putData.data.type = type;
    putData.data.source = action;
    //cout << "collect_data value " << value << "  action " << int(action)  << "  type " << int(type) << "  control " << int(control) << "  part " << int(part) << "  kit " << int(kititem) << "  engine " << int(engine) << "  insert " << int(insert)  << "  par " << int(parameter) << " offset " << int(offset) << " msg " << int(miscmsg) << endl;
    if (!synth->interchange.fromGUI.write(putData.bytes))
        synth->getRuntime().Log("Unable to write to fromGUI buffer.");
}

void alert(SynthEngine *synth, string message)
{
    synth->getGuiMaster()->query("", "", "", message);
}

int choice(SynthEngine *synth, string one, string two, string three, string message)
{
    return synth->getGuiMaster()->query(one, two, three, message);
}

string setfiler(SynthEngine *synth, string title, string name, bool save, int extension)
{
    return synth->getGuiMaster()->setfiler(title, name, save, extension);
}

string input_text(SynthEngine *synth, string label, string text)
{
    return synth->getGuiMaster()->setinput(label, text);
}

//static int keycheck = 0;
void GuiUpdates::read_updates(SynthEngine *synth)
{
    CommandBlock getData;
    while (synth->interchange.toGUI.read(getData.bytes))
    {
        Fl::lock();
        decode_updates(synth, &getData);
        Fl::unlock();
    }
    // and pull up to 5 entries from log
    for (int i = 0; !synth->getRuntime().LogList.empty() && i < 5; ++i)
    {
        synth->getGuiMaster()->Log(synth->getRuntime().LogList.front());
        synth->getRuntime().LogList.pop_front();
    }
    // direct key actions (not for LV2)
    /*if (!synth->getIsLV2Plugin() && Fl::event_ctrl() && Fl::event_alt() && keycheck == 0)
    {
        if (Fl::event_key('H')) // immediate shutdown
            synth->getGuiMaster()->masterwindow->do_callback();
        else if (Fl::event_key('R')) // resize to fit
        {
            if(!synth->getIsLV2Plugin())
            for (unsigned int id = 0; id < 31; ++id)
            {
                SynthEngine *_synth = synth->getSynthFromId(id);
                if (_synth != synth)
                    _synth->getGuiMaster()->rescaleMain();
            }
            synth->getGuiMaster()->rescaleMain();
        }
        keycheck = 90; // we need this to stop a stream of repeats

    }
    else if (keycheck > 0)
        --keycheck;*/
}


void GuiUpdates::decode_envelope(SynthEngine *synth, CommandBlock *getData)
{
    unsigned char engine = getData->data.engine;
    unsigned char parameter = getData->data.parameter;
    if (engine >= PART::engine::addMod1)
    {
        switch(parameter)
        {
            case TOPLEVEL::insertType::amplitude:
                if (synth->getGuiMaster()->partui->adnoteui->advoice->voiceFMampenvgroup)
                    synth->getGuiMaster()->partui->adnoteui->advoice->voiceFMampenvgroup->returns_update(getData);
                break;
            case TOPLEVEL::insertType::frequency:
                if (synth->getGuiMaster()->partui->adnoteui->advoice->voiceFMfreqenvgroup)
                    synth->getGuiMaster()->partui->adnoteui->advoice->voiceFMfreqenvgroup->returns_update(getData);
                break;
        }
    }
    else
    {
        switch(parameter)
        {
            case TOPLEVEL::insertType::amplitude:
                if (synth->getGuiMaster()->partui->adnoteui->advoice->voiceampenvgroup)
                    synth->getGuiMaster()->partui->adnoteui->advoice->voiceampenvgroup->returns_update(getData);
                break;
            case TOPLEVEL::insertType::frequency:
                if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqenvgroup)
                    synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqenvgroup->returns_update(getData);
                break;
            case TOPLEVEL::insertType::filter:
                if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterenvgroup)
                    synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterenvgroup->returns_update(getData);
                break;
        }
    }
}


void GuiUpdates::decode_updates(SynthEngine *synth, CommandBlock *getData)
{
    unsigned char control = getData->data.control;
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    unsigned char insert = getData->data.insert;
    unsigned char parameter = getData->data.parameter;
    unsigned char miscmsg = getData->data.miscmsg;

//        cout << "Con " << int(control) << "  Kit " << int(kititem) << "  Eng " << int(engine) << "  Ins " << int(insert) << endl;

    if (control == TOPLEVEL::control::textMessage) // just show a non-modal message
    {
        string name = textMsgBuffer.fetch(miscmsg);
        if (name.empty())
            synth->getGuiMaster()->message->hide();
        else
            synth->getGuiMaster()->setmessage(UNUSED, true, name, "Close");
        return;
    }
    if (npart == TOPLEVEL::section::scales)
    {
        synth->getGuiMaster()->microtonalui->returns_update(getData);
        return;
    }
    if (npart == TOPLEVEL::section::vector)
    {
        synth->getGuiMaster()->vectorui->returns_update(getData);
        return;
    }
    if (npart == TOPLEVEL::section::midiLearn && synth->getGuiMaster()->midilearnui != NULL)
    {
        synth->getGuiMaster()->midilearnui->returns_update(getData);
        return;
    }
    if (npart == TOPLEVEL::section::midiIn) //  catch this early
    {
        synth->getGuiMaster()->returns_update(getData);
        return;
    }

    if (npart == TOPLEVEL::section::bank)
    {
        synth->getGuiMaster()->bankui->returns_update(getData);
        return;
    }

    if (kititem >= EFFECT::type::none && kititem != UNUSED) // effects
    {
        if (npart == TOPLEVEL::section::systemEffects)
        {
            if (engine != synth->getGuiMaster()->nsyseff)
                return;
            if (insert == TOPLEVEL::insert::filterGroup) // dynefilter filter insert
                synth->getGuiMaster()->syseffectui->fwin_filterui->returns_update(getData);
            else
                synth->getGuiMaster()->syseffectui->returns_update(getData);
        }
        else if (npart == TOPLEVEL::section::insertEffects)
        {
            if (engine != synth->getGuiMaster()->ninseff)
                return;
            if (insert == TOPLEVEL::insert::filterGroup) // dynefilter filter insert
                synth->getGuiMaster()->inseffectui->fwin_filterui->returns_update(getData);
            else
                synth->getGuiMaster()->inseffectui->returns_update(getData);
        }
        else if (npart < NUM_MIDI_PARTS)
        {
            if (engine != synth->getGuiMaster()->partui->ninseff)
                return;
            if (insert == TOPLEVEL::insert::filterGroup) // dynefilter filter insert
                synth->getGuiMaster()->partui->inseffectui->fwin_filterui->returns_update(getData);
            else
                synth->getGuiMaster()->partui->inseffectui->returns_update(getData);
        }
        return;
    }

    if (npart == TOPLEVEL::section::config)
    {
        synth->getGuiMaster()->configui->returns_update(getData);
        return;
    }
    if (npart == TOPLEVEL::section::main && control == MAIN::control::exportPadSynthSamples) // special case
    {
        npart = parameter & 0x3f;
        getData->data.part = npart;
    }
    if (npart >= TOPLEVEL::section::main) // main / sys / ins
    {
        synth->getGuiMaster()->returns_update(getData);
        return;
    }

    if (npart >= NUM_MIDI_PARTS)
        return; // invalid part number

    if (kititem >= NUM_KIT_ITEMS && kititem != UNUSED)
        return; // invalid kit number

    if (insert != UNUSED || (control != PART::control::enable && control != PART::control::instrumentName))
    {
        if (synth->getGuiMaster()->partui->partname == DEFAULT_NAME)
            synth->getGuiMaster()->partui->checkEngines(UNTITLED);
    }
    if (kititem == UNUSED || insert == TOPLEVEL::insert::kitGroup) // part
    {
        Part *part = synth->part[npart];

        if (control != PART::control::kitMode && kititem != UNUSED && part->Pkitmode == 0)
            return; // invalid access
        synth->getGuiMaster()->partui->returns_update(getData);
        return;
    }
    if (kititem != synth->getGuiMaster()->partui->lastkititem)
        return; // not for us!
    if (engine == PART::engine::padSynth) // padsynth
    {
        if (synth->getGuiMaster()->partui->padnoteui)
        {
            switch (insert)
            {
                case UNUSED:
                    synth->getGuiMaster()->partui->padnoteui->returns_update(getData);
                    break;
                case TOPLEVEL::insert::LFOgroup:
                    switch(parameter)
                    {
                        case TOPLEVEL::insertType::amplitude:
                            if (synth->getGuiMaster()->partui->padnoteui->amplfo)
                                synth->getGuiMaster()->partui->padnoteui->amplfo->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::frequency:
                            if (synth->getGuiMaster()->partui->padnoteui->freqlfo)
                                synth->getGuiMaster()->partui->padnoteui->freqlfo->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::filter:
                            if (synth->getGuiMaster()->partui->padnoteui->filterlfo)
                                synth->getGuiMaster()->partui->padnoteui->filterlfo->returns_update(getData);
                            break;
                    }
                    break;
                case TOPLEVEL::insert::filterGroup:
                    if (synth->getGuiMaster()->partui->padnoteui->filterui)
                        synth->getGuiMaster()->partui->padnoteui->filterui->returns_update(getData);
                    break;
                case TOPLEVEL::insert::envelopeGroup:
                case TOPLEVEL::insert::envelopePoints:
                case TOPLEVEL::insert::envelopePointChange:
                    switch(parameter)
                    {
                        case TOPLEVEL::insertType::amplitude:
                            if (synth->getGuiMaster()->partui->padnoteui->ampenv)
                                synth->getGuiMaster()->partui->padnoteui->ampenv->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::frequency:
                            if (synth->getGuiMaster()->partui->padnoteui->freqenv)
                                synth->getGuiMaster()->partui->padnoteui->freqenv->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::filter:
                            if (synth->getGuiMaster()->partui->padnoteui->filterenv)
                                synth->getGuiMaster()->partui->padnoteui->filterenv->returns_update(getData);
                            break;
                    }
                    break;

                case TOPLEVEL::insert::oscillatorGroup:
                case TOPLEVEL::insert::harmonicAmplitude:
                case TOPLEVEL::insert::harmonicPhaseBandwidth:
                    if (synth->getGuiMaster()->partui->padnoteui->oscui)
                        synth->getGuiMaster()->partui->padnoteui->oscui->returns_update(getData);
                    break;
                case TOPLEVEL::insert::resonanceGroup:
                case TOPLEVEL::insert::resonanceGraphInsert:
                    if (synth->getGuiMaster()->partui->padnoteui->resui)
                        synth->getGuiMaster()->partui->padnoteui->resui->returns_update(getData);
                    break;
            }
        }
        else if (miscmsg != NO_MSG)
        {
            textMsgBuffer.fetch(miscmsg); // clear any text out.
        }
        return;
    }

    if (engine == PART::engine::subSynth) // subsynth
    {
        if (synth->getGuiMaster()->partui->subnoteui)
            switch (insert)
            {
                case TOPLEVEL::insert::filterGroup:
                    if (synth->getGuiMaster()->partui->subnoteui->filterui)
                        synth->getGuiMaster()->partui->subnoteui->filterui->returns_update(getData);
                    break;
                case TOPLEVEL::insert::envelopeGroup:
                case TOPLEVEL::insert::envelopePoints:
                case TOPLEVEL::insert::envelopePointChange:
                    switch(parameter)
                    {
                        case TOPLEVEL::insertType::amplitude:
                            if (synth->getGuiMaster()->partui->subnoteui->ampenv)
                                synth->getGuiMaster()->partui->subnoteui->ampenv->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::frequency:
                            if (synth->getGuiMaster()->partui->subnoteui->freqenvelopegroup)
                                synth->getGuiMaster()->partui->subnoteui->freqenvelopegroup->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::filter:
                            if (synth->getGuiMaster()->partui->subnoteui->filterenv)
                                synth->getGuiMaster()->partui->subnoteui->filterenv->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::bandwidth:
                            if (synth->getGuiMaster()->partui->subnoteui->bandwidthenvelopegroup)
                                synth->getGuiMaster()->partui->subnoteui->bandwidthenvelopegroup->returns_update(getData);
                            break;
                    }
                    break;
                case UNUSED:
                case TOPLEVEL::insert::harmonicAmplitude:
                case TOPLEVEL::insert::harmonicPhaseBandwidth:
                    synth->getGuiMaster()->partui->subnoteui->returns_update(getData);
                    break;
            }
        return;
    }

    if (engine >= PART::engine::addVoice1) // addsynth voice / modulator
    {
        if (synth->getGuiMaster()->partui->adnoteui)
        {
            if (synth->getGuiMaster()->partui->adnoteui->advoice)
            {
                switch (insert)
                {
                    case UNUSED:
                        synth->getGuiMaster()->partui->adnoteui->advoice->returns_update(getData);
                        break;
                    case TOPLEVEL::insert::LFOgroup:
                        switch(parameter)
                        {
                            case TOPLEVEL::insertType::amplitude:
                                if (synth->getGuiMaster()->partui->adnoteui->advoice->voiceamplfogroup)
                                    synth->getGuiMaster()->partui->adnoteui->advoice->voiceamplfogroup->returns_update(getData);
                                break;
                            case TOPLEVEL::insertType::frequency:
                                if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqlfogroup)
                                    synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqlfogroup->returns_update(getData);
                                break;
                            case TOPLEVEL::insertType::filter:
                                if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterlfogroup)
                                    synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterlfogroup->returns_update(getData);
                                break;
                        }
                        break;
                    case TOPLEVEL::insert::filterGroup:
                        if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefilter)
                            synth->getGuiMaster()->partui->adnoteui->advoice->voicefilter->returns_update(getData);
                        break;
                    case TOPLEVEL::insert::envelopeGroup:
                        decode_envelope(synth, getData);
                        break;
                    case TOPLEVEL::insert::envelopePoints:
                        decode_envelope(synth, getData);
                        break;
                    case TOPLEVEL::insert::envelopePointChange:
                        decode_envelope(synth, getData);
                        break;
                    case TOPLEVEL::insert::oscillatorGroup:
                    case TOPLEVEL::insert::harmonicAmplitude:
                    case TOPLEVEL::insert::harmonicPhaseBandwidth:
                        if (synth->getGuiMaster()->partui->adnoteui->advoice->oscedit)
                            synth->getGuiMaster()->partui->adnoteui->advoice->oscedit->returns_update(getData);
                        break;
                }
            }
        }
        return;
    }

    if (engine == PART::engine::addSynth) // addsynth base
    {
        if (synth->getGuiMaster()->partui->adnoteui)
            switch (insert)
            {
                case UNUSED:
                    synth->getGuiMaster()->partui->adnoteui->returns_update(getData);
                    break;
                case TOPLEVEL::insert::LFOgroup:
                    switch(parameter)
                    {
                        case TOPLEVEL::insertType::amplitude:
                            if (synth->getGuiMaster()->partui->adnoteui->amplfo)
                                synth->getGuiMaster()->partui->adnoteui->amplfo->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::frequency:
                            if (synth->getGuiMaster()->partui->adnoteui->freqlfo)
                                synth->getGuiMaster()->partui->adnoteui->freqlfo->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::filter:
                            if (synth->getGuiMaster()->partui->adnoteui->filterlfo)
                                synth->getGuiMaster()->partui->adnoteui->filterlfo->returns_update(getData);
                            break;
                    }
                    break;
                case TOPLEVEL::insert::filterGroup:
                    if (synth->getGuiMaster()->partui->adnoteui->filterui)
                        synth->getGuiMaster()->partui->adnoteui->filterui->returns_update(getData);
                    break;
                case TOPLEVEL::insert::envelopeGroup:
                case TOPLEVEL::insert::envelopePoints:
                case TOPLEVEL::insert::envelopePointChange:
                    switch(parameter)
                    {
                        case TOPLEVEL::insertType::amplitude:
                            if (synth->getGuiMaster()->partui->adnoteui->ampenv)
                                synth->getGuiMaster()->partui->adnoteui->ampenv->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::frequency:
                            if (synth->getGuiMaster()->partui->adnoteui->freqenv)
                                synth->getGuiMaster()->partui->adnoteui->freqenv->returns_update(getData);
                            break;
                        case TOPLEVEL::insertType::filter:
                            if (synth->getGuiMaster()->partui->adnoteui->filterenv)
                                synth->getGuiMaster()->partui->adnoteui->filterenv->returns_update(getData);
                            break;
                    }
                    break;

                case TOPLEVEL::insert::resonanceGroup:
                case TOPLEVEL::insert::resonanceGraphInsert:
                    if (synth->getGuiMaster()->partui->adnoteui->resui)
                        synth->getGuiMaster()->partui->adnoteui->resui->returns_update(getData);
                    break;
            }
        return;
    }
}

static string freqBPMStr(float val)
{
    // The switch statement below will need to be altered if this is ever
    // changed. Remember that intervals need to be preserved too, not just the
    // total number of steps, otherwise saved instruments will get incorrect
    // values.
    static_assert(LFO_BPM_STEPS == 33, "Need to adjust LFO_BPM_STEPS table.");
    return LFObpm[int(roundf(val * (LFO_BPM_STEPS + 2)))];
    /*
     * switch replaced by call into Interface/TextLists.h
     * so it is now unifed with CLI response - Will.
     */

    /*switch ((int)roundf(val * (LFO_BPM_STEPS + 2))) {
    case 0:
        // Some room to expand in the future. Fallthrough.
    case 1:
        return string("1/16 BPM");
    case 2:
        return string("1/15 BPM");
    case 3:
        return string("1/14 BPM");
    case 4:
        return string("1/13 BPM");
    case 5:
        return string("1/12 BPM");
    case 6:
        return string("1/11 BPM");
    case 7:
        return string("1/10 BPM");
    case 8:
        return string("1/9 BPM");
    case 9:
        return string("1/8 BPM");
    case 10:
        return string("1/7 BPM");
    case 11:
        return string("1/6 BPM");
    case 12:
        return string("1/5 BPM");
    case 13:
        return string("1/4 BPM");
    case 14:
        return string("1/3 BPM");
    case 15:
        return string("1/2 BPM");
    case 16:
        return string("2/3 BPM");
    case 17:
        return string("1/1 BPM");
    case 18:
        return string("3/2 BPM");
    case 19:
        return string("2/1 BPM");
    case 20:
        return string("3/1 BPM");
    case 21:
        return string("4/1 BPM");
    case 22:
        return string("5/1 BPM");
    case 23:
        return string("6/1 BPM");
    case 24:
        return string("7/1 BPM");
    case 25:
        return string("8/1 BPM");
    case 26:
        return string("9/1 BPM");
    case 27:
        return string("10/1 BPM");
    case 28:
        return string("11/1 BPM");
    case 29:
        return string("12/1 BPM");
    case 30:
        return string("13/1 BPM");
    case 31:
        return string("14/1 BPM");
    case 32:
        return string("15/1 BPM");
    case 34:
        // Some room to expand in the future. Fallthrough.
    case 33:
        return string("16/1 BPM");
    default:
        return string("Unknown BPM");
    }*/
}

string convert_value(ValueType type, float val)
{
    float f;
    int i;
    string s;
    switch(type)
    {
        case VC_plainReverse:
            return(custom_value_units(127.0f - val,"",1));

        case VC_pitchWheel:
            return(custom_value_units(-val,"",1));

        case VC_percent127:
            return(custom_value_units(val / 127.0f * 100.0f+0.05f,"%",1));

        case VC_percent128:
            return(custom_value_units(val / 128.0f * 100.0f+0.05f,"%",1));

        case VC_percent255:
            return(custom_value_units(val / 255.0f * 100.0f+0.05f,"%",1));

        case VC_percent64_127:
            return(custom_value_units((val-64) / 63.0f * 100.0f+0.05f,"%",1));

        case VC_PhaseOffset:
            return(custom_value_units(val / 64.0f * 90.0f,"°",1));

        case VC_WaveHarmonicMagnitude: {
            const string unit = val > 0 ? "% (inverted)" : "%";
            const int denom = val >= 0 ? 64 : -63;
            return(custom_value_units(val / denom * 100.0f,unit,1));
        }

        case VC_GlobalFineDetune:
            return(custom_value_units((val-64),"cents",1));

        case VC_MasterVolume:
            return(custom_value_units((val-96.0f)/96.0f*40.0f,"dB",1));

        case VC_LFOfreq:
            f = (powf(2.0f, val * 10.0f) - 1.0f) / 12.0f;
            return variable_prec_units(f, "Hz", 3);

        case VC_LFOfreqBPM:
            return freqBPMStr(val);

        case VC_LFOdepthFreq: // frequency LFO
            f=powf(2.0f,(int)val/127.0f*11.0f)-1.0f;
            return variable_prec_units(f, "cents", 2);

        case VC_LFOdepthAmp: // amplitude LFO
            return(custom_value_units(val / 127.0f * 200.0f,"%",1));

        case VC_LFOdepthFilter: // filter LFO
            val = (int)val / 127.0f * 4.0f; // 4 octaves
            f = val * 1200.0f; // cents
            return variable_prec_units(f, "cents", 2) + "\n("
                + custom_value_units(val, "base pos. offset)", 2);

        case VC_LFOdelay:
            f = ((int)val) / 127.0f * 4.0f + 0.005f;
            return(custom_value_units(f,"s",2));

        case VC_LFOstartphase:
            if ((int)val == 0)
                return("random");
            else
                return(custom_value_units(((int)val - 64.0f) / 127.0f
                                      * 360.0f, "°"));
        case VC_EnvelopeDT:
            // unfortunately converttofree() is not called in time for us to
            // be able to use env->getdt(), so we have to compute ourselves
            f = (powf(2.0f, ((int)val) / 127.0f * 12.0f) - 1.0f) * 10.0f;
            if (f >= 1000)
                return variable_prec_units(f/1000.0f, "s", 2);
            else
                return variable_prec_units(f, "ms", 2);

        case VC_EnvelopeFreqVal:
            f=(powf(2.0f, 6.0f * fabsf((int)val - 64.0f) / 64.0f) -1.0f) * 100.0f;
            if ((int)val<64) f = -f;
            return variable_prec_units(f, "cents", 2);

        case VC_EnvelopeFilterVal:
            val = ((int)val - 64.0f) / 64.0f;
            f = val * 7200.0f; // 6 octaves
            return variable_prec_units(f, "cents", 2) + "\n("
                + custom_value_units(val * 6.0f,"base pos. offset)",2);

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

        case VC_FilterFreq1: // Formant filter - base position in vowel sequence
            return(custom_value_units((val / 64.0f - 1.0f) * 5.0f,"x stretch (modulo 1)",2));

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
            val = (int)val / 127.0 * -6.0; // formant offset value
            f = powf(2.0f,val + log(1000.0f)/log(2.0f)); // getrealfreq
            f = log(f/1000.0f)/log(powf(2.0f,1.0f/12.0f))*100.0f; // in cents
            return custom_value_units(f-0.5, "cents") +
                   "\n(Formant offset: " + custom_value_units(val, "x stretch)",2);

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

        case VC_FormFilterClearness:
            f = powf(10.0f, (val - 32.0f) / 48.0f);
            return custom_value_units(f, " switch rate",2);

        case VC_FormFilterSlowness:
            f = powf(1.0f - (val / 128.0f), 3.0f);
            return custom_value_units(f, " morph rate",4);

        case VC_FormFilterStretch:
            f = powf(0.1f, (val - 32.0f) / 48.0f);
            return custom_value_units(f, " seq. scale factor",3);

        case VC_InstrumentVolume:
            return(custom_value_units(-60.0f*(1.0f-(int)val/96.0f),"dB",1));

        case VC_ADDVoiceVolume:
            if (val < 1)
                return "-inf dB";
            else
                return(custom_value_units(-60.0f*(1.0f-lrint(val)/127.0f),"dB",1));

        case VC_ADDVoiceDelay:
            if ((int) val == 0)
                return "No delay";
            f = (expf((val/127.0f) * logf(50.0f)) - 1) / 10;
            if (f >= 1)
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
            if (val < 0.2f)
                return "-inf dB";
            else
                return(custom_value_units((val-96.0f)/96.0f*40.0f,"dB",1));

        case VC_PartHumaniseDetune:
            s = "Detune: ";
            i = (int) val;
            if (i == 0)
                return s + "disabled";
            else
                return s + "between 0 and " + to_string(i) + " cents";

        case VC_PartHumaniseVelocity:
            s = "Attenuation: ";
            i = (int) val;
            if (i == 0)
                return s + "disabled";
            else
                return s + "between 0 and " + to_string(i) + "%";

        case VC_PanningRandom:
            return(custom_value_units(val / 63.0f * 100.0f,"%"));

        case VC_PanningStd:
            i = lrint(val);
            if (i==64)
                return("centered");
            else if (i<64)
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
            if (lrint(val) <= 1) /* 0 and 1 are both fixed */
                return "Fixed";
            else if (lrint(val) <= 64)
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
	    f = powf(100.0f, val / 64.0f);
            return variable_prec_units(f, "x", 3);

        case VC_SubHarmonicMagnitude:
            return custom_value_units(val / 127.0f * 100.0f, "%", 1);

        case VC_SubBandwidthScale:
            if ((int)val == 0)
                return "Constant";
	    f = val / 64.0f * 3.0f;
            return "Factor (100,10k): " +
                variable_prec_units(powf(10,f), "", 4) + ", " +
                variable_prec_units(powf(0.1,f), "x", 4);

        case VC_FilterVelocitySense: // this is also shown graphically
            if ((int)val==127)
                return("off");
            else
                return(custom_value_units(val,""));
            break;

        case VC_FXSysSend:
            if ((int)val==0)
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
            if (f < 0.5f)
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
            if (f < 0.5f)
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
            if (f < 0.5f)
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
    case VC_FormFilterClearness:
        w = 128;
        h = 128;
        break;
    default:
        w = 0;
        h = 0;
    }
}

inline void grid(int x, int y, int w, int h, int sections)
{
        fl_color(FL_GRAY);

        int j = 1;
        int gDist = h / sections;
        for (; j < sections; j++) /* Vertical */
        {
            fl_line(x, y - gDist * j, x + w, y - gDist * j);
        }

        gDist = w / sections;
        for (j = 1; j < sections; j++) /* Horizontal */
        {
            fl_line(x + gDist * j, y, x + gDist * j, y - h);
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
        grid(x0,y0,_w,_h, 4);
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
            for (i = 0; i < _w; i++)
            {
                x = (float)i / (float)_w;
                y = powf(x,p) * _h;
                fl_vertex((float)x0 + i, (float)y0 - y);
            }
            fl_end_line();
        }
        break;
    }
    case VC_FormFilterClearness:
    {
        p = powf(10.0f, (val - 32.0f) / 48.0f); //clearness param
        grid(x0,y0,_w,_h,10);
        fl_color(FL_BLUE);
        fl_begin_line();
        x = 0;
        float frac = 1.0f / (float)_w;
        for (i = 0; i < _w; i++)
        {
            y = (atanf((x * 2.0f - 1.0f) * p) / atanf(p) + 1.0f) * 0.5f * _h;
            fl_vertex((float)x0 + i, (float)y0 - y);
            x += frac;
        }
        fl_end_line();
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
        for (i = 0; i < 4; i++) /* 10x / 10%, 100x / 1% ... */
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

        for (i = 10;i != 0; i *= 10)
        {
            for (j = 2; j < 10; j++)
            {
                x = x0 + rx * (log10(i * j) - lg1020) + 1;
                fl_line(x, y0, x, y0 - _h);
                if (i * j >= 20000)
                {
                    i = 0;
                    break;
                }
            }
        }

        fl_font(fl_font(),10);
        for (i = 0; i < 4; i++) /* 20, 100, 1k, 10k */
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
        if ((int)val == 0)
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

string variable_prec_units(float v, const string& u, int maxPrec, bool roundup)
{
    int digits = 0, lim = (int) pow(10,maxPrec);
    float _v = fabsf(v);
    while (maxPrec > digits)
    {
        if (_v >= lim)
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

string custom_value_units(float v, const string& u, int prec)
{
    ostringstream oss;
    oss.setf(std::ios_base::fixed);
    oss.precision(prec);
    oss << v << " " << u;
    return(oss.str());
}

ValueType getLFOdepthType(int group)
{
    switch(group)
    {
        case TOPLEVEL::insertType::amplitude: return(VC_LFOdepthAmp);
        case TOPLEVEL::insertType::frequency: return(VC_LFOdepthFreq);
        case TOPLEVEL::insertType::filter: return(VC_LFOdepthFilter);
    }
    return(VC_plainValue);
}

ValueType getLFOFreqType(int bpmEnabled)
{
    return (bpmEnabled == 0) ? VC_LFOfreq : VC_LFOfreqBPM;
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

