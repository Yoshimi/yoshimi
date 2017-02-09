/*
    MidiDecode.cpp

    Copyright 2017 Will Godfrey

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

    Modified February 2017
*/

#include <iostream>
#include <bitset>
#include <unistd.h>
#include <list>
#include <string>
#include <unistd.h>

using namespace std;

#include "Interface/MidiDecode.h"
#include "MusicIO/MidiControl.h"
#include "Interface/InterChange.h"
#include "Misc/MiscFuncs.h"
#include "Misc/SynthEngine.h"

MidiDecode::MidiDecode(SynthEngine *_synth) :
    synth(_synth)
{
 //init

}


MidiDecode::~MidiDecode()
{
    //close
}


void MidiDecode::midiProcess(unsigned char par0, unsigned char par1, unsigned char par2, bool in_place)
{
    if (synth->isMuted())
        return; // nobody listening!
    unsigned char channel, note, velocity;
    int ctrltype, par;
    channel = par0 & 0x0F;
    unsigned int ev = par0 & 0xF0;
    par = 0;
    switch (ev)
    {
        case 0x80: // note-off
            note = par1;
            setMidiNoteOff(channel, note);
            break;

        case 0x90: // note-on
            note = par1;
            if (note)
            {
                velocity = par2;
                setMidiNote(channel, note, velocity);
            }
            else
                setMidiNoteOff(channel, note);
            break;

        case 0xA0: // key aftertouch
            ctrltype = C_channelpressure;
            /*
             * temporarily pretend it's a chanel aftertouch
             * need to work out how to use key numbers (par1)
             * for actual key pressure sensing.
             *
             * ctrltype = C_keypressure;
             */
            par = par2;
            setMidiController(channel, ctrltype, par, in_place);
            break;

        case 0xB0: // controller
            ctrltype = par1; // getMidiController(par1);
            par = par2;
            setMidiController(channel, ctrltype, par, in_place);
            break;

        case 0xC0: // program change
            ctrltype = C_programchange;
            par = par1;
            setMidiProgram(channel, par, in_place);
            break;

        case 0xD0: // channel aftertouch
            ctrltype = C_channelpressure;
            par = par1;
            setMidiController(channel, ctrltype, par, in_place);
            break;

        case 0xE0: // pitch bend
            ctrltype = C_pitchwheel;
            par = (par2 << 7) | par1;
            setMidiController(channel, ctrltype, par, in_place);
            break;

        default: // wot, more?
            if (par0 == 0xFF)
            {
                ctrltype = C_reset;
                if (!in_place) // never want to get this when freewheeling!
                    setMidiController(channel, ctrltype, 0);
            }
            else
                synth->getRuntime().Log("other event: " + asString((int)ev), 1);
            break;
    }
}


