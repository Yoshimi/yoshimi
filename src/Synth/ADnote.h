/*
    ADnote.h - The "additive" synthesizer

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

    This file is a derivative of the ZynAddSubFX original, modified January 2010
*/

#ifndef AD_NOTE_H
#define AD_NOTE_H

#include "Synth/LegatoTypes.h"
#include "Synth/Envelope.h"
#include "Synth/LFO.h"
#include "DSP/Filter.h"
#include "Params/ADnoteParameters.h"
#include "Params/Controller.h"

// Globals

#define FM_AMP_MULTIPLIER 14.71280603

#define OSCIL_SMP_EXTRA_SAMPLES 5

class ADnote // ADDitive note
{
    public:
        ADnote(ADnoteParameters *pars, Controller *ctl_, float freq,
               float velocity, int portamento_, int midinote_, bool besilent);
        ~ADnote();

        void ADlegatonote(float freq_, float velocity_, int portamento_,
                          int midinote_, bool externcall);

        int noteout(float *outl, float *outr);
        void relasekey(void);
        bool finished(void) { return !NoteEnabled; };

        // ready - this is 0 if it is not ready (the parameters has to be computed)
        // or other value if the parameters has been computed and if it is ready to output
        char ready;

    private:
        void setfreq(int nvoice, float freq);
        void setfreqFM(int nvoice, float freq);
        void computecurrentparameters();
        void initparameters();
        void KillVoice(int nvoice);
        void KillNote(void);
        float getvoicebasefreq(int nvoice);
        float getFMvoicebasefreq(int nvoice);
        void ComputeVoiceOscillator_LinearInterpolation(int nvoice);
        void ComputeVoiceOscillator_CubicInterpolation(int nvoice);
        void ComputeVoiceOscillatorMorph(int nvoice);
        void ComputeVoiceOscillatorRingModulation(int nvoice);
        void ComputeVoiceOscillatorFrequencyModulation(int nvoice, int FMmode);
                    // FMmode=0 for phase modulation, 1 for Frequency modulation
        //  inline void ComputeVoiceOscillatorFrequencyModulation(int nvoice);
        void ComputeVoiceOscillatorPitchModulation(int nvoice);
        void fadein(float *smps);

        // GLOBALS
        ADnoteParameters *partparams;
        bool stereo; // allows note Panning
        int midinote;
        float velocity;
        float basefreq;

        bool NoteEnabled;
        Controller *ctl;

        // Global parameters

        struct ADnoteGlobal {
            //****************************
            // FREQUENCY GLOBAL PARAMETERS
            //****************************
            float Detune;//cents

            Envelope *FreqEnvelope;
            LFO *FreqLfo;

            //****************************
            // AMPLITUDE GLOBAL PARAMETERS
            //****************************
            float Volume; // [ 0 .. 1 ]

            float Panning; // [ 0 .. 1 ]

            Envelope *AmpEnvelope;
            LFO *AmpLfo;

            struct {
                int Enabled;
                float initialvalue;
                float dt;
                float t;
            } Punch;

            //*************************
            // FILTER GLOBAL PARAMETERS
            //*************************
            Filter *GlobalFilterL;
            Filter *GlobalFilterR;

            float FilterCenterPitch; // octaves
            float FilterQ;
            float FilterFreqTracking;

            Envelope *FilterEnvelope;

            LFO *FilterLfo;
        } NoteGlobalPar;

        //*****************
        // VOICE PARAMETERS
        //*****************
        struct ADnoteVoice {
            bool Enabled;
            int noisetype; // (sound/noise)
            int filterbypass;

            int DelayTicks;

            float *OscilSmp;

            //*********************
            // FREQUENCY PARAMETERS
            //*********************
            int fixedfreq;   // if the frequency is fixed to 440 Hz
            int fixedfreqET; // if the "fixed" frequency varies according to the note (ET)

            // cents = basefreq*VoiceDetune
            float Detune;
            float FineDetune;

            Envelope *FreqEnvelope;
            LFO *FreqLfo;

            //*********************
            // AMPLITUDE PARAMETERS
            //*********************
            float  Panning; // 0.0=left, 0.5 = center, 1.0 = right
            float Volume; // [-1.0 .. 1.0]

            Envelope *AmpEnvelope;
            LFO *AmpLfo;

            //******************
            // FILTER PARAMETERS
            //******************

            Filter *VoiceFilter;

            float FilterCenterPitch;
            float FilterFreqTracking;

            Envelope *FilterEnvelope;
            LFO *FilterLfo;

            //**********************
            // MODULLATOR PARAMETERS
            //**********************

            FMTYPE FMEnabled;

            int FMVoice;

            float *VoiceOut; // Voice Output used by other voices if use this as modullator
            float *FMSmp;    // Wave of the Voice

            float FMVolume;
            float FMDetune; //in cents

            Envelope *FMFreqEnvelope;
            Envelope *FMAmpEnvelope;
        } NoteVoicePar[NUM_VOICES];


        //**********************************************
        // INTERNAL VALUES OF THE NOTE AND OF THE VOICES
        //**********************************************
        float time; // time from the start of the note
    
        float oscposlo[NUM_VOICES];  // fractional part (skip)
        float oscfreqlo[NUM_VOICES];

        int oscposhi[NUM_VOICES];  // integer part (skip)
        int oscfreqhi[NUM_VOICES];

        // fractional part (skip) of the Modullator
        float oscposloFM[NUM_VOICES];
        float oscfreqloFM[NUM_VOICES];

        // integer part (skip) of the Modullator
        unsigned short int oscposhiFM[NUM_VOICES];
        unsigned short int oscfreqhiFM[NUM_VOICES];

        // used to compute and interpolate the amplitudes of voices and modullators
        float oldamplitude[NUM_VOICES];
        float newamplitude[NUM_VOICES];
        float FMoldamplitude[NUM_VOICES];
        float FMnewamplitude[NUM_VOICES];

        // used by Frequency Modulation (for integration)
        float FMoldsmp[NUM_VOICES];

        // temporary buffer
        float *tmpwave;

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
            struct { // Fade In-Out vars
                int length;
                float m;
                float step;
            } fade;
            struct { // Note parameters
                float freq;
                float vel;
                int portamento;
                int midinote;
            } param;
        } Legato;

        unsigned int samplerate;
        int buffersize;
        int oscilsize;
};

#endif




