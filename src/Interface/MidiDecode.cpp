/*
    MidiDecode.cpp

    Copyright 2017-2020, Will Godfrey
    Copyright 2021, Will Godfrey, Rainer Hans Liffers

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

#include <iostream>
#include <bitset>
#include <unistd.h>
#include <list>
#include <string>
#include <unistd.h>

//#include <sys/time.h>

#include "Interface/MidiDecode.h"
#include "Interface/InterChange.h"
#include "Misc/SynthEngine.h"
#include "Misc/FormatFuncs.h"

using func::asString;
using func::asHexString;


MidiDecode::MidiDecode(SynthEngine *_synth) :
    synth(_synth)
{
}


MidiDecode::~MidiDecode()
{
}


void MidiDecode::midiProcess(unsigned char par0, unsigned char par1, unsigned char par2, bool in_place, bool inSync)
{
    bool a = inSync; inSync = a; // suppress warning (may need this later)
    unsigned char channel; // , note, velocity;
    int ctrltype, par;
    channel = par0 & 0x0F;
    unsigned int ev = par0 & 0xF0;
    par = 0;
    switch (ev)
    {
        case 0xA0: // key aftertouch
            ctrltype = MIDI::CC::keyPressure;
            par = par1 | (par2<< 8);
            setMidiController(channel, ctrltype, par, in_place);
            break;

        case 0xB0: // controller
            ctrltype = par1;
            par = par2;
            setMidiController(channel, ctrltype, par, in_place);
            break;

        case 0xC0: // program change
            ctrltype = MIDI::CC::programchange;
            par = par1;
            setMidiProgram(channel, par, in_place);
            break;

        case 0xD0: // channel aftertouch
            ctrltype = MIDI::CC::channelPressure;
            par = par1;
            setMidiController(channel, ctrltype, par, in_place);
            break;

        case 0xE0: // pitch bend
            ctrltype = MIDI::CC::pitchWheel;
            par = (par2 << 7) | par1;
            setMidiController(channel, ctrltype, par, in_place);
            break;

        default: // wot, more?
            if (synth->getRuntime().monitorCCin)
                synth->getRuntime().Log("Unsupported event: 0x" + asHexString(int(par0)));
            break;
    }
}


void MidiDecode::setMidiController(unsigned char ch, int ctrl, int param, bool in_place, bool inSync)
{
    if (synth->getRuntime().monitorCCin)
    {
        std::string ctltype;
        switch (ctrl)
        {
            case MIDI::CC::null:
                ctltype = "Ignored";
                break;

            case MIDI::CC::programchange:
                ctltype = "program";
                break;

            case MIDI::CC::pitchWheel:
                ctltype = "Pitchwheel";
                break;

            case MIDI::CC::channelPressure:
                ctltype = "Ch Press";
                break;

            case MIDI::CC::keyPressure:
                ctltype = "Key Press";
                break;

            default:
                ctltype = asString(ctrl);
                break;
        }
        synth->getRuntime().Log("Chan " + asString(((int) ch) + 1) + "   CC " + ctltype  + "   Value " + asString(param));
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

    if (synth->getRuntime().enable_NRPN)
    {
        if (nrpnDecode(ch, ctrl, param, in_place))
            return;
    }

    unsigned char vecChan;
    if (synth->getRuntime().channelSwitchType == 1)
        vecChan = synth->getRuntime().channelSwitchValue;
        // force vectors to obey channel switcher
    else
        vecChan = ch;
    if (synth->getRuntime().vectordata.Enabled[vecChan] && synth->getRuntime().NumAvailableParts > NUM_MIDI_CHANNELS)
    { // vector control is direct to parts
        if (nrpnRunVector(vecChan, ctrl, param, inSync))
            return;
    }
    // pick up a drop-through if CC doesn't match the above
    if (ctrl == MIDI::CC::resetAllControllers && synth->getRuntime().ignoreResetCCs == true)
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
     * Some controller values are >= 640 so they will be ignored by
     * later calls, but are passed as 128+ for this call.
     */
/*
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
*/
    if (synth->midilearn.runMidiLearn(param, ctrl & 0xff, ch, in_place))
        return; // blocking while learning