void MidiDecode::setMidiController(unsigned char ch, int ctrl, int param, bool in_place)
{
    int nLow;
    int nHigh;
    if (synth->getRuntime().monitorCCin)
    {
        string ctltype;
        switch (ctrl)
        {
            case C_NULL:
                ctltype = "Ignored";
                break;

            case C_reset:
                ctltype = "Master Reset";
                break;

            case C_programchange:
                ctltype = "program";
                break;

            case C_pitchwheel:
                ctltype = "Pitchwheel";
                break;

            case C_channelpressure:
                ctltype = "Ch Press";
                break;

            case C_keypressure:
                ctltype = "Key Press";
                break;

            default:
                ctltype = asString(ctrl);
                break;
        }
        synth->getRuntime().Log("Chan " + asString(((int) ch) + 1) + "   CC " + ctltype  + "   Value " + asString(param));
    }
    if (ctrl == C_reset)
    {
        synth->interchange.flagsWrite(0xf0000000);
        return;
    }
    if (ctrl == synth->getRuntime().midi_bank_root)
    {
        setMidiBankOrRootDir(param, in_place, true);
        return;
    }

    if (ctrl == synth->getRuntime().midi_bank_C)
    {
        setMidiBankOrRootDir(param, in_place);
        return;
    }

    if (ctrl == synth->getRuntime().midi_upper_voice_C)
    {
        // it's really an upper set program change
        setMidiProgram(ch, (param & 0x1f) | 0x80, in_place);
        return;
    }

    if (ctrl == C_nrpnL || ctrl == C_nrpnH)
    {
        if (ctrl == C_nrpnL)
        {
            if (synth->getRuntime().nrpnL != param)
            {
                synth->getRuntime().nrpnL = param;
                unsigned char type = synth->getRuntime().nrpnH;
                if (type >= 0x41 && type <= 0x43)
                { // shortform
                    if (param > 0x77) // disable it
                    {
                        synth->getRuntime().channelSwitchType = 0;
                        synth->getRuntime().channelSwitchCC = 0x80;
                    }
                    else
                    {
                        synth->getRuntime().channelSwitchType = type & 3; // row/column/loop
                        synth->getRuntime().channelSwitchCC = param;
                    }
                    return;
                }
                //synth->getRuntime().Log("Set nrpn LSB to " + asString(param));
            }
            nLow = param;
            nHigh = synth->getRuntime().nrpnH;
        }
        else // MSB
        {
            if (synth->getRuntime().nrpnH != param)
            {
                synth->getRuntime().nrpnH = param;
                //synth->getRuntime().Log("Set nrpn MSB to " + asString(param));
            if (param == 0x41) // set shortform
            {
                synth->getRuntime().nrpnL = 0x7f;
                return;
            }
            }
            nHigh = param;
            nLow = synth->getRuntime().nrpnL;
        }
        synth->getRuntime().dataL = 0x80; //  we've changed the NRPN
        synth->getRuntime().dataH = 0x80; //  so these are now invalid
        synth->getRuntime().nrpnActive = (nLow < 0x7f && nHigh < 0x7f);
//        synth->getRuntime().Log("Status nrpn " + asString(synth->getRuntime().nrpnActive));
        return;
    }

    if (synth->getRuntime().nrpnActive)
    {
        if (ctrl == C_dataI || ctrl == C_dataD)
        { // translate these to C_dataL and C_dataH
            int dHigh = synth->getRuntime().dataH;
            int dLow = synth->getRuntime().dataL;

            bool msbPar = (param >= 0x40);
            param &= 0x3f;
            if (ctrl == C_dataI)
            {
                if (msbPar)
                {
                    dHigh &= 0x7f; // clear disabled state
                    param += dHigh;
                    ctrl = C_dataH; // change controller type
                }
                else
                {
                    dLow &= 0x7f; // clear disabled state
                    param += dLow;
                    ctrl = C_dataL; // change controller type
                }
                if (param > 0x7f)
                    param = 0x7f;
            }
            else
            { // data decrement
                if (msbPar)
                {
                    param = dHigh - param;
                    ctrl = C_dataH; // change controller type
                }
                else
                {
                    param = dLow - param;
                    ctrl = C_dataL; // change controller type
                }
                if (param < 0)
                    param = 0;
            }
        }

        if (ctrl == C_dataL || ctrl == C_dataH)
        {
            nrpnProcessData(ch, ctrl, param, in_place);
            return;
        }
    }

    unsigned char vecChan;
    if (synth->getRuntime().channelSwitchType == 1)
        vecChan = synth->getRuntime().channelSwitchValue;
        // force vectors to obey channel switcher
    else
        vecChan = ch;
    if (synth->getRuntime().nrpndata.vectorEnabled[vecChan] && synth->getRuntime().NumAvailableParts > NUM_MIDI_CHANNELS)
    { // vector control is direct to parts
        if (nrpnRunVector(vecChan, ctrl, param))
            return; // **** test this it may be wrong!
    }
    // pick up a drop-through if CC doesn't match the above
    if (ctrl == C_resetallcontrollers && synth->getRuntime().ignoreResetCCs == true)
    {
        //synth->getRuntime().Log("Reset controllers ignored");
        return;
    }

    /*
     * set / run midi learn will pass 'in_place' so entire operation
     * can be done in MidiLearn.cpp
     * return true if blocking further calls
     *
     * need to work out some kind of loop-back so optional
     * vector control CCs can be picked up.
     *
     * Some controller valuse are >= 640 so they will be ignored by
     * later calls, but are passed as 128+ for this call.
     * Pitch wheel is 640 and is 14 bit. It sets bit 1 of 'category'
     */
    if (synth->midilearn.runMidiLearn(param, ctrl & 255, ch, in_place | ((ctrl == 640) << 1)))
        return;

    if (ctrl == C_breath)
    {
        synth->SetController(ch, C_volume, param);
        ctrl = C_filtercutoff;
    }
    else
        /*
         * This is done here instead of in 'setMidi' so MidiLearn
         * handles all 14 bit values the same.
         */
        if (ctrl == C_pitchwheel)
            param -= 8192;

    // do what's left!
    synth->SetController(ch, ctrl, param);
}


