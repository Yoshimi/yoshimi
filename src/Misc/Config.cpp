/*
    Config.cpp - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is somewhat derivative of ZynAddSubFX original code, modified 2010
*/

#include <iostream>

#include <cmath>
#include <string>
#include <argp.h>
#include <libgen.h>

#if defined(__SSE__)
#include <xmmintrin.h>
#endif

using namespace std;

#include "Sql/ProgramBanks.h"
#include "Misc/BodyDisposal.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"

Config Runtime;

int Config::sigIntActive = 0;
int Config::ladi1IntActive = 0;
struct sigaction Config::sigAction;

const unsigned short Config::MaxParamsHistory = 25;
unsigned short Config::nextHistoryIndex = numeric_limits<unsigned int>::max();

static char prog_doc[] =
    "Yoshimi " YOSHIMI_VERSION ", a derivative of ZynAddSubFX - "
    "Copyright 2002-2009 Nasca Octavian Paul and others, "
    "Copyright 2009-2010 Alan Calvert";

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
    {"jack-session-file", 'u',  "<file>",     0,  "load jack session file" },
    {"jack-session-file", 'U',  "<uuid>",     0,  "jack session uuid" },
    { 0, }
};



Config::Config() :
    BankSelectMethod(0),
    doRestoreState(false),
    doRestoreJackSession(false),
    Samplerate(48000),
    Buffersize(128),
    Oscilsize(1024),
    runSynth(true),
    showGui(true),
    showConsole(false),
    VirKeybLayout(1),
    audioDevice("default"),
    midiDevice("default"),
    audioEngine(DEFAULT_AUDIO),
    midiEngine(DEFAULT_MIDI),
    jackServer("default"),
    startJack(false),
    connectJackaudio(false),
    alsaAudioDevice("default"),
    alsaSamplerate(48000),
    alsaBuffersize(256),
    alsaMidiDevice("default"),
    Float32bitWavs(false),
    DefaultRecordDirectory("/tmp"),
    BankUIAutoClose(0),
    Interpolation(0),
    CheckPADsynth(1),
    rtprio(50),
    deadObjects(new BodyDisposal()),
    progBanks(NULL),
    sse_level(0),
    programCmd("yoshimi")
{
    setlocale( LC_TIME, "" ); // use compiler's native locale
    std::ios::sync_with_stdio(false);
    cerr.precision(4);
    std::ios::sync_with_stdio(false);
}


