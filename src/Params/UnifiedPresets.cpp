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

#include "Misc/XMLStore.h"
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
    if (type == TOPLEVEL::type::List && listFunction > 0)
    {
        string group{findPresetType()};
        if (listFunction == 2)
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
         * Skip this invocation without actual processing;
         * seemingly a command message is sent here redundantly...?
         */
        return findPresetType(); // listFunction != 2  ==> use human friendly id
    }

    // when command.value == 1 ==> use the technical id from presetgroups array
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
            if (listFunction == 0)
                load();
            else
                remove();
        }
    }
    return name;
}

/**
 * Access the `presetgroups` array (fixed definition in TextLists.cpp)
 * @note uses the hidden parameter listFunction to select which column to pick
 * @remark if listFunction = 2 we want to get the extension not the friendly name
 */
string UnifiedPresets::listpos(int count)  const
{
    uint fieldOffset{listFunction == 1? 1u : 0u};
    return presetgroups[count * 2 + fieldOffset];
}

string UnifiedPresets::findPresetType()
{
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
            name = listpos(16);//presetgroups[32+listFunction];//"Psubsyth";
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


string UnifiedPresets::accessXML(XMLStore& xml, bool isLoad)
{
    XMLtree xmlTop = xml.accessTop();

    if (kitItem >= EFFECT::type::none and kitItem < EFFECT::type::count
            and not (kitItem == EFFECT::type::dynFilter and insert == TOPLEVEL::insert::filterGroup)
       )                                    // ^^^^^^^  passed on to filterGroup rather
        return effectXML(xmlTop, isLoad);

    switch (insert)
    {
        case TOPLEVEL::insert::resonanceGroup:
            return resonanceXML(xmlTop, isLoad);

        case TOPLEVEL::insert::oscillatorGroup:
            return oscilXML(xmlTop, isLoad);

        case TOPLEVEL::insert::filterGroup:
            return filterXML(xmlTop, isLoad);

        case TOPLEVEL::insert::LFOgroup:
            return lfoXML(xmlTop, isLoad);

        case TOPLEVEL::insert::envelopeGroup:
            return envelopeXML(xmlTop, isLoad);

        default:
            return synthXML(xmlTop, isLoad);
    }
}


string UnifiedPresets::synthXML(XMLtree& xmlTop, bool isLoad)
{
    string name;

    if (engineType == PART::engine::addSynth)
    {
        name = "Padsyth";
        ADnoteParameters* addPars = synth.part[npart]->kit[kitItem].adpars;

        if (isLoad)
        {
            addPars->defaults();
            XMLtree xmlAddSynth = xmlTop.getElm(name);
            addPars->getfromXML(xmlAddSynth);
        }
        else
        {
            XMLtree xmlAddSynth = xmlTop.addElm(name);
            addPars->add2XML(xmlAddSynth);
        }
    }

    else if (engineType >= PART::engine::addVoice1)
    {
        name = "Padsythn";
        ADnoteParameters* addPars = synth.part[npart]->kit[kitItem].adpars;
        size_t voice = engineType - PART::engine::addVoice1;

        if (isLoad)
        {
            addPars->voiceDefaults(voice);
            XMLtree xmlVoice = xmlTop.getElm(name);
            addPars->getfromXML_voice(xmlVoice, voice);
        }
        else
        {
            XMLtree xmlVoice = xmlTop.addElm(name);
            addPars->add2XML_voice(xmlVoice, voice);
        }
    }

    else if (engineType == PART::engine::subSynth)
    {
        name = "Psubsyth";
        SUBnoteParameters* subPars = synth.part[npart]->kit[kitItem].subpars;

        if (isLoad)
        {
            subPars->defaults();
            XMLtree xmlSubSynth = xmlTop.getElm(name);
            subPars->getfromXML(xmlSubSynth);
        }
        else
        {
            XMLtree xmlSubSynth = xmlTop.addElm(name);
            subPars->add2XML(xmlSubSynth);
        }
    }

    else if (engineType == PART::engine::padSynth)
    {
        name = "Ppadsyth";
        PADnoteParameters* padPars = synth.part[npart]->kit[kitItem].padpars;

        if (isLoad)
        {
            padPars->defaults();
            XMLtree xmlPadSynth = xmlTop.getElm(name);
            padPars->getfromXML(xmlPadSynth);
        }
        else
        {
            XMLtree xmlPadSynth = xmlTop.addElm(name);
            padPars->add2XML(xmlPadSynth);
        }
    }

    return name;
}


string UnifiedPresets::effectXML(XMLtree& xmlTop, bool isLoad)
{
    EffectMgr* effect{nullptr};

    if (npart == TOPLEVEL::section::systemEffects)
    {
        effect = synth.sysefx[engineType];
    }
    else if (npart == TOPLEVEL::section::insertEffects)
    {
        effect = synth.insefx[engineType];
    }
    else
    {
        effect = synth.part[npart]->partefx[engineType];
    }
    string name{"Peffect"};

    if (isLoad)
    {
        effect->defaults();
        XMLtree xmlEffect = xmlTop.getElm(name);
        effect->getfromXML(xmlEffect);
        synth.pushEffectUpdate(npart);
    }
    else
    {
        XMLtree xmlEffect = xmlTop.addElm(name);
        effect->add2XML(xmlEffect);
    }
    return name;
}


string UnifiedPresets::resonanceXML(XMLtree& xmlTop, bool isLoad)
{
    string name{"Presonance"};

    Resonance* reson{nullptr};
    if (engineType == PART::engine::addSynth)
        reson = synth.part[npart]->kit[kitItem].adpars->GlobalPar.Reson;
    else
    if (engineType == PART::engine::padSynth)
        reson = synth.part[npart]->kit[kitItem].padpars->resonance.get();
    else
        return "";

    if (isLoad)
    {
        XMLtree xmlRes = xmlTop.getElm(name);
        reson->getfromXML(xmlRes);
    }
    else
    {
        XMLtree xmlRes = xmlTop.addElm(name);
        reson->add2XML(xmlRes);
    }

    return name;
}


string UnifiedPresets::oscilXML(XMLtree& xmlTop, bool isLoad)
{
    string name{"Poscilgen"};

    OscilParameters* oscPars{nullptr};

    if (engineType >= (PART::engine::addVoice1))
    {
        uint voiceID = engineType - PART::engine::addVoice1;
        // engine is encoded as: addSynth, subSynth, padSynth, addVoice1...+NUM_VOICES, addMod1....+NUM_VOICES
        if (engineType >= PART::engine::addMod1)
        {
            voiceID -= NUM_VOICES;
            oscPars = synth.part[npart]->kit[kitItem].adpars->VoicePar[voiceID].POscilFM;
        }
        else
        {
            oscPars = synth.part[npart]->kit[kitItem].adpars->VoicePar[voiceID].POscil;
        }
    }
    else if (engineType == PART::engine::padSynth)
    {
        oscPars = synth.part[npart]->kit[kitItem].padpars->POscil.get();
    }
    else
        return "";

    if (isLoad)
    {
        XMLtree xmlOscil = xmlTop.getElm(name);
        oscPars->getfromXML(xmlOscil);
    }
    else
    {
        XMLtree xmlOscil = xmlTop.addElm(name);
        oscPars->add2XML(xmlOscil);
    }

    return name;
}


string UnifiedPresets::filterXML(XMLtree& xmlTop, bool isLoad)
{
    FilterParams* filterPars{nullptr};

    // top level
    if (npart == TOPLEVEL::section::systemEffects)
    {
        filterPars = synth.sysefx[0]->filterpars;
    }
    else if (npart == TOPLEVEL::section::insertEffects)
    {
        filterPars = synth.insefx[0]->filterpars;
    }

    // part level
    else if (kitItem == EFFECT::type::dynFilter)
    {
        filterPars = synth.part[npart]->partefx[0]->filterpars;
    }
    else if (engineType == PART::engine::addSynth)
    {
        filterPars = synth.part[npart]->kit[kitItem].adpars->GlobalPar.GlobalFilter;
    }
    else if (engineType >= PART::engine::addVoice1)
    {
        filterPars = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].VoiceFilter;
    }
    else if (engineType == PART::engine::subSynth)
    {
        filterPars = synth.part[npart]->kit[kitItem].subpars->GlobalFilter;
    }
    else if (engineType == PART::engine::padSynth)
    {
        filterPars = synth.part[npart]->kit[kitItem].padpars->GlobalFilter.get();
    }
    else
        return "";

    string name{offset == UNUSED? "Pfilter" : "Pfiltern"};

    if (isLoad)
    {
        XMLtree xmlFilter = xmlTop.getElm(name);
        if (offset == UNUSED)
            filterPars->getfromXML(xmlFilter);
        else
            filterPars->getfromXML_vowel(xmlFilter, offset);
    }
    else
    {
        XMLtree xmlFilter = xmlTop.addElm(name);
        if (offset == UNUSED)
            filterPars->add2XML(xmlFilter);
        else
            filterPars->add2XML_vowel(xmlFilter, offset);
    }

    return name;
}


