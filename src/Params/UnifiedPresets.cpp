/*
    UnifiedPresets.cpp - Presets and Clipboard management

    Copyright 2018-2024 Will Godfrey

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
#include <memory>

#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/FileMgrFuncs.h"
#include "Interface/TextLists.h"
#include "Effects/EffectMgr.h"
#include "Params/UnifiedPresets.h"
#include "Params/ADnoteParameters.h"
#include "Params/SUBnoteParameters.h"
#include "Params/PADnoteParameters.h"
#include "Params/FilterParams.h"
#include "Params/LFOParams.h"
#include "Params/EnvelopeParams.h"


using std::string;
using std::to_string;

/**
 * type flags (set)
 *      List  - all entries of section type
 *      Group - preset extension and name
 *      Copy  - from section to file
 *      Paste - from file to section
 *
 * no name given - from/to clipboard))
 */
string UnifiedPresets::handleStoreLoad()
{
    int type = cmd.data.type;
    int value = cmd.data.value;
    human = value; // used for listing. 'value may change before it is read
    if (type == TOPLEVEL::type::List && human > 0)
    {
        string group{findPresetType()};
        if (human == 2)
        {
            /* here we abuse the list routines in order to find out
             * if  there is a clipboard entry for this preset group
            */
            string filename = file::localDir() + "/clipboard/section." + group + EXTEN::presets;
            if (file::isRegularFile(filename) == 0)
            {
                return ""; // no entry of this type
            }
        }
        /*
         * sending a message here was doubling the number of messages
         * but only one was actually being read!
         */
        value = UNUSED;
        return findPresetType(); // human friendly extension
    }

    string name{findPresetType()};
    if (name.empty())
    {
        name = "No section presets in this context";
        return name;
    }
    string dirname{synth.getRuntime().presetsDirlist[synth.getRuntime().presetsRootID]};
    if (dirname.empty())
    {
        name = "Directory empty";
        return name;
    }

    if (type == TOPLEVEL::type::List)
    {
        list(dirname, name);
    }
    else
    {
        if (type & TOPLEVEL::type::Copy)
        {
            save();
            name = "";
        }
        else if (type & TOPLEVEL::type::Paste)
        {
            if (human == 0)
                load();
            else
                remove();
        }
    }
    return name;
}

string UnifiedPresets::listpos(int count)  const
{
 // If human = 2 we want to get the extension not the friendly name
    int test = 0;
    if (human == 1)
        test = 1;
    return presetgroups[count * 2 + test];
}

string UnifiedPresets::findPresetType()
{
    int npart = cmd.data.part;
    int kitItem = cmd.data.kit;
    int engineType = cmd.data.engine;
    int insert = cmd.data.insert;
    int parameter = cmd.data.parameter;
    int offset = cmd.data.offset;
    string name = "";

    if (npart != TOPLEVEL::section::systemEffects && npart != TOPLEVEL::section::insertEffects && npart > TOPLEVEL::section::part64)
        return name;
    if (kitItem >= EFFECT::type:: none && kitItem < EFFECT::type::count)
    {
        if (insert == TOPLEVEL::insert::filterGroup)
        {
            if (offset == UNUSED)
                return listpos(0);//"Pfilter";
            else
                return listpos(1);//"Pfiltern";
        }
        else
            return listpos(2);//"Peffect";
    }

    switch (insert)
    {
        case TOPLEVEL::insert::filterGroup:
            {
                if (offset == UNUSED)
                    name = listpos(3);//"Pfilter";
                else
                    name = listpos(4);//"Pfiltern";
            }
            break;

        case TOPLEVEL::insert::oscillatorGroup:
            name = listpos(5);//"Poscilgen";
            break;
        case TOPLEVEL::insert::resonanceGroup:
            name = listpos(6);//"Presonance";
            break;
        case TOPLEVEL::insert::LFOgroup:
            switch (parameter)
            {
                case 0:
                    name = listpos(7);//"Plfoamplitude";
                    break;
                case 1:
                    name = listpos(8);//"Plfofrequency";
                    break;
                case 2:
                    name = listpos(9);//"Plfofilter";
                    break;
            }
            break;
        case TOPLEVEL::insert::envelopeGroup:
            switch (parameter)
            {
                case 0:
                    name = listpos(10);//"Penvamplitude";
                    break;
                case 1:
                    name = listpos(11);//"Penvfrequency";
                    break;
                case 2:
                    name = listpos(12);//"Penvfilter";
                    break;
                case 3:
                    name = listpos(13);//"Penvbandwidth";
                    break;
            }
            break;
    }
    if (!name.empty())
        return name;

    if (engineType >= PART::engine::addVoice1 && engineType < PART::engine::addVoiceModEnd)
    {
        return listpos(14);//"Padsythn"; // all voice and modulator level have the same extension
    }

    switch (engineType)
    {
        case PART::engine::addSynth:
            name = listpos(15);//"Padsyth";
            break;
        case PART::engine::subSynth:
            name = listpos(16);//presetgroups[32+human];//"Psubsyth";
            break;
        case PART::engine::padSynth:
            name = listpos(17);//"Ppadsyth";
            break;
    }
    return name;
}


