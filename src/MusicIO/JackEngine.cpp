/*
    JackEngine.cpp

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
#include <jack/midiport.h>
#include <jack/thread.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace std;

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/JackEngine.h"

JackEngine::JackEngine() :
    #if defined(JACK_SESSION)
        lastevent(NULL),
    #endif
    jackClient(NULL),
    jackSamplerate(0),
    jackNframes(0),
    jackaudioportL(NULL),
    jackaudioportR(NULL),
    jackmidiport(NULL),
    jackportbufL(NULL),
    jackportbufR(NULL),
    midiPthread(0),
    midiringbuf(NULL)
{ }


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
    if (!jackClient)
    {
        Runtime.Log("Failed to open jack client on server " + server);
        return false;
    }
    Runtime.setRtprio(jack_client_max_real_time_priority(jackClient));
    jackSamplerate = jack_get_sample_rate(jackClient);
    jackNframes = jack_get_buffer_size(jackClient);
    return true;
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
    if (jackmidiport && !Runtime.startThread(&midiPthread, _midiThread, this,
                                             true, true))
    {
            Runtime.Log("Failed to start jack midi thread");
            goto bail_out;
    }

    if (jack_activate(jackClient))
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
    if (jackClient)
    {
        int chk;
        if (jackaudioportL)
        {
            if ((chk = jack_port_unregister(jackClient, jackaudioportL)))
                Runtime.Log("Unregister jackaudioportL failed, status: " + asString(chk));
            jackaudioportL = NULL;
        }
        if (jackaudioportR)
        {
            if ((chk = jack_port_unregister(jackClient, jackaudioportR)))
                Runtime.Log("Unregister jackaudioportR failed, status: " + asString(chk));
            jackaudioportR = NULL;
        }
        if (jackmidiport)
        {
            if ((chk = jack_port_unregister(jackClient, jackmidiport)))
            {
                Runtime.Log("Unregister jackmidiport failed, status: " + asString(chk));
                jackmidiport = NULL;
            }
        }
        if ((chk = jack_deactivate(jackClient)))
            Runtime.Log("Deactivate jack client failed, status: " + asString(chk));
        if (midiringbuf)
        {
            jack_ringbuffer_free(midiringbuf);
            midiringbuf = NULL;
        }
        jackClient = NULL;
    }
}


bool JackEngine::openAudio(void)
{
    const char *portnames[] = { "left", "right" };
    if (!(jackaudioportL = jack_port_register(jackClient, portnames[0],
                                              JACK_DEFAULT_AUDIO_TYPE,
                                              JackPortIsOutput, 0)))
    {
        Runtime.Log("Failed to register jackaudioportL");
        goto bail_out;
    }
    if (!(jackaudioportR = jack_port_register(jackClient, portnames[1],
                                              JACK_DEFAULT_AUDIO_TYPE,
                                              JackPortIsOutput, 0)))
    {
        Runtime.Log("Failed to register jackaudioportR");
        goto bail_out;
    }
    if (prepBuffers(false))
    {
        if (jack_port_set_latency && jack_port_get_latency)
        {
            jack_port_set_latency (jackaudioportL, jack_get_buffer_size(jackClient));
            jack_port_set_latency (jackaudioportR, jack_get_buffer_size(jackClient));
            if (jack_recompute_total_latencies)
                jack_recompute_total_latencies(jackClient);
        }
        if (jack_port_get_latency)
            audioLatency = jack_port_get_latency(jackaudioportL);
        return true;
    }

bail_out:
    if (jackaudioportL)
            jack_port_unregister(jackClient, jackaudioportL);
    if (jackaudioportR)
            jack_port_unregister(jackClient, jackaudioportR);
    jackaudioportL = jackaudioportR = NULL;
    return false;
}


bool JackEngine::openMidi(void)
{
    const char *port_name = "midi in";
    if (!(jackmidiport = jack_port_register(jackClient, port_name,
                                            JACK_DEFAULT_MIDI_TYPE,
                                            JackPortIsInput, 0)))
    {
        Runtime.Log("Failed to register jack midi port");
        goto bail_out;
    }
    if (!(midiringbuf = jack_ringbuffer_create(sizeof(struct midi_event) * 4096)))
    {
        Runtime.Log("Failed to create jack midi ringbuffer");
        goto bail_out;
    }
    if (jack_port_set_latency)
        jack_port_set_latency(jackmidiport, jack_get_buffer_size(jackClient));
    if (jack_recompute_total_latencies)
        jack_recompute_total_latencies(jackClient);
    if (jack_port_get_latency)
        midiLatency = jack_port_get_latency(jackmidiport);
    return true;
    
bail_out:
    if (jackmidiport)
    {
        jack_port_unregister(jackClient, jackmidiport);
        jackmidiport = NULL;    
    }
    if (midiringbuf)
    {
        jack_ringbuffer_free(midiringbuf);
        midiringbuf = NULL;
    }
    return false;
}


int JackEngine::clientId(void)
{
    if (jackClient)
        return jack_client_thread_id(jackClient);
    else
        return -1;
}


string JackEngine::clientName(void)
{
    if (jackClient)
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

    if (jackmidiport)
        okmidi = processMidi(nframes);
    if (jackaudioportL && jackaudioportR)
        okaudio = processAudio(nframes);
    return (okaudio && okmidi) ? 0 : -1;
}


bool JackEngine::processAudio(jack_nframes_t nframes)
{
    jackportbufL = (float*)jack_port_get_buffer(jackaudioportL, nframes);
    jackportbufR = (float*)jack_port_get_buffer(jackaudioportR, nframes);
    if (!jackportbufL || !jackportbufR)
    {
            Runtime.Log("Failed to get jack audio port buffers");
            return false;
    }
    memset(jackportbufL, 0, sizeof(float) * nframes);
    memset(jackportbufR, 0, sizeof(float) * nframes);
    if (synth)
    {
        getAudio();
        memcpy(jackportbufL, zynLeft, sizeof(float) * nframes);
        memcpy(jackportbufR, zynRight, sizeof(float) * nframes);
    }
    return true;
}


bool JackEngine::processMidi(jack_nframes_t nframes)
{
    void *portBuf = jack_port_get_buffer(jackmidiport, nframes);
    if (!portBuf)
    {
        Runtime.Log("Bad get jack midi port buffer");
        return  false;
    }
    unsigned int idx;
    unsigned int act_write;
    struct midi_event event;
    char *data;
    unsigned int wrote = 0;
    unsigned int tries = 0;
    jack_midi_event_t jEvent;
    jack_nframes_t eventCount = jack_midi_get_event_count(portBuf);
    for(idx = 0; idx < eventCount; ++idx)
    {
        if (jack_midi_event_get(&jEvent, portBuf, idx))
        {
            Runtime.Log("Jack midi event get failed");
            continue;
        }
        if (jEvent.size < 1 || jEvent.size > sizeof(event.data))
            continue; // no interest in zero sized or long events
        event.time = jEvent.time;
        memset(event.data, 0, sizeof(event.data));
        memcpy(event.data, jEvent.buffer, jEvent.size);
        wrote = tries = 0;
        data = (char*)&event;
        while (wrote < sizeof(struct midi_event) && tries < 3)
        {
            act_write = jack_ringbuffer_write(midiringbuf, (const char*)data,
                                              sizeof(struct midi_event) - wrote);
            wrote += act_write;
            data += act_write;
            ++tries;
        }
        if (wrote == sizeof(struct midi_event))
        {
            if (sem_post(&midisem) < 0)
                Runtime.Log("processMidi semaphore post error, "
                            + string(strerror(errno)));
        }
        else
        {
            Runtime.Log("Bad write to midi ringbuffer: "
                        + asString(wrote) + " / "
                        + asString((int)sizeof(struct midi_event)));
            return false;
        }
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
    int ctrltype;
    int par = 0;
    unsigned int fetch;
    unsigned int ev;
    struct midi_event midiEvent;
    struct timespec waitperiod = { 0, 50000000 };
    if (sem_init(&midisem, 0, 0) < 0)
    {
        Runtime.Log("Error on jack midi sem_init " + string(strerror(errno)));
        return false;
    }
    while (Runtime.runSynth)
    {
        /*
        if (sem_timedwait(&midisem, &waitperiod) < 0)
        {
            switch (errno)
            {
                case ETIMEDOUT:
                    continue;
                default:
                    Runtime.Log("midiThread semaphore timedwait error, "
                                + string(strerror(errno)));
                    continue;
            }
        }
        */
        if (sem_wait(&midisem) < 0)
        {
            Runtime.Log("midiThread semaphore wait error, "
                        + string(strerror(errno)));
            continue;
        }
        fetch = jack_ringbuffer_read(midiringbuf, (char*)&midiEvent, sizeof(struct midi_event));
        if (fetch != sizeof(struct midi_event))
        {
            Runtime.Log("Short ringbuffer read, " + asString(fetch) + " / "
                        + asString((int)sizeof(struct midi_event)));
            continue;
        }
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
    sem_destroy(&midisem);
    return NULL;
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
        jack_session_event_free(lastevent);
        return ok;
    }
#endif
