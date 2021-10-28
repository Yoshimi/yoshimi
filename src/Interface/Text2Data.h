/*
    Text2Data.h - conversion of text to commandBlock entries

    Copyright 2021, Will Godfrey

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

#ifndef TEXTDATA_H
#define TEXTDATA_H

#include <string>

#include "globals.h"

class SynthEngine;
class TextMsgBuffer;

class TextData
{
    public:
        void encodeAll(SynthEngine *_synth, std::string _source, CommandBlock &allData);

    private:
        SynthEngine *oursynth;
        void log(std::string &line, std::string text);
        void strip(std::string &line);
        void nextWord(std::string &line);
        void encodeMain(std::string source, CommandBlock &allData);
        void encodePart(std::string source, CommandBlock &allData);
        void encodeEffects(std::string source, CommandBlock &allData);
};
#endif // TEXTDATA_H
