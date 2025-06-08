/*
    Config.cpp - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2013, Nikita Zlobin
    Copyright 2014-2024, Will Godfrey & others

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

    This file is derivative of ZynAddSubFX original code.
*/

#include <sys/types.h>
#include <iostream>
#include <fenv.h>
#include <errno.h>
#include <cmath>
#include <array>
#include <string>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <cassert>
#include <memory>
#include <bitset>
#include <regex>

#if defined(JACK_SESSION)
#include <jack/session.h>
#endif

#include "Misc/XMLStore.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/NumericFuncs.h"
#include "Misc/FormatFuncs.h"
#include "Misc/TextMsgBuffer.h"
#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif
#include "ConfBuild.h"

using file::isRegularFile;
using file::createDir;
using file::copyDir;
using file::isDirectory;
using file::extendLocalPath;
using file::setExtension;
using file::renameFile;
using file::loadText;

using func::nearestPowerOf2;
using func::asString;
using func::string2int;
using func::string2uint;

using std::string;
using std::bitset;
using std::cout;
using std::cerr;
using std::endl;

namespace { // Implementation details...

    TextMsgBuffer& textMsgBuffer = TextMsgBuffer::instance();


    static std::regex VERSION_SYNTAX{R"~((\d+)(?:\.(\d+))?(?:\.(\d+))?)~", std::regex::optimize};

    VerInfo parseVersion(string const& spec)
    {
        std::smatch mat;
        if (std::regex_search(spec, mat, VERSION_SYNTAX))
            return VerInfo{                string2uint(mat[1])
                          ,mat[2].matched? string2uint(mat[2]) : 0
                          ,mat[3].matched? string2uint(mat[3]) : 0
                          };
        else
            return VerInfo{0};
    }
}

/**
 * Implementation: parse string with program version specification
 */
VerInfo::VerInfo(string const& spec)
    : VerInfo{parseVersion(spec)}
    { }



uchar panLaw = 1;

bool       Config::showSplash{true};
bool       Config::singlePath{false};
bool       Config::autoInstance{false};
bitset<32> Config::activeInstances{0};
int        Config::showCLIcontext{1};

string Config::globalJackSessionUuid = "";

const VerInfo Config::VER_YOSHI_CURR{YOSHIMI_VERSION};
const VerInfo Config::VER_ZYN_COMPAT{2,4,3};


Config::Config(SynthEngine& synthInstance)
    : synth{synthInstance}
    , isLV2{false}
    , isMultiFeed{false}
    , build_ID{BUILD_NUMBER}
    , loadedConfigVer{VER_YOSHI_CURR}
    , incompatibleZynFile{false}
    , runSynth{false}          // will be set by Instance::startUp()
    , finishedCLI{true}
    , isLittleEndian{true}
    , virKeybLayout{0}
    , audioEngine{DEFAULT_AUDIO}
    , engineChanged{false}
    , midiEngine{DEFAULT_MIDI}
    , midiChanged{false}
    , alsaMidiType{1}          // search
    , audioDevice{"default"}
    , midiDevice{"default"}
    , jackServer{"default"}
    , jackMidiDevice{"default"}
    , startJack{false}
    , connectJackaudio{true}
    , connectJackChanged{false}
    , alsaAudioDevice{"default"}
    , alsaMidiDevice{"default"}
    , loadDefaultState{false}
    , defaultStateName{}
    , defaultSession{}
    , configFile{}
    , paramsLoad{}
    , instrumentLoad{}
    , load2part{0}
    , midiLearnLoad{}
    , rootDefine{}
    , stateFile{}
    , guiThemeID{0}
    , guiTheme{}
    , remoteGuiTheme{}
    , restoreJackSession{false}
    , jackSessionFile{}
    , sessionStage{_SYS_::type::Normal}
    , Interpolation{0}
    , xmlType{0}
    , instrumentFormat{1}
    , enableProgChange{true}   // default will be inverted
    , toConsole{false}
    , consoleTextSize{12}
    , hideErrors{false}
    , showTimes{false}
    , logXMLheaders{false}
    , xmlmax{false}
    , gzipCompression{3}
    , enablePartReports{false}
    , samplerate{48000}
    , rateChanged{false}
    , buffersize{256}
    , bufferChanged{false}
    , oscilsize{512}
    , oscilChanged{false}
    , showGui{true}
    , storedGui{true}
    , guiChanged{false}
    , showCli{true}
    , storedCli{true}
    , cliChanged{false}
    , banksChecked{false}
    , panLaw{1}
    , configChanged{false}
    , handlePadSynthBuild{0}
    , rtprio{40}
    , midi_bank_root{0}        // 128 is used as 'disabled'
    , midi_bank_C{32}          // 128 is used as 'disabled'
    , midi_upper_voice_C{128}  // disabled
    , enableOmni{true}
    , enable_NRPN{true}
    , ignoreResetCCs{false}
    , monitorCCin{false}
    , showLearnedCC{true}
    , numAvailableParts{NUM_MIDI_CHANNELS}
    , currentPart{0}
    , currentBank{0}
    , currentRoot{0}
    , bankHighlight{false}
    , lastBankPart{UNUSED}
    , presetsRootID{0}
    , tempBank{0}
    , tempRoot{0}
#ifdef REPORT_NOTES_ON_OFF
    , noteOnSent{0}
    , noteOnSeen{0}
    , noteOffSent{0}
    , noteOffSeen{0}
#endif //REPORT_NOTES_ON_OFF
    , VUcount{0}
    , channelSwitchType{0}
    , channelSwitchCC{128}     // disabled
    , channelSwitchValue{0}
    , nrpnL{127}               // off
    , nrpnH{127}               // off
    , dataL{0xff}              // disabled
    , dataH{0x80}
    , nrpnActive{false}
    , vectordata{}
    , logList{}
    , manualFile{}
    , exitType{}
    , genTmp1{}
    , genTmp2{}
    , genTmp3{}
    , genTmp4{}
    , genMixl{}
    , genMixr{}
    , findManual_Thread{}
    , sigIntActive{0}
    , ladi1IntActive{0}
    , jsessionSave{0}
    , programcommand{"yoshimi"}
    , jackSessionDir{}
    , baseConfig{}
    , presetList{}
    , presetDir{}
    , logHandler{[this](string const& msg, char tostderr){ this->Log(msg,tostderr); }}
{
    std::cerr.precision(4);
}



void Config::init()
{
    if (isLV2) return; //skip further setup, which is irrelevant for LV2 plugin instance.

    switch (audioEngine)
    {
        case alsa_audio:
            audioDevice = string(alsaAudioDevice);
            break;

        case jack_audio:
            audioDevice = string(jackServer);
            break;
        case no_audio:
        default:
            audioDevice.clear();
            break;
    }
    if (audioDevice.empty())
        audioDevice = "default";
    switch (midiEngine)
    {
        case jack_midi:
            midiDevice = string(jackMidiDevice);
            break;

        case alsa_midi:
            midiDevice = string(alsaMidiDevice);
            break;

        case no_midi:
        default:
            midiDevice.clear();
            break;
    }
    oscilsize = nearestPowerOf2(oscilsize, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);
    buffersize = nearestPowerOf2(buffersize, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);

    if (!Config::globalJackSessionUuid.empty())
        jackSessionUuid = Config::globalJackSessionUuid;
}


/**
 * when starting a new instance without pre-existing instance config,
 * fill in some relevant base settings from the primary config.
 */
