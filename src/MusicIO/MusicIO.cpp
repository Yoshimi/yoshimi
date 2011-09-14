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

#include <boost/interprocess/creation_tags.hpp>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicIO.h"

MusicIO::MusicIO()
    :audioclientid(-1),
      midiclientid(-1),
      audiolatency(0),
      midilatency(0),
      zynLeft(NULL),
      zynRight(NULL),
      interleavedShorts(NULL),
      periodstartframe(0u),
      periodendframe(0u),
      wavRecorder(NULL),
      midiRingbuf(NULL),
      midiEventsUp(NULL)
{
    baseclientname = "yoshimi";
    if(!Runtime.nameTag.empty())
        baseclientname += ("-" + Runtime.nameTag);
}


MusicIO::~MusicIO()
{
    if(midiEventsUp)
        delete midiEventsUp;
    if(midiRingbuf)
        jack_ringbuffer_free(midiRingbuf);
}


bool MusicIO::prepAudio(bool with_interleaved)
{
    int buffersize = getBuffersize();
    if(buffersize > 0) {
        zynLeft  = new float[buffersize];
        zynRight = new float[buffersize];
        if((NULL == zynLeft) || (NULL == zynRight))
            goto bail_out;
        memset(zynLeft, 0, sizeof(float) * buffersize);
        memset(zynRight, 0, sizeof(float) * buffersize);
        if(with_interleaved) {
            interleavedShorts = new short int[buffersize * 2];
            if(NULL == interleavedShorts)
                goto bail_out;
            memset(interleavedShorts, 0, sizeof(short int) * buffersize * 2);
        }
        wavRecorder = new WavRecord();
        return true;
    }

bail_out:
    Runtime.Log("Failed to allocate audio buffers, size " + asString(buffersize));
    if(NULL != zynLeft)
        delete [] zynLeft;
    if(NULL != zynRight)
        delete [] zynRight;
    if(NULL != interleavedShorts)
        delete [] interleavedShorts;
    zynLeft  = NULL;
    zynRight = NULL;
    interleavedShorts = NULL;
    return false;
}


bool MusicIO::Start(void)
{
    if(!Runtime.startThread(&midiPthread, _midiThread, this, true, true)) {
        Runtime.Log("Failed to start midi thread", true);
        return false;
    }
    return true;
}


void MusicIO::queueMidi(midimessage *msg)
{
    if(!midiRingbuf)
        return;
    unsigned int wrote = 0;
    int   tries = 0;
    char *data  = (char *)msg;
    while(wrote < sizeof(midimessage) && tries < 3) {
        unsigned int act_write =
            jack_ringbuffer_write(midiRingbuf,
                                  (const char *)data,
                                  sizeof(midimessage)
                                  - wrote);
        wrote += act_write;
        data  += act_write;
        ++tries;
    }
    if(wrote != sizeof(midimessage))
        Runtime.Log("Bad write to midi ringbuffer: " + Runtime.asString(wrote)
                    + " / " + Runtime.asString((unsigned int)sizeof(midimessage)));
    else
        midiEventsUp->post();
}


void MusicIO::midiBankChange(unsigned char chan, unsigned short bank)
{
    midimessage msg;
    msg.event_frame = 0;
    msg.bytes[0]    = MSG_control_change | chan;
    msg.bytes[1]    = C_bankselectmsb;
    msg.bytes[2]    = (bank / 128) & 0x0f;
    queueMidi(&msg);

    msg.bytes[0] = MSG_control_change | chan;
    msg.bytes[1] = C_bankselectlsb;
    msg.bytes[2] = bank & 0x0f;
    queueMidi(&msg);
}


void MusicIO::Close(void)
{
    if(NULL != zynLeft)
        delete [] zynLeft;
    if(NULL != zynRight)
        delete [] zynRight;
    if(NULL != interleavedShorts)
        delete [] interleavedShorts;
    zynLeft  = NULL;
    zynRight = NULL;
    interleavedShorts = NULL;
}


