/*
    MusicIO.h

    Copyright 2009, Alan Calvert
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

#include <jack/jack.h>

typedef jack_default_audio_sample_t jsample_t;

#include "Misc/Master.h"

class MusicIO
{
    public:
        MusicIO();
        virtual ~MusicIO() { };

        void getAudio(void);
        void getAudioInterleaved(void);
        void silenceBuffers(void);

        virtual unsigned int getSamplerate(void) { return 0; };
        virtual int getBuffersize(void) { return 0; };

        void setMidiController(unsigned char ch, unsigned int ctrl, int param);
        void setMidiNote(unsigned char chan, unsigned char note);
        void setMidiNote(unsigned char chan, unsigned char note,
                         unsigned char velocity);
        int getMidiController(unsigned char b);

    protected:
        bool prepAudiobuffers(bool with_interleaved);

        jsample_t   *zynLeft;
        jsample_t   *zynRight;
        short int   *interleavedShorts;
};



#endif
