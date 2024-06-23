/*
    MusicClient.h

    Copyright 2009-2011, Alan Calvert
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

#include "MusicIO/MusicClient.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/AlsaEngine.h"
#include "MusicIO/JackEngine.h"
#include <iostream>
#include <stdlib.h>
#include <cassert>
#include <thread>
#include <string>
#include <set>

using std::string;
using std::unique_ptr;
using std::make_shared;
using std::chrono::duration;
using std::this_thread::sleep_for;


string audio_drivers_str [] = {"no_audio", "jack_audio"
#if defined(HAVE_ALSA)
                               , "alsa_audio"
#endif
                              };
string midi_drivers_str [] = {"no_midi", "jack_midi"
#if defined(HAVE_ALSA)
                              , "alsa_midi"
#endif
                             };

MusicClient *MusicClient::newMusicClient(SynthEngine *_synth)
{
    std::set<music_clients> clSet;
    music_clients c1 = {0, _synth->getRuntime().audioEngine, _synth->getRuntime().midiEngine};
    clSet.insert(c1);
    music_clients c2 = {1, jack_audio, alsa_midi};
    clSet.insert(c2);
    music_clients c3 = {2, jack_audio, jack_midi};
    clSet.insert(c3);
    music_clients c4 = {3, alsa_audio, alsa_midi};
    clSet.insert(c4);
    music_clients c5 = {4, jack_audio, no_midi};
    clSet.insert(c5);
    music_clients c6 = {5, alsa_audio, no_midi};
    clSet.insert(c6);
    music_clients c7 = {6, no_audio, alsa_midi};
    clSet.insert(c7);
    music_clients c8 = {7, no_audio, jack_midi};
    clSet.insert(c8);
    music_clients c9 = {8, no_audio, no_midi}; //this one always will do the work :)
    clSet.insert(c9);

    for (std::set<music_clients>::iterator it = clSet.begin(); it != clSet.end(); ++it)
    {
        MusicClient *client = new MusicClient(*_synth, it->audioDrv, it->midiDrv);
        if (client)
        {
            if (client->Open()) //found working client combination
            {
                if (it != clSet.begin())
                    _synth->getRuntime().configChanged = true;
                _synth->getRuntime().runSynth = true; //reset to true
                _synth->getRuntime().audioEngine = it->audioDrv;
                _synth->getRuntime().midiEngine = it->midiDrv;
                _synth->getRuntime().Log("Using " + audio_drivers_str [it->audioDrv] + " for audio and " + midi_drivers_str [it->midiDrv] + " for midi", _SYS_::LogError);
                return client;
            }
            delete client;
        }
    }
    return 0;
}



Config& MusicClient::runtime() { return synth.getRuntime(); }


MusicClient::MusicClient(SynthEngine& _synth, audio_drivers _audioDrv, midi_drivers _midiDrv)
    : synth{_synth}
    , audioDrv{_audioDrv}
    , midiDrv{_midiDrv}
    , audioIO{}
    , midiIO{}
    , timerThreadId{0}
    , timerWorking{false}
    , dummyAllocation{}
    , dummyL{0}
    , dummyR{0}
{
    shared_ptr<BeatTracker> beat;
    if (audioDrv == jack_audio && midiDrv == jack_midi)
        beat = make_shared<SinglethreadedBeatTracker>();
    else
        beat = make_shared<MultithreadedBeatTracker>();

    switch(audioDrv)
    {
        case jack_audio:
            audioIO = make_shared<JackEngine>(synth, beat);
            break;
#if defined(HAVE_ALSA)
        case alsa_audio:
            audioIO = make_shared<AlsaEngine>(synth, beat);
            break;
#endif
        default:
            break;
    }

    switch(midiDrv)
    {
        case jack_midi:
            if (audioDrv == jack_audio)
                midiIO = audioIO;
            else
                midiIO = make_shared<JackEngine>(synth, beat);
            break;
#if defined(HAVE_ALSA)
        case alsa_midi:
            if (audioDrv == alsa_audio)
                midiIO = audioIO;
            else
                midiIO = make_shared<AlsaEngine>(synth, beat);
            break;
#endif
        default:
            break;
    }

    assert (audioIO or audioDrv == no_audio);
    assert (midiIO or midiDrv == no_midi);
}


bool MusicClient::Open()
{
    bool validAudio = !audioIO or audioIO->openAudio();
    bool validMidi  = !midiIO or midiIO->openMidi();

    if (validAudio and validMidi)
    {
        runtime().audioEngine = audioDrv;
        runtime().midiEngine = midiDrv;
    }
    return validAudio and validMidi;
}


bool MusicClient::Start()
{
    assert(timerThreadId == 0 && !timerWorking);

    bool okAudio = true;
    bool okMidi = true;

    if (audioIO)
        okAudio = audioIO->Start();
    else
        okAudio = launchReplacementThread();

    if (midiIO and midiIO != audioIO)
        okMidi = midiIO->Start();

    return okAudio and okMidi;
}


void MusicClient::Close()
{
    if (midiIO and midiIO != audioIO)
        midiIO->Close();

    if (audioIO)
        audioIO->Close();
    else
    {// stop the replacement timer thread...
        if (timerThreadId == 0 || timerWorking == false)
            return;
        timerWorking = false;
        void* ret = 0;
        pthread_join(timerThreadId, &ret);
        timerThreadId = 0;
    }
}


bool MusicClient::launchReplacementThread()
{
    return prepDummyBuffers()
       and runtime().startThread(&timerThreadId, MusicClient::timerThread_fn, this, false, 0, "Timer?");
}

/**
 * Create dummy buffers, so that the »Timer-Thread« can run the SynthEngine
 * @note this code is copied and adapted from MusicIO
 */
