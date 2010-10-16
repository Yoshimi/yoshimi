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

#include "MusicIO/MusicIO.h"

using namespace std;

class AlsaEngine : public MusicIO
{
    public:
        AlsaEngine();
        ~AlsaEngine() { };
        
        bool openAudio(WavRecord *recorder);
        bool openMidi(WavRecord *recorder);
        bool Start(void);
        void Close(void);
        
        unsigned int getSamplerate(void) { return audio.samplerate; };
        int getBuffersize(void) { return audio.period_size; };

    private:
        bool prepHwparams(void);
        bool prepSwparams(void);
        void writepcm(void);
        bool pcmRecover(int err);
        bool xrunRecover(void);
        bool alsaBad(int op_result, string err_msg);
        void closeAudio(void);
        void closeMidi(void);
        
        void *audioThread(void);
        static void *_audioThread(void *arg);
        static void _midiTimerCallback(snd_async_handler_t *midicbh);
        void midiTimerCallback(void);

        snd_pcm_sframes_t (*pcmWrite)(snd_pcm_t *handle, const void *data,
                                      snd_pcm_uframes_t nframes);
        struct {
            string             device;
            snd_pcm_t         *handle;
            unsigned int       period_time;
            unsigned int       samplerate;
            snd_pcm_uframes_t  period_size;
            snd_pcm_uframes_t  buffer_size;
            snd_pcm_state_t    pcm_state;
            pthread_t          pThread;
        } audio;

        struct {
            string               device;
            snd_seq_t           *handle;
            int                  portId;
            snd_timer_t         *timerhandle;
            snd_midi_event_t    *decoder;
            snd_async_handler_t *callbackHandler;
        } midi;
};

#endif
