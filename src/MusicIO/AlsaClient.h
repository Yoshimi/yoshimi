/*
    AlsaClient.h

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

#ifndef ALSA_CLIENT_H
#define ALSA_CLIENT_H

#include "MusicIO/MusicClient.h"
#include "MusicIO/AlsaEngine.h"

class AlsaClient : public MusicClient
{
    public:
        AlsaClient() : MusicClient() { };
        ~AlsaClient() { };

        bool openAudio(void);
        bool openMidi(void);
        bool Start(void) { return alsaEngine.Start(); };
        void Stop(void);
        void Close(void) { alsaEngine.Close(); }
        unsigned int getSamplerate(void) { return alsaEngine.getSamplerate(); };
        int getBuffersize(void) { return alsaEngine.getBuffersize(); };
        string audioClientName(void) { return alsaEngine.audioClientName(); };
        string midiClientName(void) { return alsaEngine.midiClientName(); };
        int audioClientId(void) { return alsaEngine.audioClientId(); };
        int midiClientId(void) { return alsaEngine.midiClientId(); };

    protected:
        AlsaEngine alsaEngine;
};

#endif
