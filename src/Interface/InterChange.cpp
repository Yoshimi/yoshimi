/*
    InterChange.h - General communications

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

#include <iostream>

using namespace std;

#include "Interface/InterChange.h"
#include "Misc/MiscFuncs.h"
#include "Misc/SynthEngine.h"
#include "Params/Controller.h"
#include "Params/SUBnoteParameters.h"

InterChange::InterChange(SynthEngine *_synth) :
    synth(_synth)
{
    // quite incomplete - we don't know what will develop yet!
    if (!(sendbuf = jack_ringbuffer_create(sizeof(commandSize) * 1024)))
        synth->getRuntime().Log("InteChange failed to create send ringbuffer");
}


InterChange::~InterChange()
{
    if (sendbuf)
        jack_ringbuffer_free(sendbuf);
}


void InterChange::commandFetch(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kititem, unsigned char engine, unsigned char insert, unsigned char insertParam)
{
    CommandBlock putData;
    putData.data.value = value;
    putData.data.type = type;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kititem;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = insertParam;
    if (jack_ringbuffer_write_space(sendbuf) >= commandSize)
        jack_ringbuffer_write(sendbuf, (char*) putData.bytes, commandSize);
    return;
}


void InterChange::mediate()
{
    CommandBlock getData;
    int toread = commandSize;
    char *point = (char*) &getData.bytes;
    for (size_t i = 0; i < commandSize; ++i)
        jack_ringbuffer_read(sendbuf, point, toread);

    commandSend(getData.data.value, getData.data.type, getData.data.control, getData.data.part, getData.data.kit, getData.data.engine, getData.data.insert, getData.data.parameter);
}


void InterChange::commandSend(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem, unsigned char engine, unsigned char insert, unsigned char insertParam)
{
    bool isGui = type & 0x20;
    char button = type & 0x1f;
    string isf;
    if (isGui && button != 2)
    {
        if (!(type &0x80))
            isf = "f";
        synth->getRuntime().Log("\n  Value " + asString(value) + isf
                            + "\n  Button " + asString((int) type & 7)
                            + "\n  Control " + asString((int) control)
                            + "\n  Part " + asString((int) npart)
                            + "\n  Kit " + asString((int) kititem)
                            + "\n  Engine " + asString((int) engine)
                            + "\n  Insert " + asString((int) insert)
                            + "\n  Parameter " + asString((int) insertParam));
        return;
    }
    if (npart == 0xc0)
        commandVector(value, type, control);
    else if (npart == 0xf0)
        commandMain(value, type, control);
    else if ((npart == 0xf1 || npart == 0xf2) && kititem == 0xff)
        commandSysIns(value, type, control, npart, engine, insert);
    else if (kititem == 0xff || (kititem & 0x20))
        commandPart(value, type, control, npart, kititem, engine);
    else if (kititem >= 0x80)
    {
        if (insert < 0xff)
            commandFilter(value, type, control, npart, kititem, engine, insert);
        else
            commandEffects(value, type, control, npart, kititem, engine);
    }
    else if (engine == 2)
    {
        switch(insert)
        {
            case 0xff:
                commandPad(value, type, control, npart, kititem);
                break;
            case 0:
                commandLFO(value, type, control, npart, kititem, engine, insert, insertParam);
                break;
            case 1:
                commandFilter(value, type, control, npart, kititem, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, npart, kititem, engine, insert, insertParam);
                break;
            case 5:
            case 6:
            case 7:
                commandOscillator(value, type, control, npart, kititem, engine, insert);
                break;
            case 8:
            case 9:
                commandResonance(value, type, control, npart, kititem, engine, insert);
                break;
        }
    }
    else if (engine == 1)
    {
        switch (insert)
        {
            case 0xff:
            case 6:
            case 7:
                commandSub(value, type, control, npart, kititem, insert);
                break;
            case 1:
                commandFilter(value, type, control, npart, kititem, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, npart, kititem, engine, insert, insertParam);
                break;
        }
    }
    else if (engine >= 0x80)
    {
        switch (insert)
        {
            case 0xff:
                commandAddVoice(value, type, control, npart, kititem, engine);
                break;
            case 0:
                commandLFO(value, type, control, npart, kititem, engine, insert, insertParam);
                break;
            case 1:
                commandFilter(value, type, control, npart, kititem, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, npart, kititem, engine, insert, insertParam);
                break;
            case 5:
            case 6:
            case 7:
                commandOscillator(value, type, control, npart, kititem, engine, insert);
                break;
        }
    }
    else if (engine == 0)
    {
        switch (insert)
        {
            case 0xff:
                commandAdd(value, type, control, npart, kititem);
                break;
            case 0:
                commandLFO(value, type, control, npart, kititem, engine, insert, insertParam);
                break;
            case 1:
                commandFilter(value, type, control, npart, kititem, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, npart, kititem, engine, insert, insertParam);
                break;
            case 8:
                commandResonance(value, type, control, npart, kititem, engine, insert);
                break;
        }
    }
    // just do nothing if not recognised
}


void InterChange::commandVector(float value, unsigned char type, unsigned char control)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Base Channel";
            break;
        case 1:
            contstr = "Options";
            break;

        case 16:
        case 32:
            contstr = "Controller";
            break;
        case 17:
            contstr = "Left Instrument";
            break;
        case 18:
            contstr = "Right Instrument";
            break;
        case 19:
        case 35:
            contstr = "Feature 0";
            break;
        case 20:
        case 36:
            contstr = "Feature 1";
            break;
        case 21:
        case 37:
            contstr = "Feature 2 ";
            break;
        case 22:
        case 38:
            contstr = "Feature 3";
            break;
        case 33:
            contstr = "Up Instrument";
            break;
        case 34:
            contstr = "Down Instrument";
            break;
    }
    string name = "Vector ";
    if (control >= 32)
        name += "Y ";

    else if(control >= 16)
        name += "X ";
    synth->getRuntime().Log(name + contstr + " value " + actual);
}


void InterChange::commandMain(float value, unsigned char type, unsigned char control)
{
    bool write = (type & 0x40) > 0;
    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            if (write)
                synth->setPvolume(value);
            else
                value = synth->Pvolume;
            break;

        case 14:
            contstr = "Part Number";
            if (write)
                synth->getRuntime().currentPart = value;
            else
                value = synth->getRuntime().currentPart;
            break;
        case 15:
            contstr = "Available Parts";
            if ((write) && (value == 16 || value == 32 || value == 64))
                synth->getRuntime().NumAvailableParts = value;
            else
                value = synth->getRuntime().NumAvailableParts;
            break;

        case 32:
            contstr = "Detune";
            if (write)
                synth->microtonal.Pglobalfinedetune = value;
            else
                value = synth->microtonal.Pglobalfinedetune;
            break;
        case 35:
            contstr = "Key Shift";
            if (write)
                synth->setPkeyshift(value + 64);
            else
                value = synth->Pkeyshift - 64;
            break;

        case 96:
            contstr = "Reset All";
            if (write)
                synth->resetAll();
            break;
        case 128:
            contstr = "Stop";
            if (write)
                synth->allStop();
            break;
    }

    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    synth->getRuntime().Log("Main " + contstr + " value " + actual);
}


void InterChange::commandPart(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem, unsigned char engine)
{
    bool write = (type & 0x40) > 0;
    bool kitType = (kititem >= 0x20 && kititem < 0x40);
    Part *part;
    part = synth->part[npart];

    string kitnum;
    if (kitType)
        kitnum = "  Kit " + to_string(kititem & 0x1f) + " ";
    else
        kitnum = "  ";

    string name;
    if (control >= 0x80)
    {
        if (control >= 0xe0)
            name = "";
        else
        {
            name = "Controller ";
            if (control >= 0xa0)
                name += "Portamento ";
        }
    }
    else
    {
        switch (engine)
        {
            case 0:
                name = "AddSynth ";
                break;
            case 1:
                name = "SubSynth ";
                break;
            case 2:
                name = "PadSynth ";
                break;
        }
    }

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            if (write)
                part->setVolume(value);
            else
                value = part->Pvolume;
            break;
        case 1:
            contstr = "Vel Sens";
            if (write)
                part->Pvelsns = value;
            else
                value = part->Pvelsns;
            break;
        case 2:
            contstr = "Panning";
            if (write)
                part->SetController(C_panning, value);
            else
                value = part->Ppanning;
            break;
        case 4:
            contstr = "Vel Offset";
            if (write)
                part->Pveloffs = value;
            else
                value = part->Pveloffs;
            break;
        case 5:
            contstr = "Midi";
            if (write)
                part->Prcvchn = (char) value;
            else
                value = part->Prcvchn;
            break;
        case 6:
            contstr = "Mode";
            if (write)
                synth->SetPartKeyMode(npart, (char) value);
            else
                value = synth->ReadPartKeyMode(npart);
            break;
        case 7:
            contstr = "Portamento";
            if (write)
                part->ctl->portamento.portamento = (char) value;
            else
                value = part->ctl->portamento.portamento;
            break;
        case 8:
            contstr = "Enable";
            if (kitType)
            {
                switch(engine)
                {
                    case 0:
                        if (write)
                            part->kit[kititem & 0x1f].Padenabled = (char) value;
                        else
                            value = part->kit[kititem & 0x1f].Padenabled;
                        break;
                    case 1:
                        if (write)
                            part->kit[kititem & 0x1f].Psubenabled = (char) value;
                        else
                            value = part->kit[kititem & 0x1f].Psubenabled;
                        break;
                    case 2:
                        if (write)
                            part->kit[kititem & 0x1f].Ppadenabled = (char) value;
                        else
                            value = part->kit[kititem & 0x1f].Ppadenabled;
                        break;
                    default:
                        if (write)
                            part->setkititemstatus(kititem & 0x1f, (char) value);
                        else
                            value = synth->partonoffRead(npart);
                        break;
                }
            }
            else
            {
                 switch(engine)
                {
                    case 0:
                        if (write)
                            part->kit[0].Padenabled = (char) value;
                        else
                            value = part->kit[0].Padenabled;
                        break;
                    case 1:
                        if (write)
                            part->kit[0].Psubenabled = (char) value;
                        else
                            value = part->kit[0].Psubenabled;
                        break;
                    case 2:
                        if (write)
                            part->kit[0].Ppadenabled = (char) value;
                        else
                            value = part->kit[0].Ppadenabled;
                        break;
                        break;
                    default:
                        if (write)
                            synth->partonoffLock(npart, (char) value);
                        else
                            value = synth->partonoffRead(npart);
                }
            }
            break;
        case 9:
            if (kitType)
            {
                contstr = "Mute";
                if (write)
                    part->kit[kititem & 0x1f].Pmuted = (char) value;
                else
                    value = part->kit[kititem & 0x1f].Pmuted;
            }
            break;

        case 16:
            contstr = "Min Note";
            if (kitType)
            {
                if (write)
                    part->kit[kititem & 0x1f].Pminkey = (char) value;
                else
                    value = part->kit[kititem & 0x1f].Pminkey;
            }
            else
            {
                if (write)
                    part->Pminkey = (char) value;
                else
                    value = part->Pminkey;
            }
            break;
        case 17:
            contstr = "Max Note";
            if (kitType)
            {
                if (write)
                    part->kit[kititem & 0x1f].Pmaxkey = (char) value;
                else
                    value = part->kit[kititem & 0x1f].Pmaxkey;
            }
            else
            {
                if (write)
                    part->Pmaxkey = (char) value;
                else
                    value = part->Pmaxkey;
            }
            break;
        case 18:
            contstr = "Min To Last";
            if (kitType)
            {
                if ((write) && part->lastnote >= 0)
                    part->kit[kititem & 0x1f].Pminkey = part->lastnote;
                else
                    value = part->kit[kititem & 0x1f].Pminkey;
            }
            else
            {
                if ((write) && part->lastnote >= 0)
                    part->Pminkey = part->lastnote;
                else
                    value = part->Pminkey;
            }
            break;
        case 19:
            contstr = "Max To Last";
            if (kitType)
            {
                if ((write) && part->lastnote >= 0)
                    part->kit[kititem & 0x1f].Pmaxkey = part->lastnote;
                else
                    value = part->kit[kititem & 0x1f].Pmaxkey;
            }
            else
            {
                if ((write) && part->lastnote >= 0)
                    part->Pmaxkey = part->lastnote;
                else
                    value = part->Pmaxkey;
            }
            break;
        case 20:
            contstr = "Reset Key Range";
            if (kitType)
            {
                if (write)
                {
                    part->kit[kititem & 0x1f].Pminkey = 0;
                    part->kit[kititem & 0x1f].Pmaxkey = 127;
                }
            }
            else
            {
                if (write)
                {
                    part->Pminkey = 0;
                    part->Pmaxkey = 127;
                }
            }
            break;

        case 24:
            if (kitType)
            {
                contstr = "Effect Number";
                if (write)
                    part->kit[kititem & 0x1f].Psendtoparteffect = (char) value;
                else
                    value = part->kit[kititem & 0x1f].Psendtoparteffect;
            }
            break;

        case 33:
            contstr = "Key Limit";
            if (write)
                part->setkeylimit((char) value);
            else
                value = part->Pkeylimit;
            break;
        case 35:
            contstr = "Key Shift";
            if (write)
                part->Pkeyshift = (char) value + 64;
            else
                value = part->Pkeyshift - 64;
            break;
            break;

        case 40:
            contstr = "Effect Send 0";
            if (write)
                synth->setPsysefxvol(npart,0, value);
            else
                value = synth->Psysefxvol[0][npart];
            break;
        case 41:
            contstr = "Effect Send 1";
            if (write)
                synth->setPsysefxvol(npart,1, value);
            else
                value = synth->Psysefxvol[1][npart];
            break;
        case 42:
            contstr = "Effect Send 2";
            if (write)
                synth->setPsysefxvol(npart,2, value);
            else
                value = synth->Psysefxvol[2][npart];
            break;
        case 43:
            contstr = "Effect Send 3";
            if (write)
                synth->setPsysefxvol(npart,3, value);
            else
                value = synth->Psysefxvol[3][npart];
            break;

        case 48:
            contstr = "Humanise";
            if (write)
                part->Pfrand = value;
            else
                value = part->Pfrand;
            break;

        case 57:
            contstr = "Drum Mode";
            if (write)
                part->Pdrummode = (char) value;
            else
                value = part->Pdrummode;
            break;
        case 58:
            contstr = "Kit Mode";
            if (write)
                part->Pkitmode = (char) value;
            else
                value = part->Pkitmode;
            break;

        case 120:
            contstr = "Audio destination";
            if (write)
                part->Paudiodest = (char) value;
            else
                value = part->Paudiodest;
            break;

        case 128:
            contstr = "Vol Range";
            if (write)
                part->ctl->setvolume((char) value); // not the *actual* volume
            else
                value = part->ctl->volume.data;
            break;
        case 129:
            contstr = "Vol Enable";
            if (write)
                part->ctl->volume.receive = (char) value;
            else
                value = part->ctl->volume.receive;
            break;
        case 130:
            contstr = "Pan Width";
            if (write)
                part->ctl->setPanDepth((char) value);
            else
                value = part->ctl->panning.depth;
            break;
        case 131:
            contstr = "Mod Wheel Depth";
            if (write)
                part->ctl->modwheel.depth = value;
            else
                value = part->ctl->modwheel.depth;
            break;
        case 132:
            contstr = "Exp Mod Wheel";
            if (write)
                part->ctl->modwheel.exponential = (char) value;
            else
                value = part->ctl->modwheel.exponential;
            break;
        case 133:
            contstr = "Bandwidth depth";
            if (write)
                part->ctl->bandwidth.depth = value;
            else
                value = part->ctl->bandwidth.depth;
            break;
        case 134:
            contstr = "Exp Bandwidth";
            if (write)
                part->ctl->bandwidth.exponential = (char) value;
            else
                value = part->ctl->bandwidth.exponential;
            break;
        case 135:
            contstr = "Expression Enable";
            if (write)
                part->ctl->expression.receive = (char) value;
            else
                value = part->ctl->expression.receive;
            break;
        case 136:
            contstr = "FM Amp Enable";
            if (write)
                part->ctl->fmamp.receive = (char) value;
            else
                value = part->ctl->fmamp.receive;
            break;
        case 137:
            contstr = "Sustain Ped Enable";
            if (write)
                part->ctl->sustain.receive = (char) value;
            else
                value = part->ctl->sustain.receive;
            break;
        case 138:
            contstr = "Pitch Wheel Range";
            if (write)
                part->ctl->pitchwheel.bendrange = value;
            else
                value = part->ctl->pitchwheel.bendrange;
            break;
        case 139:
            contstr = "Filter Q Depth";
            if (write)
                part->ctl->filterq.depth = value;
            else
                value = part->ctl->filterq.depth;
            break;
        case 140:
            contstr = "Filter Cutoff Depth";
            if (write)
                part->ctl->filtercutoff.depth = value;
            else
                value = part->ctl->filtercutoff.depth;
            break;

        case 144:
            contstr = "Res Cent Freq Depth";
            if (write)
                part->ctl->resonancecenter.depth = value;
            else
                value = part->ctl->resonancecenter.depth;
            break;
        case 145:
            contstr = "Res Band Depth";
            if (write)
                part->ctl->resonancebandwidth.depth = value;
            else
                value = part->ctl->resonancebandwidth.depth;
            break;

        case 160:
            contstr = "Time";
            if (write)
                part->ctl->portamento.time = value;
            else
                value = part->ctl->portamento.time;
            break;
        case 161:
            contstr = "Tme Stretch";
            if (write)
                part->ctl->portamento.updowntimestretch = value;
            else
                value = part->ctl->portamento.updowntimestretch;
            break;
        case 162:
            contstr = "Threshold";
            if (write)
                part->ctl->portamento.pitchthresh = value;
            else
                value = part->ctl->portamento.pitchthresh;
            break;
        case 163:
            contstr = "Threshold Type";
            if (write)
                part->ctl->portamento.pitchthreshtype = (char) value;
            else
                value = part->ctl->portamento.pitchthreshtype;
            break;
        case 164:
            contstr = "Prop Enable";
            if (write)
                part->ctl->portamento.proportional = (char) value;
            else
                value = part->ctl->portamento.proportional;
            break;
        case 165:
            contstr = "Prop Rate";
            if (write)
                part->ctl->portamento.propRate = value;
            else
                value = part->ctl->portamento.propRate;
            break;
        case 166:
            contstr = "Prop depth";
            if (write)
                part->ctl->portamento.propDepth = value;
            else
                value = part->ctl->portamento.propDepth;
            break;
        case 168:
            contstr = "Enable";
            if (write)
                part->ctl->portamento.receive = (char) value;
            else
                value = part->ctl->portamento.receive;
            break;

        case 224:
            contstr = "Clear controllers";
            if (write)
                part->SetController(0x79,0); // C_resetallcontrollers
            break;
    }

    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    synth->getRuntime().Log("Part " + to_string(npart) + kitnum + name + contstr + " value " + actual);
}


void InterChange::commandAdd(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name = "";
    switch (control & 0x70)
    {
        case 0:
            name = " Amplitude ";
            break;
        case 32:
            name = " Frequency ";
            break;
    }

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;
        case 1:
            contstr = "Vel Sens";
            break;
        case 2:
            contstr = "Panning";
            break;

        case 32:
            contstr = "Detune";
            break;
        case 33:
            contstr = "Eq T";
            break;
        case 34:
            contstr = "440Hz";
            break;
        case 35:
            contstr = "Octave";
            break;
        case 36:
            contstr = "Det type";
            break;
        case 37:
            contstr = "Coarse Det";
            break;
        case 39:
            contstr = "Rel B Wdth";
            break;

        case 48:
            contstr = "Par 1";
            break;
        case 49:
            contstr = "Par 2";
            break;
        case 50:
            contstr = "Force H";
            break;
        case 51:
            contstr = "Position";
            break;

        case 112:
            contstr = "Stereo";
            break;
        case 113:
            contstr = "Rnd Grp";
            break;

        case 120:
            contstr = "De Pop";
            break;
        case 121:
            contstr = "Punch Strngth";
            break;
        case 122:
            contstr = "Punch Time";
            break;
        case 123:
            contstr = "Punch Strtch";
            break;
        case 124:
            contstr = "Punch Vel";
            break;
    }

    synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + "  AddSynth " + name + contstr + " value " + actual);
}


void InterChange::commandAddVoice(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem,unsigned char engine)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name = "";
    switch (control & 0xF0)
    {
        case 0:
            name = " Amplitude ";
            break;
        case 16:
            name = " Modulator ";
            break;
        case 32:
            name = " Frequency ";
            break;
        case 48:
            name = " Unison ";
            break;
        case 64:
            name = " Filter ";
            break;
        case 80:
            name = " Modulator Amp ";
            break;
        case 96:
            name = " Modulator Freq ";
            break;
        case 112:
            name = " Modulator Osc ";
            break;
    }

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;
        case 1:
            contstr = "Vel Sens";
            break;
        case 2:
            contstr = "Panning";
            break;
        case 4:
            contstr = "Minus";
            break;
        case 8:
            contstr = "Enable Env";
            break;
        case 9:
            contstr = "Enable LFO";
            break;

        case 16:
            contstr = "Type";
            break;
        case 17:
            contstr = "Extern Mod";
            break;

        case 32:
            contstr = "Detune";
            break;
        case 33:
            contstr = "Eq T";
            break;
        case 34:
            contstr = "440Hz";
            break;
        case 35:
            contstr = "Octave";
            break;
        case 36:
            contstr = "Det type";
            break;
        case 37:
            contstr = "Coarse Det";
            break;
        case 40:
            contstr = "Enable Env";
            break;
        case 41:
            contstr = "Enable LFO";
            break;

        case 48:
            contstr = "Freq Spread";
            break;
        case 49:
            contstr = "Phase Rnd";
            break;
        case 50:
            contstr = "Stereo";
            break;
        case 51:
            contstr = "Vibrato";
            break;
        case 52:
            contstr = "Vib Speed";
            break;
        case 53:
            contstr = "Size";
            break;
        case 54:
            contstr = "Invert";
            break;
        case 56:
            contstr = "Enable";
            break;

        case 64:
            contstr = "Bypass Global";
            break;
        case 68:
            contstr = "Enable";
            break;
        case 72:
            contstr = "Enable Env";
            break;
        case 73:
            contstr = "Enable LFO";
            break;

        case 80:
            contstr = "Volume";
            break;
        case 81:
            contstr = "V Sense";
            break;
        case 82:
            contstr = "F Damp";
            break;
        case 88:
            contstr = "Enable Env";
            break;

        case 96:
            contstr = "Detune";
            break;
        case 99:
            contstr = "Octave";
            break;
        case 100:
            contstr = "Det type";
            break;
        case 101:
            contstr = "Coarse Det";
            break;
        case 104:
            contstr = "Enable Env";
            break;

        case 112:
            contstr = " Phase";
            break;
        case 113:
            contstr = " Source";
            break;

        case 128:
            contstr = " Delay";
            break;
        case 129:
            contstr = " Enable";
            break;
        case 130:
            contstr = " Resonance Enable";
            break;
        case 136:
            contstr = " Osc Phase";
            break;
        case 137:
            contstr = " Osc Source";
            break;
        case 138:
            contstr = " Sound type";
            break;
    }

    synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + "  AddSynth Voice " + to_string(engine & 0x1f) + name + contstr + " value " + actual);
}


void InterChange::commandSub(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem, unsigned char insert)
{
    bool write = (type & 0x40) > 0;
    Part *part;
    part = synth->part[npart];
    SUBnoteParameters *pars;
    pars = part->kit[kititem].subpars;

    if (kititem != 0)
    {
        if (!part->Pkitmode)
        {
            synth->getRuntime().Log("Not in kit mode");
            return;
        }
        else if (!part->kit[kititem].Penabled)
        {
            synth->getRuntime().Log("Kit item " + to_string(kititem) + " not enabled");
            return;
        }
    }

    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    if (insert == 6)
    {
        if (write)
            pars->Phmag[control] = value;
        else
            value = pars->Phmag[control];
        synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + "  Subsynth Harmonic " + to_string(control) + " Amplitude value " + actual);
        return;
    }

    if (insert == 7)
    {
        if (write)
            pars->Phrelbw[control] = value;
        else
            value = pars->Phrelbw[control];
        synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + "  Subsynth Harmonic " + to_string(control) + " Bandwidth value " + actual);
        return;
    }

    string name = "";
    switch (control & 0x70)
    {
        case 0:
            name = " Amplitude ";
            break;
        case 16:
            name = " Bandwidth ";
            break;
        case 32:
            name = " Frequency ";
            break;
        case 48:
            name = " Overtones ";
            break;
        case 64:
            name = " Filter ";
            break;
    }

    string contstr = "";
    int k; // temp variable for detune
    switch (control)
    {
        case 0:
            contstr = "Volume";
            if (write)
                pars->PVolume = value;
            else
                value = pars->PVolume;
            break;
        case 1:
            contstr = "Vel Sens";
            if (write)
                pars->PAmpVelocityScaleFunction = value;
            else
                value = pars->PAmpVelocityScaleFunction;
            break;
        case 2:
            contstr = "Panning";
            if (write)
                pars->setPan(value);
            else
                value = pars->PPanning;
            break;

        case 16:
            contstr = "";
            if (write)
                pars->Pbandwidth = value;
            else
                value = pars->Pbandwidth;
            break;
        case 17:
            contstr = "Band Scale";
            if (write)
                pars->Pbwscale = value + 64;
            else
                value = pars->Pbwscale - 64;
            break;
        case 18:
            contstr = "Env Enab";
            if (write)
                pars->PBandWidthEnvelopeEnabled = (value != 0);
            else
                value = pars->PBandWidthEnvelopeEnabled;
            break;

        case 32:
            contstr = "Detune";
            if (write)
                pars->PDetune = value + 8192;
            else
                value = pars->PDetune - 8192;
            break;
        case 33:
            contstr = "Eq T";
            if (write)
                pars->PfixedfreqET = value;
            else
                value = pars->PfixedfreqET;
            break;
        case 34:
            contstr = "440Hz";
            if (write)
                pars->Pfixedfreq = (value != 0);
            else
                value = pars->Pfixedfreq;
            break;
        case 35:
            contstr = "Octave";
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 16;
                pars->PCoarseDetune = k * 1024 + pars->PCoarseDetune % 1024;
            }
            else
            {
                k = pars->PCoarseDetune / 1024;
                if (k >= 8)
                    k -= 16;
                value = k;
            }
            break;
        case 36:
            contstr = "Det type";
            if (write)
                pars->PDetuneType = (int)value + 1;
            else
                value = pars->PDetuneType;
            break;
        case 37:
            contstr = "Coarse Det";
            if (write)
            {
                k = value;
                if (k < 0)
                    k += 1024;
                pars->PCoarseDetune = k + (pars->PCoarseDetune / 1024) * 1024;
            }
            else
            {
                k = pars->PCoarseDetune % 1024;
                if (k >= 512)
                    k -= 1024;
                value = k;
            }
            break;
        case 38:
            contstr = "Bend";
            if (write)
                pars->PBendAdjust = value;
            else
                value = pars->PBendAdjust;
            break;
        case 39:
            contstr = "Offset";
            if (write)
                pars->PBendAdjust = value;
            else
                value = pars->PBendAdjust;
            break;
        case 40:
            contstr = "Env Enab";
            if (write)
                pars->PFreqEnvelopeEnabled = (value != 0);
            else
                value = pars->PFreqEnvelopeEnabled;
            break;

        case 48:
            contstr = "Par 1";
            if (write)
                pars->POvertoneSpread.par1 = value;
            else
                value = pars->POvertoneSpread.par1;
            break;
        case 49:
            contstr = "Par 2";
            if (write)
                pars->POvertoneSpread.par2 = value;
            else
                value = pars->POvertoneSpread.par2;
            break;
        case 50:
            contstr = "Force H";
            if (write)
                pars->POvertoneSpread.par3 = value;
            else
                value = pars->POvertoneSpread.par3;
            break;
        case 51:
            contstr = "Position";
            if (write)
                pars->POvertoneSpread.type =  (int)value;
            else
                value = pars->POvertoneSpread.type;
            break;

        case 64:
            contstr = "Enable";
            if (write)
                pars->PGlobalFilterEnabled = (value != 0);
            else
                value = pars->PGlobalFilterEnabled;
            break;

        case 80:
            contstr = "Filt Stages";
            if (write)
                pars->Pnumstages = (int)value;
            else
                value = pars->Pnumstages;
            break;
        case 81:
            contstr = "Mag Type";
            if (write)
                pars->Phmagtype = (int)value;
            break;
        case 82:
            contstr = "Start";
            if (write)
                pars->Pstart = (int)value;
            else
                value = pars->Pstart;
            break;

        case 96:
            contstr = "Clear Harmonics";
            if (write)
            {
                for (int i = 0; i < MAX_SUB_HARMONICS; i++)
                {
                    pars->Phmag[i] = 0;
                    pars->Phrelbw[i] = 64;
                }
                pars->Phmag[0] = 127;
            }
            break;

        case 112:
            contstr = "Stereo";
            if (write)
                pars->Pstereo = (value != 0);
            break;
    }

    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + "  SubSynth " + name + contstr + " value " + actual);
}


void InterChange::commandPad(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name = "";
    switch (control & 0x70)
    {
        case 0:
            name = " Amplitude ";
            break;
        case 16:
            name = " Bandwidth ";
            break;
        case 32:
            name = " Frequency ";
            break;
        case 48:
            name = " Overtones ";
            break;
        case 64:
            name = " Harmonic Base ";
            break;
        case 80:
            name = " Harmonic Samples ";
            break;
    }

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;
        case 1:
            contstr = "Vel Sens";
            break;
        case 2:
            contstr = "Panning";
            break;

        case 16:
            contstr = "Bandwidth";
            break;
        case 17:
            contstr = "Band Scale";
            break;
        case 19:
            contstr = "Spect Mode";
            break;

        case 32:
            contstr = "Detune";
            break;
        case 33:
            contstr = "Eq T";
            break;
        case 34:
            contstr = "440Hz";
            break;
        case 35:
            contstr = "Octave";
            break;
        case 36:
            contstr = "Det type";
            break;
        case 37:
            contstr = "Coarse Det";
            break;
        case 38:
            contstr = "Env Ena";
            break;

        case 48:
            contstr = "Par 1";
            break;
        case 49:
            contstr = "Par 2";
            break;
        case 50:
            contstr = "Force H";
            break;
        case 51:
            contstr = "Position";
            break;

        case 64:
            contstr = "Width";
            break;
        case 65:
            contstr = "Freq Mult";
            break;
        case 66:
            contstr = "Str";
            break;
        case 67:
            contstr = "S freq";
            break;
        case 68:
            contstr = "Size";
            break;
        case 69:
            contstr = "Type";
            break;
        case 70:
            contstr = "Halves";
            break;
        case 71:
            contstr = "Par 1";
            break;
        case 72:
            contstr = "Par 2";
            break;
        case 73:
            contstr = "Amp Mult";
            break;
        case 74:
            contstr = "Amp Mode";
            break;
        case 75:
            contstr = "Autoscale";
            break;

        case 80:
            contstr = "Base";
            break;
        case 81:
            contstr = "samp/Oct";
            break;
        case 82:
            contstr = "Num Oct";
            break;
        case 83:
            contstr = "Size";
            break;

        case 104:
            contstr = "Apply Changes";
            break;

        case 112:
            contstr = "Stereo";
            break;

        case 120:
            contstr = "De Pop";
            break;
        case 121:
            contstr = "Punch Strngth";
            break;
        case 122:
            contstr = "Punch Time";
            break;
        case 123:
            contstr = "Punch Strtch";
            break;
        case 124:
            contstr = "Punch Vel";
            break;
    }

    synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + "  PadSynth " + name + contstr + " value " + actual);
}


void InterChange::commandOscillator(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string eng_name;
    if (engine == 2)
        eng_name = "  Padsysnth";
    else
    {
        eng_name = "  Addsynth Voice " + to_string( engine & 0x3f);
        if (engine & 0x40)
            eng_name += " Modulator";
    }

    if (insert == 6)
    {
        synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + eng_name + " Harmonic " + to_string(control) + " Amplitude value " + actual);
        return;
    }
    else if(insert == 7)
    {
        synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + eng_name + " Harmonic " + to_string(control) + " Phase value " + actual);
        return;
    }

    string name = "";
    switch (control & 0x70)
    {
        case 0:
            name = " Oscillator";
            break;
        case 16:
            name = " Base Funct";
            break;
        case 32:
            name = " Base Mods";
            break;
        case 64:
            name = " Harm Mods";
            break;
    }

    string contstr;
    switch (control)
    {
        case 0:
            contstr = " Random";
            break;
        case 1:
            contstr = " Mag Type";
            break;
        case 2:
            contstr = " Harm Rnd";
            break;
        case 3:
            contstr = " Harm Rnd Type";
            break;

        case 16:
            contstr = " Par";
            break;
        case 17:
            contstr = " Type";
            break;
        case 18:
            contstr = " Mod Par 1";
            break;
        case 19:
            contstr = " Mod Par 2";
            break;
        case 20:
            contstr = " Mod Par 3";
            break;
        case 21:
            contstr = " Mod Type";
            break;

        case 32:
            contstr = " Osc Autoclear";
            break;
        case 33:
            contstr = " Osc As Base";
            break;

        case 34:
            contstr = " Waveshape Par";
            break;
        case 35:
            contstr = " Waveshape Type";
            break;

        case 36:
            contstr = " Osc Filt Par 1";
            break;
        case 37:
            contstr = " Osc Filt Par 2";
            break;
        case 38:
            contstr = " Osc Filt B4 Waveshape";
            break;
        case 39:
            contstr = " Osc Filt Type";
            break;

        case 40:
            contstr = " Osc Mod Par 1";
            break;
        case 41:
            contstr = " Osc Mod Par 2";
            break;
        case 42:
            contstr = " Osc Mod Par 3";
            break;
        case 43:
            contstr = " Osc Mod Type";
            break;

        case 44:
            contstr = " Osc Spect Par";
            break;
        case 45:
            contstr = " Osc Spect Type";
            break;

        case 64:
            contstr = " Shift";
            break;
        case 65:
            contstr = " Reset";
            break;
        case 66:
            contstr = " B4 Waveshape & Filt";
            break;

        case 67:
            contstr = " Adapt Param";
            break;
        case 68:
            contstr = " Adapt Base Freq";
            break;
        case 69:
            contstr = " Adapt Power";
            break;
        case 70:
            contstr = " Adapt Type";
            break;

        case 96:
            contstr = " Clear Harmonics";
            break;
        case 97:
            contstr = " Conv To Sine";
            break;

        case 104:
            contstr = " Apply Changes";
            break;
    }

    synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + eng_name + name + contstr + "  value " + actual);

}


void InterChange::commandResonance(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (engine == 0)
        name = "  AddSynth";
    else
        name = "  PadSynth";

    if (insert == 9)
    {
        synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + name + " Resonance Point " + to_string(control) + "  value " + actual);
        return;
    }

    string contstr;
    switch (control)
    {
        case 0:
            contstr = "Max dB";
            break;
        case 1:
            contstr = "Centre Freq";
            break;
        case 2:
            contstr = "Octaves";
            break;

        case 8:
            contstr = "Enable";
            break;

        case 10:
            contstr = "Random";
            break;

        case 20:
            contstr = "Interpolate Peaks";
            break;
        case 21:
            contstr = "Protect Fundamental";
            break;

        case 96:
            contstr = "Clear";
            break;
        case 97:
            contstr = "Smooth";
            break;

        case 104:
            contstr = "Apply Changes";
            break;
    }

    synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + name + " Resonance " + contstr + "  value " + actual);
}


void InterChange::commandLFO(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem, unsigned char engine, unsigned char insert, unsigned char parameter)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (engine == 0)
        name = "  AddSynth";
    else if (engine == 2)
        name = "  PadSynth";
    else if (engine >= 0x80)
        name = "  AddSynth Voice " + to_string(engine & 0x3f);

    string lfo;
    switch (parameter)
    {
        case 0:
            lfo = "  Amp";
            break;
        case 1:
            lfo = "  Freq";
            break;
        case 2:
            lfo = "  Filt";
            break;
    }

    string contstr;
    switch (control)
    {
        case 0:
            contstr = "Freq";
            break;
        case 1:
            contstr = "Depth";
            break;
        case 2:
            contstr = "Delay";
            break;
        case 3:
            contstr = "Start";
            break;
        case 4:
            contstr = "AmpRand";
            break;
        case 5:
            contstr = "Type";
            break;
        case 6:
            contstr = "Cont";
            break;
        case 7:
            contstr = "FreqRand";
            break;
        case 8:
            contstr = "Stretch";
            break;
    }

    synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + name + lfo + " LFO  " + contstr + " value " + actual);
}


void InterChange::commandFilter(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (kititem >= 0x80)
    {
        string efftype;
        switch (kititem & 0xf)
        {
            case 0:
                efftype = " NO Effect";
                break;
            case 1:
                efftype = " Reverb";
                break;
            case 2:
                efftype = " Echo";
                break;
            case 3:
                efftype = " Chorus";
                break;
            case 4:
                efftype = " Phaser";
                break;
            case 5:
                efftype = " AlienWah";
                break;
            case 6:
                efftype = " Distortion";
                break;
            case 7:
                efftype = " EQ";
                break;
            case 8:
                efftype = " DynFilter";
                break;
        }

        if (npart == 0xf1)
            name = "System";
        else if (npart == 0xf2)
            name = "Insert";
        else name = "Part " + to_string(npart);
        name += " Effect " + to_string(engine); // this is the effect number
        synth->getRuntime().Log(name + efftype + " ~ Filter Parameter " + to_string(control) + "  Value " + actual);
    return;
    }

    if (engine == 0)
        name = "  AddSynth";
    else if (engine == 1)
        name = "  SubSynth";
    else if (engine == 2)
        name = "  PadSynth";
    else if (engine >= 0x80)
        name = "  Adsynth Voice " + to_string(engine & 0x3f);

    string contstr;
    switch (control)
    {
        case 0:
            contstr = "C_Freq";
            break;
        case 1:
            contstr = "Q";
            break;
        case 3:
            contstr = "VsensA";
            break;
        case 4:
            contstr = "Vsens";
            break;
        case 2:
            contstr = "FreqTr";
            break;
        case 5:
            contstr = "gain";
            break;
        case 6:
            contstr = "Stages";
            break;
        case 7:
            contstr = "Filt Type";
            break;
        case 8:
            contstr = "An Type";
            break;
        case 9:
            contstr = "St Type";
            break;

        case 16:
            contstr = "Form Fr Sl";
            break;
        case 17:
            contstr = "Form Vw Cl";
            break;
        case 18:
            contstr = "Form Freq";
            break;
        case 19:
            contstr = "Form Q";
            break;
        case 20:
            contstr = "Form Amp";
            break;
        case 21:
            contstr = "Form Stretch";
            break;
        case 22:
            contstr = "Form Cent Freq";
            break;
        case 23:
            contstr = "Form Octave";
            break;

        case 32:
            contstr = "Formants";
            break;
        case 33:
            contstr = "Vowel Num";
            break;
        case 34:
            contstr = "Formant Num";
            break;
        case 35:
            contstr = "Seq Size";
            break;
        case 36:
            contstr = "Seq Pos";
            break;
        case 37:
            contstr = "Seq Vowel";
            break;
        case 38:
            contstr = "Neg Input";
            break;
    }

    synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + name + " Filter  " + contstr + " value " + actual);
}


void InterChange::commandEnvelope(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem, unsigned char engine, unsigned char insert, unsigned char parameter)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (engine == 0)
        name = "  AddSynth";
    else if (engine == 1)
        name = "  SubSynth";
    else if (engine == 2)
        name = "  PadSynth";
    else if (engine >= 0x80)
    {
        name = "  Adsynth Voice ";
        if (engine >= 0xC0)
            name += "Modulator ";
        name += to_string(engine & 0x3f);
    }

    string env;
    switch(parameter)
    {
        case 0:
            env = "  Amp";
            break;
        case 1:
            env = "  Freq";
            break;
        case 2:
            env = "  Filt";
            break;
    }


    if (insert == 3)
    {
        synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + name  + env + " Env Freemode Control " + to_string(control) + "  X value " + actual);
        return;
    }
    if (insert == 4)
    {
        synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + name  + env + " Env Freemode Control " +  to_string(control) + "  Y value " + actual);
        return;
    }

    string contstr;
    switch (control)
    {
        case 0:
            contstr = "A val";
            break;
        case 1:
            contstr = "A dt";
            break;
        case 2:
            contstr = "D val";
            break;
        case 3:
            contstr = "D dt";
            break;
        case 4:
            contstr = "S val";
            break;
        case 5:
            contstr = "R dt";
            break;
        case 6:
            contstr = "R val";
            break;
        case 7:
            contstr = "Stretch";
            break;

        case 16:
            contstr = "frcR";
            break;
        case 17:
            contstr = "L";
            break;

        case 24:
            contstr = "Edit";
            break;

        case 32:
            contstr = "Freemode";
            break;
        case 33:
            contstr = "Add Point";
            break;
        case 34:
            contstr = "Del Point";
            break;
        case 35:
            contstr = "Sust";
            break;
        case 39:
            contstr = "Stretch";
            break;

        case 48:
            contstr = "frcR";
            break;
        case 49:
            contstr = "L";
            break;
    }

    synth->getRuntime().Log("Part " + to_string(npart) + "  Kit " + to_string(kititem) + name  + env + " Env  " + contstr + " value " + actual);
}


void InterChange::commandSysIns(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (npart == 0xf1)
        name = "System ";
    else
        name = "Insert ";

    string contstr;
    string second;
    if (insert == 0xff)
    {
        second = "";
        switch (control)
        {
            case 0:
                contstr = "Number ";
                break;
            case 1:
                contstr = to_string(engine) + " Type ";
                break;
            case 2:
                contstr = to_string(engine) + " To ";
                break;
        }
        contstr = "Effect " + contstr + actual;
    }
    else
    {
        contstr = "From Effect " + to_string(engine);
        second = " To Effect " + to_string(control)  + "  Value " + actual;
    }

    synth->getRuntime().Log(name  + contstr + second);
}


void InterChange::commandEffects(float value, unsigned char type, unsigned char control, unsigned char npart, unsigned char kititem, unsigned char engine)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (npart == 0xf1)
        name = "System";
    else if (npart == 0xf2)
        name = "Insert";
    else
        name = "Part " + to_string(npart);
    name += " Effect " + to_string(engine);

    string efftype;
    switch (kititem & 0xf)
    {
        case 0:
            efftype = " NO Effect";
            break;
        case 1:
            efftype = " Reverb";
            break;
        case 2:
            efftype = " Echo";
            break;
        case 3:
            efftype = " Chorus";
            break;
        case 4:
            efftype = " Phaser";
            break;
        case 5:
            efftype = " AlienWah";
            break;
        case 6:
            efftype = " Distortion";
            break;
        case 7:
            efftype = " EQ";
            break;
        case 8:
            efftype = " DynFilter";
            break;
    }

    string contstr = "  Control " + to_string(control);

    synth->getRuntime().Log(name + efftype + contstr + "  Value " + actual);
}
