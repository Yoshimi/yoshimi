/*
    Config.cpp - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cmath>
#include <string>
#include <iostream>

using namespace std;

#include "Misc/Util.h"
#include "Misc/Config.h"
#include "Misc/XMLwrapper.h"

int thread_priority = 50;

bool autostart_jack = false;

Config Runtime;

Config::Config()
{
    // defaults
    settings.Oscilsize = 2048;
    settings.Samplerate = 48000; // for Alsa audio, just a hint
    settings.Buffersize = 256;   // for Alsa audio, just a hint
#   if defined(DISABLE_GUI)
        settings.showGui = false;
#   else
        settings.showGui = true;
#   endif
    settings.verbose = true;
    settings.LinuxALSAaudioDev = "default";
    settings.LinuxALSAmidiDev = "default";
    settings.LinuxJACKserver = "default";
    settings.BankUIAutoClose = 0;
    settings.GzipCompression = 3;
    settings.Interpolation = 0;
    settings.CheckPADsynth = 1;
    settings.UserInterfaceMode = 0;
    settings.VirKeybLayout = 1;
    settings.audioEngine = DEFAULT_AUDIO;
    settings.midiEngine  = DEFAULT_MIDI;
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        settings.bankRootDirList[i] = NULL;
    settings.currentBankDir = new char[MAX_STRING_SIZE];
    sprintf(settings.currentBankDir, "./testbnk");

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        settings.presetsDirList[i] = NULL;

    readConfig();
    switch (settings.audioEngine)
    {
        case alsa_audio:
            settings.audioDevice = string(settings.LinuxALSAaudioDev);
            break;

        case jack_audio:
            settings.audioDevice = string(settings.LinuxJACKserver);
            break;

        case no_audio:
        default:
            settings.audioDevice.clear(); // = string();
            break;
    }
    if (!settings.audioDevice.size())
        settings.audioDevice = "default";

    switch (settings.midiEngine)
    {
        case jack_midi:
            settings.midiDevice = string(settings.LinuxJACKserver);
            break;

        case alsa_midi:
            settings.midiDevice = string(settings.LinuxALSAmidiDev);
            break;

        case no_midi:
        default:
            settings.midiDevice.clear();
            break;
    }
    if (!settings.midiDevice.size())
        settings.midiDevice = "default";

    if (settings.bankRootDirList[0] == NULL)
    {
        // banks
        settings.bankRootDirList[0] = new char[MAX_STRING_SIZE];
        sprintf(settings.bankRootDirList[0], "~/banks");

        settings.bankRootDirList[1] = new char[MAX_STRING_SIZE];
        sprintf(settings.bankRootDirList[1], "./");

        settings.bankRootDirList[2] = new char[MAX_STRING_SIZE];
        sprintf(settings.bankRootDirList[2], "/usr/share/zynaddsubfx/banks");

        settings.bankRootDirList[3] = new char[MAX_STRING_SIZE];
        sprintf(settings.bankRootDirList[3], "/usr/local/share/zynaddsubfx/banks");

        settings.bankRootDirList[4] = new char[MAX_STRING_SIZE];
        sprintf(settings.bankRootDirList[4], "../banks");

        settings.bankRootDirList[5] = new char[MAX_STRING_SIZE];
        sprintf(settings.bankRootDirList[5], "banks");
    }

    if (settings.presetsDirList[0] == NULL)
    {
        // presets
        settings.presetsDirList[0] = new char[MAX_STRING_SIZE];
        sprintf(settings.presetsDirList[0], "./");
        settings.presetsDirList[1] = new char[MAX_STRING_SIZE];
        sprintf(settings.presetsDirList[1], "../presets");
        settings.presetsDirList[2] = new char[MAX_STRING_SIZE];
        sprintf(settings.presetsDirList[2], "presets");
        settings.presetsDirList[3] = new char[MAX_STRING_SIZE];
        sprintf(settings.presetsDirList[3], "/usr/share/zynaddsubfx/presets");
        settings.presetsDirList[4] = new char[MAX_STRING_SIZE];
        sprintf(settings.presetsDirList[4], "/usr/local/share/zynaddsubfx/presets");
    }
}


void Config::clearbankrootdirlist(void)
{
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
    {
        if (settings.bankRootDirList[i] == NULL)
            delete(settings.bankRootDirList[i]);
        settings.bankRootDirList[i] = NULL;
    }
}


void Config::clearpresetsdirlist(void)
{
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
    {
        if (settings.presetsDirList[i] == NULL)
            delete(settings.presetsDirList[i]);
        settings.presetsDirList[i] = NULL;
    }
}


void Config::readConfig(void)
{
    XMLwrapper *xmlcfg = new XMLwrapper();
    if (xmlcfg->loadXMLfile(getConfigFilename(false)) < 0)
        return;
    if (xmlcfg->enterbranch("CONFIGURATION"))
    {
        settings.Oscilsize = xmlcfg->getpar("oscil_size", settings.Oscilsize,
                                       MAX_AD_HARMONICS * 2, 131072);
        settings.BankUIAutoClose = xmlcfg->getpar("bank_window_auto_close",
                                             settings.BankUIAutoClose, 0, 1);

        settings.GzipCompression = xmlcfg->getpar("gzip_compression",
                                             settings.GzipCompression, 0, 9);

        xmlcfg->getparstr("bank_current", settings.currentBankDir, MAX_STRING_SIZE);
        settings.Interpolation=xmlcfg->getpar("interpolation", settings.Interpolation, 0, 1);

        settings.CheckPADsynth=xmlcfg->getpar("check_pad_synth", settings.CheckPADsynth, 0, 1);

        settings.UserInterfaceMode = xmlcfg->getpar("user_interface_mode",
                                               settings.UserInterfaceMode, 0, 2);
        settings.VirKeybLayout = xmlcfg->getpar("virtual_keyboard_layout",
                                           settings.VirKeybLayout, 0, 10);

        // get bankroot dirs
        for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        {
            if (xmlcfg->enterbranch("BANKROOT", i))
            {
                settings.bankRootDirList[i] = new char[MAX_STRING_SIZE];
                xmlcfg->getparstr("bank_root", settings.bankRootDirList[i],
                                  MAX_STRING_SIZE);
                xmlcfg->exitbranch();
            }
        }

        // get preset root dirs
        for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        {
            if (xmlcfg->enterbranch("PRESETSROOT", i))
            {
                settings.presetsDirList[i] = new char[MAX_STRING_SIZE];
                xmlcfg->getparstr("presets_root", settings.presetsDirList[i],
                                  MAX_STRING_SIZE);
                xmlcfg->exitbranch();
            }
        }

        // linux stuff
        xmlcfg->getparstr("linux_alsa_audio_dev", settings.LinuxALSAaudioDev, MAX_STRING_SIZE);
        xmlcfg->getparstr("linux_alsa_midi_dev", settings.LinuxALSAmidiDev, MAX_STRING_SIZE);
        xmlcfg->getparstr("linux_jack_server", settings.LinuxJACKserver, MAX_STRING_SIZE);
        xmlcfg->exitbranch();
    }
    delete(xmlcfg);
    settings.Oscilsize = (int) powf(2, ceil(log (settings.Oscilsize - 1.0) / logf(2.0)));
}

void Config::saveConfig(void)
{
    XMLwrapper *xmlcfg = new XMLwrapper();

    xmlcfg->beginbranch("CONFIGURATION");

    xmlcfg->addpar("sample_rate", settings.Samplerate);
    xmlcfg->addpar("sound_buffer_size", settings.Buffersize);
    xmlcfg->addpar("oscil_size", settings.Oscilsize);
    xmlcfg->addpar("swap_stereo", 0);
    xmlcfg->addpar("bank_window_auto_close", settings.BankUIAutoClose);
    xmlcfg->addpar("gzip_compression", settings.GzipCompression);
    xmlcfg->addpar("check_pad_synth", settings.CheckPADsynth);
    xmlcfg->addparstr("bank_current", settings.currentBankDir);
    xmlcfg->addpar("user_interface_mode", settings.UserInterfaceMode);
    xmlcfg->addpar("virtual_keyboard_layout", settings.VirKeybLayout);

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        if (settings.bankRootDirList[i] != NULL)
        {
            xmlcfg->beginbranch("BANKROOT",i);
            xmlcfg->addparstr("bank_root", settings.bankRootDirList[i]);
            xmlcfg->endbranch();
        }

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        if (settings.presetsDirList[i] != NULL)
        {
            xmlcfg->beginbranch("PRESETSROOT",i);
            xmlcfg->addparstr("presets_root", settings.presetsDirList[i]);
            xmlcfg->endbranch();
        }
    xmlcfg->addpar("interpolation", settings.Interpolation);

    // linux stuff
    xmlcfg->addparstr("linux_alsa_audio_dev", settings.LinuxALSAaudioDev);
    xmlcfg->addparstr("linux_alsa_midi_dev", settings.LinuxALSAmidiDev);
    xmlcfg->addparstr("linux_jack_server", settings.LinuxJACKserver);
    xmlcfg->endbranch();

    int tmp = settings.GzipCompression;
    settings.GzipCompression = 0;
    xmlcfg->saveXMLfile(getConfigFilename(true));
    settings.GzipCompression = tmp;
    delete(xmlcfg);
}


string Config::getConfigFilename(bool for_save)
{
    // Try to prevent any perversion of ~/.zynaddsubfxXML.cfg.  For read,
    // use ~/.yoshimiXML.cfg if it exists, otherwise ~/.zynaddsubfxXML.cfg
    // For save, use ~/.yoshimiXML.cfg
    string yoshiCfg = string(getenv("HOME")) + "/.yoshimiXML.cfg";
    if (for_save)
        return yoshiCfg;
    else if (fileexists(yoshiCfg.c_str()))
        return yoshiCfg;
    else
        return string(getenv("HOME")) + "/.zynaddsubfxXML.cfg";
}


void Config::StartupReport(unsigned int samplerate, int buffersize)
{
    if (settings.verbose)
    {
        cerr << "Yoshimi " << YOSHIMI_VERSION << endl;
        cerr << "ADsynth Oscilsize: " << Runtime.settings.Oscilsize << endl;
        cerr << "Sample Rate: " << samplerate << endl;
        cerr << "Sound Buffer Size: " << buffersize << endl;
        cerr << "Internal latency: " << buffersize * 1000.0 / samplerate << " ms" << endl;
    }
}


void Config::Usage(void)
{
    cerr << "This is NOT ZynAddSubFX - ";
    cerr << "Copyright (c) 2002-2009 Nasca Octavian Paul and others," << endl;
    cerr << "this is instead a derivative of ZynAddSubFX known as " << endl;
    cerr << "Yoshimi Version " << YOSHIMI_VERSION << ", ";
    cerr << "Copyright (C) 2009, Alan Calvert" << endl;
    cerr << "Yoshimi comes with high hopes that it might be useful," << endl;
    cerr << "but ABSOLUTELY NO WARRANTY.  This is free software, and" << endl;
    cerr << "you are welcome to redistribute it under certain conditions;" << endl;
    cerr << "See file COPYING for details." << endl;
    cout << "Usage: yoshimi [option(s) ...]" << endl;
    cout << "  -h, --help                         display command-line help and exit" << endl;
    cout << "  -l<file>, --load=<file>            load an .xmz file" << endl;
    cout << "  -L<file>, --load-instrument=<file> load an .xiz file" << endl;
    cout << "  -o<OS>, --oscil-size=<OS>          set the ADsynth oscil. size" << endl;
    cout << "  -U , --no-gui                      run without user interface" << endl;
    cout << "  -a[dev], --alsa-midi[=dev]         use ALSA midi on optional device dev" << endl;
    cout << "  -A[dev], --alsa-audio[=dev]        use ALSA audio on optional device dev" << endl;
    cout << "  -j[server], --jack-midi[=server]   use jack midi on optional server" << endl;
    cout << "  -J[server], --jack-audio[=server]  use jack audio on optional server" << endl;
    cout << "  -k , --autostart-jack              autostart jack server" << endl;
    cout << "  -q, --quiet                        less verbose messages" << endl;
    cout << endl << endl;
}
