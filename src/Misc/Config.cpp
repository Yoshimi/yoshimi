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
#include <string>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <cassert>
#include <memory>

#if defined(JACK_SESSION)
#include <jack/session.h>
#endif

#include "Misc/XMLwrapper.h"
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

using std::cout;
using std::endl;

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

namespace { // Implementation details...

    TextMsgBuffer& textMsgBuffer = TextMsgBuffer::instance();
}

unsigned char panLaw = 1;

bool         Config::showSplash = true;
bool         Config::autoInstance = false;
unsigned int Config::activeInstance = 0;
int          Config::showCLIcontext = 1;

string jUuid = "";

Config::Config(SynthEngine *_synth, std::list<string>& allArgs, bool isLV2Plugin) :
    build_ID(BUILD_NUMBER),
    stateChanged(false),
    restoreJackSession(false),
    oldConfig(false),
    runSynth(true),
    finishedCLI(true),
    VirKeybLayout(0),
    audioEngine(DEFAULT_AUDIO),
    engineChanged(false),
    midiEngine(DEFAULT_MIDI),
    midiChanged(false),
    alsaMidiType(1), // search
    audioDevice("default"),
    midiDevice("default"),
    jackServer("default"),
    jackMidiDevice("default"),
    startJack(false),
    connectJackaudio(true),
    connectJackChanged(false),
    alsaAudioDevice("default"),
    alsaMidiDevice("default"),
    loadDefaultState(false),
    sessionStage(_SYS_::type::Normal),
    Interpolation(0),
    xmlType(0),
    instrumentFormat(1),
    EnableProgChange(1), // default will be inverted
    toConsole(0),
    consoleTextSize(12),
    hideErrors(0),
    showTimes(0),
    logXMLheaders(0),
    xmlmax(0),
    GzipCompression(3),
    Samplerate(48000),
    rateChanged(false),
    Buffersize(256),
    bufferChanged(false),
    Oscilsize(512),
    oscilChanged(false),
    showGui(true),
    storedGui(true),
    guiChanged(false),
    showCli(true),
    storedCli(true),
    cliChanged(false),
    singlePath(false),
    banksChecked(false),
    panLaw(1),
    configChanged(false),
    handlePadSynthBuild(0),
    rtprio(40),
    midi_bank_root(0), // 128 is used as 'disabled'
    midi_bank_C(32),   // 128 is used as 'disabled'
    midi_upper_voice_C(128), // disabled
    enable_NRPN(true),
    ignoreResetCCs(false),
    monitorCCin(false),
    showLearnedCC(true),
    NumAvailableParts(NUM_MIDI_CHANNELS),
    currentPart(0),
    currentBank(0),
    currentRoot(0),
    bankHighlight(false),
    lastBankPart(UNUSED),
    presetsRootID(0),
    tempBank(0),
    tempRoot(0),
    VUcount(0),
    channelSwitchType(0),
    channelSwitchCC(128), // disabled
    channelSwitchValue(0),
    nrpnL(127), // off
    nrpnH(127), // off
    nrpnActive(false),
    sigIntActive(0),
    ladi1IntActive(0),
    //sse_level(0),
    programcommand("yoshimi"),
    synth(_synth),
    bRuntimeSetupCompleted(false),
    exitType(EXIT_SUCCESS)
{
    std::cerr.precision(4);

    if (isLV2Plugin)
    {
        //Log("LV2 only");
        if (!loadConfig())
            Log("\n\nCould not load config. Using default values.\n");
        bRuntimeSetupCompleted = true;
        //skip further setup, which is irrelevant for lv2 plugin instance.
        return;
    }

    //Log("Standalone Only");
    static bool torun = true;
    if (torun) // only the first stand-alone synth can read args
    {
        applyOptions(this, allArgs);
        torun = false;
    }
    if (!loadConfig())
    {
        string message = "Could not load config. Using default values.";
        TextMsgBuffer::instance().push(message); // needed for CLI
        Log("\n\n" + message + "\n");
    }
    bRuntimeSetupCompleted = Setup();
}


bool Config::Setup(void)
{
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
    if (!audioDevice.size())
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
    if (!midiDevice.size())
        midiDevice = "";
    Oscilsize = nearestPowerOf2(Oscilsize, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);
    Buffersize = nearestPowerOf2(Buffersize, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);

    if (!jUuid.empty())
        jackSessionUuid = jUuid;
    return true;
}


Config::~Config()
{;}


void Config::flushLog(void)
{
    if (LogList.size())
    {
        while (LogList.size())
        {
            //cout << LogList.front() << endl;
            LogList.pop_front();
        }
    }
}


void *Config::_findManual(void *arg)
{
    return static_cast<Config*>(arg)->findManual();
}


void *Config::findManual(void)
{
    Log("finding manual");
    string currentV = string(YOSHIMI_VERSION);
    manualFile = findHtmlManual();
    guideVersion = loadText(manualFile);
    size_t pos = guideVersion.find(" ");
    if (pos != string::npos)
        guideVersion = guideVersion.substr(0, pos);
    Log("manual found");

    saveConfig(true);
    return NULL;
}


