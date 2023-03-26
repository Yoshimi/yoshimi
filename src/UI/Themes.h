/*
    Themes.h - FLTK theme colours

    Copyright 2023 Will Godfrey & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef THEMES_H
#define THEMES_H

#include <string>
#include "Misc/FileMgrFuncs.h"
#include "Misc/FormatFuncs.h"

/*
  The following are ordered by value for easy number tracking.
  Where possible they are fairly close to FLTK defaults.

  Fluid's colour chooser doesn't recognise our named colour numbers
  so most of these are written in numerically.
  Designing layouts without Fluid is seriously hard work so we have
  to go with it :(
*/


/*
  Reserved(*) / spare colours
  Ideally there should be no reserved colours, as that means we are
  not managing them!

  1*  root/bank 'delete' lights
  2*  root/bank 'select' lights
  3*  root 'add' bank 'save' lights
  4*  root/bank 'swap' lights
  5*  root/bank 'rename' lights
  6*  root/bank slot first swap selection

  16* bank extended instruments background
  17* bank 'occupied' extended instruments background

  46* root/bank slot normal background

  51* bank 'occupied' normal background

  56
  58 - 61
  68 - 72
  75 - 77
  78* bank root 'export' light
  79 - 80
  82 - 87
  88
  93* bank root 'import' light
  98 - 100
  101
  102 - 103
  104
  106 - 123
  124* filer icon fake lines
  125 - 127
  130
  138 - 143
  148 - 154
  156
  160
  161
  164 - 167
  176
  180
  183* filer dir icon
  184
  185
  186
  187
  192 - 196
  200 - 205
  208 - 213
  216
  218* query '?' text
  220
  224
  225
  232 - 235
  237* various down boxes
  240 - 243
  248 - 251
  252*  bank current bank highlight
*/


const int gen_text_back = 7;
const int instr_back = 17;
const int graph_back = 57;
const int slider_track = 62;
const int graph_line = 63;
const int gen_text = 64;
const int keyb_slider_back = 65;
const int tooltip_text = 66;
const int tooltip_faint_text = 67;
const int env_line_sel = 81;
const int warning_button = 89;
const int pending_button = 90;
const int warning_background = 91;
const int env_sus = 92;
const int EQ_line_off = 94;
const int EQ_line = 95;
const int EQ_back = 96;
const int EQ_back_off = 97;
const int tooltip_major_grid = 105;
const int pad_harmonic_line = 128;
const int warning_text = 129;
const int reson_graph_line = 131;
const int formant_graph_line = 132;
const int formant_ghost_marker = 133;
const int formant_marker = 134;
const int VU_rms = 135;
const int pad_prof_line = 136;
const int pad_prof_inactive = 137;
const int keyb_pitch_peg = 143; // not currently redefinable
const int knob_ring = 144;
const int knob_point = 145;
const int tooltip_grid = 146;
const int EQ_grid = 147;
const int EQ_major_grid = 155;
const int ext_voice = 157;
const int pad_back = 158;
const int pad_equiv_back = 159;
const int pad_grid_centre = 162;
const int pad_grid = 163;
const int VU_over = 168;
const int gen_opp_text = 175;
const int tooltip_curve = 177;
const int VU_bar_1dB = 178;
const int CP_background = 179;
const int dynfilter_button = 181;
const int instr_info_back = 182;
const int eff_preset = 188;
const int eff_preset_changed = 189;
const int VU_bar_10dB = 190;
const int query_back = 191;
const int CP_text = 197;
const int links = 198;
const int knob_lit = 199;
const int filer_text_back = 206;
const int knob_high = 207;
const int add_back = 214;
const int tooltip_back = 215;
const int graph_Harmonics_grid = 216;
const int graph_grid = 217;
const int yoshi_ins_typ = 219;
const int alt_links = 221;
const int VU_bar_5dB = 222;
const int panels = 223;
const int learnable_text = 226;
const int pad_prof_band = 227;
const int actions = 228;
const int VU_level = 229;
const int keyb_mod_bar = 230; // not currently redefinable
const int pad_prof_fill = 231;
const int sub_back = 236;
const int ext_osc = 238;
const int env_ctl_sel = 239;
const int knob_low = 244;
const int graph_resonance_grid = 245;
const int env_line = 246;
const int midi_text_back = 247;
const int env_ctl = 253;
const int graph_pad_back = 254;
const int VU_text = 255;


