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


const unsigned short Config::MaxParamsHistory = 25;

static char prog_doc[] =
    "Yoshimi " YOSHIMI_VERSION ", a derivative of ZynAddSubFX - "
    "Copyright 2002-2009 Nasca Octavian Paul and others, "
    "Copyright 2009-2011 Alan Calvert";

const char* argp_program_version = "Yoshimi " YOSHIMI_VERSION;

static struct argp_option cmd_options[] = {
    {"alsa-audio",        'A',  "<device>", 0x1,  "use alsa audio output" },
    {"alsa-midi",         'a',  "<device>", 0x1,  "use alsa midi input" },
    {"buffersize",        'b',  "<size>",     0,  "set alsa audio buffer size" },
    {"show-console",      'c',  NULL,         0,  "show console on startup" },
    {"no-gui",            'i',  NULL,         0,  "no gui"},
    {"jack-audio",        'J',  "<server>", 0x1,  "use jack audio output" },
    {"jack-midi",         'j',  "<device>", 0x1,  "use jack midi input" },
    {"autostart-jack",    'k',  NULL,         0,  "auto start jack server" },
    {"auto-connect",      'K',  NULL,         0,  "auto connect jack audio" },
    {"load",              'l',  "<file>",     0,  "load .xmz file" },
    {"load-instrument",   'L',  "<file>",     0,  "load .xiz file" },
    {"name-tag",          'N',  "<tag>",      0,  "add tag to clientname" },
    {"samplerate",        'R',  "<rate>",     0,  "set alsa audio sample rate" },
    {"oscilsize",         'o',  "<size>",     0,  "set oscilsize" },
    {"state",             'S',  "<file>",   0x1,  "load state from <file>, defaults to '$HOME/.config/yoshimi/yoshimi.state'" },
    #if defined(JACK_SESSION)
        {"jack-session-uuid", 'U',  "<uuid>",     0,  "jack session uuid" },
        {"jack-session-file", 'u',  "<file>",     0,  "load jack session file" },
    #endif
    { 0, }
};


Config::Config(SynthEngine *_synth, int argc, char **argv) :
    restoreState(false),
    restoreJackSession(false),
    Samplerate(48000),
    Buffersize(256),
    Oscilsize(1024),
    runSynth(true),
    showGui(true),
    showConsole(false),
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
    BankUIAutoClose(0),
    RootUIAutoClose(0),
    GzipCompression(3),
    Interpolation(0),
    CheckPADsynth(1),
    EnableProgChange(0), // default will be inverted
    rtprio(50),
    midi_bank_root(128),
    midi_bank_C(0),
    midi_upper_voice_C(128),
    enable_part_on_voice_load(0),
    single_row_panel(0),
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
            no_state0: Log("Invalid state file specified for restore: " + StateFile);
            return false;
        }
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
        case 7:
            result = "volume";
            break;
        case 10:
            result = "panning";
            break;
        case 11:
            result = "expression";
            break;
        case 64:
            result = "sustain pedal";
            break;
        case 65:
            result = "partamento";
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
                if (cc == synth->getRuntime().midi_bank_C)
                    result = "bank change";
                else if (cc == synth->getRuntime().midi_bank_root)
                    result = "bank root change";
                else if (cc == synth->getRuntime().midi_upper_voice_C)
                    result = "extended program change";
            }
        }
    }
    return result;
}


void Config::clearPresetsDirlist(void)
{
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        presetsDirlist[i].clear();
}