string UnifiedPresets::lfoXML(XMLtree& xmlTop, bool isLoad)
{
    string name;
    LFOParams* LFOpars{nullptr};

    if (engineType == PART::engine::addSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Plfoamplitude";
                LFOpars = synth.part[npart]->kit[kitItem].adpars->GlobalPar.AmpLfo;
            break;
            case 1:
                name = "Plfofrequency";
                LFOpars = synth.part[npart]->kit[kitItem].adpars->GlobalPar.FreqLfo;
            break;
            case 2:
                name = "Plfofilter";
                LFOpars = synth.part[npart]->kit[kitItem].adpars->GlobalPar.FilterLfo;
            break;
        }
    }
    else if (engineType >= PART::engine::addVoice1)
    {
        switch (parameter)
        {
            case 0:
                name = "Plfoamplitude";
                LFOpars = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].AmpLfo;
            break;
            case 1:
                name = "Plfofrequency";
                LFOpars = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].FreqLfo;
            break;
            case 2:
                name = "Plfofilter";
                LFOpars = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].FilterLfo;
            break;
        }
    }
    else if (engineType == PART::engine::padSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Plfoamplitude";
                LFOpars = synth.part[npart]->kit[kitItem].padpars->AmpLfo.get();
            break;
            case 1:
                name = "Plfofrequency";
                LFOpars = synth.part[npart]->kit[kitItem].padpars->FreqLfo.get();
            break;
            case 2:
                name = "Plfofilter";
                LFOpars = synth.part[npart]->kit[kitItem].padpars->FilterLfo.get();
            break;
        }
    }
    if (name.empty())
        return "";


    if (isLoad)
    {
        XMLtree xmlLFO = xmlTop.getElm(name);
        LFOpars->getfromXML(xmlLFO);
    }
    else
    {
        XMLtree xmlLFO = xmlTop.addElm(name);
        LFOpars->add2XML(xmlLFO);
    }

    return name;
}


