/*
    ADnoteParameters.h - Parameters for ADnote (ADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2018, Will Godfrey
    Copyright 2020-2023 Kristian Amlie, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code.

*/

#ifndef AD_NOTE_PARAMETERS_H
#define AD_NOTE_PARAMETERS_H

#include "globals.h"
#include "Params/EnvelopeParams.h"
#include "Params/OscilParameters.h"
#include "Params/FilterParams.h"
#include "Params/LFOParams.h"
#include "Params/ParamCheck.h"
#include "Synth/Resonance.h"
#include "Synth/OscilGen.h"
#include "Misc/XMLwrapper.h"
#include "DSP/FFTwrapper.h"

enum FMTYPE { NONE, MORPH, RING_MOD, PHASE_MOD, FREQ_MOD, PW_MOD };

extern int ADnote_unison_sizes[];

class SynthEngine;

/*****************************************************************/
/*                    GLOBAL PARAMETERS                          */
/*****************************************************************/

struct ADnoteGlobalParam {
    bool PStereo;

    // Frequency global parameters
    ushort PDetune;       // fine detune
    ushort PCoarseDetune; // coarse detune + octave
    uchar  PDetuneType;   // detune type
    uchar  PBandwidth;    // how much the relative fine detunes of the voices are changed

    EnvelopeParams *FreqEnvelope;  // Frequency Envelope
    LFOParams      *FreqLfo;       // Frequency LFO

    // Amplitude global parameters
    char  PPanning; // 1 - left, 64 - center, 127 - right
    bool  PRandom;
    char  PWidth;
    float pangainL; // derived from PPanning
    float pangainR; // ^
    uchar PVolume;
    uchar PAmpVelocityScaleFunction;
    uchar PPunchStrength;
    uchar PPunchTime;
    uchar PPunchStretch;
    uchar PPunchVelocitySensing;

    EnvelopeParams *AmpEnvelope;
    LFOParams      *AmpLfo;

    // Adjustment factor for anti-pop fadein
    uchar Fadein_adjustment;

    FilterParams* GlobalFilter;  // Filter global parameters
    uchar PFilterVelocityScale;  // Filter velocity sensing
    uchar PFilterVelocityScaleFunction;
    EnvelopeParams* FilterEnvelope;
    LFOParams* FilterLfo;
    Resonance* Reson;
    uchar Hrandgrouping;         // how the randomness is applied to the harmonics
                                 // on more voices using the same oscillator
};


struct ADnoteVoiceParam { // Voice parameters
    uchar Enabled;
    uchar Unison_size;              // How many subvoices are used in this voice
    uchar Unison_frequency_spread;  // How subvoices are spread
    uchar Unison_phase_randomness;  // How much phase randomisation
    uchar Unison_stereo_spread;     // Stereo spread of the subvoices
    uchar Unison_vibrato;           // Vibrato of the subvoices (which makes the unison more "natural")
    uchar Unison_vibrato_speed;     // Medium speed of the vibrato of the subvoices
    uchar Unison_invert_phase;      // Unison invert phase
                                    // 0 = none, 1 = random, 2 = 50%, 3 = 33%, 4 = 25%
    uchar Type;                     // Type of the voice 0 = Sound, 1 = Noise
    uchar PDelay;                   // Voice Delay
    uchar Presonance;               // If resonance is enabled for this voice
    short Pextoscil;                // What external oscil should I use,
    short PextFMoscil;              // -1 for internal POscil & POscilFM
                                    // it is not allowed that the externoscil,
                                    // externFMoscil => current voice
    uchar Poscilphase, PFMoscilphase; // oscillator phases
    uchar Pfilterbypass;            // filter bypass
    OscilParameters *POscil;
    OscilGen        *OscilSmp;

    // Frequency parameters
    uchar Pfixedfreq;           // If the base frequency is fixed to 440 Hz
    uchar PfixedfreqET;         // Equal temperate (this is used only if the
                                // Pfixedfreq is enabled). If this parameter is 0,
                                // the frequency is fixed (to 440 Hz); if this
                                // parameter is 64, 1 MIDI halftone -> 1 frequency
                                // halftone
    ushort PDetune;
    ushort PCoarseDetune;
    uchar PDetuneType;

