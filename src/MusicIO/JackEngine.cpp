/*
    JackEngine.cpp

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

*/

#include <errno.h>
#include <iostream>

#include <jack/midiport.h>
#include <jack/thread.h>
#include <unistd.h>     // for usleep()

#include "Misc/Config.h"
#include "Misc/FormatFuncs.h"
#include "MusicIO/JackEngine.h"

using func::asString;
using func::asHexString;


JackEngine::JackEngine(SynthEngine *_synth, BeatTracker *_beatTracker) :
    MusicIO(_synth, _beatTracker),
    jackClient(NULL),
    bpm(120)
{
    audio.jackSamplerate = 0;
    audio.jackNframes = 0;
    for (int i = 0; i < (2 * NUM_MIDI_PARTS + 2); ++i)
    {
        audio.ports[i] = NULL;
        audio.portBuffs[i] = NULL;
    }
    midi.port = NULL;
}


bool JackEngine::connectServer(std::string server)
{
    for (int tries = 0; tries < 3 && !jackClient; ++tries)
    {
        if (!openJackClient(server) && tries < 2)
        {
            synth->getRuntime().Log("Failed to open jack client, trying again", 1);
            usleep(3333);
        }
    }
    if (jackClient)
    {
        synth->getRuntime().setRtprio(jack_client_max_real_time_priority(jackClient));
        audio.jackSamplerate = jack_get_sample_rate(jackClient);
        audio.jackNframes = jack_get_buffer_size(jackClient);
        return true;
    }
    else
    {
        synth->getRuntime().Log("Failed to open jack client on server " + server);
    }
    return false;
}


bool JackEngine::openJackClient(std::string server)
{
    int jopts = JackNullOption;
    jack_status_t jstatus;
    std::string clientname = "yoshimi";
    if (synth->getRuntime().nameTag.size())
        clientname += ("-" + synth->getRuntime().nameTag);

    //Andrew Deryabin: for multi-instance support add unique id to
    //instances other then default (0)
    unsigned int synthUniqueId = synth->getUniqueId();
    if (synthUniqueId > 0)
    {
        char sUniqueId [256];
        memset(sUniqueId, 0, sizeof(sUniqueId));
        snprintf(sUniqueId, sizeof(sUniqueId), "%d", synthUniqueId);
        clientname += ("-" + std::string(sUniqueId));
    }
    //std::cout << " C name " << clientname << std::endl;
    bool named_server = server.size() > 0 && server.compare("default") != 0;
    if (named_server)
        jopts |= JackServerName;
    if (!synth->getRuntime().startJack)
        jopts |= JackNoStartServer;
    #if defined(JACK_SESSION)
        if (synth->getRuntime().restoreJackSession && synth->getRuntime().jackSessionUuid.size())
        {
            jopts |= JackSessionID;
            if (named_server)
                jackClient = jack_client_open(clientname.c_str(), (jack_options_t)jopts,
                                              &jstatus, synth->getRuntime().jackServer.c_str(),
                                              synth->getRuntime().jackSessionUuid.c_str());
            else
                jackClient = jack_client_open(clientname.c_str(), (jack_options_t)jopts,
                                              &jstatus, synth->getRuntime().jackSessionUuid.c_str());
        }
        else
        {
            if (named_server)
                jackClient = jack_client_open(clientname.c_str(), (jack_options_t)jopts,
                                              &jstatus, synth->getRuntime().jackServer.c_str());
            else
                jackClient = jack_client_open(clientname.c_str(), (jack_options_t)jopts, &jstatus);
        }
    #else
        if (named_server)
            jackClient = jack_client_open(clientname.c_str(), (jack_options_t)jopts,
                                          &jstatus, synth->getRuntime().jackServer.c_str());
        else
            jackClient = jack_client_open(clientname.c_str(), (jack_options_t)jopts, &jstatus);
    #endif
    if (jackClient)
        return true;
    else
        synth->getRuntime().Log("Failed jack_client_open(), status: " + asHexString((int)jstatus), 1);
    return false;
}


