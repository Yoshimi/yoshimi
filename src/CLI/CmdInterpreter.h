/*
    CmdInterpreter.h

    Copyright 2019, Will Godfrey.

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

#ifndef CMDINTERPRETER_H
#define CMDINTERPRETER_H



#include "Misc/SynthEngine.h"
#include "Interface/TextLists.h"

class CmdInterpreter
{
    public:
        int contextToEngines(int context);
        bool query(std::string text, bool priority);

        float readControl(SynthEngine *synth, unsigned char action, unsigned char control, unsigned char part, unsigned char kit = UNUSED, unsigned char engine = UNUSED, unsigned char insert = UNUSED, unsigned char parameter = UNUSED, unsigned char offset = UNUSED, unsigned char miscmsg = NO_MSG);
        std::string readControlText(SynthEngine *synth, unsigned char action, unsigned char control, unsigned char part, unsigned char kit = UNUSED, unsigned char engine = UNUSED, unsigned char insert = UNUSED, unsigned char parameter = UNUSED, unsigned char offset = UNUSED);
        void readLimits(SynthEngine *synth, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char miscmsg);
        int sendNormal(SynthEngine *synth, unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = UNUSED, unsigned char engine = UNUSED, unsigned char insert = UNUSED, unsigned char parameter = UNUSED, unsigned char offset = UNUSED, unsigned char miscmsg = NO_MSG);
        int sendDirect(SynthEngine *synth, unsigned char action, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit = UNUSED, unsigned char engine = UNUSED, unsigned char insert = UNUSED, unsigned char parameter = UNUSED, unsigned char offset = UNUSED, unsigned char miscmsg = NO_MSG, unsigned char request = UNUSED);

        std::string findStatus(SynthEngine *synth, int context, bool show);

        // all these are heavily used by the above function
        int filterVowelNumber;
        int filterFormantNumber;
        int insertType;
        int voiceNumber;
        int voiceFromNumber;
        int modulatorFromNumber;
        int modulatorFromVoiceNumber;
        int kitMode;
        int kitNumber;
        bool inKitEditor;
        int npart;

        int nFX;
        int nFXtype;
        int nFXpreset;
        int nFXeqBand;

        int chan;
        int axis;
        int mline;
};

#endif /*CMDINTERPRETER_H*/
