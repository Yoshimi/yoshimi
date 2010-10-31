/*
    JackEngine.cpp

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

#include <errno.h>
#include <jack/midiport.h>
#include <jack/thread.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/JackEngine.h"

JackEngine::JackEngine() :
    MusicIO(),
    #if defined(JACK_SESSION)
        lastevent(NULL),
    #endif
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
    midi.ringBuf = NULL;
    midi.pThread = 0;
    midi.semName.clear();
    midi.eventsUp = NULL;
}


bool JackEngine::connectServer(string server)
{
    for (int tries = 0; tries < 3 && !jackClient; ++tries)
    {
        if (!openJackClient(server) && tries < 2)
        {
            Runtime.Log("Failed to open jack client, trying again");
            usleep(3333);
        }
    }
    if (jackClient != NULL)
    {
        Runtime.setRtprio(jack_client_max_real_time_priority(jackClient));
        audio.jackSamplerate = jack_get_sample_rate(jackClient);
        audio.jackNframes = jack_get_buffer_size(jackClient);
        return true;
    }
    else
        Runtime.Log("Failed to open jack client on server " + server);
    return false;
}


bool JackEngine::openJackClient(string server)
{
    string clientname = "yoshimi";
    if (!Runtime.nameTag.empty())
        clientname += ("-" + Runtime.nameTag);
    bool named_server = !server.empty() && server.compare("default");
    int jopts = ((named_server) ? JackServerName : JackNullOption)              // JackNoStartServer = 0x01
                 | ((Runtime.startJack) ? JackNullOption : JackNoStartServer);  // JackServerName = 0x04
    jack_status_t jstatus;                                                      // JackSessionID = 0x20
    if (Runtime.doRestoreJackSession && !Runtime.jackSessionUuid.empty())
    {
        #if defined(JACK_SESSION)
            jopts |= JackSessionID;
        #endif
        if (named_server)
            jackClient = jack_client_open(clientname.c_str(), (jack_options_t)jopts,
                                          &jstatus, Runtime.jackServer.c_str(),
                                          Runtime.jackSessionUuid.c_str());
        else
            jackClient = jack_client_open(clientname.c_str(), (jack_options_t)jopts,
                                          &jstatus, Runtime.jackSessionUuid.c_str());
    }
    else if (named_server)
        jackClient = jack_client_open(clientname.c_str(), jack_options_t(jopts),
                                      NULL, Runtime.jackServer.c_str());
    else
        jackClient = jack_client_open(clientname.c_str(), jack_options_t(jopts), NULL);
    if (jackClient)
        return true;
    else
        Runtime.Log("Failed jack_client_open(), status: " + Runtime.asHexString((int)jstatus), true);
    return false;
}


bool JackEngine::Start(void)
{
    jack_set_error_function(_errorCallback);
    jack_set_xrun_callback(jackClient, _xrunCallback, this);

    #if defined(JACK_SESSION)
        if (jack_set_session_callback && jack_set_session_callback(jackClient, _jsessionCallback, this))
            Runtime.Log("Set jack session callback failed");
    #endif

    if (jack_set_process_callback(jackClient, _processCallback, this))
    {
        Runtime.Log("JackEngine failed to set process callback");
        goto bail_out;
    }

    synth->actionLock(lockmute);
    if (NULL != midi.port)
    {
        int chk = 999;
        pthread_attr_t attr;
        if (setThreadAttributes(&attr, true, true))
        {
            if ((chk = pthread_create(&midi.pThread, &attr, _midiThread, this)))
                Runtime.Log("Failed to start jack midi thread (sched_fifo): " + asString(chk));
        }
        if (chk)
        {
            if (!setThreadAttributes(&attr, false))
                goto bail_out;
            if ((chk = pthread_create(&midi.pThread, &attr, _midiThread, this)))
            {
                Runtime.Log("Failed to start jack midi thread (sched_other): " + asString(chk));
                goto bail_out;
            }
        }
    }
    if (!jack_activate(jackClient)
        && NULL != audio.ports[0]
        && NULL != audio.ports[1])
    {
        if (Runtime.connectJackaudio && !connectJackPorts())
        {
            Runtime.Log("Failed to connect jack audio ports");
            goto bail_out;
        }
    }
    else
    {
        Runtime.Log("Failed to activate jack client");
        goto bail_out;
    }
    synth->actionLock(unlock);
    return true;

bail_out:
    synth->actionLock(unlock);
    Close();
    return false;
}


void JackEngine::Close(void)
{
    if (NULL != midi.port && midi.pThread)
        if (pthread_cancel(midi.pThread))
            Runtime.Log("Failed to cancel Jack midi thread");
    if (NULL != jackClient)
    {
        int chk;
        for (int chan = 0; chan < 2; ++chan)
        {
            if (NULL != audio.ports[chan])
                jack_port_unregister(jackClient, audio.ports[chan]);
            audio.ports[chan] = NULL;
        }
        if (NULL != midi.port)
        {
            if ((chk = jack_port_unregister(jackClient, midi.port)))
                Runtime.Log("Failed to close jack client, status: " + asString(chk));
            midi.port = NULL;
        }
        chk = jack_deactivate(jackClient);
        if (chk)
            Runtime.Log("Failed to close jack client, status: " + asString(chk));
        if (NULL != midi.ringBuf)
        {
            jack_ringbuffer_free(midi.ringBuf);
            midi.ringBuf = NULL;
        }
        jackClient = NULL;
        MusicIO::Close();
    }
}


bool JackEngine::openAudio(WavRecord *recorder)
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
        if (prepBuffers(false))
        {
            wavRecorder = recorder;
            jack_port_set_latency (audio.ports[0], jack_get_buffer_size(jackClient));
            jack_port_set_latency (audio.ports[1], jack_get_buffer_size(jackClient));
            jack_recompute_total_latencies (jackClient);
            audioLatency = jack_port_get_latency(audio.ports[0]);
            return true;
        }
    }
    else
        Runtime.Log("Failed to register jack audio ports");
    Close();
    return false;
}


bool JackEngine::openMidi(WavRecord *recorder)
{
    if (NULL != jackClient)
    {
        midi.ringBuf =
            jack_ringbuffer_create(sizeof(struct midi_event) * 4096);
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
                {
                    wavRecorder = recorder;
                    jack_port_set_latency (midi.port, jack_get_buffer_size(jackClient));
                    jack_recompute_total_latencies (jackClient);
                    midiLatency = jack_port_get_latency (midi.port);
                    return true;
                }
                else
                    Runtime.Log("Failed to register jack midi port");
            }
            else
                Runtime.Log("Failed to create jack midi semaphore "
                            + midi.semName + string(strerror(errno)));
        }
        else
            Runtime.Log("Failed to create jack midi ringbuffer");

    }
    else
        Runtime.Log("NULL jackClient through registerMidi");
    Close();
    return false;
}


bool JackEngine::connectJackPorts(void)
{
    const char** playback_ports = jack_get_ports(jackClient, NULL, NULL,
                                                 JackPortIsPhysical|JackPortIsInput);
	if (playback_ports == NULL)
    {
        Runtime.Log("No physical jack playback ports found.");
        return false;
	}
    int ret;
    for (int port = 0; port < 2 && NULL != audio.ports[port]; ++port)
    {
        const char *port_name = jack_port_name(audio.ports[port]);
        if ((ret = jack_connect(jackClient, port_name, playback_ports[port])))
        {
            Runtime.Log("Cannot connect " + string(port_name)
                        + " to jack port " + string(playback_ports[port])
                        + ", status " + asString(ret));
            return false;
        }
    }
    return true;
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
        Runtime.Log("clientName() with null jackClient");
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

    if (NULL != midi.port)
        okmidi = processMidi(nframes);
    if (NULL != audio.ports[0] && NULL != audio.ports[1])
        okaudio = processAudio(nframes);
    return (okaudio && okmidi) ? 0 : -1;
}


bool JackEngine::processAudio(jack_nframes_t nframes)
{
    for (int port = 0; port < 2; ++port)
    {
        audio.portBuffs[port] =
            (float*)jack_port_get_buffer(audio.ports[port], nframes);
        if (NULL == audio.portBuffs[port])
        {
            Runtime.Log("Failed to get jack audio port buffer: " + asString(port));
            return false;
        }
    }
    memset(audio.portBuffs[0], 0, sizeof(float) * nframes);
    memset(audio.portBuffs[1], 0, sizeof(float) * nframes);
    if (NULL != synth)
    {
        getAudio();
        memcpy(audio.portBuffs[0], zynLeft, sizeof(float) * nframes);
        memcpy(audio.portBuffs[1], zynRight, sizeof(float) * nframes);
    }
    return true;
}


bool JackEngine::processMidi(jack_nframes_t nframes)
{
    void *portBuf = jack_port_get_buffer(midi.port, nframes);
    if (NULL == portBuf)
    {
        Runtime.Log("Bad get jack midi port buffer");
        return  false;
    }

    unsigned int byt;
    unsigned int idx;
    unsigned int act_write;
    struct midi_event event;
    char *data;
    unsigned int wrote = 0;
    unsigned int tries = 0;
    int chk;
    jack_midi_event_t jEvent;
    jack_nframes_t eventCount = jack_midi_get_event_count(portBuf);
    for(idx = 0; idx < eventCount; ++idx)
    {
        if(!jack_midi_event_get(&jEvent, portBuf, idx))
        {
            // no interest in 0 size or long events
            if (jEvent.size > 0 && jEvent.size <= sizeof(event.data))
            {
                event.time = jEvent.time;
                for (byt = 0; byt < jEvent.size; ++byt)
                    event.data[byt] = jEvent.buffer[byt];
                while (byt < sizeof(event.data))
                    event.data[byt++] = 0;
                wrote = 0;
                tries = 0;
                data = (char*)&event;
                while (wrote < sizeof(struct midi_event) && tries < 3)
                {
                    act_write =
                        jack_ringbuffer_write(midi.ringBuf, (const char*)data,
                                              sizeof(struct midi_event) - wrote);
                    wrote += act_write;
                    data += act_write;
                    ++tries;
                }
                if (wrote == sizeof(struct midi_event))
                {
                    if (Runtime.runSynth && midi.eventsUp)
                    {
                        if ((chk = sem_post(midi.eventsUp)))
                        Runtime.Log("Jack midi semaphore post failed: "
                                    + string(strerror(errno)));
                    }
                }
                else
                {
                    Runtime.Log("Bad write to midi ringbuffer: "
                                + asString(wrote) + " / "
                                + asString((int)sizeof(struct midi_event)));
                    return false;
                }
            }
        }
        else
            Runtime.Log("... jack midi read failed");
    }
    return true;
}




int JackEngine::_xrunCallback(void *arg)
{
    Runtime.Log("xrun reported");
    return 0;
}


void JackEngine::_errorCallback(const char *msg)
{
    Runtime.Log("Jack error report:" + string(msg));
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
    struct midi_event midiEvent;
    pthread_cleanup_push(_midiCleanup, this);
    while (Runtime.runSynth)
    {
        pthread_testcancel();
        if ((chk = sem_wait(midi.eventsUp)))
        {
            Runtime.Log("Error on jack midi semaphore wait: "
                        + string(strerror(errno)));
            continue;
        }
        pthread_testcancel();
        fetch = jack_ringbuffer_read(midi.ringBuf, (char*)&midiEvent,
                                     sizeof(struct midi_event));
        if (fetch != sizeof(struct midi_event))
        {
            Runtime.Log("Short ringbuffer read, " + asString(fetch) + " / "
                        + asString((int)sizeof(struct midi_event)));
            continue;
        }
        pthread_testcancel();
        channel = midiEvent.data[0] & 0x0F;
        switch ((ev = midiEvent.data[0] & 0xF0))
        {
            case 0x01: // modulation wheel or lever
                ctrltype = C_modwheel;
                par = midiEvent.data[2];
                setMidiController(channel, ctrltype, par);
                break;

            case 0x07: // channel volume (formerly main volume)
                ctrltype = C_volume;
                par = midiEvent.data[2];
                setMidiController(channel, ctrltype, par);
                break;

            case 0x0B: // expression controller
                ctrltype = C_expression;
                par = midiEvent.data[2];
                setMidiController(channel, ctrltype, par);
                break;

            case 0x0C: // program change ... but how?
                Runtime.Log("How to change to program " + asString(par)
                            + " on channel " + asString(channel) + "?");
                par = midiEvent.data[2];
                break;

            case 0x42: // Sostenuto On/Off
                {
                    par = midiEvent.data[2]; // < 63 off, > 64 on
                    string errstr = string("Sostenuto ");
                    if (par < 63)
                        errstr += "off";
                    else if (par > 64)
                        errstr += "on";
                    else
                        errstr += "?";
                    Runtime.Log(errstr);
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
                note = midiEvent.data[1];
                // velocity = midiEvent.data[2];
                setMidiNote(channel, note);
                break;

            case 0x90: // note-on
                if ((note = midiEvent.data[1])) // skip note == 0
                {
                    velocity = midiEvent.data[2];
                    setMidiNote(channel, note, velocity);
                }
                break;

            case 0xB0: // controller
                ctrltype = getMidiController(midiEvent.data[1]);
                par = midiEvent.data[2];
                setMidiController(channel, ctrltype, par);
                break;

            case 0xE0: // pitch bend
                ctrltype = C_pitchwheel;
                par = ((midiEvent.data[2] << 7) | midiEvent.data[1]) - 8192;
                setMidiController(channel, ctrltype, par);
                break;

            case 0xF0: // system exclusive
                break;

            default: // wot, more?
                Runtime.Log("other event: " + asString((int)ev));
                break;
        }
    }
    pthread_cleanup_pop(1);
    return NULL;
}


void JackEngine::_midiCleanup(void *arg)
{
    static_cast<JackEngine*>(arg)->midiCleanup();
}


void JackEngine::midiCleanup(void)
{
    int chk;
    if (NULL != midi.eventsUp && (chk = sem_close(midi.eventsUp)))
        Runtime.Log("Failed to close jack midi semaphore "
                    + midi.semName + string(strerror(errno)));
    midi.eventsUp = NULL;
    if (!midi.semName.empty() && (chk = sem_unlink(midi.semName.c_str())))
        Runtime.Log("Failed to unlink jack midi semaphore "
                    + midi.semName + string(strerror(errno)));
}


#if defined(JACK_SESSION)
    void JackEngine::_jsessionCallback(jack_session_event_t *event, void *arg)
    {
        return static_cast<JackEngine*>(arg)->jsessionCallback(event);
    }
    
    
    void JackEngine::jsessionCallback(jack_session_event_t *event)
    {
        lastevent = event;
        Runtime.setJackSessionSave(event->type, event->session_dir, event->client_uuid);
    }
    
    
    bool JackEngine::jacksessionReply(string cmdline)
    {
        lastevent->command_line = strdup(cmdline.c_str());
        bool ok = !jack_session_reply(jackClient, lastevent);
        jack_session_event_free (lastevent);
        return ok;
    }
#endif
