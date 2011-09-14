/*
    MusicClient.h

    Copyright 2009-2010 Alan Calvert
    Copyright 2009 James Morris

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

#ifndef MUSIC_CLIENT_H
#define MUSIC_CLIENT_H

#include <string>

using namespace std;

#include "MusicIO/Midi.h"
#include "MusicIO/WavRecord.h"

class MusicClient
{
    public:
        MusicClient():wavrecord(new WavRecord()) { }
        ~MusicClient() { }
        bool Open(void) { return openAudio(wavrecord) && openMidi(wavrecord); }
        virtual bool Start(void) {
            return wavrecord->Start(
                       getSamplerate(), getBuffersize());
        }
        virtual void Close(void) = 0;
        virtual void queueProgramChange(unsigned char chan,
                                        unsigned short banknum,
                                        unsigned char prog,
                                        uint32_t eventframe) = 0;
        virtual void queueMidi(midimessage *msg) = 0;
        virtual bool jacksessionReply(string cmdline) { return false; }
        virtual unsigned int getSamplerate(void) = 0;
        virtual int getBuffersize(void)      = 0;
        virtual string audioClientName(void) = 0;
        virtual string midiClientName(void)  = 0;
        virtual int audioClientId(void)      = 0;
        virtual int midiClientId(void) = 0;
        virtual int audioLatency(void) = 0;
        virtual int midiLatency(void)  = 0;
        static MusicClient *newMusicClient(void);

        bool recordTrigger(void) { return wavrecord->Trigger(); }
        void startRecord(void) { wavrecord->StartRecord(); }
        void stopRecord(void) { wavrecord->StopRecord(); }
        bool setRecordFile(const char *fpath,
                           string &errmsg) {
            return wavrecord->SetFile(string(
                                          fpath),
                                      errmsg);
        }
        bool setRecordOverwrite(string &errmsg) {
            return wavrecord->
                   SetOverwrite(errmsg);
        }
        string wavFilename(void) { return wavrecord->Filename(); }
        WavRecord *wavrecord;

    protected:
        virtual bool openAudio(WavRecord *recorder) = 0;
        virtual bool openMidi(WavRecord *recorder)  = 0;
};

extern MusicClient *musicClient;

#endif
