/*
    ADnote.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others
    Copyright 2020-2021 Kristian Amlie & Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original

*/

#ifndef AD_NOTE_H
#define AD_NOTE_H

#include "Params/ADnoteParameters.h"
#include "Misc/RandomGen.h"

class ADnoteParameters;
class Controller;
class Envelope;
class LFO;
class Filter;

// Globals

#define FM_AMP_MULTIPLIER 14.71280603f

#define OSCIL_SMP_EXTRA_SAMPLES 5

class SynthEngine;

class ADnote
{
    public:
        ADnote(ADnoteParameters *adpars_, Controller *ctl_, float freq_, float velocity_,
               int portamento_, int midinote_, SynthEngine *_synth);
        ADnote(ADnote *topVoice_, float freq_, int phase_offset_, int subVoiceNumber_,
               float *parentFMmod_, bool forFM_);
        ADnote(const ADnote &orig, ADnote *topVoice_ = NULL, float *parentFMmod_ = NULL);
        ~ADnote();

        void construct();

        int noteout(float *outl, float *outr);
        void releasekey();
        bool finished() const
        {
            return NoteStatus == NOTE_DISABLED ||
                (NoteStatus != NOTE_KEEPALIVE && legatoFade == 0.0f);
        }
        void legatoFadeIn(float freq_, float velocity_, int portamento_, int midinote_);
        void legatoFadeOut(const ADnote &syncwith);

        // Whether the note has samples to output.
        // Currently only used for dormant legato notes.
        bool ready() { return legatoFade != 0.0f || legatoFadeStep != 0.0f; };

    private:

        void setfreq(int nvoice, float in_freq, float pitchdetune);
        void setfreqFM(int nvoice, float in_freq, float pitchdetune);
        void setPitchDetuneFromParent(float pitch)
        {
            detuneFromParent = pitch;
        }
        void setUnisonDetuneFromParent(float factor)
        {
            unisonDetuneFactorFromParent = factor;
        }
        void computeUnisonFreqRap(int nvoice);
        void computeNoteParameters(void);
        void computeWorkingParameters(void);
        void initParameters(void);
        void initSubVoices(void);
        void killVoice(int nvoice);
        void killNote(void);
        float getVoiceBaseFreq(int nvoice);
        float getFMVoiceBaseFreq(int nvoice);
        void computeVoiceOscillatorLinearInterpolation(int nvoice);
        void applyVoiceOscillatorMorph(int nvoice);
        void applyVoiceOscillatorRingModulation(int nvoice);
        void computeVoiceModulator(int nvoice, int FMmode);
        void computeVoiceModulatorLinearInterpolation(int nvoice);
        void applyAmplitudeOnVoiceModulator(int nvoice);
        void normalizeVoiceModulatorFrequencyModulation(int nvoice, int FMmode);
        void computeVoiceModulatorFrequencyModulation(int nvoice, int FMmode);
        void computeVoiceModulatorForFMFrequencyModulation(int nvoice);
        void computeVoiceOscillatorFrequencyModulation(int nvoice);
        void computeVoiceOscillatorForFMFrequencyModulation(int nvoice);
            // FMmode = 0 for phase modulation, 1 for Frequency modulation
        //  void ComputeVoiceOscillatorFrequencyModulation(int nvoice);
        void computeVoiceOscillatorPitchModulation(int nvoice);

        void computeVoiceNoise(int nvoice);
        void ComputeVoicePinkNoise(int nvoice);
        void ComputeVoiceSpotNoise(int nvoice);

        void computeVoiceOscillator(int nvoice);

        void fadein(float *smps);


        // Globals
        ADnoteParameters *adpars;
        bool  stereo;
        int   midinote;
        float velocity;
        float basefreq;

        enum {
            NOTE_DISABLED,
            NOTE_ENABLED,
            NOTE_KEEPALIVE
        } NoteStatus;
        Controller *ctl;

        // Global parameters
        struct ADnoteGlobal {
            // Frequency global parameters
            float  Detune; // cents
            Envelope *FreqEnvelope;
            LFO      *FreqLfo;

            // Amplitude global parameters
            float Volume;  //  0 .. 1
            float randpanL;
            float randpanR;

            Envelope *AmpEnvelope;
            LFO      *AmpLfo;

            float Fadein_adjustment;
            struct {
                int      Enabled;
                float initialvalue, dt, t;
            } Punch;

            // Filter global parameters
            Filter *GlobalFilterL;
            Filter *GlobalFilterR;
            Envelope *FilterEnvelope;
            LFO      *FilterLfo;
        } NoteGlobalPar;

        // Voice parameters
        struct ADnoteVoice {
            bool Enabled;
            int Voice;        // Voice I use as source.
            int noisetype;    // (sound/noise)
            int filterbypass;
            int DelayTicks;
            float *OscilSmp;  // Waveform of the Voice. Shared with sub voices.
            int phase_offset; // PWM emulation

            // Frequency parameters
            int fixedfreq;   // if the frequency is fixed to 440 Hz
            int fixedfreqET; // if the "fixed" frequency varies according to the note (ET)

