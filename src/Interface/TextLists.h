/*
    TextLists.h

    Copyright 2019-2021, Will Godfrey

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
#ifndef TEXTLISTS_H
#define TEXTLISTS_H
#include <string>
#include <list>

/*
 * These are all handled bit-wise so that you can set several
 * at the same time. e.g. part, addSynth, resonance.
 * There is a function that will clear just the highest bit that
 * is set so you can then step back up the level tree.
 * It is also possible to zero it so that you immediately go to
 * the top level. Therefore, the sequence is important.
 * 19 bits are currently defined out of a possible 32.
 *
 * Top, AllFX and InsFX MUST be the first three
 */

namespace LEVEL{
    enum {
        Top = 0, // always set directly to zero as an integer to clear down
        AllFX = 1, // bits from here on
        InsFX,
        Part,
        Bank,
        Config,
        Vector,
        Scale,
        Learn,
        MControl,
        AddSynth,
        SubSynth,
        PadSynth,
        AddVoice,
        AddMod,
        Oscillator,
        Resonance,
        LFO, // amp/freq/filt
        Filter, // params only (slightly confused with env)
        Envelope, // amp/freq/filt/ (Sub only) band
    };
}

namespace REPLY {
    enum {
        todo_msg = 0,
        done_msg,
        value_msg,
        name_msg,
        op_msg,
        what_msg,
        range_msg,
        low_msg,
        high_msg,
        unrecognised_msg,
        parameter_msg,
        level_msg,
        available_msg,
        inactive_msg,
        failed_msg,
        writeOnly_msg,
        readOnly_msg,
        exit_msg
    };
}

namespace LISTS {
    enum {
    all = 0,
    syseff,
    inseff,
    eff, // effect types
    part,
    mcontrol,
    common,
    addsynth,
    subsynth,
    padsynth,
    resonance,
    addvoice,
    addmod,
    waveform,
    lfo,
    filter,
    envelope,
    reverb,
    echo,
    chorus,
    phaser,
    alienwah,
    distortion,
    eq,
    dynfilter,
    vector,
    scale,
    load,
    save,
    list,
    bank,
    config,
    mlearn
    };
}

static std::string basics [] = {
    "?  Help",       "show commands",
    "List",          "list current settings",
    "STop",          "all sound off",
    "RESet [s]",     "return to start-up conditions, 'ALL' clear MIDI-learn (if 'y')",
    "EXit [s]",      "tidy up and close Yoshimi (if 'y'), 'FOrce' instant exit regardless",
    "RUN <s>",       "execute named command script forcing top level start",
    "RUNLocal <s>",  "execute named command script from current context level",
    "  WAIT <n>",    "1mS to 30,000mS delay, within script only",
    "..",            "step back one level",
    "/",             "step back to top level",
    "@end"
};

static std::string toplist [] = {
    "ADD",                      "add paths and files",
    "  Root <s>",               "root path to list",
    "  Bank <s>",               "make new bank in current root",
    "  YOshimi [n]",            "new Yoshimi instance ID",
    "IMPort [s <n1>] <n2> <s>", "import named directory to slot n2 of current root, (or 'Root' n1)",
    "EXPort [s <n1>] <n2> <s>", "export bank at slot n2 of current root, (or 'Root' n1) to named directory",
    "REMove",                   "remove paths, files and entries",
    "  Root <n>",               "de-list root path ID",
    "  Bank <n1> [s <n2>]",     "delete bank ID n1 (and all instruments) from current root (or 'Root' n2)",
    "  INstrument <n>",         "delete instrument from slot (n) in current bank",
    "  YOshimi <n>",            "close instance ID",
    "  MLearn <s> [n]",         "delete midi learned 'ALL' whole list, or '@'(n) line",
    "Set/Read/MLearn",          "manage all main parameters",
    "MINimum/MAXimum/DEFault",  "find ranges",
    "  Part ...",               "enter context level",
    "  VEctor ...",             "enter context level",
    "  SCale ...",              "enter context level",
    "  MLearn ...",             "enter editor context level",
    "  Bank ...",               "enter context level",
    "  COnfig ...",             "enter context level",
    "  YOshimi <n>",            "read current instance or change to n",
    "  MONo <s>",               "main output mono/stereo (ON = mono, {other})",
    "  SYStem effects [n]",     "enter effects context level",
    "    <ON/OFF>",             "non-destructively enables/disables the effect",
    "    SEnd <n1> <n2>",       "send system effect to effect n1 at volume n2",
    "    (effect) <s>",         "the effect type",
    "    PREset <n>",           "set numbered effect preset to n",
    "    -- ",                  "effect dependent controls",
    "  INSert effects [n1]",    "enter effects context level",
    "    SEnd <s>/<n>",         "set where (Master, OFF or part number)",
    "    (effect) <s>",         "the effect type",
    "    PREset <n>",           "set numbered effect preset to n",
    "    -- ",                  "effect dependent controls",
    "  AVailable <n>",          "available parts (16, 32, 64)",
    "  PANning <n>",            "panning type (Cut, Default, Boost)",
    "  Volume <n>",             "master volume",
    "  SHift <n>",              "master key shift semitones (0 no shift)",
    "  DEtune <n>",             "master fine detune",
    "  SOlo [s]",               "channel 'solo' switch type (ROw, COlumn, LOop, TWoway, CHannel {other} off)",
    "  SOlo CC <n>",            "incoming 'solo' CC number (type must be set first)",
    "  CLear <n>",              "restore instrument on part n to default settings",
    "@end"
};