void UnifiedPresets::list(string dirname, string& name)
{

        string list = "";
        file::presetsList(dirname, name, presetList);
        if(presetList.size() > 1)
        {
            sort(presetList.begin(), presetList.end());
        }

        for (auto it = begin (presetList); it != end (presetList); ++it)
        {
            string tmp = file::findLeafName(*it);
            size_t pos = tmp.rfind('.');
            tmp = tmp.substr(0, pos);
            list += (tmp + "\n");
        }
        if (list.empty())
            name = "No presets of this type found";
        else
            name = list;
        return;
}


string UnifiedPresets::accessXML(XMLwrapper& xml, bool isLoad)
{
    int npart = cmd.data.part;
    int kitItem = cmd.data.kit;
    int engineType = cmd.data.engine;
    int insert = cmd.data.insert;

    string name;

    if (kitItem == EFFECT::type::dynFilter && insert == TOPLEVEL::insert::filterGroup)
    {
        ; // passed on to filters
    }
    else if (kitItem >= EFFECT::type:: none && kitItem < EFFECT::type::count)
    {
        {
            EffectMgr *sectionType;
            if (npart == TOPLEVEL::section::systemEffects)
            {
                sectionType = synth.sysefx[engineType];
            }
            else if (npart == TOPLEVEL::section::insertEffects)
            {
                sectionType = synth.insefx[engineType];
            }
            else
            {
                sectionType = synth.part[npart]->partefx[engineType];
            }
            name = "Peffect";

            if (isLoad)
            {
                sectionType->defaults();
                xml.enterbranch(name);
                sectionType->getfromXML(xml);
                xml.exitbranch();
                synth.pushEffectUpdate(npart);
            }
            else
            {
                xml.beginbranch(name);
                sectionType->add2XML(xml);
                xml.endbranch();
            }
        }
    }

    if (name.empty())
    {

        switch (insert)
        {
            case TOPLEVEL::insert::resonanceGroup:
            {
                name = resonanceXML(xml, isLoad);
            }
            break;

            case TOPLEVEL::insert::oscillatorGroup:
            {
                name = oscilXML(xml, isLoad);
            }
            break;

            case TOPLEVEL::insert::filterGroup:
            {
                name = filterXML(xml, isLoad);
            }
            break;

            case TOPLEVEL::insert::LFOgroup:
            {
                name = lfoXML(xml, isLoad);
            }
            break;

            case TOPLEVEL::insert::envelopeGroup:
            {
                name = envelopeXML(xml, isLoad);
            }
            break;
        }
    }
    if (!name.empty())
        return name;

    if (engineType == PART::engine::addSynth)
    {
        name = "Padsyth";
        ADnoteParameters *sectionType = synth.part[npart]->kit[kitItem].adpars;

        if (isLoad)
        {
            sectionType->defaults();
            xml.enterbranch(name);
            sectionType->getfromXML(xml);
            xml.exitbranch();
        }
        else
        {
            xml.beginbranch(name);
            sectionType->add2XML(xml);
            xml.endbranch();
        }
    }

    else if (engineType >= PART::engine::addVoice1)
    {
        name = "Padsythn";
        ADnoteParameters *sectionType = synth.part[npart]->kit[kitItem].adpars;
        size_t voice = engineType - PART::engine::addVoice1;

        if (isLoad)
        {
            sectionType->voiceDefaults(voice);
            xml.enterbranch(name);
            sectionType->getfromXMLsection(xml, voice);
            xml.exitbranch();
        }
        else
        {
            xml.beginbranch(name);
            sectionType->add2XMLsection(xml, voice);
            xml.endbranch();
        }
    }

    else if (engineType == PART::engine::subSynth)
    {
        name = "Psubsyth";
        SUBnoteParameters *sectionType = synth.part[npart]->kit[kitItem].subpars;

        if (isLoad)
        {
            sectionType->defaults();
            xml.enterbranch(name);
            sectionType->getfromXML(xml);
            xml.endbranch();
        }
        else
        {
            xml.beginbranch(name);
            sectionType->add2XML(xml);
            xml.endbranch();
        }
    }

    else if (engineType == PART::engine::padSynth)
    {
        name = "Ppadsyth";
        PADnoteParameters * sectionType = synth.part[npart]->kit[kitItem].padpars;

        if (isLoad)
        {
            sectionType->defaults();
            xml.enterbranch(name);
            sectionType->getfromXML(xml);
            xml.exitbranch();
        }
        else
        {
            xml.beginbranch(name);
            sectionType->add2XML(xml);
            xml.endbranch();
        }
    }

    return name;
}


