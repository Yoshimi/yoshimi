/*
    MusicIO.cpp

    Copyright 2009-2010, Alan Calvert

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

#include <cstring>
#include <iostream>

using namespace std;   

#include "MusicIO/MusicIO.h"

MusicIO::MusicIO() :
    zynLeft(NULL),
    zynRight(NULL),
    interleavedShorts(NULL),
    wavRecorder(NULL),
    rtprio(25),
    audioLatency(0),
    midiLatency(0)
{ }


MusicIO::~MusicIO()
{ }


void MusicIO::Close(void)
{
    if (NULL != zynLeft)
        delete [] zynLeft;
    if (NULL != zynRight)
        delete [] zynRight;
    if (NULL != interleavedShorts)
        delete [] interleavedShorts;
    zynLeft = NULL;
    zynRight = NULL;
    interleavedShorts = NULL;
}



 void MusicIO::getAudio(void)
{
    zynMaster->MasterAudio(zynLeft, zynRight);
    if (wavRecorder->Running())
        wavRecorder->Feed(zynLeft, zynRight);
}


void MusicIO::InterleaveShorts(void)
{
    int buffersize = getBuffersize();
    int idx = 0;
    double scaled;
    for (int frame = 0; frame < buffersize; ++frame)
    {   // with a grateful nod to libsamplerate ...
        scaled = zynLeft[frame] * (8.0 * 0x10000000);
        interleavedShorts[idx++] = (short int)(lrint(scaled) >> 16);
        scaled = zynRight[frame] * (8.0 * 0x10000000);
        interleavedShorts[idx++] = (short int)(lrint(scaled) >> 16);
    }
}


int MusicIO::getMidiController(unsigned char b)
{
    int ctl = C_NULL;
    switch (b)
    {
	    case 1: // Modulation Wheel
            ctl = C_modwheel;
            break;
	    case 7: // Volume
            ctl = C_volume;
    		break;
	    case 10: // Panning
            ctl = C_panning;
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
	    // RPN and NRPN
	    case 0x06: // Data Entry (Coarse)
            ctl = C_dataentryhi;
	         break;
	    case 0x26: // Data Entry (Fine)
            ctl = C_dataentrylo;
	         break;
	    case 99:  // NRPN (Coarse)
            ctl = C_nrpnhi;
	         break;
	    case 98: // NRPN (Fine)
            ctl = C_nrpnlo;
	        break;
	    default: // an unrecognised controller!
            ctl = C_NULL;
            break;
	}
    return ctl;
}


void MusicIO::setMidiController(unsigned char ch, unsigned int ctrl,
                                    int param)
{
    zynMaster->SetController(ch, ctrl, param);
}


void MusicIO::setMidiNote(unsigned char channel, unsigned char note,
                           unsigned char velocity)
{
    zynMaster->NoteOn(channel, note, velocity, wavRecorder->Trigger());
}


void MusicIO::setMidiNote(unsigned char channel, unsigned char note)
{
    zynMaster->NoteOff(channel, note);
}


bool MusicIO::prepBuffers(bool with_interleaved)
{
    int buffersize = getBuffersize();
    if (buffersize > 0)
    {
        zynLeft = new jsample_t[buffersize];
        zynRight = new jsample_t[buffersize];
        if (NULL == zynLeft || NULL == zynRight)
            goto bail_out;
        memset(zynLeft, 0, sizeof(jsample_t) * buffersize);
        memset(zynRight, 0, sizeof(jsample_t) * buffersize);
        if (with_interleaved)
        {
            interleavedShorts = new short int[buffersize * 2];
            if (NULL == interleavedShorts)
                goto bail_out;
            memset(interleavedShorts, 0,  sizeof(short int) * buffersize * 2);
        }
        return true;
    }

bail_out:
    Runtime.Log("Failed to allocate audio buffers, size " + asString(buffersize));
    if (NULL != zynLeft)
        delete [] zynLeft;
    if (NULL != zynRight)
        delete [] zynRight;
    if (NULL != interleavedShorts)
        delete [] interleavedShorts;
    zynLeft = NULL;
    zynRight = NULL;
    interleavedShorts = NULL;
    return false;
}


bool MusicIO::prepRecord(void)
{
    return wavRecorder->Prep(getSamplerate(), getBuffersize());
}


bool MusicIO::setThreadAttributes(pthread_attr_t *attr, bool schedfifo, bool midi)
{
    int chk;
    if ((chk = pthread_attr_init(attr)))
    {
        Runtime.Log("Failed to initialise audio thread attributes: " + asString(chk));
        return false;
    }

    if ((chk = pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED)))
    {
        Runtime.Log("Failed to set audio thread detach state: " + asString(chk));
        return false;
    }
    if (schedfifo)
    {
        if ((chk = pthread_attr_setschedpolicy(attr, SCHED_FIFO)))
        {
            Runtime.Log("Failed to set SCHED_FIFO policy in audio thread attribute: "
                        + string(strerror(errno)) + " (" + asString(chk) + ")");
            return false;
        }
        if ((chk = pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED)))
        {
            Runtime.Log("Failed to set inherit scheduler audio thread attribute: "
                        + string(strerror(errno)) + " (" + asString(chk) + ")");
            return false;
        }
        sched_param prio_params;
        int prio = rtprio;
        if (midi)
            prio--;
        prio_params.sched_priority = (prio > 0) ? prio : 0;
        if ((chk = pthread_attr_setschedparam(attr, &prio_params)))
        {
            Runtime.Log("Failed to set audio thread priority attribute: ("
                        + asString(chk) + ")  " + string(strerror(errno)));
            return false;
        }
    }
    return true;
}
