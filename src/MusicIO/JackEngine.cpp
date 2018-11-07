/*
    JackEngine.cpp

    Copyright 2009, Alan Calvert

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

#include <jack/midiport.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace std;

#include "Misc/Config.h"
#include "Misc/Master.h"
#include "MusicIO/JackEngine.h"

JackEngine::JackEngine() :
    MusicIO(),
    jackClient(NULL)
{
    audio.jackSamplerate = 0;
    audio.jackNframes = 0;
    for (int i = 0; i < 2; ++i)
    {
        audio.ports[i] = NULL;
        audio.portBuffs[i] = NULL;
    }
    midi.port = NULL;
    midi.maxdata = sizeof(midi.data);
    midi.ringBuf = NULL;
    midi.pThread = 0;
    midi.semName.clear();
    midi.eventsUp = NULL;
    midi.threadStop = true;
}


bool JackEngine::connectServer(string server)
{
    if (NULL == jackClient) // ie, not already connected
    {
        string clientname = "yoshimi";
        if (!Runtime.settings.nameTag.empty())
            clientname += ("-" + Runtime.settings.nameTag);
        jack_status_t jackstatus;
        bool use_server_name = server.size() && server.compare("default") != 0;
        jack_options_t jopts = (jack_options_t)
            (((use_server_name) ? JackServerName : JackNullOption)
            | ((autostart_jack) ? JackNullOption : JackNoStartServer));
        for (int tries = 0; tries < 3 && NULL == jackClient; ++tries)
        {
            if (use_server_name)
                jackClient = jack_client_open(clientname.c_str(), jopts, &jackstatus,
                                              server.c_str());
            else
                jackClient = jack_client_open(clientname.c_str(), jopts, &jackstatus);
            if (NULL != jackClient)
                break;
            else
                usleep(3333);
        }
        if (NULL != jackClient)
            return true;
        else
            cerr << "Error, failed to open jack client on server: " << server
                 << ", status " << jackstatus << endl;
        return false;
    }
    return true;
}


bool JackEngine::Start(void)
{
    if (NULL != jackClient)
    {
        int chk;
        jack_set_error_function(_errorCallback);
        jack_set_info_function(_infoCallback);
        if ((chk = jack_set_xrun_callback(jackClient, _xrunCallback, this)))
            cerr << "Error setting jack xrun callback" << endl;
        if (jack_set_process_callback(jackClient, _processCallback, this))
        {
            cerr << "Error, JackEngine failed to set process callback" << endl;
            goto bail_out;
        }
        if (jack_activate(jackClient))
        {
            cerr << "Error, failed to activate jack client" << endl;;
            goto bail_out;
        }

        if (NULL != midi.port)
        {
            pthread_attr_t attr;
            setThreadAttribute(&attr);
            midi.threadStop = false;
            if ((chk = pthread_create(&midi.pThread, &attr, _midiThread, this)))
            {
                cerr << "Error, failed to start jack midi thread: " << chk << endl;
                midi.threadStop = true;
                goto bail_out;
            }
        }
        return true;
    }
    else
        cerr << "Error, NULL jackClient through Start()" << endl;
bail_out:
    Close();
    return false;
}


void JackEngine::Stop(void)
{
    midi.threadStop = true;
    if (NULL != midi.port && midi.pThread)
        if (pthread_cancel(midi.pThread))
            cerr << "Error, failed to cancel Jack midi thread" << endl;
}


void JackEngine::Close(void)
{
    Stop();
    int chk;
    if (NULL != jackClient)
    {
        for (int i = 0; i < 2; ++i)
        {
            if (NULL != audio.ports[i])
                jack_port_unregister(jackClient, audio.ports[i]);
            audio.ports[i] = NULL;
        }
        if (NULL != midi.port)
        {
            jack_port_unregister(jackClient, midi.port);
            midi.port = NULL;
        }
        chk = jack_client_close(jackClient);
        if (chk && Runtime.settings.verbose)
            cerr << "Error, failed to close jack client, status: " << chk << endl;
        if (NULL != midi.ringBuf)
        {
            jack_ringbuffer_free(midi.ringBuf);
            midi.ringBuf = NULL;
        }
        jackClient = NULL;
    }
    if (NULL != midi.eventsUp && (chk = sem_close(midi.eventsUp)))
        cerr << "Error, failed to close jack midi semaphore "
             << midi.semName << strerror(errno) << endl;
    if (!midi.semName.empty() && (chk = sem_unlink(midi.semName.c_str())))
        cerr << "Error, failed to unlink jack midi semaphore "
             << midi.semName << strerror(errno) << endl;
    MusicIO::Close();
}


bool JackEngine::openAudio(void)
{
    const char *portnames[] = { "left", "right" };
    for (int port = 0; port < 2; ++port)
    {
        audio.ports[port] = jack_port_register(jackClient, portnames[port],
                                              JACK_DEFAULT_AUDIO_TYPE,
                                              JackPortIsOutput, 0);
    }
    if (NULL != audio.ports[0] && NULL != audio.ports[1])
    {
        audio.jackSamplerate = jack_get_sample_rate(jackClient);
        audio.jackNframes = jack_get_buffer_size(jackClient);
        return (prepBuffers(false) && prepRecord());
    }
    else
        cerr << "Error, failed to register jack audio ports" << endl;

    Close();
    return false;
}


bool JackEngine::openMidi(void)
{
    if (NULL != jackClient)
    {
        midi.ringBuf =
            jack_ringbuffer_create(midi.maxdata * sizeof(char) * 4096);
        if (NULL != midi.ringBuf)
        {
            midi.semName = string(clientName());
            midi.eventsUp = sem_open(midi.semName.c_str(), O_CREAT, S_IRWXU, 0);
            if (midi.eventsUp != SEM_FAILED)
            {
                const char *port_name = "midi in";
                midi.port = jack_port_register(jackClient, port_name,
                                               JACK_DEFAULT_MIDI_TYPE,
                                               JackPortIsInput, 0);
                if (NULL != midi.port)
                    return true;
                else
                    cerr << "Error, failed to register jack midi port" << endl;
            }
            else
                cerr << "Error, failed to create jack midi semaphore "
                     << midi.semName << strerror(errno) << endl;
        }
        else
            cerr << "Error, failed to create jack midi ringbuffer" << endl;;

    }
    else 
        cerr << "Error, null jackClient through registerMidi" << endl;
    Close();
    return false;
}


int JackEngine::clientId(void)
{
    if (NULL != jackClient)
        return jack_client_thread_id(jackClient);
    else
        return -1;
}


string JackEngine::clientName(void)
{
    if (NULL != jackClient)
        return string(jack_get_client_name(jackClient));
    else
        cerr << "Error, clientName() with null jackClient" << endl;
    return string("Oh, yoshimi :-(");
}


int JackEngine::_processCallback(jack_nframes_t nframes, void *arg)
{
    return static_cast<JackEngine*>(arg)->processCallback(nframes);
}


int JackEngine::processCallback(jack_nframes_t nframes)
{
    bool okaudio = true;
    bool okmidi = true;

    if (NULL != audio.ports[0] && NULL != audio.ports[1])
        okaudio = processAudio(nframes);
    if (NULL != midi.port)
        okmidi = processMidi(nframes);
    return (okaudio && okmidi) ? 0 : -1;
}


bool JackEngine::processAudio(jack_nframes_t nframes)
{
    if (NULL != audio.ports[0] && NULL != audio.ports[1])
    {
        if (NULL != zynMaster)
        {
            for (int port = 0; port < 2; ++port)
            {
                audio.portBuffs[port] =
                    (jsample_t*)jack_port_get_buffer(audio.ports[port], nframes);
                if (NULL == audio.portBuffs[port])
                {
                    cerr << "Error, failed to get jack audio port buffer: "
                         << port << endl;
                    return false;
                }
            }
            getAudio();
            memcpy(audio.portBuffs[0], zynLeft, sizeof(jsample_t) * nframes);
            memcpy(audio.portBuffs[1], zynRight, sizeof(jsample_t) * nframes);
        }
        else
        {
            memset(audio.portBuffs[0], 0, sizeof(jsample_t) * nframes);
            memset(audio.portBuffs[1], 0, sizeof(jsample_t) * nframes);
        }
        return true;
    }
    else
        cerr << "Error, invalid audioPorts in JackEngine::processAudio" << endl;
    return false;
}


bool JackEngine::processMidi(jack_nframes_t nframes)
{
    if (NULL == midi.port)
    {
        cerr << "Error, NULL midiPort through JackEngine::processMidi" << endl;
        return false;
    }
    void *portBuf = jack_port_get_buffer(midi.port, nframes);
    if (NULL == portBuf)
    {
        cerr << "Error, bad get jack midi port buffer" << endl;
        return  false;
    }

    jack_midi_event_t jEvent;
    jack_nframes_t eventCount = jack_midi_get_event_count(portBuf);
    unsigned int byt;
    unsigned int idx;
    unsigned int act_write;
    char *data;
    unsigned int wrote = 0;
    unsigned int tries = 0;
    int chk;
    for(idx = 0; idx < eventCount; ++idx)
    {
        if(!jack_midi_event_get(&jEvent, portBuf, idx))
        {
            if (jEvent.size > 0 && jEvent.size <= midi.maxdata)
            {   // no interest in 0 size or long events
                for (byt = 0; byt < jEvent.size; ++byt)
                    midi.data[byt] = jEvent.buffer[byt];
                while (byt < midi.maxdata)
                    midi.data[byt++] = 0;
                wrote = 0;
                tries = 0;
                data = midi.data;
                while (wrote < midi.maxdata && tries < 3)
                {
                    act_write = jack_ringbuffer_write(midi.ringBuf,
                                                      (const char*)data,
                                                      midi.maxdata - wrote);
                    wrote += act_write;
                    data += act_write;
                    ++tries;
                }
                if (wrote == midi.maxdata)
                {
                    if ((chk = sem_post(midi.eventsUp)))
                        cerr << "Error, jack midi semaphore post failed: "
                             << strerror(errno) << endl;
                }
                else
                {
                    cerr << "Error, bad write to midi ringbuffer: "
                         << wrote << " / " << midi.maxdata << endl;
                    return false;
                }
            }
        }
        else
            cerr << "Warn, jack midi read failed" << endl;
    }
    return true;
}


void *JackEngine::_midiThread(void *arg)
{
    return static_cast<JackEngine*>(arg)->midiThread();
}

void *JackEngine::midiThread(void)
{
    unsigned char channel, note, velocity;
    int chk;
    int ctrltype;
    int par = 0;
    unsigned int fetch;
    unsigned int ev;
    while (!midi.threadStop)
    {
       if ((chk = sem_wait(midi.eventsUp)))
        {
            cerr << "Error on jack midi semaphore wait: "
                 << strerror(errno) << endl;
            continue;
        }
        fetch = jack_ringbuffer_read(midi.ringBuf, midi.data, midi.maxdata);
        if (fetch != midi.maxdata)
        {
            cerr << "Error, short ringbuffer read, " << fetch << " / "
                 << midi.maxdata << endl;
            continue;
        }
        channel = midi.data[0] & 0x0F;
        switch ((ev = midi.data[0] & 0xF0))
        {
            case 0x01: // modulation wheel or lever
                ctrltype = C_modwheel;
                par = midi.data[2];
                setMidiController(channel, ctrltype, par);
                break;

            case 0x07: // channel volume (formerly main volume)
                ctrltype = C_volume;
                par = midi.data[2];
                setMidiController(channel, ctrltype, par);
                break;

            case 0x0B: // expression controller
                ctrltype = C_expression;
                par = midi.data[2];
                setMidiController(channel, ctrltype, par);
                break;

            case 0x0C: // program change ... but how?
                if (Runtime.settings.verbose)
                {
                    cerr << "How to change to program " << par << " on channel "
                         << channel << "?" << endl;
                    par = midi.data[2];
                }
                break;

            case 0x42: // Sostenuto On/Off
                par = midi.data[2]; // < 63 off, > 64 on
                if (Runtime.settings.verbose)
                {
                    string errstr = string("Sostenuto ");
                    if (par < 63)
                        errstr += "off";
                    else if (par > 64)
                        errstr += "on";
                    else
                        errstr += "?";
                    cerr << errstr << endl;
                }
                break;

            case 0x78: // all sound off
                ctrltype = C_allsoundsoff;
                setMidiController(channel, ctrltype, 0);
                break;

            case 0x79: // reset all controllers
                ctrltype = C_resetallcontrollers;
                setMidiController(channel, ctrltype, 0);
                break;

            case 0x7B:  // all notes off
                ctrltype = C_allnotesoff;
                setMidiController(channel, ctrltype, 0);
                break;

            case 0x80: // note-off
                note = midi.data[1];
                //velocity = midi.data[2];
                setMidiNote(channel, note);
                break;

            case 0x90: // note-on
                if ((note = midi.data[1])) // skip note == 0
                {
                    velocity = midi.data[2];
                    setMidiNote(channel, note, velocity);
                }
                break;

            case 0xB0: // controller
                ctrltype = getMidiController(midi.data[1]);
                par = midi.data[2];
                setMidiController(channel, ctrltype, par);
                break;

            case 0xE0: // pitch bend
                ctrltype = C_pitchwheel;
                par = ((midi.data[2] << 7) | midi.data[1]) - 8192;
                setMidiController(channel, ctrltype, par);
                break;

            case 0xF0: // system exclusive
                break;

            default: // wot, more?
                if (Runtime.settings.verbose)
                    cerr << "other event: " << (int)ev << endl;
                break;
        }
    }
    return NULL;
}


int JackEngine::_xrunCallback(void *arg)
{
    Runtime.settings.verbose && cerr << "Jack reports xrun" << endl;
    return 0;
}

void JackEngine::_errorCallback(const char *msg)
{
    Runtime.settings.verbose && cerr << "Jack reports error: " << msg << endl;
}

void JackEngine::_infoCallback(const char *msg)
{
    Runtime.settings.verbose && cerr << "Jack info message: " << msg << endl;
}
