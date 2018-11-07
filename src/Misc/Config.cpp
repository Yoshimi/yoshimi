/*
    Config.cpp - Configuration file functions

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/

#include <cmath>
#include <string>
#include <iostream>

using namespace std;

#include "Misc/Util.h"
#include "Misc/Config.h"
#include "Misc/XMLwrapper.h"

bool autostart_jack = false;

Config Runtime;

Config::Config()
{
    // defaults
    settings.Oscilsize = 2048;
    settings.Samplerate = 48000;
    settings.Buffersize = 512;
#   if defined(DISABLE_GUI)
        settings.showGui = false;
#   else
        settings.showGui = true;
#   endif
    settings.verbose = true;
    settings.LinuxALSAaudioDev = "default";
    settings.LinuxALSAmidiDev = "default";
    settings.LinuxJACKserver = "default";
    settings.nameTag = string();
    settings.BankUIAutoClose = 0;
    settings.GzipCompression = 3;
    settings.Interpolation = 0;
    settings.CheckPADsynth = 1;
    settings.UserInterfaceMode = 0;
    settings.VirKeybLayout = 1;
    settings.audioEngine = DEFAULT_AUDIO;
    settings.midiEngine  = DEFAULT_MIDI;

    settings.DefaultRecordDirectory = "/tmp";
    settings.Float32bitWavs  = 1;

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        settings.bankRootDirlist[i] = string();
    settings.currentBankDir = string("./testbnk");

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        settings.presetsDirlist[i] = string();

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

    if (!settings.bankRootDirlist[0].size())
    {
        // banks
        settings.bankRootDirlist[0] = "~/banks";
        settings.bankRootDirlist[1] = "./";
        settings.bankRootDirlist[2] = "/usr/share/zynaddsubfx/banks";
        settings.bankRootDirlist[3] = "/usr/local/share/zynaddsubfx/banks";
        settings.bankRootDirlist[4] = "../banks";
        settings.bankRootDirlist[5] = "banks";
    }

    if (!settings.presetsDirlist[0].size())
    {
        // presets
        settings.presetsDirlist[0] = "./";
        settings.presetsDirlist[1] = "../presets";
        settings.presetsDirlist[2] = "presets";
        settings.presetsDirlist[3] = "/usr/share/zynaddsubfx/presets";
        settings.presetsDirlist[4] = "/usr/local/share/zynaddsubfx/presets";
    }
}


void Config::clearBankrootDirlist(void)
{
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        settings.bankRootDirlist[i].clear();
}


void Config::clearPresetsDirlist(void)
{
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        settings.presetsDirlist[i].clear();
}


void Config::readConfig(void)
{
    XMLwrapper *xmlcfg = new XMLwrapper();
    if (xmlcfg->loadXMLfile(getConfigFilename(false)) < 0)
        return;
    if (xmlcfg->enterbranch("CONFIGURATION"))
    {
        settings.Samplerate = xmlcfg->getpar("sample_rate", settings.Samplerate,
                                             44100, 96000);
        settings.Buffersize = xmlcfg->getpar("sound_buffer_size",
                                             settings.Buffersize, 64, 4096);
        settings.Oscilsize = xmlcfg->getpar("oscil_size", settings.Oscilsize,
                                            MAX_AD_HARMONICS * 2, 131072);
        xmlcfg->getpar("swap_stereo", 0, 0, 1);                 // deprecated
        settings.BankUIAutoClose = xmlcfg->getpar("bank_window_auto_close",
                                                   settings.BankUIAutoClose, 0, 1);
        // Dump deprecated in yoshi
        xmlcfg->getpar("dump_notes_to_file", 0, 0, 1);
        xmlcfg->getpar("dump_append", 0, 0, 1);
        xmlcfg->getparstr("dump_file");

        settings.GzipCompression =
            xmlcfg->getpar("gzip_compression", settings.GzipCompression, 0, 9);
        settings.currentBankDir = xmlcfg->getparstr("bank_current");
        settings.Interpolation =
            xmlcfg->getpar("interpolation", settings.Interpolation, 0, 1);
        settings.CheckPADsynth =
            xmlcfg->getpar("check_pad_synth", settings.CheckPADsynth, 0, 1);
        settings.UserInterfaceMode =
            xmlcfg->getpar("user_interface_mode", settings.UserInterfaceMode, 0, 2);
        settings.VirKeybLayout =
            xmlcfg->getpar("virtual_keyboard_layout", settings.VirKeybLayout, 0, 10);

        // get bankroot dirs
        for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        {
            if (xmlcfg->enterbranch("BANKROOT", i))
            {
                settings.bankRootDirlist[i] = xmlcfg->getparstr("bank_root");
                xmlcfg->exitbranch();
            }
        }

        // get preset root dirs
        for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        {
            if (xmlcfg->enterbranch("PRESETSROOT", i))
            {
                settings.presetsDirlist[i] = xmlcfg->getparstr("presets_root");
                xmlcfg->exitbranch();
            }
        }

        xmlcfg->getparstr("linux_oss_wave_out_dev"); // deprecated in yoshi
        xmlcfg->getparstr("linux_oss_seq_in_dev");   // deprecated in yoshi

        // windows stuff, deprecated in yoshi
        xmlcfg->getpar("windows_wave_out_id", 0, 0, 0);
        xmlcfg->getpar("windows_midi_in_id", 0, 0, 0);

        // yoshi only settings
        settings.LinuxALSAaudioDev = xmlcfg->getparstr("linux_alsa_audio_dev");
        settings.LinuxALSAmidiDev = xmlcfg->getparstr("linux_alsa_midi_dev");
        settings.LinuxJACKserver = xmlcfg->getparstr("linux_jack_server");

        settings.DefaultRecordDirectory = xmlcfg->getparstr("DefaultRecordDirectory");
        if (settings.DefaultRecordDirectory.empty())
            settings.DefaultRecordDirectory = string("/tmp/");
        if (settings.DefaultRecordDirectory.at(settings.DefaultRecordDirectory.size() - 1) != '/')
                settings.DefaultRecordDirectory += "/";
        if (settings.CurrentRecordDirectory.empty())
            settings.CurrentRecordDirectory = settings.DefaultRecordDirectory;
        settings.Float32bitWavs = xmlcfg->getpar("Float32bitWavs", 0, 0, 1);

        xmlcfg->exitbranch(); // CONFIGURATION
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

    xmlcfg->addpar("dump_notes_to_file", 0);
    xmlcfg->addpar("dump_append", 0);
    xmlcfg->addparstr("dump_file", "");

    xmlcfg->addpar("gzip_compression", settings.GzipCompression);
    xmlcfg->addpar("check_pad_synth", settings.CheckPADsynth);
    xmlcfg->addparstr("bank_current", settings.currentBankDir);
    xmlcfg->addpar("user_interface_mode", settings.UserInterfaceMode);
    xmlcfg->addpar("virtual_keyboard_layout", settings.VirKeybLayout);

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        if (settings.bankRootDirlist[i].size())
        {
            xmlcfg->beginbranch("BANKROOT",i);
            xmlcfg->addparstr("bank_root", settings.bankRootDirlist[i]);
            xmlcfg->endbranch();
        }

    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        if (settings.presetsDirlist[i].size())
        {
            xmlcfg->beginbranch("PRESETSROOT",i);
            xmlcfg->addparstr("presets_root", settings.presetsDirlist[i]);
            xmlcfg->endbranch();
        }
    xmlcfg->addpar("interpolation", settings.Interpolation);

    // linux stuff - deprecated in yoshi
    xmlcfg->addparstr("linux_oss_wave_out_dev", "/dev/dsp");
    xmlcfg->addparstr("linux_oss_seq_in_dev", "/dev/sequencer");

    // windows stuff - deprecated in yoshi
    xmlcfg->addpar("windows_wave_out_id", 0);
    xmlcfg->addpar("windows_midi_in_id", 0);

    // yoshi specific
    xmlcfg->addparstr("linux_alsa_audio_dev", settings.LinuxALSAaudioDev);
    xmlcfg->addparstr("linux_alsa_midi_dev", settings.LinuxALSAmidiDev);
    xmlcfg->addparstr("linux_jack_server", settings.LinuxJACKserver);
    xmlcfg->addparstr("DefaultRecordDirectory", settings.DefaultRecordDirectory);
    xmlcfg->addpar("Float32bitWavs", settings.Float32bitWavs);

    xmlcfg->endbranch(); // CONFIGURATION

    int tmp = settings.GzipCompression;
    settings.GzipCompression = 0;
    xmlcfg->saveXMLfile(getConfigFilename(true));
    settings.GzipCompression = tmp;
    delete xmlcfg;
}


string Config::getConfigFilename(bool for_save)
{
    // Try to prevent any perversion of ~/.zynaddsubfxXML.cfg.  For read,
    // use ~/.yoshimiXML.cfg if it exists, otherwise ~/.zynaddsubfxXML.cfg
    // For save, use ~/.yoshimiXML.cfg
    string yoshiCfg = string(getenv("HOME")) + "/.yoshimiXML.cfg";
    if (for_save || isRegFile(yoshiCfg))
        return yoshiCfg;
    else if (isRegFile(yoshiCfg))
        return yoshiCfg;
    else
        return string(getenv("HOME")) + "/.zynaddsubfxXML.cfg";
}


void Config::Announce(void)
{
    settings.verbose && cout << "Yoshimi " << YOSHIMI_VERSION << endl;
}


void Config::StartupReport(unsigned int samplerate, int buffersize)
{
    if (settings.verbose)
    {
        cout << "ADsynth Oscilsize: " << Runtime.settings.Oscilsize << endl;
        cout << "Sample Rate: " << samplerate << endl;
        cout << "Sound Buffer Size: " << buffersize << endl;
        cout << "Internal latency: " << buffersize * 1000.0 / samplerate << " ms" << endl;
    }
}


void Config::Usage(void)
{
    cerr << "This is NOT ZynAddSubFX - ";
    cerr << "Copyright (c) 2002-2009 Nasca Octavian Paul and others," << endl;
    cerr << "this is instead a derivative of ZynAddSubFX known as " << endl;
    cerr << "Yoshimi Copyright (C) 2009, Alan Calvert" << endl;
    cerr << "This is version " << YOSHIMI_VERSION << endl;
    cerr << "Yoshimi comes with high hopes that it might be useful," << endl;
    cerr << "but ABSOLUTELY NO WARRANTY.  This is free software, and" << endl;
    cerr << "you are welcome to redistribute it under certain conditions;" << endl;
    cerr << "See file COPYING for details." << endl;
    cerr << "Usage: yoshimi [option(s) ...]" << endl;
    cerr << "  -h, --help                         display command-line help and exit" << endl;
    cerr << "  -l<file>, --load=<file>            load an .xmz file" << endl;
    cerr << "  -L<file>, --load-instrument=<file> load an .xiz file" << endl;
    cerr << "  -o<OS>, --oscil-size=<OS>          set the ADsynth oscil. size" << endl;
    cerr << "  -U , --no-gui                      run without user interface" << endl;
    cerr << "  -a[dev], --alsa-midi[=dev]         use ALSA midi on optional device dev" << endl;
    cerr << "  -b<size>, --buffersize=<size>      set buffersize (range 64 - 4096)" << endl;
    cerr << "  -r<srate>, --samplerate=<srate>    set samplerate (44100, 48000, or 96000)" << endl;
    cerr << "  -A[dev], --alsa-audio[=dev]        use ALSA audio on optional device dev" << endl;
    cerr << "  -j[server], --jack-midi[=server]   use jack midi on optional server" << endl;
    cerr << "  -J[server], --jack-audio[=server]  use jack audio on optional server" << endl;
    cerr << "  -k , --autostart-jack              autostart jack server" << endl;
    cerr << "  -N<tag>, --name-tag=<tag>          add <tag> to client name" << endl;
    cerr << "  -q, --quiet                        less verbose messages" << endl;
    cerr << "Buffersize and samplerate settings apply to Alsa audio only." << endl;
    cerr << endl << endl;
}
