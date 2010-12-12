/*
    AlsaEngine.cpp

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

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/AlsaEngine.h"

AlsaEngine::AlsaEngine()
{
    audio.handle = NULL;
    audio.period_time = 0;
    audio.samplerate = 0;
    audio.buffer_size = 0;
    audio.period_size = 0;
    audio.buffer_size = 0;
    audio.pThread = 0;

    midi.handle = NULL;
    midi.timerhandle = NULL;
    midi.decoder = NULL;
    midi.callbackHandler = NULL;
}


bool AlsaEngine::openAudio(WavRecord *recorder)
{
    audio.device = Runtime.audioDevice;
    audio.samplerate = Runtime.Samplerate;
    audio.period_size = Runtime.Buffersize;
    audio.period_time =  audio.period_size * 1000000.0f / audio.samplerate;
    if (!alsaBad(snd_pcm_open(&audio.handle, audio.device.c_str(),
                              SND_PCM_STREAM_PLAYBACK, SND_PCM_NO_AUTO_CHANNELS),
                 "failed to open alsa audio device:" + audio.device))
    {
        if (!alsaBad(snd_pcm_nonblock(audio.handle, 0), "set blocking failed"))
            if (prepHwparams())
                if (prepSwparams())
                {
                    snd_pcm_info_t *pcminfo;
                    snd_pcm_info_alloca(&pcminfo);
                    if (!alsaBad(snd_pcm_info(audio.handle, pcminfo), "Failed to get pcm info"))
                    {
                        // nah. audioclientid =  = Runtime.string2int(string(snd_pcm_info_get_id(pcminfo)));
                        audioclientname = string(snd_pcm_info_get_name(pcminfo));
                        audiolatency = audio.period_size;
                        wavRecorder = recorder;
                        return MusicIO::prepAudio(true);
                    }
                }
        Close();
    }
    return false;
}


bool AlsaEngine::openMidi(WavRecord *recorder) // do openAudio() before openMidi()!
{
    midi.timerhandle = NULL;
    midi.device = Runtime.midiDevice;
    string timerstr;
    long resolution = 0;
    long bestresolution = 9999999999999l;
    int ticks;
    const char* port_name = "midi in";

    if (snd_seq_open(&midi.handle, midi.device.c_str(), SND_SEQ_OPEN_INPUT, 0))
    {
        Runtime.Log("Error, failed to open alsa midi device: " + midi.device);
        goto bail_out;
    }
    if (alsaBad(snd_seq_nonblock(midi.handle, 1), "Failed to set non-blocking on midi"))
        goto bail_out;

    // midi decoder
    if (alsaBad(snd_midi_event_new(MAX_MIDI_BYTES, &midi.decoder), "Failed to allocate alsa event decoder"))
        goto bail_out;
    snd_midi_event_reset_decode(midi.decoder);
    snd_midi_event_no_status(midi.decoder, 1);

    // midi event filters
    snd_seq_client_info_t *seqclientinfo;
    snd_seq_client_info_alloca(&seqclientinfo);
    snd_seq_get_client_info(midi.handle, seqclientinfo);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_NOTEON);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_NOTEOFF);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_CONTROLLER);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_PGMCHANGE);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_PITCHBEND);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_CONTROL14);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_NONREGPARAM);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_REGPARAM);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_RESET);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_PORT_SUBSCRIBED);
    snd_seq_client_info_event_filter_add(seqclientinfo, SND_SEQ_EVENT_PORT_UNSUBSCRIBED);
    if (alsaBad(snd_seq_set_client_info(midi.handle, seqclientinfo), "Failed to set midi event filtering"))
        goto bail_out;

    // midi timer
    snd_timer_id_t *timerid;
    snd_timer_info_t *timerinfo;
    snd_timer_id_alloca(&timerid);
    snd_timer_info_alloca(&timerinfo);
    snd_timer_id_set_class(timerid, SND_TIMER_CLASS_NONE);
    int timerClass;
    int timerSClass;
    int timerCard;
    int timerDevice;
    int timerSubDev;
    snd_timer_query_t *timerquery;
    if (alsaBad(snd_timer_query_open(&timerquery, "hw", 0), "Alsa midi timers query failed"))
        goto bail_out;
    while (snd_timer_query_next_device(timerquery, timerid) >= 0)
    {
        if ((timerClass = snd_timer_id_get_class(timerid)) < 0)
            break;
        if ((timerSClass = snd_timer_id_get_sclass(timerid)) < 0)
            timerSClass = 0;
        if ((timerCard = snd_timer_id_get_card(timerid)) < 0)
            timerCard = 0;
        if ((timerDevice = snd_timer_id_get_device(timerid)) < 0)
            timerDevice = 0;
        if ((timerSubDev = snd_timer_id_get_subdevice(timerid)) < 0)
            timerSubDev = 0;
        timerstr = "hw:CLASS=" + asString(timerClass)
            + ",SCLASS=" + asString(timerSClass) + ",CARD=" + asString(timerCard)
            + ",DEV=" + asString(timerDevice) + ",SUBDEV=" + asString(timerSubDev);
        snd_timer_t *timerhandle;
        if (snd_timer_open(&timerhandle, timerstr.c_str(), SND_TIMER_OPEN_NONBLOCK) >= 0)
        {
            snd_timer_info_t *timerhandleInfo;
            snd_timer_info_alloca(&timerhandleInfo);
            if (snd_timer_info(timerhandle, timerhandleInfo) >= 0)
            {
                if (!snd_timer_info_is_slave(timerhandleInfo))
                {
                    resolution = snd_timer_info_get_resolution(timerhandleInfo);
                    if (resolution < bestresolution)
                    {
                        bestresolution = resolution;
                        midi.timerhandle = timerhandle;
                    }
                    else
                        snd_timer_close(timerhandle);
                }
                else
                    snd_timer_close(timerhandle);
            }
        }
    }
    snd_timer_query_close(timerquery);
    if (!midi.timerhandle)
    {
        Runtime.Log("Failed to find a timer for midi");
        goto bail_out;
    }
    snd_timer_params_t *timerparams;
    snd_timer_params_alloca(&timerparams);
    snd_timer_params_set_auto_start(timerparams, 1);
    ticks = 1000000000 / resolution / 1000; // approx 1 ms period ???
    snd_timer_params_set_ticks(timerparams, (ticks > 0) ? ticks : 1);
    if (snd_timer_params_get_ticks(timerparams) < 1)
        snd_timer_params_set_ticks(timerparams, 1);
    Runtime.Log("Midi timer: resolution " + asString(resolution) + ", ticks "
                + asString(snd_timer_params_get_ticks(timerparams)));
    snd_timer_params_set_exclusive(timerparams, 1);
    if (alsaBad(snd_async_add_timer_handler(&midi.callbackHandler, midi.timerhandle,
                                            _midiTimerCallback, this),
                "Failed to set midi timer callback"))
        goto bail_out;
    if (alsaBad(snd_timer_params(midi.timerhandle, timerparams),
                "Failed to set timer params"))
        goto bail_out;
    Runtime.Log("Midi timer set to " + asString(snd_timer_params_get_ticks(timerparams)));

    midi.portId = snd_seq_create_simple_port(midi.handle, port_name,
                                             SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                                             SND_SEQ_PORT_TYPE_SYNTH);
    if (alsaBad(midi.portId, "Error, failed to acquire alsa midi port"))
        goto bail_out;

    snd_seq_port_info_t  *portinfo;
    snd_seq_port_info_alloca(&portinfo);
    if (alsaBad(snd_seq_get_port_info(midi.handle, midi.portId, portinfo),
                "Failed to get port info"))
        goto bail_out;
    alsaBad(snd_seq_set_port_info(midi.handle, midi.portId, portinfo),
            "Failed to set midi port info, expect the worst");
    midiclientid  = snd_seq_client_id(midi.handle);
    midiclientname = baseclientname;
    snd_seq_set_client_name(midi.handle, midiclientname.c_str());
    wavRecorder = recorder;
    return true;

bail_out:
    Runtime.Log("AlsaEngine::openMidi() bails out");
    Close();
    return false;
}


bool AlsaEngine::Start(void)
{
    if (NULL != audio.handle && !Runtime.startThread(&audio.pThread, _audioThread, this, true, false))
        goto bail_out;
    if (midi.timerhandle && alsaBad(snd_timer_start(midi.timerhandle), "Failed to start midi timer"))
    {
        Runtime.Log("Failed to start midi timer");
        goto bail_out;
    }
    return MusicIO::Start();

bail_out:
    Runtime.Log("Error - bail out of AlsaEngine::Start()");
    Close();
    return false;
}


void AlsaEngine::Close(void)
{
    if (audio.handle && audio.pThread && pthread_cancel(audio.pThread))
        Runtime.Log("Error, failed to cancel Alsa audio thread");
    if (audio.handle)
        alsaBad(snd_pcm_close(audio.handle), "close pcm failed");
    audio.handle = NULL;
    if (midi.handle && snd_seq_close(midi.handle) < 0)
        Runtime.Log("Error closing Alsa midi connection");
    midi.handle = NULL;
    if (midi.timerhandle)
    {
        snd_timer_stop(midi.timerhandle);
        snd_timer_close(midi.timerhandle);
        midi.timerhandle = NULL;
    }


}


bool AlsaEngine::prepHwparams(void)
{
    unsigned int buffer_time = audio.period_time * 4;
    unsigned int ask_samplerate = audio.samplerate;
    unsigned int ask_buffersize = audio.period_size;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16; // Alsa appends _LE/_BE? hmmm
    snd_pcm_access_t axs = SND_PCM_ACCESS_MMAP_INTERLEAVED;
    snd_pcm_hw_params_t  *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);
    if (alsaBad(snd_pcm_hw_params_any(audio.handle, hwparams),
                "alsa audio no playback configurations available"))
        goto bail_out;
    if (alsaBad(snd_pcm_hw_params_set_periods_integer(audio.handle, hwparams),
                "alsa audio cannot restrict period size to integral value"))
        goto bail_out;
    if (!alsaBad(snd_pcm_hw_params_set_access(audio.handle, hwparams, axs),
                 "alsa audio mmap not possible"))
        pcmWrite = &snd_pcm_mmap_writei;
    else
    {
        axs = SND_PCM_ACCESS_RW_INTERLEAVED;
        if (alsaBad(snd_pcm_hw_params_set_access(audio.handle, hwparams, axs),
                    "alsa audio failed to set access, both mmap and rw failed"))
            goto bail_out;
        pcmWrite = &snd_pcm_writei;
    }
    if (alsaBad(snd_pcm_hw_params_set_format(audio.handle, hwparams, format),
                "alsa audio failed to set sample format"))
        goto bail_out;
    alsaBad(snd_pcm_hw_params_set_rate_resample(audio.handle, hwparams, 1),
            "alsa audio failed to set allow resample");
    if (alsaBad(snd_pcm_hw_params_set_rate_near(audio.handle, hwparams, &audio.samplerate, NULL),
                "alsa audio failed to set sample rate (asked for " + asString(ask_samplerate) + ")"))
        goto bail_out;
    if (alsaBad(snd_pcm_hw_params_set_channels(audio.handle, hwparams, 2),
                "alsa audio failed to set channels to 2"))
        goto bail_out;
    if (!alsaBad(snd_pcm_hw_params_set_buffer_time_near(audio.handle, hwparams, &buffer_time, NULL),
                 "initial buffer time setting failed"))
    {
        if (alsaBad(snd_pcm_hw_params_get_buffer_size(hwparams, &audio.buffer_size),
                    "alsa audio failed to get buffer size"))
            goto bail_out;
        if (alsaBad(snd_pcm_hw_params_set_period_time_near(audio.handle, hwparams, &audio.period_time, NULL),
                    "failed to set period time"))
            goto bail_out;
        if (alsaBad(snd_pcm_hw_params_get_period_size(hwparams, &audio.period_size, NULL),
                    "alsa audio failed to get period size"))
            goto bail_out;
    }
    else
    {
        if (alsaBad(snd_pcm_hw_params_set_period_time_near(audio.handle, hwparams, &audio.period_time, NULL),
                    "failed to set period time"))
            goto bail_out;
        audio.buffer_size = audio.period_size * 4;
        if (alsaBad(snd_pcm_hw_params_set_buffer_size_near(audio.handle, hwparams, &audio.buffer_size),
                    "failed to set buffer size"))
            goto bail_out;
    }
    if (alsaBad(snd_pcm_hw_params (audio.handle, hwparams), "alsa audio failed to set hardware parameters"))
		goto bail_out;
    if (alsaBad(snd_pcm_hw_params_get_buffer_size(hwparams, &audio.buffer_size),
                "alsa audio failed to get buffer size"))
        goto bail_out;
    if (alsaBad(snd_pcm_hw_params_get_period_size(hwparams, &audio.period_size, NULL),
                "failed to get period size"))
        goto bail_out;
    if (ask_buffersize != audio.period_size)
        Runtime.Log("Asked for buffersize " + asString(ask_buffersize)
                    + ", Alsa dictates " + asString((unsigned int)audio.period_size));
    return true;

bail_out:
    if (audio.handle != NULL)
        Close();
    return false;
}


bool AlsaEngine::prepSwparams(void)
{
    snd_pcm_sw_params_t *swparams;
    snd_pcm_sw_params_alloca(&swparams);
	snd_pcm_uframes_t boundary;
    if (alsaBad(snd_pcm_sw_params_current(audio.handle, swparams),
                 "alsa audio failed to get swparams"))
        goto bail_out;
    if (alsaBad(snd_pcm_sw_params_get_boundary(swparams, &boundary),
                "alsa audio failed to get boundary"))
        goto bail_out;
    if (alsaBad(snd_pcm_sw_params_set_start_threshold(audio.handle, swparams, boundary + 1),
                "failed to set start threshold")) // explicit start, not auto start
        goto bail_out;
    if (alsaBad(snd_pcm_sw_params_set_stop_threshold(audio.handle, swparams, boundary),
               "alsa audio failed to set stop threshold"))
        goto bail_out;
    if (alsaBad(snd_pcm_sw_params(audio.handle, swparams),
                "alsa audio failed to set software parameters"))
        goto bail_out;
    return true;

bail_out:
    return false;
}


void *AlsaEngine::_audioThread(void *arg)
{
    return static_cast<AlsaEngine*>(arg)->audioThread();
}


void *AlsaEngine::audioThread(void)
{
    if (NULL == audio.handle)
    {
        Runtime.Log("Null pcm handle into AlsaEngine::audioThread");
        return NULL;
    }
    alsaBad(snd_pcm_start(audio.handle), "alsa audio pcm start failed");
    pthread_cleanup_push(NULL, this);
    while (Runtime.runSynth)
    {
        pthread_testcancel();
        audio.pcm_state = snd_pcm_state(audio.handle);
        if (audio.pcm_state != SND_PCM_STATE_RUNNING)
        {
            switch (audio.pcm_state)
            {
                case SND_PCM_STATE_XRUN:
                case SND_PCM_STATE_SUSPENDED:
                    if (!xrunRecover())
                        break;
                    // else fall through to ...
                case SND_PCM_STATE_SETUP:
                    if (alsaBad(snd_pcm_prepare(audio.handle),
                                "alsa audio pcm prepare failed"))
                        break;
                case SND_PCM_STATE_PREPARED:
                    alsaBad(snd_pcm_start(audio.handle), "pcm start failed");
                    break;
                default:
                    Runtime.Log("AlsaEngine::audioThread, weird SND_PCM_STATE: "
                                + asString(audio.pcm_state));
                    break;
            }
            audio.pcm_state = snd_pcm_state(audio.handle);
        }
        if (audio.pcm_state == SND_PCM_STATE_RUNNING)
        {
            getAudio();
            interleaveShorts();
            writepcm();
        }
        else
            Runtime.Log("Audio pcm still not RUNNING");
    }
    pthread_cleanup_pop(0);
    return NULL;
}


void AlsaEngine::writepcm(void)
{
    snd_pcm_uframes_t towrite = getBuffersize();
    snd_pcm_sframes_t wrote = 0;
    short int *data = interleavedShorts;
    while (towrite > 0)
    {
        wrote = pcmWrite(audio.handle, data, towrite);
        if (wrote >= 0)
        {
            if ((snd_pcm_uframes_t)wrote < towrite || wrote == -EAGAIN)
                snd_pcm_wait(audio.handle, 333);
            if (wrote > 0)
            {
                towrite -= wrote;
                data += wrote * 2;
            }
        }
        else // (wrote < 0)
        {
            switch (wrote)
            {
                case -EBADFD:
                    alsaBad(-EBADFD, "alsa audio unfit for writing");
                    break;
                case -EPIPE:
                    xrunRecover();
                    break;
                case -ESTRPIPE:
                    pcmRecover(wrote);
                    break;
                default:
                    alsaBad(wrote, "alsa audio, snd_pcm_writei ==> weird state");
                    break;
            }
            wrote = 0;
        }
    }
    __sync_add_and_fetch(&periodendframe, wrote);
    __sync_add_and_fetch(&periodstartframe, wrote);
}


void AlsaEngine::_midiTimerCallback(snd_async_handler_t *midicbh)
{
    return static_cast<AlsaEngine*>
        (snd_async_handler_get_callback_private(midicbh))->midiTimerCallback();
}


void AlsaEngine::midiTimerCallback(void)
{
    snd_seq_event_t *event;
    unsigned char midibuffer[MAX_MIDI_BYTES];
    midimessage msg;
    int events = snd_seq_event_input_pending(midi.handle, 1); // - 1;
    while (--events >= 0 && Runtime.runSynth)
    {
        int evsize;
        switch ((evsize = snd_seq_event_input(midi.handle, &event)))
        {
            case -EAGAIN:
                return;

            case -ENOSPC:
                Runtime.Log("Midi overrun, events lost");
                break;

            default:
                if (evsize > 0 && evsize <= MAX_MIDI_BYTES)
                {
                    memset(msg.bytes, 0, MAX_MIDI_BYTES);
                    long decodecount = snd_midi_event_decode(midi.decoder,
                                                             midibuffer,
                                                             MAX_MIDI_BYTES,
                                                             event);
                    switch (decodecount)
                    {
                        case -EINVAL:
                            Runtime.Log("Not a valid midi event");
                            break;
                        case -ENOENT:
                            Runtime.Log("Not a midi message");
                            break;
                        case -ENOMEM:
                            Runtime.Log("Midi message too big");
                            break;
                        default:
                            if (decodecount > 0)
                            {
                                msg.event_frame = periodstartframe; // not good enough!
                                memcpy(msg.bytes, midibuffer, MAX_MIDI_BYTES);
                                queueMidi(&msg);
                            }
                            else if (decodecount < 0)
                                Runtime.Log("Unhandled error on midi decode");
                            break;
                    }
                }
                break;
        }
        snd_seq_free_event(event);
    }
}


bool AlsaEngine::pcmRecover(int err)
{
    if (err > 0)
        err = -err;
    bool isgood = false;
    switch (err)
    {
        case -EINTR:
            isgood = true; // nuthin to see here
            break;
        case -ESTRPIPE:
            if (!alsaBad(snd_pcm_prepare(audio.handle),
                         "Error, AlsaEngine failed to recover from suspend"))
                isgood = true;
            break;
        case -EPIPE:
            if (!alsaBad(snd_pcm_prepare(audio.handle),
                         "Error, AlsaEngine failed to recover from underrun"))
                isgood = true;
            break;
        default:
            break;
    }
    return isgood;
}


bool AlsaEngine::xrunRecover(void)
{
    bool isgood = false;
    if (audio.handle != NULL)
    {
        if (!alsaBad(snd_pcm_drop(audio.handle), "pcm drop failed"))
            if (!alsaBad(snd_pcm_prepare(audio.handle), "pcm prepare failed"))
                isgood = true;
        Runtime.Log("Alsa xrun recovery "
                    + ((isgood) ? string("good") : string("not good")));
    }
    return isgood;
}


bool AlsaEngine::alsaBad(int op_result, string err_msg)
{
    bool isbad = (op_result < 0); // (op_result < 0) -> is bad -> return true
    if (isbad)
        Runtime.Log("Error, alsa audio: " +err_msg + ": " + string(snd_strerror(op_result)), true);
    return isbad;
}
