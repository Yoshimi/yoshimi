/*
    MusicIO.cpp

    Copyright 2009-2011, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

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
        setMidiProgram(ch, (param & 0x1f) | 0x80, in_place); // it's really an upper set program change
    else
        synth->SetController(ch, ctrl, param);
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
    if(ch >= NUM_MIDI_PARTS)
        return;
    if (synth->getRuntime().EnableProgChange)
    {
        if(in_place)
            synth->SetProgram(ch, prg);
        else
        {
            pthread_t tmpPrgThread = 0;
            tmpPrgThread = __sync_fetch_and_add(&prgChangeCmd [ch].pPrgThread , 0);
            if(tmpPrgThread == 0) // don't allow more than one program change process at a time
            {
                prgChangeCmd [ch].ch = ch;
                prgChangeCmd [ch].prg = prg;
                prgChangeCmd [ch]._this_ = this;
                if(!synth->getRuntime().startThread(&prgChangeCmd [ch].pPrgThread, MusicIO::static_PrgChangeThread, &prgChangeCmd [ch], false, 0, false))
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

