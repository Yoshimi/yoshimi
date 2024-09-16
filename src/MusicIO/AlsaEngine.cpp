/*
    AlsaEngine.cpp

    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2021, Will Godfrey & others

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

#if defined(HAVE_ALSA)

#include "Misc/Util.h"
#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "Misc/FormatFuncs.h"
#include "MusicIO/AlsaEngine.h"

#include <iostream>
#include <chrono>
#include <string>

using Mircos = std::chrono::duration<int64_t, std::micro>;
using std::chrono::steady_clock;
using std::chrono::floor;
using std::string;
using std::move;

using func::asString;
using util::unConst;

// The number of nanoseconds before the MIDI clock is assumed missing.
#define MIDI_CLOCK_TIMEOUT_US 1000000


AlsaEngine::AlsaEngine(SynthEngine& _synth, shared_ptr<BeatTracker> beat)
    : MusicIO{_synth, move(beat)}
    , little_endian{runtime().isLittleEndian}
    , card_endian{false}
    , card_signed{true}
    , card_chans{2}   // got to start somewhere}
    , card_bits{0}
    , pcmWrite{nullptr}
    , interleaved{}
    , audio{}
    , midi{}
{
    for (int i = 0; i < ALSA_MIDI_BPM_MEDIAN_WINDOW; i++)
        midi.prevBpms[i] = 120;

    // monotonic time scale in microseconds as signed 64bit
    auto now = steady_clock::now();
    midi.prevClockUs = floor<Mircos>(now.time_since_epoch())
                            .count();
}


bool AlsaEngine::openAudio()
{
    audio.device = runtime().audioDevice;
    audio.samplerate = runtime().samplerate;
    audio.period_size = runtime().buffersize;
    audio.period_count = 2;
    audio.buffer_size = audio.period_size * audio.period_count;
    if (not alsaBad(snd_pcm_open(&audio.handle, audio.device.c_str(),
                                 SND_PCM_STREAM_PLAYBACK, SND_PCM_NO_AUTO_CHANNELS),
                                 "failed to open alsa audio device:" + audio.device))
        if (not alsaBad(snd_pcm_nonblock(audio.handle, 0), "set blocking failed"))
            if (prepHwparams())
                if (prepSwparams())
                {
                    prepBuffers();
                    // Buffers for interleaved audio only used by AlsaEngine
                    interleaved.reset(new int[getBuffersize() * card_chans]{0});
                    return true;
                }
    // if anything did not go well...
    Close();
    return false;
}


string AlsaEngine::findMidiClients(snd_seq_t* seq)
{
    string result;
    snd_seq_client_info_t* cinfo;
    snd_seq_port_info_t* pinfo;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0)
    {
        int client = snd_seq_client_info_get_client(cinfo);

        if (client == SND_SEQ_CLIENT_SYSTEM)
            continue; // don't show system timer and announce ports
        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0)
        {
            // port must understand MIDI messages
            if (!(snd_seq_port_info_get_type(pinfo)
                  & SND_SEQ_PORT_TYPE_MIDI_GENERIC))
                continue;
            // we need both READ and SUBS_READ
            if ((snd_seq_port_info_get_capability(pinfo)
                 & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
                != (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
                continue;
            if (string{snd_seq_client_info_get_name(cinfo)} == "Midi Through")
                continue; // don't want midi through
            result = result + snd_seq_client_info_get_name(cinfo) + ":" + std::to_string(snd_seq_port_info_get_port(pinfo)) + ", ";
        }
    }
    return result;
}

bool AlsaEngine::openMidi()
{
    synth.setBPMAccurate(false);

    const char* port_name = "input";
    int port_num;
    if (snd_seq_open(&midi.handle, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) != 0)
    {
        runtime().Log("Failed to open alsa midi");
        Close();
        return false;
    }
    snd_seq_client_info_t *seq_info;
    snd_seq_client_info_alloca(&seq_info);
    snd_seq_get_client_info(midi.handle, seq_info);
    midi.alsaId = snd_seq_client_info_get_client(seq_info);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_NOTEON);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_NOTEOFF);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_KEYPRESS);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_CHANPRESS);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_CONTROLLER);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_PGMCHANGE);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_PITCHBEND);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_CONTROL14);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_NONREGPARAM);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_REGPARAM);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_RESET);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_SONGPOS);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_CLOCK);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_PORT_SUBSCRIBED);
    snd_seq_client_info_event_filter_add(seq_info, SND_SEQ_EVENT_PORT_UNSUBSCRIBED);
    if (0 > snd_seq_set_client_info(midi.handle, seq_info))
        runtime().Log("Failed to set midi event filtering");

    snd_seq_set_client_name(midi.handle, midiClientName().c_str());

    port_num = snd_seq_create_simple_port(midi.handle, port_name,
                                       SND_SEQ_PORT_CAP_WRITE
                                       | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                       SND_SEQ_PORT_TYPE_SYNTH);
    if (port_num < 0)
    {
        runtime().Log("Failed to acquire alsa midi port");
        Close();
        return false;
    }

    string midilist;
    switch(runtime().alsaMidiType)
    {
        case 0:
            midilist = runtime().midiDevice;
            break;
        case 1:
            midilist = findMidiClients(midi.handle);
            break;
        default:
            runtime().midiDevice = "";
            return true;
    }
    string found;
    if (midilist != "default")
    {
        while (!midilist.empty())
        {
            string tmp;
            while (midilist.find(' ') == 0 || midilist.find(',') == 0)
                midilist.erase(0,1); // make entry clean
            if (midilist.empty())
                break;

            size_t pos = midilist.find(',');
            if (pos == string::npos)
            {
                tmp = midilist;
                midilist = "";
            }
            else
            {
                tmp = midilist.substr(0, pos);
                midilist = midilist.substr(pos + 1);
            }
            pos = tmp.find_last_not_of(' ');
            tmp = tmp.substr(0, pos + 1);
            midi.device = tmp;

            bool midiSource = false;
            if (snd_seq_parse_address(midi.handle,&midi.addr,midi.device.c_str()) == 0)
            {
                midiSource = (snd_seq_connect_from(midi.handle, port_num, midi.addr.client, midi.addr.port) == 0);
            }
            if (midiSource)
                found += (", " + tmp);
        }
    }
    if (found.substr(0, 2) == ", ")
        runtime().midiDevice = found.substr(2);
    else
        runtime().midiDevice = "No MIDI sources seen";
    return true;
}


void AlsaEngine::Close()
{
    if (runtime().runSynth)
    {
        runtime().runSynth = false;
    }

    if (midi.pThread != 0) //wait for midi thread to finish
    {
        void *ret = NULL;
        pthread_join(midi.pThread, &ret);
        midi.pThread = 0;
    }

    if (audio.pThread != 0) //wait for audio thread to finish
    {
        void *ret = NULL;
        pthread_join(audio.pThread, &ret);
        audio.pThread = 0;
    }

    if (audio.handle != NULL)
        alsaBad(snd_pcm_close(audio.handle), "close pcm failed");
    audio.handle = NULL;
    if (NULL != midi.handle)
        if (snd_seq_close(midi.handle) < 0)
            runtime().Log("Error closing Alsa midi connection");
    midi.handle = NULL;
}


string AlsaEngine::audioClientName()  const
{
    string name{"yoshimi"};
    auto& rt = unConst(this)->runtime();
    if (!rt.nameTag.empty())
        name += ("-" + rt.nameTag);
    return name;
}

string AlsaEngine::midiClientName()  const
{
    string name{"yoshimi"};
    auto& rt = unConst(this)->runtime();
    if (!rt.nameTag.empty())
        name += ("-" + rt.nameTag);
    //Andrew Deryabin: for multi-instance support add unique id to
    //instances other then default (0)
    uint synthUniqueId = synth.getUniqueId();
    if (synthUniqueId > 0)
    {
        char sUniqueId [256];
        memset(sUniqueId, 0, sizeof(sUniqueId));
        snprintf(sUniqueId, sizeof(sUniqueId), "%d", synthUniqueId);
        name += ("-" + string{sUniqueId});
    }
    return name;
}


bool AlsaEngine::prepHwparams()
{
    /*
     * thanks to the jack project for which formats to support and
     * the basis of a simplified structure
     */
    static struct
    {
        snd_pcm_format_t card_format;
        int card_bits;
        bool card_endian;
        bool card_signed;
    }
    card_formats[] =
    {
        {SND_PCM_FORMAT_S32_LE, 32, true, true},
        {SND_PCM_FORMAT_S32_BE, 32, false, true},
        {SND_PCM_FORMAT_S24_3LE, 24, true, true},
        {SND_PCM_FORMAT_S24_3BE, 24, false, true},
        {SND_PCM_FORMAT_S16_LE, 16, true, true},
        {SND_PCM_FORMAT_S16_BE, 16, false, true},
        {SND_PCM_FORMAT_UNKNOWN, 0, false, true}
    };
    int formidx;
    string formattxt;

    unsigned int ask_samplerate = audio.samplerate;
    unsigned int ask_buffersize = audio.period_size;

    snd_pcm_access_t axs = SND_PCM_ACCESS_MMAP_INTERLEAVED;
    snd_pcm_hw_params_t  *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);
    if (alsaBad(snd_pcm_hw_params_any(audio.handle, hwparams),
                "alsa audio no playback configurations available"))
        return false;
    if (alsaBad(snd_pcm_hw_params_set_periods_integer(audio.handle, hwparams),
                "alsa audio cannot restrict period size to integral value"))
        return false;
    if (!alsaBad(snd_pcm_hw_params_set_access(audio.handle, hwparams, axs),
                 "alsa audio mmap not possible"))
        pcmWrite = &snd_pcm_mmap_writei;
    else
    {
        axs = SND_PCM_ACCESS_RW_INTERLEAVED;
        if (alsaBad(snd_pcm_hw_params_set_access(audio.handle, hwparams, axs),
                     "alsa audio failed to set access, both mmap and rw failed"))
            return false;
        pcmWrite = &snd_pcm_writei;
    }

    formidx = 0;
    while (snd_pcm_hw_params_set_format(audio.handle, hwparams, card_formats[formidx].card_format) < 0)
    {
        ++formidx;
        if (card_formats[formidx].card_bits == 0)
        {
            runtime().Log("alsa audio failed to find matching format");
            return false;
        }
    }
    card_bits = card_formats[formidx].card_bits;
    card_endian = card_formats[formidx].card_endian;
    card_signed = card_formats[formidx].card_signed;

    if (little_endian)
        formattxt += "Little";
    else
        formattxt += "Big";

    runtime().Log("March is " + formattxt + " Endian", _SYS_::LogNotSerious);

    if (card_signed)
        formattxt = "Signed ";
    else
        formattxt = "Unsigned ";

    if (card_endian)
        formattxt += "Little";
    else
        formattxt += "Big";


    alsaBad(snd_pcm_hw_params_set_rate_resample(audio.handle, hwparams, 1),
            "alsa audio failed to set allow resample");
    if (alsaBad(snd_pcm_hw_params_set_rate_near(audio.handle, hwparams,
                                                &audio.samplerate, NULL),
                "alsa audio failed to set sample rate (asked for "
                + asString(ask_samplerate) + ")"))
        return false;
    if (alsaBad(snd_pcm_hw_params_set_channels_near(audio.handle, hwparams, &card_chans),
                "alsa audio failed to set requested channels"))
        return false;
    if (alsaBad(snd_pcm_hw_params_set_period_size_near(audio.handle, hwparams, &audio.period_size, 0), "failed to set period size"))
        return false;
    if (alsaBad(snd_pcm_hw_params_set_periods_near(audio.handle, hwparams, &audio.period_count, 0), "failed to set number of periods"))
        return false;
    if (alsaBad(snd_pcm_hw_params_set_buffer_size_near(audio.handle, hwparams, &audio.buffer_size), "failed to set buffer size"))
        return false;
    if (alsaBad(snd_pcm_hw_params (audio.handle, hwparams),
                "alsa audio failed to set hardware parameters"))
		return false;
    if (alsaBad(snd_pcm_hw_params_get_buffer_size(hwparams, &audio.buffer_size),
                "alsa audio failed to get buffer size"))
        return false;
    if (alsaBad(snd_pcm_hw_params_get_period_size(hwparams, &audio.period_size,
                NULL), "failed to get period size"))
        return false;

    runtime().Log("Card Format is " + formattxt + " Endian " + asString(card_bits) +" Bit " + asString(card_chans) + " Channel" , 2);
    if (ask_buffersize != audio.period_size)
    {
        runtime().Log("Asked for buffersize " + asString(ask_buffersize, 2)
                    + ", Alsa dictates " + asString((unsigned int)audio.period_size), _SYS_::LogNotSerious);
        runtime().buffersize = audio.period_size; // we shouldn't need to do this :(
    }
    return true;
}


