/*
    AlsaJackClient.cpp - Jack audio / Alsa midi

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

#include "Misc/Config.h"
#include "MusicIO/AlsaJackClient.h"

bool AlsaJackClient::openAudio(void)
{
    if (alsaEngine.openAudio())
    {
        synth->getRuntime().Samplerate = getSamplerate();
        synth->getRuntime().Buffersize = getBuffersize();
        return true;
    }
    else
        synth->getRuntime().Log("Failed to register audio");
    return false;
}


bool AlsaJackClient::openMidi(void)
{
    if (jackEngine.connectServer(synth->getRuntime().midiDevice))
    {
        if (jackEngine.openMidi())
        {
            return true;
        }
        else
            synth->getRuntime().Log("AlsaJackClient failed to open midi");
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
            synth->getRuntime().Log("jackEngine.Start() failed");
    }
    else
        synth->getRuntime().Log("alsaEngine.Start() failed");
    return false;
}