bool Config::loadConfig(void)
{
    if (file::userHome() == "/tmp")
        Log ("Failed to find 'Home' directory - using tmp.\nSettings will be lost on computer shutdown.");
    if (file::localDir().empty())
    {
        Log("Failed to create local yoshimi directory.");
        return false;
    }
    string foundConfig = file::configDir();
    defaultStateName = foundConfig + "/yoshimi";

    if (file::configDir().empty())
    {
        Log("Failed to create config directory '" + file::userHome() + "'");
        return false;
    }
    string yoshimi = "/" + string(YOSHIMI);

    baseConfig = foundConfig + yoshimi + string(EXTEN::config);

    int currentInstance = synth->getUniqueId();
    defaultSession = defaultStateName + "-" + asString(currentInstance) + EXTEN::state;
    yoshimi += ("-" + asString(currentInstance));
    //cout << "\nsession >" << defaultSession << "<\n" << endl;

    ConfigFile = foundConfig + yoshimi + EXTEN::instance;

    if (currentInstance == 0 && sessionStage != _SYS_::type::RestoreConf)
    {
        TextMsgBuffer::instance().init(); // sneaked it in here so it's early

        presetDir = file::localDir() + "/presets";
        if (!isDirectory(presetDir))
        { // only ever want to do this once
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
        if (!isDirectory(file::localDir() + "/found/"))
        { // only ever want to do this once
            if (createDir(file::localDir() + "/found/"))
                Log("Failed to create root directory for local banks");
        }

        // conversion for old location
        string newInstance0 = ConfigFile;
        if (isRegularFile(baseConfig) && !isRegularFile(newInstance0), 0)
        {
            file::copyFile(baseConfig, newInstance0, 0);
            Log("Reorganising config files.");
            if (isRegularFile(defaultStateName + EXTEN::state))
            {
                if (!isRegularFile(defaultSession))
                {
                    renameFile(defaultStateName + EXTEN::state, defaultSession);
                    Log("Moving default state file.");
                }
            }
        }
    }

    if (!isRegularFile(baseConfig))
    {
        Log("Basic configuration " + baseConfig + " not found, will use default settings");
            defaultPresets();
    }

    bool success{false};
    if (!isRegularFile(ConfigFile))
    {
        Log("Configuration " + ConfigFile + " not found, will use default settings");
    }
    else
    {
        // get base first
        auto xml{std::make_unique<XMLwrapper>(synth, true)};
        success = xml->loadXMLfile(baseConfig);
        if (success)
            success = extractBaseParameters(*xml);
        else
            Log("loadConfig load base failed");
    }

    if (success)
    {
        // the instance data
        auto xml{std::make_unique<XMLwrapper>(synth, true)};
        success = xml->loadXMLfile(ConfigFile);
        if (success)
            success = extractConfigData(*xml);
        else
            Log("loadConfig load instance failed");
    }
    if (currentInstance == 0 && sessionStage != _SYS_::type::RestoreConf)
    {
        int currentVersion = lastXMLmajor * 10 + lastXMLminor;
        int storedVersion = MIN_CONFIG_MAJOR * 10 + MIN_CONFIG_MINOR;
        if (currentVersion < storedVersion)
            oldConfig = true;
        else
            oldConfig = false;
    }

    if (sessionStage == _SYS_::type::RestoreConf)
        return true;

    if (sessionStage != _SYS_::type::Normal)
    {
        auto xml{std::make_unique<XMLwrapper>(synth, true)};
        success = xml->loadXMLfile(StateFile);
        if (success)
        {
            if (sessionStage == _SYS_::type::StartupFirst)
                sessionStage = _SYS_::type::StartupSecond;
            else if (sessionStage == _SYS_::type::JackFirst)
                sessionStage = _SYS_::type::JackSecond;
            success = extractConfigData(*xml);
        }
        else
            Log("loadConfig load instance failed");
    }
    if (success)
        loadPresetsList();

    if (success && currentInstance == 0)
    {
        // find user guide
        bool man_ok = false;
        string currentV = string(YOSHIMI_VERSION);
        size_t pos = currentV.find(" ");
        if (pos != string::npos)
            currentV = currentV.substr(0,pos);
//cout << "\nm >" << manualFile << "<" << endl;
//cout << "\nc " << currentV << "\ng " << guideVersion << endl;
        if (currentV == guideVersion && isRegularFile(manualFile))
            man_ok = true;

        if (!man_ok)
        {
            startThread(&findManualHandle, _findManual, this, false, 0, "CFG");
        }
    }
    return success;
}

void Config::restoreConfig(SynthEngine *_synth)
{
    size_t tmpRoot = _synth->ReadBankRoot();
    size_t tmpBank = _synth->ReadBank();
    int tmpChanged = configChanged;
    sessionStage = _SYS_::type::RestoreConf;

    // restore old settings
    loadConfig();

    // but keep current root and bank
    _synth->setRootBank(tmpRoot, tmpBank);
    // and ESPECIALLY 'load as default' status!
    configChanged = tmpChanged;
}


bool Config::updateConfig(int control, int value)
{
    /*
     * This routine only stores settings that the user has directly changed
     * and not those changed via CLI startup parameters, nor changes made
     * by loading sessions etc.
     *
     * It loads the previously saved config into an array so it doesn't
     * disrupt the complete config currently in place. It then overwrites
     * just the parameter the user changed, and resaves everything
     * including system generated entries.
     *
     * Text entries are handled via textMsgBuffer so only a single array
     * type is needed, simplifying the code.
     *
     * Some assumptions are made based on the fact the parameters must be
     * in the correct range as they otherwise couldn't have been created.
     */

    bool success{false};
    if (control <= CONFIG::control::XMLcompressionLevel)
    { // handling base config

        std::cout << "in base conf" << std::endl;

        int baseData[CONFIG::control::XMLcompressionLevel+1];
        xmlType = TOPLEVEL::XML::MasterUpdate;
        baseConfig = file::configDir() + "/yoshimi" + string(EXTEN::config);
        auto xml{std::make_unique<XMLwrapper>(synth, true)};
        success = xml->loadXMLfile(baseConfig);

        // the following two are system defined;
        bool banks = false; // default
        unsigned int instances = 1; // default
        if (success)
        {
            xml->enterbranch("BASE_PARAMETERS");
            baseData[CONFIG::control::enableGUI] = xml->getparbool("enable_gui",true);
            baseData[CONFIG::control::showSplash] = xml->getparbool("enable_splash",true);
            baseData[CONFIG::control::enableCLI] = xml->getparbool("enable_cli",true);
            baseData[CONFIG::control::exposeStatus] = xml->getpar("show_cli_context",0,3,3);
            baseData[CONFIG::control::enableSinglePath] = xml->getparbool("enable_single_master",false);
            baseData[CONFIG::control::enableAutoInstance] = xml->getparbool("enable_auto_instance",false);
            baseData[CONFIG::control::handlePadSynthBuild] = xml->getparU("handle_padsynth_build",0);
            baseData[CONFIG::control::XMLcompressionLevel] = xml->getpar("gzip_compression",0,3,9);

            // the following two are system defined;
            banks = xml->getparbool("banks_checked",false);
            instances = xml->getparU("active_instances",1);
            xml->exitbranch();

             // this is the one that changed
            baseData[control] = value;
        }

        if (success)
        {
            auto xml{std::make_unique<XMLwrapper>(synth, true)};
            xml->beginbranch("BASE_PARAMETERS");
            xml->addparbool("enable_gui",baseData[CONFIG::control::enableGUI]);
            xml->addparbool("enable_splash",baseData[CONFIG::control::showSplash]);
            xml->addparbool("enable_cli",baseData[CONFIG::control::enableCLI]);
            xml->addpar("show_cli_context",baseData[CONFIG::control::exposeStatus]);
            xml->addparbool("enable_single_master",baseData[CONFIG::control::enableSinglePath]);
            xml->addparbool("enable_auto_instance",baseData[CONFIG::control::enableAutoInstance]);
            xml->addparU("handle_padsynth_build",baseData[CONFIG::control::handlePadSynthBuild]);
            xml->addpar("gzip_compression",baseData[CONFIG::control::XMLcompressionLevel]);

            // the following four are system defined;
            xml->addparbool("banks_checked",banks);
            xml->addparU("active_instances",instances);
            xml->addparstr("guide_version", synth->getRuntime().guideVersion);
            xml->addparstr("manual", synth->getRuntime().manualFile);
            xml->endbranch(); // BASE_PARAMETERS

            if (!xml->saveXMLfile(baseConfig, false))
            {
                Log("Failed to update master config", _SYS_::LogNotSerious);
            }
        }
        else
        {
            Log("loadConfig load base failed");
        }
    }
    else
    { // handling current session
        const int offset = CONFIG::control::defaultStateStart;
        const int arraySize = CONFIG::control::historyLock - offset;
        const string instance = asString(synth->getUniqueId());

        xmlType = TOPLEVEL::XML::Config;
        string configFile = file::configDir() + "/yoshimi-" + instance + string(EXTEN::instance);
        int configData[arraySize]; // historyLock is handled elsewhere
        auto xml{std::make_unique<XMLwrapper>(synth, true)};
        success = xml->loadXMLfile(configFile);

                std::cout << "in session control " << std::endl;


        // the following two are system defined;
        int tempRoot = 5; // default
        int tempBank = 5; // default
        string tempText = "";

        if (success)
        {
            xml->enterbranch("CONFIGURATION");
            configData[CONFIG::control::defaultStateStart - offset] = xml->getpar("defaultState", 0, 0, 1);
            configData[CONFIG::control::bufferSize - offset] = xml->getpar("sound_buffer_size", 0, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);
            configData[CONFIG::control::oscillatorSize - offset] = xml->getpar("oscil_size", 0, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);
            configData[CONFIG::control::reportsDestination - offset] = xml->getpar("reports_destination", 0, 0, 1);
            configData[CONFIG::control::logTextSize - offset] = xml->getpar("console_text_size", 0, 11, 100);
            configData[CONFIG::control::padSynthInterpolation - offset] = xml->getpar("interpolation", 0, 0, 1);
            configData[CONFIG::control::virtualKeyboardLayout - offset] = xml->getpar("virtual_keyboard_layout", 0, 1, 6) - 1;
            configData[CONFIG::control::savedInstrumentFormat - offset] = xml->getpar("saved_instrument_format",0, 1, 3);
            configData[CONFIG::control::hideNonFatalErrors - offset] = xml->getpar("hide_system_errors", 0, 0, 1);
            configData[CONFIG::control::logInstrumentLoadTimes - offset] = xml->getpar("report_load_times", 0, 0, 1);
            configData[CONFIG::control::logXMLheaders - offset] = xml->getpar("report_XMLheaders", 0, 0, 1);
            configData[CONFIG::control::saveAllXMLdata - offset] = xml->getpar("full_parameters", 0, 0, 1);
            configData[CONFIG::control::enableHighlight - offset] = xml->getparbool("bank_highlight", bankHighlight);
            configData[CONFIG::control::jackMidiSource - offset] = textMsgBuffer.push(xml->getparstr("linux_jack_midi_dev"));// string
            configData[CONFIG::control::jackServer - offset] =  textMsgBuffer.push(xml->getparstr("linux_jack_server"));// string
            configData[CONFIG::control::jackAutoConnectAudio - offset] = xml->getpar("connect_jack_audio", 0, 0, 1);
            configData[CONFIG::control::alsaMidiSource - offset] = textMsgBuffer.push(xml->getparstr("linux_alsa_midi_dev"));// string
            configData[CONFIG::control::alsaMidiType - offset] = xml->getpar("alsa_midi_type", 0, 0, 2);
            configData[CONFIG::control::alsaAudioDevice - offset] = textMsgBuffer.push(xml->getparstr("linux_alsa_audio_dev"));// string
            configData[CONFIG::control::alsaSampleRate - offset] = xml->getpar("sample_rate", Samplerate, 44100, 192000);
            configData[CONFIG::control::readAudio - offset] = (audio_drivers)xml->getpar("audio_engine", 0, no_audio, alsa_audio);
            configData[CONFIG::control::readMIDI - offset] = (midi_drivers)xml->getpar("midi_engine", 0, no_midi, alsa_midi);
            //configData[CONFIG::control::addPresetRootDir - offset] = // string NOT stored
            //configData[CONFIG::control::removePresetRootDir - offset] = // returns string NOT used
            configData[CONFIG::control::currentPresetRoot - offset] = xml->getpar("presetsCurrentRootID", 0, 0, MAX_PRESETS);
            configData[CONFIG::control::bankRootCC - offset] = xml->getpar("midi_bank_root", 0, 0, 128);
            configData[CONFIG::control::bankCC - offset] = xml->getpar("midi_bank_C", midi_bank_C, 0, 128);
            configData[CONFIG::control::enableProgramChange - offset] = 1 - xml->getpar("ignore_program_change", 0, 0, 1); // inverted for Zyn compatibility
            configData[CONFIG::control::extendedProgramChangeCC - offset] = xml->getpar("midi_upper_voice_C", 0, 0, 128);// return string (in use)
            configData[CONFIG::control::ignoreResetAllCCs - offset] = xml->getpar("ignore_reset_all_CCs",0,0, 1);
            configData[CONFIG::control::logIncomingCCs - offset] = xml->getparbool("monitor-incoming_CCs", monitorCCin);
            configData[CONFIG::control::showLearnEditor - offset] = xml->getparbool("open_editor_on_learned_CC", showLearnedCC);
            configData[CONFIG::control::enableNRPNs - offset] = xml->getparbool("enable_incoming_NRPNs", enable_NRPN);
            //configData[CONFIG::control::saveCurrentConfig - offset] = // return string (dummy)

            tempRoot = xml->getpar("root_current_ID", tempRoot, 0, 127);
            tempBank = xml->getpar("bank_current_ID", tempBank, 0, 127);
            xml->exitbranch();

            // this is the one that changed
            std::cout << "control "<< control << "  val " << value << std::endl;
            std::cout << control - offset << std::endl;
            configData[control - offset] = value;

            if (success)
            {
                auto xml{std::make_unique<XMLwrapper>(synth, true)};
                xml->beginbranch("CONFIGURATION");
                xml->addpar("defaultState", configData[CONFIG::control::defaultStateStart - offset]);
                xml->addpar("sound_buffer_size", configData[CONFIG::control::bufferSize - offset]);
                xml->addpar("oscil_size", configData[CONFIG::control::oscillatorSize - offset]);
                xml->addpar("reports_destination", configData[CONFIG::control::reportsDestination - offset]);
                xml->addpar("console_text_size", configData[CONFIG::control::logTextSize - offset]);
                xml->addpar("interpolation", configData[CONFIG::control::padSynthInterpolation - offset]);
                xml->addpar("virtual_keyboard_layout", configData[CONFIG::control::virtualKeyboardLayout - offset] + 1);
                xml->addpar("saved_instrument_format", configData[CONFIG::control::savedInstrumentFormat - offset]);
                xml->addpar("hide_system_errors", configData[CONFIG::control::hideNonFatalErrors - offset]);
                xml->addpar("report_load_times", configData[CONFIG::control::logInstrumentLoadTimes - offset]);
                xml->addpar("report_XMLheaders", configData[CONFIG::control::logXMLheaders - offset]);
                xml->addpar("full_parameters", configData[CONFIG::control::saveAllXMLdata - offset]);
                xml->addparbool("bank_highlight", configData[CONFIG::control::enableHighlight - offset]);
                xml->addpar("audio_engine", configData[CONFIG::control::readAudio - offset]);
                xml->addpar("midi_engine", configData[CONFIG::control::readMIDI - offset]);
                xml->addparstr("linux_jack_server", textMsgBuffer.fetch(configData[CONFIG::control::jackServer - offset]));
                xml->addparstr("linux_jack_midi_dev", textMsgBuffer.fetch(configData[CONFIG::control::jackMidiSource - offset]));
                xml->addpar("connect_jack_audio", configData[CONFIG::control::jackAutoConnectAudio - offset]);
                xml->addpar("alsa_midi_type", configData[CONFIG::control::alsaMidiType - offset]);
                xml->addparstr("linux_alsa_audio_dev", textMsgBuffer.fetch(configData[CONFIG::control::alsaAudioDevice - offset]));
                xml->addparstr("linux_alsa_midi_dev", textMsgBuffer.fetch(configData[CONFIG::control::alsaMidiSource - offset]));
                xml->addpar("sample_rate", configData[CONFIG::control::alsaSampleRate - offset]);
                xml->addpar("presetsCurrentRootID", configData[CONFIG::control::currentPresetRoot - offset]);
                xml->addpar("midi_bank_root", configData[CONFIG::control::bankRootCC - offset]);
                xml->addpar("midi_bank_C", configData[CONFIG::control::bankCC - offset]);
                xml->addpar("midi_upper_voice_C", configData[CONFIG::control::extendedProgramChangeCC - offset]);
                xml->addpar("ignore_program_change", (1 - configData[CONFIG::control::enableProgramChange - offset]));
                xml->addpar("enable_part_on_voice_load", 1); // for backward compatibility
                xml->addparbool("enable_incoming_NRPNs", configData[CONFIG::control::enableNRPNs - offset]);
                xml->addpar("ignore_reset_all_CCs",configData[CONFIG::control::ignoreResetAllCCs - offset]);
                xml->addparbool("monitor-incoming_CCs", configData[CONFIG::control::logIncomingCCs - offset]);
                xml->addparbool("open_editor_on_learned_CC",configData[CONFIG::control::showLearnEditor - offset]);

                xml->addpar("root_current_ID", tempRoot);
                xml->addpar("bank_current_ID", tempBank);
                xml->endbranch(); // CONFIGURATION

                if (!xml->saveXMLfile(configFile, false))
                {
                    Log("Failed to update instance", _SYS_::LogNotSerious);
                }
            }
        }
        else
        {
            Log("loadConfig load instance " + instance + " failed");
        }
        ;
    }
    return success;
}


void Config::defaultPresets(void)
{
    string presetdirs[]  = {
        presetDir,
        extendLocalPath("/presets"),
        /*
         * TODO
         * We shouldn't be setting these directly
         */
        "/usr/share/yoshimi/presets",
        "/usr/local/share/yoshimi/presets",
        "@end"
    };
    int i = 0;
    int actual = 0;
    while (presetdirs[i] != "@end")
    {
        if (isDirectory(presetdirs[i]))
        {
            Log(presetdirs[i], _SYS_::LogNotSerious);
            presetsDirlist[actual] = presetdirs[i];
            ++actual;
        }
        ++i;
    }
}


bool Config::extractBaseParameters(XMLwrapper& xml)
{
    if (synth->getUniqueId() != 0)
        return true;

    if (!xml.enterbranch("BASE_PARAMETERS"))
    {
        Log("extractConfigData, no BASE_PARAMETERS branch");
        return false;
    }

    storedGui = xml.getparbool("enable_gui", showGui);
    if (!guiChanged)
        showGui = storedGui;

    showSplash = xml.getparbool("enable_splash", showSplash);

    storedCli = xml.getparbool("enable_CLI", showCli);
    if (!cliChanged)
        showCli = storedCli;

    singlePath  = xml.getparbool("enable_single_master", singlePath);
    banksChecked = xml.getparbool("banks_checked", banksChecked);
    autoInstance = xml.getparbool("enable_auto_instance", autoInstance);
    if (autoInstance)
        activeInstance = xml.getparU("active_instances", 0);
    else
        activeInstance = 1;
    handlePadSynthBuild = xml.getparU("handle_padsynth_build", 1, 0, 2);  // 0 = blocking/muted, 1 = background thread (=default), 2 = auto-Apply on param change
    showCLIcontext  = xml.getpar("show_CLI_context", 1, 0, 2);
    GzipCompression = xml.getpar("gzip_compression", GzipCompression, 0, 9);

    // get preset dirs
    int count = 0;
    bool found = false;
    if (!isRegularFile(file::localDir() + "/presetDirs"))
    {
        for (int i = 0; i < MAX_PRESET_DIRS; ++i)
        {
            if (xml.enterbranch("PRESETSROOT", i))
            {
                string dir = xml.getparstr("presets_root");
                if (isDirectory(dir))
                {
                    presetsDirlist[count] = dir;
                    found = true;
                    ++count;
                }
                xml.exitbranch();
            }
        }

        if (!found)
        {
            defaultPresets();
            presetsRootID = 0;
            configChanged = true; // give the user the choice

            savePresetsList(); // move these to new location
        }
    }


    guideVersion = xml.getparstr("guide_version");
    manualFile = xml.getparstr("manual");

    xml.exitbranch(); // BaseParameters
    return true;
}

bool Config::extractConfigData(XMLwrapper& xml)
{
    if (!xml.enterbranch("CONFIGURATION"))
    {
        Log("extractConfigData, no CONFIGURATION branch");
        Log("Running with defaults");
        return true;
    }
    /*
     * default state must be first test as we need to abort
     * and fetch this instead
     */
    if (sessionStage == _SYS_::type::Normal)
    {
        loadDefaultState = xml.getpar("defaultState", loadDefaultState, 0, 1);
        if (loadDefaultState)
        {
            xml.exitbranch(); // CONFIGURATION
            configChanged = true;
            sessionStage = _SYS_::type::Default;
            StateFile = defaultSession;
            Log("Loading default state");
            return true;
        }
    }

    if (sessionStage != _SYS_::type::InProgram)
    {

        if (!bufferChanged)
            Buffersize = xml.getpar("sound_buffer_size", Buffersize, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);
        if (!oscilChanged)
            Oscilsize = xml.getpar("oscil_size", Oscilsize, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);
        toConsole = xml.getpar("reports_destination", toConsole, 0, 1);
        consoleTextSize = xml.getpar("console_text_size", consoleTextSize, 11, 100);
        Interpolation = xml.getpar("interpolation", Interpolation, 0, 1);
        VirKeybLayout = xml.getpar("virtual_keyboard_layout", VirKeybLayout, 1, 6) - 1;
        hideErrors = xml.getpar("hide_system_errors", hideErrors, 0, 1);
        showTimes = xml.getpar("report_load_times", showTimes, 0, 1);
        logXMLheaders = xml.getpar("report_XMLheaders", logXMLheaders, 0, 1);
        xmlmax = xml.getpar("full_parameters", xmlmax, 0, 1);

        bankHighlight = xml.getparbool("bank_highlight", bankHighlight);
        loadPresetsList();
        presetsRootID = xml.getpar("presetsCurrentRootID", presetsRootID, 0, MAX_PRESETS);


        // engines
        if (!engineChanged)
            audioEngine = (audio_drivers)xml.getpar("audio_engine", audioEngine, no_audio, alsa_audio);
        if (!midiChanged)
            midiEngine = (midi_drivers)xml.getpar("midi_engine", midiEngine, no_midi, alsa_midi);
        alsaMidiType = xml.getpar("alsa_midi_type", 0, 0, 2);

        // alsa settings
        alsaAudioDevice = xml.getparstr("linux_alsa_audio_dev");
        alsaMidiDevice = xml.getparstr("linux_alsa_midi_dev");
        if (!rateChanged)
            Samplerate = xml.getpar("sample_rate", Samplerate, 44100, 192000);

        // jack settings
        jackServer = xml.getparstr("linux_jack_server");
        jackMidiDevice = xml.getparstr("linux_jack_midi_dev");
        if (!connectJackChanged)
            connectJackaudio = xml.getpar("connect_jack_audio", connectJackaudio, 0, 1);

        // midi options
        midi_bank_root = xml.getpar("midi_bank_root", midi_bank_root, 0, 128);
        midi_bank_C = xml.getpar("midi_bank_C", midi_bank_C, 0, 128);
        midi_upper_voice_C = xml.getpar("midi_upper_voice_C", midi_upper_voice_C, 0, 128);
        EnableProgChange = 1 - xml.getpar("ignore_program_change", EnableProgChange, 0, 1); // inverted for Zyn compatibility
        instrumentFormat = xml.getpar("saved_instrument_format",instrumentFormat, 1, 3);
        enable_NRPN = xml.getparbool("enable_incoming_NRPNs", enable_NRPN);
        ignoreResetCCs = xml.getpar("ignore_reset_all_CCs",ignoreResetCCs,0, 1);
        monitorCCin = xml.getparbool("monitor-incoming_CCs", monitorCCin);
        showLearnedCC = xml.getparbool("open_editor_on_learned_CC", showLearnedCC);
    }
    if (tempRoot == 0)
        tempRoot = xml.getpar("root_current_ID", 0, 0, 127);

    if (tempBank == 0)
    tempBank = xml.getpar("bank_current_ID", 0, 0, 127);
    xml.exitbranch(); // CONFIGURATION
    return true;
}


bool Config::saveConfig(bool master)
{
    bool success{false};
    if (master)
    {
        //cout << "saving master" << endl;
        xmlType = TOPLEVEL::XML::MasterConfig;
        auto xml{std::make_unique<XMLwrapper>(synth, true)};
        string resConfigFile = baseConfig;

        if (xml->saveXMLfile(resConfigFile, false))
        {
            configChanged = false;
            success = true;
        }
        else
            Log("Failed to save master config to " + resConfigFile, _SYS_::LogNotSerious);
    }

    xmlType = TOPLEVEL::XML::Config;
    auto xml{std::make_unique<XMLwrapper>(synth, true)};
    addConfigXML(*xml);
    string resConfigFile = ConfigFile;

    if (xml->saveXMLfile(resConfigFile))
    {
        configChanged = false;
        success = true;
    }
    else
        Log("Failed to save instance to " + resConfigFile, _SYS_::LogNotSerious);

    return success;
}


void Config::addConfigXML(XMLwrapper& xml)
{
    xml.beginbranch("CONFIGURATION");
    xml.addpar("defaultState", loadDefaultState);

    xml.addpar("sound_buffer_size", synth->getRuntime().Buffersize);
    xml.addpar("oscil_size", synth->getRuntime().Oscilsize);
    xml.addpar("reports_destination", toConsole);
    xml.addpar("console_text_size", consoleTextSize);
    xml.addpar("interpolation", Interpolation);
    xml.addpar("virtual_keyboard_layout", VirKeybLayout + 1);
    xml.addpar("saved_instrument_format", instrumentFormat);
    xml.addpar("hide_system_errors", hideErrors);
    xml.addpar("report_load_times", showTimes);
    xml.addpar("report_XMLheaders", logXMLheaders);
    xml.addpar("full_parameters", xmlmax);

    xml.addparbool("bank_highlight", bankHighlight);

    xml.addpar("audio_engine", synth->getRuntime().audioEngine);
    xml.addpar("midi_engine", synth->getRuntime().midiEngine);

    xml.addparstr("linux_jack_server", jackServer);
    xml.addparstr("linux_jack_midi_dev", jackMidiDevice);
    xml.addpar("connect_jack_audio", connectJackaudio);

    xml.addpar("alsa_midi_type", synth->getRuntime().alsaMidiType);
    xml.addparstr("linux_alsa_audio_dev", alsaAudioDevice);
    xml.addparstr("linux_alsa_midi_dev", alsaMidiDevice);
    xml.addpar("sample_rate", synth->getRuntime().Samplerate);

    xml.addpar("presetsCurrentRootID", presetsRootID);
    xml.addpar("midi_bank_root", midi_bank_root);
    xml.addpar("midi_bank_C", midi_bank_C);
    xml.addpar("midi_upper_voice_C", midi_upper_voice_C);
    xml.addpar("ignore_program_change", (1 - EnableProgChange));
    xml.addpar("enable_part_on_voice_load", 1); // for backward compatibility
    xml.addparbool("enable_incoming_NRPNs", enable_NRPN);
    xml.addpar("ignore_reset_all_CCs",ignoreResetCCs);
    xml.addparbool("monitor-incoming_CCs", monitorCCin);
    xml.addparbool("open_editor_on_learned_CC",showLearnedCC);
    xml.addpar("root_current_ID", synth->ReadBankRoot());
    xml.addpar("bank_current_ID", synth->ReadBank());
    xml.endbranch(); // CONFIGURATION
}


bool Config::saveSessionData(string savefile)
{
    savefile = setExtension(savefile, EXTEN::state);
    synth->getRuntime().xmlType = TOPLEVEL::XML::State;
    auto xml{std::make_unique<XMLwrapper>(synth, true)};
    bool success{false};
    addConfigXML(*xml);
    synth->add2XML(*xml);
    synth->midilearn.insertMidiListData(*xml);
    if (xml->saveXMLfile(savefile))
    {
        Log("Session data saved to " + savefile, _SYS_::LogNotSerious);
        success = true;
    }
    else
        Log("Failed to save session data to " + savefile, _SYS_::LogNotSerious);

    return success;
}


bool Config::restoreSessionData(string sessionfile)
{
    bool success{false};

    if (sessionfile.size() && !isRegularFile(sessionfile))
        sessionfile = setExtension(sessionfile, EXTEN::state);
    if (!sessionfile.size() || !isRegularFile(sessionfile))
    {
        Log("Session file " + sessionfile + " not available", _SYS_::LogNotSerious);
        return false;
    }
    auto xml{std::make_unique<XMLwrapper>(synth, true)};
    if (!xml->loadXMLfile(sessionfile))
    {
        Log("Failed to load xml file " + sessionfile, _SYS_::LogNotSerious);
        return false;
    }


    success = extractConfigData(*xml);
    if (success)
    {
        // mark as soon as anything changes
        synth->getRuntime().stateChanged = true;

        synth->defaults();
        success = synth->getfromXML(*xml);
        if (success)
            synth->setAllPartMaps();
        bool oklearn = synth->midilearn.extractMidiListData(false, *xml);
        if (oklearn)
            synth->midilearn.updateGui(MIDILEARN::control::hideGUI);
            // handles possibly undefined window
    }

    return success;
}


bool Config::loadPresetsList()
{
    string presetDirname = file::localDir()  + "/presetDirs";
    if (!isRegularFile(presetDirname))
    {
        Log("Missing preset directories file");
        return false;
    }
    xmlType = TOPLEVEL::XML::PresetDirs;

    auto xml{std::make_unique<XMLwrapper>(synth, true)};
    xml->loadXMLfile(presetDirname);

    if (!xml->enterbranch("PRESETDIRS"))
    {
        Log("loadPresetDirsData, no PRESETDIRS branch");
        return false;
    }
    int count = 0;
    bool ok{false};
    do
    {
        if (xml->enterbranch("XMZ_FILE", count))
        {
            presetsDirlist[count] = xml->getparstr("dir");
            xml->exitbranch();
            ok = true;
        }
        else
            ok = false;
        ++count;
    }
    while (ok);
    xml->endbranch();

    return true;
}


bool Config::savePresetsList()
{
    string presetDirname = file::localDir()  + "/presetDirs";
    xmlType = TOPLEVEL::XML::PresetDirs;

    auto xml{std::make_unique<XMLwrapper>(synth, true)};
    xml->beginbranch("PRESETDIRS");
    {
        int count = 0;
        while (!presetsDirlist[count].empty())
        {
            xml->beginbranch("XMZ_FILE", count);
                xml->addparstr("dir", presetsDirlist[count]);
            xml->endbranch();
            ++count;
        }
    }
    xml->endbranch();
    if (!xml->saveXMLfile(presetDirname))
        Log("Failed to save data to " + presetDirname);

    return true;
}


void Config::Log(const string& msg, char tostderr)
{
    if ((tostderr & _SYS_::LogNotSerious) && hideErrors)
        return;
    else if(!(tostderr & _SYS_::LogError))
    {
        if (showGui && toConsole)
            LogList.push_back(msg);
        else
            cout << msg << endl;
    }
    else
        std::cerr << msg << endl; // error log
}


void Config::LogError(const string &msg)
{
    std::cerr << "[ERROR] " << msg << endl;
}


void Config::StartupReport(const string& clientName)
{
    bool fullInfo = (synth->getUniqueId() == 0);
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
    }
    if (!midiDevice.size())
        midiDevice = "default";
    report += (" -> '" + midiDevice + "'");
    Log(report, _SYS_::LogNotSerious);
    if (fullInfo)
    {
        Log("Oscilsize: " + asString(synth->oscilsize), _SYS_::LogNotSerious);
        Log("Samplerate: " + asString(synth->samplerate), _SYS_::LogNotSerious);
        Log("Period size: " + asString(synth->buffersize), _SYS_::LogNotSerious);
    }
}