bool AlsaEngine::prepSwparams()
{
    snd_pcm_sw_params_t *swparams;
    snd_pcm_sw_params_alloca(&swparams); // allocated on stack and automatically freed when leaving this scope
	snd_pcm_uframes_t boundary;
	return (not alsaBad(snd_pcm_sw_params_current(audio.handle, swparams),
                        "alsa audio failed to get swparams"))
       and (not alsaBad(snd_pcm_sw_params_get_boundary(swparams, &boundary),
                        "alsa audio failed to get boundary"))
       and (not alsaBad(snd_pcm_sw_params_set_start_threshold(audio.handle
                                                             ,swparams
                                                             ,boundary + 1)
                       ,"failed to set start threshold"))  // explicit start, not auto start
       and (not alsaBad(snd_pcm_sw_params_set_stop_threshold(audio.handle
                                                            ,swparams
                                                            ,boundary)
                       ,"alsa audio failed to set stop threshold"))
       and (not alsaBad(snd_pcm_sw_params(audio.handle, swparams)
                       ,"alsa audio failed to set software parameters"))
         ;
}


void AlsaEngine::Interleave(int buffersize)
{
    size_t idx = 0;
    bool byte_swap = (little_endian != card_endian);
    ushort tmp16a, tmp16b;
    size_t chans;
    uint tmp32a, tmp32b;
    uint shift = 0x78000000;
    if (card_bits == 24)
        shift = 0x780000;

    if (card_bits == 16)
    {
        chans = card_chans / 2; // because we're pairing them on a single integer
        for (int frame = 0; frame < buffersize; ++frame)
        {
            tmp16a = ushort(lrint( zynLeft[NUM_MIDI_PARTS][frame] * 0x7800));
            tmp16b = ushort(lrint(zynRight[NUM_MIDI_PARTS][frame] * 0x7800));
            if (byte_swap)
            {
                tmp16a = (short int) ((tmp16a >> 8) | (tmp16a << 8));        //TODO shouldn't that be a cast to unsigned short? IIRC, the assignment promotes to unsigned
                tmp16b = ((tmp16b >> 8) | (tmp16b << 8));
            }
            interleaved[idx] = tmp16a | int(tmp16b << 16);
            idx += chans;
        }
    }
    else
    {
        chans = card_chans;
        for (int frame = 0; frame < buffersize; ++frame)
        {
            tmp32a = uint(lrint( zynLeft[NUM_MIDI_PARTS][frame] * shift));
            tmp32b = uint(lrint(zynRight[NUM_MIDI_PARTS][frame] * shift));
            // how should we do an endian swap for 24 bit, 3 byte?
            // is it really the same, just swapping the 'unused' byte?
            if (byte_swap)
            {
                tmp32a = (tmp32a >> 24) | ((tmp32a << 8) & 0x00FF0000) | ((tmp32a >> 8) & 0x0000FF00) | (tmp32a << 24);
                tmp32b = (tmp32b >> 24) | ((tmp32b << 8) & 0x00FF0000) | ((tmp32b >> 8) & 0x0000FF00) | (tmp32b << 24);
            }
            interleaved[idx] = int(tmp32a);
            interleaved[idx + 1] = int(tmp32b);
            idx += chans;
        }
    }
}