void MusicIO::getAudio(void)
{
    synth->MasterAudio(zynLeft, zynRight);
    if(wavRecorder->Running())
        wavRecorder->Feed(zynLeft, zynRight);
}


void MusicIO::interleaveShorts(void)
{
    int    buffersize = getBuffersize();
    int    idx = 0;
    double scaled;
    for(int frame = 0; frame < buffersize; ++frame) { // with a grateful nod to libsamplerate ...
        scaled = zynLeft[frame] * (8.0 * 0x10000000);
        interleavedShorts[idx++] = (short int)(lrint(scaled) >> 16);
        scaled = zynRight[frame] * (8.0 * 0x10000000);
        interleavedShorts[idx++] = (short int)(lrint(scaled) >> 16);
    }
}


void *MusicIO::_midiThread(void *arg)
{
    return static_cast<MusicIO *>(arg)->midiThread();
}


void *MusicIO::midiThread(void)
{
    using namespace boost::interprocess;
    midiRingbuf = jack_ringbuffer_create(4096 * sizeof(midimessage));
    if(!midiRingbuf) {
        Runtime.Log("Failed to create midi ringbuffer", true);
        return NULL;
    }
    try {
        midiEventsUp = new interprocess_semaphore(0);
    }
    catch(interprocess_exception &ex) {
        Runtime.Log("Failed to create midi semaphore", true);
        return NULL;
    }
    midimessage  msg;
    unsigned int fetch;
    pthread_cleanup_push(NULL, NULL);
    while(Runtime.runSynth) {
        pthread_testcancel();
        midiEventsUp->wait();
        pthread_testcancel();
        fetch =
            jack_ringbuffer_read(midiRingbuf, (char *)&msg, sizeof(midimessage));
        if(fetch != sizeof(midimessage)) {
            Runtime.Log("Short ringbuffer read, " + Runtime.asString(
                            fetch) + " / "
                        + Runtime.asString((unsigned int)sizeof(midimessage)));
            continue;
        }
        uint32_t endframe = __sync_or_and_fetch(&periodendframe, 0);
        if(msg.event_frame > endframe) {
            uint32_t frame_wait = 1000000u / getSamplerate();
            uint32_t wait4it    = (msg.event_frame - endframe) * frame_wait;
            Runtime.Log(string("frame_wait ") + asString(frame_wait)
                        + string(", wait4it ") + asString(wait4it));
            if(wait4it > 2 * frame_wait)
                usleep(wait4it);
        }
        synth->applyMidi(msg.bytes);
    }
    pthread_cleanup_pop(0);
    return NULL;
}


void MusicIO::queueControlChange(unsigned char controltype, unsigned char chan,
                                 unsigned char val, uint32_t eventframe)
{
//    cerr << "Into MusicIO::queueControlChange, control type " << (int)controltype
//         << ", chan " << (int)chan
//         << ", value " << (int)val
//         << ", event frame " << eventframe << endl;
    midimessage msg;
    msg.bytes[0]    = MSG_control_change | chan;
    msg.bytes[1]    = controltype;
    msg.bytes[2]    = val;
    msg.event_frame = eventframe;
    queueMidi(&msg);
}


void MusicIO::queueProgramChange(unsigned char chan, unsigned short banknum,
                                 unsigned char prog, uint32_t eventframe)
{
//    cerr << "Into MusicIO::queueProgramChange, chan " << (int)chan
//         << ", banknum " << banknum
//         << ", program " << (int)prog
//         << ", event frame " << eventframe << endl;
    queueControlChange(C_bankselectmsb, chan, 0, 0);
    queueControlChange(C_bankselectlsb, chan, banknum, 0);
    midimessage msg;
    msg.bytes[0] = MSG_program_change | chan;
    msg.bytes[1] = prog;
    msg.bytes[2] = 0;
    queueMidi(&msg);
}
