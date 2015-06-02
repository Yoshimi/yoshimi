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

MusicIO::MusicIO() :
    zynLeft(NULL),
    zynRight(NULL),
    interleavedShorts(NULL),
    rtprio(25)
{ }

MusicIO::~MusicIO()
{
    if (zynLeft)
        fftwf_free(zynLeft);
    if (zynRight)
        fftwf_free(zynRight);
    if (interleavedShorts)
        delete [] interleavedShorts;
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
	    default: // an unrecognised controller!
            ctl = C_NULL;
            break;
	}
    return ctl;
}


void MusicIO::setMidiController(unsigned char ch, unsigned int ctrl, int param)
{
    synth->SetController(ch, ctrl, param);
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
        if (!(zynLeft = (float*)fftwf_malloc(buffersize * sizeof(float))))
            goto bail_out;
        if (!(zynRight = (float*)fftwf_malloc(buffersize * sizeof(float))))
            goto bail_out;
        memset(zynLeft, 0, buffersize * sizeof(float));
        memset(zynRight, 0, buffersize * sizeof(float));
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
    if (zynLeft)
    {
        fftwf_free(zynLeft);
        zynLeft = NULL;
    }
    if (zynRight)
    {
        fftwf_free(zynRight);
        zynRight = NULL;
    }
    if (interleavedShorts)
    {
        delete [] interleavedShorts;
        interleavedShorts = NULL;
    }
    return false;
}
