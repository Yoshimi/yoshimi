/*
    MusicClient.h

    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2016-2020, Will Godfrey, Andrew Deryabin & others
    Copyright 2021-2024, Will Godfrey, Ichthyostega, Kristian Amlie & others

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

#include "globals.h"
#include "Misc/Alloc.h"

#include <string>
#include <memory>
#include <pthread.h>
#include <functional>

using std::shared_ptr;
using std::unique_ptr;
using std::string;

enum audio_driver { no_audio = 0, jack_audio, alsa_audio};
enum midi_driver  { no_midi = 0, jack_midi, alsa_midi};

class Config;
class MusicIO;
class SynthEngine;
class BeatTracker;


#define NMC_SRATE 44100

class MusicClient
{
private:
    SynthEngine& synth;
    shared_ptr<MusicIO> audioIO;
    shared_ptr<MusicIO> midiIO;

    pthread_t timerThreadId;
    static void* timerThread_fn(void*);
    bool timerWorking;
    Samples dummyAllocation;
    float*  dummyL[NUM_MIDI_PARTS + 1];
    float*  dummyR[NUM_MIDI_PARTS + 1];

public:
    // shall not be copied nor moved
    MusicClient(MusicClient&&)                 = delete;
    MusicClient(MusicClient const&)            = delete;
    MusicClient& operator=(MusicClient&&)      = delete;
    MusicClient& operator=(MusicClient const&) = delete;

    MusicClient(SynthEngine&);
   ~MusicClient();

    bool open(audio_driver, midi_driver);
    bool open(std::function<MusicIO*(SynthEngine&)>&);
    bool start();
    void close();
    uint getSamplerate();
    uint getBuffersize();
    string audioClientName();
    string midiClientName();
    int audioClientId();
    int midiClientId();
    void registerAudioPort(int portnum);

private:
    void createEngines(audio_driver, midi_driver);
    bool launchReplacementThread();
    void stopReplacementThread();
    bool prepDummyBuffers();
    Config& runtime();
};

#endif