/*
    gettimeofday(&tv2, NULL);
    if (tv1.tv_usec > tv2.tv_usec)
    {
        tv2.tv_sec--;
        tv2.tv_usec += 1000000;
    }
    int actual = (tv2.tv_sec - tv1.tv_sec) *1000000 + (tv2.tv_usec - tv1.tv_usec);
    std::cout << "Delay " << std::to_string(actual) << "uS" << std::endl;
*/
    /*
    * This is done here instead of in 'setMidi' so MidiLearn
    * handles all 14 bit values the same.
    */
    if (ctrl == MIDI::CC::pitchWheel)
    {
        param -= 8192;
        sendMidiCC(inSync, ch, ctrl, param);
        return;
    }
    // do what's left!
    if (ctrl < 0x80 || ctrl == MIDI::CC::channelPressure || ctrl == MIDI::CC::keyPressure) // don't want to pick up strays
        sendMidiCC(inSync, ch, ctrl, param);
}


void MidiDecode::sendMidiCC(bool inSync, unsigned char chan, int type, short int par)
{
    if (inSync) // no CLI or GUI updates needed
    {
        //std::cout << "CC inSync" << std::endl;
        synth->SetController(chan, type, par);
        return;
    }

    //std::cout << "CC buffered" << std::endl;
    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));
    putData.data.value = par;
    putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
    putData.data.source = TOPLEVEL::action::toAll;
    putData.data.control = MIDI::control::controller;
    putData.data.part = TOPLEVEL::section::midiIn;
    putData.data.kit = chan;
    putData.data.engine = type;
    synth->midilearn.writeMidi(&putData, false);
}

/*
 * nrpnDecode parameters are only ever seen by other MIDI controls
 * so don't need buffering.
 *
 * However nrpnProcessData is handling some 'live' data and is
 * buffered where needed.
 */

bool MidiDecode::nrpnDecode(unsigned char ch, int ctrl, int param, bool in_place)
{
    int nLow;
    int nHigh;
    if (ctrl == MIDI::CC::nrpnLSB || ctrl == MIDI::CC::nrpnMSB)
    {
        if (ctrl == MIDI::CC::nrpnLSB)
        {
            nHigh = synth->getRuntime().nrpnH;
            nLow = param;
            synth->getRuntime().nrpnL = nLow;
            if (nHigh == 0x44 && (nLow == 0x44 || nLow == 0x45)) // shutdown controls
            {
                if (nLow == 0x45)
                    synth->getRuntime().exitType = FORCED_EXIT;
                synth->getRuntime().runSynth = false;
                return true; // bye bye everyone
            }
            if (nHigh == 0x41 || nHigh == 0x42) // Solo controls
            {
                if (nHigh == 0x41) // type - must set this first
                {
                    if (nLow > MIDI::SoloType::Channel)
                        nLow = MIDI::SoloType::Disabled;
                    synth->getRuntime().channelSwitchType = nLow; // row/column/loop/channel
                }
                else // CC
                {
                    if (nLow < MIDI::CC::allSoundOff)
                        synth->getRuntime().channelSwitchCC = nLow;
                }
                return true;
            }
        }
        else // MSB
        {
            nHigh = param;
            nLow = synth->getRuntime().nrpnL;
            synth->getRuntime().nrpnH = nHigh;
            //synth->getRuntime().Log("Set nrpn MSB to " + asString(nHigh));
            if (nHigh >= 0x41 && nHigh <= 0x44) // set shortform
            {
                synth->getRuntime().nrpnL = 0x7f;
                return true;
            }
        }
        synth->getRuntime().dataL = 0x80; //  we've changed the NRPN
        synth->getRuntime().dataH = 0x80; //  so these are now invalid
        synth->getRuntime().nrpnActive = (nLow < 0x7f && nHigh < 0x7f);
//        synth->getRuntime().Log("Status nrpn " + asString(synth->getRuntime().nrpnActive));
        return true;
    }

    if (synth->getRuntime().nrpnActive)
    {
        if (ctrl == MIDI::CC::dataINC || ctrl == MIDI::CC::dataDEC)
        { // translate these to MIDI::CC::dataLSB and MIDI::CC::dataMSB
            int dHigh = synth->getRuntime().dataH;
            int dLow = synth->getRuntime().dataL;

            bool msbPar = (param >= 0x40);
            param &= 0x3f;
            if (ctrl == MIDI::CC::dataINC)
            {
                if (msbPar)
                {
                    dHigh &= 0x7f; // clear disabled state
                    param += dHigh;
                    ctrl = MIDI::CC::dataMSB; // change controller type
                }
                else
                {
                    dLow &= 0x7f; // clear disabled state
                    param += dLow;
                    ctrl = MIDI::CC::dataLSB; // change controller type
                }
                if (param > 0x7f)
                    param = 0x7f;
            }
            else
            { // data decrement
                if (msbPar)
                {
                    param = dHigh - param;
                    ctrl = MIDI::CC::dataMSB; // change controller type
                }
                else
                {
                    param = dLow - param;
                    ctrl = MIDI::CC::dataLSB; // change controller type
                }
                if (param < 0)
                    param = 0;
            }
        }

        if (ctrl == MIDI::CC::dataLSB || ctrl == MIDI::CC::dataMSB)
        {
            nrpnProcessData(ch, ctrl, param, in_place);
            return true;
        }
    }
    return false;
}


