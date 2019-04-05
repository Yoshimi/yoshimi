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

#include <pthread.h>

#include "Misc/Master.h"
#include "MusicIO/WavRecord.h"

class MusicIO
{
    public:
        MusicIO();
        ~MusicIO();

        virtual unsigned int getSamplerate(void) = 0;
        virtual int getBuffersize(void) = 0;
        virtual bool Start(void) = 0;
        virtual void Close(void);
        int grossLatency(void) { return audioLatency + midiLatency; };

    protected:
        bool prepBuffers(bool with_interleaved);
        bool prepRecord(void);
        void getAudio(void);
        void InterleaveShorts(void);

        bool setThreadAttributes(pthread_attr_t *attr, bool schedfifo, bool midi = false);

        int getMidiController(unsigned char b);
        void setMidiController(unsigned char ch, unsigned int ctrl, int param);
        void setMidiNote(unsigned char chan, unsigned char note);
        void setMidiNote(unsigned char chan, unsigned char note, unsigned char velocity);
        jsample_t *zynLeft;
        jsample_t *zynRight;
        short int *interleavedShorts;

        WavRecord *wavRecorder;
        int rtprio;
        jack_nframes_t audioLatency; // frames
        jack_nframes_t midiLatency;  // ""
};

#endif
