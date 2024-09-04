/*
    Text2Data.h - conversion of text to commandBlock entries

    Copyright 2021, Will Godfrey
    Copyright 2024, Kristian Amlie

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
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

#ifndef TEXTDATA_H
#define TEXTDATA_H

#include <string>

#include "globals.h"

using std::string;

class SynthEngine;
class TextMsgBuffer;

class TextData
{
    public:
        void encodeAll(SynthEngine*, string& sendCommand, CommandBlock&);

    private:
        SynthEngine* oursynth;

        void log(string& line, string text);
        void strip(string& line);
        void nextWord(string& line);
        bool findCharNum(string& line, uchar& value);
        bool findAndStep(string& line, string text, bool step = true);
        int findListEntry(string& line, int step, const string list []);
        int mapToEffectNumber(int textIndex, const int list []);
        int findEffectFromText(string& line, int step, const string list [], const int listmap []);
        void encodeLoop(string source, CommandBlock&);

        void encodeMain (string& source, CommandBlock&);
        void encodeScale(string& source, CommandBlock&);
        void encodePart (string& source, CommandBlock&);

        void encodeController(string& source, CommandBlock&);
        void encodeMidi     (string& source, CommandBlock&);
        void encodeEffects  (string& source, CommandBlock&);

        void encodeAddSynth (string& source, CommandBlock&);
        void encodeAddVoice (string& source, CommandBlock&);
        void encodeSubSynth (string& source, CommandBlock&);
        void encodePadSynth (string& source, CommandBlock&);

        void encodeWaveform (string& source, CommandBlock&);
        void encodeResonance(string& source, CommandBlock&);

        void encodeLFO      (string& source, CommandBlock&);
        void encodeEnvelope (string& source, CommandBlock&);
        void encodeFilter   (string& source, CommandBlock&);
};
#endif /*TEXTDATA_H*/
