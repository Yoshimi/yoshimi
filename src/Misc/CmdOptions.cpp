/*
    CmdOptions.cpp

    Copyright 2021-2023, Will Godfrey and others

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <list>
#include <errno.h>
#include <string>
#include <cstring>
#include <argp.h>

#include "Misc/Config.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/CmdOptions.h"
#include "Misc/FormatFuncs.h"

using std::string;
using file::setExtension;
using func::string2int;

namespace { // constants used in the implementation
    const char* PROG_DOC =
        "Options:"
        "\v"   // the following is printed below the options summary...
        "\nYoshimi " YOSHIMI_VERSION ", a derivative of ZynAddSubFX\n"
        "Copyright 2002-2009 Nasca Octavian Paul and others,\n"
        "Copyright 2009-2011 Alan Calvert,\n"
        "Copyright 2012-2013 Jeremy Jongepier and others,\n"
        "Copyright 2014-2025 Will Godfrey and others";

    string stateText = "load saved state, defaults to '$HOME/" + EXTEN::config + "/yoshimi/yoshimi-0"+EXTEN::state+"'";

    const argp_option OPTION_SPEC[] = {
        {"alsa-audio",        'A',  "<device>", OPTION_ARG_OPTIONAL, "use alsa audio output", 0},
        {"alsa-midi",         'a',  "<device>", OPTION_ARG_OPTIONAL, "use alsa midi input",   0},
        {"define-root",       'D',  "<path>",   0                  , "define path to new bank root", 2},
        {"buffersize",        'b',  "<size>",   0                  , "set internal buffer size",     2},
        {"no-gui",            'i',  NULL,       0                  , "disable gui", 1},
        {"gui",               'I',  NULL,       0                  , "enable gui",  1},
        {"no-cmdline",        'c',  NULL,       0                  , "disable command line interface", 1},
        {"cmdline",           'C',  NULL,       0                  , "enable command line interface",  1},
        {"jack-audio",        'J',  "<server>", OPTION_ARG_OPTIONAL,  "use jack audio output", 0},
        {"jack-midi",         'j',  "<device>", OPTION_ARG_OPTIONAL,  "use jack midi input",   0},
        {"autostart-jack",    'k',  NULL,       0                  , "auto start jack server", 0},
        {"auto-connect",      'K',  NULL,       0                  , "auto connect jack audio",0},
        {"load",              'l',  "<file>",   0                  , "load .xmz parameters file",    2},
        {"load-instrument",   'L',  "<file>[@part]",0                , "load .xiz instrument file(no space)@n to part 'n'", 2},
        {"load-midilearn",    'M',  "<file>",   0                  , "load .xly midi-learn file",    2},
        {"name-tag",          'N',  "<tag>",    0                  , "add tag to clientname",        2},
        {"samplerate",        'R',  "<rate>",   0                  , "set alsa audio sample rate",   1},
        {"oscilsize",         'o',  "<size>",   0                  , "set AddSynth oscillator size", 1},
        {"state",             'S',  "<file>",   0                  , "load .state complete machine setup file", 2},
        {"load-guitheme",     'T',  "<file>",   0                  , "load .clr GUI theme file",                2},
        {"null",               13,  NULL,       0                  , "use Null-backend without audio/midi",     0},
#if defined(JACK_SESSION)
        {"jack-session-uuid", 'U',  "<uuid>",   0                  , "jack session uuid",            2},
        {"jack-session-file", 'u',  "<file>",   0                  , "load named jack session file", 2},
#endif
        { 0, 0, 0, 0, 0, 0}
    };

    using Settings = CmdOptions::Settings;


    error_t handleOption (int key, char *arg, struct argp_state *state)
    {
        auto settings = [&]{ return static_cast<Settings*>(state->input); };

        auto recordToggle = [&]{ settings()->emplace_back(char(key), "");                  };
        auto recordOption = [&]{ settings()->emplace_back(char(key), string{arg? arg:""}); };

        if (arg && arg[0] == '=')
            ++ arg;

        switch (key)
        {
            case 'A': recordOption(); break;     // ALSA audio
            case 'a': recordOption(); break;     // ALSA MIDI
            case 'b': recordOption(); break;     // buffer-size
            case 'c': recordToggle(); break;     // disable CLI
            case 'C': recordToggle(); break;     // enable CLIE
            case 'D': recordOption(); break;     // define bank root
            case 'i': recordToggle(); break;     // without GUI
            case 'I': recordToggle(); break;     // with GUI
            case 'J': recordOption(); break;     // Jack audio
            case 'j': recordOption(); break;     // Jack MIDI
            case 'k': recordToggle(); break;     // auto-start Jack
            case 'K': recordToggle(); break;     // auto-connect to Jack
            case 'l': recordOption(); break;     // load params file
            case 'L': recordOption(); break;     // load instrument
            case 'M': recordOption(); break;     // load MIDI-learn
            case 'N': recordOption(); break;     // name-tag
            case 'o': recordOption(); break;     // oscil-size
            case 'T': recordOption(); break;     // load gui theme
            case 'R': recordOption(); break;     // set ALSA sample rate
            case 'S': recordOption(); break;     // load complete state file

            case 13:  recordToggle(); break;     // NULL backend (no audio and MIDI)

#if defined(JACK_SESSION)
            case 'u': recordOption(); break;     // load Jack session file
            case 'U': recordOption(); break;     // Jack session UUID
#endif
            case ARGP_KEY_ARG:
            case ARGP_KEY_END:
                break;
            default:
                return error_t(ARGP_ERR_UNKNOWN);
        }
        return error_t(0);
    }


    const argp PARSER_SETUP = { OPTION_SPEC, handleOption, 0, PROG_DOC, 0, 0, 0};

}//(End) local definitions for parser configuration


CmdOptions::Settings CmdOptions::parseCmdline(int argc, char **argv)
{
    Settings parsedOptions;
    argp_parse(&PARSER_SETUP, argc, argv, 0, 0, &parsedOptions);
    return parsedOptions;
}


void CmdOptions::applyTo (Config& config)  const
{
    for (auto const& [cmd, line] : settings)
    {
        switch (cmd)
        {
            case 'A':
                config.engineChanged = true;
                config.audioEngine = alsa_audio;
                if (not line.empty())
                    config.audioDevice = line;
                else
                    config.audioDevice = config.alsaAudioDevice;
            break;

            case 'a':
                config.midiChanged = true;
                config.midiEngine = alsa_midi;
                if (not line.empty())
                    config.midiDevice = line;
                else
                    config.midiDevice = string(config.alsaMidiDevice);
                break;

            case 'b':
                config.configChanged = true;
                config.bufferChanged = true;
                config.buffersize = string2int(line);
                break;

            case 'c':
                config.cliChanged = true;
                config.showCli = false;
                break;

            case 'C':
                config.cliChanged = true;
                config.showCli = true;
                break;

            case 'D':
                if (not line.empty())
                    config.rootDefine = line;
                break;

            case 'i':
                config.guiChanged = true;
                config.showGui = false;
                break;

            case 'I':
                config.guiChanged = true;
                config.showGui = true;
                break;

            case 'J':
                config.engineChanged = true;
                config.audioEngine = jack_audio;
                if (!line.empty())
                    config.audioDevice = line;
                break;

            case 'j':
                config.midiChanged = true;
                config.midiEngine = jack_midi;
                if (not line.empty())
                    config.midiDevice = line;
                else
                    config.midiDevice = config.jackMidiDevice;
                break;

            case 'K':
                config.connectJackChanged = true;
                config.connectJackaudio = true;
                break;

            case 'k':
                config.startJack = true;
                break;

            case 'l':
                config.paramsLoad = line;
                break;

            case 'L':
                {// Load instrument@part
                    uint partLoad = 0;
                    string spec(line);
                    size_t pos = spec.rfind("@");
                    if (pos != string::npos)
                    {// this provides a way to specify which part to load to
                        if (spec.length() - pos <= 3)
                        {
                            partLoad = (stoi("0" + spec.substr(pos + 1))) - 1;
                        }
                        if (partLoad >= 64)
                            partLoad = 0;
                        spec = spec.substr(0, pos);
                    }
                    config.load2part = partLoad;
                    config.instrumentLoad = spec;
                    config.configChanged = true;
                }
                break;

            case 'M':
                config.midiLearnLoad = line;
                break;

            case 'N':
                config.nameTag = line;
                break;

            case 'o':
                config.configChanged = true;
                config.oscilChanged = true;
                config.oscilsize = string2int(line);
                break;

            case 'R':
                if (not line.empty())
                {
                    config.configChanged = true;
                    config.rateChanged = true;
                    int num = (string2int(line) / 48 ) * 48;
                    if (num < 48000 || num > 192000)
                        num = 44100; // play safe
                    config.samplerate = num;
                }
                break;

            case 'S':
                config.sessionStage = _SYS_::type::StartupFirst;
                config.configChanged = true;
                config.stateFile = line;
                break;

            case 'T':
                config.remoteGuiTheme = line;
                break;

            case 'u':
                config.sessionStage = _SYS_::type::JackFirst;
                config.configChanged = true;
                config.stateFile = setExtension(line, EXTEN::state);
                break;

            case 'U':
                Config::globalJackSessionUuid = line;
                break;

            case 13:
                config.configChanged = true;
                config.engineChanged = true;
                config.midiChanged = true;
                config.audioEngine = no_audio;
                config.midiEngine  = no_midi;
                break;
        }
    }
    if (config.jackSessionUuid.size() and config.jackSessionFile.size())
    {
        config.restoreJackSession = true;
        config.configChanged = true;
    }
}



CmdOptions::CmdOptions(int argc, char **argv)
    : settings{parseCmdline(argc,argv)}
    { }

