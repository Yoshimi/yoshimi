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

InterChange::InterChange(SynthEngine *_synth) :
    synth(_synth)
{
    // quite incomplete - we don't know what will develop yet!
}


InterChange::~InterChange()
{
}


void InterChange::commandFetch(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char insertParam)
{
    /*
     * while testing, this simply sends everything to commandSend but eventually it will
     * partially do the decding and direction via ring buffers for actualy control.
     */
    commandSend(value, type, control, part, kit, engine, insert, insertParam);
    return;
}


void InterChange::commandSend(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char insertParam)
{
    bool isGui = type & 0x20;
    char button = type & 0x1f;
    string isf;
    if (isGui && button != 2)
    {
        if (!(type &0x80))
            isf = "f";
        synth->getRuntime().Log("\nButton " + asString((int) type & 7) + "\nPart " + asString((int) part) + "\nKit " + asString((int) kit) + "\nEngine " + asString((int) engine) + "\nInsert " + asString((int) insert) + "  Insert Param " + asString((int) insertParam) + "\nControl " + asString((int) control) + "  Value " + asString(value) + isf);
        return;
    }
    if (part == 0xc0)
        commandVector(value, type, control);
    else if (part == 0xf0)
        commandMain(value, type, control);
    else if ((part == 0xf1 || part == 0xf2) && kit == 0xff)
        commandSysIns(value, type, control, part, engine, insert);
    else if (kit == 0xff || (kit & 0x20))
        commandPart(value, type, control, part, kit, engine);
    else if (kit >= 0x80)
    {
        if (insert < 0xff)
            commandFilter(value, type, control, part, kit, engine, insert);
        else
            commandEffects(value, type, control, part, kit, engine);
    }
    else if (engine == 2)
    {
        switch(insert)
        {
            case 0xff:
                commandPad(value, type, control, part, kit);
                break;
            case 0:
                commandLFO(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 1:
                commandFilter(value, type, control, part, kit, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 5:
            case 6:
            case 7:
                commandOscillator(value, type, control, part, kit, engine, insert);
                break;
            case 8:
            case 9:
                commandResonance(value, type, control, part, kit, engine, insert);
                break;
        }
    }
    else if (engine == 1)
    {
        switch (insert)
        {
            case 0xff:
                commandSub(value, type, control, part, kit, insert);
                break;
            case 1:
                commandFilter(value, type, control, part, kit, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, part, kit, engine, insert, insertParam);
                break;
        }
    }
    else if (engine >= 0x80)
    {
        switch (insert)
        {
            case 0xff:
                commandAddVoice(value, type, control, part, kit, engine);
                break;
            case 0:
                commandLFO(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 1:
                commandFilter(value, type, control, part, kit, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 5:
            case 6:
            case 7:
                commandOscillator(value, type, control, part, kit, engine, insert);
                break;
        }
    }
    else if (engine == 0)
    {
        switch (insert)
        {
            case 0xff:
                commandAdd(value, type, control, part, kit);
                break;
            case 0:
                commandLFO(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 1:
                commandFilter(value, type, control, part, kit, engine, insert);
                break;
            case 2:
            case 3:
            case 4:
                commandEnvelope(value, type, control, part, kit, engine, insert, insertParam);
                break;
            case 8:
                commandResonance(value, type, control, part, kit, engine, insert);
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
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            break;

        case 14:
            contstr = "Part Number";
            break;
        case 15:
            contstr = "Available Parts";
            break;

        case 32:
            contstr = "Detune";
            break;
        case 35:
            contstr = "Key Shift";
            break;

        case 96:
            contstr = "Reset All";
            break;
        case 128:
            contstr = "Stop";
            break;
    }
    synth->getRuntime().Log("Main " + contstr + " value " + actual);
}


void InterChange::commandPart(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);


    string kitnum;
    if (kit < 0xff)
        kitnum = "  Kit " + to_string(kit & 0x1f) + " ";
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
        switch (engine) // needs aligning with other engine numbers
        {
            case 1:
                name = "AddSynth ";
                break;
            case 2:
                name = "SubSynth ";
                break;
            case 3:
                name = "PadSynth ";
                break;
        }
    }

    string contstr = "";
    switch (control)
    {
        case 0:
            contstr = "Volume";
            if (type & 0x40)
                synth->part[part]->setVolume(value);
            else
                actual = to_string(synth->part[part]->Pvolume);
            break;
        case 1:
            contstr = "Vel Sens";
            if (type & 0x40)
                synth->part[part]->Pvelsns = value;
            else
                actual = to_string(synth->part[part]->Pvelsns);
            break;
        case 2:
            contstr = "Panning";
            if (type & 0x40)
                synth->part[part]->SetController(C_panning, value);
            else
                actual = to_string(synth->part[part]->Ppanning);
            break;
        case 4:
            contstr = "Vel Offset";
            if (type & 0x40)
                synth->part[part]->Pveloffs = value;
            else
                actual = to_string(synth->part[part]->Pveloffs);
            break;
        case 5:
            contstr = "Midi";
            break;
        case 6:
            contstr = "Mode";
            break;
        case 7:
            contstr = "Portamento";
            break;
        case 8:
            contstr = "Enable";
            break;
        case 9:
            contstr = "Mute";
            break;

        case 16:
            contstr = "Min Note";
            break;
        case 17:
            contstr = "Max Note";
            break;
        case 18:
            contstr = "Min To Last";
            break;
        case 19:
            contstr = "Max To Last";
            break;

        case 33:
            contstr = "Key Limit";
            break;
        case 35:
            contstr = "Key Shift";
            break;

        case 40:
            contstr = "Effect Send 0";
            break;
        case 41:
            contstr = "Effect Send 1";
            break;
        case 42:
            contstr = "Effect Send 2";
            break;
        case 43:
            contstr = "Effect Send 3";
            break;

        case 48:
            contstr = "Humanise";
            break;

        case 56:
            contstr = "Mode";
            break;
        case 57:
            contstr = "Drum Mode";
            break;
        case 58:
            contstr = "Kit Mode";
            break;

        case 64:
            contstr = "Effect Number";
            break;

        case 96:
            contstr = "Reset Note Range";
            break;

        case 120:
            contstr = "Audio destination";
            break;

        case 128:
            contstr = "Vol Range";
            break;
        case 129:
            contstr = "Vol Enable";
            break;
        case 130:
            contstr = "Pan Width";
            break;
        case 131:
            contstr = "Mod Wheel Depth";
            break;
        case 132:
            contstr = "Exp Mod Wheel";
            break;
        case 133:
            contstr = "Bandwidth depth";
            break;
        case 134:
            contstr = "Exp Bandwidth";
            break;
        case 135:
            contstr = "Expression Enable";
            break;
        case 136:
            contstr = "FM Amp Enable";
            break;
        case 137:
            contstr = "Sustain Ped Enable";
            break;
        case 138:
            contstr = "Pitch Wheel Range";
            break;
        case 139:
            contstr = "Filter Q Depth";
            break;
        case 140:
            contstr = "Filter Cutoff Depth";
            break;

        case 144:
            contstr = "Res Cent Freq Depth";
            break;
        case 145:
            contstr = "Res Band Depth";
            break;

        case 160:
            contstr = "Time";
            break;
        case 161:
            contstr = "Tme Stretch";
            break;
        case 162:
            contstr = "Threshold";
            break;
        case 163:
            contstr = "Threshold Type";
            break;
        case 164:
            contstr = "Prop Enable";
            break;
        case 165:
            contstr = "Prop Rate";
            break;
        case 166:
            contstr = "Prop depth";
            break;
        case 168:
            contstr = "Enable";
            break;

        case 224:
            contstr = "Clear";
            break;
    }
    synth->getRuntime().Log("Part " + to_string(part) + kitnum + name + contstr + " value " + actual);
}


void InterChange::commandAdd(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit)
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

    synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  AddSynth " + name + contstr + " value " + actual);
}


void InterChange::commandAddVoice(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit,unsigned char engine)
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

    synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  AddSynth Voice " + to_string(engine & 0x1f) + name + contstr + " value " + actual);
}


void InterChange::commandSub(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    if (insert == 6)
    {
        synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  Subsynth Harmonic " + to_string(control) + " Amplitude value " + actual);
        return;
    }

    if (insert == 7)
    {
        synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  Subsynth Harmonic " + to_string(control) + " Bandwidth value " + actual);
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
        case 18:
            contstr = "Env Enab";
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
            contstr = "Enable";
            break;
        case 80:
            contstr = "Filt Stages";
            break;
        case 81:
            contstr = "Mag Type";
            break;
        case 82:
            contstr = "Start";
            break;
        case 96:
            contstr = "Clear Harmonics";
            break;
        case 112:
            contstr = "Stereo";
            break;
    }

    synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  SubSynth " + name + contstr + " value " + actual);
}


void InterChange::commandPad(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit)
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

    synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + "  PadSynth " + name + contstr + " value " + actual);
}


