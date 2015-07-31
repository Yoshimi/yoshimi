/*
    MusicIO.cpp

    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2015, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <cstring>
#include <fftw3.h>

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MidiControl.h"
#include "MusicIO/MusicIO.h"

#include <unistd.h>
#include <iostream>

MusicIO::MusicIO(SynthEngine *_synth) :
    interleaved(NULL),
    rtprio(25),
    synth(_synth),
    pBankOrRootDirThread(0)
{
    memset(zynLeft, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    memset(zynRight, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    memset(&prgChangeCmd, 0, sizeof(prgChangeCmd));
}

MusicIO::~MusicIO()
{
    pthread_t tmpBankThread = 0;
    pthread_t tmpPrgThread = 0;
    void *threadRet = NULL;
    tmpBankThread = __sync_fetch_and_add(&pBankOrRootDirThread, 0);
    if(tmpBankThread != 0)
        pthread_join(tmpBankThread, &threadRet);
    for(int i = 0; i < NUM_MIDI_PARTS; ++i)
    {
        threadRet = NULL;
        tmpPrgThread = __sync_fetch_and_add(&prgChangeCmd [i].pPrgThread, 0);
        if(tmpPrgThread != 0)
            pthread_join(tmpPrgThread, &threadRet);
    }

    for (int npart = 0; npart < (NUM_MIDI_PARTS + 1); ++npart)
    {
        if (zynLeft[npart])
        {
            fftwf_free(zynLeft[npart]);
            zynLeft[npart] = NULL;
        }
        if (zynRight[npart])
        {
            fftwf_free(zynRight[npart]);
            zynRight[npart] = NULL;
        }
    }
}


int MusicIO::getMidiController(unsigned char b)
{
    int ctl = C_NULL;
    switch (b)
    {
        case 0: // Bank Select MSB
            ctl = C_bankselectmsb;
            break;        
        case 1: // Modulation Wheel
            ctl = C_modwheel;
            break;
        case 6: // data MSB
            ctl = C_dataH;
            break;
        case 7: // Volume
            ctl = C_volume;
            break;
        case 10: // Panning
            ctl = C_panning;
            break;
            case 32: // Bank Select LSB
            ctl = C_bankselectlsb;
            break;        
        case 11: // Expression
            ctl = C_expression;
            break;
        case 38: // data LSB
            ctl = C_dataL;
            break;
        case 64: // Sustain pedal
            ctl = C_sustain;
            break;
        case 65: // Portamento
            ctl = C_portamento;
            break;
        case 71: // Filter Q (Sound Timbre)
            ctl = C_filterq;
            break;
        case 74: // Filter Cutoff (Brightness)
            ctl = C_filtercutoff;
            break;
        case 75: // BandWidth
            ctl = C_bandwidth;
            break;
        case 76: // FM amplitude
            ctl = C_fmamp;
            break;
        case 77: // Resonance Center Frequency
            ctl = C_resonance_center;
            break;
        case 78: // Resonance Bandwith
            ctl = C_resonance_bandwidth;
            break;
        case 96: // data increment
            ctl = C_dataI;
            break;
        case 97: // data decrement
            ctl = C_dataD;
            break;
        case 98: // NRPN LSB
            ctl = C_nrpnL;
            break;
        case 99: // NRPN MSB
            ctl = C_nrpnH;
            break;
        case 120: // All Sounds OFF
            ctl = C_allsoundsoff;
            break;
        case 121: // Reset All Controllers
            ctl = C_resetallcontrollers;
            break;
        case 123: // All Notes OFF
            ctl = C_allnotesoff;
            break;
        default: // an unrecognised controller!
            ctl = C_NULL;
            break;
    }
    return ctl;
}


void MusicIO::setMidiController(unsigned char ch, int ctrl, int param, bool in_place)
{
    int nLow;
    int nHigh;
    if (ctrl == synth->getRuntime().midi_bank_root)
        setMidiBankOrRootDir(param, in_place, true);
    else if (ctrl == synth->getRuntime().midi_bank_C)
        setMidiBankOrRootDir(param, in_place);
    else if (ctrl == synth->getRuntime().midi_upper_voice_C)
        // it's really an upper set program change
        setMidiProgram(ch, (param & 0x1f) | 0x80, in_place);
    else if(ctrl == C_nrpnL || ctrl == C_nrpnH)
    {
        if (ctrl == C_nrpnL)
        {
            if (synth->getRuntime().nrpnL != param)
            {
                synth->getRuntime().nrpnL = param;
                synth->getRuntime().Log("MusicIO setMidiController: Set nrpn LSB to " + asString(param));
            }
            nLow = param;
            nHigh = synth->getRuntime().nrpnH;
        }
        else
        {
            if(synth->getRuntime().nrpnH != param)
            {
                synth->getRuntime().nrpnH = param;
                synth->getRuntime().Log("MusicIO setMidiController: Set nrpn MSB to " + asString(param));
            }
            nHigh = param;
            nLow = synth->getRuntime().nrpnL;
        }

        synth->getRuntime().dataL = 0x80; //  we've changed the NRPN
        synth->getRuntime().dataH = 0x80; //  so these are now invalid
        synth->getRuntime().nrpnActive = (nLow < 0x7f && nHigh < 0x7f);
//        synth->getRuntime().Log("Status nrpn " + asString(synth->getRuntime().nrpnActive));
    }
    else
    {
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
                else{ // data decrement
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
                nrpnProcessData(ch, ctrl, param);
                return;
            }
        }
        if (synth->getRuntime().nrpndata.vectorEnabled[ch] && synth->getRuntime().NumAvailableParts > NUM_MIDI_CHANNELS)
        { // vector control is direct to parts
           if (nrpnRunVector(ch, ctrl, param));
            return;
        }
        // pick up a drop-through if CC doesn't match the above
        synth->SetController(ch, ctrl, param);
    }
}


bool MusicIO::nrpnRunVector(unsigned char ch, int ctrl, int param)
{
    int Xopps = synth->getRuntime().nrpndata.vectorXfeatures[ch];
    int Yopps = synth->getRuntime().nrpndata.vectorYfeatures[ch];
    int p_rev = 127 - param;
    int swap1;
    int swap2;
    unsigned char type;

    if(ctrl == synth->getRuntime().nrpndata.vectorXaxis[ch])
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
    else if(ctrl == synth->getRuntime().nrpndata.vectorYaxis[ch])
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


void MusicIO::nrpnProcessData(unsigned char chan, int type, int par)
{
    int nHigh = synth->getRuntime().nrpnH;
    int nLow = synth->getRuntime().nrpnL;
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
        synth->getRuntime().Log("Go away NRPN 0x" + asHexString(nHigh) + "-" + asHexString(nLow) +" We don't know you!");
        synth->getRuntime().nrpnActive = false; // we were sent a turkey!
        return;
    }
    
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
    
    if (nLow == 0) // direct part change
        nrpnDirectPart(dHigh, par);

    else if (nLow == 1) // it's vector control
        nrpnSetVector(dHigh, chan, par);
}


void MusicIO::nrpnDirectPart(int dHigh, int par)
{
    switch (dHigh)
    {
        case 0: // set part number
        {
            if (par < synth->getRuntime().NumAvailableParts)
            {
                synth->getRuntime().dataL = par;
                synth->getRuntime().nrpndata.Part = par;
            }
            else // It's bad. Kill it
                synth->getRuntime().dataL = 128;
                synth->getRuntime().dataH = 128;
            break;
        }
        case 1: // Program Change
        {
            setMidiProgram(synth->getRuntime().nrpndata.Part | 0x80, par);
            break;
        }
        case 2: // Set controller number
        {
            synth->getRuntime().nrpndata.Controller = par;
            synth->getRuntime().dataL = par;
            break;
        }
        case 3: // Set controller value
        {
            synth->SetController(synth->getRuntime().nrpndata.Part | 0x80, synth->getRuntime().nrpndata.Controller, par);
            break;
        }
        case 4: // Set part's channel number
        {
            synth->SetPartChan(synth->getRuntime().nrpndata.Part, par);
            break;
        }
        case 5: // Set part's audio destination
        {
            if (par > 0 and par < 4)
                synth->SetPartDestination(synth->getRuntime().nrpndata.Part, par);
            break;
        }
    }
}


void MusicIO:: nrpnSetVector(int dHigh, unsigned char chan,  int par)
{
    string name = "";
    if (dHigh < 2)
    {
        int parts = synth->getRuntime().NumAvailableParts;
        if ((dHigh == 0) && (parts < NUM_MIDI_CHANNELS * 2))
        {
            synth->getRuntime().Log("MusicIO nrpnSetVector: Vector control needs at least " + asString(NUM_MIDI_CHANNELS * 2) + " parts");
            return;
        }
        else if ((dHigh == 1) && (parts < NUM_MIDI_CHANNELS * 4))
        {
            synth->getRuntime().Log("MusicIO nrpnSetVector: Vector control Y axis needs " + asString(NUM_MIDI_CHANNELS * 4) + " parts");
            return;
        }
        name = synth->getRuntime().testCCvalue(par);
    }
    else if (dHigh > 7)
        name = synth->getRuntime().masterCCtest(par);

    if (name > "")
    {
        synth->getRuntime().Log("MusicIO nrpnSetVector: CC " + asString(par) + " in use for " + name);
        return;
    }

    switch (dHigh)
    {
        case 0:
        {
            synth->getRuntime().nrpndata.vectorXaxis[chan] = par;
            if (!synth->getRuntime().nrpndata.vectorEnabled[chan])
            {
                synth->getRuntime().nrpndata.vectorEnabled[chan] = true;
                synth->getRuntime().Log("MusicIO nrpnSetVector: Vector control enabled");
                // enabling is only done with a valid X CC
            }
            synth->SetPartChan(chan, chan);
            synth->SetPartChan(chan | 16, chan);
            synth->getRuntime().nrpndata.vectorXcc2[chan] = C_panning;
            synth->getRuntime().nrpndata.vectorXcc4[chan] = C_filtercutoff;
            synth->getRuntime().nrpndata.vectorXcc8[chan] = C_modwheel;
            synth->getRuntime().Log("MusicIO nrpnSetVector: Set vector X CC to " + asString(par));
            break;
        }
        case 1:
        {
            if (!synth->getRuntime().nrpndata.vectorEnabled[chan])
                synth->getRuntime().Log("MusicIO nrpnSetVector: Vector X axis must be set before Y");
            else
            {
                synth->SetPartChan(chan | 32, chan);
                synth->SetPartChan(chan | 48, chan);
                synth->getRuntime().nrpndata.vectorYaxis[chan] = par;
                synth->getRuntime().nrpndata.vectorYcc2[chan] = C_panning;
                synth->getRuntime().nrpndata.vectorYcc4[chan] = C_filtercutoff;
                synth->getRuntime().nrpndata.vectorYcc8[chan] = C_modwheel;
                synth->getRuntime().Log("MusicIO nrpnSetVector: Set vector Y CC to " + asString(par));
            }
            break;
        }
        case 2:
        {
            synth->getRuntime().nrpndata.vectorXfeatures[chan] = par;
            synth->getRuntime().Log("MusicIO nrpnSetVector: Enabled X features " + asString(par));
            break;
        }
        case 3:
        {
            if (synth->getRuntime().NumAvailableParts > NUM_MIDI_CHANNELS * 2)
            {
                synth->getRuntime().nrpndata.vectorYfeatures[chan] = par;
                synth->getRuntime().Log("MusicIO nrpnSetVector: Enabled Y features " + asString(par));
            }
            break;
        }
        
        case 4:
        {
            setMidiProgram(chan | 0x80, par);
            break;
        }
        case 5:
        {
            setMidiProgram(chan | 0x90, par);
            break;
        }
        case 6:
        {
            setMidiProgram(chan | 0xa0, par);
            break;
        }
        case 7:
        {
            setMidiProgram(chan | 0xb0, par);
            break;
        }
        
        case 8:
        {
            synth->getRuntime().nrpndata.vectorXcc2[chan] = par;
            synth->getRuntime().Log("MusicIO nrpnSetVector: Set X feature 2 to " + asString(par));
            break;
        }
        case 9:
        {
            synth->getRuntime().nrpndata.vectorXcc4[chan] = par;
            synth->getRuntime().Log("MusicIO nrpnSetVector: Set X feature 4 to " + asString(par));
            break;
        }
        case 10:
        {
            synth->getRuntime().nrpndata.vectorXcc8[chan] = par;
            synth->getRuntime().Log("MusicIO nrpnSetVector: Set X feature 8 to " + asString(par));
            break;
        }
        case 11:
        {
            synth->getRuntime().nrpndata.vectorYcc2[chan] = par;
            synth->getRuntime().Log("MusicIO nrpnSetVector: Set Y feature 2 to " + asString(par));
            break;
        }
        case 12:
        {
            synth->getRuntime().nrpndata.vectorYcc4[chan] = par;
            synth->getRuntime().Log("MusicIO nrpnSetVector: Set Y feature 4 to " + asString(par));
            break;
        }
        case 13:
        {
            synth->getRuntime().nrpndata.vectorYcc8[chan] = par;
            synth->getRuntime().Log("MusicIO nrpnSetVector: Set Y feature 8 to " + asString(par));
            break;
        }
        
        default:
        {
            synth->getRuntime().nrpndata.vectorEnabled[chan] = false;
            synth->getRuntime().nrpndata.vectorXaxis[chan] = 0xff;
            synth->getRuntime().nrpndata.vectorYaxis[chan] = 0xff;
            synth->getRuntime().nrpndata.vectorXfeatures[chan] = 0;
            synth->getRuntime().nrpndata.vectorYfeatures[chan] = 0;
            synth->getRuntime().Log("MusicIO nrpnSetVector: Vector control disabled");
        }
    }
}


//bank change and root dir change change share the same thread
//to make changes consistent
void MusicIO::setMidiBankOrRootDir(unsigned int bank_or_root_num, bool in_place, bool setRootDir)
{
    if (setRootDir && (bank_or_root_num == synth->getBankRef().getCurrentRootID()))
        return; // nothing to do!
    if(in_place)
        setRootDir ? synth->SetBankRoot(bank_or_root_num) : synth->SetBank(bank_or_root_num);
    else
    {        
        pthread_t tmpBankOrRootDirThread = 0;
        tmpBankOrRootDirThread = __sync_fetch_and_add(&pBankOrRootDirThread, 0);
        if(tmpBankOrRootDirThread == 0) // don't allow more than one bank change/root dir change process at a time
        {
            isRootDirChangeRequested = setRootDir;
            bankOrRootDirToChange = bank_or_root_num;
            if(!synth->getRuntime().startThread(&pBankOrRootDirThread, MusicIO::static_BankOrRootDirChangeThread, this, false, 0, false))
            {
                synth->getRuntime().Log("MusicIO::setMidiBankOrRootDir: failed to start midi bank/root dir change thread!");
            }
        }
        else
            synth->getRuntime().Log("Midi bank/root dir changes too close together");
    }
}


void MusicIO::setMidiProgram(unsigned char ch, int prg, bool in_place)
{
    int partnum;
    if (ch < NUM_MIDI_CHANNELS)
        partnum = ch;
    else
        partnum = ch & 0x7f; // this is for direct part access instead of channel
    if(partnum >= synth->getRuntime().NumAvailableParts)
        return;
    if (synth->getRuntime().EnableProgChange)
    {
        if(in_place)
            synth->SetProgram(ch, prg);
        else
        {
            pthread_t tmpPrgThread = 0;
            tmpPrgThread = __sync_fetch_and_add(&prgChangeCmd [partnum].pPrgThread , 0);
            if(tmpPrgThread == 0) // don't allow more than one program change process at a time
            {
                prgChangeCmd [partnum].ch = ch;
                prgChangeCmd [partnum].prg = prg;
                prgChangeCmd [partnum]._this_ = this;
                if(!synth->getRuntime().startThread(&prgChangeCmd [partnum].pPrgThread, MusicIO::static_PrgChangeThread, &prgChangeCmd [partnum], false, 0, false))
                {
                    synth->getRuntime().Log("MusicIO::setMidiProgram: failed to start midi program change thread!");
                }
            }
        }
    }
}


void MusicIO::setMidiNote(unsigned char channel, unsigned char note,
                           unsigned char velocity)
{
    synth->NoteOn(channel, note, velocity);
}


void MusicIO::setMidiNote(unsigned char channel, unsigned char note)
{
    synth->NoteOff(channel, note);
}


bool MusicIO::prepBuffers(void)
{
    int buffersize = getBuffersize();
    if (buffersize > 0)
    {
        for (int part = 0; part < (NUM_MIDI_PARTS + 1); part++)
        {
            if (!(zynLeft[part] = (float*) fftwf_malloc(buffersize * sizeof(float))))
                goto bail_out;
            if (!(zynRight[part] = (float*) fftwf_malloc(buffersize * sizeof(float))))
                goto bail_out;
            memset(zynLeft[part], 0, buffersize * sizeof(float));
            memset(zynRight[part], 0, buffersize * sizeof(float));

        }
        return true;
    }

bail_out:
    synth->getRuntime().Log("Failed to allocate audio buffers, size " + asString(buffersize));
    for (int part = 0; part < (NUM_MIDI_PARTS + 1); part++)
    {
        if (zynLeft[part])
        {
            fftwf_free(zynLeft[part]);
            zynLeft[part] = NULL;
        }
        if (zynRight[part])
        {
            fftwf_free(zynRight[part]);
            zynRight[part] = NULL;
        }
    }
    if (interleaved)
    {
        delete[] interleaved;
        interleaved = NULL;
    }
    return false;
}


void *MusicIO::bankOrRootDirChange_Thread()
{
    //std::cerr << "MusicIO::bankChange_Thread(). banknum = " << bankToChange << std::endl;
    isRootDirChangeRequested ? synth->SetBankRoot(bankOrRootDirToChange) : synth->SetBank(bankOrRootDirToChange);
    pBankOrRootDirThread = 0; // done
    return NULL;
}

void *MusicIO::prgChange_Thread(_prgChangeCmd *pCmd)
{
    pthread_t tmpBankThread = 0;
    tmpBankThread = __sync_fetch_and_add(&pBankOrRootDirThread, 0);
    if(tmpBankThread != 0) // wait for active bank thread to finish before continue
    {
        //std::cerr << "Waiting for MusicIO::bankChange_Thread()..." << std::endl;
        void *threadRet = NULL;
        pthread_join(pBankOrRootDirThread, &threadRet);
    }

    //std::cerr << "MusicIO::prgChange_Thread(). ch = " << pCmd->ch << ", prg = " << pCmd->prg << std::endl;
    synth->SetProgram(pCmd->ch, pCmd->prg);
    pCmd->pPrgThread = 0; //done
    return NULL;
}

void *MusicIO::static_BankOrRootDirChangeThread(void *arg)
{
    return static_cast<MusicIO *>(arg)->bankOrRootDirChange_Thread();
}

void *MusicIO::static_PrgChangeThread(void *arg)
{
    _prgChangeCmd *pCmd = static_cast<_prgChangeCmd *>(arg);
    return pCmd->_this_->prgChange_Thread(pCmd);
}

