/*
    AlsaJackClient.h - Alsa audio + Jack midi

    Copyright 2009-2011, Alan Calvert

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

#ifndef ALSA_JACK_CLIENT_H
#define ALSA_JACK_CLIENT_H

#include <string>

using namespace std;

#include "MusicIO/MusicClient.h"
#include "MusicIO/AlsaEngine.h"
#include "MusicIO/JackEngine.h"

class SynthEngine;

class AlsaJackClient : public MusicClient
{
    public:
        AlsaJackClient(SynthEngine *_synth) : MusicClient(_synth), alsaEngine(_synth), jackEngine(_synth) { }
        ~AlsaJackClient() { Close(); }

        bool openAudio(void);
        bool openMidi(void);
        bool Start(void);
        void Close(void) { alsaEngine.Close(); jackEngine.Close(); }
        unsigned int getSamplerate(void) { return alsaEngine.getSamplerate(); }
        int getBuffersize(void) { return alsaEngine.getBuffersize(); }
        string audioClientName(void) { return alsaEngine.audioClientName(); }
        string midiClientName(void) { return jackEngine.clientName(); }
        int audioClientId(void) { return alsaEngine.audioClientId(); }
        int midiClientId(void) { return jackEngine.clientId(); }

    private:
        AlsaEngine alsaEngine;
        JackEngine jackEngine;
};

#endif
