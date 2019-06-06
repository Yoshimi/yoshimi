/*
    MusicIO.cpp

    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others

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
    Modified May 2019
*/

#include <errno.h>
#include <cstring>
#include <fftw3.h>

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicIO.h"

#include <unistd.h>
#include <iostream>

MusicIO::MusicIO(SynthEngine *_synth) :
    interleaved(NULL),
    synth(_synth),
    LV2_engine(synth->getIsLV2Plugin()),
    samplesUsed(0)
{
    memset(zynLeft, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    memset(zynRight, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
}


MusicIO::~MusicIO()
{
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


void MusicIO::setMidi(unsigned char par0, unsigned char par1, unsigned char par2, bool in_place)
{
    if (synth->isMuted())
        return; // nobody listening!

    bool inSync = LV2_engine || (synth->getRuntime().audioEngine == jack_audio && synth->getRuntime().midiEngine == jack_midi);

    CommandBlock putData;
/*
 * This below is a much simpler (faster) way
 * to do note-on and note-off
 * Tested on ALSA JACK and LV2 all combinations!
 */
    unsigned int event = par0 & 0xf0;
    unsigned char channel = par0 & 0xf;
    if (event == 0x80 || event == 0x90)
    {
        if (par2 < 1) // zero volume note on.
            event = 0x80;

#ifdef REPORT_NOTES_ON_OFF
        if (event == 0x80) // note test
            ++synth->getRuntime().noteOffSent;
        else
            ++synth->getRuntime().noteOnSent;
#endif

        if (inSync)
        {
            if (event == 0x80)
                synth->NoteOff(channel, par1);
            else
                synth->NoteOn(channel, par1, par2);
        }
        else
        {
            putData.data.value = float(par2);
            putData.data.type = 8;
            putData.data.control = (event == 0x80);
            putData.data.part = TOPLEVEL::section::midiIn;
            putData.data.kit = channel;
            putData.data.engine = par1;
            synth->midilearn.writeMidi(&putData, false);
        }
        return;
    }
    synth->mididecode.midiProcess(par0, par1, par2, in_place, inSync);
}


bool MusicIO::prepBuffers(void)
{
    int buffersize = synth->getRuntime().Buffersize;
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


/*
 * Pull sound samples from the engine, possibly triggering sound synthesis calculations.
 * The retrieved sound data will be sent to the output sink by calling pushAudioOutput() at least once,
 * but possibly several times. At the end, precisely the requested amount of samples will be sent
 * to output. Param sample_count is a variable integer, not necessarily a power of two (due to LV2),
 * and can possibly be zero. The only requirement is that sample_count <= output buffer size.
 * Audio samples will be generated into the persistent buffers allocated for zynLeft | zynRight.
 * If the SyntEngine::buffersize (calculation chunk size) does not match the requested output
 * sample_count, some samples may be kept there to be used on the subsequent pullAudio() call.
 */
void MusicIO::pullAudio(uint32_t samples_to_send)
{
    uint32_t outputStart = 0;
    while (samples_to_send > 0)
    {
        uint32_t chunk = synth->buffersize - samplesUsed;
        if (chunk == 0)
        {
            synth->MasterAudio(zynLeft, zynRight);
            chunk = synth->buffersize;
            samplesUsed = 0;
        }
        if (chunk > samples_to_send)
            {// send only some part of the calculated samples
                chunk = samples_to_send;
            }
        pushAudioOutput(outputStart, chunk);
        samples_to_send -= chunk;
        outputStart += chunk;
        samplesUsed += chunk;
    }
}