void Config::populateFromPrimary()
{
    assert (0 < synth.getUniqueId());
    Config& primary = instances().accessPrimaryConfig();

    // The following are actually base config values,
    // yet unfortunately they are duplicated in each config instance,
    // and thus undesirable default values can sometimes spread around,
    // since parts of the code blindly use the values from current Synth.
    handlePadSynthBuild = primary.handlePadSynthBuild;
    gzipCompression     = primary.gzipCompression;
    guideVersion        = primary.guideVersion;
    manualFile          = primary.manualFile;

    // The following are instance settings, yet still desirable to initialise
    // with the values from the primary engine instance in case a config is missing
    paramsLoad          = primary.paramsLoad;
    instrumentLoad      = primary.instrumentLoad;
    load2part           = primary.load2part;
    midiLearnLoad       = primary.midiLearnLoad;
    rootDefine          = primary.rootDefine;
    virKeybLayout       = primary.virKeybLayout;
    audioEngine         = primary.audioEngine;
    midiEngine          = primary.midiEngine;
    alsaMidiType        = primary.alsaMidiType;
    connectJackaudio    = primary.connectJackaudio;
    loadDefaultState    = primary.loadDefaultState;
    Interpolation       = primary.Interpolation;
//presetsDirlist                                        /////TODO shouldn't we populate these too? if yes -> use a STL container (e.g. std::array), which can be bulk copied
    instrumentFormat    = primary.instrumentFormat;
    enableProgChange    = primary.enableProgChange;
    toConsole           = primary.toConsole;
    consoleTextSize     = primary.consoleTextSize;
    hideErrors          = primary.hideErrors;
    showTimes           = primary.showTimes;
    logXMLheaders       = primary.logXMLheaders;
    xmlmax              = primary.xmlmax;
    samplerate          = primary.samplerate;
    buffersize          = primary.buffersize;
    oscilsize           = primary.oscilsize;
    panLaw              = primary.panLaw;
    midi_bank_root      = primary.midi_bank_root;
    midi_bank_C         = primary.midi_bank_C;
    midi_upper_voice_C  = primary.midi_upper_voice_C;
    enableOmni          = primary.enableOmni;
    enable_NRPN         = primary.enable_NRPN;
    ignoreResetCCs      = primary.ignoreResetCCs;
    monitorCCin         = primary.monitorCCin;
    showLearnedCC       = primary.showLearnedCC;
    bankHighlight       = primary.bankHighlight;
    presetsRootID       = primary.presetsRootID;
    channelSwitchType   = primary.channelSwitchType;
    channelSwitchCC     = primary.channelSwitchCC;
    channelSwitchValue  = primary.channelSwitchValue;
}


void Config::flushLog()
{
    for (auto& line : logList)
        cout << line << endl;
    logList.clear();
}


void *Config::_findManual(void *arg)
{
    assert(arg);
    static_cast<Config*>(arg)->findManual();
    return nullptr;
}


void Config::findManual()
{
    Log("finding manual");
    string currentV = string(YOSHIMI_VERSION);
    manualFile = findHtmlManual();
    guideVersion = loadText(manualFile);
    size_t pos = guideVersion.find(" ");
    if (pos != string::npos)
        guideVersion = guideVersion.substr(0, pos);
    Log("manual found");

    saveMasterConfig();
}


void Config::loadConfig()
{
    bool success = initFromPersistentConfig();
    if (not success)
    {
        string message = "Problems loading config. Using default values.";
        TextMsgBuffer::instance().push(message); // needed for CLI
        Log("\n\n" + message + "\n");
    }
}


void Config::buildConfigLocation()
{
    string location = file::configDir();
    string instanceID = isLV2? file::LV2_INSTANCE                 // LV2-plugin uses a fixed key for instance config
                             : asString(synth.getUniqueId());     // standalone-instances are keyed by Synth-ID

    defaultStateName = location + "/" + YOSHIMI;
    baseConfig       = location + "/" + YOSHIMI                    + EXTEN::config;
    configFile       = location + "/" + YOSHIMI + "-" + instanceID + EXTEN::instance;
    defaultSession   = location + "/" + YOSHIMI + "-" + instanceID + EXTEN::state;

    presetList = file::localDir() + "/presetDirs";
    presetDir  = file::localDir() + "/presets";
}

bool Config::initFromPersistentConfig()
{
    if (file::userHome() == "/tmp")
        Log ("Failed to find 'Home' directory - using tmp.\nSettings will be lost on computer shutdown.");
    if (file::localDir().empty())
    {
        Log("Failed to create local yoshimi directory.");
        return false;
    }
    if (file::configDir().empty())
    {
        Log("Failed to create config directory '" + file::userHome() + "'");
        return false;
    }

    buildConfigLocation();

    if (synth.getUniqueId() == 0 && sessionStage != _SYS_::type::RestoreConf)
    {
        if (not isDirectory(presetDir))
        {// only ever want to do this once
            if (createDir(presetDir))
            {
                Log("Failed to create presets directory '" + presetDir + "'");
            }
            else
            {
                defaultPresets();
                int i = 1;
                while (!presetsDirlist[i].empty())
                {
                    copyDir(presetsDirlist[i], presetDir, 1);
                    ++i;
                }
            }
        }
        if (not isDirectory(file::localDir() + "/found/"))
        {// only ever want to do this once
            if (createDir(file::localDir() + "/found/"))
                Log("Failed to create root directory for local banks");
        }

        // conversion for old location
        string newInstance0 = configFile;
        string oldAllConfig = defaultStateName + EXTEN::state;
        if (isRegularFile(baseConfig) && !isRegularFile(newInstance0), 0)
        {
            file::copyFile(baseConfig, newInstance0, 0);
            Log("Reorganising config files.");
            if (isRegularFile(oldAllConfig))
            {
                if (not isRegularFile(defaultSession))
                {
                    renameFile(oldAllConfig, defaultSession);
                    Log("Moving default state file.");
                }
            }
        }
    }

    bool success{true};
    if (not isRegularFile(baseConfig))
    {
        Log("Basic configuration " + baseConfig + " not found, will use default settings.");
        saveMasterConfig(); // generates a pristine "yoshimi.config"
        defaultPresets();
    }
    else
    {
        // load baseConfig (always from the primary file)
        XMLStore xml{baseConfig, getLogger()};
        verifyVersion(xml);
        success = not xml.empty();
        if (success)
            success = extractBaseParameters(xml);    // note: we want correct base values even in a secondary config instance
        else
            Log("Config: failed to load base config");
    }

    if (not isRegularFile(configFile))
    {
        if (0 < synth.getUniqueId())
        {
            populateFromPrimary();
            Log("Config: create new file \""+configFile+"\" with initial values from primary Synth instance.");
        }
        else
        {
            Log("Configuration \""+configFile+"\" not found; will use default settings.");
        }
        saveInstanceConfig(); // generates a new "yoshimi-#.instance"
    }
    else
    if (success)
    {
        // load instance configuration values
        XMLStore xml{configFile, getLogger()};
        verifyVersion(xml);
        success = not xml.empty();
        if (success)
            success = extractConfigData(xml);
        else
            Log("Config: failed to load instance config");
    }


    if (sessionStage == _SYS_::type::RestoreConf)
        return true;

    if (sessionStage != _SYS_::type::Normal)
    {
        XMLStore xml{stateFile, getLogger()};
        verifyVersion(xml);
        success = not xml.empty();
        if (success)
        {
            if (sessionStage == _SYS_::type::StartupFirst)
                sessionStage = _SYS_::type::StartupSecond;
            else
            if (sessionStage == _SYS_::type::JackFirst)
                sessionStage = _SYS_::type::JackSecond;
            success = extractConfigData(xml);
        }
        else
            Log("Config: failed to load instance config");
    }
    if (success)
        loadPresetsList();

    if (success && synth.getUniqueId() == 0)
    {
        // find user guide
        bool man_ok = false;
        string currentV = string(YOSHIMI_VERSION);
        size_t pos = currentV.find(" ");
        if (pos != string::npos)
            currentV = currentV.substr(0,pos);
        if (currentV == guideVersion && isRegularFile(manualFile))
        {
            man_ok = true;
        }

        if (!man_ok)
        {
            startThread(&findManual_Thread, _findManual, this, false, 0, "CFG");
        }
    }
    return success;
}


