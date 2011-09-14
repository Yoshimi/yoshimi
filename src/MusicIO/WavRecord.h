/*
    WavRecord.h

    Copyright 2009-2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef WAV_RECORD_H
#define WAV_RECORD_H

#include <boost/shared_array.hpp>
#include <string>
#include <sndfile.h>
#include <pthread.h>

using namespace std;

#include "Misc/MiscFuncs.h"

typedef enum {
    nada = 0, ready, recording
} record_state;

class WavRecord:private MiscFuncs
{
    public:
        WavRecord();
        ~WavRecord();
        bool Start(unsigned int sample_rate, int buffer_size);
        void StartRecord(void);
        void StopRecord(void);
        void Close(void);
        bool SetFile(string fpath, string &errmsg);
        bool SetOverwrite(string &errmsg);
        string Filename(void) { return wavFile; }
        bool IsFloat(void) { return float32bit; }
        void Feed(float *samples_left, float *samples_right);
        inline bool Running(void) { return recordState == recording;  }
        inline bool Trigger(void) { return recordState == ready;  }

    private:
        void *recorderThread(void);
        static void *_recorderThread(void *arg);
        static void _cleanup(void *arg);
        void cleanup(void);
        void recordLog(string tag);

        record_state recordState;
        unsigned int samplerate;
        unsigned int buffersize;
        boost::shared_array<float> interleavedFloats;

        string     wavFile;
        bool       float32bit;
        SF_INFO    wavOutInfo;
        SNDFILE   *wavOutsnd;
        pthread_t  pThread;
        bool       threadRun;
        string     recordFifo;
        FILE      *toFifo;
        SF_INFO    fromFifoInfo;
        SNDFILE   *fromFifoSndfle;
        sf_count_t tferSamples;
};

#endif
