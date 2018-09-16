/*
    Config.cpp - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2013, Nikita Zlobin
    Copyright 2014-2018, Will Godfrey & others

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

    Modified September 2018
*/

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

using namespace std;

#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "MasterUI.h"
#include "ConfBuild.h"

static char prog_doc[] =
    "Yoshimi " YOSHIMI_VERSION ", a derivative of ZynAddSubFX - "
    "Copyright 2002-2009 Nasca Octavian Paul and others, "
    "Copyright 2009-2011 Alan Calvert, "
    "Copyright 20012-2013 Jeremy Jongepier and others, "
    "Copyright 20014-2017 Will Godfrey and others";
string argline = "Yoshimi " + (string) YOSHIMI_VERSION;// + "\nBuild Number " + to_string(BUILD_NUMBER);
const char* argp_program_version = argline.c_str();

static struct argp_option cmd_options[] = {
    {"alsa-audio",        'A',  "<device>",   1,  "use alsa audio output", 0},
    {"alsa-midi",         'a',  "<device>",   1,  "use alsa midi input", 0},
    {"define-root",       'D',  "<path>",     0,  "define path to new bank root" , 0},
    {"buffersize",        'b',  "<size>",     0,  "set internal buffer size", 0 },
    {"no-gui",            'i',  NULL,         0,  "disable gui", 0},
    {"gui",               'I',  NULL,         0,  "enable gui", 0},
    {"no-cmdline",        'c',  NULL,         0,  "disable command line interface", 0},
    {"cmdline",           'C',  NULL,         0,  "enable command line interface", 0},
    {"jack-audio",        'J',  "<server>",   1,  "use jack audio output", 0},
    {"jack-midi",         'j',  "<device>",   1,  "use jack midi input", 0},
    {"autostart-jack",    'k',  NULL,         0,  "auto start jack server", 0},
    {"auto-connect",      'K',  NULL,         0,  "auto connect jack audio", 0},
    {"load",              'l',  "<file>",     0,  "load .xmz file", 0},
    {"load-instrument",   'L',  "<file>",     0,  "load .xiz file", 0},
    {"name-tag",          'N',  "<tag>",      0,  "add tag to clientname", 0},
    {"samplerate",        'R',  "<rate>",     0,  "set alsa audio sample rate", 0},
    {"oscilsize",         'o',  "<size>",     0,  "set AddSynth oscilator size", 0},
    {"state",             'S',  "<file>",     1,  "load saved state, defaults to '$HOME/.config/yoshimi/yoshimi.state'", 0},
    #if defined(JACK_SESSION)
        {"jack-session-uuid", 'U',  "<uuid>",     0,  "jack session uuid", 0},
        {"jack-session-file", 'u',  "<file>",     0,  "load named jack session file", 0},
    #endif
    { 0, 0, 0, 0, 0, 0}
};

unsigned int Config::Samplerate = 48000;
unsigned int Config::Buffersize = 256;
unsigned int Config::Oscilsize = 512;
unsigned int Config::GzipCompression = 3;
bool         Config::showGui = true;
bool         Config::showSplash = true;
bool         Config::showCLI = true;
bool         Config::autoInstance = false;
unsigned int Config::activeInstance = 0;
int          Config::showCLIcontext = 1;

