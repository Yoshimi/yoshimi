/*
    MidiControl.h

    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2011, Alan Calvert

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

#ifndef MIDI_CONTROL_H
#define MIDI_CONTROL_H

typedef enum {
    C_NULL =               1002,
    C_programchange =      1001,
    C_pitchwheel =         1000,
    C_channelpressure =     901,
    C_keypressure =         900,
    C_bankselectmsb =         0,
    C_breath =                2,
    C_expression =           11,
    C_panning =              10,
    C_bankselectlsb =        32,
    C_filtercutoff =         74,
    C_filterq =              71,
    C_bandwidth =            75,
    C_modwheel =              1,
    C_fmamp =                76,
    C_volume =                7,
    C_sustain =              64,
    C_legatofootswitch =     68,
    C_allnotesoff =         123,
    C_allsoundsoff =        120,
    C_resetallcontrollers = 121,
    C_portamento =           65,
    C_resonance_center =     77,
    C_resonance_bandwidth =  78,
    C_nrpnL =                98,
    C_nrpnH =                99,
    C_dataH =                 6,
    C_dataL =                38,
    C_dataI =                96,
    C_dataD =                97,
} MidiControllers;

#endif