void Config::initBaseConfig(XMLStore& xml)
{
    if (xml.meta.type == TOPLEVEL::XML::MasterConfig)
    {
        XMLtree base = xml.addElm("BASE_PARAMETERS");
            base.addPar_bool("enable_gui"           , storedGui);
            base.addPar_bool("enable_splash"        , showSplash);
            base.addPar_bool("enable_CLI"           , storedCli);
            base.addPar_int ("show_CLI_context"     , showCLIcontext);
            base.addPar_bool("enable_single_master" , singlePath);
            base.addPar_bool("enable_auto_instance" , autoInstance);
            base.addPar_uint("handle_padsynth_build", handlePadSynthBuild);
            base.addPar_int ("gzip_compression"     , gzipCompression);
            base.addPar_bool("enable_part_reports" , enablePartReports);
            base.addPar_bool("banks_checked"        , banksChecked);
            base.addPar_uint("active_instances"     , activeInstances.to_ulong());
            base.addPar_str ("guide_version"        , guideVersion);
            base.addPar_str ("manual"               , manualFile);
    }
    else
    if (xml.meta.type <= TOPLEVEL::XML::Scale)
    {
        XMLtree base = xml.addElm("BASE_PARAMETERS");
            base.addPar_int("max_midi_parts"        , NUM_MIDI_CHANNELS);
            base.addPar_int("max_kit_items_per_instrument" , NUM_KIT_ITEMS);
            base.addPar_int("max_system_effects"    , NUM_SYS_EFX);
            base.addPar_int("max_insertion_effects" , NUM_INS_EFX);
            base.addPar_int("max_instrument_effects", NUM_PART_EFX);
            base.addPar_int("max_addsynth_voices"   , NUM_VOICES);
    }
}


void Config::addConfigXML(XMLStore& xml)
{
    XMLtree conf = xml.addElm("CONFIGURATION");
    conf.addPar_int ("defaultState", loadDefaultState);

    conf.addPar_int ("sound_buffer_size"      , buffersize);
    conf.addPar_int ("oscil_size"             , oscilsize);
    conf.addPar_bool("reports_destination"    , toConsole);
    conf.addPar_int ("console_text_size"      , consoleTextSize);
    conf.addPar_int ("interpolation"          , Interpolation);
    conf.addPar_int ("virtual_keyboard_layout", virKeybLayout + 1);
    conf.addPar_int ("saved_instrument_format", instrumentFormat);
    conf.addPar_bool("hide_system_errors"     , hideErrors);
    conf.addPar_bool("report_load_times"      , showTimes);
    conf.addPar_bool("report_XMLheaders"      , logXMLheaders);
    conf.addPar_bool("full_parameters"        , xmlmax);

    conf.addPar_bool("bank_highlight"         , bankHighlight);
    conf.addPar_int ("presetsCurrentRootID"   , presetsRootID);

    conf.addPar_int ("audio_engine"           , audioEngine);
    conf.addPar_int ("midi_engine"            , midiEngine);

    conf.addPar_str ("linux_jack_server"      , jackServer);
    conf.addPar_str ("linux_jack_midi_dev"    , jackMidiDevice);
    conf.addPar_bool("connect_jack_audio"     , connectJackaudio);

    conf.addPar_int ("alsa_midi_type"         , alsaMidiType);
    conf.addPar_str ("linux_alsa_audio_dev"   , alsaAudioDevice);
    conf.addPar_str ("linux_alsa_midi_dev"    , alsaMidiDevice);
    conf.addPar_int ("sample_rate"            , samplerate);

    conf.addPar_int ("midi_bank_root"         , midi_bank_root);
    conf.addPar_int ("midi_bank_C"            , midi_bank_C);
    conf.addPar_int ("midi_upper_voice_C"     , midi_upper_voice_C);
    conf.addPar_int ("ignore_program_change"  , (not enableProgChange));
    conf.addPar_int ("enable_part_on_voice_load", 1); // for backward compatibility
    conf.addPar_bool("enable_omni_change"     , enableOmni);
    conf.addPar_bool("enable_incoming_NRPNs"  , enable_NRPN);
    conf.addPar_bool("ignore_reset_all_CCs"   , ignoreResetCCs);
    conf.addPar_bool("monitor-incoming_CCs"   , monitorCCin);
    conf.addPar_bool("open_editor_on_learned_CC",showLearnedCC);

    conf.addPar_int ("root_current_ID"        , synth.ReadBankRoot());
    conf.addPar_int ("bank_current_ID"        , synth.ReadBank());
}


/**
 * This routine only stores settings that the user has directly changed
 * and not those changed via CLI startup parameters, nor changes made
 * by loading sessions etc.
 *
 * It loads the previously saved config into an array so it doesn't
 * disrupt the complete config currently in place. It then overwrites
 * just the parameter the user changed, and re-saves everything
 * including system generated entries.
 *
 * Text entries are handled via textMsgBuffer, so only a single array
 * type is needed, simplifying the code.
 *
 * Some assumptions are made based on the fact the parameters must be
 * in the correct range as they otherwise couldn't have been created.
 */
