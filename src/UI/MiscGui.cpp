/*
    MiscGui.cpp - common link between GUI and synth

    Copyright 2016 Will Godfrey

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

#include <FL/x.H>
#include <iostream>
#include "Misc/SynthEngine.h"
#include "MiscGui.h"

SynthEngine *synth;

void collect_data(SynthEngine *synth, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kititem, unsigned char engine, unsigned char insert, unsigned char parameter)
{
#ifdef ENABLE_REPORTS
    // cout << "Type " << type & 0x20 << endl;
    if ((type & 3) == 3)
    { // value type is now irrelevant
        if(Fl::event_state(FL_CTRL) != 0)
            type = 3;
            // identifying this for button 3 as MIDI learn
        else
            type = 0;
            // identifying this for button 3 as set default
    }
    else if((type & 7) > 2)
        type = 1 | (type & (1 << 7));
        // change scroll wheel to button 1

    // 0x20 = from GUI
    synth->commandFetch(value, type | 0x20, control, part, kititem, engine, insert, parameter);
#endif
}
