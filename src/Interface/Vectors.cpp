/*
    Vectors.cpp

    Copyright 2024 Will Godfrey

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

#include <string>
#include <iostream>

#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "Misc/Part.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/XMLwrapper.h"
#include <Interface/Vectors.h>

using file::isRegularFile;
using file::setExtension;
using file::findLeafName;

using std::string;
using std::to_string;


namespace { // Implementation details...
    TextMsgBuffer& textMsgBuffer = TextMsgBuffer::instance();
}



Vectors::Vectors(SynthEngine *_synth) :
    synth(_synth)
{
 //init
}

Vectors::~Vectors()
{
    //close
}


uchar Vectors::loadVectorAndUpdate(uchar baseChan, string const& name)
{
    uchar result = loadVector(baseChan, name, true);
    synth->ShutUp();
    return result;
}


uchar Vectors::loadVector(uchar baseChan, string const& name, bool full)
{
    std::cout << "loading vector" << std::endl;
    bool a = full; full = a; // suppress warning
    uchar actualBase = NO_MSG; // error!
    if (name.empty())
    {
        synth->getRuntime().Log("No filename", _SYS_::LogNotSerious);
        return actualBase;
    }
    string file = setExtension(name, EXTEN::vector);
    if (!isRegularFile(file))
    {
        synth->getRuntime().Log("Can't find " + file, _SYS_::LogNotSerious);
        return actualBase;
    }

    auto xml{std::make_unique<XMLwrapper>(*synth, true)};
    xml->loadXMLfile(file);
    if (!xml->enterbranch("VECTOR"))
    {
        synth->getRuntime(). Log("Extract Data, no VECTOR branch", _SYS_::LogNotSerious);
    }
    else
    {
        actualBase = extractVectorData(baseChan, *xml, findLeafName(name));
        int lastPart = NUM_MIDI_PARTS;
        if (synth->getRuntime().vectordata.Yaxis[actualBase] >= 0x7f)
            lastPart = NUM_MIDI_CHANNELS * 2;
        for (int npart = 0; npart < lastPart; npart += NUM_MIDI_CHANNELS)
        {
            if (xml->enterbranch("PART", npart))
            {
                synth->part[npart + actualBase]->getfromXML(*xml);
                synth->part[npart + actualBase]->Prcvchn = actualBase;
                xml->exitbranch();
                synth->setPartMap(npart + actualBase);

                synth->partonoffWrite(npart + baseChan, 1);
                if (synth->part[npart + actualBase]->Paudiodest & 2)
                    Config::instances().registerAudioPort(synth->getUniqueId(), npart+actualBase);
            }
        }
        xml->endbranch(); // VECTOR
    }
    return actualBase;
}


uchar Vectors::extractVectorData(uchar baseChan, XMLwrapper& xml, string const& name)
{
    uint lastPart = NUM_MIDI_PARTS;
    uchar tmp;
    string newname = xml.getparstr("name");

    if (baseChan >= NUM_MIDI_CHANNELS)
        baseChan = xml.getpar255("Source_channel", 0);

    if (newname > "!" && newname.find("No Name") != 1)
        synth->getRuntime().vectordata.Name[baseChan] = newname;
    else if (!name.empty())
        synth->getRuntime().vectordata.Name[baseChan] = name;
    else
        synth->getRuntime().vectordata.Name[baseChan] = "No Name " + to_string(baseChan);

    tmp = xml.getpar255("X_sweep_CC", 0xff);
    if (tmp >= 0x0e && tmp  < 0x7f)
    {
        synth->getRuntime().vectordata.Xaxis[baseChan] = tmp;
        synth->getRuntime().vectordata.Enabled[baseChan] = true;
    }
    else
    {
        synth->getRuntime().vectordata.Xaxis[baseChan] = 0x7f;
        synth->getRuntime().vectordata.Enabled[baseChan] = false;
    }

    // should exit here if not enabled

    tmp = xml.getpar255("Y_sweep_CC", 0xff);
    if (tmp >= 0x0e && tmp  < 0x7f)
        synth->getRuntime().vectordata.Yaxis[baseChan] = tmp;
    else
    {
        lastPart = NUM_MIDI_CHANNELS * 2;
        synth->getRuntime().vectordata.Yaxis[baseChan] = 0x7f;
        synth->partonoffWrite(baseChan + NUM_MIDI_CHANNELS * 2, 0);
        synth->partonoffWrite(baseChan + NUM_MIDI_CHANNELS * 3, 0);
        // disable these - not in current vector definition
    }

    int x_feat = 0;
    int y_feat = 0;
    if (xml.getparbool("X_feature_1", false))
        x_feat |= 1;
    if (xml.getparbool("X_feature_2", false))
        x_feat |= 2;
    if (xml.getparbool("X_feature_2_R", false))
        x_feat |= 0x10;
    if (xml.getparbool("X_feature_4", false))
        x_feat |= 4;
    if (xml.getparbool("X_feature_4_R", false))
        x_feat |= 0x20;
    if (xml.getparbool("X_feature_8", false))
        x_feat |= 8;
    if (xml.getparbool("X_feature_8_R", false))
        x_feat |= 0x40;
    synth->getRuntime().vectordata.Xcc2[baseChan] = xml.getpar255("X_CCout_2", 10);
    synth->getRuntime().vectordata.Xcc4[baseChan] = xml.getpar255("X_CCout_4", 74);
    synth->getRuntime().vectordata.Xcc8[baseChan] = xml.getpar255("X_CCout_8", 1);
    if (lastPart == NUM_MIDI_PARTS)
    {
        if (xml.getparbool("Y_feature_1", false))
            y_feat |= 1;
        if (xml.getparbool("Y_feature_2", false))
            y_feat |= 2;
        if (xml.getparbool("Y_feature_2_R", false))
            y_feat |= 0x10;
        if (xml.getparbool("Y_feature_4", false))
            y_feat |= 4;
        if (xml.getparbool("Y_feature_4_R", false))
            y_feat |= 0x20;
        if (xml.getparbool("Y_feature_8", false))
            y_feat |= 8;
        if (xml.getparbool("Y_feature_8_R", false))
            y_feat |= 0x40;
        synth->getRuntime().vectordata.Ycc2[baseChan] = xml.getpar255("Y_CCout_2", 10);
        synth->getRuntime().vectordata.Ycc4[baseChan] = xml.getpar255("Y_CCout_4", 74);
        synth->getRuntime().vectordata.Ycc8[baseChan] = xml.getpar255("Y_CCout_8", 1);
    }
    synth->getRuntime().vectordata.Xfeatures[baseChan] = x_feat;
    synth->getRuntime().vectordata.Yfeatures[baseChan] = y_feat;
    if (synth->getRuntime().numAvailableParts < lastPart)
        synth->getRuntime().numAvailableParts = xml.getpar255("current_midi_parts", synth->getRuntime().numAvailableParts);
    return baseChan;
}


uchar Vectors::saveVector(uchar baseChan, string const& name, bool full)
{
    bool a = full; full = a; // suppress warning
    uchar result = NO_MSG; // ok

    if (baseChan >= NUM_MIDI_CHANNELS)
        return textMsgBuffer.push("Invalid channel number");
    if (name.empty())
        return textMsgBuffer.push("No filename");
    if (synth->getRuntime().vectordata.Enabled[baseChan] == false)
        return textMsgBuffer.push("No vector data on this channel");

    string file = setExtension(name, EXTEN::vector);

    synth->getRuntime().xmlType = TOPLEVEL::XML::Vector;
    auto xml{std::make_unique<XMLwrapper>(*synth, true)};

    xml->beginbranch("VECTOR");
        insertVectorData(baseChan, true, *xml, findLeafName(file));
    xml->endbranch();

    if (!xml->saveXMLfile(file))
    {
        synth->getRuntime().Log("Failed to save data to " + file, _SYS_::LogNotSerious);
        result = textMsgBuffer.push("FAIL");
    }
    return result;
}


bool Vectors::insertVectorData(uchar baseChan, bool full, XMLwrapper& xml, string const& name)
{
    int lastPart = NUM_MIDI_PARTS;
    int x_feat = synth->getRuntime().vectordata.Xfeatures[baseChan];
    int y_feat = synth->getRuntime().vectordata.Yfeatures[baseChan];

    if (synth->getRuntime().vectordata.Name[baseChan].find("No Name") != 1)
        xml.addparstr("name", synth->getRuntime().vectordata.Name[baseChan]);
    else
        xml.addparstr("name", name);

    xml.addpar("Source_channel", baseChan);
    xml.addpar("X_sweep_CC", synth->getRuntime().vectordata.Xaxis[baseChan]);
    xml.addpar("Y_sweep_CC", synth->getRuntime().vectordata.Yaxis[baseChan]);
    xml.addparbool("X_feature_1", (x_feat & 1) > 0);
    xml.addparbool("X_feature_2", (x_feat & 2) > 0);
    xml.addparbool("X_feature_2_R", (x_feat & 0x10) > 0);
    xml.addparbool("X_feature_4", (x_feat & 4) > 0);
    xml.addparbool("X_feature_4_R", (x_feat & 0x20) > 0);
    xml.addparbool("X_feature_8", (x_feat & 8) > 0);
    xml.addparbool("X_feature_8_R", (x_feat & 0x40) > 0);
    xml.addpar("X_CCout_2",synth->getRuntime().vectordata.Xcc2[baseChan]);
    xml.addpar("X_CCout_4",synth->getRuntime().vectordata.Xcc4[baseChan]);
    xml.addpar("X_CCout_8",synth->getRuntime().vectordata.Xcc8[baseChan]);
    if (synth->getRuntime().vectordata.Yaxis[baseChan] > 0x7f)
    {
        lastPart /= 2;
    }
    else
    {
        xml.addparbool("Y_feature_1", (y_feat & 1) > 0);
        xml.addparbool("Y_feature_2", (y_feat & 2) > 0);
        xml.addparbool("Y_feature_2_R", (y_feat & 0x10) > 0);
        xml.addparbool("Y_feature_4", (y_feat & 4) > 0);
        xml.addparbool("Y_feature_4_R", (y_feat & 0x20) > 0);
        xml.addparbool("Y_feature_8", (y_feat & 8) > 0);
        xml.addparbool("Y_feature_8_R", (y_feat & 0x40) > 0);
        xml.addpar("Y_CCout_2",synth->getRuntime().vectordata.Ycc2[baseChan]);
        xml.addpar("Y_CCout_4",synth->getRuntime().vectordata.Ycc4[baseChan]);
        xml.addpar("Y_CCout_8",synth->getRuntime().vectordata.Ycc8[baseChan]);
    }
    if (full)
    {
        xml.addpar("current_midi_parts", lastPart);
        for (int npart = 0; npart < lastPart; npart += NUM_MIDI_CHANNELS)
        {
            xml.beginbranch("PART",npart);
            synth->part[npart + baseChan]->add2XML(xml);
            xml.endbranch();
        }
    }
    return true;
}


float Vectors::getVectorLimits(CommandBlock* getData)
{
    float value = getData->data.value;
    uchar request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    uchar type = 0;

    // vector defaults
    type |= TOPLEVEL::type::Integer;
    int min = 0;
    float def = 0;
    int max = 1;

    switch (control)
    {
        case VECTOR::control::undefined:
            break;
        case VECTOR::control::name:
            break;
        case VECTOR::control::Xcontroller:
            max = 119;
            break;
        case VECTOR::control::XleftInstrument:
            max = 159;
            break;
        case VECTOR::control::XrightInstrument:
            max = 159;
            break;
        case VECTOR::control::Xfeature0:
            break;
        case VECTOR::control::Xfeature1:
            max = 2;
            break;
        case VECTOR::control::Xfeature2:
            max = 2;
            break;
        case VECTOR::control::Xfeature3:
            max = 2;
            break;
        case VECTOR::control::Ycontroller:
            max = 119;
            break;
        case VECTOR::control::YupInstrument:
            max = 159;
            break;
        case VECTOR::control::YdownInstrument:
            max = 159;
            break;
        case VECTOR::control::Yfeature0:
            break;
        case VECTOR::control::Yfeature1:
            max = 2;
            break;
        case VECTOR::control::Yfeature2:
            max = 2;
            break;
        case VECTOR::control::Yfeature3:
            max = 2;
            break;
        case VECTOR::control::erase:
            break;

        default: // TODO
            type |= TOPLEVEL::type::Error;
            break;
    }
    getData->data.type = type;
    if (type & TOPLEVEL::type::Error)
        return 1;

    switch (request)
    {
        case TOPLEVEL::type::Adjust:
            if (value < min)
                value = min;
            else if (value > max)
                value = max;
        break;
        case TOPLEVEL::type::Minimum:
            value = min;
            break;
        case TOPLEVEL::type::Maximum:
            value = max;
            break;
        case TOPLEVEL::type::Default:
            value = def;
            break;
    }
    return value;
}