bool Config::updateConfig(int configKey, int value)
{
    buildConfigLocation();

    using Cfg = CONFIG::control;
    if (configKey <= Cfg::XMLcompressionLevel)
    {// handling base config
        int baseConfigData[Cfg::XMLcompressionLevel+1];
        auto par = [&](Cfg key) -> int& { return baseConfigData[key]; };

        XMLStore xml{baseConfig, getLogger()};
        if (xml.empty())
            Log("updateConfig: failed to load base config from \""+baseConfig+"\".");
        else
        {
            XMLtree xmlBase = xml.getElm("BASE_PARAMETERS");
            if (xmlBase.empty())
                Log("updateConfig: no <BASE_PARAMETERS> in XML file \""+baseConfig+"\".");
            else
            {
                par(Cfg::enableGUI          ) = xmlBase.getPar_bool("enable_gui",true);
                par(Cfg::showSplash         ) = xmlBase.getPar_bool("enable_splash",true);
                par(Cfg::enableCLI          ) = xmlBase.getPar_bool("enable_CLI",true);
                par(Cfg::exposeStatus       ) = xmlBase.getPar_int ("show_CLI_context",1,0,2);
                par(Cfg::enableSinglePath   ) = xmlBase.getPar_bool("enable_single_master",false);
                par(Cfg::enableAutoInstance ) = xmlBase.getPar_bool("enable_auto_instance",false);
                par(Cfg::handlePadSynthBuild) = xmlBase.getPar_uint("handle_padsynth_build",1,0,2);
                par(Cfg::XMLcompressionLevel) = xmlBase.getPar_int ("gzip_compression",3,0,9);
                par(Cfg::enablePartReports  ) = xmlBase.getPar_bool("enable_part_reports",false);
                par(Cfg::banksChecked       ) = xmlBase.getPar_bool("banks_checked",false);

                // Alter the specific config value given
                par(Cfg(configKey)) = value;

                // Write back the consolidated base config
                XMLStore newXml{TOPLEVEL::XML::MasterConfig};
                XMLtree xmlBase = newXml.addElm("BASE_PARAMETERS");
                xmlBase.addPar_bool("enable_gui"           , par(Cfg::enableGUI));
                xmlBase.addPar_bool("enable_splash"        , par(Cfg::showSplash));
                xmlBase.addPar_bool("enable_CLI"           , par(Cfg::enableCLI));
                xmlBase.addPar_int ("show_CLI_context"     , par(Cfg::exposeStatus));
                xmlBase.addPar_bool("enable_single_master" , par(Cfg::enableSinglePath));
                xmlBase.addPar_bool("enable_auto_instance" , par(Cfg::enableAutoInstance));
                xmlBase.addPar_uint("handle_padsynth_build", par(Cfg::handlePadSynthBuild));
                xmlBase.addPar_int ("gzip_compression"     , par(Cfg::XMLcompressionLevel));
                xmlBase.addPar_int ("enable_part_reports" , par(Cfg::enablePartReports));
                xmlBase.addPar_bool("banks_checked"        , par(Cfg::banksChecked));

                // the following are system defined;
                xmlBase.addPar_uint("active_instances", activeInstances.to_ulong());
                xmlBase.addPar_str ("guide_version"   , guideVersion);
                xmlBase.addPar_str ("manual"          , manualFile);

                if (newXml.saveXMLfile(baseConfig, getLogger(), par(Cfg::XMLcompressionLevel)))
                    return true;
                else
                    Log("updateConfig: failed to write updated base config to \""+baseConfig+"\".");
            }
        }
    }
    else
    {// handling current instance config
        const int offset      = Cfg::defaultStateStart;
        const int cntSettings = Cfg::historyLock - offset;
        int instanceConfigData[cntSettings];
        // define a lambda as shorthand notation for the following manipulations
        auto par = [&](Cfg key) -> int& { return instanceConfigData[key - offset]; };

        XMLStore xml{configFile, getLogger()};
        if (xml.empty())
            Log("updateConfig: failed to load instance config from \""+configFile+"\".");
        else
        {
            XMLtree xmlConf = xml.getElm("CONFIGURATION");
            if (xmlConf.empty())
                Log("updateConfig: no <CONFIGURATION> in XML file \""+configFile+"\".");
            else
            {
                par(Cfg::defaultStateStart      ) = xmlConf.getPar_int ("defaultState", 0, 0, 1);
                par(Cfg::bufferSize             ) = xmlConf.getPar_int ("sound_buffer_size", 0, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);
                par(Cfg::oscillatorSize         ) = xmlConf.getPar_int ("oscil_size", 0, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);
                par(Cfg::reportsDestination     ) = xmlConf.getPar_bool("reports_destination", false);
                par(Cfg::logTextSize            ) = xmlConf.getPar_int ("console_text_size", 0, 11, 100);
                par(Cfg::padSynthInterpolation  ) = xmlConf.getPar_int ("interpolation", 0, 0, 1);
                par(Cfg::virtualKeyboardLayout  ) = xmlConf.getPar_int ("virtual_keyboard_layout", 0, 1, 6) - 1;
                par(Cfg::savedInstrumentFormat  ) = xmlConf.getPar_int ("saved_instrument_format",0, 1, 3);
                par(Cfg::hideNonFatalErrors     ) = xmlConf.getPar_bool("hide_system_errors", false);
                par(Cfg::logInstrumentLoadTimes ) = xmlConf.getPar_bool("report_load_times", false);
                par(Cfg::logXMLheaders          ) = xmlConf.getPar_bool("report_XMLheaders", false);
                par(Cfg::saveAllXMLdata         ) = xmlConf.getPar_bool("full_parameters", xmlmax);
                par(Cfg::enableHighlight        ) = xmlConf.getPar_bool("bank_highlight", bankHighlight);
                par(Cfg::jackMidiSource         ) =  textMsgBuffer.push(xmlConf.getPar_str("linux_jack_midi_dev"));// string
                par(Cfg::jackServer             ) =  textMsgBuffer.push(xmlConf.getPar_str("linux_jack_server"));// string
                par(Cfg::jackAutoConnectAudio   ) = xmlConf.getPar_bool("connect_jack_audio", connectJackaudio);
                par(Cfg::alsaMidiSource         ) =  textMsgBuffer.push(xmlConf.getPar_str("linux_alsa_midi_dev"));// string
                par(Cfg::alsaMidiType           ) = xmlConf.getPar_int ("alsa_midi_type", 0, 0, 2);
                par(Cfg::alsaAudioDevice        ) =  textMsgBuffer.push(xmlConf.getPar_str("linux_alsa_audio_dev"));// string
                par(Cfg::alsaSampleRate         ) = xmlConf.getPar_int ("sample_rate", samplerate, 44100, 192000);
                par(Cfg::readAudio              ) = audio_driver(xmlConf.getPar_int("audio_engine", 0, no_audio, alsa_audio));
                par(Cfg::readMIDI               ) =  midi_driver(xmlConf.getPar_int("midi_engine", 0, no_midi, alsa_midi));
//              par(Cfg::addPresetRootDir       ) = // string NOT stored
//              par(Cfg::removePresetRootDir    ) = // returns string NOT used
                par(Cfg::currentPresetRoot      ) = xmlConf.getPar_int ("presetsCurrentRootID", 0, 0, MAX_PRESETS);
                par(Cfg::bankRootCC             ) = xmlConf.getPar_int ("midi_bank_root", 0, 0, 128);
                par(Cfg::bankCC                 ) = xmlConf.getPar_int ("midi_bank_C", midi_bank_C, 0, 128);
                par(Cfg::extendedProgramChangeCC) = xmlConf.getPar_int ("midi_upper_voice_C", 0, 0, 128);
                par(Cfg::enableProgramChange    ) = 1 - xmlConf.getPar_int("ignore_program_change", 0, 0, 1); // inverted for Zyn compatibility
                par(Cfg::ignoreResetAllCCs      ) = xmlConf.getPar_bool("ignore_reset_all_CCs", ignoreResetCCs);
                par(Cfg::logIncomingCCs         ) = xmlConf.getPar_bool("monitor-incoming_CCs", monitorCCin);
                par(Cfg::showLearnEditor        ) = xmlConf.getPar_bool("open_editor_on_learned_CC", showLearnedCC);
                par(Cfg::enableOmni             ) = xmlConf.getPar_bool("enable_omni_change", enableOmni);
                par(Cfg::enableNRPNs            ) = xmlConf.getPar_bool("enable_incoming_NRPNs", enable_NRPN);
//              par(Cfg::saveCurrentConfig      ) = // return string (dummy)

                // Alter the specific config value given
                par(Cfg(configKey)) = value;

                // Write back the consolidated instance config
                XMLStore newXml{TOPLEVEL::XML::Config};
                XMLtree xmlConf = newXml.addElm("CONFIGURATION");
                xmlConf.addPar_int ("defaultState"             , par(Cfg::defaultStateStart));
                xmlConf.addPar_int ("sound_buffer_size"        , par(Cfg::bufferSize));
                xmlConf.addPar_int ("oscil_size"               , par(Cfg::oscillatorSize));
                xmlConf.addPar_bool("reports_destination"      , par(Cfg::reportsDestination));
                xmlConf.addPar_int ("console_text_size"        , par(Cfg::logTextSize));
                xmlConf.addPar_int ("interpolation"            , par(Cfg::padSynthInterpolation));
                xmlConf.addPar_int ("virtual_keyboard_layout"  , par(Cfg::virtualKeyboardLayout) + 1);
                xmlConf.addPar_int ("saved_instrument_format"  , par(Cfg::savedInstrumentFormat));
                xmlConf.addPar_bool("hide_system_errors"       , par(Cfg::hideNonFatalErrors));
                xmlConf.addPar_bool("report_load_times"        , par(Cfg::logInstrumentLoadTimes));
                xmlConf.addPar_bool("report_XMLheaders"        , par(Cfg::logXMLheaders));
                xmlConf.addPar_bool("full_parameters"          , par(Cfg::saveAllXMLdata));
                xmlConf.addPar_bool("bank_highlight"           , par(Cfg::enableHighlight));
                xmlConf.addPar_int ("presetsCurrentRootID"     , par(Cfg::currentPresetRoot));
                xmlConf.addPar_int ("audio_engine"             , par(Cfg::readAudio));
                xmlConf.addPar_int ("midi_engine"              , par(Cfg::readMIDI));
                xmlConf.addPar_str ("linux_jack_server"        , textMsgBuffer.fetch(par(Cfg::jackServer)));
                xmlConf.addPar_str ("linux_jack_midi_dev"      , textMsgBuffer.fetch(par(Cfg::jackMidiSource)));
                xmlConf.addPar_bool("connect_jack_audio"       , par(Cfg::jackAutoConnectAudio));
                xmlConf.addPar_int ("alsa_midi_type"           , par(Cfg::alsaMidiType));
                xmlConf.addPar_str ("linux_alsa_audio_dev"     , textMsgBuffer.fetch(par(Cfg::alsaAudioDevice)));
                xmlConf.addPar_str ("linux_alsa_midi_dev"      , textMsgBuffer.fetch(par(Cfg::alsaMidiSource)));
                xmlConf.addPar_int ("sample_rate"              , par(Cfg::alsaSampleRate));
                xmlConf.addPar_int ("midi_bank_root"           , par(Cfg::bankRootCC));
                xmlConf.addPar_int ("midi_bank_C"              , par(Cfg::bankCC));
                xmlConf.addPar_int ("midi_upper_voice_C"       , par(Cfg::extendedProgramChangeCC));
                xmlConf.addPar_int ("ignore_program_change"    , (1 - par(Cfg::enableProgramChange)));
                xmlConf.addPar_int ("enable_part_on_voice_load", 1); // for backward compatibility
                xmlConf.addPar_bool("enable_omni_change"       , par(Cfg::enableOmni));
                xmlConf.addPar_bool("enable_incoming_NRPNs"    , par(Cfg::enableNRPNs));
                xmlConf.addPar_bool("ignore_reset_all_CCs"     , par(Cfg::ignoreResetAllCCs));
                xmlConf.addPar_bool("monitor-incoming_CCs"     , par(Cfg::logIncomingCCs));
                xmlConf.addPar_bool("open_editor_on_learned_CC",par(Cfg::showLearnEditor));

                xmlConf.addPar_int("root_current_ID", currentRoot); // always store the current root
                xmlConf.addPar_int("bank_current_ID", currentBank); // always store the current bank

                if (newXml.saveXMLfile(configFile, getLogger(), gzipCompression))
                    return true;
                else
                    Log("updateConfig: failed to write updated instance config to \""+configFile+"\".");
            }
        }
    }
    return false;
}