string UnifiedPresets::envelopeXML(XMLtree& xmlTop, bool isLoad)
{
    string name;
    EnvelopeParams* envPars{nullptr};
    if (engineType == PART::engine::addSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Penvamplitude";
                envPars = synth.part[npart]->kit[kitItem].adpars->GlobalPar.AmpEnvelope;
            break;
            case 1:
                name = "Penvfrequency";
                envPars = synth.part[npart]->kit[kitItem].adpars->GlobalPar.FreqEnvelope;
            break;
            case 2:
                name = "Penvfilter";
                envPars = synth.part[npart]->kit[kitItem].adpars->GlobalPar.FilterEnvelope;
            break;
        }
    }

    else if (engineType >= PART::engine::addVoice1)
    {
        switch (parameter)
        {
            case 0:
                name = "Penvamplitude";
                envPars = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].AmpEnvelope;
            break;
            case 1:
                name = "Penvfrequency";
                envPars = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].FreqEnvelope;
            break;
            case 2:
                name = "Penvfilter";
                envPars = synth.part[npart]->kit[kitItem].adpars->VoicePar[engineType - PART::engine::addVoice1].FilterEnvelope;
            break;
        }
    }

    else if (engineType == PART::engine::subSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Penvamplitude";
                envPars = synth.part[npart]->kit[kitItem].subpars->AmpEnvelope;
            break;
            case 1:
                name = "Penvfrequency";
                envPars = synth.part[npart]->kit[kitItem].subpars->FreqEnvelope;
            break;
            case 2:
                name = "Penvfilter";
                envPars = synth.part[npart]->kit[kitItem].subpars->GlobalFilterEnvelope;
            break;
            case 3:
                name = "Penvbandwidth";
                envPars = synth.part[npart]->kit[kitItem].subpars->BandWidthEnvelope;
            break;
        }
    }

    else if (engineType == PART::engine::padSynth)
    {
        switch (parameter)
        {
            case 0:
                name = "Penvamplitude";
                envPars = synth.part[npart]->kit[kitItem].padpars->AmpEnvelope.get();
            break;
            case 1:
                name = "Penvfrequency";
                envPars = synth.part[npart]->kit[kitItem].padpars->FreqEnvelope.get();
            break;
            case 2:
                name = "Penvfilter";
                envPars = synth.part[npart]->kit[kitItem].padpars->FilterEnvelope.get();
            break;
        }
    }
    if (name.empty())
        return "";


    if (isLoad)
    {
        XMLtree xmlEnv = xmlTop.getElm(name);
        envPars->getfromXML(xmlEnv);
    }
    else
    {
        XMLtree xmlEnv = xmlTop.addElm(name);
        envPars->add2XML(xmlEnv);
    }

    return name;
}



