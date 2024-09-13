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

#include <utility>
#include <chrono>

using func::asString;


MusicIO::MusicIO(SynthEngine& _synth, shared_ptr<BeatTracker>&& beat)
    : bufferAllocation{}   // Allocation happens later in prepBuffers()
    , zynLeft{0}
    , zynRight{0}
    , beatTracker{std::move(beat)}
    , synth{_synth}
    { }


Config& MusicIO::runtime() { return synth.getRuntime(); }


void MusicIO::handleMidi(uchar par0, uchar par1, uchar par2, bool in_place)
{
    if (synth.audioOut.load() != _SYS_::mute::Idle)
        return; // nobody listening!

    bool inSync = runtime().isLV2 or (runtime().audioEngine == jack_audio and runtime().midiEngine == jack_midi);

    CommandBlock putData;

    uint  event   = par0 & 0xf0;
    uchar channel = par0 & 0xf;


#ifdef AFTERTOUCH_EMULATE

    if (event == 0xb0 && par1 == AFTERTOUCH_EMULATE)
    {
        par0 = 0xd0 | channel; // change to channel aftertouch
        par1 = par2; // shift parameter across
        synth.mididecode.midiProcess(par0, par1, par2, in_place, inSync);
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
            ++runtime().noteOffSent;
        else
            ++runtime().noteOnSent;
#endif

        if (inSync)
        {
            if (event == 0x80)
                synth.NoteOff(channel, par1);
            else
                synth.NoteOn(channel, par1, par2);
        }
        else
        {
            putData.data.value = float(par2);
            putData.data.type = 8;
            putData.data.control = (event == 0x80);
            putData.data.part = TOPLEVEL::section::midiIn;
            putData.data.kit = channel;
            putData.data.engine = par1;
            synth.midilearn.writeMidi(putData, false);
        }
        if (event == 0x90)
            synth.interchange.noteSeen = true;
        return;
    }
    synth.mididecode.midiProcess(par0, par1, par2, in_place, inSync);
}


bool MusicIO::prepBuffers()
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


BeatTracker::BeatTracker()
    : songVsMonotonicBeatDiff{0}
    { }

BeatTracker::~BeatTracker() { }  // emit VTable here...


void BeatTracker::adjustMonotonicRounding(BeatTracker::BeatValues& beats)
{
    // Try to compensate for rounding errors in monotonic beat. If the
    // difference is small enough from the song beat, then we assume we have not
    // repositioned the transport and we derive an exact value of the monotonic
    // beat from the song beat, instead of adding BPM on every cycle, which
    // accumulates a lot of error over time.
    if (fabsf(beats.songBeat + songVsMonotonicBeatDiff - beats.monotonicBeat) < 0.1f)
        beats.monotonicBeat = beats.songBeat + songVsMonotonicBeatDiff;
    else
        songVsMonotonicBeatDiff = beats.monotonicBeat - beats.songBeat;
}



// to protect a critical section against concurrent access
using Guard = const std::lock_guard<std::mutex>;

// monotonic time scale in microseconds as unsigned 64bit
using Mircos = std::chrono::duration<uint64_t, std::micro>;
using std::chrono::steady_clock;
using std::chrono::floor;


MultithreadedBeatTracker::MultithreadedBeatTracker()
    : mtx{}
    , lastTimeUs{}
    , lastSongBeat{0}
    , lastMonotonicBeat{0}
    , timeUs{}
    , songBeat{0}
    , monotonicBeat{0}
    , bpm{120}
{
    auto now = steady_clock::now();
    auto microTicks = floor<Mircos>(now.time_since_epoch())
                           .count();
    lastTimeUs = microTicks;
    timeUs = microTicks;
}




BeatTracker::BeatValues MultithreadedBeatTracker::setBeatValues(BeatTracker::BeatValues beats)
{
    adjustMonotonicRounding(beats);

    Guard lock(mtx); //--synced------------------------------

    auto now = steady_clock::now();
    auto microTicks = floor<Mircos>(now.time_since_epoch())
                           .count();
    lastTimeUs = timeUs;
    timeUs = microTicks;
    bpm = beats.bpm;
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

    songBeat = beats.songBeat;
    monotonicBeat = beats.monotonicBeat;
    return beats;
}


BeatTracker::BeatValues MultithreadedBeatTracker::getBeatValues()
{
    Guard lock(mtx);  //--synced------------------------------
    BeatTracker::BeatValues ret;

    // read current monotonic time
    auto now = steady_clock::now();
    int64_t microTicks = floor<Mircos>(now.time_since_epoch())
                              .count();

    if (timeUs == lastTimeUs)
    {   // Can only happen on the very first iteration. Avoid division by zero.
        ret.songBeat = 0;
        ret.monotonicBeat = 0;
    }
    else
    {   // Based on beat and clock from MIDI thread,
        // interpolate and find the beat for audio thread.
        auto ratio = float(microTicks - lastTimeUs) / (timeUs - lastTimeUs);
        ret.songBeat = ratio * (songBeat - lastSongBeat) + lastSongBeat;
        ret.monotonicBeat = ratio * (monotonicBeat - lastMonotonicBeat) + lastMonotonicBeat;
    }
    ret.bpm = bpm;
    return ret;
}

BeatTracker::BeatValues MultithreadedBeatTracker::getRawBeatValues()
{
    Guard lock(mtx); //--synced------------------------------
    BeatValues ret = {
        songBeat,
        monotonicBeat,
        bpm,
    };
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

    adjustMonotonicRounding(beats);
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