void* AlsaEngine::_AudioThread(void* arg)
{
    return static_cast<AlsaEngine*>(arg)->AudioThread();
}


void* AlsaEngine::AudioThread()
{
    alsaBad(snd_pcm_start(audio.handle), "alsa audio pcm start failed");
    while (runtime().runSynth.load(std::memory_order_relaxed))  // read the atomic flag as we happen to see it, without forcing any sync
    {
        BeatTracker::BeatValues beats(beatTracker->getBeatValues());
        synth.setBeatValues(beats.songBeat, beats.monotonicBeat, beats.bpm);

        audio.pcm_state = snd_pcm_state(audio.handle);
        if (audio.pcm_state != SND_PCM_STATE_RUNNING)
        {
            switch (audio.pcm_state)
            {
                case SND_PCM_STATE_XRUN:

                case SND_PCM_STATE_SUSPENDED:
                    if (!xrunRecover())
                        break;
                    /* falls through */
                case SND_PCM_STATE_SETUP:
                    if (alsaBad(snd_pcm_prepare(audio.handle),
                                "alsa audio pcm prepare failed"))
                        break;
                    /* falls through */
                case SND_PCM_STATE_PREPARED:
                    alsaBad(snd_pcm_start(audio.handle), "pcm start failed");
                    break;

                default:
                    runtime().Log("Alsa AudioThread, weird SND_PCM_STATE: "
                                 + asString(audio.pcm_state));
                    break;
            }
            audio.pcm_state = snd_pcm_state(audio.handle);
        }
        if (audio.pcm_state == SND_PCM_STATE_RUNNING)
        {
            getAudio();
            int alsa_buff = getBuffersize();
            Interleave(alsa_buff);
            Write(alsa_buff);
        }
        else
            runtime().Log("Audio pcm still not running");
    }
    return NULL;
}