bool MusicClient::prepDummyBuffers()
{
    size_t buffSize = runtime().Buffersize;
    if (buffSize == 0)
        return false;

    size_t allocSize = 2 * (NUM_MIDI_PARTS + 1)
                         * buffSize;
    // All buffers allocated together
    // Note: std::bad_alloc is raised on failure, which kills the application...
    dummyAllocation.reset(allocSize);

    for (size_t i=0; i < (NUM_MIDI_PARTS + 1); ++i)
    {
        dummyL[i] = & dummyAllocation[(2*i  ) * buffSize];
        dummyR[i] = & dummyAllocation[(2*i+1) * buffSize];
    }
    return true;
}

void* MusicClient::timerThread_fn(void *arg)
{
    assert(arg);
    MusicClient& self = * static_cast<MusicClient*>(arg);
    using Seconds = duration<double>;
    auto sleepInterval = Seconds(double(self.runtime().Buffersize) / self.runtime().Samplerate);
    self.timerWorking = true;
    while (self.timerWorking && self.runtime().runSynth)
    {
        self.synth.MasterAudio(self.dummyL, self.dummyR);
        sleep_for(sleepInterval);
    }
    return 0;
}



unsigned int MusicClient::getSamplerate()
{
    return audioIO? audioIO->getSamplerate()
                  : runtime().Samplerate;
}

int MusicClient::getBuffersize()
{
    return audioIO? audioIO->getBuffersize()
                  : runtime().Buffersize;
}

string MusicClient::audioClientName()
{
    return audioIO? audioIO->audioClientName()
                  : "null_audio";
}

string MusicClient::midiClientName()
{
    return midiIO? midiIO->midiClientName()
                 : "null_midi";
}

int MusicClient::audioClientId()
{
    return audioIO? audioIO->audioClientId() : 0;
}

int MusicClient::midiClientId()
{
    return midiIO? midiIO->midiClientId() : 0;
}

void MusicClient::registerAudioPort(int portnum)
{
    if (audioIO)
        audioIO->registerAudioPort(portnum);
}