string UnifiedPresets::resonanceXML(XMLwrapper& xml, bool isLoad)
{
    int npart = cmd.data.part;
    int kitItem = cmd.data.kit;
    int engineType = cmd.data.engine;
    string name{"Presonance"};
    Resonance* sectionType;

    if (engineType == PART::engine::addSynth)
    {
        sectionType = synth.part[npart]->kit[kitItem].adpars->GlobalPar.Reson;
    }
    else if (engineType == PART::engine::padSynth)
    {
        sectionType = synth.part[npart]->kit[kitItem].padpars->resonance.get();
    }
    else
        return "";

    if (isLoad)
    {
        xml.enterbranch(name);
        sectionType->getfromXML(xml);
        xml.exitbranch();
    }
    else
    {
        xml.beginbranch(name);
        sectionType->add2XML(xml);
        xml.endbranch();
    }

    return name;
}


string UnifiedPresets::oscilXML(XMLwrapper& xml, bool isLoad)
{
    int npart = cmd.data.part;
    int kitItem = cmd.data.kit;
    int engineType = cmd.data.engine;
    string name{"Poscilgen"};

    OscilParameters *sectionType;

    if (engineType >= (PART::engine::addVoice1))
    {
        if (engineType >= PART::engine::addMod1)
        {
            engineType -= NUM_VOICES;
            sectionType = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].POscilFM;
        }
        else
        {
            sectionType = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].POscil;
        }
    }
    else if (engineType == PART::engine::padSynth)
    {
        sectionType = synth.part[npart]->kit[kitItem].padpars->POscil.get();
    }
    else
        return "";

    if (isLoad)
    {
        xml.enterbranch(name);
        sectionType->getfromXML(xml);
        xml.exitbranch();
    }
    else
    {
        xml.beginbranch(name);
        sectionType->add2XML(xml);
        xml.endbranch();
    }

    return name;
}


