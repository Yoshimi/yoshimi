/*
    AlsaJackClient.h - Alsa audio + Jack midi

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

#ifndef ALSA_JACK_CLIENT_H
#define ALSA_JACK_CLIENT_H

#include <string>

using namespace std;

#include "MusicIO/MusicClient.h"
#include "MusicIO/AlsaEngine.h"
#include "MusicIO/JackEngine.h"

class AlsaJackClient:public MusicClient
{
    public:
        AlsaJackClient():MusicClient() { }
        ~AlsaJackClient() {
            Close();
        }
        bool openAudio(WavRecord *recorder);
        bool openMidi(WavRecord *recorder);
        bool Start(void);
        void queueMidi(midimessage *msg);
        void Close(void);
        void queueProgramChange(unsigned char chan, unsigned short banknum,
                                unsigned char prog, uint32_t eventframe);
        bool jacksessionReply(string cmdline);
        unsigned int getSamplerate(void);
        int getBuffersize(void);
        int audioLatency(void);
        int midiLatency(void);
        string audioClientName(void);
        string midiClientName(void);
        int audioClientId(void);
        int midiClientId(void);

    private:
        AlsaEngine alsaEngine;
        JackEngine jackEngine;
};

#endif