static std::string configlist [] = {
    "Oscillator <n>",      "* Add/Pad size (power 2 256-16384)",
    "BUffer <n>",          "* internal size (power 2 16-4096)",
    "PAdsynth [s]",        "interpolation type (Linear, other = cubic)",
    "Virtual <n>",         "keyboard (0 = QWERTY, 1 = Dvorak, 2 = QWERTZ, 3 = AZERTY)",
    "Xml <n>",             "compression (0-9)",
    "REports [s]",         "destination (Stdout, other = console)",
    "SAved [s]",           "saved instrument type (Legacy {.xiz}, Yoshimi {.xiy}, Both)",
    //"ENGine [s]",          "enable instrument engines and types info (OFF, {other})",
    "EXPose <s>",          "show current context level (ON, OFF, PRompt)",

    "STate [s]",           "* autoload default at start (ON, {other})",
    "SIngle [s]",          "* force 2nd startup to open new instance instead (ON, {other})",
    "Hide [s]",            "non-fatal errors (ON, {other})",
    "Display [s]",         "GUI splash screen (ON, {other})",
    "Time [s]",            "add to instrument load message (ON, {other})",
    "Include [s]",         "XML headers on file load(Enable {other})",
    "Keep [s]",            "include inactive data on all file saves (ON, {other})",
    "Gui [s]",             "* run with GUI (ON, OFF)",
    "Cli [s]",             "* run with CLI (ON, OFF)",
    "IDentify",            "identify last bank entry fetched or added (ON, {other})",
    "LOCk <s1> <s2>",      "lock history of group s1 (ON, OFF)",
    "","INstrument, PAtchset, SCale, STate, VEctor, MLearn",

    "MIdi <s>",            "* connection type (Jack, Alsa)",
    "AUdio <s>",           "* connection type (Jack, Alsa)",
    "ALsa Type <n>",       "* midi connection type (Fixed, Search, External)",
    "ALsa Midi <s>",       "* comma separated source name list",
    "ALsa Audio <s>",      "* name of hardware device",
    "ALsa Sample <n>",     "* rate (0 = 192000, 1 = 96000, 2 = 48000, 3 = 44100)",
    "Jack Midi <s>",       "* name of source",
    "Jack Server <s>",     "* name",
    "Jack Auto <s>",       "* connect jack on start (ON, {other})",

    "ROot [s]",            "root CC (Msb, Lsb, Off)",
    "BAnk [s]",            "bank CC (Msb, Lsb, Off)",
    "PRogram [s]",         "program change (ON, {other})",
    "ACtivate [s]",        "program change activates part (ON, {other})",
    "EXTended [n]",        "CC value for extended program change >= 128 is off",
    "Quiet [s]",           "ignore 'reset all controllers' (ON, {other})",
    "Nrpn [s]",            "incoming NRPN (ON, {other})",
    "Log [s]",             "incoming MIDI CCs (ON, {other})",
    "SHow [s]",            "GUI MIDI learn editor (ON, {other})",
    "@end"
};

static std::string banklist [] = {
    "<n>",                       "set current bank to number n",
    " ", "or read current ID and name",
    "Name <s>",                  "change the name of the current bank",
    " ", "or read current name only",
    "Root <n>",                  "set current bank root number",
    " ", "or read current full path",
    "Root ID <n>",               "change current bank root ID to n",
//    "Swap <n1> [n2]",            "Swap current bank with bank n1, (opt. in root n2)",
    "INstrument Rename <n> <s>", "change the name of slot n in the current bank",
    "INstrument SAve <n>",       "save current part's instrument to bank slot n",
    "@end"
};

static std::string partlist [] = {
    "<n>",                 "select part number",
    "<ON/OFF>",            "enables/disables the part",
    "Volume <n>",          "volume",
    "Pan <n>",             "panning position",
    "VElocity <n>",        "velocity sensing sensitivity",
    "LEvel <n>",           "velocity sense offset level",
    "MIn <[s][n]>",        "minimum MIDI note value (Last seen or 0-127)",
    "MAx <[s][n]>",        "maximum MIDI note value (Last seen or 0-127)",
    "FUll",                "reset to full key range",
    "POrtamento <s>",      "portamento (ON, {other})",
    "Mode <s>",            "key mode (Poly, Mono, Legato)",
    "Note <n>",            "note polyphony",
    "SHift <n>",           "key shift semitones (0 no shift)",
    "BYpass <n> <s>",      "bypass part effect number n, (ON, {other})",
    "EFfects [n]",         "enter effects context level",
    "  Send <n1> <n2>",    "send part to system effect n1 at volume n2",
    "  (effect) <s>",      "the effect type",
    "  PREset <n>",        "set numbered effect preset to n",
    "    -- ",             "effect dependent controls",
    "PRogram <s>/[s]<n>",  "loads instrument ID - Group n selects from group list",
    "LAtest",              "Show most recent bank load or save",
    "NAme <s>",            "sets the display name the part can be saved with",
    "TYPe <s>",            "sets the instrument type",
    "COPyright <s>",       "sets the instrument copyright message",
    "INFo <s>",            "fills the comments info entry",
    "Humanise Pitch [n]",  "adds a small random pitch change at note_on",
    "Humanise Velocity [n]",  "adds a small random velocity change at note_on",
    "CLear",               "Sets current instrument level parameters to default.",
    "CHannel <n>",         "MIDI channel (> 32 disables, > 16 note off only)",
    "AFtertouch Chan <s1> [s2]", "Off, Filter (Down) + Peak (Down) + Bend (Down) + Modulation + Volume",
    "AFtertouch Key <s1> [s2]",  "Off, Filter (Down) + Peak (Down) + Bend (Down) + Modulation",
    "Destination <s>",     "jack audio destination (Main, Part, Both)",
    "kit mode entries","",
    "KIT",                 "access controls but don't change type",
    "   <n>",              "select kit item number (1-16)",
    "   <ON/OFF>",         "enables/disables the kit item",
    "   MUlti",            "allow item overlaps",
    "   SIngle",           "lowest numbered item in key range",
    "   CRoss",            "cross fade pairs",
    "   QUiet <s>",        "silence this item (OFF, {other})",
    "   MIn <[s][n]>",     "minimum MIDI note value for this item (Last seen or 0-127)",
    "   MAx <[s][n]>",     "maximum MIDI note value for this item (Last seen or 0-127)",
    "   FUll",             "reset to full key range",
    "   EFfect <n>",       "select effect for this item (0-none, 1-3)",
    "   NAme <s>",         "set the name for this item",
    "   DRum <s>",         "set kit to drum mode (OFF, {other})",
    "   NORmal",           "disable kit mode",
    "ADDsynth ...",        "enter AddSynth context",
    "SUBsynth ...",        "enter SubSynth context",
    "PADsynth ...",        "enter PadSynth context",
    "MCOntrol ...",        "enter MIDI controllers context",
    "@end"
};

