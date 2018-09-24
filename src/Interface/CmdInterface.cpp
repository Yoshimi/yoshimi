/*
    CmdInterface.cpp

    Copyright 2015-2018, Will Godfrey & others.

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

    Modified September 2018
*/

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <cstdio>
#include <cerrno>
#include <cfloat>
#include <sys/types.h>
#include <ncurses.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <algorithm>
#include <iterator>
#include <map>
#include <list>
#include <sstream>
#include <sys/time.h>

using namespace std;

#include "Misc/SynthEngine.h"
#include "Misc/MiscFuncs.h"
#include "Misc/Bank.h"

#include "Interface/InterChange.h"
#include "Interface/CmdInterface.h"

using namespace std;

extern SynthEngine *firstSynth;
static unsigned int currentInstance = 0;

namespace LISTS {
    enum {
    all = 0,
    syseff, // not yet
    inseff, // not yet
    part,
    common,
    addsynth,
    subsynth,
    padsynth,
    addvoice,
    waveform,
    lfo,
    filter,
    envelope,
    vector,
    scale,
    load,
    save,
    list,
    config,
    mlearn
    };
}

string basics[] = {
    "?  Help",                  "show commands",
    "STop",                     "all sound off",
    "RESet [s]",                "return to start-up conditions, 'ALL' clear MIDI-learn (if 'y')",
    "EXit",                     "tidy up and close Yoshimi (if 'y')",
    "..",                       "step back one level",
    "/",                        "step back to top level",
    "end"
};

string toplist [] = {
    "ADD",                      "add paths and files",
    "  Root <s>",               "root path to list",
    "  Bank <s>",               "make new bank in current root",
    "  YOshimi [n]",            "new Yoshimi instance ID",
    "IMPort [s <n1>] <n2> <s>", "import named directory to slot n2 of current root, (or 'Root' n1)",
    "EXPort [s <n1>] <n2> <s>", "export bank at slot n2 of current root, (or 'Root' n1) to named directory",
    "REMove",                   "remove paths, files and entries",
    "  Root <n>",               "de-list root path ID",
    "  Bank [s <n1>] <n2>",     "delete bank ID n2 (and all instruments) from current root (or 'Root' n1)",
    "  YOshimi <n>",            "close instance ID",
    "  MLearn <s> [n]",         "delete midi learned 'ALL' whole list, or '@'(n) line",
    "RUN <s>",                  "Execute named command script",
    "Set/Read/MLearn",          "manage all main parameters",
    "MINimum/MAXimum/DEFault",  "find ranges",
    "  Part",                   "enter context level",
    "  VEctor",                 "enter context level",
    "  SCale",                  "enter context level",
    "  MLearn",                 "enter editor context level",
    "  COnfig",                 "enter context level",
    "  Root <n>",               "current root path to ID",
    "  Bank <n>",               "current bank to ID",
    "  SYStem effects [n]",     "enter effects context level",
    "    Type <s>",             "the effect type",
    "    PREset <n2>",          "set numbered effect preset to n2",
    "    SEnd <n2> <n3>",       "send system effect to effect n2 at volume n3",
    "  INSert effects [n1]",    "enter effects context level",
    "    Type <s>",             "the effect type",
    "    PREset <n2>",          "set numbered effect preset to n2",
    "    SEnd <s>/<n2>",        "set where (Master, Off or part number)",
    "  AVailable <n>",          "available parts (16, 32, 64)",
    "  Volume <n>",             "master volume",
    "  SHift <n>",              "master key shift semitones (0 no shift)",
    "  DEtune <n>",             "master fine detune",
    "  SOlo [s] [n]",           "channel 'solo' switcher (Row, Column, Loop, Twoway, CC, {other} off)",
    "      CC <n>",             "Incoming 'solo' CC number (type must be set first)",
    "end"
};

string configlist [] = {
    "Oscillator <n>",           "* Add/Pad size (power 2 256-16384)",
    "BUffer <n>",               "* internal size (power 2 16-4096)",
    "PAdsynth [s]",             "interpolation type (Linear, other = cubic)",
    "Virtual <n>",              "keyboard (0 = QWERTY, 1 = Dvorak, 2 = QWERTZ, 3 = AZERTY)",
    "Xml <n>",                  "compression (0-9)",
    "REports [s]",              "destination (Stdout, other = console)",
    "SAved [s]",                "Saved instrument type (Legacy {.xiz}, Yoshimi {.xiy}, Both)",
    "Expose <s>",               "Show current context level (ON, OFf, PRompt)",

    "STate [s]",                "* autoload default at start (ON, {other})",
    "Hide [s]",                 "non-fatal errors (ON, {other})",
    "Display [s]",              "GUI splash screen (ON, {other})",
    "Time [s]",                 "add to instrument load message (ON, {other})",
    "Include [s]",              "XML headers on file load(Enable {other})",
    "Keep [s]",                 "include inactive data on all file saves (ON, {other})",
    "Gui [s]",                  "* Run with GUI (ON, OFf)",
    "Cli [s]",                  "* Run with CLI (ON, OFf)",

    "MIdi <s>",                 "* connection type (Jack, Alsa)",
    "AUdio <s>",                "* connection type (Jack, Alsa)",
    "ALsa Midi <s>",            "* name of source",
    "ALsa Audio <s>",           "* name of hardware device",
    "ALsa Sample <n>",          "* rate (0 = 192000, 1 = 96000, 2 = 48000, 3 = 44100)",
    "Jack Midi <s>",            "* name of source",
    "Jack Server <s>",          "* name",
    "Jack Auto <s>",            "* connect jack on start (ON, {other})",

    "ROot [n]",                 "root CC (0 - 119, {other} off)",
    "BAnk [n]",                 "bank CC (0, 32, {other} off)",
    "PRogram [s]",              "program change (ON, {other})",
    "ACtivate [s]",             "program change activates part (ON, {other})",
    "Extended [s]",             "extended program change (ON, {other})",
    "Quiet [s]",                "ignore 'reset all controllers' (ON, {other})",
    "Nrpn [s]",                 "incoming NRPN (ON, {other})",
    "Log [s]",                  "incoming MIDI CCs (ON, {other})",
    "SHow [s]",                 "GUI MIDI learn editor (ON, {other})",
    "end"
};

string partlist [] = {
    "OFfset <n2>",              "velocity sense offset",
    "Breath <s>",               "breath control (ON, {other})",
    "POrtamento <s>",           "portamento (ON, {other})",
    "Mode <s>",                 "key mode (Poly, Mono, Legato)",
    "Note <n2>",                "note polyphony",
    "SHift <n2>",               "key shift semitones (0 no shift)",
    "EFfects [n2]",             "enter effects context level",
    "  Type <s>",               "the effect type",
    "  PREset <n3>",            "set numbered effect preset to n3",
    "  Send <n3> <n4>",         "send part to system effect n3 at volume n4",
    "KMode <s>",                "set part to kit mode (MUlti, SIngle, CRoss, OFf)",
    "  KItem <n>",              "select kit item number (1-16)",
    "    MUte <s>",             "silence this item (ON, {other})",
    "    KEffect <n>",          "select effect for this item (0-none, 1-3)",
    "  DRum <s>",               "set kit to drum mode (ON, {other})",
    "PRogram <[n2]/[s]>",       "loads instrument ID / CLear sets default",
    "NAme <s>",                 "sets the display name the part can be saved with",
    "Channel <n2>",             "MIDI channel (> 32 disables, > 16 note off only)",
    "Destination <s2>",         "jack audio destination (Main, Part, Both)",
    "ADDsynth ...",             "Enter AddSynth context",
    "SUBsynth ...",             "Enter SubSynth context",
    "PADsynth ...",             "Enter PadSynth context",
    "? COMmon",                 "List controls common to most part contexts",
    "end"
};

string commonlist [] = {
    "ON @",                     "enables the part/kit item/engine/insert etc,",
    "OFf @",                    "disables as above",
    "Volume <n> @",             "volume",
    "Pan <n2> @",               "panning",
    "VElocity <n> @",           "velocity sensing sensitivity",
    "MIn <n> +",                "minimum MIDI note value",
    "MAx <n> +",                "maximum MIDI note value",
    "DETune Fine <n> *",        "fine frequency",
    "DETune Coarse <n> *",      "coarse stepped frequency",
    "DETune Type <n> *",        "type of coarse stepping",
    "OCTave <n> *",             "shift octaves up or down",
    "FIXed <s> *-add",          "set base frequency to 440Hz (ON, {other})",
    "EQUal <n> *-add",          "equal temper variation",
    "BENd Adjust <n>  *-add",   "pitch bend range",
    "BENd Offset <n>  *-add",   "pitch bend shift",
    "STEreo <s> *-voice",       "ON, {other}",
    "DEPop <n> &",              "initial attack slope",
    "PUnch Power <n> &",        "attack boost amplitude",
    "PUnch Duration <n> &",     "attack boost time",
    "PUnch Stretch <n> &",      "attack boost extend",
    "PUnch Velocity <n> &",     "attack boost velocity sensitivity",
    "OVertone Position <s> #",  "relationship to fundamental",
    "","HArmonic,SIne,POwer,SHift,UShift,LShift,UPower,LPower",
    "OVertone First <n> #",     "degree of first parameter",
    "OVertone Second <n> #",    "degree of second parameter",
    "OVertone Harmonic <n> #",  "amount harmonics are forced",
    "LFO ... *-sub",            "enter LFO insert context",
    "FILter ... *",             "enter Filter insert context",
    "ENVelope ... *",           "enter Envelope insert context",
    "","",
    "@",                        "exists in all part contexts",
    "+",                        "part and kit mode controls",
    "*",                        "Add, Sub, Pad and AddVoice controls",
    "*-add",                    "not AddSynth",
    "*-sub",                    "not SubSynth",
    "*-voice",                  "not AddVoice",
    "&",                        "AddSynth & PadSynth only",
    "#",                        "SubSynth & PadSynth only",
    "end"
};

string addsynthlist [] = {
    "VOice ...",                "enter Addsynth voice context",
    "end"
};

string addvoicelist [] = {
    "WAveform ...",              "enter the oscillator waveform context",
    "end"
};

string subsynthlist [] = {
    "HArmonic <n1> Amp <n2>",   "set harmonic {n1} to {n2} intensity",
    "HArmonic <n1> Band <n2>",  "set harmonic {n1} to {n2} width",
    "HArmonic Stages <n>",      "number of stages",
    "HArmonic Mag <n>",         "harmonics filtering type",
    "HArmonic Position <n>",    "start position",
    "BAnd Width <n>",           "common bandwidth",
    "BAnd Scale <n>",           "bandwidth slope v frequency",
    "end"
};

string padsynthlist [] = {
    "APply",                    "puts latest changes into the wavetable",
    "WAveform ...",             "enter the oscillator waveform context",
    "end"
};

string waveformlist [] = {
    "HArmonic <n1> Amp <n2>",   "set harmonic {n1} to {n2} intensity",
    "HArmonic <n1> Phase <n2>", "set harmonic {n1} to {n2} phase",
    "CLear",                    "clear harmonic settings",
    "SHape <s>",                "set the shape of the basic waveform",
    "","SIne,TRiangle,PUlse,SAw,POwer,GAuss,DIode,ABsine,PSine",
    "","SSine,CHIrp,ASine,CHEbyshev,SQuare,SPike,Circle",
    "APply",                    "Fix settings (only for PadSynth)",
    "end"
};

string LFOlist [] = {
    "AMplitude ~",              "amplitude type",
    "FRequency ~",              "frequency type",
    "FIlter ~",                 "filter type",
    "~  Rate <n>",              "frequency",
    "~  Start <n>",             "start position in cycle",
    "~  Delay <n>",             "time before effect",
    "~  Expand <n>",            "rate / note pitch",
    "~  Continuous <s>",        "(ON, {other})",
    "~  Type <s>",              "LFO oscillator shape",
    "   ",                      "  SIne",
    "   ",                      "  Triangle",
    "   ",                      "  SQuare",
    "   ",                      "  RUp (ramp up)",
    "   ",                      "  RDown (ramp down)",
    "   ",                      "  E1dn",
    "   ",                      "  E2dn",
    "~  AR <n>",                "amplitude randomness",
    "~  FR <n>",                "frequency randomness",
    "e.g. S FI T RU",           "set filter type ramp up",
    "end"
};

string filterlist [] = {
    "CEnter <n>",           "center frequency",
    "Q <n>",                "Q factor",
    "Velocity <n>",         "velocity sensitivity",
    "SLope <n>",            "velocity curve",
    "Gain <n>",             "overall amplitude",
    "TRacking <n>",         "frequency tracking",
    "Range <s>",            "extended tracking (ON, {other})",
    "CAtegory <s>",         "Analog, Formant, State variable",
    "STages <n>",           "filter stages (1 to 5)",
    "TYpe <s>",             "category dependent - not formant",
    "-  analog","",
    "  l1",                 "one stage low pass",
    "  h1",                 "one stage high pass",
    "  l2",                 "two stage low pass",
    "  h2",                 "two stage high pass",
    "  band",               "two stage band pass",
    "  stop",               "two stage band stop",
    "  peak",               "two stage peak",
    "  lshelf",             "two stage low shelf",
    "  hshelf",             "two stage high shelf",
    "-  state variable","",
    "  low",                "low pass",
    "  high",               "high pass",
    "  band",               "band pass",
    "  stop",               "band stop",
    "","",
    "formant editor","",
    "Invert <s>",           "invert effect of LFOs, envelopes (ON, OFf)",
    "FCenter <n>",          "center frequency of sequence",
    "FRange <n>",           "octave range of formants",
    "Expand <n>",           "stretch overal sequence time",
    "Lucidity <n>",         "clarity of vowels",
    "Morph <n>",            "speed of change between formants",
    "SIze <n>",             "number of vowels in sequence",
    "COunt <n>",            "number of formants in vowels",
    "VOwel <n>",            "vowel being processed",
    "Point <n1> <n2>",      "vowel n1 at sequence position n2",
    "Item",                 "formant being processed",
    "per formant","",
    "  FFrequency <n>",     "Center of formant frequency",
    "  FQ <n>",             "bandwidth of formant",
    "  FGain <n>",          "amplitude of formant",
    "end"
};

string envelopelist [] = {
    "types","",
    "AMplitude",              "amplitude type",
    "FRequency",              "frequency type",
    "FIlter",                 "filter type",
    "BAndwidth",              "bandwidth type (SubSynth only)",
    "","",
    "controls","",
    "Expand <n>",            "envelope time on lower notes",
    "Force <s>",             "force release (ON, {other})",
    "Linear <s>",            "linear slopes (ON, {other})",
    "FMode <s>",             "set as freemode (ON, {other})",
    "","",
    "fixed","",
    "Attack Level <n>",      "initial attack level",
    "Attack Time <n>",       "time before decay point",
    "Decay Level <n>",       "initial decay level",
    "Decay Time <n>",        "time before sustain point",
    "Sustain <n>",           "sustain level",
    "Release Time <n>",      "time to actual release",
    "Release Level <n>",     "level at envelope end",

    "e.g. S FR D T 40",      "set frequency decay time 40",
    "Note:",                 "some envelopes have limited controls",
    "","",
    "freemode","",
    "Points",                "Number of defined points (read only)",
    "Sustain <n>",           "point number where sustain starts",
    "Insert <n1> <n2> <n3>", "insert point at 'n1' with X increment 'n1', Y value 'n2'",
    "Delete <n>",            "remove point 'n'",
    "Change <n1> <n2> <n3>", "change point 'n1' to X increment 'n1', Y value 'n2'",
    "end"
};


string learnlist [] = {
    "MUte <s>",                 "completely ignore this line (ON, {other})",
    "SEven",                    "set incoming NRPNs as 7 bit (ON, {other})",
    "CC <n2>",                  "set incoming controller value",
    "CHan <n2>",                "set incoming channel number",
    "MIn <n2>",                 "set minimm percentage",
    "MAx <n2>",                 "set maximum percentage",
    "LImit <s>",                "limit instead of compress (ON, {other})",
    "BLock <s>",                "inhibit others on this CC/Chan pair (ON, {other})",
    "end"
};

string vectlist [] = {
    "[X/Y] CC <n2>",            "CC n2 is used for X or Y axis sweep",
    "[X/Y] Features <n2> [s]",  "sets X or Y features 1-4 (ON, Reverse, {other})",
    "[X] PRogram <l/r> <n2>",   "X program change ID for LEFT or RIGHT part",
    "[Y] PRogram <d/u> <n2>",   "Y program change ID for DOWN or UP part",
    "[X/Y] Control <n2> <n3>",  "sets n3 CC to use for X or Y feature n2 (2-4)",
    "Off",                      "disable vector for this channel",
    "Name <s>",                 "Text name for this complete vector",
    "end"
};

string scalelist [] = {
    "FRequency <n>",            "'A' note actual frequency",
    "NOte <n>",                 "'A' note number",
    "Invert [s]",               "invert entire scale (ON, {other})",
    "CEnter <n>",               "note number of key center",
    "SHift <n>",                "shift entire scale up or down",
    "SCale [s]",                "activate microtonal scale (ON, {other})",
    "MApping [s]",              "activate keyboard mapping (ON, {other})",
    "FIrst <n>",                "first note number to map",
    "MIddle <n>",               "middle note number to map",
    "Last <n>",                 "last note number to map",
    "Tuning <s> [s2]",          "CSV tuning values (n1.n1 or n1/n1 ,  n2.n2 or n2/n2 , etc.)",
    "",                         "s2 = 'IMPort' from named file",
    "Keymap <s> [s2]",          "CSV keymap (n1, n2, n3, etc.)",
    "",                         "s2 = 'IMPort' from named file",
    "NAme <s>",                 "internal name for this scale",
    "DEscription <s>",          "description of this scale",
    "CLEar",                    "clear all settings and revert to standard scale",
    "end"
};

string loadlist [] = {
    "Instrument <s>",           "instrument to current part from named file",
    "SCale <s>",                "scale settings from named file",
    "VEctor [n] <s>",           "vector to channel n (or saved) from named file",
    "Patchset <s>",             "complete set of instruments from named file",
    "MLearn <s>",               "midi learned list from named file",
    "STate <s>",                "all system settings and patch sets from named file",
    "end"
};

