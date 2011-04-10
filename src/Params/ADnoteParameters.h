/*
    ADnoteParameters.h - Parameters for ADnote (ADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of ZynAddSubFX original code, modified April 2011
*/

#ifndef AD_NOTE_PARAMETERS_H
#define AD_NOTE_PARAMETERS_H

#include "Params/EnvelopeParams.h"
#include "Params/LFOParams.h"
#include "Params/FilterParams.h"
#include "Synth/OscilGen.h"
#include "Synth/Resonance.h"
#include "Misc/XMLwrapper.h"
#include "DSP/FFTwrapper.h"
#include "Params/Presets.h"

class Microtonal;

enum FMTYPE { NONE, MORPH, RING_MOD, PHASE_MOD, FREQ_MOD, PITCH_MOD };

extern int ADnote_unison_sizes[];

/*****************************************************************/
/*                    GLOBAL PARAMETERS                          */
/*****************************************************************/

struct ADnoteGlobalParam {
    bool PStereo;

    // Frequency global parameters
    unsigned short int PDetune;       // fine detune
    unsigned short int PCoarseDetune; // coarse detune + octave
    unsigned char PDetuneType;        // detune type
    unsigned char PBandwidth;         // how much the relative fine detunes of
                                      // the voices are changed
    EnvelopeParams *FreqEnvelope;     // Frequency Envelope
    LFOParams      *FreqLfo;          // Frequency LFO

    // Amplitude global parameters
    char  PPanning; // 0 - random, 1 - left, 64 - center, 127 - right
    //bool  randomPan;
    float pangainL; // derived from PPanning
    float pangainR; // ^
    unsigned char PVolume;
    unsigned char PAmpVelocityScaleFunction;
    unsigned char PPunchStrength;
    unsigned char PPunchTime;
    unsigned char PPunchStretch;
    unsigned char PPunchVelocitySensing;

    EnvelopeParams *AmpEnvelope;
    LFOParams      *AmpLfo;

    FilterParams *GlobalFilter;         // Filter global parameters
    unsigned char PFilterVelocityScale; // Filter velocity sensing
    unsigned char PFilterVelocityScaleFunction;
    EnvelopeParams *FilterEnvelope;
    LFOParams *FilterLfo;
    Resonance *Reson;
    unsigned char Hrandgrouping; // how the randomness is applied to the harmonics
                                 // on more voices using the same oscillator
};


struct ADnoteVoiceParam { // Voice parameters
    unsigned char Enabled;
    unsigned char Unison_size;              // How many subvoices are used in this voice
    unsigned char Unison_frequency_spread;  // How subvoices are spread
    unsigned char Unison_stereo_spread;     // Stereo spread of the subvoices
    unsigned char Unison_vibratto;          // Vibratto of the subvoices (which makes the unison more "natural")
    unsigned char Unison_vibratto_speed;    // Medium speed of the vibratto of the subvoices
    unsigned char Unison_invert_phase;      // Unison invert phase
                                            // 0 = none, 1 = random, 2 = 50%, 3 = 33%, 4 = 25%
    unsigned char Type;                     // Type of the voice 0 = Sound, 1 = Noise
    unsigned char PDelay;                   // Voice Delay
    unsigned char Presonance;               // If resonance is enabled for this voice
    int           Pextoscil,                // What external oscil should I use,
                  PextFMoscil;              // -1 for internal OscilSmp & FMSmp
                                            // it is not allowed that the externoscil,
                                            // externFMoscil => current voice
    unsigned char Poscilphase;              // oscillator phases
    unsigned char PFMoscilphase;
    unsigned char Pfilterbypass;            // filter bypass
    OscilGen     *OscilSmp;

    // Frequency parameters
    unsigned char Pfixedfreq;
    unsigned char PfixedfreqET; // Equal temperate (this is used only if the
                                // Pfixedfreq is enabled). If this parameter is 0,
                                // the frequency is fixed (to 440 Hz); if this
                                // parameter is 64, 1 MIDI halftone -> 1 frequency
                                // halftone
    unsigned short int PDetune;
    unsigned short int PCoarseDetune;
    unsigned char PDetuneType;

