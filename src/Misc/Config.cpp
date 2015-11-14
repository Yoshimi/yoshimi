/*
    Config.cpp - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2013, Nikita Zlobin
    Copyright 2014-2015, Will Godfrey & others

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

    This file is derivative of ZynAddSubFX original code, last modified January 2015
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

#include "Synth/BodyDisposal.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "MasterUI.h"

extern void mainRegisterAudioPort(SynthEngine *s, int portnum);
const unsigned short Config::MaxParamsHistory = 25;

static char prog_doc[] =
    "Yoshimi " YOSHIMI_VERSION ", a derivative of ZynAddSubFX - "
    "Copyright 2002-2009 Nasca Octavian Paul and others, "
    "Copyright 2009-2011 Alan Calvert, "
    "Copyright 20012-2013 Jeremy Jongepier and others, "
    "Copyright 20014-2015 Will Godfrey and others";

const char* argp_program_version = "Yoshimi " YOSHIMI_VERSION;

static struct argp_option cmd_options[] = {
    {"alsa-audio",        'A',  "<device>",   1,  "use alsa audio output" },
    {"alsa-midi",         'a',  "<device>",   1,  "use alsa midi input" },
    {"define-root",       'D',  "<path>",     0,  "define path to new bank root"},
    {"buffersize",        'b',  "<size>",     0,  "set internal buffer size" },
    {"no-gui",            'i',  NULL,         0,  "no gui"},
    {"jack-audio",        'J',  "<server>",   1,  "use jack audio output" },
    {"jack-midi",         'j',  "<device>",   1,  "use jack midi input" },
    {"autostart-jack",    'k',  NULL,         0,  "auto start jack server" },
    {"auto-connect",      'K',  NULL,         0,  "auto connect jack audio" },
    {"load",              'l',  "<file>",     0,  "load .xmz file" },
    {"load-instrument",   'L',  "<file>",     0,  "load .xiz file" },
    {"name-tag",          'N',  "<tag>",      0,  "add tag to clientname" },
    {"samplerate",        'R',  "<rate>",     0,  "set alsa audio sample rate" },
    {"oscilsize",         'o',  "<size>",     0,  "set AddSynth oscilator size" },
    {"state",             'S',  "<file>",     1,  "load saved state, defaults to '$HOME/.config/yoshimi/yoshimi.state'" },
    #if defined(JACK_SESSION)
        {"jack-session-uuid", 'U',  "<uuid>",     0,  "jack session uuid" },
        {"jack-session-file", 'u',  "<file>",     0,  "load named jack session file" },
    #endif
    { 0, }
};


Config::Config(SynthEngine *_synth, int argc, char **argv) :
    restoreState(false),
    restoreJackSession(false),
    Samplerate(48000),
    Buffersize(256),
    Oscilsize(512),
    runSynth(true),
    showGui(true),
    VirKeybLayout(1),
    audioEngine(DEFAULT_AUDIO),
    midiEngine(DEFAULT_MIDI),
    audioDevice("default"),
    midiDevice("default"),
    jackServer("default"),
    startJack(false),
    connectJackaudio(false),
    alsaAudioDevice("default"),
    alsaMidiDevice("default"),
    GzipCompression(3),
    Interpolation(0),
    checksynthengines(1),
    xmlType(0),
    EnableProgChange(1), // default will be inverted
    consoleMenuItem(0),
    logXMLheaders(0),
    rtprio(50),
    midi_bank_root(0), // 128 is used as 'disabled'
    midi_bank_C(32),
    midi_upper_voice_C(128),
    enable_part_on_voice_load(1),
    single_row_panel(1),
    NumAvailableParts(NUM_MIDI_CHANNELS),
    currentPart(0),
    currentChannel(0),
    currentMode(0),
    nrpnL(127),
    nrpnH(127),
    nrpnActive(false),

    deadObjects(NULL),
    nextHistoryIndex(numeric_limits<unsigned int>::max()),
    sigIntActive(0),
    ladi1IntActive(0),    
    sse_level(0),    
    programcommand(string("yoshimi")),
    synth(_synth),
    bRuntimeSetupCompleted(false)
{
    if(!synth->getIsLV2Plugin())
        fesetround(FE_TOWARDZERO); // Special thanks to Lars Luthman for conquering
                               // the heffalump. We need lrintf() to round
                               // toward zero.
    //^^^^^^^^^^^^^^^ This call is not needed aymore (at least for lv2 plugin)
    //as all calls to lrintf() are replaced with (int)truncf()
    //which befaves exactly the same when flag FE_TOWARDZERO is set

    cerr.precision(4);
    deadObjects = new BodyDisposal();    
    bRuntimeSetupCompleted = Setup(argc, argv);
}


bool Config::Setup(int argc, char **argv)
{
    clearPresetsDirlist();
    AntiDenormals(true);

    if (!loadConfig())
        return false;

    if(synth->getIsLV2Plugin()) //skip further setup for lv2 plugin instance.
        return true;
    switch (audioEngine)
    {
        case alsa_audio:
        {
            audioDevice = string(alsaAudioDevice);
            break;
        }
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
            midiDevice = string(jackServer);
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
    if (restoreState)
    {
        char * fp;
        if (! StateFile.size()) goto no_state0;
        else fp = new char [PATH_MAX];

        if (! realpath (StateFile.c_str(), fp)) goto no_state1;
        StateFile = fp;
        delete (fp);

        if (! isRegFile(StateFile))
        {
            no_state1: delete (fp);
            no_state0: Log("Invalid state file specified for restore " + StateFile);
            return false;
        }
        Log(StateFile);
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


string Config::addParamHistory(string file)
{
    if (!file.empty())
    {
        unsigned int name_start = file.rfind("/");
        unsigned int name_end = file.rfind(".xmz");
        if (name_start != string::npos && name_end != string::npos
            && (name_start - 1) < name_end)
        {
            HistoryListItem item;
            item.name = file.substr(name_start + 1, name_end - name_start - 1);
            item.file = file;
            item.index = nextHistoryIndex--;
            itx = ParamsHistory.begin();
            for (unsigned int i = 0; i < ParamsHistory.size(); ++i, ++itx)
                if (ParamsHistory.at(i).sameFile(file))
                    ParamsHistory.erase(itx);
            ParamsHistory.insert(ParamsHistory.begin(), item);
            if (ParamsHistory.size() > MaxParamsHistory)
            {
                itx = ParamsHistory.end();
                ParamsHistory.erase(--itx);
            }
            return (CurrentXMZ = item.name);
        }
        else
            Log("Invalid param file proffered to history:" + file);
    }
    return string();
}


string Config::historyFilename(int index)
{
    if (index > 0 && index <= (int)ParamsHistory.size())
    {
        itx = ParamsHistory.begin();
        for (int i = index; i > 0; ++itx, --i) ;
        return itx->file;
    }
    return string();
}


string Config::testCCvalue(int cc)
{
    string result = "";
    switch (cc)
    {
        case 1:
            result = "mod wheel";
            break;
        case 10:
            result = "panning";
            break;
        case 11:
            result = "expression";
            break;
        case 38:
            result = "data lsb";
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
            result = "resonance centre";
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
            }
        }
    }
    return result;
}


void Config::clearPresetsDirlist(void)
{
    for (int i = 0; i < MAX_PRESETS; ++i)
        presetsDirlist[i].clear();
}


bool Config::loadConfig(void)
{
    string cmd;
    int chk;
    string homedir = string(getenv("HOME"));
    if (homedir.empty() || !isDirectory(homedir))
        homedir = string("/tmp");
    ConfigDir = homedir + string("/.config/") + programcommand;
    if (!isDirectory(ConfigDir))
    {
        cmd = string("mkdir -p ") + ConfigDir;
        if ((chk = system(cmd.c_str())) < 0)
        {
            Log("Create config directory " + ConfigDir + " failed, status " + asString(chk));
            return false;
        }
    }
    string yoshimi = "/" + programcommand;
    if (synth->getUniqueId() > 0)
        yoshimi += ("-" + asString(synth->getUniqueId()));
    string presetDir = ConfigDir + "/presets";
    if (!isDirectory(presetDir))
    {
        cmd = string("mkdir -p ") + presetDir;
        if ((chk = system(cmd.c_str())) < 0)
            Log("Create preset directory " + presetDir + " failed, status " + asString(chk));
    }
    ConfigFile = ConfigDir + yoshimi + string(".config");
    StateFile = ConfigDir + yoshimi + string(".state");
    string resConfigFile = ConfigFile;

    bool isok = true;
    if (!isRegFile(resConfigFile) && !isRegFile(ConfigFile))
    {
        Log("ConfigFile " + resConfigFile + " not found, will use default settings");
        saveConfig();
    }
    else
    {
        XMLwrapper *xml = new XMLwrapper(synth);
        if (!xml)
            Log("loadConfig failed XMLwrapper allocation");
        else
        {
            if (!xml->loadXMLfile(resConfigFile))
            {                
                if((synth->getUniqueId() > 0) && (!xml->loadXMLfile(ConfigFile)))
                {
                    Log("loadConfig loadXMLfile failed");
                    return false;
                }
            }
            isok = extractConfigData(xml);
            if (isok)
                Oscilsize = (int)truncf(powf(2.0f, ceil(log (Oscilsize - 1.0f) / logf(2.0))));
            delete xml;
        }
    }
    return isok;
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
        return false;
    }
    Samplerate = xml->getpar("sample_rate", Samplerate, 44100, 96000);
    Buffersize = xml->getpar("sound_buffer_size", Buffersize, 16, 1024);
    Oscilsize = xml->getpar("oscil_size", Oscilsize,
                                        MAX_AD_HARMONICS * 2, 131072);
    GzipCompression = xml->getpar("gzip_compression", GzipCompression, 0, 9);
    Interpolation = xml->getpar("interpolation", Interpolation, 0, 1);
    checksynthengines = xml->getpar("check_pad_synth", checksynthengines, 0, 1);
    EnableProgChange = 1 - xml->getpar("ignore_program_change", EnableProgChange, 0, 1); // inverted for Zyn compatibility
    consoleMenuItem = xml->getpar("reports_destination", consoleMenuItem, 0, 1);
    logXMLheaders = xml->getpar("report_XMLheaders", logXMLheaders, 0, 1);
    VirKeybLayout = xml->getpar("virtual_keyboard_layout", VirKeybLayout, 0, 10);

    // get bank dirs
    synth->getBankRef().parseConfigFile(xml);

    // get preset dirs
    int count = 0;
    for (int i = 0; i < MAX_PRESETS; ++i)
    {
        if (xml->enterbranch("PRESETSROOT", i))
        {
            string dir = xml->getparstr("presets_root");
            if (isDirectory(dir))
                presetsDirlist[count++] = dir;
            xml->exitbranch();
        }
    }
    if (!count)
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
                presetsDirlist[count++] = presetdirs[i];
            ++ i;
        }
    }
    
    // alsa settings
    alsaAudioDevice = xml->getparstr("linux_alsa_audio_dev");
    alsaMidiDevice = xml->getparstr("linux_alsa_midi_dev");

    // jack settings
    jackServer = xml->getparstr("linux_jack_server");

    // midi options
    midi_bank_root = xml->getpar("midi_bank_root", midi_bank_root, 0, 128);
    midi_bank_C = xml->getpar("midi_bank_C", midi_bank_C, 0, 128);
    midi_upper_voice_C = xml->getpar("midi_upper_voice_C", midi_upper_voice_C, 0, 128);
    enable_part_on_voice_load = xml->getpar("enable_part_on_voice_load", enable_part_on_voice_load, 0, 1);
//    consoleMenuItem = xml->getpar("enable_console_window", consoleMenuItem, 0, 1);
    single_row_panel = xml->getpar("single_row_panel", single_row_panel, 0, 1);

    if (xml->enterbranch("XMZ_HISTORY"))
    {
        int hist_size = xml->getpar("history_size", 0, 0, MaxParamsHistory);
        string xmz;
        for (int i = 0; i < hist_size; ++i)
        {
            if (xml->enterbranch("XMZ_FILE", i))
            {
                xmz = xml->getparstr("xmz_file");
                if (xmz.size() && isRegFile(xmz))
                    addParamHistory(xmz);
                xml->exitbranch();
            }
        }
        xml->exitbranch();
    }

    xml->exitbranch(); // CONFIGURATION
    return true;
}


void Config::saveConfig(void)
{
    xmlType = XML_CONFIG;
    XMLwrapper *xmltree = new XMLwrapper(synth);
    if (!xmltree)
    {
        Log("saveConfig failed xmltree allocation");
        return;
    }
    addConfigXML(xmltree);
    unsigned int tmp = GzipCompression;
    GzipCompression = 0;

    string resConfigFile = ConfigFile;

    if (!xmltree->saveXMLfile(resConfigFile))
    {
        Log("Failed to save config to " + resConfigFile);
    }
    GzipCompression = tmp;

    delete xmltree;
}


void Config::addConfigXML(XMLwrapper *xmltree)
{
    xmltree->beginbranch("CONFIGURATION");

    xmltree->addpar("sample_rate", Samplerate);
    xmltree->addpar("sound_buffer_size", Buffersize);
    xmltree->addpar("oscil_size", Oscilsize);

    xmltree->addpar("gzip_compression", GzipCompression);
    xmltree->addpar("check_pad_synth", checksynthengines);
    xmltree->addpar("ignore_program_change", (1 - EnableProgChange));
    xmltree->addpar("reports_destination", consoleMenuItem);
    xmltree->addpar("report_XMLheaders", logXMLheaders);
    xmltree->addpar("virtual_keyboard_layout", VirKeybLayout);

    synth->getBankRef().saveToConfigFile(xmltree);

    for (int i = 0; i < MAX_PRESETS; ++i)
        if (presetsDirlist[i].size())
        {
            xmltree->beginbranch("PRESETSROOT",i);
            xmltree->addparstr("presets_root", presetsDirlist[i]);
            xmltree->endbranch();
        }

    xmltree->addpar("interpolation", Interpolation);

    xmltree->addparstr("linux_alsa_audio_dev", alsaAudioDevice);
    xmltree->addparstr("linux_alsa_midi_dev", alsaMidiDevice);
    xmltree->addparstr("linux_jack_server", jackServer);

    xmltree->addpar("midi_bank_root", midi_bank_root);
    xmltree->addpar("midi_bank_C", midi_bank_C);
    xmltree->addpar("midi_upper_voice_C", midi_upper_voice_C);
    xmltree->addpar("enable_part_on_voice_load", enable_part_on_voice_load);
//    xmltree->addpar("enable_console_window", consoleMenuItem);
    xmltree->addpar("single_row_panel", single_row_panel);

    // Parameters history
    if (ParamsHistory.size())
    {
        xmltree->beginbranch("XMZ_HISTORY");
        xmltree->addpar("history_size", ParamsHistory.size());
        deque<HistoryListItem>::reverse_iterator rx = ParamsHistory.rbegin();
        unsigned int count = 0;
        for (int x = 0; rx != ParamsHistory.rend() && count <= MaxParamsHistory; ++rx, ++x)
        {
            xmltree->beginbranch("XMZ_FILE", x);
            xmltree->addparstr("xmz_file", rx->file);
            xmltree->endbranch();
        }
        xmltree->endbranch();
    }
    xmltree->endbranch(); // CONFIGURATION
}


void Config::saveSessionData(string savefile)
{
    string ext = ".state";
    if (savefile.rfind(ext) != (savefile.length() - 6))
        savefile += ext;
    synth->getRuntime().xmlType = XML_STATE;
    XMLwrapper *xmltree = new XMLwrapper(synth);
    if (!xmltree)
    {
        Log("saveSessionData failed xmltree allocation", true);
        return;
    }
    addConfigXML(xmltree);
    addRuntimeXML(xmltree);
    synth->add2XML(xmltree);
    if (xmltree->saveXMLfile(savefile))
        Log("Session data saved to " + savefile);
    else
        Log("Failed to save session data to " + savefile, true);
}


bool Config::restoreSessionData(string sessionfile)
{
    XMLwrapper *xml = NULL;
    bool ok = false;
    if (sessionfile.size() && !isRegFile(sessionfile))
        sessionfile += ".state";
    if (!sessionfile.size() || !isRegFile(sessionfile))
    {
        Log("Session file " + sessionfile + " not available", true);
        goto end_game;
    }
    if (!(xml = new XMLwrapper(synth)))
    {
        Log("Failed to init xmltree for restoreState", true);
        goto end_game;
    }

    if (xml->loadXMLfile(sessionfile) < 0)
    {
        Log("Failed to load xml file " + sessionfile);
        goto end_game;
    }
    ok = extractConfigData(xml) && extractRuntimeData(xml) && synth->getfromXML(xml);

end_game:
    if (xml)
        delete xml;
    return ok;
}


bool Config::extractRuntimeData(XMLwrapper *xml)
{
    if (!xml->enterbranch("RUNTIME"))
    {
        Log("Config extractRuntimeData, no RUNTIME branch", true);
        return false;
    }
    audioEngine = (audio_drivers)xml->getpar("audio_engine", DEFAULT_AUDIO, no_audio, alsa_audio);
    audioDevice = xml->getparstr("audio_device");
    midiEngine = (midi_drivers)xml->getpar("midi_engine", DEFAULT_MIDI, no_midi, alsa_midi);
    midiDevice = xml->getparstr("midi_device");
    nameTag = xml->getparstr("name_tag");
    CurrentXMZ = xml->getparstr("current_xmz");
    xml->exitbranch();
    return true;
}


void Config::addRuntimeXML(XMLwrapper *xml)
{
    xml->beginbranch("RUNTIME");
    xml->addpar("audio_engine", audioEngine);
    xml->addparstr("audio_device", audioDevice);
    xml->addpar("midi_engine", midiEngine);
    xml->addparstr("midi_device", midiDevice);
    xml->addparstr("name_tag", nameTag);
    xml->addparstr("current_xmz", CurrentXMZ);
    xml->endbranch();
}


void Config::Log(string msg, bool tostderr)
{
    if (showGui && !tostderr && consoleMenuItem)
        LogList.push_back(msg);
    else
        cerr << msg << endl;
}


void Config::StartupReport(MusicClient *musicClient)
{
    Log(string(argp_program_version));
    Log("Clientname: " + musicClient->midiClientName());
    string report = "Config: Audio: ";
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
    Log(report);
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
    Log(report);
    Log("Oscilsize: " + asString(synth->oscilsize));
    Log("Samplerate: " + asString(synth->samplerate));
    Log("Period size: " + asString(synth->buffersize));
}


void Config::setRtprio(int prio)
{
    if (prio < rtprio)
        rtprio = prio;
}


// general thread start service
bool Config::startThread(pthread_t *pth, void *(*thread_fn)(void*), void *arg,
                         bool schedfifo, char priodec, bool create_detached)
{
    pthread_attr_t attr;
    int chk;
    bool outcome = false;
    bool retry = true;
    while (retry)
    {
        if (!(chk = pthread_attr_init(&attr)))
        {
            if(create_detached)
            {
               chk = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            }
            if(!chk)
            {
                if (schedfifo)
                {
                    if ((chk = pthread_attr_setschedpolicy(&attr, SCHED_FIFO)))
                    {
                        Log("Failed to set SCHED_FIFO policy in thread attribute "
                                    + string(strerror(errno))
                                    + " (" + asString(chk) + ")", true);
                        schedfifo = false;
                        continue;
                    }
                    if ((chk = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)))
                    {
                        Log("Failed to set inherit scheduler thread attribute "
                                    + string(strerror(errno)) + " ("
                                    + asString(chk) + ")", true);
                        schedfifo = false;
                        continue;
                    }
                    sched_param prio_params;
                    int prio = rtprio;
                    if (priodec)
                        prio -= priodec;
                    prio_params.sched_priority = (prio > 0) ? prio : 0;
                    if ((chk = pthread_attr_setschedparam(&attr, &prio_params)))
                    {
                        Log("Failed to set thread priority attribute ("
                                    + asString(chk) + ")  "
                                    + string(strerror(errno)), true);
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
                Log("Failed to set thread detach state " + asString(chk), true);
            pthread_attr_destroy(&attr);
        }
        else
            Log("Failed to initialise thread attributes " + asString(chk), true);

        if (schedfifo)
        {
            Log("Failed to start thread (sched_fifo) " + asString(chk)
                + "  " + string(strerror(errno)), true);
            schedfifo = false;
            continue;
        }
        Log("Failed to start thread (sched_other) " + asString(chk)
            + "  " + string(strerror(errno)), true);
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
    Log("Interrupt received", true);
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
    if(synth->getIsLV2Plugin())
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
            settings->audioEngine = alsa_audio;
            if (arg)
                settings->audioDevice = string(arg);
            else
                settings->audioDevice = settings->alsaAudioDevice;
            break;
        case 'a':
            settings->midiEngine = alsa_midi;
            if (arg)
                settings->midiDevice = string(arg);
            break;
        case 'b': // messy but I can't think of a better way :(
            num = Config::string2int(string(arg));
            if (num >= 1024)
                num = 1024;
            else if (num >= 512)
                num = 512;
            else if (num >= 256)
                num = 256;
            else if (num >= 128)
                num = 128;
            else if (num >= 64)
                num = 64;
            else if (num >= 32)
                num = 32;
            else
                num = 16;
            settings->Buffersize = num;
            break;
        case 'D':
            if (arg)
                settings->rootDefine = string(arg);
            break;
        case 'i':
            settings->showGui = false;
            break;
        case 'J':
            settings->audioEngine = jack_audio;
            if (arg)
                settings->audioDevice = string(arg);
            break;
        case 'j':
            settings->midiEngine = jack_midi;
            if (arg)
                settings->midiDevice = string(arg);
            break;
        case 'k': settings->startJack = true; break;
        case 'K': settings->connectJackaudio = true; break;
        case 'o':
            num = Config::string2int(string(arg));
            if (num >= 16384)
                num = 16384;
            else if (num >= 8192)
                num = 8192;
            else if (num >= 4096)
                num = 4096;
            else if (num >= 2048)
                num = 2048;
            else if (num >= 1024)
                num = 1024;
            else if (num >= 512)
                num = 512;
            else if (num >= 256)
                num = 256;
            else num = 128;
            settings->Oscilsize = num;
            break;
        case 'R':
            num = Config::string2int(string(arg));
            if (num >= 96000)
                num = 96000;
            else if (num >= 48000)
                num = 48000;
            else
                num = 44100;
            settings->Samplerate = num;
            break;
        case 'S':
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
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


static struct argp cmd_argp = { cmd_options, parse_cmds, prog_doc };


void Config::loadCmdArgs(int argc, char **argv)
{
    argp_parse(&cmd_argp, argc, argv, 0, 0, this);
    if (jackSessionUuid.size() && jackSessionFile.size())
        restoreJackSession = true;
}

void GuiThreadMsg::processGuiMessages()
{
    GuiThreadMsg *msg = (GuiThreadMsg *)Fl::thread_message();
    if(msg)
    {
        switch(msg->type)
        {
        case GuiThreadMsg::NewSynthEngine:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster();
            if(!guiMaster)
            {
                cerr << "Error starting Main UI!" << endl;
            }
            else
            {
                guiMaster->Init(guiMaster->getSynth()->getWindowTitle().c_str());
            }
        }
            break;
        case GuiThreadMsg::UpdateMaster:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster(false);
            if(guiMaster)
            {
                guiMaster->refresh_master_ui();
            }
        }
            break;
        case GuiThreadMsg::UpdateConfig:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster(false);
            if(guiMaster)
            {
                guiMaster->configui->update_config(msg->index);
            }
        }
            break;
        case GuiThreadMsg::UpdatePaths:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster(false);
            if(guiMaster)
            {
                guiMaster->updatepaths(msg->index);
            }
        }
            break;
        case GuiThreadMsg::UpdatePanel:
        {
            SynthEngine *synth = ((SynthEngine *)msg->data);
            MasterUI *guiMaster = synth->getGuiMaster(false);
            if(guiMaster)
            {
                guiMaster->updatepanel();
            }
        }
            break;
        case GuiThreadMsg::UpdatePanelItem:
            if(msg->index < NUM_MIDI_PARTS && msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                MasterUI *guiMaster = synth->getGuiMaster(false);
                if(guiMaster)
                {
                    guiMaster->panellistitem[(msg->index) % NUM_MIDI_CHANNELS]->refresh();
                    guiMaster->updatepart();
                }
            }
            break;
        case GuiThreadMsg::UpdatePartProgram:
            if(msg->index < NUM_MIDI_PARTS && msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                MasterUI *guiMaster = synth->getGuiMaster(false);
                if(guiMaster)
                {
                    guiMaster->updatepartprogram(msg->index);
                }
            }
            break;
        case GuiThreadMsg::UpdateEffects:
            if(msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                MasterUI *guiMaster = synth->getGuiMaster(false);
                if(guiMaster)
                {
                    guiMaster->updateeffects(msg->index);
                }
            }
            break;
        case GuiThreadMsg::RegisterAudioPort:
            if(msg->data)
            {
                SynthEngine *synth = ((SynthEngine *)msg->data);
                mainRegisterAudioPort(synth, msg->index);
            }
            break;
        default:
            break;
        }
        delete msg;
    }
}
