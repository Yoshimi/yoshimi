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

#include "Misc/Util.h"

#include <string>
#include <jack/jack.h>

#if defined(JACK_SESSION)
    #include <jack/session.h>
#endif

#include "MusicIO/MusicIO.h"

using std::string;
using util::unConst;


class SynthEngine;


class JackEngine : public MusicIO
{
    public:
        // shall not be copied nor moved
        JackEngine(JackEngine&&)                 = delete;
        JackEngine(JackEngine const&)            = delete;
        JackEngine& operator=(JackEngine&&)      = delete;
        JackEngine& operator=(JackEngine const&) = delete;
        JackEngine(SynthEngine&, shared_ptr<BeatTracker>);
       ~JackEngine() { Close(); }

        /* ====== MusicIO interface ======== */
        bool openAudio()               override;
        bool openMidi()                override;
        bool Start()                   override;
        void Close()                   override;
        void registerAudioPort(int)    override;

        uint   getSamplerate()   const override { return audio.jackSamplerate; }
        int    getBuffersize()   const override { return audio.jackNframes; }
        string audioClientName() const override { return unConst(this)->clientName(); }
        int    audioClientId()   const override { return unConst(this)->clientId();   }
        string midiClientName()  const override { return unConst(this)->clientName(); }
        int    midiClientId()    const override { return unConst(this)->clientId();   }

        bool isConnected()                      { return (NULL != jackClient); }
        bool connectServer(string server);
        string clientName();
        int clientId();


    private:
        bool openJackClient(string server);
        bool connectJackPorts();
        bool processAudio(jack_nframes_t nframes);
        void sendAudio(int framesize, uint offset);
        bool processMidi(jack_nframes_t nframes);
        void handleBeatValues(jack_nframes_t nframes);
        bool latencyPrep();
        int processCallback(jack_nframes_t nframes);
        static int _processCallback(jack_nframes_t nframes, void* arg);
        static int _xrunCallback(void* arg);


#if defined(JACK_SESSION)
            static void _jsessionCallback(jack_session_event_t* event, void* arg);
            void jsessionCallback(jack_session_event_t* event);
#endif

#if defined(JACK_LATENCY)
            static void _latencyCallback(jack_latency_callback_mode_t mode, void* arg);
            void latencyCallback(jack_latency_callback_mode_t mode);
#endif

        struct JackAudio
        {
            unsigned int  jackSamplerate;
            unsigned int  jackNframes;
            jack_port_t  *ports[2*NUM_MIDI_PARTS+2];
            float        *portBuffs[2*NUM_MIDI_PARTS+2];
        };

        jack_client_t *jackClient;
        JackAudio      audio;
        jack_port_t   *midiPort;

        unsigned int internalbuff;
};
#endif /*JACK_ENGINE_H*/