bool MidiDecode::nrpnRunVector(unsigned char ch, int ctrl, int param, bool inSync)
{
    int Xopps = synth->getRuntime().vectordata.Xfeatures[ch];
    int Yopps = synth->getRuntime().vectordata.Yfeatures[ch];
    int p_rev = 0x7f - param;
    int swap1;
    int swap2;
    unsigned char type;

    if (ctrl == synth->getRuntime().vectordata.Xaxis[ch])
    {
        if (Xopps & 1) // fixed as volume
        {
            sendMidiCC(inSync, ch | 0x80, MIDI::CC::volume, 0x7f - (p_rev * p_rev / 0x7f));
            sendMidiCC(inSync, ch | 0x90, MIDI::CC::volume, 0x7f - (param * param / 0x7f));
        }
        if (Xopps & 2) // default is pan
        {
            type = synth->getRuntime().vectordata.Xcc2[ch];
            swap1 = (Xopps & 0x10) | 0x80;
            swap2 = swap1 ^ 0x10;
            sendMidiCC(inSync, ch | swap1, type, param);
            sendMidiCC(inSync, ch | swap2, type, p_rev);
        }
        if (Xopps & 4) // default is 'brightness'
        {
            type = synth->getRuntime().vectordata.Xcc4[ch];
            swap1 = ((Xopps >> 1) & 0x10) | 0x80;
            swap2 = swap1 ^ 0x10;
            sendMidiCC(inSync, ch | swap1, type, param);
            sendMidiCC(inSync, ch | swap2, type, p_rev);
        }
        if (Xopps & 8) // default is mod wheel
        {
            type = synth->getRuntime().vectordata.Xcc8[ch];
            swap1 = ((Xopps >> 2) & 0x10) | 0x80;
            swap2 = swap1 ^ 0x10;
            sendMidiCC(inSync, ch | swap1, type, param);
            sendMidiCC(inSync, ch | swap2, type, p_rev);
        }
        return true;
    }
    else if (ctrl == synth->getRuntime().vectordata.Yaxis[ch])
    { // if Y hasn't been set these commands will be ignored
        if (Yopps & 1) // fixed as volume
        {
            sendMidiCC(inSync, ch | 0xa0, MIDI::CC::volume, 0x7f - (p_rev * p_rev / 0x7f));
            sendMidiCC(inSync, ch | 0xb0, MIDI::CC::volume, 0x7f - (param * param / 0x7f));
        }
        if (Yopps & 2) // default is pan
        {
            type = synth->getRuntime().vectordata.Ycc2[ch];
            swap1 = (Yopps & 0x10) | 0xa0;
            swap2 = swap1 ^ 0x10;
            sendMidiCC(inSync, ch | swap1, type, param);
            sendMidiCC(inSync, ch | swap2, type, p_rev);
        }
        if (Yopps & 4) // default is 'brightness'
        {
            type = synth->getRuntime().vectordata.Ycc4[ch];
            swap1 = ((Yopps >> 1) & 0x10) | 0xa0;
            swap2 = swap1 ^ 0x10;
            sendMidiCC(inSync, ch | swap1, type, param);
            sendMidiCC(inSync, ch | swap2, type, p_rev);
        }
        if (Yopps & 8) // default is mod wheel
        {
            type = synth->getRuntime().vectordata.Ycc8[ch];
            swap1 = ((Yopps >> 2) & 0x10) | 0xa0;
            swap2 = swap1 ^ 0x10;
            sendMidiCC(inSync, ch | swap1, type, param);
            sendMidiCC(inSync, ch | swap2, type, p_rev);
        }
        return true;
    }
    return false;
}


