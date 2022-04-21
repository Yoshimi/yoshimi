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

#include <string>


class SynthEngine;
class BeatTracker;

class MusicIO
{
    public:
        virtual ~MusicIO() = default;

        MusicIO(SynthEngine *_synth, BeatTracker *_beatTracker);
        // shall not be copied or moved
        MusicIO(MusicIO&&)                 = delete;
        MusicIO(MusicIO const&)            = delete;
        MusicIO& operator=(MusicIO&&)      = delete;
        MusicIO& operator=(MusicIO const&) = delete;

        virtual unsigned int getSamplerate(void) = 0;
        virtual int getBuffersize(void) = 0;
        virtual bool Start(void) = 0;
        virtual void Close(void) = 0;
        virtual bool openAudio(void) = 0;
        virtual bool openMidi(void) = 0;
        virtual std::string audioClientName(void) = 0;
        virtual int audioClientId(void) = 0;
        virtual std::string midiClientName(void) = 0;
        virtual int midiClientId(void) = 0;
        virtual void registerAudioPort(int) = 0;

    protected:
        bool prepBuffers(void);
        void getAudio(void) { if (synth) synth->MasterAudio(zynLeft, zynRight); }
        void setMidi(unsigned char par0, unsigned char par1, unsigned char par2, bool in_place = false);

        bool LV2_engine;
        Samples bufferAllocation;
        float*  zynLeft[NUM_MIDI_PARTS + 1];
        float* zynRight[NUM_MIDI_PARTS + 1];

        std::unique_ptr<int[]> interleaved; // output buffer for 16bit interleaved audio

        // The engine which tracks song beats (MIDI driver).
        BeatTracker *beatTracker;

        SynthEngine *synth;
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
        BeatTracker();
        virtual ~BeatTracker();

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
        void adjustMonotonicRounding(BeatValues *beats);

    private:
        float songVsMonotonicBeatDiff;
};

class MultithreadedBeatTracker : public BeatTracker
{
    public:
        MultithreadedBeatTracker();
        ~MultithreadedBeatTracker();

        // These two functions are mutually thread safe, even though they
        // operate on the same data. The first is usually called from the MIDI
        // thread, the second from the audio thread.
        BeatValues setBeatValues(BeatValues beats);
        BeatValues getBeatValues();
        BeatValues getRawBeatValues();

    private:
        // Current and last time and beats of the MIDI clock.
        volatile uint64_t lastTimeUs;
        volatile float    lastSongBeat;
        volatile float    lastMonotonicBeat;
        volatile uint64_t timeUs;
        volatile float    songBeat;
        volatile float    monotonicBeat;
        volatile float    bpm;
        pthread_mutex_t   mutex;
};

class SinglethreadedBeatTracker : public BeatTracker
{
    public:
        SinglethreadedBeatTracker();

        BeatValues setBeatValues(BeatValues beats);
        BeatValues getBeatValues();
        BeatValues getRawBeatValues();

    private:
        BeatValues beats;
};

#endif
