/*
    AlsaEngine.h

    Copyright 2009-2010, Alan Calvert
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

#ifndef ALSA_ENGINE_H
#define ALSA_ENGINE_H

#include <pthread.h>
#include <alsa/asoundlib.h>
#include <string>

#include "MusicIO/MusicIO.h"

#define MIDI_CLOCKS_PER_BEAT 24
#define MIDI_CLOCK_DIVISION 3

#define MIDI_SONGPOS_BEAT_DIVISION 4

class SynthEngine;

class AlsaEngine : public MusicIO
{
    public:
        AlsaEngine(SynthEngine *_synth, BeatTracker *_beatTracker);
        ~AlsaEngine() { }

        bool openAudio(void);
        bool openMidi(void);
        bool Start(void);
        void Close(void);

        unsigned int getSamplerate(void) { return audio.samplerate; }
        int getBuffersize(void) { return audio.period_size; }

        std::string audioClientName(void);
        std::string midiClientName(void);
        int audioClientId(void) { return audio.alsaId; }
        int midiClientId(void) { return midi.alsaId; }
        virtual void registerAudioPort(int )  {}

        bool little_endian;
        bool card_endian;
        int card_bits;
        bool card_signed;
        unsigned int card_chans;

    private:
        bool prepHwparams(void);
        bool prepSwparams(void);
        void Interleave(int buffersize);
        void Write(snd_pcm_uframes_t towrite);
        bool Recover(int err);
        bool xrunRecover(void);
        bool alsaBad(int op_result, std::string err_msg);
        void closeAudio(void);
        void closeMidi(void);

        std::string findMidiClients(snd_seq_t *seq);

        void *AudioThread(void);
        static void *_AudioThread(void *arg);
        void *MidiThread(void);
        static void *_MidiThread(void *arg);

        snd_pcm_sframes_t (*pcmWrite)(snd_pcm_t *handle, const void *data,
                                      snd_pcm_uframes_t nframes);

        void handleSongPos(float beat);
        void handleMidiClock();

        struct {
            std::string        device;
            snd_pcm_t         *handle;
            unsigned int       period_count;
            unsigned int       samplerate;
            snd_pcm_uframes_t  period_size;
            snd_pcm_uframes_t  buffer_size;
            int                alsaId;
            snd_pcm_state_t    pcm_state;
            pthread_t          pThread;
        } audio;

        struct {
            std::string        device;
            snd_seq_t         *handle;
            snd_seq_addr_t     addr;
            int                alsaId;
            pthread_t          pThread;

            // When receiving MIDI clock messages, to avoid precision errors
            // (MIDI_CLOCKS_PER_BEAT (24) does not cleanly divide 1), store
            // every third (MIDI_CLOCK_DIVISION) beat here. This is reset only
            // every third clock ticks or on song repositioning. Note that the
            // value is not necessarily an exact multiple of
            // 1/MIDI_CLOCK_DIVISION, but we only ever add
            // (1/MIDI_CLOCK_DIVISION) beats to it.
            float             lastDivSongBeat;
            float             lastDivMonotonicBeat;
            // Reset to zero every MIDI_CLOCK_DIVISION. This is actually an
            // integer, but stored as float for calculation purposes.
            float             clockCount;
        } midi;
};

#endif

#endif