bool Config::Setup(int argc, char **argv)
{
    memset(&sigAction, 0, sizeof(sigAction));
    sigAction.sa_handler = sigHandler;
    if (sigaction(SIGUSR1, &sigAction, NULL))
        Log("Setting SIGUSR1 handler failed");
    if (sigaction(SIGINT, &sigAction, NULL))
        Log("Setting SIGINT handler failed");
    if (sigaction(SIGHUP, &sigAction, NULL))
        Log("Setting SIGHUP handler failed");
    if (sigaction(SIGTERM, &sigAction, NULL))
        Log("Setting SIGTERM handler failed");
    if (sigaction(SIGQUIT, &sigAction, NULL))
        Log("Setting SIGQUIT handler failed");

    string homedir = string(getenv("HOME"));
    if (homedir.empty() || !isDirectory(homedir))
        homedir = string("/tmp");
    ConfigDirectory = homedir + string("/.config/yoshimi");
    if (!isDirectory(ConfigDirectory))
    {
        string cmd = string("mkdir -p ") + ConfigDirectory;
        int chk  = system(cmd.c_str());
        if (chk < 0)
        {
            Log("Create config directory " + ConfigDirectory + " failed, status " + asString(chk), true);
            return false;
        }
    }

    ConfigFile = ConfigDirectory + string("/yoshimi.config");
    StateFile = ConfigDirectory + string("/yoshimi.state");
    if (!isRegFile(ConfigFile))
    {
        Log("ConfigFile " + ConfigFile + " not found", true);
        string oldConfigFile = string(getenv("HOME")) + string("/.yoshimiXML.cfg");
        if (isRegFile(oldConfigFile))
        {
            Log("Copying old config file " + oldConfigFile + " to new location: " + ConfigFile);
            FILE *oldfle = fopen (oldConfigFile.c_str(), "r");
            FILE *newfle = fopen (ConfigFile.c_str(), "w");
            if (oldfle != NULL && newfle != NULL)
                while (!feof(oldfle))
                    putc(getc(oldfle), newfle);
            else
                Log("Failed to copy old config file " + oldConfigFile + " to " + ConfigFile, true);
            if (newfle)
                fclose(newfle);
            if (oldfle)
                fclose(oldfle);
        }
    }
    if (!isRegFile(ConfigFile))
        Log("ConfigFile " + ConfigFile + " still not found, so using default settings");
    else
    {
        boost::shared_ptr<XMLwrapper> xmltree = boost::shared_ptr<XMLwrapper>(new XMLwrapper());
        if (!xmltree)
            Log("loadConfig failed XMLwrapper allocation", true);
        else
        {
            if (xmltree->loadXMLfile(ConfigFile) < 0)
            {
                Log("loadConfig loadXMLfile failed", true);
                return false;
            }
            if (!extractConfigData(xmltree.get()))
                return false;
            Oscilsize = lrintf(powf(2.0f, ceil(log (Oscilsize - 1.0f) / logf(2.0))));
            if (DefaultRecordDirectory.empty())
                DefaultRecordDirectory = string("/tmp/");
            if (DefaultRecordDirectory.at(DefaultRecordDirectory.size() - 1) != '/')
                DefaultRecordDirectory += "/";
            if (CurrentRecordDirectory.empty())
                CurrentRecordDirectory = DefaultRecordDirectory;
        }
    }

    DataDirectory = homedir + string("/.local/share/yoshimi");
    if (!isDirectory(DataDirectory))
    {
        int chk;
        string cmd = string("mkdir -p ") + DataDirectory;
        if ((chk = system(cmd.c_str())) < 0)
        {
            Log("Failed to create data directory " + DataDirectory + ", status "
                + asHexString(chk), true);
            return false;
        }
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
        midiDevice = "default";

    loadCmdArgs(argc, argv);
    if (doRestoreState && !(StateFile.size() && isRegFile(StateFile)))
    {
        Log("Invalid state file specified for restore: " + StateFile);
        return false;
    }
    if (jackSessionUuid.size() && jackSessionFile.size() && isRegFile(jackSessionFile))
    {
        Log(string("Restore jack session requested, uuid ") + jackSessionUuid
            + string(", session file ") + jackSessionFile);
        doRestoreJackSession = true;
    }
    AntiDenormals(true);
    return true;
}


Config::~Config()
{
    if (progBanks)
        delete progBanks;
    AntiDenormals(false);
}


void Config::flushLog(void)
{
    if (LogList.size())
    {
        cerr << "Flushing log:" << endl;
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
    Interpolation = xml->getpar("interpolation", Interpolation, 0, 1);
    CheckPADsynth = xml->getpar("check_pad_synth", CheckPADsynth, 0, 1);
    VirKeybLayout = xml->getpar("virtual_keyboard_layout", VirKeybLayout, 0, 10);

    // alsa settings
    alsaAudioDevice = xml->getparstr("linux_alsa_audio_dev");
    alsaMidiDevice = xml->getparstr("linux_alsa_midi_dev");

    // jack settings
    jackServer = xml->getparstr("linux_jack_server");

    // recorder settings
    DefaultRecordDirectory = xml->getparstr("DefaultRecordDirectory");
    Float32bitWavs = xml->getparbool("Float32bitWavs", false);
    string datadir = xml->getparstr("DataDirectory");
    if (!datadir.empty())
        DataDirectory = datadir;
    BankSelectMethod = xml->getpar("BankSelectMethod", BankSelectMethod, 0, 2);

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
    boost::shared_ptr<XMLwrapper> xmltree = boost::shared_ptr<XMLwrapper>(new XMLwrapper());
    if (!xmltree)
    {
        Log("saveConfig failed xmltree allocation");
        return;
    }
    addConfigXML(xmltree.get());
    if (xmltree->saveXMLfile(ConfigFile))
        Log("Config saved to " + ConfigFile);
     else
        Log("Failed to save config to " + ConfigFile, true);
}


void Config::addConfigXML(XMLwrapper *xmltree)
{
    xmltree->beginbranch("CONFIGURATION");

    xmltree->addparstr("state_file", StateFile);
    xmltree->addpar("sample_rate", Samplerate);
    xmltree->addpar("sound_buffer_size", Buffersize);
    xmltree->addpar("oscil_size", Oscilsize);
    xmltree->addpar("bank_window_auto_close", BankUIAutoClose);

    xmltree->addpar("check_pad_synth", CheckPADsynth);
    xmltree->addpar("virtual_keyboard_layout", VirKeybLayout);
    xmltree->addpar("interpolation", Interpolation);

    xmltree->addparstr("linux_alsa_audio_dev", alsaAudioDevice);
    xmltree->addparstr("linux_alsa_midi_dev", alsaMidiDevice);
    xmltree->addparstr("linux_jack_server", jackServer);
    xmltree->addparstr("DefaultRecordDirectory", DefaultRecordDirectory);
    xmltree->addpar("Float32bitWavs", Float32bitWavs);
    xmltree->addparstr("DataDirectory", DataDirectory);
    xmltree->addpar("BankSelectMethod", BankSelectMethod);

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


void Config::saveState(void)
{
    saveSessionData(StateFile);
}


void Config::saveSessionData(string savefile)
{
    boost::shared_ptr<XMLwrapper> xmltree = boost::shared_ptr<XMLwrapper>(new XMLwrapper());
    if (!xmltree)
    {
        Log("saveState failed xmltree allocation", true);
        return;
    }
    addConfigXML(xmltree.get());
    addRuntimeXML(xmltree.get());
    synth->add2XML(xmltree.get());
    if (xmltree->saveXMLfile(savefile))
        Log("Session state saved to " + savefile);
    else
        Log("Session state save to " + savefile + " failed");
}


bool Config::restoreState(SynthEngine *synth)
{
    return restoreSessionData(synth, StateFile);
}


bool Config::restoreSessionData(SynthEngine *synth, string sessionfile)
{
    if (!sessionfile.size() || !isRegFile(sessionfile))
    {
        Log("Session file " + sessionfile + " not available", true);
        return false;
    }

    boost::shared_ptr<XMLwrapper> xmltree = boost::shared_ptr<XMLwrapper>(new XMLwrapper());
    if (!xmltree)
    {
        Log("Failed to init xmltree for restoreState");
        return false;
    }
    if (xmltree->loadXMLfile(sessionfile) < 0)
    {
        Log("Failed to load xml file " + sessionfile);
        return false;
    }
    return extractConfigData(xmltree.get())
           && extractRuntimeData(xmltree.get())
           && synth->getfromXML(xmltree.get());
}


bool Config::extractRuntimeData(XMLwrapper *xml)
{
    if (!xml->enterbranch("RUNTIME"))
    {
        Log("Config extractRuntimeData, no RUNTIME branch");
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


void Config::StartupReport(void)
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
    report += (" -> '" + midiDevice + "'");
    Log(report);
    Log("Oscilsize: " + asString(synth->oscilsize));
    Log("Samplerate: " + asString(synth->samplerate));
    Log("Buffersize: " + asString(synth->buffersize));
    float period_milisecs =  1000.0f * synth->buffersize_f / synth->samplerate_f;
    Log("Alleged minimum latency: "
        + asString(synth->buffersize) + " frames, " + asString(period_milisecs) + " ms");
}


void Config::setRtprio(int prio)
{
    if (prio < rtprio)
        rtprio = prio;
}


// general thread start service
bool Config::startThread(pthread_t *pth, void *(*thread_fn)(void*), void *arg,
                         bool schedfifo, bool midi)
{
    pthread_attr_t attr;
    int chk;
    bool outcome = false;
    bool retry = true;
    while (retry)
    {
        if (!(chk = pthread_attr_init(&attr)))
        {
            if (!(chk = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)))
            {
                if (schedfifo)
                {
                    if ((chk = pthread_attr_setschedpolicy(&attr, SCHED_FIFO)))
                    {
                        Log("Failed to set SCHED_FIFO policy in thread attribute: "
                                    + string(strerror(errno))
                                    + " (" + asString(chk) + ")");
                        schedfifo = false;
                        continue;
                    }
                    if ((chk = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)))
                    {
                        Log("Failed to set inherit scheduler thread attribute: "
                                    + string(strerror(errno)) + " ("
                                    + asString(chk) + ")");
                        schedfifo = false;
                        continue;
                    }
                    sched_param prio_params;
                    int prio = rtprio;
                    if (midi)
                        --prio;
                    prio_params.sched_priority = (prio > 0) ? prio : 0;
                    if ((chk = pthread_attr_setschedparam(&attr, &prio_params)))
                    {
                        Log("Failed to set thread priority attribute: ("
                                    + asString(chk) + ")  "
                                    + string(strerror(errno)));
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
                Log("Failed to set thread detach state: " + asString(chk));
        }
        else
            Log("Failed to initialise thread attributes: " + asString(chk));

        if (schedfifo)
        {
            Log("Failed to start thread (sched_fifo): " + asString(chk));
            schedfifo = false;
            continue;
        }
        Log("Failed to start thread (sched_other): " + asString(chk));
        outcome = false;
        break;
    }
    return outcome;
}


void Config::sigHandler(int sig)
{
    switch (sig)
    {
        case SIGINT:
        case SIGHUP:
        case SIGTERM:
        case SIGQUIT:
            Runtime.setInterruptActive();
            break;

        case SIGUSR1:
            Runtime.setLadi1Active();
            sigaction(SIGUSR1, &sigAction, NULL);
            break;

        case SIGUSR2:
        default:
            break;
    }
}


void Config::signalCheck(void)
{
    #if defined(JACK_SESSION)
    switch (jsessionSave)
    {
        case 1: // JackSessionSave
            saveJackSession();
            __sync_and_and_fetch(&jsessionSave, 0);
            break;
        case 2: // JackSessionSaveAndQuit
            saveJackSession();
            __sync_and_and_fetch(&jsessionSave, 0);
            usleep(3333);
            runSynth = false;
            break;
        case 3: // JackSessionSaveTemplate not implemented
            __sync_and_and_fetch(&jsessionSave, 0);
            break;
        default:
            __sync_and_and_fetch(&jsessionSave, 0);
            break;
    }
    #endif

    if (ladi1IntActive)
    {
        saveState();
        __sync_and_and_fetch(&ladi1IntActive, 0);
    }

    if (sigIntActive)
        runSynth = false;
}


void Config::setInterruptActive(void)
{
    Log("Interrupt received");
    __sync_or_and_fetch(&sigIntActive, 0xFF);
}


void Config::setLadi1Active(void)
{
    __sync_or_and_fetch(&ladi1IntActive, 0xFF);
}


bool Config::restoreJsession(SynthEngine *synth)
{
    #if defined(JACK_SESSION)
        return restoreSessionData(synth, jackSessionFile);
    #else
        return false;
    #endif
}


void Config::setJackSessionSave(int event_type, const char *session_dir, const char *client_uuid)
{
    jackSessionDir = string(session_dir);
    jackSessionUuid = string(client_uuid);
    jackSessionFile = "yoshimi-" + jackSessionUuid + ".xml";
    if (!__sync_bool_compare_and_swap (&jsessionSave, jsessionSave, event_type))
        Log("setJackSessionSave, error setting jack session save flag", true);
}


void Config::saveJackSession(void)
{
    __sync_and_and_fetch(&jsessionSave, 0);
    saveSessionData(jackSessionDir + jackSessionFile);
    if (!musicClient->jacksessionReply(programCmd + string(" -U ") + jackSessionUuid
                                       + string(" -u ${SESSION_DIR}") + jackSessionFile))
        Log("Error on jack session reply");
    jackSessionDir.clear();
    jackSessionUuid.clear();
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
        case 'R': settings->Samplerate = Runtime.string2int(string(arg)); break;
        case 'b': settings->Buffersize = Runtime.string2int(string(arg)); break;
        case 'o': settings->Oscilsize = Runtime.string2int(string(arg)); break;
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
            settings->doRestoreState = true;
            if (arg)
                settings->StateFile = string(arg);
            break;
        case 'u':
            #if defined(JACK_SESSION)
                if (arg)
                    settings->jackSessionFile = string(arg);
            #endif
            break;
        case 'U':
            #if defined(JACK_SESSION)
                if (arg)
                    settings->jackSessionUuid = string(arg);
            #endif
            break;
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
}