string savelist [] = {
    "Instrument <s>",           "current part to named file",
    "SCale <s>",                "current scale settings to named file",
    "VEctor <n> <s>",           "vector on channel n to named file",
    "Patchset <s>",             "complete set of instruments to named file",
    "MLearn <s>",               "midi learned list to named file",
    "STate <s>",                "all system settings and patch sets to named file",
    "Config",                   "current configuration",
    "end",
};

string listlist [] = {
    "Roots",                    "all available root paths",
    "Banks [n]",                "banks in root ID or current",
    "Instruments [n]",          "instruments in bank ID or current",
    "Parts",                    "parts with instruments installed",
    "Vectors",                  "settings for all enabled vectors",
    "Tuning",                   "microtonal scale tunings",
    "Keymap",                   "microtonal scale keyboard map",
    "Config",                   "current configuration",
    "MLearn [s <n>]",           "midi learned controls ('@' n for full details on one line)",
    "History [s]",              "recent files (Patchsets, SCales, STates, Vectors, MLearn)",
    "Effects [s]",              "effect types ('all' include preset numbers and names)",
    "PREsets",                  "all the presets for the currently selected effect",
    "end"
};

string replies [] = {
    "OK",
    "Done",
    "Value?",
    "Name?",
    "Which Operation?",
    " what?",
    "Out of range",
    "Too low",
    "Too high",
    "Unrecognised",
    "Parameter?",
    "Not at this level",
    "Not available",
    "Unable to complete"
};

string fx_list [] = {
    "OFf",
    "REverb",
    "ECho",
    "CHorus",
    "PHaser",
    "ALienwah",
    "DIstortion",
    "EQ",
    "DYnfilter"
};


string fx_presets [] = {
    "1, off",
    "13, cathedral 1, cathedral 2, cathedral 3, hall 1, hall 2, room 1, room 2, basement, tunnel, echoed 1, echoed 2, very long 1, very long 2",
    "8, echo 1, echo 2, simple echo, canyon, panning echo 1, panning echo 2, panning echo 3, feedback echo",
    "10, chorus 1, chorus 2, chorus 3, celeste 1, celeste 2, flange 1, flange 2, flange 3, flange 4, flange 5",
    "12, phaser 1, phaser 2, phaser 3, phaser 4, phaser 5, phaser 6, aphaser 1, aphaser 2, aphaser 3, aphaser 4, aphaser 5, aphaser 6",
    "4, alienwah 1, alienwah 2, alienwah 3, alienwah 4 ",
    "6, overdrive 1, overdrive 2, exciter 1, exciter 2, guitar amp, quantisize",
    "1, not available",
    "4, wahwah, autowah, vocal morph 1, vocal morph 2"
};


void CmdInterface::defaults()
{
    context = LEVEL::Top;
    chan = 0;
    axis = 0;
    mline = 0;
    npart = 0;
    nFX = 0;
    nFXtype = 0;
    nFXpreset = 0;
    kitMode = 0;
    kitNumber = 0;
    voiceNumber = 0;
    insertType = 0;
    filterVowelNumber = 0;
    filterFormantNumber = 0;
}


bool CmdInterface::query(string text, bool priority)
{
    char *line = NULL;
    string suffix;
    char result;
    char test;

    priority = !priority; // so calls make more sense

    if (priority)
    {
        suffix = " N/y? ";
        test = 'n';
    }
    else
    {
        suffix = " Y/n? ";
        test = 'y';
    }
    result = test;
    text += suffix;
    line = readline(text.c_str());
    if (line)
    {
        if (line[0] != 0)
            result = line[0];
        free(line);
        line = NULL;
    }
    return (((result | 32) == test) ^ priority);
}


void CmdInterface::helpLoop(list<string>& msg, string *commands, int indent)
{
    int word = 0;
    int spaces = 30 - indent;
    string left;
    string right;
    string dent;
    string blanks;

    while (commands[word] != "end")
    {
        left = commands[word];
        right = commands[word + 1];
        if (right > "")
            left = left +(blanks.assign(spaces - left.length(), ' ') + right);
        msg.push_back(dent.assign(indent, ' ') + left);
        word += 2;
    }
}


bool CmdInterface::helpList(unsigned int local)
{
    if (!matchnMove(1, point, "help") && !matchnMove(1, point, "?"))
        return todo_msg;

    int listnum = LISTS::all;

    if (point[0] != 0)
    { // 1 & 2 reserved for syseff & inseff
        if (matchnMove(1, point, "part"))
            listnum = LISTS::part;
        else if (matchnMove(3, point, "common"))
            listnum = LISTS::common;
        else if (matchnMove(3, point, "addsynth"))
            listnum = LISTS::addsynth;
        else if (matchnMove(3, point, "subsynth"))
            listnum = LISTS::subsynth;
        else if (matchnMove(3, point, "padsynth"))
            listnum = LISTS::padsynth;
        else if (matchnMove(3, point, "voice"))
            listnum = LISTS::addvoice;
        else if (matchnMove(3, point, "waveform"))
            listnum = LISTS::waveform;
        else if (matchnMove(3, point, "lfo"))
            listnum = LISTS::lfo;
        else if (matchnMove(3, point, "filter"))
            listnum = LISTS::filter;
        else if (matchnMove(3, point, "envelope"))
            listnum = LISTS::envelope;
        else if (matchnMove(1, point, "vector"))
            listnum = LISTS::vector;
        else if (matchnMove(1, point, "scale"))
            listnum = LISTS::scale;
        else if (matchnMove(1, point, "load"))
            listnum = LISTS::load;
        else if (matchnMove(1, point, "save"))
            listnum = LISTS::save;
        else if (matchnMove(1, point, "list"))
            listnum = LISTS::list;
        else if (matchnMove(1, point, "config"))
            listnum = LISTS::config;
        else if (matchnMove(1, point, "mlearn"))
            listnum = LISTS::mlearn;
    }
    else
    {
        if (bitTest(local, LEVEL::Envelope))
            listnum = LISTS::envelope;
        else if (bitTest(local, LEVEL::LFO))
            listnum = LISTS::lfo;
        else if (bitTest(local, LEVEL::Filter))
            listnum = LISTS::filter;
        else if (bitTest(local, LEVEL::Oscillator))
            listnum = LISTS::waveform;
        else if (bitTest(local, LEVEL::AddVoice))
            listnum = LISTS::addvoice;
        else if (bitTest(local, LEVEL::AddSynth))
            listnum = LISTS::addsynth;
        else if (bitTest(local, LEVEL::SubSynth))
            listnum = LISTS::subsynth;
        else if (bitTest(local, LEVEL::PadSynth))
            listnum = LISTS::padsynth;
        else if (bitTest(local, LEVEL::Part))
            listnum = LISTS::part;
        else if (bitTest(local, LEVEL::Vector))
            listnum = LISTS::vector;
        else if (bitTest(local, LEVEL::Scale))
            listnum = LISTS::scale;
        else if (bitTest(local, LEVEL::Config))
            listnum = LISTS::config;
        else if (bitTest(local, LEVEL::Learn))
            listnum = LISTS::mlearn;
    }

    list<string>msg;
    msg.push_back("Commands:");
    helpLoop(msg, basics, 2);
    switch(listnum)
    {
        case 0:
            msg.push_back(" ");
            msg.push_back("  Part [n1]   ...             - part operations");
            msg.push_back("  VEctor [n1] ...             - vector operations");
            msg.push_back("  SCale       ...             - scale (microtonal) operations");
            msg.push_back("  MLearn [n1] ...             - MIDI learn operations");
            msg.push_back("  COnfig      ...             - configuration settings");
            msg.push_back("  LIst        ...             - various available parameters");
            msg.push_back("  LOad        ...             - load various files");
            msg.push_back("  SAve        ...             - save various files");

            msg.push_back(" ");
            break;
        case LISTS::part:
            msg.push_back("Part: [n1] = part number");
            helpLoop(msg, partlist, 2);
            break;
        case LISTS::common:
            msg.push_back("Part Common:");
            helpLoop(msg, commonlist, 2);
            break;
        case LISTS::addsynth:
            msg.push_back("Part AddSynth:");
            helpLoop(msg, addsynthlist, 2);
            break;
        case LISTS::subsynth:
            msg.push_back("Part SubSynth:");
            helpLoop(msg, subsynthlist, 2);
            break;
        case LISTS::padsynth:
            msg.push_back("Part PadSynth:");
            helpLoop(msg, padsynthlist, 2);
            break;
        case LISTS::addvoice:
            msg.push_back("Part AddVoice:");
            helpLoop(msg, addvoicelist, 2);
            break;
        case LISTS::waveform:
            msg.push_back("Part Waveform:");
            helpLoop(msg, waveformlist, 2);
            break;

        case LISTS::lfo:
            msg.push_back("Engine LFOs:");
            helpLoop(msg, LFOlist, 2);
            break;
        case LISTS::filter:
            msg.push_back("Engine Filters:");
            helpLoop(msg, filterlist, 2);
            break;
        case LISTS::envelope:
            msg.push_back("Engine Envelopes:");
            helpLoop(msg, envelopelist, 2);
            break;

        case LISTS::vector:
            msg.push_back("Vector: [n1] = base channel:");
            helpLoop(msg, vectlist, 2);
            break;
        case LISTS::scale:
            msg.push_back("Scale:");
            helpLoop(msg, scalelist, 2);
            break;
        case LISTS::load:
            msg.push_back("Load:");
            helpLoop(msg, loadlist, 2);
            break;
        case LISTS::save:
            msg.push_back("Save:");
            helpLoop(msg, savelist, 2);
            break;
        case LISTS::list:
            msg.push_back("List:");
            helpLoop(msg, listlist, 2);
            break;
        case LISTS::config:
            msg.push_back("Config:");
            helpLoop(msg, configlist, 2);
            msg.push_back("'*' entries need to be saved and Yoshimi restarted to activate");
            break;
        case LISTS::mlearn:
            msg.push_back("Mlearn: [n1] = line number");
            helpLoop(msg, learnlist, 2);
            break;
    }

    if (listnum == 0)
    {
        helpLoop(msg, toplist, 2);
        msg.push_back("'...' help sub-menu");
    }

    if (synth->getRuntime().toConsole)
        // we need this in case someone is working headless
        cout << "\nSet CONfig REPorts [s] - set report destination (gui/stderr)\n\n";

    synth->cliOutput(msg, LINES);
    return exit_msg;
}


void CmdInterface::historyList(int listnum)
{
    list<string>msg;
    int start = 1;
    int end = 6;
    bool found = false;

    if (listnum != 0)
    {
        start = listnum;
        end = listnum;
    }
    for (int type = start; type <= end; ++type)
    {
        vector<string> listType = *synth->getHistory(type);
        if (listType.size() > 0)
        {
            msg.push_back(" ");
            switch (type)
            {
                case XML_INSTRUMENT:
                    msg.push_back("Recent Instruments:");
                    break;
                case XML_PARAMETERS:
                    msg.push_back("Recent Patch Sets:");
                    break;
                case XML_MICROTONAL:
                    msg.push_back("Recent Scales:");
                    break;
                case XML_STATE:
                    msg.push_back("Recent States:");
                    break;
                case XML_VECTOR:
                    msg.push_back("Recent Vectors:");
                    break;
                case XML_MIDILEARN:
                    msg.push_back("Recent MIDI learned:");
                    break;
            }
            int itemNo = 0;
            for (vector<string>::iterator it = listType.begin(); it != listType.end(); ++it, ++ itemNo)
                msg.push_back(to_string(itemNo + 1) + "  " + *it);
            found = true;
        }
    }
    if (!found)
        msg.push_back("\nNo Saved History");

    synth->cliOutput(msg, LINES);
}


string CmdInterface::historySelect(int listnum, int selection)
{
    vector<string> listType = *synth->getHistory(listnum);
    if (listType.size()== 0)
    {
        synth->getRuntime().Log("No saved entries");
        return "";
    }
    else
    {
        vector<string>::iterator it = listType.begin();
        int itemNo = 0;
        while (it != listType.end() && itemNo != selection)
        {
            ++ it;
            ++ itemNo;
        }
        if (it != listType.end())
            return *it;
    }
    synth->getRuntime().Log("No such entry");
    return "";
}


int CmdInterface::effectsList(bool presets)
{
    list<string>msg;

    size_t presetsPos;
    size_t presetsLast;
    int presetsCount;
    string blanks;
    string left;
    bool all;

    if (bitTest(context, LEVEL::AllFX) && presets == true)
    {
         synth->getRuntime().Log("Type " + fx_list[nFXtype] + "\nPresets -" + fx_presets[nFXtype].substr(fx_presets[nFXtype].find(',') + 1));
         return done_msg;
    }
    else if (presets)
    {
        synth->getRuntime().Log("No effect selected");
        return done_msg;
    }
    else
        all = matchnMove(1, point, "all");
    if (!all)
        msg.push_back("  effect     presets");
    for (int i = 0; i < 9; ++ i)
    {
        presetsPos = 1;
        presetsLast = fx_presets [i].find(',') + 1; // skip over count
        presetsCount = 0;
        if (all)
        {
            msg.push_back("  " + fx_list[i]);
            msg.push_back("    presets");
            while (presetsPos != string::npos)
            {
                presetsPos = fx_presets [i].find(',', presetsLast);
                msg.push_back("      " + asString(presetsCount + 1) + " =" + fx_presets [i].substr(presetsLast, presetsPos - presetsLast));
                presetsLast = presetsPos + 1;
                ++ presetsCount;
            }
        }
        else
        {
            left = fx_list[i];
            msg.push_back("    " + left + blanks.assign(12 - left.length(), ' ') + fx_presets [i].substr(0, presetsLast - 1));
        }
    }

    synth->cliOutput(msg, LINES);
    return done_msg;
}


int CmdInterface::effects(unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    int nFXavail;
    int par = nFX;
    int value;
    string dest = "";
    bool flag;

    if (bitTest(context, LEVEL::Part))
    {
        nFXavail = NUM_PART_EFX;
        nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
    }
    else if (bitTest(context, LEVEL::InsFX))
    {
        nFXavail = NUM_INS_EFX;
        nFXtype = synth->insefx[nFX]->geteffect();
    }
    else
    {
        nFXavail = NUM_SYS_EFX;
        nFXtype = synth->sysefx[nFX]->geteffect();
    }

    if (lineEnd(controlType))
        return done_msg;

    value = string2int(point);
    if (value > 0)
    {
        value -= 1;
        point = skipChars(point);
        if (value >= nFXavail)
            return range_msg;

        if (value != nFX)
        { // calls to update GUI
            nFX = value;
            if (bitTest(context, LEVEL::Part))
            {
                nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
                sendDirect(nFXtype, TOPLEVEL::type::Write, PART::control::effectType, npart, UNUSED, nFX, TOPLEVEL::insert::partEffectSelect);
            }
            else if (bitTest(context, LEVEL::InsFX))
            {
                nFXtype = synth->insefx[nFX]->geteffect();
                sendDirect(nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::insertEffects, UNUSED, nFX);
            }
            else
            {
                nFXtype = synth->sysefx[nFX]->geteffect();
                sendDirect(nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::systemEffects, UNUSED, nFX);
            }
        }
        if (lineEnd(controlType))
        {
            Runtime.Log("efx number set to " + asString(nFX + 1));
            return done_msg;
        }
    }

    if (matchnMove(1, point, "type"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            Runtime.Log("Current efx type is " + fx_list[nFXtype]);
            return done_msg;
        }
        flag = true;
        for (int i = 0; i < 9; ++ i)
        {
            //Runtime.Log("command " + (string) point + "  list " + fx_list[i]);
            if (matchnMove(2, point, fx_list[i].c_str()))
            {
                nFXtype = i;
                flag = false;
                break;
            }
        }
        if (flag)
            return unrecognised_msg;
        nFXpreset = 0; // always set this on type change
        Runtime.Log("efx type set to " + fx_list[nFXtype]);
        //Runtime.Log("Presets -" + fx_presets[nFXtype].substr(fx_presets[nFXtype].find(',') + 1));
        if (bitTest(context, LEVEL::Part))
            sendDirect(nFXtype, TOPLEVEL::type::Write, PART::control::effectType, npart, UNUSED, nFX);
        else if (bitTest(context, LEVEL::InsFX))
            sendDirect(nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::insertEffects, UNUSED, nFX);
        else
            sendDirect(nFXtype, TOPLEVEL::type::Write, EFFECT::sysIns::effectType, TOPLEVEL::section::systemEffects, UNUSED, nFX);
        return done_msg;
    }

    if (matchnMove(2, point, "send"))
    {
        if (lineEnd(controlType))
            return parameter_msg;

        if (bitTest(context, LEVEL::InsFX))
        {
            if (matchnMove(1, point, "master"))
            {
                value = -2;
                dest = "master";
            }
            else if (matchnMove(1, point, "off"))
            {
                value = -1;
                dest = "off";
            }
            else
            {
                value = string2int(point) - 1;
                if (value >= Runtime.NumAvailableParts || value < 0)
                    return range_msg;
                dest = "part " + asString(value + 1);
                // done this way in case there is rubbish on the end
            }
        }
        else
        {
            par = string2int(point) - 1;
            point = skipChars(point);
            if (lineEnd(controlType))
                return value_msg;
            value = string2int127(point);
        }

        int control;
        int partno;
        int engine = nFX;
        int insert = UNUSED;

        if (bitTest(context, LEVEL::Part))
        {
            partno = npart;
            control = 40 + par;
            engine = UNUSED;

            dest = "part " + asString(npart + 1) + " efx sent to system "
                 + asString(par + 1) + " at " + asString(value);
        }
        else if (bitTest(context, LEVEL::InsFX))
        {
            partno = TOPLEVEL::section::insertEffects;
            control = 2;
            dest = "insert efx " + asString(nFX + 1) + " sent to " + dest;
        }
        else
        {
            if (par <= nFX)
                return range_msg;
            partno = TOPLEVEL::section::systemEffects;
            control = par;
            engine = nFX;
            insert = TOPLEVEL::insert::systemEffectSend;
            dest = "system efx " + asString(nFX + 1) + " sent to "
                 + asString(par + 1) + " at " + asString(value);
        }
        sendDirect(value, TOPLEVEL::type::Write,control, partno, UNUSED, engine, insert);
        Runtime.Log(dest);
        return done_msg;
    }

    if (matchnMove(3, point, "preset"))
    {
        /*
         * Using constant strings and bedding the number into the list
         * of presets provides a very simple way to keep track of a
         * moving target with minimal code and data space.
         * However, all of this should really be in src/Effects
         * not here *and* in the gui code!
         */
        int partno;
        par = string2int(fx_presets [nFXtype].substr(0, fx_presets [nFXtype].find(',')));
        if (par == 1)
            return available_msg;
        value = string2int127(point) - 1;
        if (value >= par || value < 0)
            return range_msg;
        nFXpreset = value;
        if (bitTest(context, LEVEL::Part))
        {
            partno = npart;
            dest = "part " + asString(npart + 1);
        }
        else if (bitTest(context, LEVEL::InsFX))
        {
            partno = TOPLEVEL::section::insertEffects;
            dest = "insert";
        }
        else
        {
            partno = TOPLEVEL::section::systemEffects;
            dest = "system";
        }
        sendDirect(nFXpreset, TOPLEVEL::type::Write,16, partno, nFXtype + EFFECT::type::none, nFX); // TODO shouldn't need this offset
        Runtime.Log(dest + " efx preset set to number " + asString(value + 1));
    }
    return done_msg;
}


