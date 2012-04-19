/*
    JackAlsaClient.h - Jack audio + Alsa midi

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

class JackAlsaClient:public MusicClient
{
    public:
        JackAlsaClient():MusicClient() { }
        ~JackAlsaClient() {
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
        JackEngine jackEngine;
        AlsaEngine alsaEngine;
};

#endif