bool JackEngine::Start(void)
{
    bool jackPortsRegistered = true;
    internalbuff = synth->getRuntime().Buffersize;
    jack_set_xrun_callback(jackClient, _xrunCallback, this);
    #if defined(JACK_SESSION)
        if (jack_set_session_callback
            && jack_set_session_callback(jackClient, _jsessionCallback, this))
            synth->getRuntime().Log("Set jack session callback failed");
    #endif

    if (jack_set_process_callback(jackClient, _processCallback, this))
    {
        synth->getRuntime().Log("JackEngine failed to set process callback");
        goto bail_out;
    }

    if (!latencyPrep())
    {
        synth->getRuntime().Log("Jack latency prep failed ");
        goto bail_out;
    }

    if (!jack_activate(jackClient) && jackPortsRegistered)
    {
        if (!synth->getRuntime().restoreJackSession && synth->getRuntime().connectJackaudio && !connectJackPorts())
        {
            synth->getRuntime().Log("Failed to connect jack audio ports");
            goto bail_out;
        }
    }
    else
    {
        synth->getRuntime().Log("Failed to activate jack client");
        goto bail_out;
    }
    /*
     * TODO fix this - now moved to where it should be.
     * Shows identical results but doesn't connect.
     * Original 1.4.1 version also fails - it used to work.
     */

    // style-wise I think the next bit is the wrong place
    /*if (synth->getRuntime().midiEngine  == jack_midi
      && !synth->getRuntime().midiDevice.empty()
      && synth->getRuntime().midiDevice != "default")
    {
        if (jack_connect(jackClient, synth->getRuntime().midiDevice.c_str(), jack_port_name(midi.port)))
        {
            synth->getRuntime().Log("Didn't find jack MIDI source '"
            + synth->getRuntime().midiDevice + "'", 1);
            synth->getRuntime().midiDevice = "";
        }
    }*/
    return true;

bail_out:
    Close();
    return false;
}


void JackEngine::Close(void)
{
    if (synth->getRuntime().runSynth)
    {
        synth->getRuntime().runSynth = false;
    }

    if (NULL != jackClient)
    {
        int chk;
        for (int chan = 0; chan < (2*NUM_MIDI_PARTS+2); ++chan)
        {
            if (NULL != audio.ports[chan])
                jack_port_unregister(jackClient, audio.ports[chan]);
            audio.ports[chan] = NULL;
        }
        if (NULL != midi.port)
        {
            if ((chk = jack_port_unregister(jackClient, midi.port)))
                synth->getRuntime().Log("Failed to close jack client, status: " + asString(chk));
            midi.port = NULL;
        }
        chk = jack_deactivate(jackClient);
        if (chk)
            synth->getRuntime().Log("Failed to close jack client, status: " + asString(chk));

        jackClient = NULL;
    }
}


