/*
    TextLists.h

    Copyright 2019-2023, Will Godfrey
    Copyright 2024, Kristian Amlie

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
 * 21 bits are currently defined out of a possible 32.
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
        Formant, // in the formant editor itself
        Envelope, // amp/freq/filt/ (Sub only) band
        Test, // special ops for Yoshimi-testsuite
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
    addsynth,
    subsynth,
    padsynth,
    resonance,
    addvoice,
    addmod,
    waveform,
    lfo,
    formant,
    filter,
    envelope,
    reverb,
    section,
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
    mlearn,
    test,
    };
}

extern std::string basics [];

extern std::string toplist [];

extern std::string configlist [];

extern std::string banklist [];

extern std::string partlist [];

extern std::string mcontrollist [];

extern std::string commonlist [];

extern std::string addsynthlist [];

extern std::string addvoicelist [];

extern std::string addmodlist [];

extern std::string addmodnameslist [];

extern std::string subsynthlist [];

extern std::string padsynthlist [];

extern std::string  resonancelist [];

extern std::string waveformlist [];

extern std::string LFOlist [];

extern std::string LFOtype [];

extern std::string LFObpm [];

extern std::string filterlist [];

extern std::string formantlist [];

extern std::string envelopelist [];

extern std::string reverblist [];

extern int reverblistmap[];

extern std::string echolist [];

extern int echolistmap[];

extern std::string choruslist [];

extern int choruslistmap[];

extern std::string phaserlist [];

extern int phaserlistmap[];

extern std::string alienwahlist [];

extern int alienwahlistmap[];

extern std::string distortionlist [];

extern int distortionlistmap[];

extern std::string eqlist [];

extern int eqlistmap[];

extern std::string dynfilterlist [];

extern int dynfilterlistmap[];

extern std::string filtershapes [];

extern std::string learnlist [];

extern std::string vectlist [];

extern std::string scalelist [];

extern std::string scale_errors [];

extern std::string noteslist [];

extern std::string loadlist [];

extern std::string savelist [];

extern std::string listlist [];

extern std::string testlist [];

extern std::string presetgroups [];

extern std::string replies [];

extern std::string fx_list [];

extern std::string type_list [];
extern const int type_offset [];

extern std::string fx_presets [];

extern std::string effreverb [];
extern std::string effecho [];
extern std::string effchorus [];
extern std::string effphaser [];
extern std::string effalienwah [];
extern std::string effdistortion [];
extern std::string effdistypes [];
extern std::string effeq [];
extern std::string eqtypes [];
extern std::string effdynamicfilter [];

extern std::string detuneType [];

extern std::string waveshape [];
extern std::string wavebase [];
extern std::string basetypes [];
extern std::string filtertype [];
extern std::string adaptive [];

extern std::string historyGroup [];

extern std::string subMagType [];
extern std::string subPadPosition [];
extern std::string unisonPhase [];

#endif
