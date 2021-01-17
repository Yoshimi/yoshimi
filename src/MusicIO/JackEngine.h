/*
    JackEngine.h

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

#ifndef JACK_ENGINE_H
#define JACK_ENGINE_H

#include <string>
#include <pthread.h>
#include <semaphore.h>
#include <jack/jack.h>
//#include <jack/ringbuffer.h>

#if defined(JACK_SESSION)
    #include <jack/session.h>
#endif

#include "MusicIO/MusicIO.h"

class SynthEngine;

class JackEngine : public MusicIO
{
    public:
        JackEngine(SynthEngine *_synth, BeatTracker *_beatTracker);
        ~JackEngine() { Close(); }
        bool isConnected(void) { return (NULL != jackClient); }
        bool connectServer(std::string server);
        bool openAudio(void);
        bool openMidi(void);
        bool Start(void);
        void Close(void);
        unsigned int getSamplerate(void) { return audio.jackSamplerate; }
        int getBuffersize(void) { return audio.jackNframes; }
        std::string clientName(void);
        int clientId(void);
        virtual std::string audioClientName(void) { return clientName(); }
        virtual int audioClientId(void) { return clientId(); }
        virtual std::string midiClientName(void) { return clientName(); }
        virtual int midiClientId(void) { return clientId(); }
        void registerAudioPort(int portnum);

    private:
        bool openJackClient(std::string server);
        bool connectJackPorts(void);
        bool processAudio(jack_nframes_t nframes);
        void sendAudio(int framesize, unsigned int offset);
        bool processMidi(jack_nframes_t nframes);
        void handleBeatValues(jack_nframes_t nframes);
        bool latencyPrep(void);
        int processCallback(jack_nframes_t nframes);
        static int _processCallback(jack_nframes_t nframes, void *arg);
        static int _xrunCallback(void *arg);


#if defined(JACK_SESSION)
            static void _jsessionCallback(jack_session_event_t *event, void *arg);
            void jsessionCallback(jack_session_event_t *event);
            jack_session_event_t *lastevent;
#endif

#if defined(JACK_LATENCY)
            static void _latencyCallback(jack_latency_callback_mode_t mode, void *arg);
            void latencyCallback(jack_latency_callback_mode_t mode);
#endif

        jack_client_t      *jackClient;
        struct {
            unsigned int  jackSamplerate;
            unsigned int  jackNframes;
            jack_port_t  *ports[2*NUM_MIDI_PARTS+2];
            float        *portBuffs[2*NUM_MIDI_PARTS+2];
        } audio;

        struct {
            jack_port_t*       port;
            jack_ringbuffer_t *ringBuf;
            pthread_t          pThread;
        } midi;

        float bpm;

        unsigned int internalbuff;
};

#endif
