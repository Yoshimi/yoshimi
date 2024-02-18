/*
    MusicIO.cpp

    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2020, Will Godfrey & others

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

*/

/*
 * Uncomment the following define to emulate aftertouch
 * To get the impression of channel aftertouch we change the
 * event of the specified controller number.
 * Change the value to suit your circumstances.
 */
//#define AFTERTOUCH_EMULATE 94

#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "Misc/FormatFuncs.h"
#include "MusicIO/MusicIO.h"

using func::asString;


MusicIO::MusicIO(SynthEngine *_synth, BeatTracker *_beatTracker) :
    LV2_engine{_synth->getIsLV2Plugin()},
    bufferAllocation{},  // Allocation happens later in prepBuffers()
    zynLeft{0},
    zynRight{0},
    interleaved(),
    beatTracker(_beatTracker),
    synth(_synth)
{ }



void MusicIO::setMidi(unsigned char par0, unsigned char par1, unsigned char par2, bool in_place)
{
    if (synth->audioOut.load() != _SYS_::mute::Idle)
        return; // nobody listening!

    bool inSync = LV2_engine || (synth->getRuntime().audioEngine == jack_audio && synth->getRuntime().midiEngine == jack_midi);

    CommandBlock putData;

    unsigned int event = par0 & 0xf0;
    unsigned char channel = par0 & 0xf;


#ifdef AFTERTOUCH_EMULATE

    if (event == 0xb0 && par1 == AFTERTOUCH_EMULATE)
    {
        par0 = 0xd0 | channel; // change to channel aftertouch
        par1 = par2; // shift parameter across
        synth->mididecode.midiProcess(par0, par1, par2, in_place, inSync);
        return;
    }
#endif

/*
 * This below is a much simpler (faster) way
 * to do note-on and note-off
 * Tested on ALSA JACK and LV2 all combinations!
 */

    if (event == 0x80 || event == 0x90)
    {
        if (par2 < 1) // zero volume note on.
            event = 0x80;

#ifdef REPORT_NOTES_ON_OFF
        if (event == 0x80) // note test
            ++synth->getRuntime().noteOffSent;
        else
            ++synth->getRuntime().noteOnSent;
#endif

        if (inSync)
        {
            if (event == 0x80)
                synth->NoteOff(channel, par1);
            else
                synth->NoteOn(channel, par1, par2);
        }
        else
        {
            putData.data.value = float(par2);
            putData.data.type = 8;
            putData.data.control = (event == 0x80);
            putData.data.part = TOPLEVEL::section::midiIn;
            putData.data.kit = channel;
            putData.data.engine = par1;
            synth->midilearn.writeMidi(&putData, false);
        }
        if (event == 0x90)
            synth->interchange.noteSeen = true;
        return;
    }
    synth->mididecode.midiProcess(par0, par1, par2, in_place, inSync);
}


bool MusicIO::prepBuffers(void)
{
    size_t buffSize = getBuffersize();
    if (buffSize == 0)
        return false;

    size_t allocSize = 2 * (NUM_MIDI_PARTS + 1)
                         * buffSize;
    // All buffers allocated together
    // Note: std::bad_alloc is raised on failure, which kills the application...
    bufferAllocation.reset(allocSize);

    for (size_t i=0; i < (NUM_MIDI_PARTS + 1); ++i)
    {
        zynLeft[i]  = & bufferAllocation[(2*i  ) * buffSize];
        zynRight[i] = & bufferAllocation[(2*i+1) * buffSize];
    }
    return true;
}


BeatTracker::BeatTracker() :
    songVsMonotonicBeatDiff(0)
{
}

BeatTracker::~BeatTracker()
{
}

void BeatTracker::adjustMonotonicRounding(BeatTracker::BeatValues *beats)
{
    // Try to compensate for rounding errors in monotonic beat. If the
    // difference is small enough from the song beat, then we assume we have not
    // repositioned the transport and we derive an exact value of the monotonic
    // beat from the song beat, instead of adding BPM on every cycle, which
    // accumulates a lot of error over time.
    if (fabsf(beats->songBeat + songVsMonotonicBeatDiff - beats->monotonicBeat) < 0.1f)
        beats->monotonicBeat = beats->songBeat + songVsMonotonicBeatDiff;
    else
        songVsMonotonicBeatDiff = beats->monotonicBeat - beats->songBeat;
}