void Config::setRtprio(int prio)
{
    if (prio < rtprio)
        rtprio = prio;
}


// general thread start service
bool Config::startThread(pthread_t *pth, void *(*thread_fn)(void*), void *arg,
                         bool schedfifo, char priodec, const string& name)
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
            if (!(chk = pthread_create(pth, &attr, thread_fn, arg)))
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


void Config::signalCheck(void)
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
                    runSynth = false;
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
        saveSessionData(StateFile);
    }

    if (sigIntActive)
        runSynth = false;
}


void Config::setInterruptActive(void)
{
    Log("Interrupt received", _SYS_::LogError);
    __sync_or_and_fetch(&sigIntActive, 0xFF);
}


void Config::setLadi1Active(void)
{
    __sync_or_and_fetch(&ladi1IntActive, 0xFF);
}


bool Config::restoreJsession(void)
{
    #if defined(JACK_SESSION)
        return restoreSessionData(jackSessionFile);
    #else
        return false;
    #endif
}


void Config::setJackSessionSave(int event_type, const string& session_file)
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
        {
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
        }
    }
    return result;
}


void Config::saveJackSession(void)
{
    saveSessionData(jackSessionFile);
    jackSessionFile.clear();
}


void Config::applyOptions(Config* settings, std::list<string>& allArgs)
{
    if (allArgs.empty())
        return;
    for (std::list<string>::iterator it = allArgs.begin(); it != allArgs.end(); ++it)
    {
        string line = *it;
        size_t pos = line.find(":");
        char cmd = line.at(0);
        line = line.substr(pos +1);
        switch (cmd)
        {
            case 'A':
                settings->configChanged = true;
                settings->engineChanged = true;
                settings->audioEngine = alsa_audio;
                if (!line.empty())
                    settings->audioDevice = line;
                else
                    settings->audioDevice = settings->alsaAudioDevice;
            break;

        case 'a':
            settings->configChanged = true;
            settings->midiChanged = true;
            settings->midiEngine = alsa_midi;
            if (!line.empty())
                settings->midiDevice = line;
            else
                settings->midiDevice = string(settings->alsaMidiDevice);
            break;

        case 'b':
            settings->configChanged = true;
            settings->bufferChanged = true;
            settings->Buffersize = string2int(line);
            //cout << "B "<< line << endl;
            break;

        case 'c':
            //settings->configChanged = true;
            settings->cliChanged = true;
            settings->showCli = false;
            break;

        case 'C':
            //settings->configChanged = true;
            settings->cliChanged = true;
            settings->showCli = true;
            break;

        case 'D':
            if (!line.empty())
                settings->rootDefine = line;
            break;

        case 'i':
            //settings->configChanged = true;
            settings->guiChanged = true;
            settings->showGui = false;
            break;

        case 'I':
            //settings->configChanged = true;
            settings->guiChanged = true;
            settings->showGui = true;
            break;

        case 'J':
            settings->configChanged = true;
            settings->engineChanged = true;
            settings->audioEngine = jack_audio;
            if (!line.empty())
                settings->audioDevice = line;
            break;

        case 'j':
            settings->configChanged = true;
            settings->midiChanged = true;
            settings->midiEngine = jack_midi;
            if (!line.empty())
                settings->midiDevice = line;
            else
                settings->midiDevice = string(settings->jackMidiDevice);
            break;

        case 'K':
            settings->configChanged = true;
            settings->connectJackChanged = true;
            settings->connectJackaudio = true;
            break;

        case 'k':
            settings->startJack = true;
            break;

        case 'l': settings->paramsLoad = line; break;

        case 'L':
        {
            unsigned int partLoad = 0;
            size_t pos = line.rfind("@");
            // this provides a way to specify which part to load to
            if (pos != string::npos)
            {
                if (line.length() - pos <= 3)
                {
                    partLoad = (stoi("0" + line.substr(pos + 1))) - 1;
                }
                if (partLoad >= 64)
                    partLoad = 0;
                line = line.substr(0, pos);
            }
            settings->load2part = partLoad;
            settings->instrumentLoad = line;
            break;
        }

        case 'M':settings->midiLearnLoad = line; break;

        case 'N': settings->nameTag = line; break;

        case 'o':
            settings->configChanged = true;
            settings->oscilChanged = true;
            settings->Oscilsize = string2int(line);
            break;

        case 'R':
        {
            settings->configChanged = true;
            settings->rateChanged = true;
            int num = (string2int(line) / 48 ) * 48;
            if (num < 48000 || num > 192000)
                num = 44100; // play safe
            settings->Samplerate = num;
            break;
        }

        case 'S':
            if (!line.empty())
            {
                settings->sessionStage = _SYS_::type::StartupFirst;
                settings->configChanged = true;
                settings->StateFile = line;
            }
            break;

        case 'T':
            if (!line.empty())
            {
                settings->remoteGuiTheme = line;
            }
            break;

        case 'u':
            if (!line.empty())
            {
                settings->sessionStage = _SYS_::type::JackFirst;
                settings->configChanged = true;
                settings->StateFile = setExtension(line, EXTEN::state);
            }
            break;

        case 'U':
            if (!line.empty())
                jUuid = line;
            break;

        case '@':
            settings->configChanged = true;
            settings->engineChanged = true;
            settings->midiChanged = true;
            settings->audioEngine = no_audio;
            settings->midiEngine  = no_midi;
            break;
        }

        //cout << cmd << " line " << line << endl;
    }
    if (jackSessionUuid.size() && jackSessionFile.size())
        restoreJackSession = true;
}


