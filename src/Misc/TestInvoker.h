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
#include <functional>
#include <vector>
#include <memory>
#include <ctime>

#include "Misc/SynthEngine.h"
#include "Misc/CliFuncs.h"
#include "CLI/Parser.h"


namespace test {

using std::cout;
using std::endl;
using std::string;
using std::function;

using func::asString;
using func::asCompactString;
using func::asMidiNoteString;
using func::string2int;
using func::string2int127;
using func::string2float;

namespace type = TOPLEVEL::type;

using midiVal = unsigned char;


namespace { // local implementation details

    class StopWatch
    {
        timespec mark;
        size_t nanoSum;

    public:
        StopWatch() :
            mark{0,0},
            nanoSum{0}
        { }

        void start()
        {
            clock_gettime(CLOCK_REALTIME, &mark);
        }

        void stop()
        {
            timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            nanoSum += (now.tv_nsec - mark.tv_nsec)
                     + (now.tv_sec  - mark.tv_sec) * 1000*1000*1000;
        }

        size_t getCumulatedNanos()
        {
            return nanoSum;
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



    /* === helpers to handle CLI parameter parsing uniformly === */
    inline string showTestParam(unsigned char val) { return asString(int(val)); }
    inline string showTestParam(size_t val)        { return asString(val);      }
    inline string showTestParam(float val)         { return asString(val);      }
    inline string showTestParam(int val)           { return asString(val);      }
    inline string showTestParam(string s)          { return string{"\""}+s+"\"";}

    template<typename NUM>
    inline NUM clamped(NUM rawVal, NUM min, NUM max)
    {
        return rawVal > max? max
             : rawVal < min? min
             :               rawVal;
    }

    inline function<int(string)> limited(int min, int max)
    {
        return [=](string s)
                  {
                      int rawVal = string2int(s);
                      return clamped(rawVal,min,max);
                  };
    }

    inline function<float(string)> limited(float min, float max)
    {
        return [=](string s)
                  {
                      float rawVal = string2float(s);
                      return clamped(rawVal,min,max);
                  };
    }

    inline string getFilename(string cliInput)
    {
        string name;
        for (char c : cliInput)
        {
            if (::isspace(c)) break;
            name += c;
        }
        if (name.length() < 4 || ".raw" != name.substr(name.length()-4, 4))
            name += ".raw";
        return name;
    }

    /* Bounce the resulting MIDI note when repeating a scale step up or down.
     * At the end of the value range, this sequence proceeds mirrored downwards:
     * 0..127,126..1,0..127... */
    inline unsigned char bouncedNote(int note)
    {
        const int cycle = 2 * 127;
        assert(-100*cycle < note && note < +100*cycle);
        note = (note + 100*cycle) % cycle;
        if (note > cycle/2)
            note = cycle - note;
        assert(0 <= note && note <= 127);
        return note;
    }

}//(End)implementation detail namespace



/* Self-contained test invoker component to perform acceptance tests of the SynthEngine.
 * Used by the "test"-context within the CLI to define parameters and launch a test run.
 * This kind of test run will disrupt any other sound production, then synchronously
 * launch sound calculation with well defined initial state and finally exit Yoshimi.
 */
class TestInvoker
{
    midiVal chan;            // MIDI channel (1..16)
    midiVal pitch;           // MIDI note
    midiVal velocity;
    float  duration;         // in seconds; overall extension of the individual test calculation
    float  holdfraction;     // fraction of the duration until sending note off
    int    repetitions;      // number of test tones in sequence
    int    scalestep;        // semi tones up/down to move for each tone
    size_t chunksize;        // number of samples to calculate at once. Note: < SynthEngine.buffersize
    string targetFilename;   // RAW file to write generated samples; "" => just calculate, don't write to file

    size_t smpCnt;

    using BufferHolder = std::unique_ptr<float[]>;
    enum ParamOp{SET,GET,MAX,MIN,DEFAULT};

    public:
        TestInvoker() :
            chan{1},
            pitch{60},       // C4
            velocity{64},
            duration{1.0},   // 1sec
            holdfraction{0.8},
            repetitions{4},
            scalestep{4},    // move major third upwards
            chunksize{0},    // 0 means: initialise to SynthEngine.buffersize
            targetFilename{""},
            smpCnt{0}
        { }

