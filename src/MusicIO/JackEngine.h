/*
    JackEngine.h

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

#ifndef JACK_ENGINE_H
#define JACK_ENGINE_H

#include <string>
#include <pthread.h>
#include <semaphore.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

using namespace std;

#include "MusicIO/MusicIO.h"

class JackEngine : public MusicIO
{
    public:
        JackEngine();
        ~JackEngine() { Close(); };

        bool isConnected(void) { return (NULL != jackClient); };
        bool connectServer(string server);
        bool openAudio(WavRecord *recorder);
        bool openMidi(WavRecord *recorder);
        bool Start(void);
        void Close(void);
        
        unsigned int getSamplerate(void) { return audio.jackSamplerate; };
        int getBuffersize(void) { return audio.jackNframes; };

        string clientName(void);
        int clientId(void);

    private:
        bool connectJackPorts(void);
        bool processAudio(jack_nframes_t nframes);
        bool processMidi(jack_nframes_t nframes);
        int processCallback(jack_nframes_t nframes);
        static int _processCallback(jack_nframes_t nframes, void *arg);
        static void *_midiThread(void *arg);
        void *midiThread(void);
        void midiCleanup(void);
        static void _midiCleanup(void *arg);
        static void _errorCallback(const char *msg);
        static void _infoCallback(const char *msg);
        static int _xrunCallback(void *arg);

        jack_client_t      *jackClient;
        struct {
            unsigned int  jackSamplerate;
            unsigned int  jackNframes;
            jack_port_t  *ports[2];
            jsample_t    *portBuffs[2];
        } audio;

        struct {
            jack_port_t*       port;
            jack_ringbuffer_t *ringBuf;
            pthread_t          pThread;
            string             semName;
            sem_t             *eventsUp;
            bool               threadStop;
        } midi;

        struct midi_event {
            jack_nframes_t time;
            char data[4]; // all events of interest are <= 4bytes
        };
            
};

#endif