void AlsaEngine::Write(snd_pcm_uframes_t towrite)
{
    snd_pcm_sframes_t wrote = 0;
    int *data = interleaved.get();

    while (towrite > 0)
    {
        wrote = pcmWrite(audio.handle, data, towrite);
        if (wrote >= 0)
        {
            if ((snd_pcm_uframes_t)wrote < towrite || wrote == -EAGAIN)
                snd_pcm_wait(audio.handle, 666);
            if (wrote > 0)
            {
                towrite -= wrote;
                data += wrote * card_chans;
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
                    Recover(wrote);
                    break;

                default:
                    alsaBad(wrote, "alsa audio, snd_pcm_writei ==> weird state");
                    break;
            }
            wrote = 0;
        }
    }
}


bool AlsaEngine::Recover(int err)
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


bool AlsaEngine::xrunRecover()
{
    bool isgood = false;
    if (audio.handle != NULL)
    {
        if (!alsaBad(snd_pcm_drop(audio.handle), "pcm drop failed"))
            if (!alsaBad(snd_pcm_prepare(audio.handle), "pcm prepare failed"))
                isgood = true;
        runtime().Log("Alsa xrun recovery "
                     + (isgood? string{"good"} : string{"not good"}));
    }
    return isgood;
}


bool AlsaEngine::Start()
{
    if (NULL != midi.handle && !runtime().startThread(&midi.pThread, _MidiThread,
                                                      this, true, 1, "Alsa midi"))
    {
        runtime().Log("Failed to start Alsa midi thread");
        goto bail_out;
    }
    if (NULL != audio.handle && !runtime().startThread(&audio.pThread, _AudioThread,
                                                       this, true, 0, "Alsa audio"))
    {
        runtime().Log(" Failed to start Alsa audio thread");
        goto bail_out;
    }

    return true;

bail_out:
    runtime().Log("Bailing from AlsaEngine Start");
    Close();
    return false;
}