void MidiDecode::nrpnProcessData(unsigned char chan, int type, int par, bool in_place)
{
    int nHigh = synth->getRuntime().nrpnH;
    int nLow = synth->getRuntime().nrpnL;
    bool noHigh = (synth->getRuntime().dataH > 0x7f);
    if (type == MIDI::CC::dataLSB)
    {
        synth->getRuntime().dataL = par;
//        synth->getRuntime().Log("Data LSB    value " + asString(par));
        if (noHigh)
            return;
    }
    if (type == MIDI::CC::dataMSB)
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
                          + "   type "  + asString((int)type)
                          + "   par " + asString((int)par));
    */

    // For NRPNs midi learn must come before everything else
   if (synth->midilearn.runMidiLearn(dHigh << 7 | par, MIDI::CC::identNRPN | (nHigh << 7) | nLow , chan, in_place))
        return; // blocking while learning

    if (nLow < nHigh && (nHigh == 4 || nHigh == 8 ))
    {
        if (type == MIDI::CC::dataLSB)
            synth->getRuntime().dataL = par;
        else
             synth->getRuntime().dataH = par;
        if (synth->getRuntime().dataL <= 0x7f && synth->getRuntime().dataL <= 0x7f)
            synth->SetZynControls(in_place);
        return;
    }

    if (nHigh == 0x60)
        if (nrpnProcessHistory(nLow, dHigh, par, in_place))
            return;

    if (nHigh != 64 && nLow < 0x7f)
    {
        synth->getRuntime().Log("Go away NRPN 0x" + asHexString(nHigh) + " " + asHexString(nLow) +" We don't know you!");
        // done this way to ensure we see both bytes even if nHigh is zero
        synth->getRuntime().nrpnActive = false; // we were sent a turkey!
        return;
    }

    if (nLow == 0) // direct part change
        nrpnDirectPart(dHigh, par);

    else if (nLow == 1) // it's vector control
        nrpnSetVector(dHigh, chan, par);

    else if (nLow == 2) // system settings
        synth->SetSystemValue(dHigh, par); // *** CHANGE
}


bool MidiDecode::nrpnProcessHistory(unsigned char nLow, unsigned char dHigh, unsigned char dLow, bool in_place)
{
    if (nLow > TOPLEVEL::XML::MLearn)
        return false;
    if (dLow >= MAX_HISTORY)
        return true;// not an error but not wanted anyway!

    if (in_place)
    { // not done yet
        return true;
    }
    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));
    putData.data.type = TOPLEVEL::type::Integer;
    putData.data.source = TOPLEVEL::action::fromMIDI;
    putData.data.control = MAIN::control::loadFileFromList;
    putData.data.part = TOPLEVEL::section::main;
    putData.data.kit = nLow;
    putData.data.engine = dLow;
    if (nLow == TOPLEVEL::XML::Vector && dHigh < NUM_MIDI_CHANNELS)
        putData.data.insert = dHigh; // othewise set by file
    else if (nLow == TOPLEVEL::XML::Instrument)
    {
        unsigned char tmp = synth->getRuntime().vectordata.Part;
        if (dHigh >= NUM_MIDI_PARTS && tmp < NUM_MIDI_PARTS)
            putData.data.insert = tmp; // last seen by part NRPN
        else if (dHigh < NUM_MIDI_PARTS)
            putData.data.insert = dHigh;
        else
            return true; // not an error but undefined
    }
    synth->interchange.fromMIDI.write(putData.bytes);
    return true;
}