/*
  The following are ordered as they are in theme lists.
  They are grouped mainly by function.

  All new definitions must be placed at the end of the list table
  so that new work doesn't mess up existing themes.
  Ideally, use colours as close as possible to the colour table.
*/
const int COLOURLIST = 80;
const unsigned char colourNumbers [COLOURLIST] = {
    knob_low,
    knob_high,
    knob_ring,
    knob_lit,
    knob_point,
    slider_track,
    warning_button,
    warning_background,
    CP_background,
    CP_text,
    tooltip_back,
    tooltip_grid,
    tooltip_major_grid,
    tooltip_curve,
    tooltip_text,
    tooltip_faint_text,
    filer_text_back,
    gen_text_back,
    gen_text,
    gen_opp_text,
    graph_back,
    graph_grid,
    graph_resonance_grid,
    graph_Harmonics_grid,
    graph_line,
    formant_graph_line,
    reson_graph_line,
    EQ_back,
    EQ_back_off,
    EQ_grid,
    EQ_major_grid,
    EQ_line,
    EQ_line_off,
    env_ctl,
    env_ctl_sel,
    env_sus,
    env_line,
    env_line_sel,
    eff_preset,
    eff_preset_changed,
    yoshi_ins_typ,
    instr_info_back,
    instr_back,
    add_back,
    sub_back,
    pad_back,
    graph_pad_back,
    pad_equiv_back,
    pad_grid,
    pad_grid_centre,
    pad_prof_band,
    pad_prof_fill,
    pad_prof_line,
    pad_prof_inactive,
    pad_harmonic_line,
    ext_voice,
    ext_osc,
    VU_bar_1dB,
    VU_bar_5dB,
    VU_bar_10dB,
    VU_level,
    VU_rms,
    VU_over,
    VU_text,
    midi_text_back,
    links,
    alt_links,
    actions,
    panels,
    learnable_text,
    formant_marker,
    formant_ghost_marker,
    warning_text,
    pending_button,
    query_back,
    dynfilter_button,
    keyb_slider_back,
    keyb_pitch_peg,
    keyb_mod_bar,
};

static std::string colourPreamble [] = {
    "Do not edit this. It may be overwritten.",
    "Instead, copy as template for other named themes.",
    "Don't add or remove lines between and including dashes.",
    "This would corrupt the colour map.",
    "Instead place @ at start of line for default setting.",
    "------------------ data start marker",
    "END"
}; // Do not change the last two text lines!

static std::string colourData [] = {
    "0,255, gray scale min-max (can be reversed)",
    "186,198,211, Knob shadow (R,G,B or #rrggbb)",
    "#e7ebef, Knob highlight (231,235,239)",
    "51,51,51, Knob ring",
    "0,197,255, Knob ring lit",
    "61,61,61, Knob pointer",
    "0,0,0, Slider track",
    "220,0,0, Warning type button",
    "250,150,90, Warning background patch",
    "0,109,191, Copy/Paste background",
    "255,255,255, Copy/Paste text",
    "255,255,210, Tooltip background",
    "180,180,180, Tooltip grid",
    "50,50,50, Tooltip major grid",
    "0,0,255, Tooltip curve",
    "0,0,0, Tooltip text",
    "150,150,150, Tooltip faint text",
    "240,250,230, Filer favourites background",
    "255,255,255, General text background",
    "0,0,0, General text",
    "255,255,255, General opposite text",
    "0,0,0, General graph background",
    "40,120,190, Waveform graph grid",
    "180,180,180, Resonance graph grid",
    "30,70,255, Harmonics graph grid",
    "0,255,0, Waveform graph line",
    "255,0,0, Formant graph line",
    "255,0,0, Resonance graph line",
    "0,70,150, EQ graph background",
    "80,120,160, EQ background disabled",
    "200,200,200, EQ graph grid",
    "255,255,255, EQ graph major grid",
    "255,255,0, EQ graph line",
    "200,200,80, EQ graph line disabled",
    "255,255,255, Envelope control point",
    "0,255,255, Envelope control point selected",
    "255,255,0, Envelope sustain line",
    "255,255,255, Envelope line",
    "255,0,0, Envelope line selected",
    "80,0,0, Effect preset",
    "0,80,255, Effect preset changed",
    "0,0,225, Yoshimi instrument type",
    "240,250,230, Instrument info background",
    "253,246,230, Instrument background",
    "223,175,191, AddSynth background",
    "175,207,223, SubSynth background",
    "205,221,173, PadSynth background",
    "245,245,245, Padsynth harmonics background",
    "225,225,225, PadSynth profile equivalent background",
    "180,180,180, Padsynth profile grid",
    "90,90,90, Padsynth profile centre mark",
    "90,120,250, PadSynth profile equivalent markers",
    "180,210,240, PadSynth profile fill",
    "0,0,120, PadSynth profile line",
    "150,150,150, PadSynth profile line disabled",
    "200,0,0, PadSynth harmonic line",
    "159,223,143, External voice",
    "143,191,223, External oscillator",
    "0,145,255, VU 1dB marker",
    "63,218,255, VU 5dB marker",
    "140,180,220, VU 10dB marker",
    "63,182,255, VU level",
    "255,255,0, VU rms",
    "255,0,0,VU Overload",
    "255,254,254,254, VU_text",
    "191,255,255, Midilearn text background",
    "127,200,191, Link buttons",
    "180,180,200, Alternative link buttons",
    "63,145,255, Action buttons",
    "0,255,255, Panels",
    "0,0,255, Learnable text",
    "255,255,0, Formant marker",
    "150,150,0, Formant ghost marker",
    "255,0,0, Warning text",
    "255,720,0, Pending button",
    "255,255,255, Query/Alert background",
    "0,182,191, Dynfilter filter button",
    "127,127,127, Keyboard slider backgrounds",
    "0,255,0, Keyboard pitchwheel peg",
    "63,127,255, Keyboard Mod Wheel",
    "------------------ data end marker",
    "Add your own notes here:",
    "Copyright © 2020 A. N. Other",
    "The default theme",
    "END"
}; // Do not change the last five text lines!

#endif
