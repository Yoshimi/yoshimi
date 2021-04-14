/*
    Config.cpp - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2013, Nikita Zlobin
    Copyright 2014-2021, Will Godfrey & others

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

    This file is derivative of ZynAddSubFX original code.
*/

#include <sys/types.h>
#include <iostream>
#include <fenv.h>
#include <errno.h>
#include <cmath>
#include <string>
#include <argp.h>
#include <libgen.h>
#include <limits.h>

#if defined(__SSE__)
#include <xmmintrin.h>
#endif

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

using file::isRegularFile;
using file::createDir;
using file::copyDir;
using file::isDirectory;
using file::extendLocalPath;
using file::setExtension;
using file::renameFile;

using func::nearestPowerOf2;
using func::asString;
using func::string2int;

unsigned char panLaw = 1;

namespace { // constants used in the implementation
    string stateText = "load saved state, defaults to '$HOME/" + EXTEN::config + "/yoshimi/yoshimi.state'";
}

bool         Config::showSplash = true;
bool         Config::autoInstance = false;
unsigned int Config::activeInstance = 0;
int          Config::showCLIcontext = 1;

string jUuid = "";

Config::Config(SynthEngine *_synth, int argc, char **argv) :
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
    checksynthengines(1),
    xmlType(0),
    instrumentFormat(1),
    EnableProgChange(1), // default will be inverted
    toConsole(0),
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
    guiChanged(false),
    showCli(true),
    cliChanged(false),
    singlePath(false),
    banksChecked(false),
    panLaw(1),
    configChanged(false),
    rtprio(40),
    midi_bank_root(0), // 128 is used as 'disabled'
    midi_bank_C(32),
    midi_upper_voice_C(128),
    enable_part_on_voice_load(1),
    enable_NRPN(true),
    ignoreResetCCs(false),
    monitorCCin(false),
    showLearnedCC(true),
    single_row_panel(1),
    NumAvailableParts(NUM_MIDI_CHANNELS),
    currentPart(0),
    currentBank(0),
    currentRoot(0),
    bankHighlight(false),
    lastBankPart(UNUSED),
    currentPreset(0),
    tempBank(0),
    tempRoot(0),
    VUcount(0),
    channelSwitchType(0),
    channelSwitchCC(128),
    channelSwitchValue(0),
    nrpnL(127),
    nrpnH(127),
    nrpnActive(false),
    sigIntActive(0),
    ladi1IntActive(0),
    sse_level(0),
    programcommand("yoshimi"),
    synth(_synth),
    bRuntimeSetupCompleted(false),
    exitType(EXIT_SUCCESS)
{
    (void)argc;
    (void)**argv;
    std::cerr.precision(4);

    if (!loadConfig())
    {
        string message = "Could not load config. Using default values.";
        TextMsgBuffer::instance().push(message);
        Log("\n\n" + message + "\n");
    }

    rtprio = 4; // To force internal threads below LV2 host
    bRuntimeSetupCompleted = true;
    return;
}


Config::~Config()
{;}


void Config::flushLog(void)
{
    if (LogList.size())
    {
        while (LogList.size())
        {
            std::cout << LogList.front() << std::endl;
            LogList.pop_front();
        }
    }
}


void Config::clearPresetsDirlist(void)
{
    for (int i = 0; i < MAX_PRESET_DIRS; ++i)
        presetsDirlist[i].clear();
}


