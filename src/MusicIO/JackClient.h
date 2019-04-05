/*
    JackClient.h

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

#ifndef JACK_CLIENT_H
#define JACK_CLIENT_H

#include <string>

using namespace std;

#include "MusicIO/MusicClient.h"
#include "MusicIO/JackEngine.h"

class JackClient : public MusicClient
{
    public:
        JackClient() : MusicClient() { };
        ~JackClient() { Close(); };

        bool openAudio(WavRecord *recorder);
        bool openMidi(WavRecord *recorder);
        bool Start(void) { return jackEngine.Start(); };
        void Close(void);

        unsigned int getSamplerate(void) { return jackEngine.getSamplerate(); };
        int getBuffersize(void) { return jackEngine.getBuffersize(); };
        int grossLatency(void) { return jackEngine.grossLatency(); };

        string audioClientName(void) { return jackEngine.clientName(); };
        string midiClientName(void) { return jackEngine.clientName(); };
        int audioClientId(void) { return jackEngine.clientId(); };
        int midiClientId(void) { return jackEngine.clientId(); };


    private:
        JackEngine jackEngine;
};

#endif
