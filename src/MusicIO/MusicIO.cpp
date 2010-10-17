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

#include <iostream>

#include <boost/interprocess/creation_tags.hpp>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicIO.h"

MusicIO::MusicIO() :
    audioclientid(-1),
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
    midiEventsUp(NULL),
    bankselectMsb(0),
    bankselectLsb(0)
{
    baseclientname = "yoshimi";
    if (!Runtime.nameTag.empty())
        baseclientname += ("-" + Runtime.nameTag);
}


MusicIO::~MusicIO()
{
    if (midiEventsUp)
        delete midiEventsUp;
    if (midiRingbuf)
        jack_ringbuffer_free(midiRingbuf);
}


bool MusicIO::prepAudio(bool with_interleaved)
{
    int buffersize = getBuffersize();
    if (buffersize > 0)
    {
        zynLeft = new float[buffersize];
        zynRight = new float[buffersize];
        if (NULL == zynLeft || NULL == zynRight)
            goto bail_out;
        memset(zynLeft, 0, sizeof(float) * buffersize);
        memset(zynRight, 0, sizeof(float) * buffersize);
        if (with_interleaved)
        {
            interleavedShorts = new short int[buffersize * 2];
            if (NULL == interleavedShorts)
                goto bail_out;
            memset(interleavedShorts, 0,  sizeof(short int) * buffersize * 2);
        }
        wavRecorder = new WavRecord();
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


bool MusicIO::Start(void)
{
    if (!Runtime.startThread(&midiPthread, _midiThread, this, true, true))
    {
        Runtime.Log("Failed to start midi thread", true);
        return false;
    }
    return true;
}


void MusicIO::queueMidi(midimessage *msg)
{
    if (!midiRingbuf)
        return;
    unsigned int wrote = 0;
    int tries = 0;
    char *data = (char*)msg;
    while (wrote < sizeof(midimessage) && tries < 3)
    {
        unsigned int act_write = jack_ringbuffer_write(midiRingbuf, (const char*)data,
                                                       sizeof(midimessage) - wrote);
        wrote += act_write;
        data += act_write;
        ++tries;
    }
    if (wrote != sizeof(midimessage))
        Runtime.Log("Bad write to midi ringbuffer: " + Runtime.asString(wrote)
                     + " / " + Runtime.asString((unsigned int)sizeof(midimessage)));
    else
        midiEventsUp->post();
}


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
    synth->MasterAudio(zynLeft, zynRight);
    if (wavRecorder->Running())
        wavRecorder->Feed(zynLeft, zynRight);
}


void MusicIO::interleaveShorts(void)
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

/**
void MusicIO::applyMidi(unsigned char* bytes)
{
    unsigned char channel = bytes[0] & 0x0f;
    switch (bytes[0] & 0xf0)
    {
        case MSG_noteoff: // 128
            synth->noteOff(channel, bytes[1]);
            break;

        case MSG_noteon: // 144
            synth->noteOn(channel, bytes[1], bytes[2], wavRecorder->Trigger());
            break;

        case MSG_control_change: // 176
            switch(bytes[1])
            {
                case C_dataentrymsb: //  6
                    break;
                    case C_modwheel:             //   1
                    case C_volume:               //   7
                    case C_pan:                  //  10
                    case C_expression:           //  11
                    case C_effectcontrol2:       //  13
                    case C_sustain:              //  64
                    case C_portamento:           //  65
                    case C_filterq:              //  71
                    case C_filtercutoff:         //  74
                    case C_bandwidth:            //  75
                    case C_fmamp:                //  76
                    case C_resonance_center:     //  77
                    case C_resonance_bandwidth:  //  78
                    case C_allsoundsoff:         // 120
                    case C_resetallcontrollers:  // 121
                    case C_allnotesoff:          // 123
                        synth->setController(channel, bytes[1], bytes[2]);
                        break;

                    case C_bankselectmsb:
                        bankselectMsb = bytes[2];
                        break;

                    case C_bankselectlsb:
                        bankselectLsb = bytes[2];
                        break;

                    default:
                        break;
            }
            break;

         case MSG_program_change: // 224
             synth->programChange(bankselectMsb, bankselectLsb);
             break;

         case MSG_pitchwheel_control: // 224
             synth->setController(channel, MSG_pitchwheel_control,
                                  ((bytes[2] << 7) | bytes[1]) - 8192);
             break;

        default: // too difficult or just uninteresting
            break;
    }
}
**/

void *MusicIO::_midiThread(void *arg)
{
    return static_cast<MusicIO*>(arg)->midiThread();
}


void *MusicIO::midiThread(void)
{
    using namespace boost::interprocess;
    midiRingbuf = jack_ringbuffer_create(4096 * sizeof(midimessage));
    if (!midiRingbuf)
    {
        Runtime.Log("Failed to create midi ringbuffer", true);
        return NULL;
    }
    try { midiEventsUp = new interprocess_semaphore(0); }
    catch (interprocess_exception &ex)
    {
        Runtime.Log("Failed to create midi semaphore", true);
        return NULL;
    }
    midimessage msg;
    unsigned int fetch;
    pthread_cleanup_push(NULL, this);
    while (Runtime.runSynth)
    {
        pthread_testcancel();
        midiEventsUp->wait();
        pthread_testcancel();
        fetch = jack_ringbuffer_read(midiRingbuf, (char*)&msg, sizeof(midimessage));
        if (fetch != sizeof(midimessage))
        {
            Runtime.Log("Short ringbuffer read, " + Runtime.asString(fetch) + " / "
                        + Runtime.asString((unsigned int)sizeof(midimessage)));
            continue;
        }
        uint32_t endframe = __sync_or_and_fetch(&periodendframe, 0);
        if (msg.event_frame > endframe)
        {
            uint32_t frame_wait = 1000000u / getSamplerate();
            uint32_t wait4it = (msg.event_frame - endframe) * frame_wait;
            cerr << "frame_wait " << frame_wait << ", wait4it " << wait4it << endl;
            if (wait4it > 2 * frame_wait)
                usleep(wait4it);
        }
        unsigned char channel = msg.bytes[0] & 0x0f;
        switch (msg.bytes[0] & 0xf0)
        {
            case MSG_noteoff: // 128
                synth->noteOff(channel, msg.bytes[1]);
                break;

            case MSG_noteon: // 144
               synth->noteOn(channel, msg.bytes[1], msg.bytes[2],
                             (wavRecorder != NULL) ? wavRecorder->Trigger() : false);
                break;

            case MSG_control_change: // 176
                processControlChange(&msg);
                break;

            case MSG_program_change: // 192
                 synth->programChange(channel, bankselectMsb, bankselectLsb);
                 break;

             case MSG_pitchwheel_control: // 224
                 synth->setPitchwheel(channel, ((msg.bytes[2] << 7) | msg.bytes[1]) - 8192);
                 break;

             default: // too difficult or just uninteresting
                break;
        }
    }
    pthread_cleanup_pop(0);
    return NULL;
}


void MusicIO::processControlChange(midimessage *msg)
{
    unsigned char channel = msg->bytes[0] & 0x0f;
    switch(msg->bytes[1])
    {
        case C_dataentrymsb: //  6
            break;
        default:
            switch (msg->bytes[1])
            {
                case C_bankselectmsb: // inactive
                    bankselectMsb = msg->bytes[2];
                    break;

                case C_bankselectlsb: // inactive
                    bankselectLsb = msg->bytes[2];
                    break;

                case C_modwheel:             //   1
                case C_volume:               //   7
                case C_pan:                  //  10
                case C_expression:           //  11
                case C_sustain:              //  64
                case C_portamento:           //  65
                case C_filterq:              //  71
                case C_filtercutoff:         //  74
                case C_soundcontroller6:     //  75 bandwidth
                case C_soundcontroller7:     //  76 fmamp
                case C_soundcontroller8:     //  77 resonance center     
                case C_soundcontroller9:     //  78 resonance bandwidth
                case C_allsoundsoff:         // 120
                case C_resetallcontrollers:  // 121
                case C_allnotesoff:          // 123
                    synth->setController(channel, msg->bytes[1], msg->bytes[2]);
                    break;

                default:
                    cerr << "Midi control change " << (int)msg->bytes[1] << " ignored" << endl; 
                    break;
            }
            break;
    }
}