Config::Config(SynthEngine *_synth, int argc, char **argv) :
    restoreState(false),
    stateChanged(false),
    restoreJackSession(false),
    oldConfig(false),
    runSynth(true),
    finishedCLI(true),
    VirKeybLayout(0),
    audioEngine(DEFAULT_AUDIO),
    midiEngine(DEFAULT_MIDI),
    audioDevice("default"),
    midiDevice("default"),
    jackServer("default"),
    jackMidiDevice("default"),
    startJack(false),
    connectJackaudio(true),
    alsaAudioDevice("default"),
    alsaMidiDevice("default"),
    loadDefaultState(false),
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
    programcommand(string("yoshimi")),
    synth(_synth),
    bRuntimeSetupCompleted(false)
{
    if (synth->getIsLV2Plugin())
    {
        rtprio = 4; // To force internal threads below LV2 host
    }
    else
        fesetround(FE_TOWARDZERO); // Special thanks to Lars Luthman for conquering
                               // the heffalump. We need lrintf() to round
                               // toward zero.
    //^^^^^^^^^^^^^^^ This call is not needed aymore (at least for lv2 plugin)
    //as all calls to lrintf() are replaced with (int)truncf()
    //which befaves exactly the same when flag FE_TOWARDZERO is set

    cerr.precision(4);
    bRuntimeSetupCompleted = Setup(argc, argv);
}


bool Config::Setup(int argc, char **argv)
{
    clearPresetsDirlist();
    AntiDenormals(true);

    if (!loadConfig())
        return false;

    if (synth->getIsLV2Plugin()) //skip further setup for lv2 plugin instance.
    {
        /*
         * These are needed here now, as for stand-alone they have
         * been moved to main to give the users the impression of
         * a faster startup, and reduce the likelyhood of thinking
         * they failed and trying to start again.
         */
        synth->installBanks();
        synth->loadHistory();
        return true;
    }

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
    loadCmdArgs(argc, argv);
    Oscilsize = nearestPowerOf2(Oscilsize, MAX_AD_HARMONICS * 2, 16384);
    Buffersize = nearestPowerOf2(Buffersize, 16, 4096);
    //Log(asString(Oscilsize));
    //Log(asString(Buffersize));

    if (restoreState)
    {
        char *fp = NULL;
        if (!StateFile.size())
            goto no_state;

        fp = realpath (StateFile.c_str(), NULL);
        if (fp == NULL)
            goto no_state;

        StateFile = fp;
        free (fp);
        if (!isRegFile(StateFile))
        {
            no_state: Log("Invalid state file specified for restore " + StateFile, 2);
            return true;
        }
        Log("Using " + StateFile);
        restoreSessionData(StateFile, true);
        /* There is a single state file that contains both startup config
         * data that must be set early, and runtime data that must be set
         * after synth has been initialised.
         *
         * We open it here and fetch just the essential BASE_PARAMETERS;
         * buffer size, oscillator size, sample rate. We then reopen it
         * in synth and fetch the remaining settings from CONFIG.
         */
    }
    return true;
}


Config::~Config()
{
    AntiDenormals(false);
}


void Config::flushLog(void)
{
    if (LogList.size())
    {
        while (LogList.size())
        {
            cerr << LogList.front() << endl;
            LogList.pop_front();
        }
    }
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


void Config::clearPresetsDirlist(void)
{
    for (int i = 0; i < MAX_PRESET_DIRS; ++i)
        presetsDirlist[i].clear();
}


bool Config::loadConfig(void)
{
    string cmd;
    int chk;
    string homedir = string(getenv("HOME"));
    if (homedir.empty() || !isDirectory(homedir))
        homedir = string("/tmp");
    userHome = homedir + '/';
    ConfigDir = homedir + string("/.config/") + YOSHIMI;
    defaultStateName = ConfigDir + "/yoshimi";
    if (!isDirectory(ConfigDir))
    {
        cmd = string("mkdir -p ") + ConfigDir;
        if ((chk = system(cmd.c_str())) < 0)
        {
            Log("Create config directory " + ConfigDir + " failed, status " + asString(chk));
            return false;
        }
    }
    string yoshimi = "/" + string(YOSHIMI);

    string baseConfig = ConfigDir + yoshimi + ".config";
    int thisInstance = synth->getUniqueId();
    if (thisInstance > 0)
        yoshimi += ("-" + asString(thisInstance));
    else
        miscMsgInit(); // sneaked it in here so it's early
    string presetDir = ConfigDir + "/presets";
    if (!isDirectory(presetDir))
    {
        cmd = string("mkdir -p ") + presetDir;
        if ((chk = system(cmd.c_str())) < 0)
            Log("Create preset directory " + presetDir + " failed, status " + asString(chk));
    }

    ConfigFile = ConfigDir + yoshimi;
    StateFile = ConfigDir + yoshimi + string(".state");

    if (thisInstance == 0)
        ConfigFile = baseConfig;
    else
        ConfigFile += ".instance";

    if (!isRegFile(baseConfig))
    {
        Log("Basic configuration " + baseConfig + " not found, will use default settings");
        defaultPresets();
    }

    bool isok = true;
    if (!isRegFile(ConfigFile))
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
            if (!xml->loadXMLfile(baseConfig))
            {
                if (thisInstance > 0)
                {
                    Log("loadConfig loadXMLfile failed");
                    return false;
                }
            }
            isok = extractBaseParameters(xml);
            delete xml;
            if (isok)
            {
                XMLwrapper *xml = new XMLwrapper(synth, true);
                isok = xml->loadXMLfile(ConfigFile);
                if (isok)
                {
                    isok = extractConfigData(xml);
                    delete xml;
                }
            }
            if (thisInstance == 0)
            {
                if (lastXMLmajor < MIN_CONFIG_MAJOR || lastXMLminor < MIN_CONFIG_MINOR)
                    oldConfig = true;
                else
                    oldConfig = false;
            }
        }
    }
    return isok;
}


