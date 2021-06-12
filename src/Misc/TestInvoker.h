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
#include <vector>
#include <memory>
#include <ctime>

#include "Misc/SynthEngine.h"

using std::cout;
using std::endl;
using std::string;

namespace { // local implementation details

    class StopWatch
    {
        timespec start;
    public:
        StopWatch()
        {
            clock_gettime(CLOCK_REALTIME, &start);
        }
        
        size_t getNanosSinceStart()
        {
            timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            
            return (now.tv_nsec - start.tv_nsec)
                 + (now.tv_sec  - start.tv_sec) * 1000*1000*1000;
        }
    };


    class OutputFile
    {
        std::vector<float> buffer;
        std::fstream file;

    public:
        /* disabled output; discard all data */
        OutputFile()
        { }

        /* open file and allocate buffer to collect sound */
        OutputFile(string targetFilename, size_t maxSamples) :
            buffer{},
            file{targetFilename, ios_base::out | ios_base::trunc | ios_base::binary}
        {
            if (not isActive())
                throw std::runtime_error(string{"Failure to open file '"}+targetFilename+"' for writing");
            buffer.reserve(maxSamples);
        }

        OutputFile(OutputFile&) = delete;
        OutputFile(OutputFile&&) = default;
        OutputFile& operator=(OutputFile&) = delete;
        OutputFile& operator=(OutputFile&&) = delete;


        bool isActive() const
        {
            return file.is_open() and file.good();
        }

        explicit operator bool() const
        {
            return isActive();
        }


        void maybeWrite()
        {
            if (not isActive()) return;
            
            char* rawData = reinterpret_cast<char*>(buffer.data());
            size_t numBytes = buffer.size() * sizeof(float);
            file.write(rawData, numBytes);
        }

        void interleave(size_t numSamples, float* samplesL, float* samplesR)
        {
            for (size_t i=0; i<numSamples; ++i)
            {
                buffer.push_back(*(samplesL+i));
                buffer.push_back(*(samplesR+i));
            }
        }
    };

}//(End)implementation details



class TestInvoker
{
    int chan;
    int pitch;
    float duration;
    float holdtime;
    int repetitions;
    int pitchscalestep;
    size_t chunksize;
    size_t smpCnt;
    string targetFilename;

    using BufferHolder = std::unique_ptr<float[]>;

    public:
        TestInvoker() :
            chan{0},
            pitch{60},        // C4
            duration{1.0},    // 1sec
            holdtime{0.8},
            repetitions{4},
            pitchscalestep{4},// move major third upwards
            chunksize{128},
            smpCnt{0},
            targetFilename{"test-out.raw"}
        { }


        /* Main test function: run the SynthEngine synchronous, possibly dump results into a file.
         * Note: the current audio/midi backend is not used at all.
         */
        void performSoundCalculation(SynthEngine& synth)
        {
            BufferHolder buffer;
            OutputFile output = prepareOutput(synth.samplerate);
            allocate(buffer);
            synth.setReproducibleState(0);

            cout << "TEST::Launch"<<endl;
            smpCnt = 0;
            StopWatch timer;
            pullSound(synth, buffer, output);

            size_t runtime = timer.getNanosSinceStart();
            double speed = double(runtime) / smpCnt;
            cout << "TEST::Complete"
                 << " runtime "<<runtime<<" ns"
                 << " speed "<<speed<<" ns/Sample"
                 << endl;
            output.maybeWrite();
        }


    private:
        void allocate(BufferHolder& buffer)
        {
            size_t size = 2 * (NUM_MIDI_PARTS + 1)
                            * chunksize;
            buffer.reset(new float[size]);
        }


        OutputFile prepareOutput(unsigned int samplerate)
        {
            if (0 == targetFilename.size())
                return OutputFile{}; // discard output

            size_t chunkCnt = size_t(ceil(duration * samplerate / chunksize));
            size_t maxSamples = 2 * repetitions * chunkCnt * chunksize;
            return OutputFile{targetFilename, maxSamples};
        }



        void pullSound(SynthEngine& synth, BufferHolder& buffer, OutputFile& output)
        {
            float* buffL[NUM_MIDI_PARTS + 1];
            float* buffR[NUM_MIDI_PARTS + 1];
            for (int i=0; i<=NUM_MIDI_PARTS; ++i)
            {
                buffL[i] = & buffer[(2*i  ) * chunksize];
                buffR[i] = & buffer[(2*i+1) * chunksize];
            }
            
            // find out how much buffer cycles are required to get the desired note play time
            size_t turnCnt = ceilf(duration * synth.samplerate / chunksize);
            size_t holdCnt = ceilf(holdtime * synth.samplerate / chunksize);
            holdCnt = std::min(holdCnt,turnCnt);
            
            // calculate sound data
            for (int tone=0; tone<repetitions; ++tone)
            {
                synth.ShutUp();
                int midiNote = pitch + tone*pitchscalestep;
                midiNote = midiNote % 128;  // possibly wrap around
                synth.NoteOn(chan, midiNote, 64);  //////TODO velocity
                for (size_t i=0; i<holdCnt; ++i)
                    computeCycle(synth,buffL,buffR,output);
                synth.NoteOff(chan, midiNote);
                for (size_t i=0; i < (turnCnt-holdCnt); ++i)
                    computeCycle(synth,buffL,buffR,output);
            }
        }

        void computeCycle(SynthEngine& synth, float** buffL, float** buffR, OutputFile& output)
        {
            size_t numSamples = synth.MasterAudio(buffL,buffR, chunksize);
            smpCnt += numSamples;
            output.interleave(numSamples, buffL[NUM_MIDI_PARTS],buffR[NUM_MIDI_PARTS]);
        }
};



#endif /*TESTINVOKER_H*/
