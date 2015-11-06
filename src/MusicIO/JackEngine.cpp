/*
    JackEngine.cpp

    Copyright 2009-2011, Alan Calvert
    Copyright 2014, Will Godfrey

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
    
    Modified October 2014
*/

#include <errno.h>
#include <unistd.h>
#include <jack/midiport.h>
#include <jack/thread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sstream>
#include <stdio.h>

using namespace std;

#include "Misc/Config.h"
#include "MusicIO/JackEngine.h"

// NSM instance name
extern char *instance_name;

JackEngine::JackEngine(SynthEngine *_synth) : MusicIO(_synth), jackClient(NULL)
{
    audio.jackSamplerate = 0;
    audio.jackNframes = 0;
    for (int i = 0; i < (2 * NUM_MIDI_PARTS + 2); ++i)
    {
        audio.ports[i] = NULL;
        audio.portBuffs[i] = NULL;
    }
    midi.port = NULL;
    midi.ringBuf = NULL;
    midi.pThread = 0;
}


bool JackEngine::connectServer(string server)
{
    for (int tries = 0; tries < 3 && !jackClient; ++tries)
    {
        if (!openJackClient(server) && tries < 2)
        {
            synth->getRuntime().Log("Failed to open jack client, trying again");
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
        splashMessages.push_back("Can't connect to jack audio :(");
    }
    return false;
}


bool JackEngine::openJackClient(string server)
{
    int jopts = JackNullOption;
    jack_status_t jstatus;
    // NSM sets the name when running
    string clientname = instance_name ? instance_name : "yoshimi";
    if (synth->getRuntime().nameTag.size())
        clientname += ("-" + synth->getRuntime().nameTag);

    //Andrew Deryabin: for multi-instance support add unique id to
    //instances other then default (0)
    unsigned int synthUniqueId = synth->getUniqueId();
    if(synthUniqueId > 0)
    {
        char sUniqueId [256];
        memset(sUniqueId, 0, sizeof(sUniqueId));
        snprintf(sUniqueId, sizeof(sUniqueId), "%d", synthUniqueId);
        clientname += ("-" + std::string(sUniqueId));
    }

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
        synth->getRuntime().Log("Failed jack_client_open(), status: " + synth->getRuntime().asHexString((int)jstatus), true);
    return false;
}


bool JackEngine::Start(void)
{
    bool jackPortsRegistered = true;
    internalbuff = synth->getRuntime().Buffersize;
    //Andrew Deryabin: use default error callback function provided by jack
    //jack_set_error_function(_errorCallback);
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

    if (midi.port && !synth->getRuntime().startThread(&midi.pThread, _midiThread, this, true, 1, false))
    {
        synth->getRuntime().Log("Failed to start jack midi thread");
        goto bail_out;
    }

    if (!latencyPrep())
    {
        synth->getRuntime().Log("Jack latency prep failed ");
        goto bail_out;
    }

    /*
    for (int port = 0; port < (2 * NUM_MIDI_PARTS + 2); ++port) // include mains
    {
        if (!audio.ports[port])
        {
            jackPortsRegistered = false;
            break;
        }
    }
    */

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
    
    // style-wise I think the next bit is the wrong place
    if (synth->getRuntime().midiEngine  == jack_midi and synth->getRuntime().midiDevice.size() and jack_connect(jackClient,synth->getRuntime().midiDevice.c_str(),jack_port_name(midi.port)))
    {
        synth->getRuntime().Log("Didn't find jack MIDI source '" + synth->getRuntime().midiDevice + "'");
        synth->getRuntime().midiDevice = "";
    }
    
    return true;

bail_out:
    Close();
    return false;
}


void JackEngine::Close(void)
{
    if(synth->getRuntime().runSynth)
    {
        synth->getRuntime().runSynth = false;
    }

    if(midi.pThread != 0) //wait for midi thread to finish
    {
        if (sem_post(&midiSem) == 0)
        {
            void *ret = NULL;
            pthread_join(midi.pThread, &ret);
            midi.pThread = 0;
        }
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
        if (NULL != midi.ringBuf)
        {
            jack_ringbuffer_free(midi.ringBuf);
            midi.ringBuf = NULL;
        }
        jackClient = NULL;
    }
}

void JackEngine::registerJackPort(int partnum)
{
    int portnum = partnum * 2;
    if(partnum >=0 && partnum < NUM_MIDI_PARTS)
    {
        if(audio.ports [portnum] != NULL)
        {
            //port already registered
            //synth->getRuntime().Log("Jack port " + asString(partnum) + " already registered!");
            return;
        }
        if(synth->part [partnum] && synth->part [partnum]->Penabled)
        {
            string portName = "track_" + asString(partnum + 1) + "_r";
            audio.ports[portnum + 1] = jack_port_register(jackClient, portName.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            portName = "track_" + asString(partnum + 1) + "_l";
            audio.ports[portnum] = jack_port_register(jackClient, portName.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

            if(audio.ports [portnum])
            {
                synth->getRuntime().Log("Registered jack port " + asString(partnum));
            }
            else
            {
                synth->getRuntime().Log("Error registering jack port " + asString(partnum));
            }
        }
    }
}

bool JackEngine::openAudio(void)
{
    // Register mixed outputs
    audio.ports[2 * NUM_MIDI_PARTS] = jack_port_register(jackClient, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    audio.ports[2 * NUM_MIDI_PARTS + 1] = jack_port_register(jackClient, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    // And individual parts
/* this section is now reduntant!
    for (int port = 0; port < NUM_MIDI_PARTS; ++port)
    {
        //stringstream portName;
        //portName << "track_" << ((port / 2) + 1) << ((port % 2) ? "_r" : "_l");
        //audio.ports[port] = jack_port_register(jackClient, portName.str().c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        registerJackPort(port);
    }
*/
    bool jackPortsRegistered = true;
    /*for (int port = 0; port < (2 * NUM_MIDI_PARTS + 2); ++port)
    {        
        if (!audio.ports[port])
        {
            jackPortsRegistered = false;
            break;
        }
    }*/

    if (jackPortsRegistered)
        return prepBuffers() && latencyPrep();
    else
        synth->getRuntime().Log("Failed to register jack audio ports");
    Close();
    return false;
}


bool JackEngine::openMidi(void)
{
    const char *port_name = "midi in";
    midi.port = jack_port_register(jackClient, port_name,
                                   JACK_DEFAULT_MIDI_TYPE,
                                   JackPortIsInput, 0);
    if (!midi.port)
    {
        synth->getRuntime().Log("Failed to register jack midi port");
        return false;
    }
    midi.ringBuf = jack_ringbuffer_create(sizeof(struct midi_event) * 4096);
    if (!midi.ringBuf)
    {
        synth->getRuntime().Log("Failed to create jack midi ringbuffer");
        return false;
    }
    if (jack_ringbuffer_mlock(midi.ringBuf))
    {
        synth->getRuntime().Log("Failed to lock memory");
        return false;
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
            synth->getRuntime().Log("Cannot connect " + string(port_name)
                        + " to jack port " + string(playback_ports[port])
                        + ", status " + asString(ret));
            return false;
        }
    }
    return true;
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
        synth->getRuntime().Log("clientName() with null jackClient");
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

    if (midi.port)
        okmidi = processMidi(nframes);
    if (audio.ports[0] && audio.ports[1])
        okaudio = processAudio(nframes);
    return (okaudio && okmidi) ? 0 : -1;
}


bool JackEngine::processAudio(jack_nframes_t nframes)
{
    // Part buffers
    for (int port = 0; port < 2 * NUM_MIDI_PARTS; ++port)
    {
        if(audio.ports [port])
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
    //synth->getRuntime().Log("Jack " + asString(nframes) + "  Internal " + asString(internalbuff));
    int framesize = sizeof(float) * nframes;
    if (nframes < internalbuff)
    {
        synth->MasterAudio(zynLeft, zynRight, nframes);
        sendAudio(framesize, 0);
    }
    else
    {
        for (unsigned int pos = 0; pos < nframes; pos += internalbuff)
        {
        synth->MasterAudio(zynLeft, zynRight, 0);
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
        if(audio.ports [idx])
        {
            if (jack_port_connected(audio.ports[idx])) // just a few % improvement.
            {
                float *lpoint = audio.portBuffs[idx] + offset;
                float *rpoint = audio.portBuffs[idx + 1] + offset;
                if ((synth->part[port]->Paudiodest & 2) && port < currentmax)
                {
                    
//                    memcpy(audio.portBuffs[idx], zynLeft[port], framesize);
//                    memcpy(audio.portBuffs[idx + 1], zynRight[port], framesize);
                    memcpy(lpoint, zynLeft[port], framesize);
                    memcpy(rpoint, zynRight[port], framesize);
                }
                else
                {
//                    memset(audio.portBuffs[idx], 0, framesize);
//                    memset(audio.portBuffs[idx + 1], 0, framesize);
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
//    memcpy(audio.portBuffs[2 * NUM_MIDI_PARTS], zynLeft[NUM_MIDI_PARTS], framesize);
//    memcpy(audio.portBuffs[2 * NUM_MIDI_PARTS + 1], zynRight[NUM_MIDI_PARTS], framesize);
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
    unsigned int act_write;
    struct midi_event event;
    char *data;
    unsigned int wrote = 0;
    unsigned int tries = 0;
    jack_midi_event_t jEvent;
    jack_nframes_t eventCount = jack_midi_get_event_count(portBuf);
    for(idx = 0; idx < eventCount; ++idx)
    {
        if(!jack_midi_event_get(&jEvent, portBuf, idx))
        {
            if (jEvent.size < 1 || (jEvent.size > sizeof(event.data)))
                continue; // no interest in zero sized or long events
            event.time = jEvent.time;
            memset(event.data, 0, sizeof(event.data));
            memcpy(event.data, jEvent.buffer, jEvent.size);
            wrote = 0;
            tries = 0;
            data = (char*)&event;
            while (wrote < sizeof(struct midi_event) && tries < 3)
            {
                act_write = jack_ringbuffer_write(midi.ringBuf, (const char*)data,
                                                  sizeof(struct midi_event) - wrote);
                wrote += act_write;
                data += act_write;
                ++tries;
            }
            if (wrote == sizeof(struct midi_event))
            {
                if (sem_post(&midiSem) < 0)
                    synth->getRuntime().Log("processMidi semaphore post error, "
                                + string(strerror(errno)));
            }
            else
            {
                synth->getRuntime().Log("Bad write to midi ringbuffer: "
                            + asString(wrote) + " / "
                            + asString((int)sizeof(struct midi_event)));
                return false;
            }
        }
        else
            synth->getRuntime().Log("... jack midi read failed");
    }
    return true;
}




int JackEngine::_xrunCallback(void *arg)
{
    ((JackEngine *)arg)->synth->getRuntime().Log("xrun reported");
    return 0;
}


void JackEngine::_errorCallback(const char *msg)
{
    //synth->getRuntime().Log("Jack reports error: " + string(msg));
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
    if (sem_init(&midiSem, 0, 0) < 0)
    {
        synth->getRuntime().Log("Error on jack midi sem_init " + string(strerror(errno)));
        return NULL;
    }

    while (synth->getRuntime().runSynth)
    {
        if (sem_wait(&midiSem) < 0)
        {
            synth->getRuntime().Log("midiThread semaphore wait error, "
                        + string(strerror(errno)));
            continue;
        }
        if (!synth->getRuntime().runSynth)
            break;
        fetch = jack_ringbuffer_read(midi.ringBuf, (char*)&midiEvent, sizeof(struct midi_event));
        if (fetch != sizeof(struct midi_event))
        {
            synth->getRuntime().Log("Short ringbuffer read, " + asString(fetch) + " / "
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
                
            case 0xC0: // program change
                ctrltype = C_programchange;
                par = midiEvent.data[1];
                setMidiProgram(channel, par);
                break;

            case 0xE0: // pitch bend
                ctrltype = C_pitchwheel;
                par = ((midiEvent.data[2] << 7) | midiEvent.data[1]) - 8192;
                setMidiController(channel, ctrltype, par);
                break;

            case 0xF0: // system exclusive
                break;

            default: // wot, more? commented out some progs spam us :(
                // synth->getRuntime().Log("other event: " + asString((int)ev));
                break;
        }
    }
    sem_destroy(&midiSem);
    return NULL;
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

    if (jack_port_set_latency && audio.ports[0] && audio.ports[1])
    {
        jack_port_set_latency(audio.ports[0], jack_get_buffer_size(jackClient));
        jack_port_set_latency(audio.ports[1], jack_get_buffer_size(jackClient));
        if (jack_recompute_total_latencies)
            jack_recompute_total_latencies(jackClient);
    }
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
    string uuid = string(event->client_uuid);
    string filename = string("yoshimi-") + uuid + string(".xml");
    string filepath = string(event->session_dir) + filename;
    synth->getRuntime().setJackSessionSave((int)event->type, filepath);
    string cmd = synth->getRuntime().programCmd() + string(" -U ") + uuid
                 + string(" -u ${SESSION_DIR}") + filename;
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
        if (audio.ports[0] && audio.ports[1])
        {
            jack_latency_range_t range[2];
            for (int i = 0; i < 2; ++i)
            {
                jack_port_get_latency_range(audio.ports[i], mode, &range[i]);
                range[i].min++;
                range[i].max += audio.jackNframes;
                jack_port_set_latency_range(audio.ports[i], JackPlaybackLatency, &range[i]);
            }
        }
    }
}

#endif