void Config::defaultPresets(void)
{
    string presetdirs[]  = {
        "/usr/share/yoshimi/presets",
        "/usr/local/share/yoshimi/presets",
        "/usr/share/zynaddsubfx/presets",
        "/usr/local/share/zynaddsubfx/presets",
        string(getenv("HOME")) + "/.config/yoshimi/presets",
        localPath("/presets"),
        "end"
    };
    int i = 0;
    while (presetdirs[i] != "end")
    {
        if (isDirectory(presetdirs[i]))
        {
            Log(presetdirs[i], 2);
            presetsDirlist[i] = presetdirs[i];
        }
        ++ i;
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
    Samplerate = xml->getpar("sample_rate", Samplerate, 44100, 192000);
    Buffersize = xml->getpar("sound_buffer_size", Buffersize, 16, 4096);
    Oscilsize = xml->getpar("oscil_size", Oscilsize, MAX_AD_HARMONICS * 2, 16384);
    GzipCompression = xml->getpar("gzip_compression", GzipCompression, 0, 9);
    showGui = xml->getparbool("enable_gui", showGui);
    showSplash = xml->getparbool("enable_splash", showSplash);
    showCLI = xml->getparbool("enable_CLI", showCLI);
    autoInstance = xml->getparbool("enable_auto_instance", autoInstance);
    activeInstance = xml->getparU("active_instances", 0);
    showCLIcontext = xml->getpar("show_CLI_context", 1, 0, 2);
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
    single_row_panel = xml->getpar("single_row_panel", single_row_panel, 0, 1);
    toConsole = xml->getpar("reports_destination", toConsole, 0, 1);
    hideErrors = xml->getpar("hide_system_errors", hideErrors, 0, 1);
    showTimes = xml->getpar("report_load_times", showTimes, 0, 1);
    logXMLheaders = xml->getpar("report_XMLheaders", logXMLheaders, 0, 1);
    VirKeybLayout = xml->getpar("virtual_keyboard_layout", VirKeybLayout, 1, 6) - 1;
    xmlmax = xml->getpar("full_parameters", xmlmax, 0, 1);

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
                presetsDirlist[count++] = dir;
                found = true;
            }
            xml->exitbranch();
        }
    }
    if (!found)
    {
        defaultPresets();
        configChanged = true; // give the user the choice
    }

    loadDefaultState = xml->getpar("defaultState", loadDefaultState, 0, 1);
    Interpolation = xml->getpar("interpolation", Interpolation, 0, 1);

    // engines
    audioEngine = (audio_drivers)xml->getpar("audio_engine", audioEngine, no_audio, alsa_audio);
    midiEngine = (midi_drivers)xml->getpar("midi_engine", midiEngine, no_midi, alsa_midi);

    // alsa settings
    alsaAudioDevice = xml->getparstr("linux_alsa_audio_dev");
    alsaMidiDevice = xml->getparstr("linux_alsa_midi_dev");

    // jack settings
    jackServer = xml->getparstr("linux_jack_server");
    jackMidiDevice = xml->getparstr("linux_jack_midi_dev");
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

    //misc
    checksynthengines = xml->getpar("check_pad_synth", checksynthengines, 0, 1);
    if (tempRoot == 0)
        tempRoot = xml->getpar("root_current_ID", 0, 0, 127);
    //else
        //cout << "root? " << xml->getpar("root_current_ID", 0, 0, 127) << endl;
    if (tempBank == 0)
    tempBank = xml->getpar("bank_current_ID", 0, 0, 127);
    xml->exitbranch(); // CONFIGURATION
    return true;
}


