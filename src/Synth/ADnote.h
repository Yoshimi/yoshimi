/*
    ADnote.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of a ZynAddSubFX original, modified October 2010
*/

#ifndef AD_NOTE_H
#define AD_NOTE_H

#include "Misc/SynthHelper.h"
#include "Synth/Carcass.h"
#include "Synth/LegatoTypes.h"

class ADnoteParameters;
class Controller;
class Envelope;
class LFO;
class Filter;

// Globals

#define FM_AMP_MULTIPLIER 14.71280603f

#define OSCIL_SMP_EXTRA_SAMPLES 5

class ADnote : public Carcass, private SynthHelper
{
    public:
        ADnote(ADnoteParameters *pars, Controller *ctl_, float freq_,
               float velocity_, int portamento_, int midinote_,
               bool besilent);
        ~ADnote();

        int noteout(float *outl, float *outr);
        void relasekey();
        int finished() const;
        void ADlegatonote(float freq_, float velocity_, int portamento_,
                          int midinote_, bool externcall);
        char ready;

    private:

        void setfreq(int nvoice, float in_freq);
        void setfreqFM(int nvoice, float in_freq);
        void computeUnisonFreqRap(int nvoice);
        void computeCurrentParameters();
        void initParameters();
        void killVoice(int nvoice);
        void killNote();
        float getVoiceBaseFreq(int nvoice) const;
        float getFMVoiceBaseFreq(int nvoice) const;
        void computeVoiceOscillator_LinearInterpolation(int nvoice);
        void computeVoiceOscillator_CubicInterpolation(int nvoice);
        void computeVoiceOscillatorMorph(int nvoice);
        void computeVoiceOscillatorRingModulation(int nvoice);
        void computeVoiceOscillatorFrequencyModulation(int nvoice, int FMmode);
            // FMmode = 0 for phase modulation, 1 for Frequency modulation
        //  void ComputeVoiceOscillatorFrequencyModulation(int nvoice);
        void computeVoiceOscillatorPitchModulation(int nvoice);

        void computeVoiceNoise(int nvoice);

        void fadein(float *smps) const;


        // Globals
        ADnoteParameters *partparams;
        unsigned char stereo; // allows note Panning
        int midinote;
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
            float  Volume;  // [ 0 .. 1 ]
            float  Panning; // [ 0 .. 1 ]
            Envelope *AmpEnvelope;
            LFO      *AmpLfo;
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
            int noisetype; // (sound/noise)
            int filterbypass;
            int DelayTicks;
            float *OscilSmp; // Waveform of the Voice

            // Frequency parameters
            int fixedfreq;   // if the frequency is fixed to 440 Hz
            int fixedfreqET; // if the "fixed" frequency varies according to the note (ET)

            // cents = basefreq*VoiceDetune
            float Detune;
            float FineDetune;

            Envelope *FreqEnvelope;
            LFO      *FreqLfo;

            // Amplitude parameters
            float  Panning; // 0.0=left, 0.5 = center, 1.0 = right
            float  Volume;  // [-1.0 .. 1.0]

            Envelope *AmpEnvelope;
            LFO      *AmpLfo;

            // Filter parameters
            Filter   *VoiceFilterL;
            Filter   *VoiceFilterR;

            float  FilterCenterPitch;
            float  FilterFreqTracking;

            Envelope *FilterEnvelope;
            LFO      *FilterLfo;

            // Modullator parameters
            FMTYPE FMEnabled;
            int    FMVoice;
            float *VoiceOut; // Voice Output used by other voices if use this as modullator
            float *FMSmp; // Wave of the Voice
            float  FMVolume;
            float  FMDetune; // in cents
            Envelope *FMFreqEnvelope;
            Envelope *FMAmpEnvelope;
        } NoteVoicePar[NUM_VOICES];

        // Internal values of the note and of the voices
        float time; // time from the start of the note

        int unison_size[NUM_VOICES]; //the size of unison for a single voice

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

        // used to compute and interpolate the amplitudes of voices and modullators
        float oldamplitude[NUM_VOICES];
        float newamplitude[NUM_VOICES];
        float FMoldamplitude[NUM_VOICES];
        float FMnewamplitude[NUM_VOICES];

        // used by Frequency Modulation (for integration)
        float *FMoldsmp[NUM_VOICES];

        // temporary buffers
        float *tmpwavel;
        float *tmpwaver;
        float **tmpwave_unison;
        int max_unison;

        // Filter bypass samples
        float *bypassl;
        float *bypassr;

        // interpolate the amplitudes
        float globaloldamplitude;
        float globalnewamplitude;

        // 1 - if it is the fitst tick (used to fade in the sound)
        char firsttick[NUM_VOICES];

        // 1 if the note has portamento
        int portamento;

        // how the fine detunes are made bigger or smaller
        float bandwidthDetuneMultiplier;

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
                float freq, vel;
                int portamento;
                int midinote;
            } param;
        } Legato;
};

#endif