    unsigned char PFreqEnvelopeEnabled;      // Frequency Envelope
    EnvelopeParams *FreqEnvelope;

    unsigned char PFreqLfoEnabled;           // Frequency LFO
    LFOParams *FreqLfo;

    // Amplitude parameters
    unsigned char PPanning; // 0 - random, 1 - left, 64 - center, 127 - right
                            // panning is ignored if the instrument is mono
    //bool  randomPan;
    float pangainL;         // derived from PPanning
    float pangainR;         // ^
    unsigned char PVolume;
    unsigned char PVolumeminus; // ?? doesn't seem to be currently associated
                                // with anything

    unsigned char PAmpVelocityScaleFunction; // Velocity sensing

    unsigned char PAmpEnvelopeEnabled;       // Amplitude Envelope
    EnvelopeParams *AmpEnvelope;

    unsigned char PAmpLfoEnabled;            // Amplitude LFO
    LFOParams *AmpLfo;

    // Filter parameters
    unsigned char PFilterEnabled;            // Voice Filter
    FilterParams *VoiceFilter;

    unsigned char PFilterEnvelopeEnabled;    // Filter Envelope
    EnvelopeParams *FilterEnvelope;

    unsigned char PFilterLfoEnabled;         // LFO Envelope
    LFOParams *FilterLfo;

    // Modullator parameters
    unsigned char PFMEnabled; // 0 = off, 1 = Morph, 2 = RM, 3 = PM, 4 = FM..
    short int     PFMVoice;   // Voice that I use as modullator instead of FMSmp.
                              // It is -1 if I use FMSmp(default).
                              // It may not be equal or bigger than current voice
    OscilGen *FMSmp;          // Modullator oscillator

    unsigned char      PFMVolume;                // Modulator Volume
    unsigned char      PFMVolumeDamp;            // Modulator damping at higher frequencies
    unsigned char      PFMVelocityScaleFunction; // Modulator Velocity Sensing
    unsigned short int PFMDetune;                // Fine Detune of the Modulator
    unsigned short int PFMCoarseDetune;          // Coarse Detune of the Modulator
    unsigned char      PFMDetuneType;            // The detune type
    unsigned char      PFMFreqEnvelopeEnabled;   // Frequency Envelope of the Modulator
    EnvelopeParams    *FMFreqEnvelope;
    unsigned char      PFMAmpEnvelopeEnabled;    // Frequency Envelope of the Modulator
    EnvelopeParams    *FMAmpEnvelope;
};


class ADnoteParameters : public Presets
{
    public:
        ADnoteParameters(Microtonal *micro_, FFTwrapper *fft_);
        ~ADnoteParameters();
        void defaults(void);
        void add2XML(XMLwrapper *xml);
        void getfromXML(XMLwrapper *xml);
        float getBandwidthDetuneMultiplier(void);
        float getUnisonFrequencySpreadCents(int nvoice);
        int getUnisonSizeIndex(int nvoice);
        void setUnisonSizeIndex(int nvoice, int index);
        void setGlobalPan(char pan);
        void setVoicePan(int voice, char pan);
        bool randomGlobalPan(void) { return !GlobalPar.PPanning; }
        bool randomVoicePan(int nvoice) { return !VoicePar[nvoice].PPanning; }

        ADnoteGlobalParam GlobalPar;
        ADnoteVoiceParam VoicePar[NUM_VOICES];
        Microtonal *microtonal;
        static int ADnote_unison_sizes[15];

    private:
        void defaults(int n); // n is the nvoice
        void enableVoice(int nvoice);
        void killVoice(int nvoice);
        void add2XMLsection(XMLwrapper *xml, int n);
        void getfromXMLsection(XMLwrapper *xml, int n);

        FFTwrapper *fft;
};

#endif
