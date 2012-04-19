/*
    JackAlsaClient.cpp - Jack audio / Alsa midi

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

#include "MusicIO/JackAlsaClient.h"

bool JackAlsaClient::openAudio(WavRecord *recorder)
{
    if(jackEngine.connectServer(Runtime.audioDevice)) {
        if(jackEngine.openAudio(recorder)) {
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



bool JackAlsaClient::openMidi(WavRecord *recorder)
{ return alsaEngine.openMidi(recorder); }

void JackAlsaClient::queueProgramChange(unsigned char chan,
                                        unsigned short banknum,
                                        unsigned char prog,
                                        uint32_t eventframe)
{ return alsaEngine.queueProgramChange(chan, banknum, prog, eventframe); }

bool JackAlsaClient::Start(void)
{ return jackEngine.Start() && alsaEngine.Start(); }

void JackAlsaClient::queueMidi(midimessage *msg)
{ alsaEngine.queueMidi(msg); }

void JackAlsaClient::Close(void)
{ jackEngine.Close(); alsaEngine.Close(); }

bool JackAlsaClient::jacksessionReply(string cmdline)
{ return jackEngine.jacksessionReply(cmdline); }

unsigned int JackAlsaClient::getSamplerate(void)
{ return jackEngine.getSamplerate(); }

int JackAlsaClient::getBuffersize(void)
{ return jackEngine.getBuffersize(); }

int JackAlsaClient::audioLatency(void)
{ return jackEngine.audioLatency(); }

int JackAlsaClient::midiLatency(void)
{ return alsaEngine.midiLatency(); }

string JackAlsaClient::audioClientName(void)
{ return jackEngine.audioClientName(); }

string JackAlsaClient::midiClientName(void)
{ return alsaEngine.midiClientName(); }

int JackAlsaClient::audioClientId(void)
{ return jackEngine.audioClientId(); }

int JackAlsaClient::midiClientId(void)
{ return alsaEngine.midiClientId(); }