int CmdInterface::partCommonControls(unsigned char controlType)
{
    int cmd = -1;
    int engine = contextToEngines();
    int insert = UNUSED;
    int kit = UNUSED;

    if (engine == PART::engine::addVoice1)
        engine += voiceNumber; // voice numbers are 0 to 7

    if (kitMode)
        kit = kitNumber;

    if (bitFindHigh(context) != LEVEL::Part)
    {
        // these are all common to Add, Sub, Pad, Voice
        int value = 0;
        if (matchnMove(3, point, "detune"))
        {
            if (matchnMove(1, point, "fine"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = ADDSYNTH::control::detuneFrequency;
            }
            else if (matchnMove(1, point, "coarse"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = ADDSYNTH::control::coarseDetune;
            }
            else if (matchnMove(1, point, "type"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = ADDSYNTH::control::detuneType;
            }
        }
        else if (matchnMove(3, point, "octave"))
        {
            if (lineEnd(controlType))
                return value_msg;
            value = string2int(point);
            cmd = ADDSYNTH::control::octave;
        }

        if (cmd == -1 && matchnMove(3, point, "lfo"))
        {
            if(engine == PART::engine::subSynth)
                return available_msg;
            bitSet(context, LEVEL::LFO);
            return LFOselect(controlType);
        }
        if (cmd == -1 && matchnMove(3, point, "filter"))
        {
            bitSet(context, LEVEL::Filter);
            return filterSelect(controlType);
        }
        if (cmd == -1 && matchnMove(3, point, "envelope"))
        {
            bitSet(context, LEVEL::Envelope);
            return envelopeSelect(controlType);
        }

        // not AddVoice
        if (cmd == -1 && (matchnMove(3, point, "stereo") && bitFindHigh(context) != LEVEL::AddVoice))
        {
            cmd = ADDSYNTH::control::stereo;
            value = (toggle() == 1);
        }
        // not AddSynth
        if (cmd == -1 && (bitFindHigh(context) != LEVEL::AddSynth))
        {
            int tmp_cmd = -1;
            if (matchnMove(3, point, "fixed"))
            {
                value = (toggle() == 1);
                cmd = SUBSYNTH::control::baseFrequencyAs440Hz;
            }
            else if (matchnMove(3, point, "equal"))
                tmp_cmd = SUBSYNTH::control::equalTemperVariation;
            else if (matchnMove(3, point, "bend"))
            {
                if (matchnMove(1, point, "adjust"))
                    tmp_cmd = SUBSYNTH::control::pitchBendAdjustment;
                else if (matchnMove(1, point, "offset"))
                    tmp_cmd = SUBSYNTH::control::pitchBendOffset;
            }
            if (tmp_cmd > -1)
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = tmp_cmd;
            }
        }
        // Add/Pad only
        if (cmd == -1 && (bitFindHigh(context) == LEVEL::AddSynth || bitFindHigh(context) == LEVEL::PadSynth))
        {
            int tmp_cmd = -1;
            if (matchnMove(3, point, "depop"))
                tmp_cmd = ADDSYNTH::control::dePop;
            else if (matchnMove(2, point, "punch"))
            {
                if (matchnMove(1, point, "power"))
                    tmp_cmd = ADDSYNTH::control::punchStrength;
                else if (matchnMove(1, point, "duration"))
                    tmp_cmd = ADDSYNTH::control::punchDuration;
                else if (matchnMove(1, point, "stretch"))
                    tmp_cmd = ADDSYNTH::control::punchStretch;
                else if (matchnMove(1, point, "velocity"))
                    tmp_cmd = ADDSYNTH::control::punchVelocity;
            }
            if (tmp_cmd > -1)
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = tmp_cmd;
            }
        }
        // Sub/Pad only
        if (cmd == -1 && (bitFindHigh(context) == LEVEL::SubSynth || bitFindHigh(context) == LEVEL::PadSynth))
        {
            value = -1;
            if (matchnMove(2, point, "overtone"))
            {
                if (matchnMove(1, point, "Position"))
                {
                    if (matchnMove(2, point, "harmonic"))
                        value = 0;
                    else if(matchnMove(2, point, "usine"))
                        value = 1;
                    else if(matchnMove(2, point, "lsine"))
                        value = 2;
                    else if(matchnMove(2, point, "upower"))
                        value = 3;
                    else if(matchnMove(2, point, "lpower"))
                        value = 4;
                    else if(matchnMove(2, point, "sine"))
                        value = 5;
                    else if(matchnMove(2, point, "power"))
                        value = 6;
                    else if(matchnMove(2, point, "shift"))
                        value = 6;
                    else
                        return range_msg;
                    cmd = SUBSYNTH::control::overtonePosition;
                }
                else
                {
                    if (matchnMove(1, point, "First"))
                        cmd = SUBSYNTH::control::overtoneParameter1;
                    else if (matchnMove(1, point, "Second"))
                        cmd = SUBSYNTH::control::overtoneParameter2;
                    else if (matchnMove(1, point, "Harmonic"))
                        cmd = SUBSYNTH::control::overtoneForceHarmonics;
                    if (cmd > -1)
                    {
                        if (lineEnd(controlType))
                            return value_msg;
                        value = string2int(point);
                    }
                }
            }
        }

        if (cmd > -1)
        {
            sendNormal(value, controlType, cmd, npart, kitNumber, engine);
            return done_msg;
        }
    }

    int value = toggle();
    if (value >= 0)
    {
        if (kit == 0 && bitFindHigh(context) == LEVEL::Part)
        {
            synth->getRuntime().Log("Kit item 1 always on.");
            return done_msg;
        }
        else
            cmd = PART::control::enable;
    }

    if (cmd == -1 && bitFindHigh(context) == LEVEL::Part)
    { // the following can only be done at part/kit level
        if (matchnMove(2, point, "min"))
        {
            if(controlType == TOPLEVEL::type::Write)
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                if (value > synth->part[npart]->Pmaxkey)
                    return high_msg;
            }
            cmd = PART::control::minNote;
        }
        else if (matchnMove(2, point, "max"))
        {
            if(controlType == TOPLEVEL::type::Write)
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                if (value < synth->part[npart]->Pminkey)
                    return low_msg;
            }
            cmd = PART::control::maxNote;
        }
    }
    if (cmd != -1)
    {
        if (kitMode)
            insert = TOPLEVEL::insert::kitGroup;
        //cout << ">> kit cmd " << int(cmd) << "  part " << int(npart) << "  kit " << int(kitNumber) << "  engine " << int(engine) << "  insert " << int(insert) << endl;
        sendNormal(value, controlType, cmd, npart, kit, engine, insert);
        return done_msg;
    }

    if (matchnMove(1, point, "volume"))
        cmd = PART::control::volume;
    else if(matchnMove(1, point, "pan"))
        cmd = PART::control::panning;
    else if (matchnMove(2, point, "velocity"))
        cmd = PART::control::velocitySense;

    if (cmd == -1)
        return todo_msg;
    if (lineEnd(controlType))
        return value_msg;

    if (bitFindHigh(context) == LEVEL::Part)
        kit = UNUSED;
    else
        kit = kitNumber;
    //cout << ">> base cmd " << int(cmd) << "  part " << int(npart) << "  kit " << int(kit) << "  engine " << int(engine) << "  insert " << int(insert) << endl;

    return sendNormal(string2float(point), controlType, cmd, npart, kit, engine);
}


int CmdInterface::LFOselect(unsigned char controlType)
{
    int cmd = -1;
    float value = -1;
    int group = -1;
    if (lineEnd(controlType))
        return done_msg;

    int engine = contextToEngines();
    if (engine == PART::engine::addVoice1)
        engine += voiceNumber;

    if (matchnMove(2, point, "amplitude"))
        group = TOPLEVEL::insertType::amplitude;
    else if (matchnMove(2, point, "frequency"))
        group = TOPLEVEL::insertType::frequency;
    else if (matchnMove(2, point, "filter"))
        group = TOPLEVEL::insertType::filter;
    if (group > -1)
        insertType = group;
    else
        group = insertType;
    switch (group)
    {
        case TOPLEVEL::insertType::amplitude:
            cmd = ADDVOICE::control::enableAmplitudeLFO;
            break;
        case TOPLEVEL::insertType::frequency:
            cmd = ADDVOICE::control::enableFrequencyLFO;
            break;
        case TOPLEVEL::insertType::filter:
            cmd = ADDVOICE::control::enableFilterLFO;
            break;
    }

    value = toggle();
    if (value > -1)
    {
        if (engine != PART::engine::addVoice1 + voiceNumber)
            return available_msg;
        return sendNormal(value, controlType, cmd, npart, kitNumber, engine);;
    }
    if (lineEnd(controlType))
        return done_msg;

    value = -1;
    cmd = -1;

    if (matchnMove(1, point, "rate"))
        cmd = LFOINSERT::control::speed;
    else if (matchnMove(1, point, "intensity"))
        cmd = LFOINSERT::control::depth;
    else if (matchnMove(1, point, "start"))
        cmd = LFOINSERT::control::start;
    else if (matchnMove(1, point, "delay"))
        cmd = LFOINSERT::control::delay;
    else if (matchnMove(1, point, "expand"))
        cmd = LFOINSERT::control::stretch;
    else if (matchnMove(1, point, "continuous"))
    {
        value = (toggle() == 1);
        cmd = LFOINSERT::control::continuous;
    }
    else if (matchnMove(1, point, "type"))
    {
        if (lineEnd(controlType))
            return what_msg;
        if (matchnMove(2, point, "sine"))
            value = 0;
        else if (matchnMove(1, point, "triangle"))
            value = 1;
        else if (matchnMove(2, point, "square"))
            value = 2;
        else if (matchnMove(2, point, "rup"))
            value = 3;
        else if (matchnMove(2, point, "rdown"))
            value = 4;
        else if (matchnMove(1, point, "e1dn"))
            value = 5;
        else if (matchnMove(1, point, "e2dn"))
            value = 6;
        cmd = LFOINSERT::control::type;
    }
    else if (matchnMove(2, point, "ar"))
        cmd = LFOINSERT::control::amplitudeRandomness;
    else if (matchnMove(2, point, "fr"))
        cmd = LFOINSERT::control::frequencyRandomness;

    //cout << ">> base cmd " << int(cmd) << "  part " << int(npart) << "  kit " << int(kitNumber) << "  engine " << int(engine) << "  parameter " << int(group) << endl;

    if (value == -1)
        value = string2float(point);
    return sendNormal(value, controlType, cmd, npart, kitNumber, engine, TOPLEVEL::insert::LFOgroup, group);
}