bool Config::extractBaseParameters(XMLStore& xml)
{
    XMLtree basePars = xml.getElm("BASE_PARAMETERS");
    if (not basePars)
    {
        Log("extractConfigData, no <BASE_PARAMETERS> branch");
        return false;
    }

    storedGui  = basePars.getPar_bool("enable_gui", showGui);
    if (not guiChanged)
        showGui = storedGui;

    showSplash = basePars.getPar_bool("enable_splash", showSplash);

    storedCli  = basePars.getPar_bool("enable_CLI", showCli);
    if (not cliChanged)
        showCli = storedCli;
    showCLIcontext  = basePars.getPar_int("show_CLI_context", 1, 0, 2);

    singlePath   = basePars.getPar_bool("enable_single_master", singlePath);
    autoInstance = basePars.getPar_bool("enable_auto_instance", autoInstance);
    if (autoInstance)
        activeInstances = bitset<32>{basePars.getPar_uint("active_instances", 0)};
    handlePadSynthBuild = basePars.getPar_uint("handle_padsynth_build", 1, 0, 2);  // 0 = blocking/muted, 1 = background thread (=default), 2 = auto-Apply on param change
    gzipCompression   = basePars.getPar_int("gzip_compression", gzipCompression, 0, 9);
    enablePartReports = basePars.getPar_bool("enable_part_reports",enablePartReports);
    banksChecked      = basePars.getPar_bool("banks_checked", banksChecked);
    guideVersion      = basePars.getPar_str("guide_version");
    manualFile        = basePars.getPar_str("manual");

    migrateLegacyPresetsList(basePars);
    return true;
}


bool Config::extractConfigData(XMLStore& xml)
{
    XMLtree conf = xml.getElm("CONFIGURATION");
    if (not conf)
    {
        Log("extractConfigData, no CONFIGURATION branch");
        Log("Running with defaults");
        return true;
    }
    /*
     * default state setting must be checked first
     * as we need to abort then and fetch an explicit default-state instead
     */
    if (sessionStage == _SYS_::type::Normal)
    {
        loadDefaultState = bool(conf.getPar_int("defaultState", loadDefaultState, 0, 1));
        if (loadDefaultState)
        {
            configChanged = true;
            sessionStage = _SYS_::type::Default;
            stateFile = defaultSession;
            Log("Loading default state");
            return true;
        }
    }

    if (sessionStage != _SYS_::type::InProgram)
    {

        if (!bufferChanged)
            buffersize = conf.getPar_int("sound_buffer_size"   , buffersize, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);
        if (!oscilChanged)
            oscilsize = conf.getPar_int ("oscil_size"          , oscilsize, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);
        toConsole     = conf.getPar_bool("reports_destination" , toConsole);
        consoleTextSize=conf.getPar_int ("console_text_size"   , consoleTextSize, 11, 100);
        Interpolation = conf.getPar_int ("interpolation"       , Interpolation,    0, 1);
        virKeybLayout = conf.getPar_int ("virtual_keyboard_layout", virKeybLayout,    1, 6) - 1;
        instrumentFormat=conf.getPar_int("saved_instrument_format", instrumentFormat, 1, 3);
        hideErrors    = conf.getPar_bool("hide_system_errors"  , hideErrors);
        showTimes     = conf.getPar_bool("report_load_times"   , showTimes);
        logXMLheaders = conf.getPar_bool("report_XMLheaders"   , logXMLheaders);
        xmlmax        = conf.getPar_bool("full_parameters"     , true);                        // defensive fall-back for migration

        bankHighlight = conf.getPar_bool("bank_highlight"      , bankHighlight);
        presetsRootID = conf.getPar_int ("presetsCurrentRootID", presetsRootID, 0, MAX_PRESETS);


        // engines
        if (!engineChanged)
            audioEngine = (audio_driver)conf.getPar_int("audio_engine", audioEngine, no_audio, alsa_audio);
        if (!midiChanged)
            midiEngine = (midi_driver)conf.getPar_int("midi_engine", midiEngine, no_midi, alsa_midi);
        alsaMidiType    = conf.getPar_int ("alsa_midi_type", 0, 0, 2);

        // jack settings
        jackServer      = conf.getPar_str ("linux_jack_server");
        jackMidiDevice  = conf.getPar_str ("linux_jack_midi_dev");
        if (!connectJackChanged)
            connectJackaudio = conf.getPar_bool("connect_jack_audio", connectJackaudio);

        // alsa settings
        alsaAudioDevice = conf.getPar_str ("linux_alsa_audio_dev");
        alsaMidiDevice  = conf.getPar_str ("linux_alsa_midi_dev");
        if (!rateChanged)
            samplerate  = conf.getPar_int ("sample_rate", samplerate, 44100, 192000);

        // midi options
        midi_bank_root   = conf.getPar_int ("midi_bank_root"           , midi_bank_root,     0, 128);
        midi_bank_C      = conf.getPar_int ("midi_bank_C"              , midi_bank_C,        0, 128);
        midi_upper_voice_C=conf.getPar_int ("midi_upper_voice_C"       , midi_upper_voice_C, 0, 128);
        enableProgChange = not conf.getPar_int("ignore_program_change" , enableProgChange,   0, 1); // inverted for Zyn compatibility
        enableOmni       = conf.getPar_bool("enable_omni_change"       , enableOmni);
        enable_NRPN      = conf.getPar_bool("enable_incoming_NRPNs"    , enable_NRPN);
        ignoreResetCCs   = conf.getPar_bool("ignore_reset_all_CCs"     , ignoreResetCCs);
        monitorCCin      = conf.getPar_bool("monitor-incoming_CCs"     , monitorCCin);
        showLearnedCC    = conf.getPar_bool("open_editor_on_learned_CC", showLearnedCC);
    }
    if (tempRoot == 0)
        tempRoot = conf.getPar_int("root_current_ID", 0, 0, 127);

    if (tempBank == 0)
        tempBank = conf.getPar_int("bank_current_ID", 0, 0, 127);
    return true;
}


