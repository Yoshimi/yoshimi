/*
    JackClient.cpp

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
#include "MusicIO/JackClient.h"

bool JackClient::openAudio(WavRecord *recorder)
{
    if(!jackEngine.isConnected()
       && !jackEngine.connectServer(Runtime.audioDevice)) {
        Runtime.Log("Failed to connect to jack server");
        return false;
    }
    if(jackEngine.openAudio(recorder)) {
        Runtime.Samplerate = getSamplerate();
        Runtime.Buffersize = getBuffersize();
        return true;
    }
    else
        Runtime.Log("Failed to register audio");
    return false;
}

bool JackClient::openMidi(WavRecord *recorder)
{
    if(!jackEngine.isConnected()
       && !jackEngine.connectServer(Runtime.midiDevice)) {
        Runtime.Log("Failed to connect to jack server");
        return false;
    }
    if(jackEngine.openMidi(recorder))
        return true;
    else
        Runtime.Log("JackClient failed to open midi");
    return false;
}

bool JackClient::Start(void)
{ return jackEngine.Start() && MusicClient::Start(); }

void JackClient::queueMidi(midimessage *msg)
{ jackEngine.queueMidi(msg); }

void JackClient::Close(void)
{ jackEngine.Close(); wavrecord->Close(); }

void JackClient::queueProgramChange(unsigned char chan, unsigned short banknum,
                                    unsigned char prog, uint32_t eventframe)
{ return jackEngine.queueProgramChange(chan, banknum, prog, eventframe); }

bool JackClient::jacksessionReply(string cmdline)
{ return jackEngine.jacksessionReply(cmdline); }

unsigned int JackClient::getSamplerate(void)
{ return jackEngine.getSamplerate(); }

int JackClient::getBuffersize(void)
{ return jackEngine.getBuffersize(); }

int JackClient::audioLatency(void)
{ return jackEngine.audioLatency(); }

int JackClient::midiLatency(void)
{ return jackEngine.midiLatency(); }

string JackClient::audioClientName(void)
{ return jackEngine.audioClientName(); }

string JackClient::midiClientName(void)
{ return jackEngine.midiClientName(); }

int JackClient::audioClientId(void)
{ return jackEngine.audioClientId(); }

int JackClient::midiClientId(void)
{ return jackEngine.midiClientId(); }