bool MidiDecode::nrpnRunVector(unsigned char ch, int ctrl, int param)
{
    int Xopps = synth->getRuntime().nrpndata.vectorXfeatures[ch];
    int Yopps = synth->getRuntime().nrpndata.vectorYfeatures[ch];
    int p_rev = 127 - param;
    int swap1;
    int swap2;
    unsigned char type;

    if (ctrl == synth->getRuntime().nrpndata.vectorXaxis[ch])
    {
        if (Xopps & 1) // fixed as volume
        {
            synth->SetController(ch | 0x80, C_volume,127 - (p_rev * p_rev / 127));
            synth->SetController(ch | 0x90, C_volume, 127 - (param * param / 127));
        }
        if (Xopps & 2) // default is pan
        {
            type = synth->getRuntime().nrpndata.vectorXcc2[ch];
            swap1 = (Xopps & 0x10) | 0x80;
            swap2 = swap1 ^ 0x10;
            synth->SetController(ch | swap1, type, param);
            synth->SetController(ch | swap2, type, p_rev);
        }
        if (Xopps & 4) // default is 'brightness'
        {
            type = synth->getRuntime().nrpndata.vectorXcc4[ch];
            swap1 = ((Xopps >> 1) & 0x10) | 0x80;
            swap2 = swap1 ^ 0x10;
            synth->SetController(ch | swap1, type, param);
            synth->SetController(ch | swap2, type, p_rev);
        }
        if (Xopps & 8) // default is mod wheel
        {
            type = synth->getRuntime().nrpndata.vectorXcc8[ch];
            swap1 = ((Xopps >> 2) & 0x10) | 0x80;
            swap2 = swap1 ^ 0x10;
            synth->SetController(ch | swap1, type, param);
            synth->SetController(ch | swap2, type, p_rev);
        }
        return true;
    }
    else if (ctrl == synth->getRuntime().nrpndata.vectorYaxis[ch])
    { // if Y hasn't been set these commands will be ignored
        if (Yopps & 1) // fixed as volume
        {
            synth->SetController(ch | 0xa0, C_volume,127 - (p_rev * p_rev / 127));
            synth->SetController(ch | 0xb0, C_volume, 127 - (param * param / 127));
        }
        if (Yopps & 2) // default is pan
        {
            type = synth->getRuntime().nrpndata.vectorYcc2[ch];
            swap1 = (Yopps & 0x10) | 0xa0;
            swap2 = swap1 ^ 0x10;
            synth->SetController(ch | swap1, type, param);
            synth->SetController(ch | swap2, type, p_rev);
        }
        if (Yopps & 4) // default is 'brightness'
        {
            type = synth->getRuntime().nrpndata.vectorYcc4[ch];
            swap1 = ((Yopps >> 1) & 0x10) | 0xa0;
            swap2 = swap1 ^ 0x10;
            synth->SetController(ch | swap1, type, param);
            synth->SetController(ch | swap2, type, p_rev);
        }
        if (Yopps & 8) // default is mod wheel
        {
            type = synth->getRuntime().nrpndata.vectorYcc8[ch];
            swap1 = ((Yopps >> 2) & 0x10) | 0xa0;
            swap2 = swap1 ^ 0x10;
            synth->SetController(ch | swap1, type, param);
            synth->SetController(ch | swap2, type, p_rev);
        }
        return true;
    }
    return false;
}


void MidiDecode::nrpnProcessData(unsigned char chan, int type, int par, bool in_place)
{
    int nHigh = synth->getRuntime().nrpnH;
    int nLow = synth->getRuntime().nrpnL;
/*    if (nLow < nHigh && (nHigh == 4 || nHigh == 8 ))
    {
        if (type == C_dataL)
            synth->getRuntime().dataL = par;
        else
             synth->getRuntime().dataH = par;
        synth->SetZynControls();
        return;
    }*/

    bool noHigh = (synth->getRuntime().dataH > 0x7f);
    if (type == C_dataL)
    {
        synth->getRuntime().dataL = par;
//        synth->getRuntime().Log("Data LSB    value " + asString(par));
        if (noHigh)
            return;
    }
    if (type == C_dataH)
    {
        synth->getRuntime().dataH = par;
//        synth->getRuntime().Log("Data MSB    value " + asString(par));
        if (noHigh && synth->getRuntime().dataL <= 0x7f)
            par = synth->getRuntime().dataL;
        else
            return; // we're currently using MSB as parameter not a value
    }
    /*
     * All the above runaround performance is to deal with a data LSB
     * arriving either before or after the MSB and immediately after
     * a new NRPN has been set. After this, running data values expect
     * MSB sub parameter before LSB value until the next full NRPN.
     */
    int dHigh = synth->getRuntime().dataH;

    /*synth->getRuntime().Log("   nHigh " + asString((int)nHigh)
                          + "   nLow " + asString((int)nLow)
                          + "   dataH " + asString((int)synth->getRuntime().dataH)
                          + "   dataL " + asString((int)synth->getRuntime().dataL)
                          + "   chan " + asString((int)chan)
                          + "   type"  + asString((int)type)
                          + "   par " + asString((int)par));
    */

    // midi learn must come before everything else
    if (synth->midilearn.runMidiLearn(dHigh << 7 | par, 0x10000 | (nHigh << 7) | nLow , chan, in_place |2))
        return;

    if (nLow < nHigh && (nHigh == 4 || nHigh == 8 ))
    {
        if (type == C_dataL)
            synth->getRuntime().dataL = par;
        else
             synth->getRuntime().dataH = par;
        synth->SetZynControls();
        return;
    }

    if (nHigh != 64 && nLow < 0x7f)
    {
        synth->getRuntime().Log("Go away NRPN 0x" + asHexString(nHigh >> 1) + asHexString(nLow) +" We don't know you!");
        //done this way to ensure we see both bytes even if nHigh is zero
        synth->getRuntime().nrpnActive = false; // we were sent a turkey!
        return;
    }

    if (nLow == 0) // direct part change
        nrpnDirectPart(dHigh, par);

    else if (nLow == 1) // it's vector control
        nrpnSetVector(dHigh, chan, par);

    else if (nLow == 2) // system settings
        synth->SetSystemValue(dHigh, par);
}


