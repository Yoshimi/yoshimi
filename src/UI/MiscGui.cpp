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
    int typetop = type & 0xc0;
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
        type = 1 | typetop;
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
    unsigned char insert = getData->data.insert;
    unsigned char insertParam = getData->data.parameter;
    //unsigned char insertPar2 = getData->data.par2;

    if (npart >= 0xc0 && npart < 0xd0) // vector
    {
        return; // todo
    }
    if (npart >= 0xf0) // main / sys / ins
    {
        synth->getGuiMaster()->returns_update(getData);
        return;
    }
    if (kititem == 0xff || (kititem & 0x20)) // part
    {
        synth->getGuiMaster()->partui->returns_update(getData);
        return;
    }
    if (kititem >= 0x80) // effects
    {
        return; // todo
    }

    if (engine == 2) // padsynth
    {
        if(synth->getGuiMaster()->partui->padnoteui)
        {
            switch (insert)
            {
                case 0xff:
                    synth->getGuiMaster()->partui->padnoteui->returns_update(getData);
                    break;
                case 0:
                    switch(insertParam)
                    {
                        case 0:
                            if (synth->getGuiMaster()->partui->padnoteui->amplfo)
                                synth->getGuiMaster()->partui->padnoteui->amplfo->returns_update(getData);
                            break;
                        case 1:
                            if (synth->getGuiMaster()->partui->padnoteui->freqlfo)
                                synth->getGuiMaster()->partui->padnoteui->freqlfo->returns_update(getData);
                            break;
                        case 2:
                            if (synth->getGuiMaster()->partui->padnoteui->filterlfo)
                                synth->getGuiMaster()->partui->padnoteui->filterlfo->returns_update(getData);
                            break;
                    }
                    break;
                case 5:
                case 6:
                case 7:
                    if(synth->getGuiMaster()->partui->padnoteui->oscui)
                        synth->getGuiMaster()->partui->padnoteui->oscui->returns_update(getData);
                    break;
                case 8:
                case 9:
                    if(synth->getGuiMaster()->partui->padnoteui->resui)
                        synth->getGuiMaster()->partui->padnoteui->resui->returns_update(getData);
                    break;
            }
        }
        return;
    }

    if (engine == 1) // subsynth
    {
        if (synth->getGuiMaster()->partui->subnoteui)
            switch (insert)
            {
                case 0xff:
                case 6:
                case 7:
                    synth->getGuiMaster()->partui->subnoteui->returns_update(getData);
                    break;
            }
        return;
    }

    if (engine >= 0x80) // addsynth voice / modulator
    {
        if (synth->getGuiMaster()->partui->adnoteui)
        {
            if (synth->getGuiMaster()->partui->adnoteui->advoice)
            {
                switch (insert)
                {
                    case 0xff:
                        synth->getGuiMaster()->partui->adnoteui->advoice->returns_update(getData);
                        break;
                    case 0:
                        switch(insertParam)
                        {
                            case 0:
                                if (synth->getGuiMaster()->partui->adnoteui->advoice->voiceamplfogroup)
                                synth->getGuiMaster()->partui->adnoteui->advoice->voiceamplfogroup->returns_update(getData);
                                break;
                            case 1:
                                if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqlfogroup)
                                synth->getGuiMaster()->partui->adnoteui->advoice->voicefreqlfogroup->returns_update(getData);
                                break;
                            case 2:
                                if (synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterlfogroup)
                                synth->getGuiMaster()->partui->adnoteui->advoice->voicefilterlfogroup->returns_update(getData);
                                break;
                        }
                        break;
                    case 5:
                    case 6:
                    case 7:
                        if (synth->getGuiMaster()->partui->adnoteui->advoice->oscedit)
                            synth->getGuiMaster()->partui->adnoteui->advoice->oscedit->returns_update(getData);
                        break;
                }
            }
        }
        return;
    }

    if (engine == 0) // addsynth base
    {
        if (synth->getGuiMaster()->partui->adnoteui)
            switch (insert)
            {
                case 0xff:
                    synth->getGuiMaster()->partui->adnoteui->returns_update(getData);
                    break;
                case 0:
                    switch(insertParam)
                    {
                        case 0:
                            if (synth->getGuiMaster()->partui->adnoteui->amplfo)
                                synth->getGuiMaster()->partui->adnoteui->amplfo->returns_update(getData);
                            break;
                        case 1:
                            if (synth->getGuiMaster()->partui->adnoteui->freqlfo)
                                synth->getGuiMaster()->partui->adnoteui->freqlfo->returns_update(getData);
                            break;
                        case 2:
                            if (synth->getGuiMaster()->partui->adnoteui->filterlfo)
                                synth->getGuiMaster()->partui->adnoteui->filterlfo->returns_update(getData);
                            break;
                    }
                    break;
                case 8:
                case 9:
                    if (synth->getGuiMaster()->partui->adnoteui->resui)
                        synth->getGuiMaster()->partui->adnoteui->resui->returns_update(getData);
                    break;
            }
        return;
    }
}