void MidiDecode::nrpnDirectPart(int dHigh, int par)
{
    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));

    switch (dHigh)
    {
        case 0: // set part number to use for later calls
            if (par < synth->getRuntime().NumAvailableParts)
            {
                synth->getRuntime().dataL = par;
                synth->getRuntime().vectordata.Part = par;
            }
            else // It's bad. Kill it
            {
                synth->getRuntime().dataL = 0x80;
                synth->getRuntime().dataH = 0x80;
            }
            break;

        case 1: // Program Change
            setMidiProgram(synth->getRuntime().vectordata.Part | 0x80, par);
            break;

        case 2: // Set controller number
            synth->getRuntime().vectordata.Controller = par;
            synth->getRuntime().dataL = par;
            break;

        case 3: // Set controller value
            setMidiController(synth->getRuntime().vectordata.Part | 0x80, synth->getRuntime().vectordata.Controller, par, false);
            break;

        case 4: // Set part's channel number
            putData.data.value = par;
            putData.data.control = 5;
            putData.data.part = synth->getRuntime().vectordata.Part;
            break;

        case 5: // Set part's audio destination
            if (par > 0 and par < 4)
            putData.data.value = par;
            putData.data.control = 120;
            putData.data.part = synth->getRuntime().vectordata.Part;
            putData.data.parameter = 192;
            break;

        case 64: // key shift
            par -= 64;
            if (par < MIN_KEY_SHIFT)
                par = MIN_KEY_SHIFT;
            else if (par > MAX_KEY_SHIFT)
                par = MAX_KEY_SHIFT;
            putData.data.value = par;
            putData.data.control = 35;
            putData.data.part = synth->getRuntime().vectordata.Part;
            break;
        default:
            return;
            break;
    }
    if (dHigh < 4)
        return;
    //std::cout << "part " << int(putData.data.part) << "  Chan " << int(par) << std::endl;
    putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
    putData.data.source = TOPLEVEL::action::toAll;
    synth->midilearn.writeMidi(&putData, false);
}


void MidiDecode:: nrpnSetVector(int dHigh, unsigned char chan,  int par)
{

    if (synth->vectorInit(dHigh, chan, par))
        return;

    switch (dHigh)
    {
        /*
         * These have to go through the program change
         * thread otherwise they could block following
         * MIDI messages.
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
        if (bank_or_root_num == synth->getRuntime().currentRoot)
            return; // nothing to do!
    }
    else
    {
        if (bank_or_root_num == synth->getRuntime().currentBank)
            return; // still nothing to do!
    }

    if (in_place)
    {
        if (!setRootDir)
            synth->setRootBank(UNUSED, bank_or_root_num, false);
        else
            synth->setRootBank(bank_or_root_num, UNUSED, false);
        return;
    }

    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));
    putData.data.value = 0xff;
    putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
    putData.data.source = TOPLEVEL::action::toAll;
    putData.data.control = MIDI::control::bankChange;
    putData.data.part = TOPLEVEL::section::midiIn;
    putData.data.kit = 0;
    putData.data.parameter = 0xc0;

    if (setRootDir)
        putData.data.insert = bank_or_root_num;
    else
        putData.data.engine = bank_or_root_num;

    synth->midilearn.writeMidi(&putData, false);
}


void MidiDecode::setMidiProgram(unsigned char ch, int prg, bool in_place)
{
    if (!synth->getRuntime().EnableProgChange)
        return;
    int maxparts = synth->getRuntime().NumAvailableParts;
    if (ch >= maxparts)
        return;

    CommandBlock putData;
    memset(&putData, 0xff, sizeof(putData));
    putData.data.value = prg;
    putData.data.type = TOPLEVEL::type::Write | TOPLEVEL::type::Integer;
    putData.data.source = TOPLEVEL::action::toAll;
    putData.data.control = MIDI::control::instrument;
    putData.data.part = TOPLEVEL::section::midiIn;
    //putData.data.parameter = 0xc0;

    /*
     * This is a bit slow as we send each part individually
     * but it is the simplest way to ensure partonoff doesn't
     * get out of step.
     *
     * Changes won't normally happen while MIDI is incoming
     * on the same channel.
     */
    if (ch < NUM_MIDI_CHANNELS)
    {
        for (int npart = 0; npart < maxparts; ++ npart)
        {
            if (ch == synth->part[npart]->Prcvchn)
            {
                putData.data.kit = npart;
                if (in_place)
                {
                    synth->partonoffLock(npart, -1);
                    synth->setProgramFromBank(&putData, false);
                }
                else
                {
                    synth->midilearn.writeMidi(&putData, false);
                }
            }
        }
        return;
    }
    else
    {
        putData.data.kit = ch & 0x3f;
        if (in_place)
        {
            synth->partonoffLock(ch, -1);
            synth->setProgramFromBank(&putData, false);
        }
        else
            synth->midilearn.writeMidi(&putData, false);
    }
}