void JackEngine::registerAudioPort(int partnum)
{
    int portnum = partnum * 2;
    if (partnum >=0 && partnum < NUM_MIDI_PARTS)
    {
        if (audio.ports [portnum] != NULL)
        {
            synth->getRuntime().Log("Jack port " + asString(partnum) + " already registered!", 2);
            return;
        }
        /* This has a hack to stop all enabled parts from resistering
         * individual ports (at startup) if part is not configured for
         * direct O/P.
         */
        std::string portName;
        if (synth->part [partnum] && synth->partonoffRead(partnum) && (synth->part [partnum]->Paudiodest > 1))
        {
            portName = "track_" + asString(partnum + 1) + "_l";
            audio.ports[portnum] = jack_port_register(jackClient, portName.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            portName = "track_" + asString(partnum + 1) + "_r";
            audio.ports[portnum + 1] = jack_port_register(jackClient, portName.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

            if (audio.ports [portnum])
            {
                synth->getRuntime().Log("Registered jack port " + asString(partnum + 1));
            }
            else
            {
                synth->getRuntime().Log("Error registering jack port " + asString(partnum + 1));
            }
        }
    }
}


bool JackEngine::openAudio(void)
{
    if (jackClient == 0)
    {
        if (!connectServer(synth->getRuntime().audioDevice))
        {
            return false;
        }
    }
    // Register mixed outputs
    audio.ports[2 * NUM_MIDI_PARTS] = jack_port_register(jackClient, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    audio.ports[2 * NUM_MIDI_PARTS + 1] = jack_port_register(jackClient, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    bool jackPortsRegistered = true;
    if (!audio.ports[2 * NUM_MIDI_PARTS] || !audio.ports[2 * NUM_MIDI_PARTS + 1])
        jackPortsRegistered = false;

    if (jackPortsRegistered)
        return prepBuffers() && latencyPrep();
    else
        synth->getRuntime().Log("Failed to register jack audio ports");
    Close();
    return false;
}


bool JackEngine::openMidi(void)
{
    if (jackClient == 0)
    {
        if (!connectServer(synth->getRuntime().midiDevice))
        {
            return false;
        }
    }

    const char *port_name = "midi in";
    midi.port = jack_port_register(jackClient, port_name,
                                   JACK_DEFAULT_MIDI_TYPE,
                                   JackPortIsInput, 0);
    if (!midi.port)
    {
        synth->getRuntime().Log("Failed to register jack midi port");
        return false;
    }

    std::cout << "client " << jackClient<< "  device " << synth->getRuntime().midiDevice << "  port " << jack_port_name(midi.port) << std::endl;
    if (jack_connect(jackClient, synth->getRuntime().midiDevice.c_str(), jack_port_name(midi.port)))
        {
            synth->getRuntime().Log("Didn't find jack MIDI source '"
            + synth->getRuntime().midiDevice + "'");
            //synth->getRuntime().midiDevice = "";
        }

    return true;
}


bool JackEngine::connectJackPorts(void)
{
    const char** playback_ports = jack_get_ports(jackClient, NULL, NULL,
                                                 JackPortIsPhysical|JackPortIsInput);
	if (!playback_ports)
    {
        synth->getRuntime().Log("No physical jack playback ports found.");
        return false;
	}
    int ret;
    for (int port = 0; port < 2 && (NULL != audio.ports[port + NUM_MIDI_PARTS*2]); ++port)
    {
        const char *port_name = jack_port_name(audio.ports[port + NUM_MIDI_PARTS * 2]);
        if ((ret = jack_connect(jackClient, port_name, playback_ports[port])))
        {
            if (ret == EEXIST)
            {
            synth->getRuntime().Log(std::string(port_name)
                        + " is already connected to jack port " + std::string(playback_ports[port])
                        + ", status " + asString(ret));
            }
            else
            {
            synth->getRuntime().Log("Cannot connect " + std::string(port_name)
                        + " to jack port " + std::string(playback_ports[port])
                        + ", status " + asString(ret));
            return false;
            }
        }
    }
    return true;
}


int JackEngine::clientId(void)
{
    if (jackClient)
        return long(jack_client_thread_id(jackClient));
    else
        return -1;
}


std::string JackEngine::clientName(void)
{
    if (jackClient)
        return std::string(jack_get_client_name(jackClient));
    else
        synth->getRuntime().Log("clientName() with null jackClient");
    return std::string("Oh, yoshimi :-(");
}


int JackEngine::_processCallback(jack_nframes_t nframes, void *arg)
{
    return static_cast<JackEngine*>(arg)->processCallback(nframes);
}


int JackEngine::processCallback(jack_nframes_t nframes)
{
    bool okaudio = true;
    bool okmidi = true;

    if (midi.port) {
        // input exists, using jack midi
        handleBeatValues(nframes);
        okmidi = processMidi(nframes);
    }
    if (audio.ports[NUM_MIDI_PARTS * 2] && audio.ports[NUM_MIDI_PARTS * 2 + 1])
        // (at least) main outputs exist, using jack audio
        okaudio = processAudio(nframes);
    return (okaudio && okmidi) ? 0 : -1;
}


bool JackEngine::processAudio(jack_nframes_t nframes)
{
    // Part buffers
    for (int port = 0; port < 2 * NUM_MIDI_PARTS; ++port)
    {
        if (audio.ports [port])
        {
            audio.portBuffs[port] =
                    (float*)jack_port_get_buffer(audio.ports[port], nframes);
            if (!audio.portBuffs[port])
            {
                synth->getRuntime().Log("Failed to get jack audio port buffer: " + asString(port));
                return false;
            }
        }
    }
    // And mixed outputs
    audio.portBuffs[2 * NUM_MIDI_PARTS] = (float*)jack_port_get_buffer(audio.ports[2 * NUM_MIDI_PARTS], nframes);
    if (!audio.portBuffs[2 * NUM_MIDI_PARTS])
    {
        synth->getRuntime().Log("Failed to get jack audio port buffer: " + asString(2 * NUM_MIDI_PARTS));
        return false;
    }
    audio.portBuffs[2 * NUM_MIDI_PARTS + 1] = (float*)jack_port_get_buffer(audio.ports[2 * NUM_MIDI_PARTS + 1], nframes);
    if (!audio.portBuffs[2 * NUM_MIDI_PARTS + 1])
    {
        synth->getRuntime().Log("Failed to get jack audio port buffer: " + asString(2 * NUM_MIDI_PARTS + 1));
        return false;
    }

    std::pair<float, float> beats(beatTracker->getBeatValues());
    int framesize;
    if (nframes <= internalbuff)
    {
        synth->setBeatValues(beats.first, beats.second);
        framesize = sizeof(float) * nframes;
        synth->MasterAudio(zynLeft, zynRight, nframes);
        sendAudio(framesize, 0);
    }
    else
    {
        framesize = sizeof(float) * internalbuff;
        for (unsigned int pos = 0; pos < nframes; pos += internalbuff)
        {
            float bpmInc = (float)pos * bpm / (audio.jackSamplerate * 60.0f);
            synth->setBeatValues(beats.first + bpmInc, beats.second + bpmInc);
            synth->MasterAudio(zynLeft, zynRight, internalbuff);
            sendAudio(framesize, pos);
        }
    }
    return true;
}


void JackEngine::sendAudio(int framesize, unsigned int offset)
{
    // Part outputs
    int currentmax = synth->getRuntime().NumAvailableParts;
    for (int port = 0, idx = 0; idx < 2 * NUM_MIDI_PARTS; port++ , idx += 2)
    {
        if (audio.ports [idx])
        {
            if (jack_port_connected(audio.ports[idx])) // just a few % improvement.
            {
                float *lpoint = audio.portBuffs[idx] + offset;
                float *rpoint = audio.portBuffs[idx + 1] + offset;
                if ((synth->part[port]->Paudiodest & 2) && port < currentmax)
                {
                    memcpy(lpoint, zynLeft[port], framesize);
                    memcpy(rpoint, zynRight[port], framesize);
                }
                else
                {
                    memset(lpoint, 0, framesize);
                    memset(rpoint, 0, framesize);
                }
            }
        }
    }
    // And mixed outputs
    float *Lpoint = audio.portBuffs[2 * NUM_MIDI_PARTS] + offset;
    float *Rpoint = audio.portBuffs[2 * NUM_MIDI_PARTS + 1] + offset;
    memcpy(Lpoint, zynLeft[NUM_MIDI_PARTS], framesize);
    memcpy(Rpoint, zynRight[NUM_MIDI_PARTS], framesize);
}


bool JackEngine::processMidi(jack_nframes_t nframes)
{
    void *portBuf = jack_port_get_buffer(midi.port, nframes);
    if (!portBuf)
    {
        synth->getRuntime().Log("Bad midi jack_port_get_buffer");
        return  false;
    }

    unsigned int idx;
    jack_midi_event_t jEvent;
    jack_nframes_t eventCount = jack_midi_get_event_count(portBuf);

    for (idx = 0; idx < eventCount; ++idx)
    {
        if (!jack_midi_event_get(&jEvent, portBuf, idx))
        {
            if (jEvent.size >= 1 && jEvent.size <= 4) // no interest in zero sized or long events
            {
                //std::cout << "Offset " << int(jEvent.time) << std::endl;
                setMidi(jEvent.buffer[0], jEvent.buffer[1], jEvent.buffer[2]);
            }
        }
    }
    return true;
}

void JackEngine::handleBeatValues(jack_nframes_t nframes)
{
    jack_position_t pos;
    jack_transport_state_t state = jack_transport_query(jackClient, &pos);

    std::pair<float, float> beats(beatTracker->getBeatValues());

    if (pos.valid & JackPositionBBT)
        bpm = pos.beats_per_minute;

    float bpmInc = (float)nframes * bpm
        / ((float)audio.jackSamplerate * 60.0f);

    beats.second += bpmInc;

    if (!(pos.valid & JackPositionBBT) || state == JackTransportStopped)
        // If stopped, keep oscillating.
        beats.first += bpmInc;
    else
    {
        // If rolling, sync to exact beat.
        beats.first = (float)pos.tick / (float)pos.ticks_per_beat;
        beats.first += pos.beat - 1;
        beats.first += (pos.bar - 1) * pos.beats_per_bar;
    }

    beatTracker->setBeatValues(beats);
}


int JackEngine::_xrunCallback(void *arg)
{
    ((JackEngine *)arg)->synth->getRuntime().Log("xrun reported", 2);
    return 0;
}


bool JackEngine::latencyPrep(void)
{
#if defined(JACK_LATENCY)  // >= 0.120.1 API

    if (jack_set_latency_callback(jackClient, _latencyCallback, this))
    {
        synth->getRuntime().Log("Set latency callback failed");
        return false;
    }
    return true;

#else // < 0.120.1 API

    for (int i = 0; i < 2 * NUM_MIDI_PARTS + 2; ++i)
    {
        if (jack_port_set_latency && audio.ports[i])
            jack_port_set_latency(audio.ports[i], jack_get_buffer_size(jackClient));
    }
    if (jack_recompute_total_latencies)
        jack_recompute_total_latencies(jackClient);
    return true;

#endif
}


#if defined(JACK_SESSION)

void JackEngine::_jsessionCallback(jack_session_event_t *event, void *arg)
{
    return static_cast<JackEngine*>(arg)->jsessionCallback(event);
}

void JackEngine::jsessionCallback(jack_session_event_t *event)
{
    std::string uuid = std::string(event->client_uuid);
    std::string filename = std::string("yoshimi-") + uuid + std::string(".xml");
    std::string filepath = std::string(event->session_dir) + filename;
    synth->getRuntime().setJackSessionSave((int)event->type, filepath);
    std::string cmd = synth->getRuntime().programCmd() + std::string(" -U ") + uuid
                 + std::string(" -u ${SESSION_DIR}") + filename;
    event->command_line = strdup(cmd.c_str());
    if (jack_session_reply(jackClient, event))
        synth->getRuntime().Log("Jack session reply failed");
    jack_session_event_free(event);
}

#endif


#if defined(JACK_LATENCY)

void JackEngine::_latencyCallback(jack_latency_callback_mode_t mode, void *arg)
{
    return static_cast<JackEngine*>(arg)->latencyCallback(mode);
}


void JackEngine::latencyCallback(jack_latency_callback_mode_t mode)
{
    if (mode == JackCaptureLatency)
    {
        for (int i = 0; i < 2 * NUM_MIDI_PARTS + 2; ++i)
        {
            jack_latency_range_t range;
            if (audio.ports[i])
            {
                jack_port_get_latency_range(audio.ports[i], mode, &range);
                range.min++;
                range.max += audio.jackNframes;
                jack_port_set_latency_range(audio.ports[i], JackPlaybackLatency, &range);
            }
        }
    }
}

#endif