            float Detune;     // cents = basefreq * VoiceDetune
            float FineDetune;
            float BendAdjust;
            float OffsetHz;

            Envelope *FreqEnvelope;
            LFO      *FreqLfo;

            // Amplitude parameters
            float Volume;  // -1.0 .. 1.0
            float Panning; // 0.0 = left, 0.5 = center, 1.0 = right
            float randpanL;
            float randpanR;

            Envelope *AmpEnvelope;
            LFO      *AmpLfo;

            struct {
                int   Enabled;
                float initialvalue, dt, t;
            } Punch;

            // Filter parameters
            Filter   *VoiceFilterL;
            Filter   *VoiceFilterR;

            Envelope *FilterEnvelope;
            LFO      *FilterLfo;

            // Modulator parameters
            FMTYPE FMEnabled;
            bool FMringToSide;
            unsigned char FMFreqFixed;
            int    FMVoice;
            float *VoiceOut; // Voice Output used by other voices if use this as modullator
            float *FMSmp;    // Wave of the Voice. Shared by sub voices.
            int    FMphase_offset;
            float  FMVolume;
            bool FMDetuneFromBaseOsc;  // Whether we inherit the base oscillator's detuning
            float  FMDetune; // in cents
            Envelope *FMFreqEnvelope;
            Envelope *FMAmpEnvelope;
        } NoteVoicePar[NUM_VOICES];

        // Internal values of the note and of the voices
        float time; // time from the start of the note
        int Tspot; // spot noise noise interrupt time

        RandomGen paramRNG; // A preseeded random number generator, reseeded
                            // with a known seed every time parameters are
                            // updated. This allows parameters to be changed
                            // smoothly. New notes will get a new seed.
        uint32_t paramSeed; // The seed for paramRNG.

        //pinking filter (Paul Kellet)
        float pinking[NUM_VOICES][14];

        int unison_size[NUM_VOICES]; // the size of unison for a single voice

        float unison_stereo_spread[NUM_VOICES]; // stereo spread of unison subvoices (0.0=mono,1.0=max)

        float *oscposlo[NUM_VOICES], *oscfreqlo[NUM_VOICES]; // fractional part (skip)

        int *oscposhi[NUM_VOICES], *oscfreqhi[NUM_VOICES]; // integer part (skip)

        float *oscposloFM[NUM_VOICES], *oscfreqloFM[NUM_VOICES]; // fractional part (skip) of the Modullator

        float *unison_base_freq_rap[NUM_VOICES]; // the unison base_value

        float *unison_freq_rap[NUM_VOICES]; // how the unison subvoice's frequency is changed (1.0 for no change)

        // These are set by parent voices.
        float detuneFromParent;             // How much the voice should be detuned.
        float unisonDetuneFactorFromParent; // How much the voice should be detuned from unison.

        bool *unison_invert_phase[NUM_VOICES]; // which unison subvoice has phase inverted

        struct { // unison vibratto
            float  amplitude; // amplitude which be added to unison_freq_rap
            float *step;      // value which increments the position
            float *position;  // between -1.0 and 1.0
        } unison_vibratto[NUM_VOICES];

        // integer part (skip) of the Modullator
        int *oscposhiFM[NUM_VOICES];
        int *oscfreqhiFM[NUM_VOICES];

        float oldamplitude[NUM_VOICES];  // used to compute and interpolate the
        float newamplitude[NUM_VOICES];  // amplitudes of voices and modullators
        float FMoldamplitude[NUM_VOICES];
        float FMnewamplitude[NUM_VOICES];

        float *FMoldsmp[NUM_VOICES]; // used by Frequency Modulation (for integration)

        float *FMFMoldPhase[NUM_VOICES]; // use when rendering FM modulator with parent FM
        float *FMFMoldInterpPhase[NUM_VOICES];
        float *FMFMoldPMod[NUM_VOICES];
        float *oscFMoldPhase[NUM_VOICES]; // use when rendering oscil with parent FM that will
                                         // be used for FM
        float *oscFMoldInterpPhase[NUM_VOICES];
        float *oscFMoldPMod[NUM_VOICES];
        bool forFM; // Whether this voice will be used for FM modulation.

        float **tmpwave_unison;
        int max_unison;

        float **tmpmod_unison;
        bool freqbasedmod[NUM_VOICES];

        float globaloldamplitude; // interpolate the amplitudes
        float globalnewamplitude;

        char firsttick[NUM_VOICES]; // 1 - if it is the fitst tick.
                                    // used to fade in the sound

        int portamento; // 1 if the note has portamento

        float bandwidthDetuneMultiplier; // how the fine detunes are made bigger or smaller

        // Legato vars
        float legatoFade;
        float legatoFadeStep;

        float pangainL;
        float pangainR;

        ADnote **subVoice[NUM_VOICES];
        ADnote **subFMVoice[NUM_VOICES];

        int subVoiceNumber;
        // For sub voices: The original, "topmost" voice that triggered this
        // one.
        ADnote *topVoice;
        // For sub voices: Pointer to the closest parent that has
        // phase/frequency modulation.
        float *parentFMmod;

        Presets::PresetsUpdate paramsUpdate;

        SynthEngine *synth;
};

#endif
