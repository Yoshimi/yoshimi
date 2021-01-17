/*
    AlsaEngine.cpp

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

#if defined(HAVE_ALSA)

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "Misc/FormatFuncs.h"
#include "MusicIO/AlsaEngine.h"
#include <iostream>

using func::asString;


AlsaEngine::AlsaEngine(SynthEngine *_synth, BeatTracker *_beatTracker) :
    MusicIO(_synth, _beatTracker)
{
    audio.handle = NULL;
    audio.period_count = 0; // re-used as number of periods
    audio.samplerate = 0;
    audio.buffer_size = 0;
    audio.period_size = 0;
    audio.buffer_size = 0;
    audio.alsaId = -1;
    audio.pThread = 0;

    midi.handle = NULL;
    midi.addr.client = 0;
    midi.addr.port = 0;
    midi.alsaId = -1;
    midi.pThread = 0;
    midi.lastDivSongBeat = 0;
    midi.lastDivMonotonicBeat = 0;
    midi.clockCount = 0;
    little_endian = synth->getRuntime().isLittleEndian;
}


bool AlsaEngine::openAudio(void)
{
    audio.device = synth->getRuntime().audioDevice;
    audio.samplerate = synth->getRuntime().Samplerate;
    audio.period_size = synth->getRuntime().Buffersize;
    audio.period_count = 2;
    audio.buffer_size = audio.period_size * audio.period_count;
    if (alsaBad(snd_pcm_open(&audio.handle, audio.device.c_str(),
                             SND_PCM_STREAM_PLAYBACK, SND_PCM_NO_AUTO_CHANNELS),
            "failed to open alsa audio device:" + audio.device))
        goto bail_out;

    if (!alsaBad(snd_pcm_nonblock(audio.handle, 0), "set blocking failed"))
    {
        if (prepHwparams())
        {
            if (prepSwparams())
            {
                if (prepBuffers())
                {
                    int buffersize = getBuffersize();
                    interleaved = new int[buffersize * card_chans];
                    if (NULL == interleaved)
                        goto bail_out;
                    memset(interleaved, 0, sizeof(int) * buffersize * card_chans);
                }
            }
        }
    }
    return true;
bail_out:
    Close();
    return false;
}

std::string AlsaEngine::findMidiClients(snd_seq_t *seq)
{
    string result = "";
    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);

        if (client == SND_SEQ_CLIENT_SYSTEM)
            continue; // don't show system timer and announce ports
        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            // port must understand MIDI messages
            if (!(snd_seq_port_info_get_type(pinfo)
                  & SND_SEQ_PORT_TYPE_MIDI_GENERIC))
                continue;
            // we need both READ and SUBS_READ
            if ((snd_seq_port_info_get_capability(pinfo)
                 & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
                != (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
                continue;
            if (std::string(snd_seq_client_info_get_name(cinfo)) == "Midi Through")
                continue; // don't want midi through
            result = result + snd_seq_client_info_get_name(cinfo) + ":" + std::to_string(snd_seq_port_info_get_port(pinfo)) + ", ";
        }
    }
    return result;
}

bool AlsaEngine::openMidi(void)
{
    const char* port_name = "input";
    int port_num;
    if (snd_seq_open(&midi.handle, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) != 0)
    {
        synth->getRuntime().Log("Failed to open alsa midi");
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
        synth->getRuntime().Log("Failed to set midi event filtering");

    snd_seq_set_client_name(midi.handle, midiClientName().c_str());

    port_num = snd_seq_create_simple_port(midi.handle, port_name,
                                       SND_SEQ_PORT_CAP_WRITE
                                       | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                       SND_SEQ_PORT_TYPE_SYNTH);
    if (port_num < 0)
    {
        synth->getRuntime().Log("Failed to acquire alsa midi port");
        Close();
        return false;
    }

    std::string midilist;
    switch(synth->getRuntime().alsaMidiType)
    {
        case 0:
            midilist = synth->getRuntime().midiDevice;
            break;
        case 1:
            midilist = findMidiClients(midi.handle);
            break;
        default:
            synth->getRuntime().midiDevice = "";
            return true;
    }
    std::string found = "";
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
            if (pos == std::string::npos)
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
        synth->getRuntime().midiDevice = found.substr(2);
    else
        synth->getRuntime().midiDevice = "No MIDI sources seen";
    return true;
}


void AlsaEngine::Close(void)
{
    if (synth->getRuntime().runSynth)
    {
        synth->getRuntime().runSynth = false;
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
            synth->getRuntime().Log("Error closing Alsa midi connection");
    midi.handle = NULL;
}


std::string AlsaEngine::audioClientName(void)
{
    std::string name = "yoshimi";
    if (!synth->getRuntime().nameTag.empty())
        name += ("-" + synth->getRuntime().nameTag);
    return name;
}

std::string AlsaEngine::midiClientName(void)
{
    std::string name = "yoshimi";
    if (!synth->getRuntime().nameTag.empty())
        name += ("-" + synth->getRuntime().nameTag);
    //Andrew Deryabin: for multi-instance support add unique id to
    //instances other then default (0)
    unsigned int synthUniqueId = synth->getUniqueId();
    if (synthUniqueId > 0)
    {
        char sUniqueId [256];
        memset(sUniqueId, 0, sizeof(sUniqueId));
        snprintf(sUniqueId, sizeof(sUniqueId), "%d", synthUniqueId);
        name += ("-" + std::string(sUniqueId));
    }
    return name;
}


bool AlsaEngine::prepHwparams(void)
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
    std::string formattxt = "";
    card_chans = 2; // got to start somewhere

    unsigned int ask_samplerate = audio.samplerate;
    unsigned int ask_buffersize = audio.period_size;

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

    formidx = 0;
    while (snd_pcm_hw_params_set_format(audio.handle, hwparams, card_formats[formidx].card_format) < 0)
    {
        ++formidx;
        if (card_formats[formidx].card_bits == 0)
        {
            synth->getRuntime().Log("alsa audio failed to find matching format");
            goto bail_out;
        }
    }
    card_bits = card_formats[formidx].card_bits;
    card_endian = card_formats[formidx].card_endian;
    card_signed = card_formats[formidx].card_signed;

    if (little_endian)
        formattxt += "Little";
    else
        formattxt += "Big";

    synth->getRuntime().Log("March is " + formattxt + " Endian", 2);

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
        goto bail_out;
    if (alsaBad(snd_pcm_hw_params_set_channels_near(audio.handle, hwparams, &card_chans),
                "alsa audio failed to set requested channels"))
        goto bail_out;
    if (alsaBad(snd_pcm_hw_params_set_period_size_near(audio.handle, hwparams, &audio.period_size, 0), "failed to set period size"))
        goto bail_out;
    if (alsaBad(snd_pcm_hw_params_set_periods_near(audio.handle, hwparams, &audio.period_count, 0), "failed to set number of periods"))
        goto bail_out;
    if (alsaBad(snd_pcm_hw_params_set_buffer_size_near(audio.handle, hwparams, &audio.buffer_size), "failed to set buffer size"))
        goto bail_out;
    if (alsaBad(snd_pcm_hw_params (audio.handle, hwparams),
                "alsa audio failed to set hardware parameters"))
		goto bail_out;
    if (alsaBad(snd_pcm_hw_params_get_buffer_size(hwparams, &audio.buffer_size),
                "alsa audio failed to get buffer size"))
        goto bail_out;
    if (alsaBad(snd_pcm_hw_params_get_period_size(hwparams, &audio.period_size,
                NULL), "failed to get period size"))
        goto bail_out;

    synth->getRuntime().Log("Card Format is " + formattxt + " Endian " + asString(card_bits) +" Bit " + asString(card_chans) + " Channel" , 2);
    if (ask_buffersize != audio.period_size)
    {
        synth->getRuntime().Log("Asked for buffersize " + asString(ask_buffersize, 2)
                    + ", Alsa dictates " + asString((unsigned int)audio.period_size), 2);
        synth->getRuntime().Buffersize = audio.period_size; // we shouldn't need to do this :(
    }
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
    if (alsaBad(snd_pcm_sw_params_set_start_threshold(audio.handle, swparams,
                                                      boundary + 1),
                "failed to set start threshold")) // explicit start, not auto start
        goto bail_out;
    if (alsaBad(snd_pcm_sw_params_set_stop_threshold(audio.handle, swparams,
                                                    boundary),
               "alsa audio failed to set stop threshold"))
        goto bail_out;
    if (alsaBad(snd_pcm_sw_params(audio.handle, swparams),
                "alsa audio failed to set software parameters"))
        goto bail_out;
    return true;

bail_out:
    return false;
}


void AlsaEngine::Interleave(int buffersize)
{
    int idx = 0;
    bool byte_swap = (little_endian != card_endian);
    unsigned short int tmp16a, tmp16b;
    int chans;
    unsigned int tmp32a, tmp32b;
    unsigned int shift = 0x78000000;
    if (card_bits == 24)
        shift = 0x780000;

    if (card_bits == 16)
    {
        chans = card_chans / 2; // because we're pairing them on a single integer
        for (int frame = 0; frame < buffersize; ++frame)
        {
            tmp16a = (unsigned short int) (lrint(zynLeft[NUM_MIDI_PARTS][frame] * 0x7800));
            tmp16b = (unsigned short int) (lrint(zynRight[NUM_MIDI_PARTS][frame] * 0x7800));
            if (byte_swap)
            {
                tmp16a = (short int) ((tmp16a >> 8) | (tmp16a << 8));
                tmp16b = ((tmp16b >> 8) | (tmp16b << 8));
            }
            interleaved[idx] = tmp16a | (int) (tmp16b << 16);
            idx += chans;
        }
    }
    else
    {
        chans = card_chans;
        for (int frame = 0; frame < buffersize; ++frame)
        {
            tmp32a = (unsigned int) (lrint(zynLeft[NUM_MIDI_PARTS][frame] * shift));
            tmp32b = (unsigned int) (lrint(zynRight[NUM_MIDI_PARTS][frame] * shift));
            // how should we do an endian swap for 24 bit, 3 byte?
            // is it really the same, just swapping the 'unused' byte?
            if (byte_swap)
            {
                tmp32a = (tmp32a >> 24) | ((tmp32a << 8) & 0x00FF0000) | ((tmp32a >> 8) & 0x0000FF00) | (tmp32a << 24);
                tmp32b = (tmp32b >> 24) | ((tmp32b << 8) & 0x00FF0000) | ((tmp32b >> 8) & 0x0000FF00) | (tmp32b << 24);
            }
            interleaved[idx] = (int) tmp32a;
            interleaved[idx + 1] = (int) tmp32b;
            idx += chans;
        }
    }
}


void *AlsaEngine::_AudioThread(void *arg)
{
    return static_cast<AlsaEngine*>(arg)->AudioThread();
}


void *AlsaEngine::AudioThread(void)
{
    alsaBad(snd_pcm_start(audio.handle), "alsa audio pcm start failed");
    while (synth->getRuntime().runSynth)
    {
        std::pair<float, float> beats(beatTracker->getBeatValues());
        synth->setBeatValues(beats.first, beats.second);

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
                    synth->getRuntime().Log("Alsa AudioThread, weird SND_PCM_STATE: "
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
            synth->getRuntime().Log("Audio pcm still not running");
    }
    return NULL;
}


void AlsaEngine::Write(snd_pcm_uframes_t towrite)
{
    snd_pcm_sframes_t wrote = 0;
    int *data = interleaved;

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


bool AlsaEngine::xrunRecover(void)
{
    bool isgood = false;
    if (audio.handle != NULL)
    {
        if (!alsaBad(snd_pcm_drop(audio.handle), "pcm drop failed"))
            if (!alsaBad(snd_pcm_prepare(audio.handle), "pcm prepare failed"))
                isgood = true;
        synth->getRuntime().Log("Alsa xrun recovery "
                    + ((isgood) ? std::string("good") : std::string("not good")));
    }
    return isgood;
}


bool AlsaEngine::Start(void)
{
    if (NULL != midi.handle && !synth->getRuntime().startThread(&midi.pThread, _MidiThread,
                                                    this, true, 1, "Alsa midi"))
    {
        synth->getRuntime().Log("Failed to start Alsa midi thread");
        goto bail_out;
    }
    if (NULL != audio.handle && !synth->getRuntime().startThread(&audio.pThread, _AudioThread,
                                                     this, true, 0, "Alsa audio"))
    {
        synth->getRuntime().Log(" Failed to start Alsa audio thread");
        goto bail_out;
    }

    return true;

bail_out:
    synth->getRuntime().Log("Bailing from AlsaEngine Start");
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

void *AlsaEngine::MidiThread(void)
{
    snd_seq_event_t *event;
    unsigned int par;
    int chk;
    bool sendit;
    unsigned char par0, par1 = 0, par2 = 0;
    while (synth->getRuntime().runSynth)
    {
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
                    setMidi(par0, 99, par >> 7);
                    setMidi(par0, 99, par & 0x7f);
                    par = event->data.control.value;
                    setMidi(par0, 6, par >> 7);
                    par1 = 38;
                    par2 = par & 0x7f; // let last one through
                    break;

                case SND_SEQ_EVENT_RESET: // reset to power-on state
                    par0 = 0xff;
                    break;

                case SND_SEQ_EVENT_PORT_SUBSCRIBED: // ports connected
                    synth->getRuntime().Log("Alsa midi port connected");
                    sendit = false;
                    break;

                case SND_SEQ_EVENT_PORT_UNSUBSCRIBED: // ports disconnected
                    synth->getRuntime().Log("Alsa midi port disconnected");
                    sendit = false;
                    break;

                case SND_SEQ_EVENT_SONGPOS:
                    handleSongPos((float)event->data.control.value
                        / (float)MIDI_SONGPOS_BEAT_DIVISION);
                    sendit = false;
                    break;

                case SND_SEQ_EVENT_CLOCK:
                    handleMidiClock();
                    sendit = false;
                    break;

                default:
                    sendit = false;// commented out some progs spam us :(
                    /* synth->getRuntime().Log("Other non-handled midi event, type: "
                                + asString((int)event->type));*/
                    break;
            }
            if (sendit)
                setMidi(par0, par1, par2);
            snd_seq_free_event(event);
        }
;
        if (chk < 0)
        {
            usleep(1024);
        }
    }
    return NULL;
}


bool AlsaEngine::alsaBad(int op_result, std::string err_msg)
{
    bool isbad = (op_result < 0);
    if (isbad)
        synth->getRuntime().Log("Error, alsa audio: " +err_msg + ": "
                     + std::string(snd_strerror(op_result)));
    return isbad;
}

void AlsaEngine::handleSongPos(float beat)
{
    const float subDiv = 1.0f / (float)(MIDI_CLOCKS_PER_BEAT / MIDI_CLOCK_DIVISION);

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

void AlsaEngine::handleMidiClock()
{
    midi.clockCount++;

    float inc = midi.clockCount / (float)MIDI_CLOCKS_PER_BEAT;

    std::pair<float, float> beats(midi.lastDivSongBeat + inc, midi.lastDivMonotonicBeat + inc);

    beats = beatTracker->setBeatValues(beats);

    if (midi.clockCount >= MIDI_CLOCK_DIVISION) {
        // Possibly preserve wrapped around beat values, if we are on the start
        // of a clock division.
        midi.lastDivSongBeat = beats.first;
        midi.lastDivMonotonicBeat = beats.second;
        midi.clockCount = 0;
    }
}

#endif
