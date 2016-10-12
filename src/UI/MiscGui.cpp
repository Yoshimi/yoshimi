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
#include "MasterUI.h"

SynthEngine *synth;

void collect_data(SynthEngine *synth, float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kititem, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2)
{
#ifdef ENABLE_REPORTS
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

    CommandBlock putData;
    size_t commandSize = sizeof(putData);
    putData.data.value = value;
    putData.data.type = type | 0x20; // 0x20 = from GUI
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kititem;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;
    if (jack_ringbuffer_write_space(synth->interchange.fromGUI) >= commandSize)
        jack_ringbuffer_write(synth->interchange.fromGUI, (char*) putData.bytes, commandSize);

#endif
}


void read_updates(SynthEngine *synth)
{
    CommandBlock getData;
    size_t commandSize = sizeof(getData);

    while(jack_ringbuffer_read_space(synth->interchange.toGUI) >= commandSize)
    {
        int toread = commandSize;
        char *point = (char*) &getData.bytes;
        for (size_t i = 0; i < commandSize; ++i)
        {
            jack_ringbuffer_read(synth->interchange.toGUI, point, toread);
        }
        decode_updates(synth, &getData);
    }
}


void decode_updates(SynthEngine *synth, CommandBlock *getData)
{
    unsigned char npart = getData->data.part;
    unsigned char kititem = getData->data.kit;
    unsigned char engine = getData->data.engine;
    //unsigned char insert = getData->data.insert;
    //unsigned char insertParam = getData->data.parameter;
    //unsigned char insertPar2 = getData->data.par2;

    if (npart >= 0xc0 && npart < 0xd0)
    {
        return; // todo
    }
    if (npart == 0xf0)
    {
        synth->getGuiMaster()->returns_update(getData);
        return;
    }
    if ((npart == 0xf1 || npart == 0xf2) && kititem == 0xff)
    {
        return; // todo
    }
    if (kititem == 0xff || (kititem & 0x20))
    {
        synth->getGuiMaster()->partui->returns_update(getData);
        return;
    }
    if (kititem >= 0x80)
    {
        return; // todo
    }

    if (engine == 2 && synth->getGuiMaster()->partui->padnoteui)
    {
        synth->getGuiMaster()->partui->padnoteui->returns_update(getData);
        return;
    }
    if (engine == 1 && synth->getGuiMaster()->partui->subnoteui)
    {
        synth->getGuiMaster()->partui->subnoteui->returns_update(getData);
        return;
    }
    if (engine >= 0x80)
    {
        return; // todo
    }
    if (engine == 0 && synth->getGuiMaster()->partui->adnoteui)
    {
        synth->getGuiMaster()->partui->adnoteui->returns_update(getData);
        return;
    }

}
