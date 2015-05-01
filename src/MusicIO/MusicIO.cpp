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
    interleavedShorts(NULL),
    rtprio(25),
    synth(_synth),
    pBankOrRootDirThread(0)
{
    memset(zynLeft, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    memset(zynRight, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    memset(&prgChangeCmd, 0, sizeof(prgChangeCmd));
    for (int chan = 0; chan < NUM_MIDI_CHANNELS; ++chan)
    {
        nrpndata.vectorEnabled[chan] = false;
        nrpndata.vectorXaxis[chan] = 0xff;
        nrpndata.vectorYaxis[chan] = 0xff;
    }
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
    if (interleavedShorts)
        delete[] interleavedShorts;
}


void MusicIO::InterleaveShorts(void)
{
    int buffersize = getBuffersize();
    int idx = 0;
    double scaled;
    for (int frame = 0; frame < buffersize; ++frame)
    {   // with a grateful nod to libsamplerate ...
        scaled = zynLeft[NUM_MIDI_PARTS][frame] * (8.0 * 0x10000000);
        interleavedShorts[idx++] = (short int) (lrint(scaled) >> 16);
        scaled = zynRight[NUM_MIDI_PARTS][frame] * (8.0 * 0x10000000);
        interleavedShorts[idx++] = (short int) (lrint(scaled) >> 16);
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
    if (ctrl == synth->getRuntime().midi_bank_root)
        setMidiBankOrRootDir(param, in_place, true);
    else if (ctrl == synth->getRuntime().midi_bank_C)
        setMidiBankOrRootDir(param, in_place);
    else if (ctrl == synth->getRuntime().midi_upper_voice_C)
        // it's really an upper set program change
        setMidiProgram(ch, (param & 0x1f) | 0x80, in_place);
    else if(ctrl == C_nrpnL)
    {
        if(synth->getRuntime().nrpnL == param)
            return;
        synth->getRuntime().nrpnL = param;
        synth->getRuntime().dataL = 128; //  we've changed the NRPN
        synth->getRuntime().dataH = 128; //  so these are now invalid
        if (param == 1 && synth->getRuntime().nrpnH == 64) // clear out previous vector settings
        {
            nrpndata.vectorEnabled[ch] = false;
            nrpndata.vectorXaxis[ch] = 0xff;
            nrpndata.vectorYaxis[ch] = 0xff;
        }
        synth->getRuntime().nrpnActive = (param < 127 && synth->getRuntime().nrpnH < 127);
        synth->getRuntime().Log("Set nrpn LSB to " + asString(param));
    }
    else if(ctrl == C_nrpnH)
    {
        if(synth->getRuntime().nrpnH == param)
            return;
        synth->getRuntime().nrpnH = param;
        synth->getRuntime().dataL = 128; //  we've changed the NRPN
        synth->getRuntime().dataH = 128; //  so these are now invalid
        if (param == 64 && synth->getRuntime().nrpnL == 1) // clear out previous vector settings
        {
            nrpndata.vectorEnabled[ch] = false;
            nrpndata.vectorXaxis[ch] = 0xff;
            nrpndata.vectorYaxis[ch] = 0xff;
        }
        synth->getRuntime().nrpnActive = (param < 127 && synth->getRuntime().nrpnL < 127);
        synth->getRuntime().Log("Set nrpn MSB to " + asString(param));
    }
    else
    {
        if (synth->getRuntime().nrpnActive)
        {
            if(ctrl == C_dataI || ctrl == C_dataD)
            {
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
                else{
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
            
            if(ctrl == C_dataL || ctrl == C_dataH)
            {
                ProcessNrpn(ch, ctrl, param);
                return;
            }
#warning should some NRPN & vector stuff move out of the MIDI thread?
# if NUM_MIDI_PARTS == 64
            if (nrpndata.vectorEnabled[ch])
            {
                int Xopps = nrpndata.vectorXaxis[ch];
                int Xtype = Xopps & 0xff;
                int Yopps = nrpndata.vectorYaxis[ch];
                int Ytype = Yopps & 0xff;
                Xopps = Xopps >> 8;
                Yopps = Yopps >> 8;
                if(Xtype == ctrl || Ytype == ctrl)
                { // vector control is direct to parts
                    if(Xtype == ctrl)
                    {
//                        synth->getRuntime().Log("X D H " + asString(Xopps)  + "   D L " + asString(Xtype) + "  V " + asString(param));
                        if (Xopps & 1) // volume
                        {
                            synth->SetController(ch | 0x80, C_volume, param); // needs improving
                            synth->SetController(ch | 0x90, C_volume, 127 - param);
                        }
                        if (Xopps & 2) // pan
                        {
                            synth->SetController(ch | 0x80, C_panning, param);
                            synth->SetController(ch | 0x90, C_panning, 127 - param);
                        }
                        if (Xopps & 4) // 'brightness'
                        {
                            synth->SetController(ch | 0x80, C_filtercutoff, param);
                            synth->SetController(ch | 0x90, C_filtercutoff, 127 - param);
                        }
                    }
                    else // if Y hasn't been set these commands will be ignored
                    {
//                       synth->getRuntime().Log("Y D H " + asString(Yopps)  + "   D L " + asString(Ytype) + "  V " + asString(param));
                        if (Yopps & 1) // volume
                        {
                            synth->SetController(ch | 0xa0, C_volume, param);
                            synth->SetController(ch | 0xb0, C_volume, 127 - param);
                        }
                        if (Yopps & 2) // pan
                        {
                            synth->SetController(ch | 0xa0, C_panning, param);
                            synth->SetController(ch | 0xb0, C_panning, 127 - param);
                        }
                        if (Yopps & 4) // 'brightness'
                        {
                            synth->SetController(ch | 0xa0, C_filtercutoff, param);
                            synth->SetController(ch | 0xb0, C_filtercutoff, 127 - param);
                        }
                    }
                    return;
                }
            }
#endif
        }
        synth->SetController(ch, ctrl, param);
    }
}


void MusicIO::ProcessNrpn(unsigned char chan, int type, short int par)
{
    int nHigh = synth->getRuntime().nrpnH;
    if (nHigh != 64)
    {   
        synth->getRuntime().Log("Go away " + asString(nHigh) + ". We don't know you");
        return;
    }
    int nLow = synth->getRuntime().nrpnL;
    
    if (type == C_dataH)
    {
        synth->getRuntime().dataH = par;
        synth->getRuntime().Log("Data MSB    value " + asString(par));
        return; // we're not currently using MSB as a value
    }
    synth->getRuntime().dataL = par;
    synth->getRuntime().Log("Data LSB    value " + asString(par));
    int dHigh = synth->getRuntime().dataH;
    
    if (type == C_dataL && nLow == 0) // direct part change
    {
        switch (dHigh)
        {
            case 0: // set part number
                {
                    if (par < NUM_MIDI_PARTS)
                    {
                        synth->getRuntime().dataL = par;
                        nrpndata.Part = par;
                    }
                    else // It's bad. Kill it
                        synth->getRuntime().dataL = 128;
                        synth->getRuntime().dataH = 128;
                    break;
                }
                case 1: // Program Change
                {
                    setMidiProgram(nrpndata.Part | 0x80, par);
                    break;
                }
                case 2: // Set controller number
                {
                    nrpndata.Controller = par;
                    synth->getRuntime().dataL = par;
                    break;
                }

                case 3: // Set controller value
                {
                    synth->SetController(nrpndata.Part | 0x80, nrpndata.Controller, par);
                    break;
                }
                case 4: // Set part's channel number
                {
                     synth->SetPartChan(nrpndata.Part, par);
                     break;
                }
        }
    }

#if NUM_MIDI_PARTS == 64
    else if (nLow == 1) // it's vector control
        if (type == C_dataL)
        {
            switch (dHigh)
            {
                case 0:
                    {
                        nrpndata.vectorXaxis[chan]
                        = (nrpndata.vectorXaxis[chan] & 0xff00) | par;
                        if (!nrpndata.vectorEnabled[chan])
                        {
                            nrpndata.vectorEnabled[chan] = true;
                            synth->getRuntime().Log("Vector control enabled");
                            // enabling is only done with a valid X CC
                        }
                        break;
                    }
                case 1:
                    {
                        if ((nrpndata.vectorXaxis[chan] & 0xff) == 0xff)
                            synth->getRuntime().Log("Vector X axis must be set before Y");
                        else
                            nrpndata.vectorYaxis[chan]
                            = (nrpndata.vectorYaxis[chan] & 0xff00) | par;
                        break;
                    }
                case 2:
                    {
                        nrpndata.vectorXaxis[chan]
                        = (nrpndata.vectorXaxis[chan] & 0xff) | (par << 8);
                        break;
                    }
                case 3:
                    {
                        
                        nrpndata.vectorYaxis[chan]
                        = (nrpndata.vectorYaxis[chan] & 0xff) | (par << 8);
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
                 default:
                    {
                        nrpndata.vectorEnabled[chan] = false;
                        nrpndata.vectorXaxis[chan] = 0xff;
                        nrpndata.vectorYaxis[chan] = 0xff;
                        synth->getRuntime().Log("Vector control disabled");
                    }
            }
        }
#endif
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
    if(partnum >= NUM_MIDI_PARTS)
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


bool MusicIO::prepBuffers(bool with_interleaved)
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
        if (with_interleaved)
        {
            interleavedShorts = new short int[buffersize * 2];
            if (NULL == interleavedShorts)
                goto bail_out;
            memset(interleavedShorts, 0, sizeof(short int) * buffersize * 2);
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
    if (interleavedShorts)
    {
        delete[] interleavedShorts;
        interleavedShorts = NULL;
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

