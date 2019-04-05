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

    This file is a derivative of the ZynAddSubFX original, modified January 2010
*/

#include <cmath>
#include <string>
#include <argp.h>
#include <dirent.h>
#include <sys/stat.h>

#if defined(__SSE__)
#include <xmmintrin.h>
#endif

using namespace std;

#include "GuiThreadUI.h"
#include "Misc/Master.h"
#include "Misc/Config.h"

const char* argp_program_version = "Yoshimi " YOSHIMI_VERSION;
static char prog_doc[] =
    "Yoshimi " YOSHIMI_VERSION ", a derivative of ZynAddSubFX - "
    "Copyright 2002-2009 Nasca Octavian Paul and others, "
    "Copyright 2009-2010 Alan Calvert";

static struct argp_option cmd_options[] = {
    {"show-console",      'c',  NULL,         0,  "show console on startup" },
    {"name-tag",          'N',  "<tag>",      0,  "add tag to clientname" },
    {"load",              'l',  "<file>",     0,  "load .xmz file" },
    {"load-instrument",   'L',  "<file>",     0,  "load .xiz file" },
    {"samplerate",        'R',  "<rate>",     0,  "set sample rate (alsa audio)" },
    {"buffersize",        'b',  "<size>",     0,  "set buffer size (alsa audio)" },
    {"oscilsize",         'o',  "<size>",     0,  "set ADsynth oscilsize" },
    {"alsa-audio",        'A',  "<device>", 0x1,  "use alsa audio output" },
    {"no-gui",            'i',  NULL,         0,  "no gui"},
    {"alsa-midi",         'a',  "<device>", 0x1,  "use alsa midi input" },
    {"jack-audio",        'J',  "<server>", 0x1,  "use jack audio output" },
    {"jack-midi",         'j',  "<device>", 0x1,  "use jack midi input" },
    {"autostart-jack",    'k',  NULL,         0,  "auto start jack server" },
    {"auto-connect",      'K',  NULL,         0,  "auto connect jack audio" },
    {"jack-session-dir",  'd',  "<dir>",     0,  "jack session directory" },
    {"jack-session-uuid", 'u',  "<uuid>",     0,  "load jack session uuid" },
    {"state",             'S',  "<file>",   0x1,
        "load state from <file>, defaults to '$HOME/.yoshimi/yoshimi.state'" },
    { 0, }
};

static error_t parse_cmds(int key, char *arg, struct argp_state *state);
static struct argp cmd_argp = { cmd_options, parse_cmds, prog_doc };


Config Runtime;

Config::Config() :
    doRestoreState(false),
    doRestoreJackSession(false),
    Samplerate(48000),
    Buffersize(128),
    Oscilsize(1024),
    showGui(true),
    showConsole(false),
    VirKeybLayout(1),
    audioDevice("default"),
    audioEngine(DEFAULT_AUDIO),
    midiDevice("default"),
    midiEngine(DEFAULT_MIDI),
    nameTag("yoshimi"),
    jackServer("default"),
    startJack(false),
    connectJackaudio(false),
    baseCmdLine("yoshimi"),
    alsaMidiDevice("default"),
    alsaAudioDevice("default"),
    Float32bitWavs(false),
    DefaultRecordDirectory("/tmp"),
    Interpolation(0),
    CheckPADsynth(1),
    BankUIAutoClose(0),
    homeDir(string(getenv("HOME"))),
    yoshimiHome(homeDir + "/.yoshimi"),
    sse_level(0)
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
    AntiDenormals(true);       

    ConfigFile = homeDir + string("/.yoshimiXML.cfg");
    if (!isDirectory(yoshimiHome))
    {
        int chk = mkdir(yoshimiHome.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (chk)
        {
            Log(string("Failed to create yoshimi home ") + yoshimiHome
                       + string(": ") + string(strerror(chk)));
            yoshimiHome = "/tmp";
        }
    }
    StateFile = yoshimiHome + string("/yoshimi.state");
    
    clearBankrootDirlist();
    clearPresetsDirlist();
    loadConfig();

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
}


Config::~Config()
{
    AntiDenormals(false);       
}


