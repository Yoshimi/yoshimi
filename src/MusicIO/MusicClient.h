/*
    MusicClient.h

    Copyright 2009-2011 Alan Calvert
    Copyright 2009 James Morris

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

#ifndef MUSIC_CLIENT_H
#define MUSIC_CLIENT_H

#include <string>

using namespace std;

#include "MusicIO/MidiControl.h"

class SynthEngine;

class MusicClient
{
    public:
        MusicClient(SynthEngine *_synth): synth(_synth) { }
        virtual ~MusicClient() { }
        bool Open(void) { return openAudio() && openMidi(); }
        virtual bool Start(void) = 0;
        virtual void Close(void) = 0;
        virtual unsigned int getSamplerate(void) = 0;
        virtual int getBuffersize(void) = 0;
        virtual string audioClientName(void) = 0;
        virtual string midiClientName(void) = 0;
        virtual int audioClientId(void) = 0;
        virtual int midiClientId(void) = 0;
        virtual void registerAudioPort(int /*portnum*/) {}
        static MusicClient *newMusicClient(SynthEngine *_synth);
        string audiodevice;
        string mididevice;

    protected:
        virtual bool openAudio(void) = 0;
        virtual bool openMidi(void) = 0;

        SynthEngine *synth;
};

#endif