bool Config::saveConfig(void)
{
    bool result = false;
    xmlType = XML_CONFIG;
    XMLwrapper *xmltree = new XMLwrapper(synth, true);
    if (!xmltree)
    {
        Log("saveConfig failed xmltree allocation", 2);
        return result;
    }
    addConfigXML(xmltree);
    string resConfigFile = ConfigFile;

    if (xmltree->saveXMLfile(resConfigFile))
    {
        configChanged = false;
        result = true;
    }
    else
        Log("Failed to save config to " + resConfigFile, 2);

    delete xmltree;
    return result;
}


void Config::addConfigXML(XMLwrapper *xmltree)
{
    xmltree->beginbranch("CONFIGURATION");
    xmltree->addpar("single_row_panel", single_row_panel);
    xmltree->addpar("reports_destination", toConsole);
    xmltree->addpar("hide_system_errors", hideErrors);
    xmltree->addpar("report_load_times", showTimes);
    xmltree->addpar("report_XMLheaders", logXMLheaders);
    xmltree->addpar("virtual_keyboard_layout", VirKeybLayout + 1);
    xmltree->addpar("full_parameters", xmlmax);

    for (int i = 0; i < MAX_PRESET_DIRS; ++i)
    {
        if (presetsDirlist[i].size())
        {
            xmltree->beginbranch("PRESETSROOT",i);
            xmltree->addparstr("presets_root", presetsDirlist[i]);
            xmltree->endbranch();
        }
    }
    xmltree->addpar("defaultState", loadDefaultState);
    xmltree->addpar("interpolation", Interpolation);

    xmltree->addpar("audio_engine", synth->getRuntime().audioEngine);
    xmltree->addpar("midi_engine", synth->getRuntime().midiEngine);

    xmltree->addparstr("linux_alsa_audio_dev", alsaAudioDevice);
    xmltree->addparstr("linux_alsa_midi_dev", alsaMidiDevice);

    xmltree->addparstr("linux_jack_server", jackServer);
    xmltree->addparstr("linux_jack_midi_dev", jackMidiDevice);
    xmltree->addpar("connect_jack_audio", connectJackaudio);

    xmltree->addpar("midi_bank_root", midi_bank_root);
    xmltree->addpar("midi_bank_C", midi_bank_C);
    xmltree->addpar("midi_upper_voice_C", midi_upper_voice_C);
    xmltree->addpar("ignore_program_change", (1 - EnableProgChange));
    xmltree->addpar("enable_part_on_voice_load", enable_part_on_voice_load);
    xmltree->addpar("saved_instrument_format", instrumentFormat);
    xmltree->addparbool("enable_incoming_NRPNs", enable_NRPN);
    xmltree->addpar("ignore_reset_all_CCs",ignoreResetCCs);
    xmltree->addparbool("monitor-incoming_CCs", monitorCCin);
    xmltree->addparbool("open_editor_on_learned_CC",showLearnedCC);
    xmltree->addpar("check_pad_synth", checksynthengines);
    xmltree->addpar(string("root_current_ID"), synth->ReadBankRoot());
    xmltree->addpar(string("bank_current_ID"), synth->ReadBank());
    xmltree->endbranch(); // CONFIGURATION
}


