/*
    AlsaEngine.h

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

#ifndef ALSA_ENGINE_H
#define ALSA_ENGINE_H

#include <pthread.h>
#include <string>
#include <alsa/asoundlib.h>

using namespace std;

#include "MusicIO/MusicIO.h"

class AlsaEngine : public MusicIO
{
    public:
        AlsaEngine();
        ~AlsaEngine() { };

        bool openAudio(void);
        bool openMidi(void);
        bool Start(void);
        void Close(void);

        unsigned int getSamplerate(void) { return audio.samplerate; };
        int getBuffersize(void) { return audio.period_size; };

        string audioClientName(void);
        string midiClientName(void);
        int audioClientId(void) { return audio.alsaId; };
        int midiClientId(void) { return midi.alsaId; };

    private:
        bool prepHwparams(void);
        bool prepSwparams(void);
        void Write(void);
        bool Recover(int err);
        bool xrunRecover(void);
        bool alsaBad(int op_result, string err_msg);
        void closeAudio(void);
        void closeMidi(void);

        void *AudioThread(void);
        static void *_AudioThread(void *arg);
        void *MidiThread(void);
        static void *_MidiThread(void *arg);

        snd_pcm_sframes_t (*pcmWrite)(snd_pcm_t *handle, const void *data,
                                      snd_pcm_uframes_t nframes);

        struct {
            string             device;
            snd_pcm_t         *handle;
            unsigned int       period_time;
            unsigned int       samplerate;
            snd_pcm_uframes_t  period_size;
            snd_pcm_uframes_t  buffer_size;
            int                alsaId;
            snd_pcm_state_t    pcm_state;
            pthread_t          pThread;
        } audio;

        struct {
            string              device;
            snd_seq_t           *handle;
            snd_seq_addr_t      *link;
            int                 alsaId;
            pthread_t           pThread;
        } midi;
};

#endif