namespace {
    string render(VerInfo const& ver)
    {
        return "v"
             + asString(ver.maj)
             + "."
             + asString(ver.min)
             + (ver.rev? "."+asString(ver.rev)
                       : "")
             ;
    }
}


/** @remark to simplify invoking the version check after loading XML */
void postLoadCheck(XMLStore const& xml, SynthEngine& synth)
{
    synth.getRuntime().verifyVersion(xml);
}

/**
 * Evaluate Metadata to ensure version compatibility.
 * Generate diagnostic and warnings and set compatibility flags.
 * @note this function should be invoked explicitly,
 *       whenever some XML file has been loaded.
 */
void Config::verifyVersion(XMLStore const& xml)
{
    if (not xml.meta.isValid())
        Log("XML: no valid data format found in file.", _SYS_::LogNotSerious);
    else
    if (xml.meta.isZynCompat()
        and not xml.meta.yoshimiVer        // file was indeed written by ZynAddSubFX
        and VER_ZYN_COMPAT < xml.meta.zynVer)
    {
        this->incompatibleZynFile |= true;
        Log("XML: found incompatible ZynAddSubFX version "
           +asString(xml.meta.zynVer.maj) +"."
           +asString(xml.meta.zynVer.min)
           , _SYS_::LogNotSerious);
    }
    else
    if (xml.meta.yoshimiVer     // it's a Yoshimi config file
        and (  xml.meta.type == TOPLEVEL::XML::MasterConfig
            or xml.meta.type == TOPLEVEL::XML::Config))
    {
        if (not is_compatible(xml.meta.yoshimiVer))
            loadedConfigVer.forceReset(xml.meta.yoshimiVer);
    }

    if (logXMLheaders)
    {
        string text{"XML("};
        if (not xml.meta.isValid())
            text += "empty/invalid metadata).";
        else
        {
            text += renderXmlType(xml.meta.type) +") ";
            if (xml.meta.zynVer and not xml.meta.yoshimiVer)
                text += "ZynAddSubFX format " + render(xml.meta.zynVer);
                     //  XML with Zyn doctype, presumably written by ZynAddSubFX
            else
            if (xml.meta.isZynCompat())
                text += "ZynAddSubFX compatible "
                      + render(xml.meta.zynVer)
                      + " by Yoshimi "
                      + render(xml.meta.yoshimiVer);
                      // XML with ZynAddSubFX doctype, yet written by Yoshimi
            else
                text += "YoshimiFormat " + render(xml.meta.yoshimiVer);
        }

        Log(text);
    }
}

bool Config::is_compatible (VerInfo const& ver)
{
    if (is_equivalent(ver, VER_YOSHI_CURR))
        return true;                                 // only revision differs => compatible
    if (ver.maj == VER_YOSHI_CURR.maj)
        return not (VER_YOSHI_CURR.min < ver.min);   // same major: silently accept any lower minor
    else
     return false;                                   // else mark any differing version as incompatible
}

/**
 * Perform migration tasks in case an incompatible configuration is detected.
 * @remark basically it is sufficient just to re-save the current config,
 *         since our code for loading XML typically performs any necessary
 *         data migration on the spot.
 * @warning only to be called on the primary instance, and after initialisation
 *         and loading of config, history and banks is complete.
 */
void Config::maybeMigrateConfig()
{
    if (is_compatible(loadedConfigVer)) return;

    if (loadedConfigVer < VER_YOSHI_CURR)
    {// loadedConfig is from an earlier version => safe to migrate
        saveMasterConfig();
        saveInstanceConfig();
        Log("\n"
            "\n+++++++++----------------------------++"
            "\nMigration of Config "
           +render(loadedConfigVer)
           +" --> "
           +render(VER_YOSHI_CURR)
           +"\n+++++++++----------------------------++"
            "\n"
           );
    }
 // NOTE: never automatically re-save config written by a never version than this codebase,
 //       since doing so may potentially discard additional settings present in the loaded config.
}


bool Config::saveMasterConfig()
{
    XMLStore xml{TOPLEVEL::XML::MasterConfig};
    initBaseConfig(xml);

    bool success = xml and xml.saveXMLfile(baseConfig, getLogger(), gzipCompression);
    if (success)
        configChanged = false;
    else
        Log("Failed to save base config to \""+baseConfig+"\"", _SYS_::LogNotSerious);
    return success;
}

bool Config::saveInstanceConfig()
{
    XMLStore xml{TOPLEVEL::XML::Config};
    addConfigXML(xml);

    bool success = xml and xml.saveXMLfile(configFile, getLogger(), gzipCompression);
    if (success)
        configChanged = false;
    else
        Log("Failed to save instance config to \""+configFile+"\"", _SYS_::LogNotSerious);
    return success;
}



/**
 * Extract current instance config and complete patch state from the engine,
 * encode it as XML and write to a _state file_
 */
bool Config::saveSessionData(string sessionfile)
{
    sessionfile = setExtension(sessionfile, EXTEN::state);
    XMLStore xml{TOPLEVEL::XML::State};

    capturePatchState(xml);

    bool success = xml.saveXMLfile(sessionfile, getLogger(), gzipCompression);
    if (success)
        Log("Session data saved to \""+sessionfile+"\"", _SYS_::LogNotSerious);
    else
        Log("Failed to save session data to \""+sessionfile+"\"", _SYS_::LogNotSerious);
    return success;
}

/** Variation to extract config and patch state for LV2 */
int Config::saveSessionData(char** dataBuffer)
{
    XMLStore xml{TOPLEVEL::XML::State};

    capturePatchState(xml);

    *dataBuffer = xml.render();
    return strlen(*dataBuffer) + 1;
}

void Config::capturePatchState(XMLStore& xml)
{
    addConfigXML(xml);
    synth.add2XML(xml);
    synth.midilearn.insertMidiListData(xml);
}


/**
 * Read configuration and patch state from XML state file
 * and overwrite config and engine settings with these values.
 */
bool Config::restoreSessionData(string sessionfile)
{
    if (sessionfile.size() && !isRegularFile(sessionfile))
        sessionfile = setExtension(sessionfile, EXTEN::state);
    if (!sessionfile.size() || !isRegularFile(sessionfile))
        Log("Session file \""+sessionfile+"\" not available", _SYS_::LogNotSerious);
    else
    {
        XMLStore xml{sessionfile, getLogger()};
        verifyVersion(xml);
        if (not xml)
            Log("Failed to load xml file \""+sessionfile+"\"", _SYS_::LogNotSerious);
        else
            return restorePatchState(xml);
    }
    return false;
}

/** Variation to retrieve patch state and config from the LV2 host */
bool Config::restoreSessionData(const char* dataBuffer, int size)
{
    (void)size; // currently unused

    XMLStore xml{dataBuffer};
    if (not xml)
        Log("Unable to parse XML to restore session state.");
    else
        return restorePatchState(xml);

    return false;
}

