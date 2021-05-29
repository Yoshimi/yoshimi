/*
    TestInvoker.h - invoke sound synthesis for automated testing

    Copyright 2021, Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef TESTINVOKER_H
#define TESTINVOKER_H

#include <string>
#include <unistd.h>
#include <iostream>
#include <memory>

#include "Misc/SynthEngine.h"

using std::cout;
using std::endl;
using std::string;

namespace { // local implementation details
}//(End)implementation details


class TestInvoker
{
    int chan;
    int pitch;
    float duration;
    uint repetitions;
    size_t chunksize;
    size_t smpCnt;

    using BufferHolder = std::unique_ptr<float[]>;
    
    public:
        TestInvoker() :
            chan{0},
            pitch{60},        // C4
            duration{1.0},    // 1sec
            repetitions{1},
            chunksize{128},
            smpCnt{0}
        { }


        /* Main test function: run the SynthEngine synchronous, possibly dump results into a file.
         * Note: the current audio/midi backend is not used at all.
         */
        void performSoundCalculation(SynthEngine& synth)
        {
            BufferHolder buffer;
            allocate(buffer);
            awaitMute(synth);
            synth.reseed(0);
            cout << "TEST::Launch"<<endl;
            //////////////////////TODO maybe open output file
            //////////////////////TODO capture start time
            for (uint i=0; i<repetitions; ++i)
                pullSound(synth, buffer);
            //////////////////////TODO capture end time
            //////////////////////TODO calculated overall time
            cout << "TEST::Complete"<<endl;
        }


    private:
        /* wait for a preceding stop command to reach the Synth,
         * and for the Synth to fade down and kill all running notes.
         * See InterChange::sortResultsThread and the corresponding
         * switch(sound)... in SynthEngine::MasterAudio
         */
        void awaitMute(SynthEngine& synth)
        {
            while (synth.audioOut.load() != _SYS_::mute::Idle)
            {
                usleep(2000); // with buffersize 128 and 48kHz -> one buffer lasts ~ 2.6ms
            }
        }


        void allocate(BufferHolder& buffer)
        {
            size_t size = 2 * (NUM_MIDI_PARTS + 1)
                            * chunksize;
            buffer.reset(new float[size]);
        }



        void pullSound(SynthEngine& synth, BufferHolder& buffer)
        {
            float* buffL[NUM_MIDI_PARTS + 1];
            float* buffR[NUM_MIDI_PARTS + 1];
            for (uint i=0; i<=NUM_MIDI_PARTS; ++i)
            {
                buffL[i] = & buffer[(2*i  ) * chunksize];
                buffR[i] = & buffer[(2*i+1) * chunksize];
            }
            
            // find out how much buffer cycles are required to get the desired note play time
            size_t buffCnt = rintf(duration * synth.samplerate / chunksize);
            
            // calculate sound data
            synth.NoteOn(chan, pitch, 64);  //////TODO velocity
            for (size_t i=0; i<buffCnt; ++i)
                smpCnt += synth.MasterAudio(buffL,buffR, chunksize);
            synth.NoteOff(chan, pitch);
        }
};



#endif /*TESTINVOKER_H*/