static std::string mcontrollist [] = {
    "VOlume <s>",               "enables/disables volume control (OFF {other})",
    "VRange <n>",               "volume control range",
    "PAn <n>",                  "panning control width",
    "MOdwheel <s>",             "enables/disables exponential modulation (ON {other})",
    "MRange <n>",               "modulation control range",
    "EXpression <s>",           "enables/disables expression control (OFF {other})",
    "SUstain <s>",              "enables/disables sustain control (OFF {other})",
    "PWheel <n>",               "pitch wheel control range",
    "BReath <s>",               "enables/disables breath control (OFF {other})",
    "CUtoff <n>",               "filter cutoff range",
    "Q <n>",                    "filter Q range",
    "BANdwidth <s>",            "enables/disables exponential bandwidth (ON {other})",
    "BARange <n>",              "bandwidth control range",
    "FMamplitude <s>",          "enables/disables FM amplitude control (OFF {other})",
    "RCenter <n>",              "resonance center frequency",
    "RBand <n>",                "resonance bandwidth",
    "POrtamento <s>",           "enables/disables portamento control (OFF {other})",
    "PGate <n>",                "point when portamento starts or ends",
    "PForm <s>",                "whether portamento is from or to (Start / End)",
    "PTime <n>",                "portamento sweep time",
    "PDownup <n>",              "portamento time stretch - down/up ratio",
    "PProportional <s",         "enables/disables proportional portamento (ON {other})",
    "PExtent <n>",              "distance to double change",
    "PRange <n>",               "difference from non proportional",
    "CLear",                    "set all controllers to defaults",
    "  emulators","",
    "E Modulation <n>",         "emulate modulation controller",
    "E Expression <n>",         "emulate expression controller",
    "E BReath <n>",             "emulate breath controller",
    "E Cutoff <n>",             "emulate filter cutoff controller",
    "E Q <n>",                  "emulate filter Q controller",
    "E BAndwidth <n>",          "emulate bandwidth controller",
    "@end"
};

static std::string commonlist [] = {
    "ON @",                     "enables the part/kit item/engine/insert etc,",
    "OFF @",                    "disables as above",
    "Volume <n> @",             "volume",
    "Pan <n> @",                "panning position",
    "VElocity <n> @",           "velocity sensing sensitivity",
    "MIn <[s][n]> +",           "minimum MIDI note value (Last seen or 0-127)",
    "MAx <[s][n]> +",           "maximum MIDI note value (Last seen or 0-127)",
    "DETune Fine <n> *",        "fine frequency",
    "DETune Coarse <n> *",      "coarse stepped frequency",
    "DETune Type <s> *",        "type of coarse stepping", "","(DEFault, L35, L10, E100, E1200)",
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
    "","(HArmonic, SIne, POwer, SHift, UShift, LShift, UPower, LPower)",
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
    "@end"
};

static std::string addsynthlist [] = {
    "<ON/OFF>",                 "enables/disables the part",
    "Volume <n>",               "volume",
    "Pan <n>",                  "panning position",
    "PRandom <s>",              "enable random panning(ON, {other})",
    "PWidth <n>",               "random panning range",
    "VElocity <n>",             "velocity sensing sensitivity",
    "STEreo <s>",               "ON, {other}",
    "DEPop <n>",                "initial attack slope",
    "PUnch Power <n>",          "attack boost amplitude",
    "PUnch Duration <n>",       "attack boost time",
    "PUnch Stretch <n>",        "attack boost extend",
    "PUnch Velocity <n>",       "attack boost velocity sensitivity",
    "DETune Fine <n>",          "fine frequency",
    "DETune Coarse <n>",        "coarse stepped frequency",
    "DETune Type <s>",          "type of coarse stepping",
    "","(DEFault, L35, L10, E100, E1200)",
    "OCTave <n>",               "shift octaves up or down",
    "BAndwidth <n>",            "modifies relative fine detune of voices",
    "GRoup <s>",                "disables harmonic amplitude randomness of voices with",
    "","a common oscllator (ON, {other})",
    "VOIce ...",                "enter Addsynth voice context",
    "LFO ...",                  "enter LFO insert context",
    "FILter ...",               "enter Filter insert context",
    "ENVelope ...",             "enter Envelope insert context",
    "REsonance ...",            "enter Resonance context",
    "@end"
};

static std::string addvoicelist [] = {
    "<n>",                  "select voice number",
    "<ON/OFF>",             "enables/disables the part",
    "Volume <n>",           "volume",
    "Pan <n>",              "panning position",
    "PRandom <s>",          "enable random panning(ON, {other})",
    "PWidth <n>",           "random panning range",
    "VElocity <n>",         "velocity sensing sensitivity",
    "BENd Adjust <n>",      "pitch bend range",
    "BENd Offset <n>",      "pitch bend shift",
    "DETune Fine <n>",      "fine frequency",
    "DETune Coarse <n>",    "coarse stepped frequency",
    "DETune Type <s>",      "type of coarse stepping",
    "","(DEFault, L35, L10, E100, E1200)",
    "OCTave <n>",           "shift octaves up or down",
    "FIXed <s>",            "set base frequency to 440Hz (ON, {other})",
    "EQUal <n>",            "equal temper variation",
    "Type <s>",             "sound type (Oscillator, White noise, Pink noise, Spot noise)",
    "SOurce <[s]/[n]>",     "voice source (Local, {voice number})",
    "OSCillator <[s]/[n]>", "oscillator source (Internal, {voice number})",
    "Phase <n>",            "relative voice phase",
    "Minus <s>",            "invert entire voice (ON, {other})",
    "DELay <n>",            "delay before this voice starts",
    "REsonance <s>",        "enable resonance for this voice (ON, {other})",
    "BYpass <s>",           "bypass global filter for this voice (ON, {other})",
    "Unison <s>",           "(ON, OFF)",
    "Unison Size <n>",      "number of unison elements",
    "Unison Frequency <n>", "frequency spread of elements",
    "Unison Phase <n>",     "phase randomness of elements",
    "Unison Width <n>",     "stereo width",
    "Unison Vibrato <n>",   "vibrato depth",
    "Unison Rate <n>",      "vibrato speed",
    "Unison Invert <s>",    "phase inversion type (None, Random, Half, Third, Quarter, Fifth)",
    "MOdulator ...",        "enter modulator context",
    "WAveform ...",         "enter the oscillator waveform context",
    "LFO ...",              "enter LFO insert context",
    "FILter ...",           "enter Filter insert context",
    "ENVelope ...",         "enter Envelope insert context",
    "@end"
};