string UnifiedPresets::filterXML(XMLwrapper& xml, bool isLoad)
{
    int npart = cmd.data.part;
    int kitItem = cmd.data.kit;
    int engineType = cmd.data.engine;
    int offset = cmd.data.offset;
    string name;
    if (offset == UNUSED)
    {
        name = "Pfilter";
    }
    else
    {
        name = "Pfiltern";
    }

    FilterParams *sectionType;

    // top level
    if (npart == TOPLEVEL::section::systemEffects)
    {
        sectionType = synth.sysefx[0]->filterpars;
    }
    else if (npart == TOPLEVEL::section::insertEffects)
    {
        sectionType = synth.insefx[0]->filterpars;
    }

    // part level
    else if (kitItem == EFFECT::type::dynFilter)
    {
        sectionType = synth.part[npart]->partefx[0]->filterpars;
    }
    else if (engineType == PART::engine::addSynth)
    {
        sectionType = synth.part[npart]->kit[kitItem].adpars->GlobalPar.GlobalFilter;
    }
    else if (engineType >= PART::engine::addVoice1)
    {
        sectionType = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].VoiceFilter;
    }
    else if (engineType == PART::engine::subSynth)
    {
        sectionType = synth.part[npart]->kit[kitItem].subpars->GlobalFilter;
    }
    else if (engineType == PART::engine::padSynth)
    {
        sectionType = synth.part[npart]->kit[kitItem].padpars->GlobalFilter.get();
    }
    else
    {
        return "";
    }

    if (isLoad)
    {
        if (offset == UNUSED)
        {
            xml.enterbranch(name);
            sectionType->getfromXML(xml);
            xml.exitbranch();
        }
        else
        {
            xml.enterbranch(name);
            sectionType->getfromXMLsection(xml, offset);
            xml.exitbranch();
        }
    }
    else
    {
        if (offset == UNUSED)
        {
            xml.beginbranch(name);
            sectionType->add2XML(xml);
            xml.endbranch();
        }
        else
        {
            xml.beginbranch(name);
            sectionType->add2XMLsection(xml, offset);
            xml.endbranch();
        }
    }

    return name;
}


string UnifiedPresets::lfoXML(XMLwrapper& xml, bool isLoad)
{
    int npart = cmd.data.part;
    int kitItem = cmd.data.kit;
    int engineType = cmd.data.engine;
    int parameter = cmd.data.parameter;

    string name;
    LFOParams *sectionType = NULL;

    if (engineType == PART::engine::addSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Plfoamplitude";
                sectionType = synth.part[npart]->kit[kitItem].adpars->GlobalPar.AmpLfo;
            break;
            case 1:
                name = "Plfofrequency";
                sectionType = synth.part[npart]->kit[kitItem].adpars->GlobalPar.FreqLfo;
            break;
            case 2:
                name = "Plfofilter";
                sectionType = synth.part[npart]->kit[kitItem].adpars->GlobalPar.FilterLfo;
            break;
        }
    }
    else if (engineType >= PART::engine::addVoice1)
    {
        switch (parameter)
        {
            case 0:
                name = "Plfoamplitude";
                sectionType = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].AmpLfo;
            break;
            case 1:
                name = "Plfofrequency";
                sectionType = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].FreqLfo;
            break;
            case 2:
                name = "Plfofilter";
                sectionType = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].FilterLfo;
            break;
        }
    }
    else if (engineType == PART::engine::padSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Plfoamplitude";
                sectionType = synth.part[npart]->kit[kitItem].padpars->AmpLfo.get();
            break;
            case 1:
                name = "Plfofrequency";
                sectionType = synth.part[npart]->kit[kitItem].padpars->FreqLfo.get();
            break;
            case 2:
                name = "Plfofilter";
                sectionType = synth.part[npart]->kit[kitItem].padpars->FilterLfo.get();
            break;
        }
    }
    if (name.empty())
        return "";


    if (isLoad)
    {
        xml.enterbranch(name);
        sectionType->getfromXML(xml);
        xml.exitbranch();
    }
    else
    {
        xml.beginbranch(name);
        sectionType->add2XML(xml);
        xml.endbranch();
    }

    return name;
}


