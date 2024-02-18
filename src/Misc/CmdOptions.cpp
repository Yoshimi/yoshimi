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
#include <argp.h>

#include "Misc/FileMgrFuncs.h"
#include "Misc/CmdOptions.h"

using file::setExtension;

namespace { // constants used in the implementation
    char prog_doc[] =
        "Yoshimi " YOSHIMI_VERSION ", a derivative of ZynAddSubFX - "
        "Copyright 2002-2009 Nasca Octavian Paul and others, "
        "Copyright 2009-2011 Alan Calvert, "
        "Copyright 2012-2013 Jeremy Jongepier and others, "
        "Copyright 2014-2023 Will Godfrey and others";

    string stateText = "load saved state, defaults to '$HOME/" + EXTEN::config + "/yoshimi/yoshimi-0.state'";

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
        {"load",              'l',  "<file>",     0,  "load .xmz parameters file", 0},
        {"load-instrument",   'L',  "<file>[@part]",     0,  "load .xiz instrument file(no space)@n to part 'n'", 0},
        {"load-midilearn",    'M',  "<file>",     0,  "load .xly midi-learn file", 0},
        {"name-tag",          'N',  "<tag>",      0,  "add tag to clientname", 0},
        {"samplerate",        'R',  "<rate>",     0,  "set alsa audio sample rate", 0},
        {"oscilsize",         'o',  "<size>",     0,  "set AddSynth oscillator size", 0},
        {"state",             'S',  "<file>",     1,  "load .state complete machine setup file", 0},
        {"load-guitheme",     'T',  "<file>",     0,  "load .clr GUI theme file", 0},
        {"null",               13,  NULL,         0,  "use Null-backend without audio/midi", 0},
        #if defined(JACK_SESSION)
            {"jack-session-uuid", 'U',  "<uuid>",     0,  "jack session uuid", 0},
            {"jack-session-file", 'u',  "<file>",     0,  "load named jack session file", 0},
        #endif
        { 0, 0, 0, 0, 0, 0}
    };
}


CmdOptions::CmdOptions(int argc, char **argv, std::list<string> &allArgs, int &guin, int &cmdn) :
    gui(0),
    cmd(0)
{
    loadCmdArgs(argc, argv);
    allArgs = settings;
    guin = gui;
    cmdn = cmd;
    return;
}


static error_t parse_cmds (int key, char *arg, struct argp_state *state)
{
    CmdOptions *base = (CmdOptions*)state->input;
    if (arg && arg[0] == 0x3d)
        ++ arg;
    base->gui = base->cmd = 0;
    switch (key)
    {
        case 'N': base->settings.push_back("N:" + string(arg)); break;
        case 'l': base->settings.push_back("l:" + string(arg)); break;
        case 'L': base->settings.push_back("L:" + string(arg)); break;
        case 'M': base->settings.push_back("M:" + string(arg)); break;
        case 'T': base->settings.push_back("T:" + string(arg)); break;
        case 'A':
            if (arg)
                base->settings.push_back("A:" + string(arg));
            else
                base->settings.push_back("A:");
            break;

        case 'a':
            if (arg)
                base->settings.push_back("a:" + string(arg));
            else
                base->settings.push_back("a:");
            break;

        case 'b': base->settings.push_back("b:" + string(arg)); break;
        case 'c':
            base->settings.push_back("c:");
            base->cmd = -1;
            break;

        case 'C':
            base->settings.push_back("C:");
            base->cmd = 1;
            break;

        case 'D':
            if (arg)
                base->settings.push_back("D:" + string(arg));
            break;

        case 'i':
            base->settings.push_back("i:");
            base->gui = -1;
            break;

        case 'I':
            base->settings.push_back("I:");
            base->gui = 1;
            break;

        case 'j':
            if (arg)
                base->settings.push_back("j:" + string(arg));
            else
                base->settings.push_back("j:");
            break;

        case 'J':
            if (arg)
                base->settings.push_back("J:" + string(arg));
            else
                base->settings.push_back("J:");
            break;

        case 'k': base->settings.push_back("k:"); break;
        case 'K': base->settings.push_back("K:"); break;
        case 'o': base->settings.push_back("o:" + string(arg)); break;
        case 'R': base->settings.push_back("R:" + string(arg)); break;
        case 'S':
            if (arg)
                base->settings.push_back("S:" + string(arg));
            break;

        case 13:base->settings.push_back("@:"); break;

#if defined(JACK_SESSION)
        case 'u':
            if (arg)
                base->settings.push_back("u:" + string(arg));
            break;

        case 'U':
            if (arg)
                base->settings.push_back("U:" + string(arg));
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

void CmdOptions::loadCmdArgs(int argc, char **argv)
{
    argp_parse(&cmd_argp, argc, argv, 0, 0, this);
}
