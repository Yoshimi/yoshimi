/*
    LASHClient.h - LASH support

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

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
#ifndef LASHClient_h
#define LASHClient_h

#include <string>
#include <pthread.h>
#include <lash/lash.h>

using namespace std;

#include "MusicIO/AudioOut.h"
#include "MusicIO/MidiIn.h"

class LASHClient;
extern LASHClient *lash;

class LASHClient
{
    public:
        LASHClient(int* argc, char*** argv);
        void setAlsaId(int id);
        void setJackName(const char* name);

        enum Event { Save, Restore, Quit, NoEvent };
        Event checkEvents(string& filename);
        void confirmEvent(Event event);
        void setIdent(audio_drivers audio, midi_drivers midi,
                      string jackclientName, int alsaclientId);

    private:
        lash_client_t* client;
};

#endif
