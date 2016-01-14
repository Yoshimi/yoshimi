/*
    MusicIO.h

    Copyright 2009-2011, Alan Calvert
    Copyright 2009, James Morris
    Copyright 2014-2015, Will Godfrey & others

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

#ifndef MUSIC_IO_H
#define MUSIC_IO_H

#include "Misc/SynthEngine.h"

class SynthEngine;

class MusicIO : virtual protected MiscFuncs
{
    public:
        MusicIO(SynthEngine *_synth);
        virtual ~MusicIO();
        virtual unsigned int getSamplerate(void) = 0;
        virtual int getBuffersize(void) = 0;
        virtual bool Start(void) = 0;
        virtual void Close(void) = 0;
        virtual bool openAudio(void) = 0;
        virtual bool openMidi(void) = 0;
        virtual string audioClientName(void) = 0;
        virtual int audioClientId(void) = 0;
        virtual string midiClientName(void) = 0;
        virtual int midiClientId(void) = 0;
        virtual void registerAudioPort(int) = 0;

    protected:
        bool prepBuffers(void);
        void getAudio(void) { if (synth) synth->MasterAudio(zynLeft, zynRight); }
        int getMidiController(unsigned char b);
        void setMidiController(unsigned char ch, int ctrl, int param, bool in_place = false);
        bool nrpnRunVector(unsigned char ch, int ctrl, int param);
        void nrpnProcessData(unsigned char chan, int type, int par);
        void nrpnDirectPart(int dHigh, int par);
        void nrpnSetVector(int dHigh, unsigned char chan,  int par);

        //if setBank is false then set RootDir number else current bank number
        void setMidiBankOrRootDir(unsigned int bank_or_root_num, bool in_place = false, bool setRootDir = false);
        void setMidiProgram(unsigned char ch, int prg, bool in_place = false);
        void setMidiNote(unsigned char chan, unsigned char note);
        void setMidiNote(unsigned char chan, unsigned char note, unsigned char velocity);

        float *zynLeft [NUM_MIDI_PARTS + 1];
        float *zynRight [NUM_MIDI_PARTS + 1];
        int *interleaved;

        SynthEngine *synth;
};

#endif