std::string Config::findHtmlManual(void)
{
    string namelist = "";
    string tempnames = "";
    if(file::cmd2string("find /usr/share/ -type f -name 'yoshimi_user_guide_version' 2>/dev/null", tempnames))
        namelist = tempnames;

    if(file::cmd2string("find /usr/local/share/ -type f -name 'yoshimi_user_guide_version' 2>/dev/null", tempnames))
        namelist += tempnames;

    if(file::cmd2string("find /home/ -type f -name 'yoshimi_user_guide_version' 2>/dev/null", tempnames))
        namelist += tempnames;

    //cout << "man list" << namelist << endl;

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
            if (current > lastversion)
            {
                lastversion = current;
                found = name;
                //cout << "found >" << found << endl;
            }
            namelist = namelist.substr( next +1);
        }
    }
    return found;
}


float Config::getConfigLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    unsigned char type = 0;

    std::cout << "In config defaults" << std::endl;

    int min = 0;
    float def = 0;
    int max = 1;
    type |= TOPLEVEL::type::Integer;

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


#ifdef GUI_FLTK
void GuiThreadMsg::processGuiMessages()
{
    GuiThreadMsg *msg = (GuiThreadMsg *)Fl::thread_message();
    if (msg)
    {
        assert(msg->data);
        InterChange& interChange = * static_cast<InterChange*>(msg->data);
        if (msg->type == GuiThreadMsg::NewSynthEngine)
        {
            MasterUI& guiMaster = interChange.createGuiMaster(msg->index);
            guiMaster.Init();
            ///////////////////////////////////////TODO 3/2024 the following should be done from the Synth thread
            guiMaster.synth->postGuiStartHook();   //// this is a premature hack to fix missing push-updates from early load / state files

            if (guiMaster.synth->getRuntime().audioEngine < 1)
                alert(guiMaster.synth, "Yoshimi could not connect to any sound system. Running with no Audio.");
            if (guiMaster.synth->getRuntime().midiEngine < 1)
                alert(guiMaster.synth, "Yoshimi could not connect to any MIDI system. Running with no MIDI.");
        }
        delete msg;
    }
}
#endif