bool Config::loadConfig(void)
{
    string homedir = string(getenv("HOME"));
    if (homedir.empty() || !isDirectory(homedir))
    {
        homedir = string("/tmp");
        Log ("Failed to find 'Home' directory - using tmp.\nSettings will be lost on computer shutdown.");
    }
    userHome = homedir + '/';
    localDir = userHome + ".local/share/yoshimi";
    if (!isDirectory(localDir))
    {
        if (createDir(localDir))
        {
            Log("Failed to create local yoshimi directory.");
            return false;
        }
    }
    ConfigDir = userHome + string(EXTEN::config) + "/" + YOSHIMI;
    defaultStateName = ConfigDir + "/yoshimi";

    if (!isDirectory(ConfigDir))
    {
        if (createDir(ConfigDir))
        {
            Log("Failed to create config directory '" + ConfigDir + "'");
            return false;
        }
    }
    string yoshimi = "/" + string(YOSHIMI);

    baseConfig = ConfigDir + yoshimi + string(EXTEN::config);
    int thisInstance = synth->getUniqueId();
    defaultSession = defaultStateName + "-" + asString(thisInstance) + EXTEN::state;
    yoshimi += ("-" + asString(thisInstance));
    if (thisInstance == 0 && sessionStage != _SYS_::type::RestoreConf)
    {
        TextMsgBuffer::instance().init(); // sneaked it in here so it's early

        presetDir = localDir + "/presets";
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
        definedBankRoot = localDir + "/found/";
        if (!isDirectory(definedBankRoot))
        { // only ever want to do this once
            if (createDir(definedBankRoot))
                Log("Failed to create root directory '" + definedBankRoot + "'");
        }
    }

    ConfigFile = ConfigDir + yoshimi;

    if (thisInstance == 0 && sessionStage != _SYS_::type::RestoreConf)
    {
        string newInstance0 = ConfigDir + yoshimi + EXTEN::instance;
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
    ConfigFile += EXTEN::instance;

    if (!isRegularFile(baseConfig))
    {
        Log("Basic configuration " + baseConfig + " not found, will use default settings");
            defaultPresets();
    }

    bool isok = true;
    if (!isRegularFile(ConfigFile))
    {
        Log("Configuration " + ConfigFile + " not found, will use default settings");
        configChanged = true; // give the user the choice
    }
    else
    {
        XMLwrapper *xml = new XMLwrapper(synth, true);
        if (!xml)
            Log("loadConfig failed XMLwrapper allocation");
        else
        {
            // get base first
            isok = xml->loadXMLfile(baseConfig);
            if (isok)
                isok = extractBaseParameters(xml);
            else
                Log("loadConfig load base failed");
            delete xml;

            // now the instance data
            if (isok)
            {
                XMLwrapper *xml = new XMLwrapper(synth, true);
                if (!xml)
                    Log("loadConfig failed XMLwrapper allocation");
                else
                {
                    isok = xml->loadXMLfile(ConfigFile);
                    if (isok)
                        isok = extractConfigData(xml);
                    else
                        Log("loadConfig load instance failed");
                    delete xml;
                }
            }
            if (thisInstance == 0 && sessionStage != _SYS_::type::RestoreConf)
            {
                int currentVersion = lastXMLmajor * 10 + lastXMLminor;
                int storedVersion = MIN_CONFIG_MAJOR * 10 + MIN_CONFIG_MINOR;
                if (currentVersion < storedVersion)
                    oldConfig = true;
                else
                    oldConfig = false;
            }
        }
    }

    //std::cout << "Session Stage " << sessionStage << std::endl;

    if (sessionStage == _SYS_::type::RestoreConf)
        return true;

    if (sessionStage != _SYS_::type::Normal)
    {
        XMLwrapper *xml = new XMLwrapper(synth, true);
        if (!xml)
            Log("loadConfig failed XMLwrapper allocation");
        else
        {
            isok = xml->loadXMLfile(StateFile);
            if (isok)
            {
                if (sessionStage == _SYS_::type::StartupFirst)
                    sessionStage = _SYS_::type::StartupSecond;
                else if (sessionStage == _SYS_::type::JackFirst)
                    sessionStage = _SYS_::type::JackSecond;
                isok = extractConfigData(xml);
            }
            else
                Log("loadConfig load instance failed");
            delete xml;
        }
    }
    return isok;
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


void Config::defaultPresets(void)
{
    string presetdirs[]  = {
        //string(getenv("HOME")) + "/.local/yoshimi/presets",
        presetDir,
        extendLocalPath("/presets"),
        // The following is not a default one.
        //string(getenv("HOME")) + "/" + string(EXTEN::config) + "/yoshimi/presets",
        "/usr/share/yoshimi/presets",
        "/usr/local/share/yoshimi/presets",
        /*
         * We no longer include zyn presets as they changed the filenames.
        "/usr/share/zynaddsubfx/presets",
        "/usr/local/share/zynaddsubfx/presets",
        */
        "@end"
    };
    int i = 0;
    int actual = 0;
    while (presetdirs[i] != "@end")
    {
        if (isDirectory(presetdirs[i]))
        {
            Log(presetdirs[i], 2);
            presetsDirlist[actual] = presetdirs[i];
            ++actual;
        }
        ++i;
    }
}


bool Config::extractBaseParameters(XMLwrapper *xml)
{
    if (synth->getUniqueId() != 0)
        return true;

    if (!xml)
    {
        Log("extractConfigData on NULL");
        return false;
    }
    if (!xml->enterbranch("BASE_PARAMETERS"))
    {
        Log("extractConfigData, no BASE_PARAMETERS branch");
        return false;
    }

    if (!guiChanged)
        showGui = xml->getparbool("enable_gui", showGui);
    showSplash = xml->getparbool("enable_splash", showSplash);
    if (!cliChanged)
        showCli = xml->getparbool("enable_CLI", showCli);
    singlePath = xml->getparbool("enable_single_master", singlePath);
    banksChecked = xml->getparbool("banks_checked", banksChecked);
    autoInstance = xml->getparbool("enable_auto_instance", autoInstance);
    if (autoInstance)
        activeInstance = xml->getparU("active_instances", 0);
    else
        activeInstance = 1;
    showCLIcontext = xml->getpar("show_CLI_context", 1, 0, 2);
    GzipCompression = xml->getpar("gzip_compression", GzipCompression, 0, 9);

    // get preset dirs
    int count = 0;
    bool found = false;
    for (int i = 0; i < MAX_PRESET_DIRS; ++i)
    {
        if (xml->enterbranch("PRESETSROOT", i))
        {
            string dir = xml->getparstr("presets_root");
            if (isDirectory(dir))
            {
                presetsDirlist[count] = dir;
                found = true;
                ++count;
            }
            xml->exitbranch();
        }
    }
    if (!found)
    {
        defaultPresets();
        currentPreset = 0;
        configChanged = true; // give the user the choice
    }

    // the following three retained here for compatibility with old config type
    if (!rateChanged)
        Samplerate = xml->getpar("sample_rate", Samplerate, 44100, 192000);
    if (!bufferChanged)
        Buffersize = xml->getpar("sound_buffer_size", Buffersize, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);
    if (!oscilChanged)
        Oscilsize = xml->getpar("oscil_size", Oscilsize, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);


    xml->exitbranch(); // BaseParameters
    return true;
}

bool Config::extractConfigData(XMLwrapper *xml)
{
    if (!xml)
    {
        Log("extractConfigData on NULL");
        return false;
    }
    if (!xml->enterbranch("CONFIGURATION"))
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
        loadDefaultState = xml->getpar("defaultState", loadDefaultState, 0, 1);
        if (loadDefaultState)
        {
            xml->exitbranch(); // CONFIGURATION
            configChanged = true;
            sessionStage = _SYS_::type::Default;
            StateFile = defaultSession;
            Log("Loading default state");
            return true;
        }
    }

    if (sessionStage != _SYS_::type::InProgram)
    {

        if (!rateChanged)
            Samplerate = xml->getpar("sample_rate", Samplerate, 44100, 192000);
        if (!bufferChanged)
            Buffersize = xml->getpar("sound_buffer_size", Buffersize, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);
        if (!oscilChanged)
            Oscilsize = xml->getpar("oscil_size", Oscilsize, MIN_OSCIL_SIZE, MAX_OSCIL_SIZE);
        single_row_panel = xml->getpar("single_row_panel", single_row_panel, 0, 1);
        toConsole = xml->getpar("reports_destination", toConsole, 0, 1);
        hideErrors = xml->getpar("hide_system_errors", hideErrors, 0, 1);
        showTimes = xml->getpar("report_load_times", showTimes, 0, 1);
        logXMLheaders = xml->getpar("report_XMLheaders", logXMLheaders, 0, 1);
        VirKeybLayout = xml->getpar("virtual_keyboard_layout", VirKeybLayout, 1, 6) - 1;
        xmlmax = xml->getpar("full_parameters", xmlmax, 0, 1);

        // get legacy preset dirs
        int count = 0;
        for (int i = 0; i < MAX_PRESET_DIRS; ++i)
        {
            if (xml->enterbranch("PRESETSROOT", i))
            {
                string dir = xml->getparstr("presets_root");
                if (isDirectory(dir))
                {
                    presetsDirlist[count] = dir;
                    ++count;
                }
                xml->exitbranch();
            }
        }

        bankHighlight = xml->getparbool("bank_highlight", bankHighlight);

        currentPreset = xml->getpar("presetsCurrentRootID", currentPreset, 0, MAX_PRESETS);

        Interpolation = xml->getpar("interpolation", Interpolation, 0, 1);

        // engines
        if (!engineChanged)
            audioEngine = (audio_drivers)xml->getpar("audio_engine", audioEngine, no_audio, alsa_audio);
        if (!midiChanged)
            midiEngine = (midi_drivers)xml->getpar("midi_engine", midiEngine, no_midi, alsa_midi);
        alsaMidiType = xml->getpar("alsa_midi_type", 0, 0, 2);

        // alsa settings
        alsaAudioDevice = xml->getparstr("linux_alsa_audio_dev");
        alsaMidiDevice = xml->getparstr("linux_alsa_midi_dev");

        // jack settings
        jackServer = xml->getparstr("linux_jack_server");
        jackMidiDevice = xml->getparstr("linux_jack_midi_dev");
        if (!connectJackChanged)
            connectJackaudio = xml->getpar("connect_jack_audio", connectJackaudio, 0, 1);

        // midi options
        midi_bank_root = xml->getpar("midi_bank_root", midi_bank_root, 0, 128);
        midi_bank_C = xml->getpar("midi_bank_C", midi_bank_C, 0, 128);
        midi_upper_voice_C = xml->getpar("midi_upper_voice_C", midi_upper_voice_C, 0, 128);
        EnableProgChange = 1 - xml->getpar("ignore_program_change", EnableProgChange, 0, 1); // inverted for Zyn compatibility
        enable_part_on_voice_load = xml->getpar("enable_part_on_voice_load", enable_part_on_voice_load, 0, 1);
        instrumentFormat = xml->getpar("saved_instrument_format",instrumentFormat, 1, 3);
        enable_NRPN = xml->getparbool("enable_incoming_NRPNs", enable_NRPN);
        ignoreResetCCs = xml->getpar("ignore_reset_all_CCs",ignoreResetCCs,0, 1);
        monitorCCin = xml->getparbool("monitor-incoming_CCs", monitorCCin);
        showLearnedCC = xml->getparbool("open_editor_on_learned_CC", showLearnedCC);
    }
    if (tempRoot == 0)
        tempRoot = xml->getpar("root_current_ID", 0, 0, 127);

    if (tempBank == 0)
    tempBank = xml->getpar("bank_current_ID", 0, 0, 127);
    xml->exitbranch(); // CONFIGURATION
    return true;
}


bool Config::saveConfig(bool master)
{
    bool result = false;
    if (master)
    {
        //std::cout << "saving master" << std::endl;
        xmlType = TOPLEVEL::XML::MasterConfig;
        XMLwrapper *xml = new XMLwrapper(synth, true);
        if (!xml)
        {
            Log("saveConfig failed xml allocation", 2);
            return result;
        }
        string resConfigFile = baseConfig;

        if (xml->saveXMLfile(resConfigFile, false))
        {
            configChanged = false;
            result = true;
        }
        else
            Log("Failed to save master config to " + resConfigFile, 2);

        delete xml;
    }
    xmlType = TOPLEVEL::XML::Config;
    XMLwrapper *xml = new XMLwrapper(synth, true);
    if (!xml)
    {
        Log("saveConfig failed xml allocation", 2);
        return result;
    }
    addConfigXML(xml);
    string resConfigFile = ConfigFile;

    if (xml->saveXMLfile(resConfigFile))
    {
        configChanged = false;
        result = true;
    }
    else
        Log("Failed to save instance to " + resConfigFile, 2);

    delete xml;
    return result;
}


void Config::addConfigXML(XMLwrapper *xml)
{
    xml->beginbranch("CONFIGURATION");
    xml->addpar("defaultState", loadDefaultState);

    xml->addpar("sample_rate", synth->getRuntime().Samplerate);
    xml->addpar("sound_buffer_size", synth->getRuntime().Buffersize);
    xml->addpar("oscil_size", synth->getRuntime().Oscilsize);

    xml->addpar("single_row_panel", single_row_panel);
    xml->addpar("reports_destination", toConsole);
    xml->addpar("hide_system_errors", hideErrors);
    xml->addpar("report_load_times", showTimes);
    xml->addpar("report_XMLheaders", logXMLheaders);
    xml->addpar("virtual_keyboard_layout", VirKeybLayout + 1);
    xml->addpar("full_parameters", xmlmax);

    xml->addparbool("bank_highlight", bankHighlight);

    xml->addpar("presetsCurrentRootID", currentPreset);

    xml->addpar("interpolation", Interpolation);

    xml->addpar("audio_engine", synth->getRuntime().audioEngine);
    xml->addpar("midi_engine", synth->getRuntime().midiEngine);
    xml->addpar("alsa_midi_type", synth->getRuntime().alsaMidiType);

    xml->addparstr("linux_alsa_audio_dev", alsaAudioDevice);
    xml->addparstr("linux_alsa_midi_dev", alsaMidiDevice);

    xml->addparstr("linux_jack_server", jackServer);
    xml->addparstr("linux_jack_midi_dev", jackMidiDevice);
    xml->addpar("connect_jack_audio", connectJackaudio);

    xml->addpar("midi_bank_root", midi_bank_root);
    xml->addpar("midi_bank_C", midi_bank_C);
    xml->addpar("midi_upper_voice_C", midi_upper_voice_C);
    xml->addpar("ignore_program_change", (1 - EnableProgChange));
    xml->addpar("enable_part_on_voice_load", enable_part_on_voice_load);
    xml->addpar("saved_instrument_format", instrumentFormat);
    xml->addparbool("enable_incoming_NRPNs", enable_NRPN);
    xml->addpar("ignore_reset_all_CCs",ignoreResetCCs);
    xml->addparbool("monitor-incoming_CCs", monitorCCin);
    xml->addparbool("open_editor_on_learned_CC",showLearnedCC);
    xml->addpar("check_pad_synth", checksynthengines);
    xml->addpar("root_current_ID", synth->ReadBankRoot());
    xml->addpar("bank_current_ID", synth->ReadBank());
    xml->endbranch(); // CONFIGURATION
}


bool Config::saveSessionData(string savefile)
{
    savefile = setExtension(savefile, EXTEN::state);
    synth->getRuntime().xmlType = TOPLEVEL::XML::State;
    XMLwrapper *xml = new XMLwrapper(synth, true);
    if (!xml)
    {
        Log("saveSessionData failed xml allocation", 3);
        return false;
    }
    bool ok = true;
    addConfigXML(xml);
    synth->add2XML(xml);
    synth->midilearn.insertMidiListData(xml);
    if (xml->saveXMLfile(savefile))
        Log("Session data saved to " + savefile, 2);
    else
    {
        ok = false;
        Log("Failed to save session data to " + savefile, 2);
    }
    if (xml)
        delete xml;
    return ok;
}


bool Config::restoreSessionData(string sessionfile)
{
    XMLwrapper *xml = NULL;
    bool ok = false;

    if (sessionfile.size() && !isRegularFile(sessionfile))
        sessionfile = setExtension(sessionfile, EXTEN::state);
    if (!sessionfile.size() || !isRegularFile(sessionfile))
    {
        Log("Session file " + sessionfile + " not available", 2);
        goto end_game;
    }
    if (!(xml = new XMLwrapper(synth, true)))
    {
        Log("Failed to init xml for restoreState", 3);
        goto end_game;
    }
    if (!xml->loadXMLfile(sessionfile))
    {
        Log("Failed to load xml file " + sessionfile, 2);
        goto end_game;
    }


    ok = extractConfigData(xml);
    if (ok)
    {
        // mark as soon as anything changes
        synth->getRuntime().stateChanged = true;
        for (int npart = 0; npart < NUM_MIDI_PARTS; ++ npart)
        {
            synth->part[npart]->defaults();
            synth->part[npart]->Prcvchn = npart % NUM_MIDI_CHANNELS;
        }
        ok = synth->getfromXML(xml);
        if (ok)
            synth->setAllPartMaps();
        bool oklearn = synth->midilearn.extractMidiListData(false, xml);
        if (oklearn)
            synth->midilearn.updateGui(MIDILEARN::control::hideGUI);
            // handles possibly undefined window
        }


end_game:
    if (xml)
        delete xml;
    return ok;
}


void Config::Log(const string& msg, char tostderr)
{
    if ((tostderr & 2) && hideErrors)
        return;
    if (showGui && !(tostderr & 1) && toConsole)
        LogList.push_back(msg);
    else if (!(tostderr & 1))
        std::cout << msg << std::endl; // normal log

    else
        std::cerr << msg << std::endl; // error log
}

void Config::LogError(const string &msg)
{
    Log("[ERROR] " + msg, 1);
}

void Config::StartupReport(const string& clientName)
{
    bool fullInfo = (synth->getUniqueId() == 0);
    if (fullInfo)
    {
        //Log(argline);
        //Log("Build Number " + std::to_string(BUILD_NUMBER), 1);
    }
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
    Log(report, 2);
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
    Log(report, 2);
    if (fullInfo)
    {
        Log("Oscilsize: " + asString(synth->oscilsize), 2);
        Log("Samplerate: " + asString(synth->samplerate), 2);
        Log("Period size: " + asString(synth->buffersize), 2);
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
                                + " (" + asString(chk) + ")", 1);
                    schedfifo = false;
                    continue;
                }
                if ((chk = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)))
                {
                    Log("Failed to set inherit scheduler thread attribute "
                                + string(strerror(errno)) + " ("
                                + asString(chk) + ")", 1);
                    schedfifo = false;
                    continue;
                }
                sched_param prio_params;
                int prio = rtprio - priodec;
                if (prio < 1)
                    prio = 1;
                Log(name + " priority is " + std::to_string(prio), 1);
                prio_params.sched_priority = prio;
                if ((chk = pthread_attr_setschedparam(&attr, &prio_params)))
                {
                    Log("Failed to set thread priority attribute ("
                                + asString(chk) + ")  ", 3);
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
            Log("Failed to initialise thread attributes " + asString(chk), 1);

        if (schedfifo)
        {
            Log("Failed to start thread (sched_fifo) " + asString(chk)
                + "  " + string(strerror(errno)), 1);
            schedfifo = false;
            continue;
        }
        Log("Failed to start thread (sched_other) " + asString(chk)
            + "  " + string(strerror(errno)), 1);
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
    Log("Interrupt received", 1);
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


int Config::SSEcapability(void)
{
    #if !defined(__SSE__)
        return 0;
    #else
        #if defined(__x86_64__)
            int64_t edx;
            __asm__ __volatile__ (
                "mov %%rbx,%%rdi\n\t" // save PIC register
                "movl $1,%%eax\n\t"
                "cpuid\n\t"
                "mov %%rdi,%%rbx\n\t" // restore PIC register
                : "=d" (edx)
                : : "%rax", "%rcx", "%rdi"
            );
        #else
            int32_t edx;
            __asm__ __volatile__ (
                "movl %%ebx,%%edi\n\t" // save PIC register
                "movl $1,%%eax\n\t"
                "cpuid\n\t"
                "movl %%edi,%%ebx\n\t" // restore PIC register
                : "=d" (edx)
                : : "%eax", "%ecx", "%edi"
            );
        #endif
        return ((edx & 0x02000000 /*SSE*/) | (edx & 0x04000000 /*SSE2*/)) >> 25;
    #endif
}

/*
 * The code below has been replaced with specific anti-denormal code where needed.
 * Although the new code is slightly less efficient it is compatible across platforms,
 * where as the 'daz' processor code is not available on platforms such as ARM.
 */

/*void Config::AntiDenormals(bool set_daz_ftz)
{
    return;
    if (synth->getIsLV2Plugin())
    {
        return;// no need to set floating point rules for lv2 - host should control it.
    }
    #if defined(__SSE__)
        if (set_daz_ftz)
        {
            sse_level = SSEcapability();
            if (sse_level & 0x01)
                // SSE, turn on flush to zero (FTZ) and round towards zero (RZ)
                _mm_setcsr(_mm_getcsr() | 0x8000|0x6000);
            if (sse_level & 0x02)
                // SSE2, turn on denormals are zero (DAZ)
               _mm_setcsr(_mm_getcsr() | 0x0040);
        }
        else if (sse_level)
        {
            // Clear underflow and precision flags,
            // turn DAZ, FTZ off, restore round to nearest (RN)
            _mm_setcsr(_mm_getcsr() & ~(0x0030|0x8000|0x0040|0x6000));
        }
    #endif
}*/


/**
SSEcapability() and AntiDenormals() draw gratefully on the work of others,
including:

Jens M Andreasen, LAD, <http://lists.linuxaudio.org/pipermail/linux-audio-dev/2009-August/024707.html>).

LinuxSampler src/common/Features.cpp, licensed thus -

 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
 *   Copyright (C) 2005 - 2008 Christian Schoenebeck                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
**/


/*static error_t parse_cmds (int key, char *arg, struct argp_state *state)
{
    Config *settings = (Config*)state->input;
    if (arg && arg[0] == 0x3d)
        ++ arg;
    int num;

    switch (key)
    {
        case 'N': settings->nameTag = string(arg); break;

        case 'l': settings->paramsLoad = string(arg); break;

        case 'L': settings->instrumentLoad = string(arg); break;

        case 'M':settings->midiLearnLoad = string(arg);break;

        case 'A':
            settings->configChanged = true;
            settings->engineChanged = true;
            settings->audioEngine = alsa_audio;
            if (arg)
                settings->audioDevice = string(arg);
            else
                settings->audioDevice = settings->alsaAudioDevice;
            break;

        case 'a':
            settings->configChanged = true;
            settings->midiChanged = true;
            settings->midiEngine = alsa_midi;
            if (arg)
                settings->midiDevice = string(arg);
            else
                settings->midiDevice = string(settings->alsaMidiDevice);
            break;

        case 'b':
            settings->configChanged = true;
            settings->bufferChanged = true;
            settings->Buffersize = string2int(string(arg));
            break;

        case 'D':
            if (arg)
                settings->rootDefine = string(arg);
            break;

        case 'c':
            settings->configChanged = true;
            settings->cliChanged = true;
            settings->showCli = false;
            break;

        case 'C':
            settings->configChanged = true;
            settings->cliChanged = true;
            settings->showCli = true;
            break;

        case 'i':
            settings->configChanged = true;
            settings->guiChanged = true;
            settings->showGui = false;
            break;

        case 'I':
            settings->configChanged = true;
            settings->guiChanged = true;
            settings->showGui = true;
            break;

        case 'J':
            settings->configChanged = true;
            settings->engineChanged = true;
            settings->audioEngine = jack_audio;
            if (arg)
                settings->audioDevice = string(arg);
            break;

        case 'j':
            settings->configChanged = true;
            settings->midiChanged = true;
            settings->midiEngine = jack_midi;
            if (arg)
                settings->midiDevice = string(arg);
            else
                settings->midiDevice = string(settings->jackMidiDevice);
            break;

        case 'k':
            settings->startJack = true;
            break;

        case 'K':
            settings->configChanged = true;
            settings->connectJackChanged = true;
            settings->connectJackaudio = true;
            break;

        case 'o':
            settings->configChanged = true;
            settings->oscilChanged = true;
            settings->Oscilsize = string2int(string(arg));
            break;

        case 'R':
            settings->configChanged = true;
            settings->rateChanged = true;
            num = (string2int(string(arg)) / 48 ) * 48;
            if (num < 48000 || num > 192000)
                num = 44100; // play safe
            settings->Samplerate = num;
            break;

        case 'S':
            if (arg)
            {
                settings->sessionStage = _SYS_::type::StartupFirst;
                settings->configChanged = true;
                settings->StateFile = string(arg);
            }
            break;

#if defined(JACK_SESSION)
        case 'u':
            if (arg)
            {
                settings->sessionStage = _SYS_::type::JackFirst;
                settings->configChanged = true;
                settings->StateFile = setExtension(string(arg), EXTEN::state);
            }
            break;

        case 'U':
                if (arg)
                    jUuid = string(arg);
        break;
#endif

        case ARGP_KEY_ARG:
        case ARGP_KEY_END:
            break;

        default:
            return error_t(ARGP_ERR_UNKNOWN);
    }
    return error_t(0);
}


static struct argp cmd_argp = { cmd_options, parse_cmds, prog_doc, 0, 0, 0, 0};


void Config::loadCmdArgs(int argc, char **argv)
{
    argp_parse(&cmd_argp, argc, argv, 0, 0, this);
    if (jackSessionUuid.size() && jackSessionFile.size())
        restoreJackSession = true;
}*/

#ifdef GUI_FLTK
void GuiThreadMsg::processGuiMessages()
{
    GuiThreadMsg *msg = (GuiThreadMsg *)Fl::thread_message();
    if (msg)
    {
        SynthEngine *synth = ((SynthEngine *)msg->data);
        MasterUI *guiMaster = synth->getGuiMaster((msg->type == GuiThreadMsg::NewSynthEngine));
        if (msg->type == GuiThreadMsg::NewSynthEngine)
        {
            // This *defines* guiMaster
            if (!guiMaster)
                std::cerr << "Error starting Main UI!" << std::endl;
            else
                guiMaster->Init(guiMaster->getSynth()->getWindowTitle().c_str());
        }
        else if (guiMaster)
        {
            switch(msg->type)
            {
                default:
                    break;
            }
        }
        delete msg;
    }
}
#endif
