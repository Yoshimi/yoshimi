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
        std::string buildStatus(SynthEngine *synth, int context, bool show);
        std::string buildAllFXStatus(SynthEngine *synth, int context);
        std::string buildPartStatus(SynthEngine *synth, int context, bool showPartDetails);

        /* == state fields == */  // all these are used by findStatus()

        // the following are used pervasively
        int npart;
        int kitMode;
        int kitNumber;
        bool inKitEditor;
        int voiceNumber;
        int insertType;
        int nFXtype;

        // the remaining ones are only used at some places
        int nFXpreset;
        int nFXeqBand;
        int nFX;

        int filterVowelNumber;
        int filterFormantNumber;

        int chan;
        int axis;
        int mline;
};

#endif /*CMDINTERPRETER_H*/