static std::string addmodlist [] = {
    "MOdulator",            "enter Modulator context",
    " - catgory - ","",
    "OFF",                  "disable modulator",
    "MORph","",
    "RINg","",
    "PHAse","",
    "FREquency","",
    "PULse",                "pulse width",
    "","",
//    "SIdeband <s>",         "only sidebands for ring mod (ON, {other})",
    "Volume <n>",           "volume",
    "VElocity <n>",         "velocity sensing sensitivity",
    "Damping <n>",          "higher frequency relative damping",
    "DETune Fine <n>",      "fine frequency",
    "DETune Coarse <n>",    "coarse stepped frequency",
    "DETune Type <s>",      "type of coarse stepping",
    "","(DEFault, L35, L10, E100, E1200)",
    "SOurce <[s]/[n]>",     "oscillator source (Local, {voice number})",
    "OSCillator <[s]/[n]>", "modulation oscillator(Internal, {modulator number})",
    "FOLlow <s>",           "use source oscillator detune (ON, {other})",
    "FIXed <s>",            "set modulator frequency to 440Hz (ON, {other})",
    "SHift <n>",            "oscillator relative phase",
    "WAveform ...",         "enter the oscillator waveform context",
    "@end"
};

// need to find a way to avoid this kind of duplication
static std::string addmodnameslist [] = {"Off", "Morph", "Ring", "Phase", "Frequency", "Pulsewidth", "@end"};

static std::string subsynthlist [] = {
    "<ON/OFF>",                 "enables/disables the part",
    "Volume <n>",               "volume",
    "Pan <n>",                  "panning position",
    "PRandom <s>",              "enable random panning(ON, {other})",
    "PWidth <n>",               "random panning range",
    "VElocity <n>",             "velocity sensing sensitivity",
    "STEreo <s>",               "ON, {other}",
    "BENd Adjust <n>",          "pitch bend range",
    "BENd Offset <n>",          "pitch bend shift",
    "DETune Fine <n>",          "fine frequency",
    "DETune Coarse <n>",        "coarse stepped frequency",
    "DETune Type <s>",          "type of coarse stepping",
    "","(DEFault, L35, L10, E100, E1200)",
    "OCTave <n>",               "shift octaves up or down",
    "FIXed <s>",                "set base frequency to 440Hz (ON, {other})",
    "EQUal <n>",                "equal temper variation",
    "OVertone Position <s>",    "relationship to fundamental",
    "","(HArmonic, SIne, POwer, SHift, UShift, LShift, UPower, LPower)",
    "OVertone First <n>",       "degree of first parameter",
    "OVertone Second <n>",      "degree of second parameter",
    "OVertone Harmonic <n>",    "amount harmonics are forced",
    "HArmonic <n1> Amp <n2>",   "set harmonic n1 to n2 intensity",
    "HArmonic <n1> Band <n2>",  "set harmonic n1 to n2 width",
    "HArmonic Stages <n>",      "number of stages",
    "HArmonic Mag <s>",         "harmonics filtering type",
    "", "Linear, 40dB, 60dB, 80dB, 100dB",
    "HArmonic Position <s>",    "start position (Zero, Random, Maximum)",
    "BAnd Width <n>",           "common bandwidth",
    "BAnd Scale <n>",           "bandwidth slope v frequency",
    "FILter ...",               "enter Filter insert context",
    "ENVelope ...",             "enter Envelope insert context",
    "@end"
};

static std::string padsynthlist [] = {
    "<ON/OFF>",                 "enables/disables the part",
    "Apply",                    "use on 1st entry & harmonic changes",
    "Volume <n>",               "volume",
    "Pan <n>",                  "panning position",
    "PRandom <s>",              "enable random panning(ON, {other})",
    "PWidth <n>",               "random panning range",
    "VElocity <n>",             "velocity sensing sensitivity",
    "STEreo <s>",               "ON, {other}",
    "DEPop <n>",                "initial attack slope",
    "PUnch Power <n>",          "attack boost amplitude",
    "PUnch Duration <n>",       "attack boost time",
    "PUnch Stretch <n>",        "attack boost extend",
    "PUnch Velocity <n>",       "attack boost velocity sensitivity",
    "BENd Adjust <n>",          "pitch bend range",
    "BENd Offset <n>",          "pitch bend shift",
    "DETune Fine <n>",          "fine frequency",
    "DETune Coarse <n>",        "coarse stepped frequency",
    "DETune Type <s>",          "type of coarse stepping",
    "","(DEFault, L35, L10, E100, E1200)",
    "OCTave <n>",               "shift octaves up or down",
    "FIXed <s>",                "set base frequency to 440Hz (ON, {other})",
    "EQUal <n>",                "equal temper variation",
    "OVertone Position <s>",    "relationship to fundamental",
    "","(HArmonic, SIne, POwer, SHift, UShift, LShift, UPower, LPower)",
    "OVertone First <n>",       "degree of first parameter",
    "OVertone Second <n>",      "degree of second parameter",
    "OVertone Harmonic <n>",    "amount harmonics are forced",
    "PRofile <s>",              "shape of harmonic profile (Gauss, Square Double exponent)",
    "WIdth <n>",                "width of harmonic profile",
    "COunt <n>",                "number of profile repetitions",
    "EXpand <n>",               "adds harmonics and changes distribution",
    "FRequency <n>",            "further modifies distribution (dependent on stretch)",
    "SIze <n>",                 "change harmonic width retaining shape",

    "CRoss <s>",                "cross section of profile (Full, Upper, Lower)",
    "MUltiplier <s>",           "amplitude multiplier (OFF, Gauss, Sine, Flat)",
    "MOde <s>",                 "amplitude mode (Sum, Mult, D1, D2)",

    "CEnter <n>",               "changes the central harmonic component width",
    "RELative <n>",             "changes central component relative amplitude",
    "AUto <s>",                 "(ON {other})",

    "BASe <s>",                 "base profile distribution (C2, G2, C3, G3, C4, G4, C5, G5, G6)",
    "SAmples <n>",              "samples/octave (0.5, 1, 2, 3, 4, 6, 12)",
    "RAnge <n>",                "number of octaves (1 to 8)",
    "LEngth <n>",               "length of one sample in k (16, 32, 64, 128, 256, 512, 1024)",

    "BAndwidth <n>",            "overall bandwidth",
    "SCale <s>",                "bandwidth scale (Normal, Equalhz, Quarter, Half, Threequart, Oneandhalf, Double, Inversehalf)",
    "SPectrum <s>",             "spectrum mode (Bandwidth, Discrete, Continuous)",

    "APply",                    "puts latest changes into the wavetable",
    "XPort <s>",                "export current sample set to named file",
    "WAveform ...",             "enter the oscillator waveform context",
    "RESonance ...",            "enter Resonance context",
    "LFO ...",                  "enter LFO insert context",
    "FILter ...",               "enter Filter insert context",
    "ENVelope ...",             "enter Envelope insert context",
    "@end"
};