bool Config::restorePatchState(XMLStore& xml)
{
    if (extractConfigData(xml))
    {
        synth.defaults();
        if (synth.getfromXML(xml))
        {
            synth.setAllPartMaps();
            if (synth.midilearn.extractMidiListData(xml))
                synth.midilearn.updateGui(MIDILEARN::control::hideGUI);
                                       // handles possibly undefined window
            return true;
        }
    }
    return false;
}


bool Config::loadPresetsList()
{
    if (not isRegularFile(presetList))
        Log("Missing preset directories file \""+presetList+"\"");
    else
    {
        XMLStore xml{presetList, getLogger()};
        XMLtree xmlDirs = xml.getElm("PRESETDIRS");
        if (not xmlDirs)
            Log("loadPresetsList: no <PRESETDIRS> branch in \""+presetList+"\"");
        else
        {
            for (uint idx=0; idx < MAX_PRESETS; ++idx)
                if (auto entry = xmlDirs.getElm("XMZ_FILE", idx))
                    presetsDirlist[idx] = entry.getPar_str("dir");
                else break;
            return true;
        }// loaded successfully
    }
    return false;
}


bool Config::savePresetsList()
{
    XMLStore xml{TOPLEVEL::XML::PresetDirs};
    XMLtree xmlDirs = xml.addElm("PRESETDIRS");
    {
        for (uint idx=0;
             idx < MAX_PRESETS and not presetsDirlist[idx].empty();
             ++idx)
        {
            xmlDirs.addElm("XMZ_FILE", idx)
                   .addPar_str("dir", presetsDirlist[idx]);
        }
    }
    bool success = xmlDirs and xml.saveXMLfile(presetList, getLogger(), gzipCompression);
    if (not success)
        Log("Failed to save preset directory list to \""+presetList+"\"");
    return success;
}


/** @remark before 2022, this was part of the global config;
 *          with change set `fcedcc05` this is migrated into a separate file
 */
void Config::migrateLegacyPresetsList(XMLtree& basePars)
{
    int count{0};
    bool found{false};
    if (not isRegularFile(presetList))
    {// attempt to migrate legacy settings
        for (int i = 0; i < MAX_PRESET_DIRS; ++i)
        {
            if (XMLtree presetsRoot = basePars.getElm("PRESETSROOT", i))
            {
                string dir = presetsRoot.getPar_str("presets_root");
                if (isDirectory(dir))
                {
                    presetsDirlist[count] = dir;
                    found = true;
                    ++count;
                }
            }
        }
        if (not found)
        {// otherwise build the list anew from defaults
            defaultPresets();
            presetsRootID = 0;
        }
        savePresetsList(); // move settings to new location
    }
}


void Config::defaultPresets()
{
    auto presetDirsToTry = std::array{presetDir
                                     ,file::configDir()+"/presets"
                                     ,extendLocalPath("/presets")
                                        /*
                                         * TODO
                                         * We shouldn't be setting these directly
                                         */
                                     ,string{"/usr/share/yoshimi/presets"}
                                     ,string{"/usr/local/share/yoshimi/presets"}
                                     };
    int actual{0};
    Log("Setup default preset directories...", _SYS_::LogNotSerious);
    for (string& presetDir : presetDirsToTry)
        if (isDirectory(presetDir))
        {
            Log("adding: "+presetDir, _SYS_::LogNotSerious);
            presetsDirlist[actual++] = presetDir;
        }
}



void Config::Log(string const& msg, char tostderr)
{
    if ((tostderr & _SYS_::LogNotSerious) && hideErrors)
        return;
    else if(!(tostderr & _SYS_::LogError))
    {
        if (showGui && toConsole)
            logList.push_back(msg);
        else
            cout << msg << endl;
    }
    else
        cerr << msg << endl; // error log
}


void Config::startupReport(string const& clientName)
{
    bool fullInfo = (synth.getUniqueId() == 0);
    if (fullInfo)
        Log("Build Number " + std::to_string(build_ID));
    Log("Clientname: " + clientName);
    string report = "Audio: ";
    switch (audioEngine)
    {
        case jack_audio:
            report += "jack";
            break;

        case alsa_audio:
            report += "alsa";
            break;

        default:
            report += "nada";
            break;
    }
    report += (" -> '" + audioDevice + "'");
    Log(report, _SYS_::LogNotSerious);
    report = "Midi: ";
    switch (midiEngine)
    {
        case jack_midi:
            report += "jack";
            break;

        case alsa_midi:
            report += "alsa";
            break;

        default:
            report += "nada";
            break;
    }
    if (!midiDevice.size())
        midiDevice = "default";
    report += (" -> '" + midiDevice + "'");
    Log(report, _SYS_::LogNotSerious);
    if (fullInfo)
    {
        Log("Oscilsize: " + asString(synth.oscilsize), _SYS_::LogNotSerious);
        Log("Samplerate: " + asString(synth.samplerate), _SYS_::LogNotSerious);
        Log("Period size: " + asString(synth.buffersize), _SYS_::LogNotSerious);
    }
}


void Config::setRtprio(int prio)
{
    if (prio < rtprio)
        rtprio = prio;
}


// general thread start service
bool Config::startThread(pthread_t *pth, ThreadFun* threadFun, void *arg,
                         bool schedfifo, char priodec, string const& name)
{
    pthread_attr_t attr;
    int chk;
    bool outcome = false;
    bool retry = true;
    while (retry)
    {
        if (!(chk = pthread_attr_init(&attr)))
        {

            if (schedfifo)
            {
                if ((chk = pthread_attr_setschedpolicy(&attr, SCHED_FIFO)))
                {
                    Log("Failed to set SCHED_FIFO policy in thread attribute "
                                + string(strerror(errno))
                                + " (" + asString(chk) + ")", _SYS_::LogError);
                    schedfifo = false;
                    continue;
                }
                if ((chk = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)))
                {
                    Log("Failed to set inherit scheduler thread attribute "
                                + string(strerror(errno)) + " ("
                                + asString(chk) + ")", _SYS_::LogError);
                    schedfifo = false;
                    continue;
                }
                sched_param prio_params;
                int prio = rtprio - priodec;
                if (prio < 1)
                    prio = 1;
                Log(name + " priority is " + std::to_string(prio), _SYS_::LogError);
                prio_params.sched_priority = prio;
                if ((chk = pthread_attr_setschedparam(&attr, &prio_params)))
                {
                    Log("Failed to set thread priority attribute ("
                                + asString(chk) + ")  ", _SYS_::LogNotSerious | _SYS_::LogError);
                    schedfifo = false;
                    continue;
                }
            }
            if (!(chk = pthread_create(pth, &attr, threadFun, arg)))
            {
                outcome = true;
                break;
            }
            else if (schedfifo)
            {
                schedfifo = false;
                continue;
            }
            outcome = false;
            break;
        }
        else
            Log("Failed to initialise thread attributes " + asString(chk), _SYS_::LogError);

        if (schedfifo)
        {
            Log("Failed to start thread (sched_fifo) " + asString(chk)
                + "  " + string(strerror(errno)), _SYS_::LogError);
            schedfifo = false;
            continue;
        }
        Log("Failed to start thread (sched_other) " + asString(chk)
            + "  " + string(strerror(errno)), _SYS_::LogError);
        outcome = false;
        break;
    }
    return outcome;
}


void Config::signalCheck()
{
    #if defined(JACK_SESSION)
        int jsev = __sync_fetch_and_add(&jsessionSave, 0);
        if (jsev != 0)
        {
            __sync_and_and_fetch(&jsessionSave, 0);
            switch (jsev)
            {
                case JackSessionSave:
                    saveJackSession();
                    break;

                case JackSessionSaveAndQuit:
                    saveJackSession();
                    runSynth.store(false, std::memory_order_release);
                    break;

                case JackSessionSaveTemplate: // not implemented
                    break;

                default:
                    break;
            }
        }
    #endif

    if (ladi1IntActive)
    {
        __sync_and_and_fetch(&ladi1IntActive, 0);
        saveSessionData(stateFile);
    }

    if (sigIntActive)
        runSynth.store(false, std::memory_order_release);
}


