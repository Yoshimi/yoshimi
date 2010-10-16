/*
    Midi.h
  
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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

#ifndef MIDI_H
#define MIDI_H

#include <inttypes.h>

#define MAX_MIDI_BYTES 3

typedef struct {
    uint32_t event_frame;
    unsigned char bytes[MAX_MIDI_BYTES];
} midimessage;

typedef enum {
    MSG_noteoff =                128,
    MSG_noteon =                 144,
    MSG_polyphonic_aftertouch =  160,
    MSG_control_change =         176,
    MSG_program_change =         192,
    MSG_channel_aftertouch =     208,
    MSG_pitchwheel_control =     224,
    MSG_system_exclusive =       240,
    MSG_midi_time_code =         241, // the famous F1
    MSG_song_position_pointer =  242,
    MSG_songselect =             243,
    MSG_tunerequest =            246,
    MSG_endofsysex =             247,
    MSG_timing_clock =           248, // the infamous F8
    MSG_start =                  250,
    MSG_continue =               251,
    MSG_stop =                   252,
    MSG_activesensing =          254, 
} midimsgtype;

typedef enum {
    C_bankselectmsb =         0,
    C_modwheel =              1, // active
    C_breathcontroller =      2,
    C_undefined3 =            3,         
    C_footcontroller =        4,
    C_portamentotime =        5, 
    C_dataentrymsb =          6,
    C_volume =                7, // active
    C_balance =               8,
    C_undefined9 =            9,         
    C_pan =                  10, // active
    C_expression =           11, // active
    C_effectcontrol1 =       12, 
    C_effectcontrol2 =       13, // active
    C_undefined14 =          14,
    C_undefined15 =          15,
    C_generalpurpose1 =      16,
    C_generalpurpose2 =      17,
    C_generalpurpose3 =      18,
    C_generalpurpose4 =      19,
    C_undefined20 =          20,
    C_undefined21 =          21,
    C_undefined22 =          22,
    C_undefined23 =          23,
    C_undefined24 =          24,
    C_undefined25 =          25,
    C_undefined26 =          26,
    C_undefined27 =          27,
    C_undefined28 =          28,
    C_undefined29 =          29,
    C_undefined30 =          30,
    C_undefined31 =          31,
    C_bankselectlsb =        32,
    C_modwheellsb =          33,
    C_breathcontrollerlsb =  34,
    C_undefined3lsb =        35, 
    C_footcontrollerlsb =    36,
    C_portamentotimelsb =    37,
    C_dataentrylsb =         38,
    C_volumelsb =            39,
    C_balancelsb =           40,
    C_undefined9lsb =        41,
    C_panlsb =               42,
    C_expressionlsb =        43,
    C_effectcontrol1lsb =    44,
    C_effectcontrol2lsb =    45,
    C_undefined14lsb =       46,
    C_undefined15lsb =       47,
    // ...  
    C_sustain =              64, // active
    C_portamento =           65, // active
    C_sostenuto =            66,
    C_softpedal =            67,
    C_legatofootswitch =     68,
    C_hold2 =                69,
    C_soundcontroller1 =     70,
    C_filterq =              71, // active
    C_soundcontroller3 =     72,
    C_soundcontroller4 =     73, 
    C_filtercutoff =         74, // active
    C_bandwidth =            75, // active
    C_fmamp =                76, // active
    C_resonance_center =     77, // active
    C_resonance_bandwidth =  78, // active
    C_soundcontroller10 =    79,
    C_gpcontroller5lsb =     80, 
    C_gpcontroller6lsb =     81, 
    C_gpcontroller7lsb =     82, 
    C_gpcontroller8lsb =     83,
    C_portamentolsb =        84,
    C_undefined85 =          85,
    C_undefined86 =          86,
    C_undefined87 =          87,
    C_undefined88 =          88,
    C_undefined89 =          89,
    C_undefined90 =          90,
    C_undefined102 =        102,
    C_undefined103 =        103,
    C_undefined104 =        104,
    C_undefined105 =        105,
    C_undefined106 =        106,
    C_undefined107 =        107,
    C_undefined108 =        108,
    C_undefined109 =        109,
    C_undefined110 =        110,
    C_undefined111 =        111,
    C_undefined112 =        112,
    C_undefined113 =        113,
    C_undefined114 =        114,
    C_undefined115 =        115,
    C_undefined116 =        116,
    C_undefined117 =        117,
    C_undefined118 =        118,
    C_undefined119 =        119,
    C_allsoundsoff =        120, // active
    C_resetallcontrollers = 121, // active 
    C_allnotesoff =         123, // active
} midicontroltype;

#endif