MultithreadedBeatTracker::MultithreadedBeatTracker()
{
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    uint64_t clock = time.tv_sec * 1000000 + time.tv_nsec / 1000;

    lastTimeUs = clock;
    lastSongBeat = 0;
    lastMonotonicBeat = 0;
    timeUs = clock;
    songBeat = 0;
    monotonicBeat = 0;
    bpm = 120;
    pthread_mutex_init(&mutex, NULL);
}

MultithreadedBeatTracker::~MultithreadedBeatTracker()
{
    pthread_mutex_destroy(&mutex);
}

BeatTracker::BeatValues MultithreadedBeatTracker::setBeatValues(BeatTracker::BeatValues beats)
{
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    uint64_t clock = time.tv_sec * 1000000 + time.tv_nsec / 1000;

    adjustMonotonicRounding(&beats);

    //--------------------------------
    pthread_mutex_lock(&mutex);

    lastTimeUs = timeUs;

    if (beats.songBeat >= LFO_BPM_LCM)
    {
        beats.songBeat -= LFO_BPM_LCM;
        lastSongBeat = songBeat - LFO_BPM_LCM;
    }
    else
        lastSongBeat = songBeat;

    if (beats.monotonicBeat >= LFO_BPM_LCM)
    {
        beats.monotonicBeat -= LFO_BPM_LCM;
        lastMonotonicBeat = monotonicBeat - LFO_BPM_LCM;
    }
    else
        lastMonotonicBeat = monotonicBeat;

    timeUs = clock;
    songBeat = beats.songBeat;
    monotonicBeat = beats.monotonicBeat;
    bpm = beats.bpm;

    pthread_mutex_unlock(&mutex);
    //--------------------------------

    return beats;
}

BeatTracker::BeatValues MultithreadedBeatTracker::getBeatValues()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);

    uint64_t clock = t.tv_sec * 1000000 + t.tv_nsec / 1000;

    BeatTracker::BeatValues ret;

    pthread_mutex_lock(&mutex);
    uint64_t lastTime = lastTimeUs;
    float lastSongBeatTmp = lastSongBeat;
    float lastMonotonicBeatTmp = lastMonotonicBeat;
    uint64_t time = timeUs;
    float songBeatTmp = songBeat;
    float monotonicBeatTmp = monotonicBeat;
    ret.bpm = bpm;
    pthread_mutex_unlock(&mutex);

    if (time == lastTime)
    {
        // Can only happen on the very first iteration. Avoid division by zero.
        ret.songBeat = 0;
        ret.monotonicBeat = 0;
    }
    else
    {
        // Based on beat and clock from MIDI thread, interpolate and find the
        // beat for audio thread.
        float ratio = (float)(clock - lastTime) / (time - lastTime);
        ret.songBeat = ratio * (songBeatTmp - lastSongBeatTmp) + lastSongBeatTmp;
        ret.monotonicBeat = ratio * (monotonicBeatTmp - lastMonotonicBeatTmp) + lastMonotonicBeatTmp;
    }

    return ret;
}

BeatTracker::BeatValues MultithreadedBeatTracker::getRawBeatValues()
{
    pthread_mutex_lock(&mutex);
    BeatValues ret = {
        songBeat,
        monotonicBeat,
        bpm,
    };
    pthread_mutex_unlock(&mutex);
    return ret;
}

SinglethreadedBeatTracker::SinglethreadedBeatTracker()
{
    beats.songBeat = 0;
    beats.monotonicBeat = 0;
    beats.bpm = 120;
}

BeatTracker::BeatValues SinglethreadedBeatTracker::setBeatValues(BeatTracker::BeatValues beats)
{
    if (beats.songBeat >= LFO_BPM_LCM)
        beats.songBeat -= LFO_BPM_LCM;
    if (beats.monotonicBeat >= LFO_BPM_LCM)
        beats.monotonicBeat -= LFO_BPM_LCM;

    adjustMonotonicRounding(&beats);

    this->beats = beats;

    return beats;
}

BeatTracker::BeatValues SinglethreadedBeatTracker::getBeatValues()
{
    return beats;
}

BeatTracker::BeatValues SinglethreadedBeatTracker::getRawBeatValues()
{
    return beats;
}
