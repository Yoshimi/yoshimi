/*
    MusicIO.h

    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2014-2019, Will Godfrey & others

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

#ifndef MUSIC_IO_H
#define MUSIC_IO_H

#include "globals.h"
#include "Misc/Alloc.h"
#include "Misc/SynthEngine.h"

#include <mutex>
#include <memory>
#include <string>

using std::unique_ptr;
using std::shared_ptr;
using std::string;

class BeatTracker;


class MusicIO
{
    public:
        virtual ~MusicIO() = default;

        MusicIO(SynthEngine&, shared_ptr<BeatTracker>&&);
        // shall not be copied or moved
        MusicIO(MusicIO&&)                 = delete;
        MusicIO(MusicIO const&)            = delete;
        MusicIO& operator=(MusicIO&&)      = delete;
        MusicIO& operator=(MusicIO const&) = delete;

        virtual bool openAudio()               = 0;
        virtual bool openMidi()                = 0;
        virtual bool Start()                   = 0;
        virtual void Close()                   = 0;
        virtual void registerAudioPort(int)    = 0;

        virtual uint getSamplerate()     const = 0;
        virtual int getBuffersize()      const = 0;
        virtual string audioClientName() const = 0;
        virtual int audioClientId()      const = 0;
        virtual string midiClientName()  const = 0;
        virtual int midiClientId()       const = 0;

    protected:
        bool prepBuffers();
        void getAudio()    { synth.MasterAudio(zynLeft, zynRight); }
        void handleMidi(uchar par0, uchar par1, uchar par2, bool in_place = false);

        Samples bufferAllocation;
        float*  zynLeft[NUM_MIDI_PARTS + 1];
        float* zynRight[NUM_MIDI_PARTS + 1];

        // The engine which tracks song beats (MIDI driver).
        shared_ptr<BeatTracker> beatTracker;
        SynthEngine& synth;

        Config& runtime();
};

class BeatTracker
{
    public:
        struct BeatValues {
            float songBeat;
            float monotonicBeat;
            float bpm;
        };

    public:
        virtual ~BeatTracker();  // this is an interface

        BeatTracker();

        // The pair contains the song beat (relative to song beginning) and
        // monotonic beat (relative to time origin), respectively, and is used
        // by subclasses to set the current beat values.
        //
        // The setter returns the same values it's given, except that they can
        // wrap around. Sub classes that call this function should consider
        // storing the wrapped value in order to preserve precision when the
        // beat count gets high. The wrapped around value is guaranteed to
        // divide all possible LFO fractions.
        virtual BeatValues setBeatValues(BeatValues beats) = 0;
        // Gets the beat values as close as possible in time to this moment.
        virtual BeatValues getBeatValues() = 0;
        // Gets the raw beat values without any sort of time calculation.
        virtual BeatValues getRawBeatValues() = 0;

    protected:
        void adjustMonotonicRounding(BeatValues& beats);

    private:
        float songVsMonotonicBeatDiff;
};

class MultithreadedBeatTracker : public BeatTracker
{
    public:
        MultithreadedBeatTracker();
       ~MultithreadedBeatTracker() = default;
        // shall not be copied or moved
        MultithreadedBeatTracker(MultithreadedBeatTracker&&)                 = delete;
        MultithreadedBeatTracker(MultithreadedBeatTracker const&)            = delete;
        MultithreadedBeatTracker& operator=(MultithreadedBeatTracker&&)      = delete;
        MultithreadedBeatTracker& operator=(MultithreadedBeatTracker const&) = delete;

        // These two functions are mutually thread safe, even though they
        // operate on the same data. The first is usually called from the MIDI
        // thread, the second from the audio thread.
        BeatValues setBeatValues(BeatValues beats) override;
        BeatValues getBeatValues()                 override;
        BeatValues getRawBeatValues()              override;

    private:
        // Current and last time and beats of the MIDI clock.
        std::mutex mtx;
        uint64_t lastTimeUs;
        float    lastSongBeat;
        float    lastMonotonicBeat;
        uint64_t timeUs;
        float    songBeat;
        float    monotonicBeat;
        float    bpm;
};

class SinglethreadedBeatTracker : public BeatTracker
{
    public:
        SinglethreadedBeatTracker();

        BeatValues setBeatValues(BeatValues beats) override;
        BeatValues getBeatValues()                 override;
        BeatValues getRawBeatValues()              override;

    private:
        BeatValues beats;
};

#endif /*MUSIC_IO_H*/