void InterChange::commandOscillator(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert)
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
        synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + eng_name + " Harmonic " + to_string(control) + " Amplitude value " + actual);
        return;
    }
    else if(insert == 7)
    {
        synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + eng_name + " Harmonic " + to_string(control) + " Phase value " + actual);
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

    synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + eng_name + name + contstr + "  value " + actual);

}


void InterChange::commandResonance(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert)
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
        synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name + " Resonance Point " + to_string(control) + "  value " + actual);
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

    synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name + " Resonance " + contstr + "  value " + actual);
}


void InterChange::commandLFO(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter)
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

    synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name + lfo + " LFO  " + contstr + " value " + actual);
}


void InterChange::commandFilter(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (kit >= 0x80)
    {
        string efftype;
        switch (kit & 0xf)
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

        if (part == 0xf1)
            name = "System";
        else if (part == 0xf2)
            name = "Insert";
        else name = "Part " + to_string(part);
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

    synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name + " Filter  " + contstr + " value " + actual);
}


void InterChange::commandEnvelope(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter)
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
        synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name  + env + " Env Freemode Control " + to_string(control) + "  X value " + actual);
        return;
    }
    if (insert == 4)
    {
        synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name  + env + " Env Freemode Control " +  to_string(control) + "  Y value " + actual);
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

    synth->getRuntime().Log("Part " + to_string(part) + "  Kit " + to_string(kit) + name  + env + " Env  " + contstr + " value " + actual);
}


void InterChange::commandSysIns(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char engine, unsigned char insert)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (part == 0xf1)
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


void InterChange::commandEffects(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine)
{
    string actual;
    if (type & 0x80)
        actual = to_string((int)round(value));
    else
        actual = to_string(value);

    string name;
    if (part == 0xf1)
        name = "System";
    else if (part == 0xf2)
        name = "Insert";
    else
        name = "Part " + to_string(part);
    name += " Effect " + to_string(engine);

    string efftype;
    switch (kit & 0xf)
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