int CmdInterface::filterSelect(unsigned char controlType)
{
    int cmd = -1;
    float value = -1;
    int param = UNUSED;
    if (lineEnd(controlType))
        return done_msg;

    int engine = contextToEngines();
    if (engine == PART::engine::addVoice1)
        engine += voiceNumber;

    if (engine == PART::engine::subSynth || engine == PART::engine::addVoice1 + voiceNumber)
    {
        value = toggle();
        if (value > -1)
        {
            if (engine == PART::engine::subSynth)
                cmd = SUBSYNTH::control::enableFilter;
            else
                cmd = ADDVOICE::control::enableFilter;
            readControl(FILTERINSERT::control::baseType, npart, kitNumber, engine, TOPLEVEL::insert::filterGroup);

            return sendNormal(value, controlType, cmd, npart, kitNumber, engine);;
        }
        value = -1; // return it as not set
    }

    if (matchnMove(2, point, "center"))
        cmd = FILTERINSERT::control::centerFrequency;
    else if (matchnMove(1, point, "q"))
        cmd = FILTERINSERT::control::Q;
    else if (matchnMove(1, point, "velocity"))
        cmd = FILTERINSERT::control::velocitySensitivity;
    else if (matchnMove(2, point, "slope"))
        cmd = FILTERINSERT::control::velocityCurve;
    else if (matchnMove(1, point, "gain"))
        cmd = FILTERINSERT::control::gain;
    else if (matchnMove(2, point, "tracking"))
        cmd = FILTERINSERT::control::frequencyTracking;
    else if (matchnMove(1, point, "range"))
    {
        value = (toggle() == 1);
        cmd = FILTERINSERT::control::frequencyTrackingRange;
    }
    else if (matchnMove(2, point, "category"))
    {
        if (matchnMove(1, point, "analog"))
            value = 0;
        else if(matchnMove(1, point, "formant"))
            value = 1;
        else if(matchnMove(1, point, "state"))
            value = 2;
        else
            return range_msg;
        cmd = FILTERINSERT::control::baseType;
    }
    else if (matchnMove(2, point, "stages"))
    {
        if (lineEnd(controlType))
            return value_msg;
        value = string2int(point) - 1;
        cmd = FILTERINSERT::control::stages;
    }
    if (cmd == -1)
    {
        int baseType = readControl(FILTERINSERT::control::baseType, npart, kitNumber, engine, TOPLEVEL::insert::filterGroup);
        if (baseType == 1) // formant
        {
            if (matchnMove(1, point, "invert"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = (toggle() == 1);
                cmd = FILTERINSERT::control::negateInput;
            }
            else if (matchnMove(2, point, "fcenter"))
                cmd = FILTERINSERT::control::formantCenter;
            else if (matchnMove(2, point, "frange"))
                cmd = FILTERINSERT::control::formantOctave;
            else if (matchnMove(1, point, "expand"))
                cmd = FILTERINSERT::control::formantStretch;
            else if (matchnMove(1, point, "lucidity"))
                cmd = FILTERINSERT::control::formantClearness;
            else if (matchnMove(1, point, "morph"))
                cmd = FILTERINSERT::control::formantSlowness;
            else if (matchnMove(2, point, "size"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = FILTERINSERT::control::sequenceSize;
            }
            else if (matchnMove(2, point, "count"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                cmd = FILTERINSERT::control::numberOfFormants;
            }
            else if (matchnMove(2, point, "vowel"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                filterVowelNumber = string2int(point);
                return done_msg;
            }
            else if (matchnMove(1, point, "point"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                if (lineEnd(controlType))
                    return value_msg;
                point = skipChars(point);
                int position = string2int(point);
                cout << "val " << value << "  pos " << position << endl;
                return sendNormal(value, controlType, FILTERINSERT::control::vowelPositionInSequence, npart, kitNumber, engine, TOPLEVEL::insert::filterGroup, position);
            }
            else if (matchnMove(1, point, "item"))
            {
                if (lineEnd(controlType))
                    return value_msg;
                filterFormantNumber = string2int(point);
                return done_msg;
            }
            else
            {
                if (matchnMove(2, point, "ffrequency"))
                    cmd = FILTERINSERT::control::formantFrequency;
                else if (matchnMove(2, point, "fq"))
                    cmd = FILTERINSERT::control::formantQ;
                else if (matchnMove(2, point, "fgain"))
                    cmd = FILTERINSERT::control::formantAmplitude;
                if (cmd == -1)
                    return range_msg;
                value = string2int(point);
                return sendNormal(value, controlType, cmd, npart, kitNumber, engine, TOPLEVEL::insert::filterGroup, filterFormantNumber, filterVowelNumber);
            }
        }
        else if (matchnMove(2, point, "type"))
        {
            switch (baseType)
            {
                case 0: // analog
                {
                    if (matchnMove(2, point, "l1"))
                        value = 0;
                    else if (matchnMove(2, point, "h1"))
                        value = 1;
                    if (matchnMove(2, point, "l2"))
                        value = 2;
                    else if (matchnMove(2, point, "h2"))
                        value = 3;
                    else if (matchnMove(2, point, "bpass"))
                        value = 4;
                    else if (matchnMove(2, point, "stop"))
                        value = 5;
                    else if (matchnMove(2, point, "peak"))
                        value = 6;
                    else if (matchnMove(2, point, "lshelf"))
                        value = 7;
                    else if (matchnMove(2, point, "hshelf"))
                        value = 8;
                    else
                        return range_msg;
                    cmd = FILTERINSERT::control::analogType;
                    break;
                }
                case 2: // state variable
                {
                    if (matchnMove(1, point, "low"))
                        value = 0;
                    else if (matchnMove(1, point, "high"))
                        value = 1;
                    else if (matchnMove(1, point, "band"))
                        value = 2;
                    else if (matchnMove(1, point, "stop"))
                        value = 3;
                    else
                        return range_msg;
                    cmd = FILTERINSERT::control::stateVariableType;
                    break;
                }
                default:
                    return available_msg;
                    break;
            }
        }
    }

    //cout << ">> base cmd " << int(cmd) << "  part " << int(npart) << "  kit " << int(kitNumber) << "  engine " << int(engine) << "  parameter " << int(param) << endl;

    if (value == -1)
        value = string2float(point);
    return sendNormal(value, controlType, cmd, npart, kitNumber, engine, TOPLEVEL::insert::filterGroup, param);
}


int CmdInterface::envelopeSelect(unsigned char controlType)
{
    int cmd = -1;
    float value = -1;
    int group = -1;
    unsigned char insert = TOPLEVEL::insert::envelopeGroup;
    unsigned char par2 = UNUSED;
    if (lineEnd(controlType))
        return done_msg;

    int engine = contextToEngines();
    if (engine == PART::engine::addVoice1)
        engine += voiceNumber;

    if (matchnMove(2, point, "amplitute"))
        group = TOPLEVEL::insertType::amplitude;
    else if (matchnMove(2, point, "frequency"))
        group = TOPLEVEL::insertType::frequency;
    else if (matchnMove(2, point, "filter"))
        group = TOPLEVEL::insertType::filter;
    else if (matchnMove(2, point, "bandwidth"))
    {
        if(bitTest(context, LEVEL::SubSynth))
            group = TOPLEVEL::insertType::bandwidth;
        else
            return available_msg;
    }

    if (group > -1)
        insertType = group;
    else
        group = insertType;

    switch (insertType)
    {
        case TOPLEVEL::insertType::amplitude:
            cmd = ADDVOICE::control::enableAmplitudeEnvelope;
            break;
        case TOPLEVEL::insertType::frequency:
            cmd = ADDVOICE::control::enableFrequencyEnvelope;
            break;
        case TOPLEVEL::insertType::filter:
            cmd = ADDVOICE::control::enableFilterEnvelope;
            break;
        case TOPLEVEL::insertType::bandwidth:
            cmd = SUBSYNTH::control::enableBandwidthEnvelope;
            break;
    }
    if (lineEnd(controlType))
        return done_msg;

    value = toggle();
    if (value > -1)
    {
        if (engine == PART::engine::addVoice1 + voiceNumber || engine == PART::engine::subSynth )
            return sendNormal(value, controlType, cmd, npart, kitNumber, engine);
        else
            return available_msg;
    }

    if (matchnMove(2, point, "fmode"))
    {
        return sendNormal((toggle() == 1), controlType, ENVELOPEINSERT::control::enableFreeMode, npart, kitNumber, engine, TOPLEVEL::insert::envelopeGroup, insertType);
    }

    // common controls
    value = -1;
    cmd = -1;
    if (matchnMove(2, point, "expand"))
        cmd = ENVELOPEINSERT::control::stretch;
    else if (matchnMove(1, point, "force"))
    {
        cmd = ENVELOPEINSERT::control::forcedRelease;
        value = (toggle() == 1);
    }
    else if (matchnMove(2, point, "linear"))
    {
        cmd = ENVELOPEINSERT::control::linearEnvelope;
        value = (toggle() == 1);
    }

    bool freeMode = readControl(ENVELOPEINSERT::control::enableFreeMode, npart, kitNumber, engine, TOPLEVEL::insert::envelopeGroup, insertType);

    if (freeMode && cmd == -1)
    {
        int pointCount = readControl(ENVELOPEINSERT::control::points, npart, kitNumber, engine, insert, insertType);
        if (matchnMove(1, point, "Points"))
        {
            value = 0; // dummy value
            cmd = ENVELOPEINSERT::control::points;
            // not using already fetched value to get normal reporting
        }
        else if (matchnMove(1, point, "Sustain"))
        {
            if (lineEnd(controlType))
                return value_msg;
            value = string2int(point);
            if (value == 0)
            {
                    synth->getRuntime().Log("Sustain can't be at first point");
                    return done_msg;
            }
            else if (value >= (pointCount - 1))
            {
                    synth->getRuntime().Log("Sustain can't be at last point");
                    return done_msg;
            }
            else if (value < 0)
                return range_msg;
            cmd = ENVELOPEINSERT::control::sustainPoint;
        }
        else
        {
            if (matchnMove(1, point, "insert"))
            {
                if ((MAX_ENVELOPE_POINTS - pointCount) < 2)
                {
                    synth->getRuntime().Log("Max points already defined");
                    return done_msg;
                }
                if (lineEnd(controlType))
                    return value_msg;

                cmd = string2int(point); // point number
                if (cmd == 0)
                {
                    synth->getRuntime().Log("Can't add at first point");
                    return done_msg;
                }
                if (cmd < 0 || cmd >= pointCount)
                    return range_msg;
                point = skipChars(point);
                if (lineEnd(controlType))
                    return value_msg;

                par2 = string2int(point); // X
                point = skipChars(point);
                if (lineEnd(controlType))
                    return value_msg;

                value = string2int(point); // Y
                insert = TOPLEVEL::insert::envelopePoints;

            }
            else if (matchnMove(1, point, "delete"))
            {
                if (pointCount <= 3)
                {
                    synth->getRuntime().Log("Can't have less than three points");
                    return done_msg;
                }
                if (lineEnd(controlType))
                    return value_msg;

                cmd = string2int(point); // point number
                if (cmd == 0)
                {
                    synth->getRuntime().Log("Can't delete first point");
                    return done_msg;
                }
                if (cmd >= (pointCount - 1))
                {
                    synth->getRuntime().Log("Can't delete last point");
                    return done_msg;
                }
                if (cmd < 0 || cmd >= (MAX_ENVELOPE_POINTS - 1))
                    return range_msg;
                insert = TOPLEVEL::insert::envelopePoints;
            }
            else if (matchnMove(1, point, "change"))
            {
                if (lineEnd(controlType))
                return value_msg;

                cmd = string2int(point); // point number
                if (cmd < 0 || cmd >= (pointCount - 1))
                    return range_msg;
                point = skipChars(point);
                if (lineEnd(controlType))
                return value_msg;

                par2 = string2int(point); // X
                point = skipChars(point);
                if (lineEnd(controlType))
                return value_msg;

                value = string2int(point); // Y
                insert = TOPLEVEL::insert::envelopePointChange;
            }
        }
    }
    else if (cmd == -1)
    {
        if (matchnMove(1, point, "attack"))
        {
            if (matchnMove(1, point, "level"))
                cmd = ENVELOPEINSERT::control::attackLevel;
            else if (matchnMove(1, point, "time"))
                cmd = ENVELOPEINSERT::control::attackTime;
        }
        else if (matchnMove(1, point, "decay"))
        {
            if (matchnMove(1, point, "level"))
                cmd = ENVELOPEINSERT::control::decayLevel;
            else if (matchnMove(1, point, "time"))
                cmd = ENVELOPEINSERT::control::decayTime;
        }
        else if (matchnMove(1, point, "sustain"))
            cmd = ENVELOPEINSERT::control::sustainLevel;
        else if (matchnMove(1, point, "release"))
        {
            if (matchnMove(1, point, "level"))
                cmd = ENVELOPEINSERT::control::releaseLevel;
            else if (matchnMove(1, point, "time"))
                cmd = ENVELOPEINSERT::control::releaseTime;
        }
    }

    if (cmd == -1)
        return opp_msg;

    if (value == -1)
    {
        if (lineEnd(controlType))
            return value_msg;
        value = string2float(point);
    }

    //cout << ">> base cmd " << int(cmd) << "  part " << int(npart) << "  kit " << int(kitNumber) << "  engine " << int(engine) << "  parameter " << int(insertType) << endl;

    return sendNormal(string2float(point), controlType, cmd, npart, kitNumber, engine, insert, insertType, par2);
}


int CmdInterface::commandList()
{
    Config &Runtime = synth->getRuntime();
    int ID;
    int tmp;
    list<string> msg;

    if (matchnMove(1, point, "instruments") || matchnMove(2, point, "programs"))
    {
        if (point[0] == 0)
            ID = 128;
        else
            ID = string2int(point);
        synth->ListInstruments(ID, msg);
        synth->cliOutput(msg, LINES);
        return done_msg;
    }

    if (matchnMove(1, point, "banks"))
    {
        if (point[0] == 0)
            ID = 128;
        else
            ID = string2int(point);
        synth->ListBanks(ID, msg);
        synth->cliOutput(msg, LINES);
        return done_msg;
    }

    if (matchnMove(1, point, "roots"))
    {
        synth->ListPaths(msg);
        synth->cliOutput(msg, LINES);
        return done_msg;
    }

    if (matchnMove(1, point, "vectors"))
    {
        synth->ListVectors(msg);
        synth->cliOutput(msg, LINES);
        return done_msg;
    }

    if (matchnMove(1, point, "parts"))
    {
        synth->ListCurrentParts(msg);
        synth->cliOutput(msg, LINES);
        return done_msg;
    }

    if (matchnMove(1, point, "config"))
    {
        synth->ListSettings(msg);
        synth->cliOutput(msg, LINES);
        return done_msg;
    }

    if (matchnMove(2, point, "mlearn"))
    {
        if (point[0] == '@')
        {
            point += 1;
            point = skipSpace(point);
            tmp = string2int(point);
            if (tmp > 0)
                synth->midilearn.listLine(tmp - 1);
            else
                return value_msg;
        }
        else
        {
            synth->midilearn.listAll(msg);
            synth->cliOutput(msg, LINES);
        }
        return done_msg;
    }

    if (matchnMove(1, point, "tuning"))
    {
        Runtime.Log("Tuning:\n" + synth->microtonal.tuningtotext());
        return done_msg;
    }
    if (matchnMove(1, point, "keymap"))
    {
        Runtime.Log("Keymap:\n" + synth->microtonal.keymaptotext());
        return done_msg;
    }

    if (matchnMove(1, point, "history"))
    {
        if (point[0] == 0)
            historyList(0);
        else if (matchnMove(1, point, "instruments") || matchnMove(2, point, "program") )
            historyList(XML_INSTRUMENT);
        else if (matchnMove(1, point, "patchsets"))
            historyList(XML_PARAMETERS);
        else if (matchnMove(2, point, "scales"))
            historyList(XML_MICROTONAL);
        else if (matchnMove(2, point, "states"))
            historyList(XML_STATE);
        else if (matchnMove(1, point, "vectors"))
            historyList(XML_VECTOR);
        else if (matchnMove(2, point, "mlearn"))
            historyList(XML_MIDILEARN);
        else
            historyList(0);
        return done_msg;
    }

    if (matchnMove(1, point, "effects") || matchnMove(1, point, "efx"))
        return effectsList();
    if (matchnMove(3, point, "presets"))
        return effectsList(true);
    replyString = "list";
    return what_msg;
}


string CmdInterface::findStatus(bool show)
{
    string text = "";
    int kit = UNUSED;
    int insert = UNUSED;

    // effects block needs cleaning up
    // to remove direct reads
    if (bitTest(context, LEVEL::AllFX))
    {
        if (bitTest(context, LEVEL::Part))
        {
            text += " Part ";
            text += to_string(int(npart) + 1);
        }
        else if (bitTest(context, LEVEL::InsFX))
        {
            text += " Ins";
            nFXtype = synth->insefx[nFX]->geteffect();
        }
        else
        {
            text += " Sys";
            nFXtype = synth->sysefx[nFX]->geteffect();
        }
        text += (" efx " + asString(nFX + 1) + " " + fx_list[nFXtype].substr(0, 5));
        if (nFXtype > 0)
            text += ("-" + asString(nFXpreset + 1));
        return text;
    }

    if (bitTest(context, LEVEL::Part))
    {
        bool justPart = false;
        if (bitFindHigh(context) == LEVEL::Part)
        {
            text += " Part ";
            justPart = true;
        }
        else text = " P";
        text += to_string(int(npart) + 1);
        if (readControl(PART::control::enable, npart))
            text += "+";
        kitMode = readControl(PART::control::kitMode, npart);
        if (kitMode != PART::kitType::Off)
        {
            kit = kitNumber;
            insert = TOPLEVEL::insert::kitGroup;
            if (justPart)
                text += ", kit ";
            else
                text += ", K";
            text += to_string(kitNumber + 1);
            if (readControl(PART::control::enable, npart, kitNumber, UNUSED, insert))
                text += "+";
            text += ", ";
            switch (kitMode)
            {
                case PART::kitType::Multi:
                    if (justPart)
                        text += "multi";
                    else
                        text += "M";
                    break;
                case PART::kitType::Single:
                    if (justPart)
                        text += "single";
                    else
                        text += "S";
                    break;
                case PART::kitType::CrossFade:
                    if (justPart)
                        text += "cross";
                    else
                        text += "C";
                    break;
                default:
                    break;
            }
        }
        else
            kitNumber = 0;
        if (!show)
            return "";

        int engine = contextToEngines();
        switch (engine)
        {
            case PART::engine::addSynth:
                if (bitFindHigh(context) == LEVEL::AddSynth)
                    text += ", Add";
                else
                    text += ", A";
                if (readControl(ADDSYNTH::control::enable, npart, kit, PART::engine::addSynth, insert))
                    text += "+";
                break;
            case PART::engine::subSynth:
                if (bitFindHigh(context) == LEVEL::SubSynth)
                    text += ", Sub";
                else
                    text += ", S";
                if (readControl(SUBSYNTH::control::enable, npart, kit, PART::engine::subSynth, insert))
                    text += "+";
                break;
            case PART::engine::padSynth:
                if (bitFindHigh(context) == LEVEL::PadSynth)
                    text += ", Pad";
                else
                    text += ", P";
                if (readControl(PADSYNTH::control::enable, npart, kit, PART::engine::padSynth, insert))
                    text += "+";
                break;
            case PART::engine::addVoice1:
                text += ", A";
                if (readControl(ADDSYNTH::control::enable, npart, kit, PART::engine::addSynth, insert))
                    text += "+";
                if (bitFindHigh(context) == LEVEL::AddVoice)
                    text += ", Voice ";
                else
                    text += ", V";
                text += to_string(voiceNumber + 1);
                if (readControl(ADDVOICE::control::enableVoice, npart, kitNumber, PART::engine::addVoice1 + voiceNumber))
                    text += "+";
                break;
        }

        if (bitTest(context, LEVEL::Oscillator))
        {
            text += " Wave ";
            /*
             * TODO not yet!
            int source = readControl(ADDVOICE::control::voiceOscillatorSource, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
            if (source > -1)
            {
                text += "V" + to_string(source + 1);
                if (readControl(ADDVOICE::control::enableVoice, npart, kitNumber, PART::engine::addVoice1 + source))
                    text += "+";
            }*/
        }

        if (bitTest(context, LEVEL::LFO))
        {
            int cmd = -1;
            switch (insertType)
            {
                case TOPLEVEL::insertType::amplitude:
                    cmd = ADDVOICE::control::enableAmplitudeLFO;
                    text += ", amp";
                    break;
                case TOPLEVEL::insertType::frequency:
                    cmd = ADDVOICE::control::enableFrequencyLFO;
                    text += ", freq";
                    break;
                case TOPLEVEL::insertType::filter:
                    cmd = ADDVOICE::control::enableFilterLFO;
                    text += ", filt";
                    break;
            }
            text += " LFO";
            if (engine == PART::engine::addVoice1)
            {
                if (readControl(cmd, npart, kitNumber, engine + voiceNumber))
                    text += "+";
            }
            else
                text += "+";
        }
        else if (bitTest(context, LEVEL::Filter))
        {
            int baseType = readControl(FILTERINSERT::control::baseType, npart, kitNumber, engine, TOPLEVEL::insert::filterGroup);
            text += ", ";
            switch (baseType)
            {
                case 0:
                    text += "analog";
                    break;
                case 1:
                    text += "formant";
                    break;
                case 2:
                    text += "state var";
                    break;
            }
            text += " Filter";
            if (engine == PART::engine::subSynth)
            {
                if (readControl(SUBSYNTH::control::enableFilter, npart, kitNumber, engine))
                    text += "+";
            }
            else if (engine == PART::engine::addVoice1)
            {
                if (readControl(ADDVOICE::control::enableFilter, npart, kitNumber, engine + voiceNumber))
                    text += "+";
            }
            else
                text += "+";
        }
        else if (bitTest(context, LEVEL::Envelope))
        {
            int cmd = -1;
            switch (insertType)
            {
                case TOPLEVEL::insertType::amplitude:
                    cmd = ADDVOICE::control::enableAmplitudeEnvelope;
                    text += ", amp";
                    break;
                case TOPLEVEL::insertType::frequency:
                    cmd = ADDVOICE::control::enableFrequencyEnvelope;
                    text += ", freq";
                    break;
                case TOPLEVEL::insertType::filter:
                    cmd = ADDVOICE::control::enableFilterEnvelope;
                    text += ", filt";
                    break;
                case TOPLEVEL::insertType::bandwidth:
                    cmd = SUBSYNTH::control::enableBandwidthEnvelope;
                    text += ", band";
                    break;
            }
            text += " Envel";
            if (readControl(ENVELOPEINSERT::control::enableFreeMode, npart, kitNumber, engine, TOPLEVEL::insert::envelopeGroup, insertType))
                text += " free";
            if (engine == PART::engine::addVoice1 || (engine == PART::engine::subSynth && cmd != ADDVOICE::control::enableAmplitudeEnvelope && cmd != ADDVOICE::control::enableFilterEnvelope))
            {
                if (readControl(cmd, npart, kitNumber, engine + voiceNumber))
                    text += "+";
            }
            else
                text += "+";
        }
    }
    else if (bitTest(context, LEVEL::Scale))
        text += " Scale ";
    else if (bitTest(context, LEVEL::Config))
        text += " Config ";
    else if (bitTest(context, LEVEL::Vector))
    {
        text += (" Vect Ch " + asString(chan + 1) + " ");
        if (axis == 0)
            text += "X";
        else
            text += "Y";
    }
    else if (bitTest(context, LEVEL::Learn))
        text += (" MLearn line " + asString(mline + 1) + " ");

    return text;
}


int CmdInterface::contextToEngines()
{
    int engine = UNUSED;
    if (bitTest(context, LEVEL::SubSynth))
        engine = PART::engine::subSynth;
    else if (bitTest(context, LEVEL::PadSynth))
        engine = PART::engine::padSynth;
    else if (bitTest(context, LEVEL::AddVoice))
        engine = PART::engine::addVoice1;
    else if (bitTest(context, LEVEL::AddSynth))
        engine = PART::engine::addSynth;
    return engine;
}


int CmdInterface::toggle()
{
    if (matchnMove(2, point, "enable") || matchnMove(2, point, "on") || matchnMove(3, point, "yes"))
        return 1;
    if (matchnMove(2, point, "disable") || matchnMove(3, point, "off") || matchnMove(2, point, "no") )
        return 0;
    return -1;
    /*
     * this allows you to specify enable or other, disable or other or must be those specifics
     */
}


bool CmdInterface::lineEnd(unsigned char controlType)
{
    return (point[0] == 0 && controlType == TOPLEVEL::type::Write);
}


int CmdInterface::commandMlearn(unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    list<string> msg;
    bitSet(context, LEVEL::Learn);

    if (controlType != TOPLEVEL::type::Write)
    {
        Runtime.Log("Write only");
        return done_msg; // will eventually be readable
    }

    if (isdigit(point[0]) || point[0] == '-') // negative should never happen!
    {
        int lineNo = string2int(point);
        point = skipChars(point);
        if (lineNo <= 0)
            return value_msg;
        else
            mline = lineNo -1;
    }
    int tmp = synth->midilearn.findSize();
    if (tmp == 0 || tmp <= mline)
    {
        if (tmp == 0)
            Runtime.Log("No learned lines");
        else
            Runtime.Log("Line " + to_string(mline + 1) + " Not found");
        mline = 0;
        return (done_msg);
    }
    if (lineEnd(controlType))
        return done_msg;
    {
        unsigned char type = 0;
        unsigned char control = 0;
        unsigned char kit = UNUSED;
        unsigned char engine = UNUSED;
        unsigned char insert = UNUSED;
        unsigned char parameter = UNUSED;

        if (matchnMove(2, point, "cc"))
        {
            if (!isdigit(point[0]))
                return value_msg;
            kit = string2int(point);
            if (kit > 129)
            {
                Runtime.Log("Max CC value is 129");
                return done_msg;
            }
            control = MIDILEARN::control::CCorChannel;
            Runtime.Log("Lines may be re-ordered");
        }
        else if (matchnMove(2, point, "channel"))
        {
            engine = string2int(point) - 1;
            if (engine > 16)
                engine = 16;
            control = MIDILEARN::control::CCorChannel;
            Runtime.Log("Lines may be re-ordered");;
        }
        else if (matchnMove(2, point, "minimum"))
        {
            insert = int((string2float(point)* 2.0f) + 0.5f);
            if (insert > 200)
                return value_msg;
            control = MIDILEARN::control::minimum;
        }
        else if (matchnMove(2, point, "maximum"))
        {
            parameter = int((string2float(point)* 2.0f) + 0.5f);
            if (parameter > 200)
                return value_msg;
            control = MIDILEARN::control::maximum;
        }
        else if (matchnMove(2, point, "mute"))
        {
            type = (toggle() == 1) * 4;
            control = MIDILEARN::control::mute;
        }
        else if (matchnMove(2, point, "limit"))
        {
            type = (toggle() == 1) * 2;
            control = MIDILEARN::control::limit;
        }
        else if (matchnMove(2, point, "block"))
        {
            type = (toggle() == 1);
            control = MIDILEARN::control::block;
        }
        else if (matchnMove(2, point, "seven"))
        {
            type = (toggle() == 1) * 16;
            control = MIDILEARN::control::sevenBit;
        }
        sendNormal(mline, type, control, TOPLEVEL::section::midiLearn, kit, engine, insert, parameter);
        return done_msg;
    }
    return opp_msg;
}


int CmdInterface::commandVector(unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    list<string> msg;
    int tmp;
    bitSet(context, LEVEL::Vector);
    if (controlType != TOPLEVEL::type::Write)
    {
        if (synth->SingleVector(msg, chan))
            synth->cliOutput(msg, LINES);
        else
            Runtime.Log("No vector on channel " + asString(chan + 1));
        return done_msg;
    }
    if (lineEnd(controlType))
    {
        if (!Runtime.vectordata.Enabled[chan])
            Runtime.Log("No vector on channel " + asString(chan + 1));
        return done_msg;
    }

    unsigned char ch = string2int127(point);
    if (ch > 0)
    {
        ch -= 1;
        if (ch >= NUM_MIDI_CHANNELS)
            return range_msg;
        point = skipChars(point);
        if (chan != ch)
        {
            chan = ch;
            axis = 0;
        }

        Runtime.Log("Vector channel set to " + asString(chan + 1));
    }

    if (matchWord(1, point, "off"))
    {
        sendDirect(0,controlType,VECTOR::control::erase, TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
        axis = 0;
        bitClear(context, LEVEL::Vector);
        return done_msg;
    }
    if (matchnMove(1, point, "xaxis"))
        axis = 0;
    else if (matchnMove(1, point, "yaxis"))
    {
        if (!Runtime.vectordata.Enabled[chan])
        {
            Runtime.Log("Vector X must be set first");
            return done_msg;
        }
        axis = 1;
    }

    if (lineEnd(controlType))
        return done_msg;

    if (matchnMove(2, point, "cc"))
    {
        if (lineEnd(controlType))
            return value_msg;

        tmp = string2int(point);
        if (axis == 0)
        {
            sendDirect(tmp, controlType, VECTOR::control::Xcontroller, TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
            bitSet(context, LEVEL::Vector);
            return done_msg;
        }
        if (Runtime.vectordata.Enabled[chan])
        {
            sendDirect(tmp, controlType, VECTOR::control::Ycontroller, TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
            return done_msg;
        }
    }

    if (!Runtime.vectordata.Enabled[chan])
    {
        Runtime.Log("Vector X CC must be set first");
        return done_msg;
    }

    if (axis == 1 && (Runtime.vectordata.Yaxis[chan] > 0x7f))
    {
        Runtime.Log("Vector Y CC must be set first");
        return done_msg;
    }

    if (matchnMove(1, point, "name"))
    {
        string name = "!";
        if (controlType == TOPLEVEL::type::Write)
        {
            name = string(point);
            if (name <= "!")
                return value_msg;
        }
        sendDirect(0, controlType, VECTOR::control::name, TOPLEVEL::section::vector, UNUSED, UNUSED, chan, TOPLEVEL::route::lowPriority, miscMsgPush(name));
        return done_msg;
    }

    if (matchnMove(1, point, "features"))
    {
        if (lineEnd(controlType))
            return value_msg;
        int feat = string2int(point) - 1;
        if (feat < 0 || feat > 3)
            return range_msg;
        point = skipChars(point);
        int enable = 0;
        if (toggle() == 1)
            enable = 1;
        else if (feat > 1 && matchnMove(1, point, "reverse"))
            enable = 2;
        sendDirect(enable, controlType, VECTOR::control::Xfeature0 + (axis * (VECTOR::control::Ycontroller - VECTOR::control::Xcontroller)) + feat , TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
        return done_msg;
    }

    if (matchnMove(2, point, "program") || matchnMove(1, point, "instrument"))
    {
        int hand = point[0] | 32;
        point = skipChars(point); // in case they type the entire word
        if ((axis == 0 && (hand == 'd' || hand == 'u')) || (axis == 1 && (hand == 'l' || hand == 'r')))
        {
            Runtime.Log("Bad direction for this axis");
            return done_msg;
        }
        if (hand == 'l' || hand == 'd')
            hand = 0;
        else if (hand == 'r' || hand == 'u')
            hand = 1;
        else
            return opp_msg;
        tmp = string2int(point);
        sendDirect(tmp, controlType, VECTOR::control::XleftInstrument + hand + (axis * (VECTOR::control::Ycontroller - VECTOR::control::Xcontroller)), TOPLEVEL::section::vector, UNUSED, UNUSED, chan);
        return done_msg;
    }

    // this disabled for now - needs a lot of work.
    /*if (!matchnMove(1, point, "control"))
        return opp_msg;
    if(isdigit(point[0]))
    {
        int cmd = string2int(point);
        if (cmd < 2 || cmd > 4)
            return range_msg;
        point = skipChars(point);
        if (lineEnd(controlType))
            return value_msg;
        tmp = string2int(point);
        if (!synth->vectorInit(axis * 3 + cmd + 6, chan, tmp))
        {
            synth->vectorSet(axis * 3 + cmd + 6, chan, tmp);
            return done_msg;
        }
        else
            return value_msg;
    }*/

    return opp_msg;
}


int CmdInterface::commandConfig(unsigned char controlType)
{
    /*if (lineEnd(controlType))
    {
        if (controlType != TOPLEVEL::type::Write)
            sendDirect(0, 0, 80, TOPLEVEL::section::main); // status
        return done_msg;
    }*/
    float value = 0;
    unsigned char command = UNUSED;
    unsigned char par = UNUSED;
    unsigned char par2 = UNUSED;

    if (matchnMove(1, point, "oscillator"))
    {
        command = CONFIG::control::oscillatorSize;
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(2, point, "buffer"))
    {
        command = CONFIG::control::bufferSize;
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(2, point, "padsynth"))
    {
        command = CONFIG::control::padSynthInterpolation;
        value = !matchnMove(1, point, "linear");
    }
    else if (matchnMove(1, point, "virtual"))
    {
        command = CONFIG::control::virtualKeyboardLayout;
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(1, point, "xml"))
    {
        command = CONFIG::control::XMLcompressionLevel;
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        value = string2int(point);
    }
    else if (matchnMove(2, point, "reports"))
    {
        command = CONFIG::control::reportsDestination;
        value = !matchnMove(1, point, "stdout");
    }
    else if (matchnMove(2, point, "saved"))
    {
        command = CONFIG::control::savedInstrumentFormat;
        if (matchnMove(1, point, "legacy"))
            value = 1;
        else if (matchnMove(1, point, "yoshimi"))
            value = 2;
        else if (matchnMove(1, point, "both"))
            value = 3;
        else if (controlType == TOPLEVEL::type::Write)
            return value_msg;
    }

    else if (matchnMove(2, point, "state"))
    {
        command = CONFIG::control::defaultStateStart;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "hide"))
    {
        command = CONFIG::control::hideNonFatalErrors;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "display"))
    {
        command = CONFIG::control::showSplash;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "time"))
    {
        command = CONFIG::control::logInstrumentLoadTimes;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "include"))
    {
        command = CONFIG::control::logXMLheaders;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "keep"))
    {
        command = CONFIG::control::saveAllXMLdata;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "gui"))
    {
        command = CONFIG::control::enableGUI;
        value = toggle();
        if (value == -1)
            return value_msg;
    }
    else if (matchnMove(1, point, "cli"))
    {
        command = CONFIG::control::enableCLI;
        value = toggle();
        if (value == -1)
            return value_msg;
    }

    else if (matchnMove(1, point, "expose"))
    {
        value = toggle();
        if (value == -1 && matchnMove(2, point, "prompt"))
            value = 2;
        if (value == -1)
            return value_msg;
        command = CONFIG::control::exposeStatus;
    }

    else if (matchnMove(1, point, "jack"))
    {
        if (matchnMove(1, point, "midi"))
        {
            command = CONFIG::control::jackMidiSource;
            par = TOPLEVEL::route::lowPriority;
            if (controlType != TOPLEVEL::type::Write || point[0] != 0)
            {
                if (controlType == TOPLEVEL::type::Write)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "server"))
        {
            command = CONFIG::control::jackServer;
            par = TOPLEVEL::route::lowPriority;
            if (controlType != TOPLEVEL::type::Write || point[0] != 0)
            {
                if (controlType == TOPLEVEL::type::Write)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "auto"))
        {
            command = CONFIG::control::jackAutoConnectAudio;
            value = (toggle() == 1);
        }
        else
            return opp_msg;
    }

    else if (matchnMove(2, point, "alsa"))
    {
        if (matchnMove(1, point, "midi"))
        {
            command = CONFIG::control::alsaMidiSource;
            par = TOPLEVEL::route::lowPriority;
            if (controlType != TOPLEVEL::type::Write || point[0] != 0)
            {
                if (controlType == TOPLEVEL::type::Write)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "audio"))
        {
            command = CONFIG::control::alsaAudioDevice;
            par = TOPLEVEL::route::lowPriority;
            if (controlType != TOPLEVEL::type::Write || point[0] != 0)
            {
                if (controlType == TOPLEVEL::type::Write)
                    par2 = miscMsgPush(string(point));
            }
            else
                return value_msg;
        }
        else if (matchnMove(1, point, "s"))
        {
            command = CONFIG::control::alsaSampleRate;
            if (controlType == TOPLEVEL::type::Write)
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int(point);
                if (value < 0 || value > 3)
                    return range_msg;
            }
        }
        else
            return opp_msg;
    }

    else if (matchnMove(2, point, "midi"))
    {
        value = 1;
        if (matchnMove(1, point, "alsa"))
            command = CONFIG::control::alsaPreferredMidi;
        else if (controlType != TOPLEVEL::type::Write || matchnMove(1, point, "jack"))
            command = CONFIG::control::jackPreferredMidi;
        else
            return value_msg;
    }

    else if (matchnMove(2, point, "audio"))
    {
        value = 1;
        if (matchnMove(1, point, "alsa"))
            command = CONFIG::control::alsaPreferredAudio;
        else if (controlType != TOPLEVEL::type::Write || matchnMove(1, point, "jack"))
            command = CONFIG::control::jackPreferredAudio;
        else
            return value_msg;
    }

    else if (matchnMove(2, point, "root"))
    {
        command = CONFIG::control::bankRootCC;
        if (controlType != TOPLEVEL::type::Write)
            value = 128; // ignored by range check
        else if (lineEnd(controlType))
            return value_msg;
        else
            value = string2int(point);
    }
    else if (matchnMove(2, point, "bank"))
    {
        command = CONFIG::control::bankCC;
        if (controlType != TOPLEVEL::type::Write)
            value = 128; // ignored by range check
        else if (lineEnd(controlType))
            return value_msg;
        else
            value = string2int(point);
    }
    else if (matchnMove(2, point, "program") || matchnMove(2, point, "instrument"))
    {
        command = CONFIG::control::enableProgramChange;
        value = (toggle() == 1);
    }
    else if (matchnMove(2, point, "activate"))
    {
        command = CONFIG::control::programChangeEnablesPart;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "extend"))
    {
        command = CONFIG::control::extendedProgramChangeCC;
        if (controlType != TOPLEVEL::type::Write)
            value = 128; // ignored by range check
        else if (lineEnd(controlType))
            return value_msg;
        else
            value = string2int(point);
    }
    else if (matchnMove(1, point, "Quiet"))
    {
        command = CONFIG::control::ignoreResetAllCCs;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "log"))
    {
        command = CONFIG::control::logIncomingCCs;
        value = (toggle() == 1);
    }
    else if (matchnMove(2, point, "show"))
    {
        command = CONFIG::control::showLearnEditor;
        value = (toggle() == 1);
    }
    else if (matchnMove(1, point, "nrpn"))
    {
        command = CONFIG::control::enableNRPNs;
        value = (toggle() == 1);
    }

    else
        return todo_msg; // may be picked up later

    sendDirect(value, controlType, command, TOPLEVEL::section::config, UNUSED, UNUSED, UNUSED, par, par2);
    return done_msg;
}