void MidiDecode::nrpnDirectPart(int dHigh, int par)
{
    switch (dHigh)
    {
        case 0: // set part number
            if (par < synth->getRuntime().NumAvailableParts)
            {
                synth->getRuntime().dataL = par;
                synth->getRuntime().nrpndata.Part = par;
            }
            else // It's bad. Kill it
            {
                synth->getRuntime().dataL = 128;
                synth->getRuntime().dataH = 128;
            }
            break;

        case 1: // Program Change
            setMidiProgram(synth->getRuntime().nrpndata.Part | 0x80, par);
            break;

        case 2: // Set controller number
            synth->getRuntime().nrpndata.Controller = par;
            synth->getRuntime().dataL = par;
            break;

        case 3: // Set controller value
            synth->SetController(synth->getRuntime().nrpndata.Part | 0x80, synth->getRuntime().nrpndata.Controller, par);
            break;

        case 4: // Set part's channel number
            synth->SetPartChan(synth->getRuntime().nrpndata.Part, par);
            break;

        case 5: // Set part's audio destination
            if (par > 0 and par < 4)
                synth->SetPartDestination(synth->getRuntime().nrpndata.Part, par);
            break;

        case 64:
            synth->SetPartShift(synth->getRuntime().nrpndata.Part, par);
            break;
    }
}


void MidiDecode:: nrpnSetVector(int dHigh, unsigned char chan,  int par)
{

    if (synth->vectorInit(dHigh, chan, par))
        return;

    switch (dHigh)
    {
        /*
         * these have to go through the program change
         * thread otherwise they could block following
         * MIDI messages
         */
        case 4:
            setMidiProgram(chan | 0x80, par);
            break;

        case 5:
            setMidiProgram(chan | 0x90, par);
            break;

        case 6:
            setMidiProgram(chan | 0xa0, par);
            break;

        case 7:
            setMidiProgram(chan | 0xb0, par);
            break;

        default:
            synth->vectorSet(dHigh, chan, par);
            break;
    }
}


//bank change and root dir change change share the same thread
//to make changes consistent
void MidiDecode::setMidiBankOrRootDir(unsigned int bank_or_root_num, bool in_place, bool setRootDir)
{
    if (setRootDir)
    {
        if (bank_or_root_num == synth->getBankRef().getCurrentRootID())
            return; // nothing to do!
    }
    else
        if (bank_or_root_num == synth->getBankRef().getCurrentBankID())
            return; // still nothing to do!
    if (in_place)
        setRootDir ? synth->SetBankRoot(bank_or_root_num) : synth->SetBank(bank_or_root_num);
    else
    {
        if (setRootDir)
            synth->writeRBP(1 ,bank_or_root_num,0);
        else
            synth->writeRBP(2 ,bank_or_root_num,0);
    }
}


void MidiDecode::setMidiProgram(unsigned char ch, int prg, bool in_place)
{
    int partnum;
    if (ch < NUM_MIDI_CHANNELS)
        partnum = ch;
    else
        partnum = ch & 0x7f; // this is for direct part access instead of channel
    if (partnum >= synth->getRuntime().NumAvailableParts)
        return;
    if (synth->getRuntime().EnableProgChange)
    {
        if (in_place)
        {
            //synth->getRuntime().Log("Freewheeling");
            synth->SetProgram(ch, prg);
        }
        else
        {
            //synth->getRuntime().Log("Normal");
            synth->writeRBP(3, ch ,prg);
        }
    }
}


void MidiDecode::setMidiNote(unsigned char channel, unsigned char note,
                           unsigned char velocity)
{
    synth->NoteOn(channel, note, velocity);
}


void MidiDecode::setMidiNoteOff(unsigned char channel, unsigned char note)
{
    synth->NoteOff(channel, note);
}

