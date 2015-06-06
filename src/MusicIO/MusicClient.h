/*
    MusicClient.h

    Copyright 2009, Alan Calvert
    Copyright 2009, James Morris

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

#ifndef MUSIC_CLIENT_H
#define MUSIC_CLIENT_H

#include <string>

using namespace std;

#include "MusicIO/MidiControl.h"

typedef enum {
    no_audio = 0,
    jack_audio,
    alsa_audio,
} audio_drivers;

typedef enum {
    no_midi = 0,
    jack_midi,
    alsa_midi,
} midi_drivers;

class MusicClient
{
    public:
        MusicClient() { };
        virtual ~MusicClient() { };

        static MusicClient *newMusicClient(void);
        bool Open(void) { return openAudio() && openMidi(); };
        virtual bool Start(void) { return true; };
        virtual void Stop(void) { };
        virtual void Close(void) { };

        virtual unsigned int getSamplerate(void) { return 0; };
        virtual unsigned int getBuffersize(void) { return 0; };

        virtual string audioClientName(void) { return string("Nada"); };
        virtual string midiClientName(void) { return string("Nada"); };
        virtual int audioClientId(void) { return -1; };
        virtual int midiClientId(void) { return -1; };

        string      audiodevice;
        string      mididevice;

    protected:
        virtual bool openAudio(void) { return true; };
        virtual bool openMidi(void) { return true; };

};

extern MusicClient *musicClient;

#endif