static std::string  resonancelist [] = {
    "(enable) <s>",        "activate resonance (ON, {other})",
    "PRotect <s>",         "leave fundamental unchanged (ON, {other})",
    "Maxdb <n>",           "maximum attenuation of points",
    "Random <s>",          "set a random distribution (Coarse, Medium, Fine)",
    "CEnter <n>",          "center frequency of range",
    "Octaves <n>",         "number of octaves covered",
    "Interpolate <s>",     "turn isolated peaks into lines or curves (Linear, Smooth)",
    "Smooth",              "reduce range and sharpness of peaks",
    "CLear",               "set all points to mid level",
    "POints [<n1> [n2]]",  "show all or set/read n1 to n2",
    "@end"
};

static std::string waveformlist [] = {
    "SINe",                     "basic waveforms",
    "TRIangle","",
    "PULse","",
    "SAW","",
    "POWer","",
    "GAUss","",
    "DIOde","",
    "ABSsine","",
    "PULsesine","",
    "STRetchsine","",
    "CHIrp","",
    "ASIne","",
    "CHEbyshev","",
    "SQUare","",
    "SPIke","",
    "CIRcle","",
    "HYPersec","",
    "Harmonic <n1> Amp <n2>",   "harmonic n1 to n2 intensity",
    "Harmonic <n1> Phase <n2>", "harmonic n1 to n2 phase",
    "Harmonic Shift <n>",       "amount harmonics are moved",
    "Harmonic Before <s>",      "shift before waveshaping and filtering (ON {other})",
    "COnvert",                  "change resultant wave to groups of sine waves",
    "CLear",                    "clear harmonic settings",
    "Base Par <n>",             "basic wave parameter",
    "Base Mod Type <s>",        "basic modulation type (OFF, Rev, Sine Power)",
    "Base Mod Par <n1> <n2>",   "parameter number n1 (1 - 3), n2 value",
    "Base Convert [s]",         "use resultant basic wave as base shape",
    "","also clear modifiers and harmonics (OFF {other})",
    "SHape Type <s>",           "wave shape modifier type",
    "","(OFF, ATAn, ASYm1, POWer, SINe, QNTs, ZIGzag, LMT, ULMt, LLMt, ILMt, CLIp, AS2, PO2, SGM)",
    "SHape Par <n>",            "wave shape modifier amount",
    "Filter Type <s>","",
    "","(OFF, LP1, HPA1, HPB1, BP1, BS1, LP2, HP2, BP2, BS2, COS, SIN, LSH, SGM)",
    "Filter Par <n1> <n2>",     "filter parameters  n1 (1/2), n2 value",
    "Filter Before <s>",        "do filtering before waveshaping (ON {other})",
    "Modulation Par <n1 <n2>",  "overall modulation n1 (1 - 3), n2 value",
    "SPectrum Type <s>",        "spectrum adjust type (OFF, Power, Down/Up threshold)",
    "SPectrum Par <n>",         "spectrum adjust amount",
    "ADdaptive Type <s>",       "adaptive harmonics (OFF, ON, SQUare, 2XSub, 2XAdd, 3XSub, 3XAdd, 4XSub, 4XAdd)",
    "ADdaptive Base <n>",       "adaptive base frequency",
    "ADdaptive Level <n>",      "adaptive power",
    "ADdaptive Par <n>",        "adaptive parameter",
    "APply",                    "fix settings (only for PadSynth)",
    "@end"
};

static std::string LFOlist [] = {
    " - catgory -","",
    "AMplitude",             "amplitude type",
    "FRequency",             "frequency type",
    "FIlter",                "filter type",
    " - control -","",
    "Rate [<n>][<n1> <n2>]", "frequency or BPM [1 16] to [16 1], [2 3], [3 2]",
    "Intensity <n>",         "depth",
    "Start <n>",             "start position in cycle",
    "Delay <n>",             "time before effect",
    "Expand <n>",            "rate / note pitch",
    "Bpm <s>",               "sync frequency to MIDI clock (ON, {other})",
    "Continuous <s>",        "(ON, {other})",
    "AR <n>",                "amplitude randomness",
    "FR <n>",                "frequency randomness",
    "Type <s>",              "LFO oscillator shape",
    "","SIne, TRiangle, SQuare, RUp (ramp up), RDown (ramp down), E1down (exp. 1), E2down (exp. 1)",
    "","SH (sample/hold), RSU (rand square up), RSD (rand square down)",
    "e.g. S FI T RU",        "set filter type ramp up",
    "@end"
};
// TODO need to find a way to safely (and efficiently) combine these
static std::string LFOtype [] = {
    "SIne", "TRiangle", "SQuare", "RUp (ramp up)", "RDown (ramp down)", "E1down (exp. 1)", "E2down (exp. 2)", "SH (sample/hold)", "RSU (rand square up)", "RSD (rand square dn)", "@end"
};

static std::string LFObpm [] = {
"1/16 BPM", // space for expansion
"1/16 BPM", "1/15 BPM", "1/14 BPM", "1/13 BPM", "1/12 BPM", "1/11 BPM", "1/10 BPM", "1/9 BPM", "1/8 BPM", "1/7 BPM", "1/6 BPM", "1/5 BPM", "1/4 BPM", "1/3 BPM", "1/2 BPM", "2/3 BPM", "1/1 BPM", "3/2 BPM", "2/1 BPM", "3/1 BPM", "4/1 BPM", "5/1 BPM", "6/1 BPM", "7/1 BPM", "8/1 BPM", "9/1 BPM", "10/1 BPM", "11/1 BPM", "12/1 BPM", "13/1 BPM", "14/1 BPM", "15/1 BPM", "16/1 BPM",
"16/1 BPM", // space for expansion
"Unknown BPM",
};