        /* Delegate for the CLI-CmdInterpreter: handle the CLI instructions
         * to get and set parameter values for the SynthEngine test.
         * return: response to be printed to show command results.
         */
        bool handleParameterChange(cli::Parser& input, unsigned char controlType, string& response, size_t bfsz)
        {
            if (!chunksize)
                chunksize = bfsz; // fill in default (depends on Synth)

            controlType &= (type::Write|type::Default| type::Maximum|type::Minimum);
            ParamOp operation = controlType == type::Write?   SET
                              : controlType == type::Maximum? MAX
                              : controlType == type::Minimum? MIN
                              : controlType == type::Default? DEFAULT
                              :                               GET;
                                           //--------------------------------+cmdID--------+descriptive-name----+default+min+max--+converter-func-----
            return doTreatParameter<midiVal>(operation, this->pitch,         "note",       "MIDI Note",              60,   0,127,  string2int127,       input, response)
                || doTreatParameter<midiVal>(operation, this->chan,          "channel",    "MIDI Channel",            1,   1, 16,  limited(1,16),       input, response)
                || doTreatParameter<midiVal>(operation, this->velocity,      "velocity",   "MIDI Velocity",          64,   0,127,  string2int127,       input, response)
                || doTreatParameter<float>  (operation, this->duration,      "duration",   "Overall duration(secs)",1.0,   0, 10,  limited(0.01f,10.0f),input, response)
                || doTreatParameter<float>  (operation, this->holdfraction,  "holdfraction","Note hold (fraction)", 0.8,   0,1.0,  limited(0.1f,1.0f),  input, response)
                || doTreatParameter<int>    (operation, this->repetitions,   "repetitions","Test note repetitions",   4,   1,100,  limited(1,100),      input, response)
                || doTreatParameter<int>    (operation, this->scalestep,     "scalestep",  "Semi tones up/down",      4,-100,100,  limited(-100,+100),  input, response)
                || doTreatParameter<size_t> (operation, this->chunksize,     "buffersize", "Smps per call",        bfsz,   1,bfsz, limited(1,bfsz),     input, response)
                || doTreatParameter<string> (operation, this->targetFilename,"target",     "Target RAW-filename",    "",  "","?",  getFilename,         input, response)
                 ;
        }

        string showTestParams(bool compact)
        {
            if (compact)
                return string{" TEST: "}
                     + (repetitions > 1? asString(repetitions)+"·":"")
                     + asMidiNoteString(pitch)
                     + (repetitions == 1? "" : scalestep==0? "" : " "+asString(scalestep) + (scalestep > 0? "⤴":"⤵"))
                     + " "+(duration < 1.0? asCompactString(duration*1000)+"ms" : asCompactString(duration)+"s")
                     + (0==targetFilename.length()? "":" >>\""+targetFilename+"\"")
                     ;
            else
                return string{" TEST: exec "}
                     + (repetitions > 1? asString(repetitions)+" notes ":"")
                     + (repetitions > 1 && scalestep != 0? ("start "+asMidiNoteString(pitch)+" step "+asString(scalestep)
                                                           +  (scalestep > 0? " up":" down")
                                                           +" to "+asMidiNoteString(bouncedNote(pitch+(repetitions-1)*scalestep))
                                                           )
                                                         : asMidiNoteString(pitch))
                     + " on Ch."+asString(int(chan))
                     + (velocity!=64? " vel."+asString(int(velocity)):"")
                     + (repetitions > 1? " each ":" for ")
                     + (duration < 1.0? asCompactString(duration*1000)+"ms" : asCompactString(duration)+"s")
                     + (holdfraction < 1.0? " (hold"+asString(100*holdfraction)+"%)":"")
                     + " buffer="+asString(chunksize)
                     + (0==targetFilename.length()? " [calc only]":" write \""+targetFilename+"\"")
                     ;
        }


