/*
    MusicClient.h

    Copyright 2009-2010 Alan Calvert
    Copyright 2009 James Morris

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
#include "MusicIO/WavRecord.h"

typedef enum { no_audio = 0, jack_audio, alsa_audio, } audio_drivers;
typedef enum { no_midi = 0, jack_midi, alsa_midi, } midi_drivers;

class MusicClient
{
    public:
        MusicClient();
        ~MusicClient() { };

        bool Open(void);
        virtual bool Start(void) = 0;
        virtual void Close(void) = 0;
        virtual unsigned int getSamplerate(void) = 0;
        virtual int getBuffersize(void) = 0;
        virtual int grossLatency(void) = 0;
        virtual string audioClientName(void) = 0;
        virtual string midiClientName(void) = 0;
        virtual int audioClientId(void) = 0;
        virtual int midiClientId(void) = 0;

        static MusicClient *newMusicClient(void);

        void startRecord(void)  { Recorder->Start(); };
        void stopRecord(void) { Recorder->Stop(); };

        bool setRecordFile(const char* fpath, string& errmsg)
            { return Recorder->SetFile(string(fpath), errmsg); };

        bool setRecordOverwrite(string& errmsg)
            { return Recorder->SetOverwrite(errmsg); };

        string wavFilename(void) { return Recorder->Filename(); };

        string      audiodevice;
        string      mididevice;

    protected:
        virtual bool openAudio(WavRecord *recorder) = 0;
        virtual bool openMidi(WavRecord *recorder) = 0;

        WavRecord *Recorder;
};

extern MusicClient *musicClient;

#endif