static std::string filterlist [] = {
    "CEnter <n>",       "center frequency",
    "Q <n>",            "Q factor",
    "Velocity <n>",     "velocity sensitivity",
    "SLope <n>",        "velocity curve",
    "Gain <n>",         "overall amplitude",
    "TRacking <n>",     "frequency tracking",
    "Range <s>",        "extended tracking (ON, {other})",
    "CAtegory <s>",     "Analog, Formant, State variable",
    "STages <n>",       "filter stages (1 to 5)",
    "TYpe <s>",         "category dependent - not formant",
    " - analog -","",
    "l1",           "one stage low pass",
    "h1",           "one stage high pass",
    "l2",           "two stage low pass",
    "h2",           "two stage high pass",
    "band",         "two stage band pass",
    "stop",         "two stage band stop",
    "peak",         "two stage peak",
    "lshelf",       "two stage low shelf",
    "hshelf",       "two stage high shelf",
    " - state variable -","",
    "low",      "low pass",
    "high",     "high pass",
    "band",     "band pass",
    "stop",     "band stop",
    "","",
    " - formant editor -",   "(shows V current vowel, F current formant)",
    "Invert <s>",       "invert effect of LFOs, envelopes (ON, OFF)",
    "FCenter <n>",      "center frequency of sequence",
    "FRange <n>",       "octave range of formants",
    "Expand <n>",       "stretch overall sequence time",
    "Lucidity <n>",     "clarity of vowels",
    "Morph <n>",        "speed of change between formants",
    "SIze <n>",         "number of vowels in sequence",
    "COunt <n>",        "number of formants in vowels",
    "VOwel <n>",        "vowel being processed",
    "Point <n1> <n2>",  "vowel n1 at sequence position n2",
    "FOrmant <n>",      "formant being processed",
    " - per formant -","",
    "  FFrequency <n>", "center frequency of formant",
    "  FQ <n>",         "bandwidth of formant",
    "  FGain <n>",      "amplitude of formant",
    "@end"
};

static std::string envelopelist [] = {
    " - category -","",
    "AMplitude",             "amplitude type",
    "FRequency",             "frequency type",
    "FIlter",                "filter type",
    "BAndwidth",             "bandwidth type (SubSynth only)",
    "","",
    " - common - ","",
    "Expand <n>",            "envelope time on lower notes",
    "Force <s>",             "force release (ON, {other})",
    "Linear <s>",            "linear slopes (ON, {other})",
    "FMode <s>",             "set as freemode (ON, {other})",
    "","",
    " - fixed -","",
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
    " - freemode -","",
    "Points",                "number of defined points (read only)",
    "Sustain <n>",           "point number where sustain starts",
    "Insert <n1> <n2> <n3>", "insert point at n1 with X increment n2, Y value n3",
    "Delete <n>",            "remove point n",
    "Change <n1> <n2> <n3>", "change point n1 to X increment n2, Y value n3",
    "@end"
};

static std::string reverblist [] = {
    "LEVel <n>",        "intensity",
    "PANning <n>",      "L/R panning",
    "TIMe <n>",         "reverb time",
    "DELay <n>",        "initial delay",
    "FEEdback <n>",     "delay feedback",
    "LOW <n>",          "low pass filter",
    "HIGh <n>",         "high pass filter",
    "DAMp <n>",         "feedback damping",
    "TYPe <s>",         "reverb type (Random, Freeverb, Bandwidth)",
    "ROOm <n>",         "room size",
    "BANdwidth <n>",    "actual bandwidth (only for bandwidth type)",
    "@end"
};

static std::string echolist [] = {
    "LEVel <n>",        "intensity",
    "PANning <n>",      "L/R panning",
    "DELay <n>",        "initial delay",
    "LRDelay <n>",      "left-right delay",
    "CROssover <n>",    "left-right crossover",
    "FEEdback <n>",     "echo feedback",
    "DAMp <n>",         "feedback damping",
    "@end"
};

static std::string choruslist [] = {
    "LEVel <n>",        "intensity",
    "PANning <n>",      "L/R panning",
    "FREquency <n>",    "LFO frequency",
    "RANdom <n>",       "LFO randomness",
    "WAVe <s>",         "LFO waveshape (sine, triangle)",
    "SHIft <n>",        "L/R phase shift",
    "DEPth <n>",        "LFO depth",
    "DELay <n>",        "LFO delay",
    "FEEdback <n>",     "chorus feedback",
    "CROssover <n>",    "L/R routing",
    "SUBtract <s>",     "invert output (ON {other})",
    "@end"
};

static std::string phaserlist [] = {
    "LEVel <n>",        "intensity",
    "PANning <n>",      "L/R panning",
    "FREquency <n>",    "LFO frequency",
    "RANdom <n>",       "LFO randomness",
    "WAVe <s>",         "LFO waveshape (sine, triangle)",
    "SHIft <n>",        "L/R phase shift",
    "DEPth <n>",        "LFO depth",
    "FEEdback <n>",     "phaser feedback",
    "STAges <n>",       "filter stages",
    "CROssover <n>",    "L/R routing",
    "SUBtract <s>",     "invert output (ON {other})",
    "RELative <n>",     "relative phase",
    "HYPer <s>",        "hyper ?  (ON {other})",
    "OVErdrive <n>",    "distortion",
    "ANAlog <s>",       "analog emulation (ON {other})",
    "@end"
};

static std::string alienwahlist [] = {
    "LEVel <n>",        "intensity",
    "PANning <n>",      "L/R panning",
    "FREquency <n>",    "LFO frequency",
    "RANdom",           "LFO randomness",
    "WAVe <s>",         "LFO waveshape (sine, triangle)",
    "SHIft <n>",        "L/R phase shift",
    "DEPth <n>",        "LFO depth",
    "FEEdback <n>",     "filter feedback",
    "DELay <n>",        "LFO delay",
    "CROssover <n>",    "L/R routing",
    "RELative <n>",     "relative phase",
    "@end"
};

static std::string distortionlist [] = {
    "LEVel <n>",        "intensity",
    "PANning <n>",      "L/R panning",
    "MIX <n>",          "L/R mix",
    "DRIve <n>",        "input level",
    "OUTput <n>",       "output balance",
    "WAVe <s>",         "function waveshape",
    "","(ATAn, ASYm1, POWer, SINe, QNTs, ZIGzag, LMT, ULMt, LLMt, ILMt, CLIp, AS2, PO2, SGM)",
    "INVert <s>",       "invert ?  (ON {other})",
    "LOW <n>",          "low pass filter",
    "HIGh <n>",         "high pass filter",
    "STEreo <s>",       "stereo (ON {other})",
    "FILter <s>",       "filter before distortion",
    "@end"
};

