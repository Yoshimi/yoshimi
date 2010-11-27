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
    jackClient(NULL),
    midiPort(NULL),
    jackSamplerate(0),
    jackNframes(0),
    audioPortL(NULL),
    audioPortR(NULL)
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
    if (jackClient != NULL)
    {
        Runtime.setRtprio(jack_client_max_real_time_priority(jackClient));
        return true;
    }
    else
        Runtime.Log("Failed to open jack client on server " + server);
    return false;
}


bool JackEngine::openJackClient(string server)
{
    const char *clientname = baseclientname.c_str();
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
            jackClient = jack_client_open(clientname, (jack_options_t)jopts,
                                          &jstatus, Runtime.jackServer.c_str(),
                                          Runtime.jackSessionUuid.c_str());
        else
            jackClient = jack_client_open(clientname, (jack_options_t)jopts,
                                          &jstatus, Runtime.jackSessionUuid.c_str());
    }
    else if (named_server)
        jackClient = jack_client_open(clientname, jack_options_t(jopts),
                                      NULL, Runtime.jackServer.c_str());
    else
        jackClient = jack_client_open(clientname, jack_options_t(jopts), NULL);
    if (jackClient)
    {
        jackSamplerate = jack_get_sample_rate(jackClient);
        jackNframes = jack_get_buffer_size(jackClient);
        return true;
    }
    else
        Runtime.Log("Failed jack_client_open(), status: " + Runtime.asHexString((int)jstatus), true);
    return false;
}


