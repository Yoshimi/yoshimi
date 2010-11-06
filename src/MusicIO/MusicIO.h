/*
    MusicIO.h

    Copyright 2009-2010, Alan Calvert
    Copyright 2009, James Morris

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

#ifndef MUSIC_IO_H
#define MUSIC_IO_H

#include <inttypes.h>
#include <string>
#include <pthread.h>
#include <jack/ringbuffer.h>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

#include "Misc/MiscFuncs.h"
//#include "MusicIO/Midi.h"
#include "MusicIO/WavRecord.h"

class MusicIO : protected MiscFuncs
{
    public:
        MusicIO();
        ~MusicIO();
        virtual bool openAudio(WavRecord *recorder) = 0;
        virtual bool openMidi(WavRecord *recorder) = 0;
        virtual bool Start(void);
        virtual void Close(void);
        virtual unsigned int getSamplerate(void) = 0;
        virtual int getBuffersize(void) = 0;
        virtual bool jacksessionReply(string cmdline) { return false; }
        string audioClientName(void) { return audioclientname; }
        string midiClientName(void) { return midiclientname; }
        int audioClientId(void) { return audioclientid; }
        int midiClientId(void) { return midiclientid; }
        int audioLatency(void) { return audiolatency; }
        int midiLatency(void) { return midilatency; }
        void queueMidi(midimessage *msg);
        void queueControlChange(unsigned char controltype, unsigned char chan,
                                unsigned char val, uint32_t eventframe);
        void queueProgramChange(unsigned char chan, unsigned short banknum,
                                unsigned char prog, uint32_t eventframe);
        void midiBankChange(unsigned char chan, unsigned short bank);

    protected:
        bool prepAudio(bool with_interleaved);
        void getAudio(void);
        void interleaveShorts(void);
        static void *_midiThread(void *arg);
        void *midiThread(void);

        string baseclientname;
        string audioclientname;
        string midiclientname;
        int audioclientid;
        int midiclientid;
        int audiolatency;
        int midilatency;
        float *zynLeft;
        float *zynRight;
        short int *interleavedShorts;
        uint32_t periodstartframe;
        uint32_t periodendframe;
        WavRecord *wavRecorder;

    private:
        void processControlChange(unsigned char byte0,
                                   unsigned char byte1,
                                   unsigned char byte2);
        jack_ringbuffer_t *midiRingbuf;
        boost::interprocess::interprocess_semaphore *midiEventsUp;
        pthread_t  midiPthread;

};

#endif