int CmdInterface::commandScale(unsigned char controlType)
{
    if (lineEnd(controlType))
        return done_msg;
    Config &Runtime = synth->getRuntime();
    float value = 0;
    unsigned char command = UNUSED;
    unsigned char par = UNUSED;
    unsigned char par2 = UNUSED;
    if (controlType != TOPLEVEL::type::Write)
        return done_msg;

    string name;

    if (matchnMove(1, point, "tuning"))
        command = SCALES::control::tuning;
    else if (matchnMove(1, point, "keymap"))
        command = SCALES::control::keyboardMap;
    else if (matchnMove(2, point, "name"))
        command = SCALES::control::name;
    else if (matchnMove(2, point, "description"))
        command = SCALES::control::comment;

    if (command >= SCALES::control::tuning && command <= SCALES::control::comment)
    {
        if (controlType != TOPLEVEL::type::Write && command <= SCALES::control::importKbm)
        {
            Runtime.Log("Write only - use list");
            return done_msg;
        }
        if (command <= SCALES::control::keyboardMap)
        {
            if (matchnMove(3, point, "import"))
                command += (SCALES::control::importKbm - SCALES::control::keyboardMap);
        }
        name = (string)point;
        if (name == "")
            return value_msg;
        par = TOPLEVEL::route::lowPriority;
        par2 = miscMsgPush(name);
    }
    else
    {
        int min = 0;
        int max = 127;
        if (matchnMove(2, point, "frequency"))
        {
            command = SCALES::control::Afrequency;
            min = 1;
            max = 20000;
            controlType &= ~TOPLEVEL::type::Integer; // float
        }
        else if(matchnMove(2, point, "note"))
            command = SCALES::control::Anote;
        else if(matchnMove(1, point, "invert"))
        {
            command = SCALES::control::invertScale;
            max = 1;
        }
        else if(matchnMove(2, point, "center"))
            command = SCALES::control::invertedScaleCenter;
        else if(matchnMove(2, point, "shift"))
        {
            command = SCALES::control::scaleShift;
            min = -63;
            max = 64;
        }
        else if(matchnMove(2, point, "scale"))
        {
            command = SCALES::control::enableMicrotonal;
            max = 1;
        }
        else if(matchnMove(2, point, "mapping"))
        {
            command = SCALES::control::enableKeyboardMap;
            max = 1;
        }
        else if(matchnMove(2, point, "first"))
            command = SCALES::control::lowKey;
        else if(matchnMove(2, point, "middle"))
            command = SCALES::control::middleKey;
        else if(matchnMove(1, point, "last"))
            command = SCALES::control::highKey;
        else if(matchnMove(3, point, "CLEar"))
        {
            point -=1; // sneaky way to force a zero :)
            command = SCALES::control::clearAll;
        }
        else
            return todo_msg;

        if (controlType == TOPLEVEL::type::Write)
        {
            if (lineEnd(controlType))
                return value_msg;
            if ((toggle() == 1))
                value = 1;
            else//if (isdigit(point[0]))
            {
                value = string2float(point);
                if (value < min || value > max)
                    return value_msg;
            }
        }
    }
//cout << "par " << int(par) << endl;
    sendDirect(value, controlType, command, TOPLEVEL::section::scales, UNUSED, UNUSED, UNUSED, par, par2);
    return done_msg;
}


