/*
    JackAlsaClient.cpp - Jack audio / Alsa midi

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

#include "MusicIO/JackAlsaClient.h"

bool JackAlsaClient::openAudio(void)
{
    if (jackEngine.connectServer(Runtime.audioDevice))
    {
        if (jackEngine.openAudio())
        {
            Runtime.Samplerate = getSamplerate();
            Runtime.Buffersize = getBuffersize();
            return true;
        }
        else
            Runtime.Log("Error, failed to register audio");
    }
    else
        Runtime.Log("Error, failed to connect to jack server");
    return false;
}


bool JackAlsaClient::openMidi(void)
{
    return alsaEngine.openMidi();
}


bool JackAlsaClient::Start(void)
{
    return jackEngine.Start() && alsaEngine.Start();
}