void *AlsaEngine::_MidiThread(void *arg)
{
    return static_cast<AlsaEngine*>(arg)->MidiThread();
}


/*
 * This next function needs a lot of work we shouldn't need
 * to decode then re-encode the data in a different form
 */

void* AlsaEngine::MidiThread()
{
    unsigned int pollCount = snd_seq_poll_descriptors_count(midi.handle, POLLIN);
    struct pollfd pollfds[pollCount];

    while (runtime().runSynth.load(std::memory_order_relaxed))
    {
        snd_seq_poll_descriptors(midi.handle, pollfds, pollCount, POLLIN);

        // Poll with timeout. Should be long-ish for performance reasons, but
        // should be short enough to be smaller than MIDI_CLOCK_TIMEOUT_US, and
        // short enough to be able to quit relatively quickly.
        int pollResult = poll(pollfds, pollCount, 500);

        if (pollResult < 0)
        {
            if (errno == EINTR)
                continue;
            else
            {
                char errMsg[200];
                snprintf(errMsg, sizeof(errMsg),
                    "Unable to handle error in MIDI thread: %s. Shutting down MIDI.",
                    strerror(errno));
                runtime().Log(errMsg);
                break;
            }
        }

        auto now = steady_clock::now();
        auto clock = floor<Mircos>(now.time_since_epoch())
                                 .count();
        if (pollResult > 0)
            handleMidiEvents(clock);

        if ((clock - midi.prevClockUs) >= MIDI_CLOCK_TIMEOUT_US)
            handleMidiClockSilence(clock);
    }
    return nullptr;
}