bool Config::extractConfigData(XMLwrapper *xml)
{
    if (NULL == xml)
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
    currentBankDir = xml->getparstr("bank_current");
    Interpolation =
        xml->getpar("interpolation", Interpolation, 0, 1);
    CheckPADsynth =
        xml->getpar("check_pad_synth", CheckPADsynth, 0, 1);
    VirKeybLayout =
        xml->getpar("virtual_keyboard_layout", VirKeybLayout, 0, 10);

    // get bank dirs
    int count = 0;
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
    {
        if (xml->enterbranch("BANKROOT", i))
        {
            string dir = xml->getparstr("bank_root");
            if (isDirectory(dir))
                bankRootDirlist[count++] = dir;
            xml->exitbranch();
        }
    }
    if (!count)
    {
        string bankdirs[] = {
            "/usr/share/yoshimi/banks",
            "/usr/local/share/yoshimi/banks",
            "/usr/share/zynaddsubfx/banks",
            "/usr/local/share/zynaddsubfx/banks",
            string(getenv("HOME")) + "/banks",
            "../banks",
            "banks"
        };
        const int defaultsCount = 7; // as per bankdirs[] size above
        for (int i = 0; i < defaultsCount; ++i)
        {
            if (bankdirs[i].size() && isDirectory(bankdirs[i]))
                bankRootDirlist[count++] = bankdirs[i];
        }
    }
    if (!currentBankDir.size())
        currentBankDir = bankRootDirlist[0];

    // get preset dirs
    count = 0;
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


    // jack session
    string startcmd = xml->getparstr("base_command_line");
    if (startcmd.size())
        baseCmdLine = startcmd;

    // recorder settings
    DefaultRecordDirectory = xml->getparstr("DefaultRecordDirectory");
    Float32bitWavs = xml->getparbool("Float32bitWavs", false);

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
    XMLwrapper *xmltree = new XMLwrapper();
    addConfigXML(xmltree);
    xmltree->saveXMLfile(ConfigFile);
    delete xmltree;
    return;
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
    xmltree->addparstr("bank_current", currentBankDir);
    xmltree->addpar("virtual_keyboard_layout", VirKeybLayout);

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        if (bankRootDirlist[i].size())
        {
            xmltree->beginbranch("BANKROOT",i);
            xmltree->addparstr("bank_root", bankRootDirlist[i]);
            xmltree->endbranch();
        }

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        if (presetsDirlist[i].size())
        {
            xmltree->beginbranch("PRESETSROOT",i);
            xmltree->addparstr("presets_root", presetsDirlist[i]);
            xmltree->endbranch();
        }
    xmltree->addpar("interpolation", Interpolation);

    xmltree->addparstr("base_command_line", baseCmdLine);
    xmltree->addparstr("linux_alsa_audio_dev", alsaAudioDevice);
    xmltree->addparstr("linux_alsa_midi_dev", alsaMidiDevice);
    xmltree->addparstr("linux_jack_server", jackServer);
    xmltree->addparstr("DefaultRecordDirectory", DefaultRecordDirectory);
    xmltree->addpar("Float32bitWavs", Float32bitWavs);

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


void Config::saveSessionData(string tag, string savefile)
{
    XMLwrapper *xmltree = new XMLwrapper();
    xmltree->beginbranch(tag);
    addConfigXML(xmltree);
    addRuntimeXML(xmltree);
    zynMaster->add2XML(xmltree);
    xmltree->saveXMLfile(savefile);
    xmltree->endbranch();
}


void Config::saveState(void)
{
    saveSessionData("SESSION_STATE", StateFile);
}


void Config::saveJackSession(void)
{
    saveSessionData("JackSession", jacksessionDir + "jacksession-"
                                   + jacksessionUuid + ".xml");
}


bool Config::restoreState(Master *synth)
{
    return restoreSessionData(synth, StateFile);
}


bool Config::restoreJackSession(Master *synth)
{
    string sessionfile =
        jacksessionDir + "/jacksession-" + jacksessionUuid + ".xml";
    return restoreSessionData(synth, sessionfile);
}


bool Config::restoreSessionData(Master *synth, string sessionfile)
{
    XMLwrapper *xml = NULL;
    bool ok = false;
    if (!sessionfile.size() || !isRegFile(sessionfile))
    {
        Log("Session file " + sessionfile + " not available");
        goto end_game;
    }
    if (!(xml = new XMLwrapper()))
    {
        Log("Failed to init xmltree for restoreState");
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
            {
                if (ParamsHistory.at(i).sameFile(file))
                    ParamsHistory.erase(itx);
            }
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


void Config::clearBankrootDirlist(void)
{
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        bankRootDirlist[i].clear();
}


void Config::clearPresetsDirlist(void)
{
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        presetsDirlist[i].clear();
}


bool Config::loadConfig(void)
{
    XMLwrapper *xml = new XMLwrapper();
    if (!ConfigFile.size() || !isRegFile(ConfigFile))
    {
        Log("Config file " + ConfigFile + " not available");
        return false;
    }
    if (xml->loadXMLfile(ConfigFile) < 0)
        return false;

    bool isok = extractConfigData(xml);
    if (isok)
    {
        Oscilsize = (int) powf(2.0f, ceil(log (Oscilsize - 1.0f) / logf(2.0)));
        if (DefaultRecordDirectory.empty())
            DefaultRecordDirectory = string("/tmp/");
        if (DefaultRecordDirectory.at(DefaultRecordDirectory.size() - 1) != '/')
            DefaultRecordDirectory += "/";
        if (CurrentRecordDirectory.empty())
            CurrentRecordDirectory = DefaultRecordDirectory;
    }
    delete xml;
    return isok;
}


void Config::sigHandler(int sig)
{
    switch (sig)
    {
        case SIGINT:
        case SIGHUP:
        case SIGTERM:
        case SIGQUIT:
            Runtime.setInterruptActive(sig);
            break;

        case SIGUSR1:
            Runtime.setLadi1Active(sig);
            sigaction(SIGUSR1, &sigAction, NULL);
            break;

        default:
            Runtime.Log("Unexpected signal: " + asString(sig));
            break;
    }
}


void Config::signalCheck(void)
{
    if (jsessionSave)
    {
        saveJackSession();
        __sync_sub_and_fetch(&jsessionSave, 1);
    }
    
    if (jsessionSaveAndQuit)
    {
        saveJackSession();
        __sync_sub_and_fetch(&jsessionSaveAndQuit, 1); 
        Pexitprogram = true;
    }
    
    if (jsessionSaveTemplate)
    {
        saveJackSessionTemplate();
        __sync_sub_and_fetch(&jsessionSaveTemplate, 1);
    }
    
    if (ladi1IntActive)
    {
        saveState();
        __sync_sub_and_fetch (&ladi1IntActive, 1);
    }

    if (sigIntActive)
        Pexitprogram = true;
}


void Config::setInterruptActive(int sig)
{
    Log("Interrupt received");
    __sync_add_and_fetch(&sigIntActive, 1);
}


void Config::setLadi1Active(int sig)
{ __sync_add_and_fetch(&ladi1IntActive, 1); }


void Config::setJackSessionDetail(const char *session_dir, const char *client_uuid)
{
    jacksessionDir = string(session_dir);
    jacksessionUuid = string(client_uuid);
}


void Config::setJackSessionSave(void)
{ __sync_add_and_fetch(&jsessionSave, 1); }


void Config::setJackSessionSaveAndQuit(void)
{ __sync_add_and_fetch(&jsessionSaveAndQuit, 1); }


void Config::setJackSessionSaveTemplate(void)
    { __sync_add_and_fetch(&jsessionSaveTemplate, 1); }


void Config::bringOutYerDead(void)
{
    if (!dead_ptrs.empty())
        dead_ptrs.clear();
    if (!dead_floats.empty())
        dead_floats.clear();
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


int Config::SSEcapability(void)
{
#if !defined(__SSE__)
    return 0;
#else
#   if defined(__x86_64__)
        int64_t edx;
        __asm__ __volatile__ (
            "mov %%rbx,%%rdi\n\t" // save PIC register
            "movl $1,%%eax\n\t"
            "cpuid\n\t"
            "mov %%rdi,%%rbx\n\t" // restore PIC register
            : "=d" (edx)
            : : "%rax", "%rcx", "%rdi"
        );
#   else
        int32_t edx;
        __asm__ __volatile__ (
            "movl %%ebx,%%edi\n\t" // save PIC register
            "movl $1,%%eax\n\t"
            "cpuid\n\t"
            "movl %%edi,%%ebx\n\t" // restore PIC register
            : "=d" (edx)
            : : "%eax", "%ecx", "%edi"
        );
#   endif
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


void Config::Log(string msg)
{
    if (showGui)
        LogList.push_back(msg);
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


void Config::StartupReport(unsigned int samplerate, int buffersize)
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
    Log("Oscilsize: " + asString(Runtime.Oscilsize));
    Log("Samplerate: " + asString(samplerate));
    Log("Buffersize: " + asString(buffersize));
    Log("Alleged latency: " + asString(buffersize) + " frames, "
        + asString(buffersize * 1000.0f / samplerate) + " ms");
    Log("Gross latency: " + asString(musicClient->grossLatency()) + " frames, "
         + asString(musicClient->grossLatency() * 1000.0f / samplerate) + " ms");
}


static error_t parse_cmds (int key, char *arg, struct argp_state *state)
{
    Config *settings = (Config*)state->input;

    switch (key)
    {
        case 'c': settings->showConsole = true; break;
        case 'N': settings->nameTag = string(arg); break;
        case 'l': settings->paramsLoad = string(arg); break;
        case 'L': settings->instrumentLoad = string(arg); break;
        case 'R': settings->Samplerate = string2int(string(arg)); break;
        case 'b': settings->Buffersize = string2int(string(arg)); break;
        case 'o': settings->Oscilsize = string2int(string(arg)); break;
        case 'k': settings->startJack = true; break;
        case 'K': settings->connectJackaudio = true; break;
        case 'd': settings->jacksessionDir = string(arg); break;
        case 'u': settings->jacksessionUuid = string(arg);
                  settings->doRestoreJackSession = true;
                  break;
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
        case 'S':
            settings->doRestoreState = true;
            if (arg)
                settings->StateFile = string(arg);
            break;
        case ARGP_KEY_ARG:
        case ARGP_KEY_END:
            break;
        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


void Config::loadCmdArgs(int argc, char **argv)
{
    argp_parse(&cmd_argp, argc, argv, 0, 0, this);
}


int Config::sigIntActive = 0;
int Config::ladi1IntActive = 0;
struct sigaction Config::sigAction;

int Config::jsessionSave = 0;
int Config::jsessionSaveAndQuit = 0;
int Config::jsessionSaveTemplate = 0;

const unsigned short Config::MaxParamsHistory = 25;
unsigned short Config::nextHistoryIndex = numeric_limits<unsigned int>::max();
