/*
    AlsaJackClient.cpp - Jack audio / Alsa midi
    
    Copyright 2009, Alan Calvert

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

#include <iostream>

#include "Misc/Config.h"
#include "MusicIO/AlsaJackClient.h"

bool AlsaJackClient::openAudio(void)
{
    if (alsaEngine.openAudio())
    {
        if (alsaEngine.prepAudiobuffers(getBuffersize(), true))
        {
            Runtime.settings.Samplerate = getSamplerate();
            Runtime.settings.Buffersize = getBuffersize();
            return true;
        }
        else
            cerr << "Error, failed to prep audio buffers" << endl;
    }
    else
        cerr << "Error, failed to register audio" << endl;
    return false;
}


bool AlsaJackClient::openMidi(void)
{
    if (jackEngine.connectServer(Runtime.settings.midiDevice))
    {
        if (jackEngine.openMidi())
        {
            return true;
        }
        else
            cerr << "Error, AlsaJackClient failed to open midi" << endl;
    }
    return false;
}

bool AlsaJackClient::Start(void)
{
    if (alsaEngine.Start())
    {
        if (jackEngine.Start())
        {
            return true;
        }
        else
            cerr << "jackEngine.Start() failed" << endl;
    }
    else
        cerr << "alsaEngine.Start() failed" << endl;
    return false;
}