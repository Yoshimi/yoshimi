/*
    JackAlsaClient.h - Jack audio + Alsa midi

    Copyright 2009-2010, Alan Calvert

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

#ifndef JACK_ALSA_CLIENT_H
#define JACK_ALSA_CLIENT_H

#include <string>
#include <pthread.h>
#include <semaphore.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

using namespace std;

#include "Misc/Config.h"
#include "MusicIO/MusicClient.h"
#include "MusicIO/JackEngine.h"
#include "MusicIO/AlsaEngine.h"

class SynthEngine;

class JackAlsaClient : public MusicClient
{
    public:
        JackAlsaClient(SynthEngine *_synth) : MusicClient(_synth), jackEngine(_synth), alsaEngine (_synth) { }
        ~JackAlsaClient() { Close(); }

        bool openAudio(void);
        bool openMidi(void);
        bool Start(void);
        void Close(void) { jackEngine.Close(); alsaEngine.Close(); }
        unsigned int getSamplerate(void) { return jackEngine.getSamplerate(); }
        int getBuffersize(void) { return jackEngine.getBuffersize(); }
        string audioClientName(void) { return jackEngine.clientName(); }
        string midiClientName(void) { return alsaEngine.midiClientName(); }
        int audioClientId(void) { return jackEngine.clientId(); }
        int midiClientId(void) { return alsaEngine.midiClientId(); }
        virtual void registerAudioPort(int portnum) { jackEngine.registerJackPort(portnum);}

    private:
        JackEngine jackEngine;
        AlsaEngine alsaEngine;
};

#endif