string UnifiedPresets::envelopeXML(XMLwrapper& xml, bool isLoad)
{
    int npart = cmd.data.part;
    int kitItem = cmd.data.kit;
    int engineType = cmd.data.engine;
    int parameter = cmd.data.parameter;

    string name;
    EnvelopeParams* sectionType{nullptr};
    if (engineType == PART::engine::addSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Penvamplitude";
                sectionType = synth.part[npart]->kit[kitItem].adpars->GlobalPar.AmpEnvelope;
            break;
            case 1:
                name = "Penvfrequency";
                sectionType = synth.part[npart]->kit[kitItem].adpars->GlobalPar.FreqEnvelope;
            break;
            case 2:
                name = "Penvfilter";
                sectionType = synth.part[npart]->kit[kitItem].adpars->GlobalPar.FilterEnvelope;
            break;
        }
    }

    else if (engineType >= PART::engine::addVoice1)
    {
        switch (parameter)
        {
            case 0:
                name = "Penvamplitude";
                sectionType = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].AmpEnvelope;
            break;
            case 1:
                name = "Penvfrequency";
                sectionType = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].FreqEnvelope;
            break;
            case 2:
                name = "Penvfilter";
                sectionType = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].FilterEnvelope;
            break;
        }
    }

    else if (engineType == PART::engine::subSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Penvamplitude";
                sectionType = synth.part[npart]->kit[kitItem].subpars->AmpEnvelope;
            break;
            case 1:
                name = "Penvfrequency";
                sectionType = synth.part[npart]->kit[kitItem].subpars->FreqEnvelope;
            break;
            case 2:
                name = "Penvfilter";
                sectionType = synth.part[npart]->kit[kitItem].subpars->GlobalFilterEnvelope;
            break;
            case 3:
                name = "Penvbandwidth";
                sectionType = synth.part[npart]->kit[kitItem].subpars->BandWidthEnvelope;
            break;
        }
    }

    else if (engineType == PART::engine::padSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Penvamplitude";
                sectionType = synth.part[npart]->kit[kitItem].padpars->AmpEnvelope.get();
            break;
            case 1:
                name = "Penvfrequency";
                sectionType = synth.part[npart]->kit[kitItem].padpars->FreqEnvelope.get();
            break;
            case 2:
                name = "Penvfilter";
                sectionType = synth.part[npart]->kit[kitItem].padpars->FilterEnvelope.get();
            break;
        }
    }
    if (name.empty())
        return "";


    if (isLoad)
    {
        xml.enterbranch(name);
        sectionType->getfromXML(xml);
        xml.exitbranch();
    }
    else
    {
        xml.beginbranch(name);
        sectionType->add2XML(xml);
        xml.endbranch();
    }

    return name;
}


void UnifiedPresets::save()
{
    synth.getRuntime().xmlType = TOPLEVEL::XML::Presets;
    auto xml{std::make_unique<XMLwrapper>(synth, false)};
    string type{accessXML(*xml, false)};
    if (type.empty())
        synth.getRuntime().Log("Unrecognised preset type");
    else
    {
        string dirname;
        string name = synth.textMsgBuffer.fetch(cmd.data.miscmsg);
        if (name.empty())
        {
            dirname = file::localDir() + "/clipboard";
            if (file::createDir(dirname))
                synth.getRuntime().Log("Failed to open clipboard directory");
            else
                xml->saveXMLfile(dirname + "/section." + type + EXTEN::presets);
        }
        else
        {
            dirname = synth.getRuntime().presetsDirlist[synth.getRuntime().presetsRootID];
            xml->saveXMLfile(dirname + "/" + name + "." + type + EXTEN::presets);
        }
    }
}


void UnifiedPresets::load()
{
    synth.getRuntime().xmlType = TOPLEVEL::XML::Presets;
    string type{findPresetType()};
    auto xml{std::make_unique<XMLwrapper>(synth, false)};
    string name = synth.textMsgBuffer.fetch(cmd.data.miscmsg);
    string dirname;
    string prefix;

    if (name.empty())
    {
        dirname = file::localDir() + "/clipboard";
        if (file::createDir(dirname))
        {
            synth.getRuntime().Log("Failed to open clipboard directory");
        }
        else
        {
            prefix = dirname + "/section.";
        }
    }
    else
    {
        dirname = synth.getRuntime().presetsDirlist[synth.getRuntime().presetsRootID];
        prefix = dirname + "/" + name + ".";
    }
    string filename = prefix + type + EXTEN::presets;

    if (file::isRegularFile(prefix + type + EXTEN::presets) == 0)
    {
        synth.getRuntime().Log("Can't match " + filename + " here.");
        return;
    }

    xml->loadXMLfile(filename);
    accessXML(*xml, true);
}


void UnifiedPresets::remove()
{
    human = 0; // we need the extension this time.
    string type = findPresetType();
    string name = synth.textMsgBuffer.fetch(cmd.data.miscmsg);
    string dirname = synth.getRuntime().presetsDirlist[synth.getRuntime().presetsRootID];
    string filename = dirname + "/" + name + "." + type + EXTEN::presets;
    file::deleteFile(filename);
}
