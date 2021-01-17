/*
    MusicClient.h

    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2016-2019, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

    Modified May 2019
*/

#ifndef MUSIC_CLIENT_H
#define MUSIC_CLIENT_H

#include <string>
#include <pthread.h>

#include "globals.h"

enum audio_drivers { no_audio = 0, jack_audio, alsa_audio};
enum midi_drivers { no_midi = 0, jack_midi, alsa_midi};

class SynthEngine;
class MusicIO;
class BeatTracker;

struct music_clients
{
    int order;
    audio_drivers audioDrv;
    midi_drivers midiDrv;
    bool operator ==(const music_clients& other) const { return audioDrv == other.audioDrv && midiDrv == other.midiDrv; }
    bool operator >(const music_clients& other) const { return (order > other.order) && (other != *this); }
    bool operator <(const music_clients& other) const { return (order < other.order)  && (other != *this); }
    bool operator !=(const music_clients& other) const { return audioDrv != other.audioDrv || midiDrv != other.midiDrv; }
};

#define NMC_SRATE 44100

class MusicClient
{
private:
    SynthEngine *synth;
    pthread_t timerThreadId;
    static void *timerThread_fn(void*);
    bool timerWorking;
    float *buffersL [NUM_MIDI_PARTS + 1];
    float *buffersR [NUM_MIDI_PARTS + 1];
    audio_drivers audioDrv;
    midi_drivers midiDrv;
    MusicIO *audioIO;
    MusicIO *midiIO;
    BeatTracker *beatTracker;
public:
    MusicClient(SynthEngine *_synth, audio_drivers _audioDrv, midi_drivers _midiDrv);
    ~MusicClient();
    bool Open(void);
    bool Start(void);
    void Close(void);
    unsigned int getSamplerate(void);
    int getBuffersize(void);
    std::string audioClientName(void);
    std::string midiClientName(void);
    int audioClientId(void);
    int midiClientId(void);
    void registerAudioPort(int /*portnum*/);

    static MusicClient *newMusicClient(SynthEngine *_synth);
};

#endif