void AlsaEngine::handleMidiEvents(uint64_t clock)
{
    snd_seq_event_t *event;
    unsigned int par;
    int chk;
    bool sendit;
    unsigned char par0, par1 = 0, par2 = 0;

    while ((chk = snd_seq_event_input(midi.handle, &event)) > 0)
    {
        if (!event)
            continue;
        sendit = true;
        par0 = event->data.control.channel;
        par = 0;
        switch (event->type)
        {
            case SND_SEQ_EVENT_NOTEON:
                par0 = event->data.note.channel;
                par0 |= 0x90;
                par1 = event->data.note.note;
                par2 = event->data.note.velocity;
                break;

            case SND_SEQ_EVENT_NOTEOFF:
                par0 = event->data.note.channel;
                par0 |= 0x80;
                par1 = event->data.note.note;
                break;

            case SND_SEQ_EVENT_KEYPRESS:
                par0 = event->data.note.channel;
                par0 |= 0xa0;
                par1 = event->data.note.note;
                par2 = event->data.note.velocity;
                break;

            case SND_SEQ_EVENT_CHANPRESS:
                par0 |= 0xd0;
                par1 = event->data.control.value;
                break;

            case SND_SEQ_EVENT_PGMCHANGE:
                par0 |= 0xc0;
                par1 = event->data.control.value;
                break;

            case SND_SEQ_EVENT_PITCHBEND:
                par0 |= 0xe0;
                par = event->data.control.value + 8192;
                par1 = par & 0x7f;
                par2 = par >> 7;
                break;

            case SND_SEQ_EVENT_CONTROLLER:
                par0 |= 0xb0;
                par1 = event->data.control.param;
                par2 = event->data.control.value;
                break;

            case SND_SEQ_EVENT_NONREGPARAM:
                par0 |= 0xb0; // splitting into separate CCs
                par = event->data.control.param;
                handleMidi(par0, 99, par >> 7);
                handleMidi(par0, 99, par & 0x7f);
                par = event->data.control.value;
                handleMidi(par0, 6, par >> 7);
                par1 = 38;
                par2 = par & 0x7f; // let last one through
                break;

            case SND_SEQ_EVENT_RESET: // reset to power-on state
                par0 = 0xff;
                break;

            case SND_SEQ_EVENT_PORT_SUBSCRIBED: // ports connected
                runtime().Log("Alsa midi port connected");
                sendit = false;
                break;

            case SND_SEQ_EVENT_PORT_UNSUBSCRIBED: // ports disconnected
                runtime().Log("Alsa midi port disconnected");
                sendit = false;
                break;

            case SND_SEQ_EVENT_SONGPOS:
                handleSongPos((float)event->data.control.value
                    / (float)MIDI_SONGPOS_BEAT_DIVISION);
                sendit = false;
                break;

            case SND_SEQ_EVENT_CLOCK:
                handleMidiClock(clock);
                sendit = false;
                break;

            default:
                sendit = false;// commented out some progs spam us :(
                /* runtime().Log("Other non-handled midi event, type: "
                            + asString((int)event->type));*/
                break;
        }
        if (sendit)
            handleMidi(par0, par1, par2);
        snd_seq_free_event(event);
    }
}

void AlsaEngine::handleMidiClockSilence(uint64_t clock)
{
    // This is equivalent to receiving a clock beat every MIDI_CLOCK_TIMEOUT_US
    // nanoseconds, except we do not use it to calculate the BPM, but use the
    // fallback value instead. In between these fake "beats", the BeatTracker
    // interpolates the values for us, as it also does for normal MIDI clock
    // beats. This means it may take up to MIDI_CLOCK_TIMEOUT_US nanoseconds to
    // react to a change in the BPM fallback.
    BeatTracker::BeatValues beats {
        midi.lastDivSongBeat,
        midi.lastDivMonotonicBeat,
        synth.PbpmFallback,
    };
    float diff = (clock - midi.prevClockUs) * beats.bpm / (60.0f * 1000000.0f);
    beats.songBeat += diff;
    beats.monotonicBeat += diff;
    midi.lastDivSongBeat = beats.songBeat;
    midi.lastDivMonotonicBeat = beats.monotonicBeat;
    beatTracker->setBeatValues(beats);
    midi.prevClockUs = clock;
}