static std::string eqlist [] = {
    "LEVel <n>",        "intensity",
    "BANd <n>",         "EQ band number for following controls",
    "FILter <s>",       "filter type",
    "","(LP1, HP1, LP2, HP2, NOT, PEA, LOW, HIG)",
    "FREquency <n>",    "cutoff/band frequency",
    "GAIn <n>",         "makeup gain",
    "Q <n>",            "filter Q",
    "STAges <n>",       "filter stages",
    "@end"
};

static std::string dynfilterlist [] = {
    "LEVel <n>",        "intensity",
    "PANning <n>",      "L/R panning",
    "FREquency <n>",    "LFO frequency",
    "RANdom <n>",       "LFO randomness",
    "WAVe <s>",         "LFO waveshape (sine, triangle)",
    "SHIft <n>",        "L/R phase shift",
    "DEPth <n>",        "LFO depth",
    "SENsitivity <n>",  "amount amplitude changes filter",
    "INVert <s>",       "reverse effect of sensitivity (ON {other})",
    "RATe <n>",         "speed of filter change with amplitude",
    "FILter ...",       "enter dynamic filter context",
    "@end"
};

static std::string filtershapes [] = {"OFF" ,"ATA", "ASY", "POW", "SIN", "QNT", "ZIG", "LMT", "ULM", "LLM", "ILM", "CLI", "CLI", "AS2", "PO2", "SGM", "@end"};

static std::string learnlist [] = {
    "<n>",          "set current line number",
    "MUte <s>",     "completely ignore this line (ON, {other})",
    "SEven",        "set incoming NRPNs as 7 bit (ON, {other})",
    "CC <n>",       "set incoming controller value",
    "CHan <n>",     "set incoming channel number",
    "MIn <n>",      "set minimum percentage",
    "MAx <n>",      "set maximum percentage",
    "LImit <s>",    "limit instead of compress (ON, {other})",
    "BLock <s>",    "inhibit others on this CC/Chan pair (ON, {other})",
    "@end"
};

static std::string vectlist [] = {
    "<n>",                      "set current base channel",
    "[X/Y] CC <n>",             "CC n is used for X or Y axis sweep",
    "[X/Y] Features <n> [s]",   "sets X or Y features 1-4 (ON, Reverse, {other})",
    "[X] PRogram <l/r> <n>",    "X program change ID for LEFT or RIGHT part",
    "[Y] PRogram <d/u> <n>",    "Y program change ID for DOWN or UP part",
    "[X/Y] Control <n1> <n2>",  "sets n2 CC to use for X or Y feature n1 (2-4)",
    "OFF",                      "disable vector for this channel",
    "Name <s>",                 "text name for this complete vector",
    "@end"
};

static std::string scalelist [] = {
    "FRequency <n>",      "Reference note actual frequency",
    "NOte <n>",           "Reference note number",
    "Invert [s]",         "invert entire scale (ON, {other})",
    "CEnter <n>",         "note number of key center",
    "SHift <n>",          "shift entire scale up or down",
    "SCale [s]",          "activate microtonal scale (ON, {other})",
    "MApping [s]",        "activate keyboard mapping (ON, {other})",
    "FIrst <n>",          "first note number to map",
    "MIddle <n>",         "middle note number to map",
    "Last <n>",           "last note number to map",
    "Tuning <s1> [s2]",   "CSV tuning values (n1.n1 or n1/n1 , n2.n2 or n2/n2 , etc.)",
    "","s1 = 'IMPort' from s2 named file",
    "Keymap <s1> [s2]",   "CSV keymap (n1, n2, n3, etc.)",
    "","s1 = 'IMPort' from s2 named file",
    "NAme <s>",           "internal name for this scale",
    "DEscription <s>",    "description of this scale",
    "CLEar",              "clear all settings and revert to standard scale",
    "@end"
};

static std::string noteslist [] = { // from 21
    "(A0)", "(A#0)", "(B0)",
    "(C1)", "(C#1)", "(D1)", "(D#1)", "(E1)", "(F1)", "(F#1)", "(G1)", "(G#0)", "(A1)", "(A#1)", "(B1)",
    "(C2)", "(C#2)", "(D2)", "(D#2)", "(E2)", "(F2)", "(F#2)", "(G2)", "(G#2)", "(A2)", "(A#2)", "(B2)",
    "(C3)", "(C#3)", "(D3)", "(D#3)", "(E3)", "(F3)", "(F#3)", "(G3)", "(G#3)", "(A3)", "(A#3)", "(B3)",
    "(C4)", "(C#4)", "(D4)", "(D#4)", "(E4)", "(F4)", "(F#4)", "(G4)", "(G#4)", "(A4)", "(A#4)", "(B4)",
    "(C5)", "(C#5)", "(D5)", "(D#5)", "(E5)", "(F5)", "(F#5)", "(G5)", "(G#5)", "(A5)", "(A#5)", "(B5)",
    "(C6)"
};

static std::string loadlist [] = {
    "Instrument <s>",   "instrument to current part from named file",
    "Default",          "default copyright to current part",
    "SCale <s>",        "scale settings from named file",
    "VEctor [n] <s>",   "vector to channel n (or saved) from named file",
    "Patchset <s>",     "complete set of instruments from named file",
    "MLearn <s>",       "midi learned list from named file",
    "STate <s>",        "all system settings and patch sets from named file",
    "@end"
};

static std::string savelist [] = {
    "Instrument <s>",   "current part to named file",
    "Default",          "current part copyright as default",
    "SCale <s>",        "current scale settings to named file",
    "VEctor <n> <s>",   "vector on channel n to named file",
    "Patchset <s>",     "complete set of instruments to named file",
    "MLearn <s>",       "midi learned list to named file",
    "STate <s>",        "all system settings and patch sets to named file",
    "Config",           "current configuration",
    "@end",
};

