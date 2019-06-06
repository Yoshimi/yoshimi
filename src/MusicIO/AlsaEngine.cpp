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

    Modified May 2019
*/

#if defined(HAVE_ALSA)

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/AlsaEngine.h"
#include <iostream>

AlsaEngine::AlsaEngine(SynthEngine *_synth) : MusicIO(_synth),
    little_endian(synth->getRuntime().isLittleEndian),
    card_endian(little_endian),
    card_signed(true),
    card_chans(2),
    card_bits(8),
    pcmWrite(NULL)
{
    audio.handle = NULL;
    audio.period_count = 0; // re-used as number of periods
    audio.samplerate = 0;
    audio.period_size = 0;
    audio.buffer_size = 0;
    audio.alsaId = -1;
    audio.pThread = 0;

    midi.handle = NULL;
    midi.alsaId = -1;
    midi.pThread = 0;
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
                    interleaved = new int[audio.buffer_size * card_chans];
                    if (NULL == interleaved)
                        goto bail_out;
                    memset(interleaved, 0, sizeof(int) * audio.buffer_size * card_chans);
                }
            }
        }
    }
    cout << "period " << audio.period_size << "   buff " << audio.buffer_size << "   inter " << audio.buffer_size * card_chans << "   chans " << card_chans << endl;
    return true;
bail_out:
    Close();
    return false;
}


bool AlsaEngine::openMidi(void)
{
    midi.device = synth->getRuntime().midiDevice;
    const char* port_name = "input";
    int port_num;
    if (snd_seq_open(&midi.handle, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) != 0)
    {
        synth->getRuntime().Log("Failed to open alsa midi");
        goto bail_out;
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
        goto bail_out;
    }
    if (!midi.device.empty() && midi.device != "default")
    {
        bool midiSource = false;
        if (snd_seq_parse_address(midi.handle,&midi.addr,midi.device.c_str()) == 0)
        {
            midiSource = (snd_seq_connect_from(midi.handle, port_num, midi.addr.client, midi.addr.port) == 0);
        }
        if (!midiSource)
        {
            synth->getRuntime().Log("Didn't find alsa MIDI source '" + midi.device + "'");
            synth->getRuntime().midiDevice = "";
        }
    }
    return true;

bail_out:
    Close();
    return false;
}


void AlsaEngine::Close(void)
{
    if(synth->getRuntime().runSynth)
    {
        synth->getRuntime().runSynth = false;
    }

    if(midi.pThread != 0) //wait for midi thread to finish
    {
        void *ret = NULL;
        pthread_join(midi.pThread, &ret);
        midi.pThread = 0;
    }

    if(audio.pThread != 0) //wait for audio thread to finish
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
    if(synthUniqueId > 0)
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


void AlsaEngine::Interleave(int offset, int buffersize)
{
    bool byte_swap = (little_endian != card_endian);

    if (card_bits == 16)
    {
        int chans = card_chans / 2; // because we're pairing them on a single integer
        int idx = offset * chans;
        for (int frame = 0; frame < buffersize; ++frame)
        {
            uint16_t tmp16a = uint16_t(zynLeft[NUM_MIDI_PARTS][frame] * float(0x7800));
            uint16_t tmp16b = uint16_t(zynRight[NUM_MIDI_PARTS][frame] * float(0x7800));
            if (byte_swap)
            {
                tmp16a = uint16_t((tmp16a >> 8) | (tmp16a << 8));
                tmp16b = uint16_t((tmp16b >> 8) | (tmp16b << 8));
            }
            interleaved[idx] = tmp16a | (int) (tmp16b << 16);
            idx += chans;
        }
    }
    else
    {
        int idx = offset * card_chans;
        float shift;
        if (card_bits == 32)
            shift = float(0x78000000);
        else
            shift = float(0x780000);
        for (int frame = 0; frame < buffersize; ++frame)
        {
            uint32_t tmp32a = uint32_t((zynLeft[NUM_MIDI_PARTS][frame] * shift));
            uint32_t tmp32b = uint32_t((zynRight[NUM_MIDI_PARTS][frame] * shift));
            // how should we do an endian swap for 24 bit, 3 byte?
            // is it really the same, just swapping the 'unused' byte?
            if (byte_swap)
            {
                tmp32a = (tmp32a >> 24) | ((tmp32a << 8) & 0x00FF0000) | ((tmp32a >> 8) & 0x0000FF00) | (tmp32a << 24);
                tmp32b = (tmp32b >> 24) | ((tmp32b << 8) & 0x00FF0000) | ((tmp32b >> 8) & 0x0000FF00) | (tmp32b << 24);
            }
            interleaved[idx] = int32_t(tmp32a);
            interleaved[idx + 1] = int32_t(tmp32b);
            idx += card_chans;
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
        audio.pcm_state = snd_pcm_state(audio.handle);
        if (audio.pcm_state != SND_PCM_STATE_RUNNING)
        {
            bool done = false; // now done this way to suppress warnings
            if (audio.pcm_state == SND_PCM_STATE_XRUN || audio.pcm_state == SND_PCM_STATE_SUSPENDED)
            {
                if (!xrunRecover())
                    done = true;
            }
            if (!done || audio.pcm_state == SND_PCM_STATE_SETUP)
            {
                if (alsaBad(snd_pcm_prepare(audio.handle), "alsa audio pcm prepare failed"))
                    done = true;
            }
            if (!done || audio.pcm_state == SND_PCM_STATE_PREPARED)
            {
                alsaBad(snd_pcm_start(audio.handle), "pcm start failed");
            }
            /*switch (audio.pcm_state)
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
                    // and again
                case SND_PCM_STATE_PREPARED:
                    alsaBad(snd_pcm_start(audio.handle), "pcm start failed");
                    break;

                default:
                    synth->getRuntime().Log("Alsa AudioThread, weird SND_PCM_STATE: "
                                + asString(audio.pcm_state));
                    break;
            }*/
            audio.pcm_state = snd_pcm_state(audio.handle);
        }

        if (audio.pcm_state == SND_PCM_STATE_RUNNING)
        {
            int alsa_buff = audio.period_size;//audio.buffer_size;
            int offset = 0;
            while (alsa_buff - offset >= synth->buffersize)
            {
                synth->MasterAudio(zynLeft, zynRight, synth->buffersize);
                Interleave(offset, synth->buffersize);
                offset += synth->buffersize;
            }
            int remainder = alsa_buff - offset;
            if ( remainder > 0)
            {
                synth->MasterAudio(zynLeft, zynRight, remainder);
                Interleave(offset, remainder);
            }

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
        if(chk < 0)
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

#endif