bool JackEngine::openAudio(WavRecord *recorder)
{
    audioPortL = jack_port_register(jackClient, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    audioPortR = jack_port_register(jackClient, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (audioPortL != NULL && audioPortR != NULL)
    {
        jack_port_set_latency(audioPortL, jack_get_buffer_size(jackClient));
        jack_port_set_latency(audioPortR, jack_get_buffer_size(jackClient));
        jack_recompute_total_latency(jackClient, audioPortL);
        jack_recompute_total_latency(jackClient, audioPortR);
        jack_recompute_total_latencies(jackClient);
        audioclientname = string(jack_get_client_name(jackClient));
        audioclientid = jack_client_thread_id(jackClient);
        audiolatency = jack_port_get_latency(audioPortL);
        wavRecorder = recorder;
        return MusicIO::prepAudio(false);
    }
    else
        Runtime.Log("Failed to register jack audio ports");
    Close();
    return false;
}


bool JackEngine::openMidi(WavRecord *recorder)
{
    const char *port_name = "midi in";
    midiPort = jack_port_register(jackClient, port_name, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    if (midiPort)
    {
        jack_port_set_latency(midiPort, jack_get_buffer_size(jackClient));
        jack_recompute_total_latency(jackClient, midiPort);
        jack_recompute_total_latencies(jackClient);
        midiclientid = jack_client_thread_id(jackClient);
        midiclientname = string(jack_get_client_name(jackClient));
        midilatency = jack_port_get_latency(midiPort);
        wavRecorder = recorder;
        return true;
    }
    else
        Runtime.Log("Failed to register jack midi port");
    return false;
}


bool JackEngine::connectJackPorts(void)
{
    const char** playback_ports = jack_get_ports(jackClient, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
	if (!playback_ports)
    {
        Runtime.Log("No physical jack playback ports found.");
        return false;
	}
    int ret;
    const char *port_name = jack_port_name(audioPortL);
    if ((ret = jack_connect(jackClient, port_name, playback_ports[0])))
    {
        Runtime.Log("Failed to connect " + string(port_name) + " to jack port "
                    + string(playback_ports[0]) + ", status " + asString(ret));
        return false;
    }
    port_name = jack_port_name(audioPortR);
    if ((ret = jack_connect(jackClient, port_name, playback_ports[1])))
    {
        Runtime.Log("Failed to connect " + string(port_name) + " to jack port "
                    + string(playback_ports[1]) + ", status " + asString(ret));
        return false;
    }
    return true;
}


bool JackEngine::Start(void)
{
    jack_set_error_function(_errorCallback);
    jack_set_xrun_callback(jackClient, _xrunCallback, this);
    #if defined(JACK_SESSION)
        if (jack_set_session_callback && jack_set_session_callback(jackClient, _jsessionCallback, this))
            Runtime.Log("Set jack session callback failed", true);
    #endif
    if (jack_set_process_callback(jackClient, _processCallback, this))
    {
        Runtime.Log("JackEngine failed to set process callback");
        goto bail_out;
    }

    if (jack_activate(jackClient))
    {
        Runtime.Log("Failed to activate jack client");
        goto bail_out;
    }
    if (Runtime.connectJackaudio)
    {
        if (!audioPortL || !audioPortR)
        {
            Runtime.Log("Failed to connect null jack audio ports");
            goto bail_out;
        }
        if (!connectJackPorts())
        {
             Runtime.Log("Failed to connect jack audio ports");
             goto bail_out;
        }
    }
    return MusicIO::Start();

bail_out:
    Close();
    return false;
}


void JackEngine::Close(void)
{
    if (NULL != jackClient)
    {
        int chk;
        if (audioPortL)
        {
            if ((chk = jack_port_unregister(jackClient, audioPortL)))
                Runtime.Log("Failed to unregister audioPortL, status: " + asString(chk));
            audioPortL = NULL;
        }
        if (audioPortR)
        {
            if ((chk = jack_port_unregister(jackClient, audioPortR)))
                Runtime.Log("Failed to unregister audioPortR, status: " + asString(chk));
            audioPortR = NULL;
        }
        if (NULL != midiPort)
        {
            if ((chk = jack_port_unregister(jackClient, midiPort)))
                Runtime.Log("Failed to close jack client, status: " + asString(chk));
            midiPort = NULL;
        }
        chk = jack_deactivate(jackClient);
        if (chk)
            Runtime.Log("Failed to close jack client, status: " + asString(chk));
        jackClient = NULL;
    }
    MusicIO::Close();
}


int JackEngine::_processCallback(jack_nframes_t nframes, void *arg)
{
    return static_cast<JackEngine*>(arg)->processCallback(nframes);
}


int JackEngine::processCallback(jack_nframes_t nframes)
{
    bool midiactive = midiPort != NULL;
    bool audioactive = (audioPortL != NULL && audioPortR != NULL);
    if (audioactive)
    {
        __sync_bool_compare_and_swap(&periodstartframe, periodstartframe, jack_last_frame_time(jackClient));
        __sync_bool_compare_and_swap(&periodendframe, periodendframe, periodstartframe + jackNframes);
    }
    bool okmidi = (midiactive) ? processMidi(nframes) : true;
    bool okaudio = (audioactive) ? processAudio(nframes) : true;
    return (okaudio && okmidi) ? 0 : -1;
}


bool JackEngine::processAudio(jack_nframes_t nframes)
{
    float *bufL = (float*)jack_port_get_buffer(audioPortL, nframes);
    float *bufR = (float*)jack_port_get_buffer(audioPortR, nframes);
    if (!bufL || !bufR)
    {
        Runtime.Log("Failed to get jack audio port buffers");
        return false;
    }
    memset(bufL, 0, sizeof(float) * nframes);
    memset(bufR, 0, sizeof(float) * nframes);
    getAudio();
    memcpy(bufL, zynLeft, nframes * sizeof(float));
    memcpy(bufR, zynRight, nframes * sizeof(float));
    return true;
}


bool JackEngine::processMidi(jack_nframes_t nframes)
{
    midimessage msg;
    jack_midi_event_t jEvent;
    bool ok = true;
    void *portBuf = jack_port_get_buffer(midiPort, nframes);
    if (!portBuf)
    {
        Runtime.Log("Bad jack midi port buffer", true);
        return  false;
    }
    jack_nframes_t eventCount = jack_midi_get_event_count(portBuf);
    if (eventCount > 0)
    {
        for(unsigned int idx = 0; ok && idx < eventCount; ++idx)
        {
            if(jack_midi_event_get(&jEvent, portBuf, idx))
            {
                Runtime.Log("jack midi read failed");
                ok = false;
                break;
            }
            if (!(jEvent.size && jEvent.size <= MAX_MIDI_BYTES))
                continue; // uninterested in long/empty events

            switch (jEvent.buffer[0] & 0xf0)
            {
                case MSG_noteoff:
                case MSG_noteon:
                case MSG_polyphonic_aftertouch:
                case MSG_control_change:
                case MSG_program_change:
                case MSG_pitchwheel_control:
                    msg.event_frame = jEvent.time + __sync_or_and_fetch(&periodstartframe, 0);
                    msg.bytes[0] = jEvent.buffer[0];
                    msg.bytes[1] = (jEvent.size > 1) ? jEvent.buffer[1] : 0;
                    msg.bytes[2] = (jEvent.size > 2) ? jEvent.buffer[2] : 0;
                    queueMidi(&msg);
                    break;
                default:
                    break;
            }
        }
    }
    return ok;
}


int JackEngine::_xrunCallback(void *arg)
{
    Runtime.Log("Jack xrun");
    return 0;
}


void JackEngine::_errorCallback(const char *msg)
{
    Runtime.Log(string(msg));
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