static std::string listlist [] = {
    "Roots",            "all available root paths",
    "Banks [n]",        "banks in root ID or current",
    "Instruments [n]",  "instruments in bank ID or current",
    "Group <s1> [s2]",  "instruments by type grouping ('Location' for extra details)",
    "Parts [s]",        "parts with instruments installed ('More' for extra details)",
    "Vectors",          "settings for all enabled vectors",
    "Tuning",           "microtonal scale tunings",
    "Keymap",           "microtonal scale keyboard map",
    "Config",           "current configuration",
    "MLearn [s <n>]",   "midi learned controls ('@' n for full details on one line)",
    "History [s]",      "recent files (Patchsets, SCales, STates, Vectors, MLearn)",
    "Effects [s]",      "effect types ('all' include preset numbers and names)",
    "PREsets",          "all the presets for the currently selected effect",
    "@end"
};

static std::string replies [] = {
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
    "Control or section inactive",
    "Unable to complete",
    "write only",
    "read only",
    "EXIT"
};

static std::string fx_list [] = {
    "UNset",
    "REverb",
    "ECho",
    "CHorus",
    "PHaser",
    "ALienwah",
    "DIstortion",
    "EQ",
    "DYnfilter",
    "@end"
};

static std::string type_list [] = {
    "undefined",
    "Piano",
    "Bells and Chimes",
    "Chromatic Percussion",
    "Organ",
    "Guitar",
    "Bass",
    "Solo Strings",
    "Ensemble",
    "Single Voice",
    "Choir",
    "Brass",
    "Reed",
    "Pipe",
    "Wind (other)",
    "Lead Synth",
    "Pad Synth",
    "Warm Pad",
    "Synth Effects",
    "Ethnic",
    "Percussive",
    "Sound Effects",
    "@end"
};
const int type_offset [] = {0, 1, -3, 2, 3, 4, 5, 6, 7, -6, -2, 8, 9, 10, -5, 11, 12, -4, 13, 14, 15, 16, 255};
/*
 * the number of the above entries must match
 * @end and 255 are the recognised terminators
 * only add negative numbers for backward compatibility
 * they will resolve as 'undefined'
 * note: can't use -1 as ID here
 */

static std::string fx_presets [] = {
    "1, off",
    "13, cathedral 1, cathedral 2, cathedral 3, hall 1, hall 2, room 1, room 2, basement, tunnel, echoed 1, echoed 2, very long 1, very long 2",
    "8, echo 1, echo 2, simple echo, canyon, panning echo 1, panning echo 2, panning echo 3, feedback echo",
    "10, chorus 1, chorus 2, chorus 3, celeste 1, celeste 2, flange 1, flange 2, flange 3, flange 4, flange 5",
    "12, phaser 1, phaser 2, phaser 3, phaser 4, phaser 5, phaser 6, aphaser 1, aphaser 2, aphaser 3, aphaser 4, aphaser 5, aphaser 6",
    "4, alienwah 1, alienwah 2, alienwah 3, alienwah 4",
    "6, overdrive 1, overdrive 2, exciter 1, exciter 2, guitar amp, quantisize",
    "1, not available",
    "4, wahwah, autowah, vocal morph 1, vocal morph 2"
};

// effect controls
static std::string effreverb [] = {"LEV", "PAN", "TIM", "DEL", "FEE", "none5", "none6", "LOW", "HIG", "DAM", "TYP", "ROO", "BAN", "@end"};
static std::string effecho [] = {"LEV", "PAN", "DEL", "LRD", "CRO", "FEE", "DAM",  "@end"};
static std::string effchorus [] = {"LEV", "PAN", "FRE", "RAN", "WAV", "SHI", "DEP", "DEL", "FEE", "CRO", "none11", "SUB", "@end"};
static std::string effphaser [] = {"LEV", "PAN", "FRE", "RAN", "WAV", "SHI", "DEP", "FEE", "STA", "CRO", "SUB", "REL", "HYP", "OVE", "ANA", "@end"};
static std::string effalienwah [] = {"LEV", "PAN", "FRE", "RAN", "WAV", "SHI", "DEP", "FEE", "DEL", "CRO", "REL", "@end"};
static std::string effdistortion [] = {"LEV", "PAN", "MIX", "DRI", "OUT", "WAV", "INV", "LOW", "HIG", "STE", "FIL", "@end"};
static std::string effdistypes [] = {"ATAn", "ASYm1", "POWer", "SINe", "QNTs", "ZIGzag", "LMT", "ULMt", "LLMt", "ILMt", "CLIp", "AS2", "PO2", "SGM", "@end"};
static std::string effeq [] = {"LEV", "BAN", "FIL", "FRE", "GAI", "Q", "STA"};
static std::string eqtypes [] = {"OFF", "LP1", "HP1", "LP2", "HP2", "BP2", "NOT", "PEAk", "LOW shelf", "HIGh shelf", "@end"};
static std::string effdynamicfilter [] = {"LEV", "PAN", "FRE", "RAN", "WAV", "SHI", "DEP", "SEN", "INV", "RAT", "FIL", "@end"};

// common controls
static std::string detuneType [] = {"DEFault", "L35", "L10", "E100", "E1200", "@end"};

// waveform controls
static std::string waveshape [] = {"Sine", "Triangle", "Pulse", "Saw", "Power", "Gauss", "Diode", "AbsSine", "PulseSine", "StretchSine", "Chirp", "AbsStretchSine", "Chebyshev", "Square", "Spike", "Circle", "HyperSec"};
static std::string wavebase [] = {"SIN", "TRI", "PUL", "SAW", "POW", "GAU", "DIO", "ABS", "PSI", "SSI", "CHI", "ASI", "CHE", "SQU", "SPI", "CIR", "HYP", "@end"};
static std::string basetypes [] = {"c2", "g2", "c3", "g3", "c4", "g4", "c5", "g5", "g6"};
static std::string filtertype [] = {"OFF", "LP1", "HPA", "HPB", "BP1", "BS1", "LP2", "HP2", "BP2", "BS2", "COS", "SIN", "LSH", "SGM", "@end"};
static std::string adaptive [] = {"OFF", "ON", "SQU", "2XS", "2XA", "3XS", "3XA", "4XS", "4XA"};

// misc controls
static std::string historyGroup [] = {"IN", "PA", "SC", "ST", "VE", "ML"};

static std::string subMagType [] = {"Linear", "40dB", "60dB", "80dB", "100dB"};
static std::string subPadPosition [] = {"harmonic", "ushift", "lshift", "upower", "lpower", "sine", "power", "shift"};
static std::string unisonPhase [] = {"none", "random", "half", "third", "quarter", "fifth"};

#endif