bool Config::saveSessionData(string savefile)
{
    savefile = setExtension(savefile, "state");
    synth->getRuntime().xmlType = XML_STATE;
    XMLwrapper *xmltree = new XMLwrapper(synth, true);
    if (!xmltree)
    {
        Log("saveSessionData failed xmltree allocation", 3);
        return false;
    }
    bool ok = true;
    addConfigXML(xmltree);
    synth->add2XML(xmltree);
    synth->midilearn.insertMidiListData(false, xmltree);
    if (xmltree->saveXMLfile(savefile))
        Log("Session data saved to " + savefile, 2);
    else
    {
        ok = false;
        Log("Failed to save session data to " + savefile, 2);
    }
    if (xmltree)
        delete xmltree;
    return ok;
}


bool Config::restoreSessionData(string sessionfile, bool startup)
{
    XMLwrapper *xml = NULL;
    bool ok = false;

    if (sessionfile.size() && !isRegFile(sessionfile))
        sessionfile = setExtension(sessionfile, "state");
    if (!sessionfile.size() || !isRegFile(sessionfile))
    {
        Log("Session file " + sessionfile + " not available", 2);
        goto end_game;
    }
    if (!(xml = new XMLwrapper(synth, true)))
    {
        Log("Failed to init xmltree for restoreState", 3);
        goto end_game;
    }
    if (!xml->loadXMLfile(sessionfile))
    {
        Log("Failed to load xml file " + sessionfile, 2);
        goto end_game;
    }
    if (startup)
        ok = extractBaseParameters(xml);
    else
    {
        ok = extractConfigData(xml); // this still needs improving
        if (ok)
        { // mark as soon as anything changes
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
                synth->midilearn.updateGui(2);
                // handles possibly undefined window
        }
    }

end_game:
    if (xml)
        delete xml;
    return ok;
}


void Config::Log(const string &msg, char tostderr)
{
    if ((tostderr & 2) && hideErrors)
        return;
    if (showGui && !(tostderr & 1) && toConsole)
        LogList.push_back(msg);
    else if (!tostderr & 1)
        cout << msg << endl; // normal log

    else
        cerr << msg << endl; // error log
}

void Config::LogError(const string &msg)
{
    Log("[ERROR] " + msg, 1);
}