void UnifiedPresets::save()
{
    auto& logger{synth.getRuntime().getLogger()};
    bool zynCompat = true;  // for some unclear reason preset/clipboard data is marked as Zyn compatible
                            // (this was discussed 4/2025 and we were not sure what to do about it)
    XMLStore xml{TOPLEVEL::XML::Presets, zynCompat};
    string type = accessXML(xml, false);
    if (type.empty())
        logger("Unrecognised preset type");
    else
    if (not xml)
        logger("no data retrieved; nothing to store.");
    else
    {
        string filename;
        string dirname;
        string name = synth.textMsgBuffer.fetch(mesgID);
        if (name.empty())
        {
            dirname = file::localDir() + "/clipboard";
            if (file::createDir(dirname))
                logger("Failed to open clipboard directory");
            else
                filename = dirname + "/section." + type + EXTEN::presets;
        }
        else
        {
            dirname = synth.getRuntime().presetsDirlist[synth.getRuntime().presetsRootID];
            filename = dirname + "/" + name + "." + type + EXTEN::presets;
        }
        if (not filename.empty())
            xml.saveXMLfile(filename, logger, synth.getRuntime().gzipCompression);
    }
}


void UnifiedPresets::load()
{
    string type{findPresetType()};
    string name = synth.textMsgBuffer.fetch(mesgID);
    string dirname;
    string prefix;
    auto& logger{synth.getRuntime().getLogger()};

    if (name.empty())
    {
        dirname = file::localDir() + "/clipboard";
        if (file::createDir(dirname))
        {
            logger("Failed to open clipboard directory");
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
        logger("Can't match " + filename + " here.");
        return;
    }

    XMLStore xml{filename, logger};
    postLoadCheck(xml,synth);
    if (xml)
        accessXML(xml, true);
    else
        logger("Warning: could not read/parse preset file \""+filename+"\"");
}


void UnifiedPresets::remove()
{
    listFunction = 0; // cause findPresetType() to pick the extension (not the description)
    string type = findPresetType();
    string name = synth.textMsgBuffer.fetch(mesgID);
    string dirname = synth.getRuntime().presetsDirlist[synth.getRuntime().presetsRootID];
    string filename = dirname + "/" + name + "." + type + EXTEN::presets;
    file::deleteFile(filename);
}