int CmdInterface::addVoice(unsigned char controlType)
{
    if (matchnMove(2, point, "waveform"))
    {
        bitSet(context, LEVEL::Oscillator);
        return waveform(controlType);
    }
    if (isdigit(point[0]))
    {
        voiceNumber = string2int(point) - 1;
        point = skipChars(point);
    }
    if (lineEnd(controlType))
        return done_msg;

    int value = toggle();
    if (value > -1)
    {
        sendNormal(value, controlType, ADDVOICE::control::enableVoice, npart, kitNumber, PART::engine::addVoice1 + voiceNumber);
        return done_msg;
    }
    int result = partCommonControls(controlType);
    if (result != todo_msg)
        return result;
    return available_msg;
}


int CmdInterface::addSynth(unsigned char controlType)
{

    if (matchnMove(1, point, "voice"))
    {
        bitSet(context, LEVEL::AddVoice);
        insertType = TOPLEVEL::insertType::amplitude;
        return addVoice(controlType);
    }
    if (lineEnd(controlType))
        return done_msg;
    int result = partCommonControls(controlType);
    if (result != todo_msg)
        return result;

    return available_msg;
}


int CmdInterface::subSynth(unsigned char controlType)
{
    if (lineEnd(controlType))
        return done_msg;
    int result = partCommonControls(controlType);
    if (result != todo_msg)
        return result;

    int cmd = -1;
    if (matchnMove(2, point, "harmonic"))
    {
        if (matchnMove(1, point, "stages"))
            cmd = SUBSYNTH::control::filterStages;
        else if (matchnMove(1, point, "mag"))
            cmd = SUBSYNTH::control::magType;
        else if (matchnMove(1, point, "position"))
            cmd = SUBSYNTH::control::startPosition;
        if (cmd != -1)
        {
            if (lineEnd(controlType))
                return value_msg;
            return sendNormal(string2int(point), controlType, cmd, npart, kitNumber, PART::engine::subSynth);
        }

        int control = -1;
        unsigned char insert = UNUSED;
        bool set = false;
        if (lineEnd(controlType))
            return parameter_msg;
        control = string2int(point) - 1;
        point = skipChars(point);
        if (matchnMove(1, point, "amplitude"))
        {
            insert = TOPLEVEL::insert::harmonicAmplitude;
            set = true;
        }
        else if (matchnMove(1, point, "bandwidth"))
        {
            insert = TOPLEVEL::insert::harmonicPhaseBandwidth;
            set = true;
        }
        if (set)
        {
            if (lineEnd(controlType))
                return value_msg;
            return sendNormal(string2int(point), controlType, control, npart, kitNumber, PART::engine::subSynth, insert);
        }
    }

    float value = -1;
    if (cmd == -1)
    {
        if (matchnMove(2, point, "band"))
        {
            if (matchnMove(1, point, "width"))
                cmd = SUBSYNTH::control::bandwidth;
            else if (matchnMove(1, point, "scale"))
                cmd = SUBSYNTH::control::bandwidthScale;
            else if (matchnMove(1, point, "envelope"))
            {
                value = (toggle() == 1);
                cmd = SUBSYNTH::control::enableBandwidthEnvelope;
            }
        }
        else if (matchnMove(2, point, "frequency"))
        {
            if (matchnMove(1, point, "envelope"))
            {
                value = (toggle() == 1);
                cmd = SUBSYNTH::control::enableFrequencyEnvelope;
            }
        }
        else if (matchnMove(2, point, "filter"))
        {
            value = (toggle() == 1);
            cmd = SUBSYNTH::control::enableFilter;
        }

    }

    if (cmd != -1)
    {
        //cout << "control " << int(cmd) << "  part " << int(npart) << "  kit " << int(kitNumber) << "  engine " << int(PART::engine::subSynth) << endl;
        if (value == -1)
        {
            if (lineEnd(controlType))
                return value_msg;
            value = string2int(point);
        }
        return sendNormal(value, controlType, cmd, npart, kitNumber, PART::engine::subSynth);
    }
    return available_msg;
}


int CmdInterface::padSynth(unsigned char controlType)
{
    if (matchnMove(2, point, "waveform"))
    {
        bitSet(context, LEVEL::Oscillator);
        return waveform(controlType);
    }
    float value = -1;
    int cmd;
    if (lineEnd(controlType))
        return done_msg;
    int result = partCommonControls(controlType);
    if (result != todo_msg)
        return result;

    if (matchnMove(2, point, "apply"))
    {
        value = 0; // dummy
        cmd = PADSYNTH::control::applyChanges;
    }

    return sendNormal(value, controlType, cmd, npart, kitNumber, PART::engine::padSynth);
    return available_msg;
}


int CmdInterface::waveform(unsigned char controlType)
{
    if (lineEnd(controlType))
        return done_msg;
    float value = -1;
    int cmd = -1;
    int engine = contextToEngines();
    unsigned char insert;

    if (matchnMove(2, point, "harmonic"))
    {
        if (lineEnd(controlType))
            return value_msg;
        cmd = string2int(point);
        if (cmd < 1 || cmd > MAX_AD_HARMONICS)
            return range_msg;
        point = skipChars(point);

        if (matchnMove(1, point, "amp"))
            insert = TOPLEVEL::insert::harmonicAmplitude;
        else if (matchnMove(1, point, "phase"))
            insert = TOPLEVEL::insert::harmonicPhaseBandwidth;
        else
            return opp_msg;

        if (lineEnd(controlType))
            return value_msg;
        return sendNormal(string2int(point), controlType, cmd - 1, npart, kitNumber, engine, insert);
    }

    insert = TOPLEVEL::insert::oscillatorGroup;
    if (matchnMove(2, point, "shape"))
    {
        if (matchnMove(2, point, "sine"))
            value = 0;
        else if (matchnMove(2, point, "triange"))
            value = 1;
        else if (matchnMove(2, point, "pulse"))
            value = 2;
        else if (matchnMove(2, point, "saw"))
            value = 3;
        else if (matchnMove(2, point, "power"))
            value = 4;
        else if (matchnMove(2, point, "gauss"))
            value = 5;
        else if (matchnMove(2, point, "diode"))
            value = 6;
        else if (matchnMove(2, point, "absine"))
            value = 7;
        else if (matchnMove(2, point, "psine"))
            value = 8;
        else if (matchnMove(2, point, "ssine"))
            value = 9;
        else if (matchnMove(3, point, "chirp"))
            value = 10;
        else if (matchnMove(2, point, "asine"))
            value = 11;
        else if (matchnMove(3, point, "chebyshev"))
            value = 12;
        else if (matchnMove(2, point, "square"))
            value = 13;
        else if (matchnMove(2, point, "spike"))
            value = 14;
        else if (matchnMove(2, point, "circle"))
            value = 15;
        if (value > -1)
            cmd = OSCILLATOR::control::baseFunctionType;
    }
    else if (matchnMove(2, point, "clear"))
    {
        value = 0; // dummy
        cmd = OSCILLATOR::control::clearHarmonics;
    }
    else if (matchnMove(2, point, "apply"))
    {
        if (engine != PART::engine::padSynth)
            return available_msg;
        value = 0; // dummy
        insert = UNUSED;
        cmd = PADSYNTH::control::applyChanges;
    }
    if (cmd == -1)
        return available_msg;
    return sendNormal(value, controlType, cmd, npart, kitNumber, engine, insert);
}


int CmdInterface::commandPart(bool justSet, unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    int tmp;

    if (lineEnd(controlType))
        return done_msg;
    if (justSet || isdigit(point[0]))
    {
        tmp = string2int127(point);
        if (tmp > 0)
        {
            tmp -= 1;
            if (tmp >= Runtime.NumAvailableParts)
            {
                Runtime.Log("Part number too high");
                return done_msg;
            }
            point = skipChars(point);
            if (npart != tmp)
            {
                npart = tmp;
                if (controlType == TOPLEVEL::type::Write)
                {
                    context = LEVEL::Top;
                    bitSet(context, LEVEL::Part);
                    kitMode = PART::kitType::Off;
                    kitNumber = 0;
                    sendDirect(npart, TOPLEVEL::type::Write, MAIN::control::partNumber, TOPLEVEL::section::main);
                }
            }
            if (lineEnd(controlType))
                return done_msg;
        }
    }

    if (bitTest(context, LEVEL::AllFX))
        return effects(controlType);

    if (matchnMove(3, point, "addsynth"))
    {
        bitSet(context, LEVEL::AddSynth);
        insertType = TOPLEVEL::insertType::amplitude;
        return addSynth(controlType);
    }

    if (matchnMove(3, point, "subsynth"))
    {
        bitSet(context, LEVEL::SubSynth);
        insertType = TOPLEVEL::insertType::amplitude;
        return subSynth(controlType);
    }

    if (matchnMove(3, point, "padsynth"))
    {
        bitSet(context, LEVEL::PadSynth);
        insertType = TOPLEVEL::insertType::amplitude;
        return padSynth(controlType);
    }

    tmp = partCommonControls(controlType);
    if (tmp != todo_msg)
        return tmp;

    if (matchnMove(2, point, "effects") || matchnMove(2, point, "efx"))
    {
        context = LEVEL::Top;
        bitSet(context, LEVEL::AllFX);
        bitSet(context, LEVEL::Part);
        return effects(controlType);
    }

    if (matchnMove(2, point, "kmode"))
    {
        if (matchnMove(2, point, "off"))
            kitMode = PART::kitType::Off;
        else if(matchnMove(2, point, "multi"))
            kitMode = PART::kitType::Multi;
        else if(matchnMove(2, point, "single"))
            kitMode = PART::kitType::Single;
        else if(matchnMove(2, point, "crossfade"))
            kitMode = PART::kitType::CrossFade;
        else if (controlType == TOPLEVEL::type::Write)
            return value_msg;
        sendDirect(kitMode, controlType, PART::control::kitMode, npart);
        kitNumber = 0;
        voiceNumber = 0; // must clear this too!
        return done_msg;
    }
    if (kitMode == PART::kitType::Off)
        kitNumber = UNUSED; // always clear it if not kit mode
    else
    {
        if (matchnMove(2, point, "kitem"))
        {
            if (controlType == TOPLEVEL::type::Write)
            {
                if (lineEnd(controlType))
                    return value_msg;
                int tmp = string2int(point);
                if (tmp < 1 || tmp > NUM_KIT_ITEMS)
                    return range_msg;
                kitNumber = tmp - 1;
                voiceNumber = 0;// to avoid confusion
            }
            Runtime.Log("Kit item number " + to_string(kitNumber + 1));
            return done_msg;
        }
    }
    if (kitMode)
    {
        int value;
        if (matchnMove(2, point, "drum"))
        {
            value = toggle();
            sendDirect((value == 1), controlType, PART::control::drumMode, npart);
            return done_msg;
        }
        if (matchnMove(2, point, "mute"))
        {
            value = toggle();
            sendDirect((value == 1), controlType, PART::control::kitItemMute, npart, kitNumber, UNUSED, TOPLEVEL::insert::kitGroup);
            return done_msg;
        }
        if (matchnMove(2, point,"keffect"))
        {
            if (controlType == TOPLEVEL::type::Write && point[0] == 0)
                return value_msg;
            value = string2int(point);
            if (value < 0 || value > 3)
                return range_msg;
            sendDirect(value, controlType, PART::control::kitEffectNum, npart, kitNumber, UNUSED, TOPLEVEL::insert::kitGroup);
            return done_msg;
        }
    }

    if (matchnMove(2, point, "shift"))
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        int value = string2int(point);
        if (value < MIN_KEY_SHIFT)
            value = MIN_KEY_SHIFT;
        else if(value > MAX_KEY_SHIFT)
            value = MAX_KEY_SHIFT;
        sendDirect(value, controlType, PART::control::keyShift, npart, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
        return done_msg;
    }

    if (matchnMove(2, point, "offset"))
    {
        tmp = string2int127(point);
        if(controlType == TOPLEVEL::type::Write && tmp < 1)
            return value_msg;
        sendDirect(tmp, controlType, PART::control::velocityOffset, npart);
    }

    if (matchnMove(2, point, "program") || matchnMove(1, point, "instrument"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            Runtime.Log("Part name is " + synth->part[npart]->Pname);
            return done_msg;
        }
        if (matchnMove(2, point, "clear"))
        {
            sendDirect(0, controlType, PART::control::defaultInstrument, npart, UNUSED, UNUSED, UNUSED, UNUSED, tmp);
            return done_msg;
        }
        if (point[0] != 0) // force part not channel number
        {
            tmp = string2int(point) - 1;
            if (tmp < 0 || tmp > 159)
                return range_msg;
            sendDirect(npart, controlType, MAIN::control::loadInstrument, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, tmp);
            return done_msg;
        }
        else
            return value_msg;
    }
    if (matchnMove(1, point, "channel"))
    {
        tmp = string2int127(point);
        if(controlType == TOPLEVEL::type::Write && tmp < 1)
            return value_msg;
        tmp -= 1;
        sendDirect(tmp, controlType, PART::control::midiChannel, npart);
        return done_msg;
    }
    if (matchnMove(1, point, "destination"))
    {
        int dest = 0;
        if (controlType == TOPLEVEL::type::Write)
        {
            if (matchnMove(1, point, "main"))
                dest = 1;
            else if (matchnMove(1, point, "part"))
                dest = 2;
            else if (matchnMove(1, point, "both"))
                dest = 3;
            if (dest == 0)
                return range_msg;
        }
        sendDirect(dest, controlType, PART::control::audioDestination, npart, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::adjustAndLoopback);
        return done_msg;
    }
    if (matchnMove(1, point, "breath"))
    {
        sendDirect((toggle() == 1), controlType, PART::control::breathControlEnable, npart);
        return done_msg;
    }
    if (matchnMove(1, point, "note"))
    {
        int value = 0;
        if(controlType == TOPLEVEL::type::Write)
        {
            if (lineEnd(controlType))
                return value_msg;
            value = string2int(point);
            if (value < 1 || (value > POLIPHONY - 20))
                return range_msg;
        }
        sendDirect(value, controlType, 33, npart);
        return done_msg;
    }

    if (matchnMove(1, point, "mode"))
    {
        int value = 0;
        if(controlType == TOPLEVEL::type::Write)
        {
            if (matchnMove(1, point, "poly"))
                value = 0;
            else if (matchnMove(1, point, "mono"))
                value = 1;
            else if (matchnMove(1, point, "legato"))
                value = 2;
            else
                return name_msg;
        }
        sendDirect(value, controlType, 6, npart);
        return done_msg;
    }
    if (matchnMove(2, point, "portamento"))
    {
        sendDirect((toggle() == 1), controlType, PART::control::portamento, npart);
        return done_msg;
    }
    if (matchnMove(2, point, "name"))
    {
        string name;
        unsigned char par2 = NO_MSG;
        if (controlType == TOPLEVEL::type::Write)
        {
            name = (string) point;
            if (name.size() < 3)
            {
                Runtime.Log("Name too short");
                return done_msg;
            }
            else if ( name == "Simple Sound")
            {
                Runtime.Log("Cant use name of default sound");
                return done_msg;
            }
            else
                par2 = miscMsgPush(name);
        }
        sendDirect(0, controlType, PART::control::instrumentName, npart, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, par2, UNUSED);
        return done_msg;
    }
    return opp_msg;
}


