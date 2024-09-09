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





Config& MusicClient::runtime() { return synth.getRuntime(); }


MusicClient::MusicClient(SynthEngine& connect_to_engine)
    : synth{connect_to_engine}
    , audioIO{}
    , midiIO{}
    , timerThreadId{0}
    , timerWorking{false}
    , dummyAllocation{}
    , dummyL{0}
    , dummyR{0}
    { }


MusicClient::~MusicClient()
try { close(); }
catch(std::exception& ex){ std::cerr << "Failure closing Music-IO: "<<ex.what()<<std::endl; }
catch(...)               { std::cerr << "Unidentified problem while closing Music-IO."<<std::endl; }



void MusicClient::createEngines(audio_driver useAudio, midi_driver useMidi)
{
    shared_ptr<BeatTracker> beat;
    if (useAudio == jack_audio && useMidi == jack_midi)
        beat = make_shared<SinglethreadedBeatTracker>();
    else
        beat = make_shared<MultithreadedBeatTracker>();

    switch(useAudio)
    {
#ifndef YOSHIMI_LV2_PLUGIN
        case jack_audio:
            audioIO = make_shared<JackEngine>(synth, beat);
            break;
#if defined(HAVE_ALSA)
        case alsa_audio:
            audioIO = make_shared<AlsaEngine>(synth, beat);
            break;
#endif /*ALSA*/
#endif /*LV2*/
        default:
            break;
    }

    switch(useMidi)
    {
#ifndef YOSHIMI_LV2_PLUGIN
        case jack_midi:
            if (useAudio == jack_audio)
                midiIO = audioIO;
            else
                midiIO = make_shared<JackEngine>(synth, beat);
            break;
#if defined(HAVE_ALSA)
        case alsa_midi:
            if (useAudio == alsa_audio)
                midiIO = audioIO;
            else
                midiIO = make_shared<AlsaEngine>(synth, beat);
            break;
#endif /*ALSA*/
#endif /*LV2*/
        default:
            break;
    }

    assert (audioIO or useAudio == no_audio);
    assert (midiIO or useMidi == no_midi);
}


/**
 * Attempt to establish the given combination of audio and MIDI backends.
 * @return `true` if both back-ends could be opened successfully
 */
bool MusicClient::open(audio_driver tryAudio, midi_driver tryMidi)
{
    createEngines(tryAudio, tryMidi);
    return (not audioIO or audioIO->openAudio())
       and (not midiIO  or midiIO->openMidi());
}

/**
 * Attach to an external back-end or plugin-host (notably LV2), handling both audio and MIDI.
 * @param createBackend a functor which creates/attaches the backend to the SynthEngine
 */
bool MusicClient::open(InstanceManager::PluginCreator& createBackend)
{
    audioIO.reset(createBackend(synth));
    // BeatTracker assumed to be created implicitly
    midiIO = audioIO;
    bool success = audioIO->openAudio() and midiIO->openMidi();
    if (not success)
        audioIO.reset();
    return success;
}


bool MusicClient::start()
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


void MusicClient::close()
{
    if (midiIO and midiIO != audioIO)
        midiIO->Close();

    if (audioIO)
        audioIO->Close();
    else
        stopReplacementThread();
}


void MusicClient::stopReplacementThread()
{
    if (timerThreadId == 0 || timerWorking == false)
        return;
    timerWorking = false;
    void* ret = 0;
    pthread_join(timerThreadId, &ret);
    timerThreadId = 0;
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
    size_t buffSize = runtime().buffersize;
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
    auto sleepInterval = Seconds(double(self.runtime().buffersize) / self.runtime().samplerate);
    self.timerWorking = true;
    while (self.timerWorking and self.runtime().runSynth.load(std::memory_order_relaxed))
    {
        self.synth.MasterAudio(self.dummyL, self.dummyR);
        sleep_for(sleepInterval);
    }
    return 0;
}



uint MusicClient::getSamplerate()
{
    return audioIO? audioIO->getSamplerate()
                  : runtime().samplerate;
}

uint MusicClient::getBuffersize()
{
    return audioIO? audioIO->getBuffersize()
                  : runtime().buffersize;
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