bool Config::loadConfig(void)
{
    string cmd;
    int chk;
    string homedir = string(getenv("HOME"));
    if (homedir.empty() || !isDirectory(homedir))
        homedir = string("/tmp");
    ConfigDir = homedir + string("/.config/yoshimi");
    if (!isDirectory(ConfigDir))
    {
        cmd = string("mkdir -p ") + ConfigDir;
        if ((chk = system(cmd.c_str())) < 0)
        {
            Log("Create config directory " + ConfigDir + " failed, status " + asString(chk));
            return false;
        }
    }
    ConfigFile = ConfigDir + string("/yoshimi.config");
    StateFile = ConfigDir + string("/yoshimi.state");
    string resConfigFile = ConfigFile;
    if(synth->getUniqueId() > 0)
    {
        resConfigFile += asString(synth->getUniqueId());
    }
    if (!isRegFile(resConfigFile) && !isRegFile(ConfigFile))
    {
        Log("ConfigFile " + resConfigFile + " not found");
        string oldConfigFile = string(getenv("HOME")) + string("/.yoshimiXML.cfg");
        if (isRegFile(oldConfigFile))
        {
            Log("Copying old config file " + oldConfigFile + " to new location: " + resConfigFile);
            FILE *oldfle = fopen (oldConfigFile.c_str(), "r");
            FILE *newfle = fopen (resConfigFile.c_str(), "w");
            if (oldfle != NULL && newfle != NULL)
                while (!feof(oldfle))
                    putc(getc(oldfle), newfle);
            else
                Log("Failed to copy old config file " + oldConfigFile + " to " + resConfigFile);
            if (newfle)
                fclose(newfle);
            if (oldfle)
                fclose(oldfle);
        }
    }

    bool isok = true;
    if (!isRegFile(resConfigFile) && !isRegFile(ConfigFile))
        Log("ConfigFile " + resConfigFile + " still not found, will use default settings");
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
    Buffersize = xml->getpar("sound_buffer_size", Buffersize, 64, 4096);
    Oscilsize = xml->getpar("oscil_size", Oscilsize,
                                        MAX_AD_HARMONICS * 2, 131072);
    BankUIAutoClose = xml->getpar("bank_window_auto_close",
                                               BankUIAutoClose, 0, 1);
    RootUIAutoClose = xml->getpar("root_window_auto_close",
                                               RootUIAutoClose, 0, 1);
    GzipCompression = xml->getpar("gzip_compression", GzipCompression, 0, 9);
    Interpolation = xml->getpar("interpolation", Interpolation, 0, 1);
    CheckPADsynth = xml->getpar("check_pad_synth", CheckPADsynth, 0, 1);
    EnableProgChange = 1 - xml->getpar("ignore_program_change", EnableProgChange, 0, 1); // inverted for Zyn compatibility
    VirKeybLayout = xml->getpar("virtual_keyboard_layout", VirKeybLayout, 0, 10);

    // get bank dirs
    synth->getBankRef().parseConfigFile(xml);

    // get preset dirs
    int count = 0;
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
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
            string(getenv("HOME")) + "/presets",
            "../presets",
            "presets"
        };
        const int defaultsCount = 7; // as per presetdirs[] above
        for (int i = 0; i < defaultsCount; ++i)
            if (presetdirs[i].size() && isDirectory(presetdirs[i]))
                presetsDirlist[count++] = presetdirs[i];
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
    if(synth->getUniqueId() > 0)
    {
        resConfigFile += asString(synth->getUniqueId());
    }
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

    xmltree->addparstr("state_file", StateFile);
    xmltree->addpar("sample_rate", Samplerate);
    xmltree->addpar("sound_buffer_size", Buffersize);
    xmltree->addpar("oscil_size", Oscilsize);
    xmltree->addpar("bank_window_auto_close", BankUIAutoClose);
    xmltree->addpar("root_window_auto_close", RootUIAutoClose);

    xmltree->addpar("gzip_compression", GzipCompression);
    xmltree->addpar("check_pad_synth", CheckPADsynth);
    xmltree->addpar("ignore_program_change", (1 - EnableProgChange));    
    xmltree->addpar("virtual_keyboard_layout", VirKeybLayout);

    synth->getBankRef().saveToConfigFile(xmltree);

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
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
    if (showGui && !tostderr)
        LogList.push_back(msg);
    else
        cerr << msg << endl;
}


void Config::StartupReport(MusicClient *musicClient)
{
    if (!showGui)
        return;

    Log(string(argp_program_version));
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
                        Log("Failed to set SCHED_FIFO policy in thread attribute: "
                                    + string(strerror(errno))
                                    + " (" + asString(chk) + ")", true);
                        schedfifo = false;
                        continue;
                    }
                    if ((chk = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)))
                    {
                        Log("Failed to set inherit scheduler thread attribute: "
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
                        Log("Failed to set thread priority attribute: ("
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
                Log("Failed to set thread detach state: " + asString(chk), true);
            pthread_attr_destroy(&attr);
        }
        else
            Log("Failed to initialise thread attributes: " + asString(chk), true);

        if (schedfifo)
        {
            Log("Failed to start thread (sched_fifo): " + asString(chk)
                + "  " + string(strerror(errno)), true);
            schedfifo = false;
            continue;
        }
        Log("Failed to start thread (sched_other): " + asString(chk)
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
    switch (key)
    {
        case 'c': settings->showConsole = true; break;
        case 'N': settings->nameTag = string(arg); break;
        case 'l': settings->paramsLoad = string(arg); break;
        case 'L': settings->instrumentLoad = string(arg); break;
        case 'R': settings->Samplerate = Config::string2int(string(arg)); break;
        case 'b': settings->Buffersize = Config::string2int(string(arg)); break;
        case 'o': settings->Oscilsize = Config::string2int(string(arg)); break;
        case 'A':
            settings->audioEngine = alsa_audio;
            if (arg)
                settings->audioDevice = string(arg);
            break;
        case 'a':
            settings->midiEngine = alsa_midi;
            if (arg)
                settings->midiDevice = string(arg);
            break;
        case 'i':
            settings->showGui = false;
            settings->showConsole = false;
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
        default: return ARGP_ERR_UNKNOWN;
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