int CmdInterface::commandReadnSet(unsigned char controlType)
{
    Config &Runtime = synth->getRuntime();
    string name;
    if (matchnMove(2, point, "yoshimi"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            //Runtime.Log("Instance " + asString(currentInstance), 1);
            Runtime.Log("Instance " + to_string(synth->getUniqueId()));
            return done_msg;
        }
        if (lineEnd(controlType))
            return value_msg;
        currentInstance = string2int(point);
        synth = firstSynth->getSynthFromId(currentInstance);
        unsigned int newID = synth->getUniqueId();
        if (newID != currentInstance)
        {
            Runtime.Log("Instance " + to_string(currentInstance) + " not found. Set to " + to_string(newID), 1);
            currentInstance = newID;
        }
        defaults();
        return done_msg;
    }

 // these must all be highest (relevant) bit first
    if (bitTest(context, LEVEL::Config))
        return commandConfig(controlType);
    if (bitTest(context, LEVEL::Scale))
        return commandScale(controlType);
    if (bitTest(context, LEVEL::Envelope))
        return envelopeSelect(controlType);
    if (bitTest(context, LEVEL::Filter))
        return filterSelect(controlType);
    if (bitTest(context, LEVEL::LFO))
        return LFOselect(controlType);
    if (bitTest(context, LEVEL::Oscillator))
        return waveform(controlType);
    if (bitTest(context, LEVEL::AddVoice))
        return addVoice(controlType);
    if (bitTest(context, LEVEL::AddSynth))
        return addSynth(controlType);
    if (bitTest(context, LEVEL::SubSynth))
        return subSynth(controlType);
    if (bitTest(context, LEVEL::PadSynth))
        return padSynth(controlType);
    if (bitTest(context, LEVEL::Part))
        return commandPart(false, controlType);
    if (bitTest(context, LEVEL::Vector))
        return commandVector(controlType);
    if (bitTest(context, LEVEL::Learn))
        return commandMlearn(controlType);

    if (matchnMove(2, point, "config"))
    {
        context = LEVEL::Top;
        bitSet(context, LEVEL::Config);
        return commandConfig(controlType);
    }

    if (matchnMove(1, point, "scale"))
    {
        context = LEVEL::Top;
        bitSet(context, LEVEL::Scale);
        return commandScale(controlType);
    }

    if (matchnMove(1, point, "part"))
    {
        nFX = 0; // effects number limit changed
        if (controlType != TOPLEVEL::type::Write && point[0] == 0)
        {
            if (synth->partonoffRead(npart))
                name = " enabled";
            else
                name = " disabled";
            Runtime.Log("Current part " + asString(npart) + name, 1);
            return done_msg;
        }
        context = LEVEL::Top;
        bitSet(context, LEVEL::Part);
        nFXtype = synth->part[npart]->partefx[nFX]->geteffect();
        return commandPart(true, controlType);
    }

    if (matchnMove(2, point, "vector"))
    {
        context = LEVEL::Top;
        return commandVector(controlType);
    }

    if (matchnMove(2, point, "mlearn"))
    {
        context = LEVEL::Top;
        return commandMlearn(controlType);
    }

    if ((context == LEVEL::Top || bitTest(context, LEVEL::InsFX)) && matchnMove(3, point, "system"))
    {
        bitSet(context,LEVEL::AllFX);
        bitClear(context, LEVEL::InsFX);
        nFX = 0; // effects number limit changed
        matchnMove(2, point, "effects"); // clear it if given
        matchnMove(2, point, "efx");
        nFXtype = synth->sysefx[nFX]->geteffect();
        return effects(controlType);
    }
    if ((context == LEVEL::Top || bitTest(context, LEVEL::AllFX)) && !bitTest(context, LEVEL::Part) && matchnMove(3, point, "insert"))
    {
        bitSet(context,LEVEL::AllFX);
        bitSet(context,LEVEL::InsFX);
        nFX = 0; // effects number limit changed
        matchnMove(2, point, "effects"); // clear it if given
        matchnMove(2, point, "efx");
        nFXtype = synth->insefx[nFX]->geteffect();
        return effects(controlType);
    }
    if (bitTest(context, LEVEL::AllFX))
        return effects(controlType);

    if (matchnMove(1, point, "root"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            Runtime.Log("Root is ID " + asString(synth->ReadBankRoot()), 1);
            return done_msg;
        }
        if (point[0] != 0)
        {
            sendDirect(255, controlType, 8, TOPLEVEL::section::midiIn, 0, UNUSED, string2int(point), TOPLEVEL::route::adjustAndLoopback);
            return done_msg;
        }
        else
            return value_msg;
    }

    if (matchnMove(1, point, "bank"))
    {
        if (controlType != TOPLEVEL::type::Write)
        {
            Runtime.Log("Bank is ID " + asString(synth->ReadBank()), 1);
            return done_msg;
        }
        if (point[0] != 0)
        {
            sendDirect(255, TOPLEVEL::type::Write, 8, TOPLEVEL::section::midiIn, 0, string2int(point), UNUSED, TOPLEVEL::route::adjustAndLoopback);
            return done_msg;
        }
        else
            return value_msg;
    }

    if (matchnMove(1, point, "volume"))
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        sendDirect(string2int127(point), controlType, MAIN::control::volume, TOPLEVEL::section::main);
        return done_msg;
    }
    if (matchnMove(2, point, "detune"))
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        sendDirect(string2int127(point), controlType, MAIN::control::detune, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
        return done_msg;
    }

    if (matchnMove(2, point, "shift"))
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        int value = string2int(point);
        if (value < MIN_KEY_SHIFT)
            value = MIN_KEY_SHIFT;
        else if(value > MAX_KEY_SHIFT)
            value = MAX_KEY_SHIFT;
        sendDirect(value, controlType, MAIN::control::keyShift, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
        return done_msg;
    }

    if (matchnMove(2, point, "solo"))
    {
        int value = 0; // disable

        if (matchnMove(2, point, "cc"))
        {
            if (controlType == TOPLEVEL::type::Write)
            {
                if (lineEnd(controlType))
                    return value_msg;
                value = string2int127(point);
                string otherCC = Runtime.masterCCtest(value);
                if (otherCC > "")
                {
                    Runtime.Log("In use for " + otherCC);
                    return done_msg;
                }
            }
            sendDirect(value, controlType, 49, TOPLEVEL::section::main);
            return done_msg;
        }

        else if (matchnMove(1, point, "row"))
            value = 1;
        else if (matchnMove(1, point, "column"))
            value = 2;
        else if (matchnMove(1, point, "loop"))
            value = 3;
        else if (matchnMove(1, point, "twoway"))
            value = 4;
        sendDirect(value, controlType, 48, TOPLEVEL::section::main);
        return done_msg;
    }
    if (matchnMove(2, point, "available")) // 16, 32, 64
    {
        if (controlType == TOPLEVEL::type::Write && point[0] == 0)
            return value_msg;
        int value = string2int(point);
        if (value != 16 && value != 32 && value != 64)
            return range_msg;
        sendDirect(value, controlType, 15, TOPLEVEL::section::main);
        return done_msg;
    }

        return opp_msg;
}


int CmdInterface::cmdIfaceProcessCommand(char *cCmd)
{
    // in case it's been changed from elsewhere
    synth = firstSynth->getSynthFromId(currentInstance);
    unsigned int newID = synth->getUniqueId();
    if (newID != currentInstance)
    {
        currentInstance = newID;
        defaults();
    }

    Config &Runtime = synth->getRuntime();

    int tmp;
    point = cCmd;
    point = skipSpace(point); // just to be sure
    tmp = strlen(cCmd) - 1;
    while (point[tmp] < '!' && tmp > 0)
    {
        point[tmp] = 0; // also trailing spaces
        -- tmp;
    }

    list<string> msg;

    findStatus(false);

#ifdef REPORT_NOTES_ON_OFF
    if (matchnMove(3, point, "report")) // note test
    {
        cout << "note on sent " << Runtime.noteOnSent << endl;
        cout << "note on seen " << Runtime.noteOnSeen << endl;
        cout << "note off sent " << Runtime.noteOffSent << endl;
        cout << "note off seen " << Runtime.noteOffSeen << endl;
        cout << "notes hanging sent " << Runtime.noteOnSent - Runtime.noteOffSent << endl;
        cout << "notes hanging seen " << Runtime.noteOnSeen - Runtime.noteOffSeen << endl;
        return done_msg;
    }
#endif
    if (matchnMove(2, point, "exit"))
    {
        if (currentInstance > 0)
        {
            Runtime.Log("Can only exit from instance 0", 1);
            return done_msg;
        }
        string message;
        if (Runtime.configChanged)
            message = "System config has been changed. Still exit";
        else
            message = "All data will be lost. Still exit";
        if (query(message, false))
        {
            // this seems backwards but it *always* saves.
            // seeing configChanged makes it reload the old config first.
            Runtime.runSynth = false;
            return exit_msg;
        }
        return done_msg;
    }
    if (point[0] == '/')
    {
        ++ point;
        point = skipSpace(point);
        context = LEVEL::Top;
        if (point[0] == 0)
            return done_msg;
    }

    if (matchnMove(3, point, "reset"))
    {
        int control = MAIN::control::masterReset;
        if (matchnMove(3, point, "all"))
            control = MAIN::control::masterResetAndMlearn;
        if (query("Restore to basic settings", false))
        {
            sendDirect(0, TOPLEVEL::type::Write, control, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::adjustAndLoopback);
            context = LEVEL::Top;
        }
        return done_msg;
    }
    if (point[0] == '.' && point[1] == '.')
    {
        point += 2;
        point = skipSpace(point);

        if (bitFindHigh(context) == LEVEL::AllFX || bitFindHigh(context) == LEVEL::InsFX)
            context = LEVEL::Top;
        else if (bitFindHigh(context) == LEVEL::Part && (bitTest(context, LEVEL::AllFX) || bitTest(context, LEVEL::InsFX)))
        {
            context = LEVEL::Top;
            bitSet(context, LEVEL::Part); // restore part level
        }
        else
        {
            bitClearHigh(context);
        }
        if (point[0] == 0)
            return done_msg;
    }

    if (helpList(context))
        return done_msg;

    if (matchnMove(2, point, "stop"))
    {
        sendDirect(0, TOPLEVEL::type::Write,MAIN::control::stopSound, TOPLEVEL::section::main);
        return done_msg;
    }
    if (matchnMove(1, point, "list"))
        return commandList();

    if (matchnMove(3, point, "run"))
    {
        string filename = string(point);
        if (filename > "!")
        {
            char *to_send;
            char *mark;
            to_send = (char*) malloc(0xff);;
            int count = 0;
            bool isok = true;

            FILE *readfile = fopen(filename.c_str(), "r");
            if (readfile)
            {
                context = LEVEL::Top; // start from top level
                while (!feof(readfile) && isok)
                {
                    if(fgets(to_send , 0xff , readfile))
                    {
                        ++ count;
                        mark = skipSpace(to_send);
                        if ( mark[0] < ' ' || mark [0] == '#')
                            continue;
                        if (matchnMove(3, mark, "run"))
                        {
                            isok = false;
                            synth->getRuntime().Log("*** Error: scripts are not recursive @ line " + to_string(count) + " ***");
                            continue;
                        }
                        reply = cmdIfaceProcessCommand(mark);
                        if (reply > done_msg)
                        {
                            isok = false;
                            synth->getRuntime().Log("*** Error: " + replies[reply] + " @ line " + to_string(count) + " ***");
                        }
                    }
                }
                cout << "here" << endl;
                fclose (readfile);
            }
            else
                synth->getRuntime().Log("Can't read file " + filename);
            free (to_send);
            to_send = NULL;
            context = LEVEL::Top; // leave it tidy
            return done_msg;
        }
        replyString = "Exec";
        return what_msg;
    }

    if (matchnMove(1, point, "set"))
    {
        if (point[0] != 0)
            return commandReadnSet(TOPLEVEL::type::Write);
        else
        {
            replyString = "set";
            return what_msg;
        }
    }

    if (matchnMove(1, point, "read") || matchnMove(1, point, "get"))
    {
        if (point[0] != 0)
            return commandReadnSet(TOPLEVEL::type::Read);
        else
        {
            replyString = "read";
            return what_msg;
        }
    }

    if (matchnMove(3, point, "minimum"))
    {
        if (point[0] != 0)
            return commandReadnSet(TOPLEVEL::type::Minimum | TOPLEVEL::type::Limits);
        else
        {
            replyString = "minimum";
            return what_msg;
        }
    }

    if (matchnMove(3, point, "maximum"))
    {
        if (point[0] != 0)
            return commandReadnSet(TOPLEVEL::type::Maximum | TOPLEVEL::type::Limits);
        else
        {
            replyString = "maximum";
            return what_msg;
        }
    }

    if (matchnMove(3, point, "default"))
    {
        if (point[0] != 0)
            return commandReadnSet(TOPLEVEL::type::Default | TOPLEVEL::type::Limits);
        else
        {
            replyString = "default";
            return what_msg;
        }
    }

    if (matchnMove(2, point, "mlearn"))
    {
        if (point[0] != 0)
            return commandReadnSet(TOPLEVEL::type::LearnRequest);
        else
        {
            replyString = "mlearn";
            return what_msg;
        }
    }

    if (matchnMove(3, point, "add"))
    {
        if (matchnMove(1, point, "root"))
        {
            int found = synth->getBankRef().addRootDir(point);
            if (!found)
            {
                Runtime.Log("Can't find path " + (string) point);
            }
            else
            {
                GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePaths, 0);
                Runtime.Log("Added new root ID " + asString(found) + " as " + (string) point);
                synth->saveBanks();
            }
            return done_msg;
        }
        if (matchnMove(1, point, "bank"))
        {
            int slot;
            for (slot = 0; slot < MAX_BANKS_IN_ROOT; ++slot)
            {
                if (synth->getBankRef().getBankName(slot).empty())
                    break;
            }
            if (!synth->getBankRef().newIDbank(point, (unsigned int)slot))
            {
                Runtime.Log("Could not create bank " + (string) point + " for ID " + asString(slot));
            }

            Runtime.Log("Created  new bank " + (string) point + " with ID " + asString(slot));
            GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePaths, 0);
            return done_msg;
        }
        if (matchnMove(2, point, "yoshimi"))
        {
            if (currentInstance !=0)
            {
                Runtime.Log("Only instance 0 can start others");
                return done_msg;
            }
            int forceId = string2int(point);
            if (forceId < 1 || forceId >= 32)
                forceId = 0;
            sendDirect(forceId, TOPLEVEL::type::Write, MAIN::control::startInstance, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
            return done_msg;
        }
        else
        {
            replyString = "add";
            return what_msg;
        }
    }
    if (matchWord(3, point, "import") || matchWord(3, point, "export") )
    { // need the double test to find which then move along line
        int type = 0;
        if (matchnMove(3, point, "import"))
            type = MAIN::control::importBank;
        else if (matchnMove(3, point, "export"))
            type = MAIN::control::exportBank;
        int root = UNUSED;
        if (matchnMove(1, point, "root"))
        {
            if (isdigit(point[0]))
            {
                root = string2int(point);
                point = skipChars(point);
            }
            else
                root = 200; // force invalid root error
        }
        int value = string2int(point);
        point = skipChars(point);
        string name = string(point);
        if (root < 0 || (root > 127 && root != UNUSED) || value < 0 || value > 127 || name <="!")
        {
            if (type == MAIN::control::importBank)
                replyString = "import";
            else
                replyString = "export";
            return value_msg;
        }
        else
        {
            sendDirect(value, TOPLEVEL::type::Write, type, TOPLEVEL::section::main, root, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(name));
            return done_msg;
        }
    }

    if (matchnMove(3, point, "remove"))
    {
        if  (matchnMove(1, point, "root"))
        {
            if (isdigit(point[0]))
            {
                int rootID = string2int(point);
                if (rootID >= MAX_BANK_ROOT_DIRS)
                    return range_msg;
                else
                {
                    string rootname = synth->getBankRef().getRootPath(rootID);
                    if (rootname.empty())
                        Runtime.Log("Can't find path " + asString(rootID));
                    else
                    {
                        synth->getBankRef().removeRoot(rootID);
                        GuiThreadMsg::sendMessage(synth, GuiThreadMsg::UpdatePaths, 0);
                        Runtime.Log("Un-linked " + rootname);
                        synth->saveBanks();
                    }
                    return done_msg;
                }
            }
            else
                return value_msg;
        }
        if (matchnMove(1, point, "bank"))
        {
            int rootID = UNUSED;
            if (matchnMove(1, point, "root"))
            {
                if (isdigit(point[0]))
                    rootID = string2int(point);
                if (rootID >= MAX_BANK_ROOT_DIRS)
                    return range_msg;
            }
            if (isdigit(point[0]))
            {
                point = skipChars(point);
                int bankID = string2int(point);
                if (bankID >= MAX_BANKS_IN_ROOT)
                    return range_msg;
                else
                {
                    string filename = synth->getBankRef().getBankName(bankID);
                    if (filename.empty())
                        Runtime.Log("No bank at this location");
                    else
                    {
                        tmp = synth->getBankRef().getBankSize(bankID);
                        if (tmp)
                        {
                            Runtime.Log("Bank " + filename + " has " + asString(tmp) + " Instruments");
                            if (query("Delete bank and all of these", false))
                                tmp = 0;
                            else
                                Runtime.Log("Aborted");
                        }
                        if (tmp == 0)
                        {
                            sendDirect(bankID, TOPLEVEL::type::Write, MAIN::control::deleteBank, TOPLEVEL::section::main, rootID, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
                        }
                    }

                }
                return done_msg;
            }
            else
                return value_msg;
        }
        if(matchnMove(2, point, "yoshimi"))
        {
            if (point[0] == 0)
            {
                replyString = "remove";
                return what_msg;
            }
            else
            {
                unsigned int to_close = string2int(point);
                if (to_close == 0)
                    Runtime.Log("Use 'Exit' to close main instance");
                else if (to_close == currentInstance)
                    Runtime.Log("Instance can't close itself");
                else
                {
                    sendDirect(to_close, TOPLEVEL::type::Write, MAIN::control::stopInstance, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority);
                }
                return done_msg;
            }
        }
        if (matchnMove(2, point, "mlearn"))
        {
            if (matchnMove(3, point, "all"))
            {
                sendNormal(0, 0, MIDILEARN::control::clearAll, TOPLEVEL::section::midiLearn);
                return done_msg;
            }
            else if (point[0] == '@')
            {
                point += 1;
                point = skipSpace(point);
                tmp = string2int(point);
                if (tmp == 0)
                    return value_msg;
                sendNormal(tmp - 1, 0, MIDILEARN::control::deleteLine, TOPLEVEL::section::midiLearn);
                return done_msg;
            }
        }
        replyString = "remove";
        return what_msg;
    }

    else if (matchnMove(2, point, "load"))
    {
        if(matchnMove(2, point, "mlearn"))
        {
            if (point[0] == '@')
            {
                point += 1;
                tmp = string2int(point);
                if (tmp == 0)
                    return value_msg;
                sendNormal(0, TOPLEVEL::type::Write, MIDILEARN::control::loadFromRecent, TOPLEVEL::section::midiLearn, 0, 0, 0, 0, tmp - 1);
                return done_msg;
            }
            if ((string) point == "")
                return name_msg;
            sendNormal(0, TOPLEVEL::type::Write, MIDILEARN::control::loadList, TOPLEVEL::section::midiLearn, 0, 0, 0, 0, miscMsgPush((string) point));
            return done_msg;
        }
        if(matchnMove(2, point, "vector"))
        {
            string loadChan;
            unsigned char ch;
            if(matchnMove(1, point, "channel"))
            {
                ch = string2int127(point);
                if (ch > 0)
                {
                    ch -= 1;
                    point = skipChars(point);
                }
                else
                    ch = chan;
                loadChan = "channel " + asString(ch + 1);
            }
            else
            {
                ch = UNUSED;
                loadChan = "source channel";
            }
            if (ch != UNUSED && ch >= NUM_MIDI_CHANNELS)
                return range_msg;
            if (point[0] == 0)
                return name_msg;
            string name;
            if (point[0] == '@')
            {
                point += 1;
                point = skipSpace(point);
                tmp = string2int(point);
                if (tmp <= 0)
                    return value_msg;
                name = historySelect(5, tmp - 1);
                if (name == "")
                    return done_msg;
            }
            else
            {
                name = (string)point;
                if (name == "")
                    return name_msg;
            }
            sendDirect(0, TOPLEVEL::type::Write, MAIN::control::loadNamedVector, TOPLEVEL::section::main, UNUSED, UNUSED, ch, TOPLEVEL::route::adjustAndLoopback, miscMsgPush(name));
            return done_msg;
        }
        if(matchnMove(2, point, "state"))
        {
            if (point[0] == 0)
                return name_msg;
            string name;
            if (point[0] == '@')
            {
                point += 1;
                point = skipSpace(point);
                tmp = string2int(point);
                if (tmp <= 0)
                    return value_msg;
                name = historySelect(4, tmp - 1);
                if (name == "")
                    return done_msg;
            }
            else
            {
                name = (string)point;
                if (name == "")
                        return name_msg;
            }
            sendDirect(0, TOPLEVEL::type::Write, MAIN::control::loadNamedState, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::adjustAndLoopback, miscMsgPush(name));
            return done_msg;
        }
        if (matchnMove(2, point, "scale"))
        {
            if (point[0] == 0)
                return name_msg;
            string name;
            if (point[0] == '@')
            {
                point += 1;
                point = skipSpace(point);
                tmp = string2int(point);
                if (tmp <= 0)
                    return value_msg;
                name = historySelect(3, tmp - 1);
                if (name == "")
                    return done_msg;
            }
            else
            {
                name = (string)point;
                if (name == "")
                    return name_msg;
            }
            sendDirect(0, TOPLEVEL::type::Write, MAIN::control::loadNamedScale, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(name));
            return done_msg;
        }
        if (matchnMove(1, point, "patchset"))
        {
            if (point[0] == 0)
                return name_msg;
            string name;
            if (point[0] == '@')
            {
                point += 1;
                point = skipSpace(point);
                tmp = string2int(point);
                if (tmp <= 0)
                    return value_msg;
                name = historySelect(2, tmp - 1);
                if (name == "")
                    return done_msg;
            }
            else
            {
                name = (string)point;
                if (name == "")
                    return name_msg;
            }
            sendDirect(0, TOPLEVEL::type::Write, MAIN::control::loadNamedPatchset, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::adjustAndLoopback, miscMsgPush(name));
            return done_msg;
        }
        if (matchnMove(1, point, "instrument"))
        {
            if (point[0] == 0)
                return name_msg;
            string name;
            if (point[0] == '@')
            {
                point += 1;
                point = skipSpace(point);
                tmp = string2int(point);
                if (tmp <= 0)
                    return value_msg;
                name = historySelect(1, tmp - 1);
                if (name == "")
                    return done_msg;
            }
            else
            {
                name = (string)point;
                if (name == "")
                    return name_msg;
            }

            sendDirect(npart, TOPLEVEL::type::Write, MAIN::control::loadNamedInstrument, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, UNUSED, miscMsgPush(name));
            return done_msg;
        }
        replyString = "load";
        return what_msg;
    }

    if (matchnMove(2, point, "save"))
    {
        if(matchnMove(2, point, "mlearn"))
        {
            if (point[0] == 0)
                return name_msg;

            sendNormal(0, TOPLEVEL::type::Write, MIDILEARN::control::saveList, TOPLEVEL::section::midiLearn, 0, 0, 0, 0, miscMsgPush((string) point));
            return done_msg;
        }
        if(matchnMove(2, point, "vector"))
        {
            tmp = chan;
            if(matchnMove(1, point, "channel"))
            {
                tmp = string2int127(point) - 1;
                point = skipChars(point);
            }
            if (tmp >= NUM_MIDI_CHANNELS || tmp < 0)
                return range_msg;
            if (point[0] == 0)
                return name_msg;
            chan = tmp;
            sendDirect(0, TOPLEVEL::type::Write, MAIN::control::saveNamedVector, TOPLEVEL::section::main, UNUSED, UNUSED, chan, TOPLEVEL::route::lowPriority, miscMsgPush((string) point));
            return done_msg;
        }
        if(matchnMove(2, point, "state"))
        {
            if (point[0] == 0)
                return value_msg;
            sendDirect(0, TOPLEVEL::type::Write, MAIN::control::saveNamedState, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(string(point)));
            return done_msg;
        }
        if(matchnMove(1, point, "config"))
        {
            sendDirect(0, TOPLEVEL::type::Write, CONFIG::control::saveCurrentConfig, TOPLEVEL::section::config, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush("DUMMY"));
            return done_msg;
        }

        if (matchnMove(2, point, "scale"))
        {
            if (point[0] == 0)
                return name_msg;
            sendDirect(0, TOPLEVEL::type::Write, MAIN::control::saveNamedScale, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(string(point)));
            return done_msg;
        }
        else if (matchnMove(1, point, "patchset"))
        {
            if (point[0] == 0)
                return name_msg;
            sendDirect(0, TOPLEVEL::type::Write, MAIN::control::saveNamedPatchset, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(string(point)));
            return done_msg;
        }
        if (matchnMove(1, point, "instrument"))
        {
            if (synth->part[npart]->Pname == "Simple Sound")
            {
                Runtime.Log("Nothing to save!");
                return done_msg;
            }
            if (point[0] == 0)
                return name_msg;
            sendDirect(npart, TOPLEVEL::type::Write, MAIN::control::saveNamedInstrument, TOPLEVEL::section::main, UNUSED, UNUSED, UNUSED, TOPLEVEL::route::lowPriority, miscMsgPush(string(point)));
            return done_msg;
        }
        replyString = "save";
        return what_msg;
    }

    if (matchnMove(6, point, "direct"))
    {
        unsigned char request;
        float value;
        unsigned char type = 0;
        if (matchnMove(3, point, "limits"))
        {
            value = 0;
            type = TOPLEVEL::type::Limits;
            if (matchnMove(3, point, "min"))
                request = TOPLEVEL::type::Minimum;
            else if (matchnMove(3, point, "max"))
                request = TOPLEVEL::type::Maximum;
            else if (matchnMove(3, point, "default"))
                request = TOPLEVEL::type::Default;
            else request = UNUSED;
        }
        else
        {
            request = UNUSED;
            value = string2float(point);
            if (strchr(point, '.') == NULL)
                type |= TOPLEVEL::type::Integer;
            point = skipChars(point);
            type |= (string2int127(point) & 0x43); // Allow 'pretend' and MIDI learn
            point = skipChars(point);
        }
        type |= TOPLEVEL::source::CLI;
        unsigned char control = string2int(point);
        point = skipChars(point);
        unsigned char part = string2int(point);
        point = skipChars(point);
        unsigned char kit = UNUSED;
        unsigned char engine = UNUSED;
        unsigned char insert = UNUSED;
        unsigned char param = UNUSED;
        unsigned char par2 = UNUSED;
        if (point[0] != 0)
        {
            kit = string2int(point);
            point = skipChars(point);
            if (point[0] != 0)
            {
                engine = string2int(point);
                point = skipChars(point);
                if (point[0] != 0)
                {
                    insert = string2int(point);
                    point = skipChars(point);
                    if (point[0] != 0)
                    {
                        param = string2int(point);
                        point = skipChars(point);
                        if ((part == TOPLEVEL::section::main && (control == MAIN::control::loadNamedPatchset || control == MAIN::control::loadNamedScale)) || ((param & TOPLEVEL::lowPriority) && param != UNUSED && insert != TOPLEVEL::insert::resonanceGraphInsert))
                        {
                            string name = string(point);
                            if (name > "!")
                                par2 = miscMsgPush(name);
                            //cout << "name " << name << endl;
                        }
                        else if (point[0] != 0)
                            par2 = string2int(point);
                    }
                }
            }
        }
        sendDirect(value, type, control, part, kit, engine, insert, param, par2, request);
        return done_msg;
    }
    else if (matchnMove(2, point, "zread"))
    {
        /*
         * This is a very specific test for reading values and is intended to measure
         * the time these calls take. For that reason the return echos to the CLI and
         * GUI are suppressed, and all results are sent to the CLI only.
         *
         * It is only the selection time we are measuring, and that the correct
         * value is returned.
         *
         * The limit to the number of repeats is INT max. Using high repeat numbers
         * reduces the effect of the processing overhead outside the call loop itself.
         */

        float value, result;
        unsigned char control, part;
        unsigned char kit = UNUSED;
        unsigned char engine = UNUSED;
        unsigned char insert = UNUSED;
        unsigned char parameter = UNUSED;
        unsigned char par2 = UNUSED;
        int repeats;
        if (point[0] == 0)
            return value_msg;
        value = string2int(point);
        point = skipChars(point);
        if (point[0] == 0)
            return value_msg;
        repeats = string2int(point);
        if (repeats < 1)
            repeats = 1;
        point = skipChars(point);
        if (point[0] == 0)
            return value_msg;
        control = string2int(point);
        point = skipChars(point);
        if (point[0] == 0)
            return value_msg;
        part = string2int(point);
        point = skipChars(point);
        if (point[0] != 0)
        {
            kit = string2int(point);
            point = skipChars(point);
            if (point[0] != 0)
            {
                engine = string2int(point);
                point = skipChars(point);
                if (point[0] != 0)
                {
                    insert = string2int(point);
                    point = skipChars(point);
                    if (point[0] != 0)
                    {
                        parameter = string2int(point);
                        point = skipChars(point);
                        if (point[0] != 0)
                            par2 = string2int(point);
                    }
                }
            }
        }

        CommandBlock putData;
        putData.data.value = value;
        putData.data.control = control;
        putData.data.part = part;
        putData.data.kit = kit;
        putData.data.engine = engine;
        putData.data.insert = insert;
        putData.data.parameter = parameter;
        putData.data.par2 = par2;
        putData.data.type = 0;
        struct timeval tv1, tv2;
        gettimeofday(&tv1, NULL);
        for (int i = 0; i < repeats; ++ i)
            result = synth->interchange.readAllData(&putData);
        gettimeofday(&tv2, NULL);

        if (tv1.tv_usec > tv2.tv_usec)
        {
            tv2.tv_sec--;
            tv2.tv_usec += 1000000;
            }
        float actual = (tv2.tv_sec - tv1.tv_sec) *1000000 + (tv2.tv_usec - tv1.tv_usec);
        cout << "result " << result << endl;
        cout << "Loops " << repeats << "  Total time " << actual << "uS" << "  average call time " << actual/repeats * 1000.0f << "nS" << endl;
        return done_msg;
    }
    return unrecognised_msg;
}