#ifndef YOSHIMI_LV2_PLUGIN
void Config::StartupReport(MusicClient *musicClient)
{
    bool fullInfo = (synth->getUniqueId() == 0);
    if (fullInfo)
    {
        Log(argline);
        Log("Build Number " + to_string(BUILD_NUMBER), 1);
    }
    Log("Clientname: " + musicClient->midiClientName());
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
#endif


void Config::setRtprio(int prio)
{
    if (prio < rtprio)
        rtprio = prio;
}


// general thread start service
bool Config::startThread(pthread_t *pth, void *(*thread_fn)(void*), void *arg,
                         bool schedfifo, char priodec, bool create_detached, string name)
{
    pthread_attr_t attr;
    int chk;
    bool outcome = false;
    bool retry = true;
    while (retry)
    {
        if (!(chk = pthread_attr_init(&attr)))
        {
            if (create_detached)
            {
               chk = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            }
            if (!chk)
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
                    Log(name + " priority is " + to_string(prio), 1);
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
                Log("Failed to set thread detach state " + asString(chk), 1);
            pthread_attr_destroy(&attr);
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
        return restoreSessionData(jackSessionFile, false);
    #else
        return false;
    #endif
}


void Config::setJackSessionSave(int event_type, string session_file)
{
    jackSessionFile = session_file;
    __sync_and_and_fetch(&jsessionSave, 0);
    __sync_or_and_fetch(&jsessionSave, event_type);
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


void Config::AntiDenormals(bool set_daz_ftz)
{
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
}


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


static error_t parse_cmds (int key, char *arg, struct argp_state *state)
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

        case 'A':
            settings->configChanged = true;
            settings->audioEngine = alsa_audio;
            if (arg)
                settings->audioDevice = string(arg);
            else
                settings->audioDevice = settings->alsaAudioDevice;
            break;

        case 'a':
            settings->configChanged = true;
            settings->midiEngine = alsa_midi;
            if (arg)
                settings->midiDevice = string(arg);
            else
                settings->midiDevice = string(settings->alsaMidiDevice);
            break;

        case 'b':
            settings->configChanged = true;
            settings->Buffersize = Config::string2int(string(arg));
            break;

        case 'D':
            if (arg)
                settings->rootDefine = string(arg);
            break;

        case 'c':
            settings->configChanged = true;
            settings->showCLI = false;
            break;

        case 'C':
            settings->configChanged = true;
            settings->showCLI = true;
            break;

        case 'i':
            settings->configChanged = true;
            settings->showGui = false;
            break;

        case 'I':
            settings->configChanged = true;
            settings->showGui = true;
            break;

        case 'J':
            settings->configChanged = true;
            settings->audioEngine = jack_audio;
            if (arg)
                settings->audioDevice = string(arg);
            break;

        case 'j':
            settings->configChanged = true;
            settings->midiEngine = jack_midi;
            if (arg)
                settings->midiDevice = string(arg);
            else
                settings->midiDevice = string(settings->jackMidiDevice);
            break;

        case 'k': settings->startJack = true; break;

        case 'K': settings->connectJackaudio = true; break;

        case 'o':
            settings->configChanged = true;
            settings->Oscilsize = Config::string2int(string(arg));
            break;

        case 'R':
            settings->configChanged = true;
            num = (Config::string2int(string(arg)) / 48 ) * 48;
            if (num < 48000 || num > 192000)
                num = 44100; // play safe
            settings->Samplerate = num;
            break;

        case 'S':
            settings->configChanged = true;
            settings->restoreState = true;
            if (arg)
                settings->StateFile = string(arg);
            break;

#if defined(JACK_SESSION)
        case 'u':
            if (arg)
                settings->jackSessionFile = string(arg);
            break;

        case 'U':
                if (arg)
                    settings->jackSessionUuid = string(arg);
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
}


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
                cerr << "Error starting Main UI!" << endl;
            else
                guiMaster->Init(guiMaster->getSynth()->getWindowTitle().c_str());
        }
        else if (guiMaster)
        {
            switch(msg->type)
            {

                case GuiThreadMsg::UpdateMaster:
                    guiMaster->refresh_master_ui(msg->index);
                    break;

                case GuiThreadMsg::UpdateConfig:
                    if (guiMaster->configui)
                        guiMaster->configui->update_config(msg->index);
                    break;

                case GuiThreadMsg::UpdatePaths:
                    guiMaster->updatepaths(msg->index);
                    break;

                case GuiThreadMsg::UpdatePart:
                    guiMaster->updatepart();
                    guiMaster->updatepanel();
                    break;

                case GuiThreadMsg::RefreshCurBank:
                    if (msg->data && guiMaster->bankui)
                    {
                        if (msg->index == 1)
                        {
                            // special case for first synth startup
                            guiMaster->bankui->readbankcfg();
                            guiMaster->bankui->rescan_for_banks(false);
                        }
                        guiMaster->bankui->set_bank_slot();
                        guiMaster->bankui->refreshmainwindow();
                    }
                    break;

                case GuiThreadMsg::GuiAlert:
                    if (msg->data)
                        guiMaster->ShowAlert(msg->index);
                    break;

                default:
                    break;
            }
        }
        delete msg;
    }
}
