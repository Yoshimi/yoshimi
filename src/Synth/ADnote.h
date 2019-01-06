/*
    ADnote.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2017, Will Godfrey & others

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
    Modified September 2017
*/

#ifndef AD_NOTE_H
#define AD_NOTE_H

#include "Misc/SynthHelper.h"
#include "Synth/LegatoTypes.h"
#include "Misc/Float2Int.h"

class ADnoteParameters;
class Controller;
class Envelope;
class LFO;
class Filter;

// Globals

#define FM_AMP_MULTIPLIER 14.71280603f

#define OSCIL_SMP_EXTRA_SAMPLES 5

class SynthEngine;

class ADnote : private SynthHelper, private Float2Int
{
    public:
        ADnote(ADnoteParameters *adpars_, Controller *ctl_, float freq_, float velocity_,
               int portamento_, int midinote_, bool besilent, SynthEngine *_synth);
        ~ADnote();

        int noteout(float *outl, float *outr);
        void releasekey();
        int finished() const;
        void ADlegatonote(float freq_, float velocity_, int portamento_,
                          int midinote_, bool externcall);
        char ready;

    private:

        void setfreq(int nvoice, float in_freq);
        void setfreqFM(int nvoice, float in_freq);
        void computeUnisonFreqRap(int nvoice);
        void computeCurrentParameters(void);
        void initParameters(void);
        void killVoice(int nvoice);
        void killNote(void);
        float getVoiceBaseFreq(int nvoice);
        float getFMVoiceBaseFreq(int nvoice);
        void computeVoiceOscillatorLinearInterpolation(int nvoice);
        void computeVoiceOscillatorCubicInterpolation(int nvoice);
        void computeVoiceOscillatorMorph(int nvoice);
        void computeVoiceOscillatorRingModulation(int nvoice);
        void computeVoiceOscillatorFrequencyModulation(int nvoice, int FMmode);
            // FMmode = 0 for phase modulation, 1 for Frequency modulation
        //  void ComputeVoiceOscillatorFrequencyModulation(int nvoice);
        void computeVoiceOscillatorPitchModulation(int nvoice);

        void computeVoiceNoise(int nvoice);
        void ComputeVoicePinkNoise(int nvoice);

        void fadein(float *smps);


        // Globals
        ADnoteParameters *adpars;
        bool  stereo;
        int   midinote;
        float velocity;
        float basefreq;

        bool NoteEnabled;
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
            float  FilterCenterPitch; // octaves
            float  FilterQ;
            float  FilterFreqTracking;
            Envelope *FilterEnvelope;
            LFO      *FilterLfo;
        } NoteGlobalPar;

        // Voice parameters
        struct ADnoteVoice {
            bool Enabled;
            int noisetype;    // (sound/noise)
            int filterbypass;
            int DelayTicks;
            float *OscilSmp;  // Waveform of the Voice
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

            float  FilterCenterPitch;
            float  FilterFreqTracking;

            Envelope *FilterEnvelope;
            LFO      *FilterLfo;

            // Modulator parameters
            FMTYPE FMEnabled;
            unsigned char FMFreqFixed;
            int    FMVoice;
            float *VoiceOut; // Voice Output used by other voices if use this as modullator
            float *FMSmp;    // Wave of the Voice
            float  FMVolume;
            float  FMDetune; // in cents
            Envelope *FMFreqEnvelope;
            Envelope *FMAmpEnvelope;
        } NoteVoicePar[NUM_VOICES];

        // Internal values of the note and of the voices
        float time; // time from the start of the note

        //pinking filter (Paul Kellet)
        float pinking[NUM_VOICES][14];

        int unison_size[NUM_VOICES]; // the size of unison for a single voice

        float unison_stereo_spread[NUM_VOICES]; // stereo spread of subvoices (0.0=mono,1.0=max)

        float *oscposlo[NUM_VOICES], *oscfreqlo[NUM_VOICES]; // fractional part (skip)

        int *oscposhi[NUM_VOICES], *oscfreqhi[NUM_VOICES]; // integer part (skip)

        float *oscposloFM[NUM_VOICES], *oscfreqloFM[NUM_VOICES]; // fractional part (skip) of the Modullator

        float *unison_base_freq_rap[NUM_VOICES]; // the unison base_value

        float *unison_freq_rap[NUM_VOICES]; // how the unison subvoice's frequency is changed (1.0 for no change)

        bool *unison_invert_phase[NUM_VOICES]; // which subvoice has phase inverted

        struct { // unison vibratto
            float  amplitude; // amplitude which be added to unison_freq_rap
            float *step;      // value which increments the position
            float *position;  // between -1.0 and 1.0
        } unison_vibratto[NUM_VOICES];

        // integer part (skip) of the Modullator
        unsigned int *oscposhiFM[NUM_VOICES];
        unsigned int *oscfreqhiFM[NUM_VOICES];

        float oldamplitude[NUM_VOICES];  // used to compute and interpolate the
        float newamplitude[NUM_VOICES];  // amplitudes of voices and modullators
        float FMoldamplitude[NUM_VOICES];
        float FMnewamplitude[NUM_VOICES];

        float *FMoldsmp[NUM_VOICES]; // used by Frequency Modulation (for integration)

        float *tmpwavel; // temporary buffers
        float *tmpwaver;
        float **tmpwave_unison;
        int max_unison;

        float *bypassl; // Filter bypass samples
        float *bypassr;

        float globaloldamplitude; // interpolate the amplitudes
        float globalnewamplitude;

        char firsttick[NUM_VOICES]; // 1 - if it is the fitst tick.
                                    // used to fade in the sound

        int portamento; // 1 if the note has portamento

        float bandwidthDetuneMultiplier; // how the fine detunes are made bigger or smaller

        // Legato vars
        struct {
            bool silent;
            float lastfreq;
            LegatoMsg msg;
            int decounter;
            struct {
                // Fade In/Out vars
                int length;
                float m;
                float step;
            } fade;
            struct {
                // Note parameters
                float freq;
                float vel;
                int portamento;
                int midinote;
            } param;
        } Legato;

        float pangainL;
        float pangainR;

        SynthEngine *synth;
};


inline int ADnote::finished() const // Check if the note is finished
{
    return (NoteEnabled) ? 0 : 1;
}

#endif
