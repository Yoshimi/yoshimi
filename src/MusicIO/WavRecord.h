/*
    WavRecord.h

    Copyright 2009, Alan Calvert

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

#include <string>
#include <sndfile.h>
#include <pthread.h>

using namespace std;

typedef enum { nada = 0, stopped, recording } record_state;

class WavRecord {
    public:
        WavRecord();
        ~WavRecord();
        bool PrepWav(void);
        void RecordStart(void);
        void RecordFeed(void);
        void RecordStop(void);
        void RecordClose(void);
        bool SetWavFile(string fpath, string& errmsg);
        bool SetWavOverwrite(string& errmsg);
        string WavFilename(void) { return wavFile; };
        bool WavIsFloat(void) { return float32bit; };

    protected:
        virtual unsigned int getSamplerate(void) { return 0; };
        virtual int getBuffersize(void) { return 0; };
        void feedRecord(float* samples_left, float *samples_right);
        bool recordRunning;

    private:
        void *recordThread(void);
        static void *_recordThread(void *arg);
        static void _wavCleanup(void *arg);
        void wavCleanup(void);
        void recordLog(string tag);

        unsigned int  samplerate;
        unsigned int  buffersize;

        string      recordFifo;
        FILE       *toFifo;
        SF_INFO     fromFifoInfo;
        SNDFILE    *fromFifoSndfle;
        sf_count_t  tferSamples;
        float      *tferBuf;

        string   wavFile;
        SF_INFO  wavOutInfo;
        SNDFILE *wavOutsnd;

        float        *interleavedFloats;
        bool          float32bit;
        record_state  recordState;
        bool          runRecordThread;
        pthread_t     pThread;
};

#endif
