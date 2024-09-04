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

#include "MusicIO/MusicIO.h"

#include <pthread.h>
#include <alsa/asoundlib.h>
#include <string>

using std::string;

#define MIDI_CLOCKS_PER_BEAT 24
#define MIDI_CLOCK_DIVISION 3

#define MIDI_SONGPOS_BEAT_DIVISION 4

#define ALSA_MIDI_BPM_MEDIAN_WINDOW 48
#define ALSA_MIDI_BPM_MEDIAN_AVERAGE_WINDOW 20

class SynthEngine;


class AlsaEngine : public MusicIO
{
    public:
        // shall not be copied nor moved
        AlsaEngine(AlsaEngine&&)                 = delete;
        AlsaEngine(AlsaEngine const&)            = delete;
        AlsaEngine& operator=(AlsaEngine&&)      = delete;
        AlsaEngine& operator=(AlsaEngine const&) = delete;
        AlsaEngine(SynthEngine&, shared_ptr<BeatTracker>);
       ~AlsaEngine() { Close(); }


        /* ====== MusicIO interface ======== */
        bool openAudio()               override;
        bool openMidi()                override;
        bool Start()                   override;
        void Close()                   override;
        void registerAudioPort(int)    override { /*ignore*/ }

        uint   getSamplerate()   const override { return audio.samplerate; }
        int    getBuffersize()   const override { return audio.period_size; }
        string audioClientName() const override ;
        int    audioClientId()   const override { return audio.alsaId; }
        string midiClientName()  const override ;
        int    midiClientId()    const override { return midi.alsaId; }

    private:
        bool prepHwparams();
        bool prepSwparams();
        void Interleave(int buffersize);
        void Write(snd_pcm_uframes_t towrite);
        bool Recover(int err);
        bool xrunRecover();
        bool alsaBad(int op_result, string err_msg);
        void closeAudio();
        void closeMidi();

        string findMidiClients(snd_seq_t* seq);

        void* AudioThread();
        static void* _AudioThread(void* arg);
        void *MidiThread();
        static void* _MidiThread(void* arg);

        void handleMidiEvents(uint64_t clock);
        void handleMidiClockSilence(uint64_t clock);

        void handleSongPos(float beat);
        void handleMidiClock(uint64_t clock);

        bool little_endian;
        bool card_endian;
        bool card_signed;
        uint card_chans;
        int  card_bits;

        using PcmOutput = snd_pcm_sframes_t(snd_pcm_t*, const void*, snd_pcm_uframes_t);
        PcmOutput* pcmWrite;

        unique_ptr<int[]> interleaved; // output buffer for 16bit interleaved audio

        struct Audio {
            string            device{};
            snd_pcm_t*        handle{nullptr};
            uint              period_count{0}; // re-used as number of periods
            uint              samplerate{0};
            snd_pcm_uframes_t period_size{0};
            snd_pcm_uframes_t buffer_size{0};
            int               alsaId{-1};
            snd_pcm_state_t   pcm_state{SND_PCM_STATE_DISCONNECTED};
            pthread_t         pThread{0};
        };

        struct Midi {
            string            device{};
            snd_seq_t*        handle{nullptr};
            snd_seq_addr_t    addr{0,0};
            int               alsaId{-1};
            pthread_t         pThread{0};

            // When receiving MIDI clock messages, to avoid precision errors
            // (MIDI_CLOCKS_PER_BEAT (24) does not cleanly divide 1), store
            // every third (MIDI_CLOCK_DIVISION) beat here. This is reset only
            // every third clock ticks or on song repositioning. Note that the
            // value is not necessarily an exact multiple of
            // 1/MIDI_CLOCK_DIVISION, but we only ever add
            // (1/MIDI_CLOCK_DIVISION) beats to it.
            float             lastDivSongBeat{0};
            float             lastDivMonotonicBeat{0};
            // Reset to zero every MIDI_CLOCK_DIVISION. This is actually an
            // integer, but stored as float for calculation purposes.
            float             clockCount{0};

            float             prevBpms[ALSA_MIDI_BPM_MEDIAN_WINDOW];
            int               prevBpmsPos{0};
            int64_t           prevClockUs{-1};
        };

        Audio audio;
        Midi midi;
};

#endif /*ALSA_ENGINE_H*/
#endif /*defined(HAVE_ALSA)*/