bool AlsaEngine::alsaBad(int op_result, string err_msg)
{
    bool isbad = (op_result < 0);
    if (isbad)
        runtime().Log("Error, alsa audio: " +err_msg + ": "
                     + string{snd_strerror(op_result)});
    return isbad;
}

void AlsaEngine::handleSongPos(float beat)
{
    const float subDiv = 1.0f / float(MIDI_CLOCKS_PER_BEAT / MIDI_CLOCK_DIVISION);

    // The next MIDI clock should trigger this beat.
    midi.lastDivSongBeat = beat - subDiv;

    // Possibly adjust the monotonic beat backwards to avoid accumulating too
    // many beats when we adjust clockCount below.
    midi.lastDivMonotonicBeat -= (MIDI_CLOCK_DIVISION - midi.clockCount - 1) * subDiv;

    // Force next clock tick to be a clean beat, on zero.
    midi.clockCount = MIDI_CLOCK_DIVISION - 1;

    // Tempting to call this here, but it is actually the next MIDI clock which
    // signals the next beat.
    //beatTracker->setBeatValues(beats);
}

void AlsaEngine::handleMidiClock(uint64_t clock)
{
    float bpm = 1000000.0f * 60.0f / float((clock - midi.prevClockUs) * MIDI_CLOCKS_PER_BEAT);
    if (++midi.prevBpmsPos >= ALSA_MIDI_BPM_MEDIAN_WINDOW)
        midi.prevBpmsPos = 0;
    midi.prevBpms[midi.prevBpmsPos] = bpm;

    float tmpBpms[ALSA_MIDI_BPM_MEDIAN_WINDOW];
    memcpy(tmpBpms, midi.prevBpms+midi.prevBpmsPos, (ALSA_MIDI_BPM_MEDIAN_WINDOW - midi.prevBpmsPos)
           * sizeof(*midi.prevBpms));
    memcpy(tmpBpms + ALSA_MIDI_BPM_MEDIAN_WINDOW - midi.prevBpmsPos, midi.prevBpms, midi.prevBpmsPos
           * sizeof(*midi.prevBpms));

    // To avoid fluctuations in the BPM value due to clock inaccuracies, sort
    // all the most recent bpm values, and take the average of the middle part
    // (an average median). For this, we use Bubble sort, but we only need to
    // sort the half that we use.
    for (int i = 0; i < (ALSA_MIDI_BPM_MEDIAN_WINDOW+ALSA_MIDI_BPM_MEDIAN_AVERAGE_WINDOW)/2; i++)
    {
        for (int j = i+1; j < ALSA_MIDI_BPM_MEDIAN_WINDOW; j++)
        {
            if (tmpBpms[i] > tmpBpms[j])
            {
                float tmp = tmpBpms[i];
                tmpBpms[i] = tmpBpms[j];
                tmpBpms[j] = tmp;
            }
        }
    }
    bpm = 0;
    for (int i = (ALSA_MIDI_BPM_MEDIAN_WINDOW - ALSA_MIDI_BPM_MEDIAN_AVERAGE_WINDOW) / 2;
         i < (ALSA_MIDI_BPM_MEDIAN_WINDOW + ALSA_MIDI_BPM_MEDIAN_AVERAGE_WINDOW) / 2; i++)
        bpm += tmpBpms[i];
    bpm /= (float)ALSA_MIDI_BPM_MEDIAN_AVERAGE_WINDOW;

    midi.prevClockUs = clock;

    midi.clockCount++;

    float inc = midi.clockCount / (float)MIDI_CLOCKS_PER_BEAT;

    BeatTracker::BeatValues beats
    {
        midi.lastDivSongBeat + inc,
        midi.lastDivMonotonicBeat + inc,
        bpm
    };

    beats = beatTracker->setBeatValues(beats);

    if (midi.clockCount >= MIDI_CLOCK_DIVISION)
    {
        // Possibly preserve wrapped around beat values, if we are on the start
        // of a clock division.
        midi.lastDivSongBeat = beats.songBeat;
        midi.lastDivMonotonicBeat = beats.monotonicBeat;
        midi.clockCount = 0;
    }
}

#endif /*defined(HAVE_ALSA)*/
