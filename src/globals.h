/*
    globals.h - it contains program settings and the program capabilities
                like number of parts, of effects

    Original ZynAddfSubFX author Nasca Octavian Paul
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

#ifndef GLOBALS_H
#define GLOBALS_H

#define MAX_AD_HARMONICS 128

#define MAX_SUB_HARMONICS 64

// The maximum number of samples that are used for 1 PADsynth instrument(or item)
#define PAD_MAX_SAMPLES 64

// Number of parts
#define NUM_MIDI_PARTS 16

// Number of Midi channes
#define NUM_MIDI_CHANNELS 16

// The number of voices of additive synth for a single note
#define NUM_VOICES 8

// The poliphony (notes)
#define POLIPHONY 60

// Number of system effects
#define NUM_SYS_EFX 4

// Number of insertion effects
#define NUM_INS_EFX 8

// Number of part's insertion effects
#define NUM_PART_EFX 3

// Maximum number of the instrument on a part
#define NUM_KIT_ITEMS 16

// How is applied the velocity sensing
#define VELOCITY_MAX_SCALE 8.0

// The maximum length of instrument's name
#define PART_MAX_NAME_LEN 30

// The maximum number of bands of the equaliser
#define MAX_EQ_BANDS 8
#if (MAX_EQ_BANDS>=20)
#error "Too many EQ bands in globals.h"
#endif

// Maximum filter stages
#define MAX_FILTER_STAGES 5

// Formant filter (FF) limits
#define FF_MAX_VOWELS 6
#define FF_MAX_FORMANTS 12
#define FF_MAX_SEQUENCE 8

#define LOG_2 0.693147181
#define PI 3.1415926536
#define LOG_10 2.302585093

// The threshold for the amplitude interpolation used if the amplitude
// is changed (by LFO's or Envelope's). If the change of the amplitude
// is below this, the amplitude is not interpolated
#define AMPLITUDE_INTERPOLATION_THRESHOLD 0.0001

// How the amplitude threshold is computed
#define ABOVE_AMPLITUDE_THRESHOLD(a,b) ( ( 2.0*fabsf( (b) - (a) ) /  \
      ( fabsf( (b) + (a) + 0.0000000001) ) ) > AMPLITUDE_INTERPOLATION_THRESHOLD )

// Interpolate Amplitude
#define INTERPOLATE_AMPLITUDE(a,b,x,size) ( (a) + \
      ( (b) - (a) ) * (float)(x) / (float)(size) )


// dB
#define dB2rap(dB) ((exp10f((dB) / 20.0)))
#define rap2dB(rap) ((20 * log10f(rap)))

// The random generator (0.0..1.0)
#define RND (rand()/(RAND_MAX+1.0))

//#define ZERO(data,size) {char *data_=(char *) data;for (int i=0;i<size;++i) data_[i]=0;};
#define ZERO(data, size) (memset((void*)data, 0, size))

#define ZERO_REALTYPE(data, size) (memset((void*)data, 0, size * sizeof(float)))
/*    {REALTYPE *data_=(REALTYPE *) data; \
    for (int i = 0; i < size; ++i) data_[i] = 0.0; \
    };
*/

enum ONOFFTYPE {
    OFF = 0,
    ON = 1
};

enum LegatoMsg {
    LM_Norm,
    LM_FadeIn,
    LM_FadeOut,
    LM_CatchUp,
    LM_ToNorm
};

#endif // GLOBALS_H