    uchar PBendAdjust;           // Pitch Bend
    uchar POffsetHz;

    uchar PFreqEnvelopeEnabled;  // Frequency Envelope
    EnvelopeParams *FreqEnvelope;

    uchar PFreqLfoEnabled;       // Frequency LFO
    LFOParams *FreqLfo;

    // Amplitude parameters
    uchar PPanning;              //  1 - left, 64 - center, 127 - right
                                 // panning is ignored if the instrument is mono
    bool  PRandom;
    char  PWidth;
    float pangainL;              // derived from PPanning
    float pangainR;              // ^
    uchar PVolume;
    uchar PVolumeminus;          // reverse voice phase relative to others

    uchar PAmpVelocityScaleFunction; // Velocity sensing

    uchar PAmpEnvelopeEnabled;   // Amplitude Envelope
    EnvelopeParams *AmpEnvelope;

    uchar PAmpLfoEnabled;        // Amplitude LFO
    LFOParams *AmpLfo;

    // Filter parameters
    uchar PFilterEnabled;        // Voice Filter
    FilterParams *VoiceFilter;

    uchar PFilterEnvelopeEnabled;// Filter Envelope
    EnvelopeParams *FilterEnvelope;

    uchar PFilterLfoEnabled;     // LFO Envelope
    LFOParams *FilterLfo;

    uchar PFilterVelocityScale;
    uchar PFilterVelocityScaleFunction;


    short PVoice;                // Voice that I use as external oscillator.
                                 // It is -1 if I use POscil(default).
                                 // It may not be equal or bigger than current voice

    // Modulator parameters
    uchar PFMEnabled;            // 0 = off, 1 = Morph, 2 = RM, 3 = PM, 4 = FM, 5 = PWM
    bool  PFMringToSide;         // allow carrier through
    short PFMVoice;              // Voice that I use as modullator instead of POscilFM.
                                 // It is -1 if I use POscilFM(default).
                                 // It may not be equal or bigger than current voice
    OscilParameters *POscilFM;   // Modullator oscillator
    OscilGen        *FMSmp;

    uchar  PFMVolume;                // Modulator Volume
    uchar  PFMVolumeDamp;            // Modulator damping at higher frequencies
    uchar  PFMVelocityScaleFunction; // Modulator Velocity Sensing
    uchar  PFMDetuneFromBaseOsc;     // Whether we inherit the base oscillator's detuning
    ushort PFMDetune;                // Fine Detune of the Modulator
    ushort PFMCoarseDetune;          // Coarse Detune of the Modulator
    uchar  PFMDetuneType;            // The detune type
    uchar  PFMFixedFreq;             // FM base freq fixed at 440Hz
    uchar  PFMFreqEnvelopeEnabled;   // Frequency Envelope of the Modulator
    EnvelopeParams*  FMFreqEnvelope;
    uchar  PFMAmpEnvelopeEnabled;    // Frequency Envelope of the Modulator
    EnvelopeParams*  FMAmpEnvelope;
};


class ADnoteParameters : public ParamBase
{
    public:
        ADnoteParameters(fft::Calc&, SynthEngine&);
       ~ADnoteParameters() override;
        void defaults()    override;
        void voiceDefaults(int n) {defaults(n);};
        void add2XML(XMLwrapper& xml);
        void getfromXML(XMLwrapper& xml);
        float getLimits(CommandBlock *getData);
        float getBandwidthDetuneMultiplier();
        float getUnisonFrequencySpreadCents(int nvoice);
        void setGlobalPan(char pan, uchar panLaw);
        void setVoicePan(int voice, char pan, uchar panLaw);
        ADnoteGlobalParam GlobalPar;
        ADnoteVoiceParam VoicePar[NUM_VOICES];
        /*
         * didn't want to make the following two public but could find
         * no other way to access them from UnifiedPresets.
         * Will.
         */
        void add2XMLsection(XMLwrapper& xml, int n);
        void getfromXMLsection(XMLwrapper& xml, int n);
        static int ADnote_unison_sizes[15];

    private:
        void defaults(int n); // n is the nvoice
        void enableVoice(int nvoice);
        void killVoice(int nvoice);

        fft::Calc& fft;
};

#endif