void Config::setInterruptActive()
{
    Log("Interrupt received", _SYS_::LogError);
    __sync_or_and_fetch(&sigIntActive, 0xFF);
}


void Config::setLadi1Active()
{
    __sync_or_and_fetch(&ladi1IntActive, 0xFF);
}


bool Config::restoreJsession()
{
    #if defined(JACK_SESSION)
        return restoreSessionData(jackSessionFile);
    #else
        return false;
    #endif
}


void Config::setJackSessionSave(int event_type, string const& session_file)
{
    jackSessionFile = session_file;
    __sync_and_and_fetch(&jsessionSave, 0);
    __sync_or_and_fetch(&jsessionSave, event_type);
}


string Config::testCCvalue(int cc)
{
    string result = "";
    switch (cc)
    {
        case 1:
            result = "mod wheel";
            break;

        case 11:
            result = "expression";
            break;

        case 71:
            result = "filter Q";
            break;

        case 74:
            result = "filter cutoff";
            break;

        case 75:
            result = "bandwidth";
            break;

        case 76:
            result = "FM amplitude";
            break;

        case 77:
            result = "resonance center";
            break;

        case 78:
            result = "resonance bandwidth";
            break;

        default:
            result = masterCCtest(cc);
            break;
    }
    return result;
}


string Config::masterCCtest(int cc)
{
    string result = "";
    switch (cc)
    {
         case 6:
            result = "data msb";
            break;

        case 7:
            result = "volume";
            break;

        case 10:
            result = "panning";
            break;

        case 38:
            result = "data lsb";
            break;

        case 64:
            result = "sustain pedal";
            break;

        case 65:
            result = "portamento";
            break;

        case 96:
            result = "data increment";
            break;

        case 97:
            result = "data decrement";
            break;

        case 98:
            result = "NRPN lsb";
            break;

        case 99:
            result = "NRPN msb";
            break;

        case 120:
            result = "all sounds off";
            break;

        case 121:
            result = "reset all controllers";
            break;

        case 123:
            result = "all notes off";
            break;

        default:
            if (cc < 128) // don't compare with 'disabled' state
            {
                if (cc == midi_bank_C)
                    result = "bank change";
                else if (cc == midi_bank_root)
                    result = "bank root change";
                else if (cc == midi_upper_voice_C)
                    result = "extended program change";
                else if (cc == channelSwitchCC)
                    result = "channel switcher";
            }
            break;
    }
    return result;
}


void Config::saveJackSession()
{
    saveSessionData(jackSessionFile);
    jackSessionFile.clear();
}


std::string Config::findHtmlManual()
{
    string namelist = "";
    string tempnames = "";
    if(file::cmd2string("find /usr/share/doc/ -xdev -type f -name 'yoshimi_user_guide_version' 2>/dev/null", tempnames))
    {
        namelist = tempnames;
        tempnames = "";
    }

    if(file::cmd2string("find /usr/local/share/doc/ -xdev -type f -name 'yoshimi_user_guide_version' 2>/dev/null", tempnames))
    {
        namelist += tempnames;
        tempnames = "";
    }

    if(file::cmd2string("find $HOME/.local/share/doc/yoshimi/ -xdev -type f -name 'yoshimi_user_guide_version' 2>/dev/null", tempnames))
        namelist += tempnames;
    //std::cout << "Manual lists\n" << namelist << std::endl;
    size_t next = 0;
    string lastversion = "";
    string found = "";
    string name = "";
    string current = "";
    while (next != string::npos)
    {
        next = namelist.find("\n");
        if (next != string::npos)
        {
            name = namelist.substr(0, next);
            current = loadText(name);
            if (current >= lastversion)
            {
                lastversion = current;
                found = name;
            }
            namelist = namelist.substr( next +1);
        }
    }
    return found;
}


float Config::getConfigLimits(CommandBlock* getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    int min = 0;
    int max = 1;
    float def{0};
    uchar type{TOPLEVEL::type::Integer};

    switch (control)
    {
        case CONFIG::control::oscillatorSize:
            min = MIN_OSCIL_SIZE;
            def = 1024;
            max = MAX_OSCIL_SIZE;
            break;
        case CONFIG::control::bufferSize:
            min = MIN_BUFFER_SIZE;
            def = 512;
            max = MAX_BUFFER_SIZE;
           break;
        case CONFIG::control::padSynthInterpolation:
            break;
        case CONFIG::control::handlePadSynthBuild:
            max = 2;
            break;
        case CONFIG::control::virtualKeyboardLayout:
            max = 3;
            break;
        case CONFIG::control::XMLcompressionLevel:
            def = 3;
            max = 9;
            break;
        case CONFIG::control::reportsDestination:
            break;
        case CONFIG::control::logTextSize:
            def = 12;
            min = 11;
            max = 100;
            break;
        case CONFIG::control::savedInstrumentFormat:
            max = 3;
            break;
        case CONFIG::control::defaultStateStart:
            break;
        case CONFIG::control::hideNonFatalErrors:
            break;
        case CONFIG::control::showSplash:
            def = 1;
            break;
        case CONFIG::control::logInstrumentLoadTimes:
            break;
        case CONFIG::control::logXMLheaders:
            break;
        case CONFIG::control::saveAllXMLdata:
            break;
        case CONFIG::control::enableGUI:
            def = 1;
            break;
        case CONFIG::control::enableCLI:
            def = 1;
            break;
        case CONFIG::control::enableAutoInstance:
            def = 1;
            break;
        case CONFIG::control::exposeStatus:
            def = 1;
            max = 2;
            break;
        case CONFIG::control::enableHighlight:
            break;

        case CONFIG::control::jackMidiSource:
            min = 3; // anything greater than max
            def = textMsgBuffer.push("default");
            break;
        case CONFIG::control::jackPreferredMidi:
            def = 1;
            break;
        case CONFIG::control::jackServer:
            min = 3;
            def = textMsgBuffer.push("default");
            break;
        case CONFIG::control::jackPreferredAudio:
            def = 1;
            break;
        case CONFIG::control::jackAutoConnectAudio:
            def = 1;
            break;

        case CONFIG::control::alsaMidiSource:
            min = 3;
            def = textMsgBuffer.push("default");
            break;
        case CONFIG::control::alsaPreferredMidi:
            def = 1;
            break;
        case CONFIG::control::alsaAudioDevice:
            min = 3;
            def = textMsgBuffer.push("default");
            break;
        case CONFIG::control::alsaPreferredAudio:
            break;
        case CONFIG::control::alsaSampleRate:
            def = 2;
            max = 3;
            break;

        case CONFIG::control::bankRootCC: // runtime midi checked elsewhere
            def = 0;
            max = 119;
            break;
        case CONFIG::control::bankCC: // runtime midi checked elsewhere
            def = 32;
            max = 119;
            break;
        case CONFIG::control::enableProgramChange:
            break;
        case CONFIG::control::extendedProgramChangeCC: // runtime midi checked elsewhere
            def = 110;
            max = 119;
            break;
        case CONFIG::control::ignoreResetAllCCs:
            break;
        case CONFIG::control::logIncomingCCs:
            break;
        case CONFIG::control::showLearnEditor:
            def = 1;
            break;
        case CONFIG::control::enableOmni:
            break;
        case CONFIG::control::enableNRPNs:
            def = 1;
            break;

        case CONFIG::control::saveCurrentConfig:
            break;

        default:
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