        /* Main test function: run the SynthEngine synchronous, possibly dump results into a file.
         * Note: the current audio/midi backend is not used at all.
         */
        void performSoundCalculation(SynthEngine& synth)
        {
            if (!chunksize) chunksize = synth.buffersize;
            BufferHolder buffer;
            OutputFile output = prepareOutput(synth.samplerate);
            allocate(buffer);
            synth.getRuntime().Log("TEST::Prepare");
            synth.setReproducibleState(0);

            synth.getRuntime().Log("TEST::Launch");
            smpCnt = 0;
            StopWatch timer;
            pullSound(synth, buffer, output, timer);

            size_t runtime = timer.getCumulatedNanos();
            double speed = double(runtime) / smpCnt;
            synth.getRuntime().Log(string{"TEST::Complete"}
                                  +" runtime "+asCompactString(runtime)+" ns"
                                  +" speed "+asCompactString(speed)+" ns/Sample"
                                  +" samples "+asString(smpCnt)
                                  +" notes "+asString(repetitions)
                                  +" buffer "+asString(chunksize)
                                  +" rate "+asString(synth.samplerate)
                                  );
            output.maybeWrite();
        }



    private:
        template<typename VAL>
        bool doTreatParameter(ParamOp operation, VAL& theParameter,
                              string const& cmdID, string const& descriptiveName,
                              VAL defaultVal, VAL minVal, VAL maxVal,
                              function<VAL(string)> parseVal,
                              cli::Parser& input, string& response);


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



        void pullSound(SynthEngine& synth, BufferHolder& buffer, OutputFile& output, StopWatch& timer)
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
            size_t holdCnt = ceilf(holdfraction*duration * synth.samplerate / chunksize);
            holdCnt = std::min(holdCnt,turnCnt);

            synth.ShutUp(); // TODO Ichthyo 6/21: why do we need to invoke that twice to clear state on chained reverb effects?

            // calculate sound data
            for (int tone=0; tone<repetitions; ++tone)
            {
                synth.ShutUp();
                int midiNote = pitch + tone*scalestep;
                midiNote = bouncedNote(midiNote); // bounce back when leaving value range

                timer.start();
                synth.NoteOn(chan-1, midiNote, velocity);
                for (size_t i=0; i<holdCnt; ++i)
                    computeCycle(synth,buffL,buffR,output);
                synth.NoteOff(chan-1, midiNote);
                for (size_t i=0; i < (turnCnt-holdCnt); ++i)
                    computeCycle(synth,buffL,buffR,output);
                timer.stop();
            }
        }

        void computeCycle(SynthEngine& synth, float** buffL, float** buffR, OutputFile& output)
        {
            size_t numSamples = synth.MasterAudio(buffL,buffR, chunksize);
            smpCnt += numSamples;
            output.interleave(numSamples, buffL[NUM_MIDI_PARTS],buffR[NUM_MIDI_PARTS]);
        }
};



/* Probe if the current CLI parser input can be interpreted as operation
 * to set, get or retrieve max/min/default for one specific test parameter.
 * If possible, perform that operation, fill out the response for the CLI.
 */
template<typename VAL>
inline bool TestInvoker::doTreatParameter(ParamOp operation, VAL& theParameter,
                                          string const& cmdID, string const& descriptiveName,
                                          VAL defaultVal, VAL minVal, VAL maxVal,
                                          function<VAL(string)> parseVal,
                                          cli::Parser& input, string& response)
{
    if (input.matchnMove(2, cmdID.c_str()))
    {
        VAL resVal = defaultVal;
        response = descriptiveName;
        switch (operation)
        {
            case SET:
                if (input.isalnum() || '-' == input.peek() || '.' == input.peek())
                {
                    resVal = parseVal(input);
                    input.skipChars();
                }
                theParameter = resVal;
                response += " set to: ";
            break;
            case GET:
                resVal = theParameter;
                response += " is: ";
            break;
            case MAX:
                resVal = maxVal;
                response += " Max ";
            break;
            case MIN:
                resVal = minVal;
                response += " Min ";
            break;
            case DEFAULT:
                response += " Default ";
            break;
        }
        response += showTestParam(resVal);
        return true;
    }
    else
        return false;
}

}// namespace test
#endif /*TESTINVOKER_H*/
