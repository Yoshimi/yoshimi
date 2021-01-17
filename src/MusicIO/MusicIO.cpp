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
    interleaved(NULL),
    beatTracker(_beatTracker),
    synth(_synth)
{
    memset(zynLeft, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    memset(zynRight, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    LV2_engine = synth->getIsLV2Plugin();
}


MusicIO::~MusicIO()
{
    for (int npart = 0; npart < (NUM_MIDI_PARTS + 1); ++npart)
    {
        if (zynLeft[npart])
        {
            fftwf_free(zynLeft[npart]);
            zynLeft[npart] = NULL;
        }
        if (zynRight[npart])
        {
            fftwf_free(zynRight[npart]);
            zynRight[npart] = NULL;
        }
    }
}


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
        par0 = 0xd0 | channel; // change to chanel aftertouch
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
        return;
    }
    synth->mididecode.midiProcess(par0, par1, par2, in_place, inSync);
}


bool MusicIO::prepBuffers(void)
{
    int buffersize = getBuffersize();
    if (buffersize > 0)
    {
        for (int part = 0; part < (NUM_MIDI_PARTS + 1); part++)
        {
            if (!(zynLeft[part] = (float*) fftwf_malloc(buffersize * sizeof(float))))
                goto bail_out;
            if (!(zynRight[part] = (float*) fftwf_malloc(buffersize * sizeof(float))))
                goto bail_out;
            memset(zynLeft[part], 0, buffersize * sizeof(float));
            memset(zynRight[part], 0, buffersize * sizeof(float));

        }
        return true;
    }

bail_out:
    synth->getRuntime().Log("Failed to allocate audio buffers, size " + asString(buffersize));
    for (int part = 0; part < (NUM_MIDI_PARTS + 1); part++)
    {
        if (zynLeft[part])
        {
            fftwf_free(zynLeft[part]);
            zynLeft[part] = NULL;
        }
        if (zynRight[part])
        {
            fftwf_free(zynRight[part]);
            zynRight[part] = NULL;
        }
    }
    if (interleaved)
    {
        delete[] interleaved;
        interleaved = NULL;
    }
    return false;
}

BeatTracker::BeatTracker() :
    songVsMonotonicBeatDiff(0)
{
}

BeatTracker::~BeatTracker()
{
}

void BeatTracker::adjustMonotonicRounding(std::pair<float, float> *beats)
{
    // Try to compensate for rounding errors in monotonic beat. If the
    // difference is small enough from the song beat, then we assume we have not
    // repositioned the transport and we derive an exact value of the monotonic
    // beat from the song beat, instead of adding BPM on every cycle, which
    // accumulates a lot of error over time.
    if (fabsf(beats->first + songVsMonotonicBeatDiff - beats->second) < 0.1f)
        beats->second = beats->first + songVsMonotonicBeatDiff;
    else
        songVsMonotonicBeatDiff = beats->second - beats->first;
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
    pthread_mutex_init(&mutex, NULL);
}

MultithreadedBeatTracker::~MultithreadedBeatTracker()
{
    pthread_mutex_destroy(&mutex);
}

std::pair<float, float> MultithreadedBeatTracker::setBeatValues(std::pair<float, float> beats)
{
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    uint64_t clock = time.tv_sec * 1000000 + time.tv_nsec / 1000;

    adjustMonotonicRounding(&beats);

    //--------------------------------
    pthread_mutex_lock(&mutex);

    lastTimeUs = timeUs;

    if (beats.first >= LFO_BPM_LCM) {
        beats.first -= LFO_BPM_LCM;
        lastSongBeat = songBeat - LFO_BPM_LCM;
    } else
        lastSongBeat = songBeat;

    if (beats.second >= LFO_BPM_LCM) {
        beats.second -= LFO_BPM_LCM;
        lastMonotonicBeat = monotonicBeat - LFO_BPM_LCM;
    } else
        lastMonotonicBeat = monotonicBeat;

    timeUs = clock;
    songBeat = beats.first;
    monotonicBeat = beats.second;

    pthread_mutex_unlock(&mutex);
    //--------------------------------

    return beats;
}

std::pair<float, float> MultithreadedBeatTracker::getBeatValues()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);

    uint64_t clock = t.tv_sec * 1000000 + t.tv_nsec / 1000;

    pthread_mutex_lock(&mutex);
    uint64_t lastTime = lastTimeUs;
    std::pair<float, float> lastBeats(lastSongBeat, lastMonotonicBeat);
    uint64_t time = timeUs;
    std::pair<float, float> beats(songBeat, monotonicBeat);
    pthread_mutex_unlock(&mutex);

    std::pair<float, float> ret;
    if (time == lastTime) {
        if (clock - time > 1000000) {
            // If no MIDI clock messages have arrived for over a second, revert
            // to a static 120 BPM. This is just a fallback to prevent
            // oscillators from stalling completely.
            ret.first = beats.first + (float)(clock - time) / 1000000.0f * 120.0f / 60.0f;
            ret.second = beats.second + (float)(clock - time) / 1000000.0f * 120.0f / 60.0f;
        }
    } else {
        // Based on beat and clock from MIDI thread, interpolate and find the
        // beat for audio thread.
        float ratio = (float)(clock - lastTime) / (time - lastTime);
        ret.first = ratio * (beats.first - lastBeats.first) + lastBeats.first;
        ret.second = ratio * (beats.second - lastBeats.second) + lastBeats.second;
    }

    return ret;
}

SinglethreadedBeatTracker::SinglethreadedBeatTracker() :
    songBeat(0),
    monotonicBeat(0)
{
}

std::pair<float, float> SinglethreadedBeatTracker::setBeatValues(std::pair<float, float> beats)
{
    if (beats.first >= LFO_BPM_LCM)
        beats.first -= LFO_BPM_LCM;
    if (beats.second >= LFO_BPM_LCM)
        beats.second -= LFO_BPM_LCM;

    adjustMonotonicRounding(&beats);

    songBeat = beats.first;
    monotonicBeat = beats.second;

    return beats;
}

std::pair<float, float> SinglethreadedBeatTracker::getBeatValues()
{
    return std::pair<float, float>(songBeat, monotonicBeat);
}
