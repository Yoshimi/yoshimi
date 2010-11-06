/*
    AlsaClient.cpp - Alsa audio / Alsa midi

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

#include "Misc/Config.h"
#include "MusicIO/AlsaClient.h"

bool AlsaClient::openAudio(WavRecord *recorder)
{
    if (alsaEngine.openAudio(recorder))
    {
        Runtime.Samplerate = getSamplerate();
        Runtime.Buffersize = getBuffersize();
        return true;
    }
    Runtime.Log("AlsaClient audio open failed");
    return false;
}


bool AlsaClient::openMidi(WavRecord *recorder)
{
    if (alsaEngine.openMidi(recorder))
        return true;
    Runtime.Log("AlsaClient midi open failed");
    return false;
}


void AlsaClient::queueMidi(midimessage *msg)
    { alsaEngine.queueMidi(msg); }

void AlsaClient::Close(void)
    { alsaEngine.Close(); }

bool AlsaClient::Start(void)
    { return alsaEngine.Start(); }

void AlsaClient::queueProgramChange(unsigned char chan, unsigned short banknum,
                                    unsigned char prog, uint32_t eventframe)
    { return alsaEngine.queueProgramChange(chan, banknum, prog, eventframe); }

unsigned int AlsaClient::getSamplerate(void)
    { return alsaEngine.getSamplerate(); }

int AlsaClient::getBuffersize(void)
    { return alsaEngine.getBuffersize(); }

int AlsaClient::audioLatency(void)
    { return alsaEngine.audioLatency(); }

int AlsaClient::midiLatency(void)
    { return alsaEngine.midiLatency(); }

string AlsaClient::audioClientName(void)
    { return alsaEngine.audioClientName(); }

string AlsaClient::midiClientName(void)
    { return alsaEngine.midiClientName(); }

int AlsaClient::audioClientId(void)
    { return alsaEngine.audioClientId(); }

int AlsaClient::midiClientId(void)
    { return alsaEngine.midiClientId(); }