int CmdInterface::sendNormal(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2)
{
    if (type >= TOPLEVEL::type::Limits && type < TOPLEVEL::source::CLI && part != TOPLEVEL::section::midiLearn)
    {
        readLimits(value, type, control, part, kit, engine, insert, parameter, par2);
        return done_msg;
    }

    CommandBlock putData;
    size_t commandSize = sizeof(putData);

    putData.data.value = value;
    putData.data.type = type;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;

    /*
     * MIDI learn settings are synced by the audio thread
     * but not passed on to any of the normal controls.
     * The type field is used for a different purpose.
     */

    if (part != TOPLEVEL::section::midiLearn)
    {
        putData.data.type = TOPLEVEL::type::Limits;
        float newValue = synth->interchange.readAllData(&putData);
        if (type == TOPLEVEL::type::LearnRequest)
        {
            if ((putData.data.type & TOPLEVEL::type::Learnable) == 0)
            {
            synth->getRuntime().Log("Can't learn this control");
            return failed_msg;
            }
        }
        else
        {
            if (putData.data.type & TOPLEVEL::type::Error)
                return available_msg;
            if (newValue != value && (type & TOPLEVEL::type::Write))
            { // checking the original type not the reported one
                putData.data.value = newValue;
                synth->getRuntime().Log("Range adjusted");
            }
        }
        type |= TOPLEVEL::source::CLI;
    }

    putData.data.type = type;
    if (jack_ringbuffer_write_space(synth->interchange.fromCLI) >= commandSize)
    {
        synth->getRuntime().finishedCLI = false;
        jack_ringbuffer_write(synth->interchange.fromCLI, (char*) putData.bytes, commandSize);
    }
    else
    {
        synth->getRuntime().Log("Unable to write to fromCLI buffer");
        return failed_msg;
    }
    return done_msg;
}


int CmdInterface::sendDirect(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2, unsigned char request)
{
    if (type >= TOPLEVEL::type::Limits && type <= TOPLEVEL::source::CLI)
        request = type & TOPLEVEL::type::Default;
    CommandBlock putData;
    size_t commandSize = sizeof(putData);

    putData.data.value = value;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;

    if (type == TOPLEVEL::type::Default)
    {
        putData.data.type |= TOPLEVEL::type::Limits;
        synth->interchange.readAllData(&putData);
        if ((putData.data.type & TOPLEVEL::type::Learnable) == 0)
        {
            synth->getRuntime().Log("Can't learn this control");
            return 0;
        }
    }

    if (part != TOPLEVEL::section::midiLearn)
        type |= TOPLEVEL::source::CLI;
    /*
     * MIDI learn is synced by the audio thread but
     * not passed on to any of the normal controls.
     * The type field is used for a different purpose.
     */

    putData.data.type = type;
    if (request < TOPLEVEL::type::Limits)
    {
        putData.data.type = request | TOPLEVEL::type::Limits;
        value = synth->interchange.readAllData(&putData);
        string name;
        switch (request)
        {
            case TOPLEVEL::type::Minimum:
                name = "Min ";
                break;
            case TOPLEVEL::type::Maximum:
                name = "Max ";
                break;
            default:
                name = "Default ";
                break;
        }
        type = putData.data.type;
        if ((type & TOPLEVEL::type::Integer) == 0)
            name += to_string(value);
        else if (value < 0)
            name += to_string(int(value - 0.5f));
        else
            name += to_string(int(value + 0.5f));
        if (type & TOPLEVEL::type::Error)
            name += " - error";
        else if (type & TOPLEVEL::type::Learnable)
            name += " - learnable";
        synth->getRuntime().Log(name);
        return 0;
    }

    if (part == TOPLEVEL::section::main && (type & TOPLEVEL::type::Write) == 0 && control >= MAIN::control::readPartPeak && control <= MAIN::control::readMainLRrms)
    {
        string name;
        switch (control)
        {
            case MAIN::control::readPartPeak:
                name = "part " + to_string(int(kit)) + " peak ";
                break;
            case MAIN::control::readMainLRpeak:
                name = "main ";
                if (kit == 0)
                    name += "L ";
                else
                    name += "R ";
                name += "peak ";
                break;
            case MAIN::control::readMainLRrms:
                name = "main ";
                if (kit == 0)
                    name += "L ";
                else
                    name += "R ";
                name += "RMS ";
                break;
            }
            value = synth->interchange.readAllData(&putData);
            synth->getRuntime().Log(name + to_string(value));
        return 0;
    }

    if (part == TOPLEVEL::section::config && putData.data.par2 != UNUSED && (control == CONFIG::control::bankRootCC || control == CONFIG::control::bankCC || control == CONFIG::control::extendedProgramChangeCC))
    {
        synth->getRuntime().Log("In use by " + miscMsgPop(putData.data.par2) );
        return 0;
    }

    if (jack_ringbuffer_write_space(synth->interchange.fromCLI) >= commandSize)
    {
        synth->getRuntime().finishedCLI = false;
        jack_ringbuffer_write(synth->interchange.fromCLI, (char*) putData.bytes, commandSize);
    }
    else
        synth->getRuntime().Log("Unable to write to fromCLI buffer");
    return 0; // no function for this yet
}


float CmdInterface::readControl(unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2)
{
    float value;
    CommandBlock putData;

    putData.data.value = 0;
    putData.data.type = 0;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;
    value = synth->interchange.readAllData(&putData);
    //if (putData.data.type & TOPLEVEL::type::Error)
        //return 0xfffff;
        //cout << "err" << endl;
    return value;
}


void CmdInterface::readLimits(float value, unsigned char type, unsigned char control, unsigned char part, unsigned char kit, unsigned char engine, unsigned char insert, unsigned char parameter, unsigned char par2)
{
    CommandBlock putData;

    putData.data.value = value;
    putData.data.type = type;
    putData.data.control = control;
    putData.data.part = part;
    putData.data.kit = kit;
    putData.data.engine = engine;
    putData.data.insert = insert;
    putData.data.parameter = parameter;
    putData.data.par2 = par2;

        //putData.data.type = request | TOPLEVEL::type::Limits;
    value = synth->interchange.readAllData(&putData);
    string name;
    switch (type & 3)
    {
        case TOPLEVEL::type::Minimum:
            name = "Min ";
            break;
        case TOPLEVEL::type::Maximum:
            name = "Max ";
            break;
        default:
            name = "Default ";
            break;
    }
    type = putData.data.type;
    if ((type & TOPLEVEL::type::Integer) == 0)
        name += to_string(value);
    else if (value < 0)
        name += to_string(int(value - 0.5f));
    else
        name += to_string(int(value + 0.5f));
    if (type & TOPLEVEL::type::Error)
        name += " - error";
    else if (type & TOPLEVEL::type::Learnable)
        name += " - learnable";
    synth->getRuntime().Log(name);
}


void CmdInterface::cmdIfaceCommandLoop()
{
    // Initialise the history functionality
    // Set up the history filename
    string hist_filename;

    { // put this in a block to lose the passwd afterwards
        struct passwd *pw = getpwuid(getuid());
        hist_filename = string(pw->pw_dir) + string("/.yoshimi_history");
    }
    using_history();
    stifle_history(80); // Never more than 80 commands
    if (read_history(hist_filename.c_str()) != 0) // reading failed
    {
        perror(hist_filename.c_str());
        ofstream outfile (hist_filename.c_str()); // create an empty file
    }
    cCmd = NULL;
    bool exit = false;
    sprintf(welcomeBuffer, "yoshimi> ");
    synth = firstSynth;
    while(!exit)
    {
        cCmd = readline(welcomeBuffer);
        if (cCmd)
        {
            if(cCmd[0] != 0)
            {
                reply = todo_msg;
                replyString = "";
                int reply = cmdIfaceProcessCommand(cCmd);
                exit = (reply == exit_msg);

                if (reply == what_msg)
                    synth->getRuntime().Log(replyString + replies[what_msg]);
                else if (reply > done_msg)
                    synth->getRuntime().Log(replies[reply]);
                add_history(cCmd);
            }
            free(cCmd);
            cCmd = NULL;

            if (!exit)
            {
                do
                { // create enough delay for most ops to complete
                    usleep(2000);
                }
                while (synth->getRuntime().runSynth && !synth->getRuntime().finishedCLI);
            }
            if (synth->getRuntime().runSynth)
            {
                string prompt = "yoshimi";
                if (currentInstance > 0)
                    prompt += (":" + asString(currentInstance));
                int expose = readControl(CONFIG::control::exposeStatus, TOPLEVEL::section::config);
                if (expose == 1)
                {
                    string status = findStatus(true);
                    if (status == "" )
                        status = " Top";
                    synth->getRuntime().Log("@" + status, 1);
                }
                else if (expose == 2)
                    prompt += findStatus(true);
                prompt += "> ";
                sprintf(welcomeBuffer,"%s",prompt.c_str());
            }
        }
        if (!exit && synth->getRuntime().runSynth)
            usleep(20000);
    }

    if (write_history(hist_filename.c_str()) != 0) // writing of history file failed
    {
        perror(hist_filename.c_str());
    }
}
