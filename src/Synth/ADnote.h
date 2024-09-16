/*
    ADnote.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others
    Copyright 2020-2021 Kristian Amlie & Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original

*/

#ifndef AD_NOTE_H
#define AD_NOTE_H

#include "Params/ADnoteParameters.h"
#include "Misc/RandomGen.h"
#include "DSP/FFTwrapper.h"
#include "Misc/Alloc.h"

#include <memory>
#include <array>

using std::unique_ptr;

class ADnoteParameters;
class SynthEngine;
class Controller;
class Envelope;
class Filter;
class LFO;

// Globals

#define FM_AMP_MULTIPLIER 14.71280603f



/* Helper to either manage sample data or link to another voice's data.
 * This class allows to mimic the behaviour of the original code base,
 * while encapsulating and automatically managing the allocation.
 * Initially created empty, it can either allocate a buffer or attach to
 * existing storage managed elsewhere; Ownership is locked subsequently.
 * Beyond that, SampleHolder can be used like a fft::Waveform in Synth code.
 * Warning: beware of slicing -- use only as nested component or local object.
 */
class SampleHolder
    : public fft::Waveform
{
    bool ownData = false;

public:
        // by default created in empty state
        SampleHolder() : fft::Waveform() { }

        SampleHolder(SampleHolder const& r)
            : fft::Waveform()
        {
            if (r.size() > 0)
                throw std::logic_error("fully engaged SampleHolder not meant to be copied");
        }

        SampleHolder(SampleHolder && rr)
            : fft::Waveform()
            , ownData(rr.ownData)
        {
            if (rr.size() > 0)
            {
                if (ownData) // transfer ownership
                    swap(*this, rr);
                else
                    attach(rr);
            }
        }
        // Assignment to existing objects not permitted
        SampleHolder& operator=(SampleHolder const&) =delete;
        SampleHolder& operator=(SampleHolder &&)     =delete;

        /* Note: SampleHolder can be an "alias" to another SampleHolder;
         *       and in this case we don't take ownership of the data allocation */
       ~SampleHolder()
        {
            if (not ownData) detach();
            // otherwise the parent dtor will automatically discard storage
        }

        void allocateWaveform(size_t tableSize)
        {
            if (size() > 0) throw std::logic_error("already engaged.");
            fft::Waveform allocation(tableSize);
            swap(*this, allocation);
            ownData = true;
        }

        void copyWaveform(SampleHolder const& src)
        {
            if (size() > 0) throw std::logic_error("already engaged.");
            if (src.size() == 0) return;
            allocateWaveform(src.size());
            fft::Waveform::operator=(src);
        }

        void attachReference(fft::Waveform& existing)
        {
            if (size() > 0 and ownData)
                throw std::logic_error("SampleHolder already owns and manages a data allocation");
            attach(existing);
            ownData = false;
        }
};



class ADnote
{
        ADnote(ADnoteParameters& adpars_, Controller& ctl_, Note note_, bool portamento_
              ,ADnote *topVoice_, int subVoiceNr, int phaseOffset, float *parentFMmod_, bool forFM_);
    public:
        ADnote(ADnoteParameters& adpars_, Controller& ctl_, Note, bool portamento_);
        ADnote(ADnote *topVoice_, float freq_, int phase_offset_, int subVoiceNumber_,
               float *parentFMmod_, bool forFM_);
        ADnote(const ADnote &orig, ADnote *topVoice_ = NULL, float *parentFMmod_ = NULL);
       ~ADnote();

        // shall not be moved or assigned
        ADnote(ADnote&&)                 = delete;
        ADnote& operator=(ADnote&&)      = delete;
        ADnote& operator=(ADnote const&) = delete;

        void noteout(float *outl, float *outr);
        void releasekey();
        bool finished() const { return noteStatus == NOTE_DISABLED; }
        void performPortamento(Note);
        void legatoFadeIn(Note);
        void legatoFadeOut();

    private:
        void construct();
        void allocateUnison(size_t unisonCnt, size_t buffSize);

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
        void computeNoteParameters();
        void computeWorkingParameters();
        void computePhaseOffsets(int nvoice);
        void computeFMPhaseOffsets(int nvoice);
        void initParameters();
        void initSubVoices();
        void killVoice(int nvoice);
        void killNote();
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

        void fadein(Samples& smps);


        // Globals
        SynthEngine& synth;
        ADnoteParameters& adpars;
        ParamBase::ParamsUpdate paramsUpdate;
        Controller& ctl;

        Note note;
        bool stereo;

        enum NoteStatus {
            NOTE_DISABLED,
            NOTE_ENABLED,
            NOTE_LEGATOFADEOUT
        } noteStatus;

        // Global parameters
        struct ADnoteGlobal {
            //****************************
            // FREQUENCY GLOBAL PARAMETERS
            //****************************
            float detune; // in cents

            unique_ptr<Envelope> freqEnvelope;
            unique_ptr<LFO>      freqLFO;

            //****************************
            // AMPLITUDE GLOBAL PARAMETERS
            //****************************
            float volume;   // [ 0 .. 1 ]
            float randpanL; // [ 0 .. 1 ]
            float randpanR;
            float fadeinAdjustment;

            unique_ptr<Envelope> ampEnvelope;
            unique_ptr<LFO>      ampLFO;

            struct Punch {
                bool  enabled;
                float initialvalue;
                float dt;
                float t;
            } punch;

            //*************************
            // FILTER GLOBAL PARAMETERS
            //*************************
            unique_ptr<Filter> filterL;
            unique_ptr<Filter> filterR;

            unique_ptr<Envelope> filterEnvelope;
            unique_ptr<LFO>      filterLFO;

            ADnoteGlobal();
            ADnoteGlobal(ADnoteGlobal const&);
        };
        ADnoteGlobal noteGlobal;

        // Voice parameters
        struct ADnoteVoice {
            bool enabled;
            int voice;              // the voice used as source.
            int noiseType;          // (sound/noise)
            int filterBypass;
            int delayTicks;
            SampleHolder oscilSmp;  // Waveform of the Voice. Shared with sub voices.
            int phaseOffset;        // PWM emulation

            // Frequency parameters
            int fixedFreq;          // if the frequency is fixed to 440 Hz
            int fixedFreqET;        // if the "fixed" frequency varies according to the note (ET)

            float detune;           // cents = basefreq * VoiceDetune
            float fineDetune;
            float bendAdjust;
            float offsetHz;

            unique_ptr<Envelope> freqEnvelope;
            unique_ptr<LFO>      freqLFO;

            // Amplitude parameters
            float volume;  // -1.0 .. 1.0
            float panning; // 0.0 = left, 0.5 = center, 1.0 = right
            float randpanL;
            float randpanR;

            unique_ptr<Envelope> ampEnvelope;
            unique_ptr<LFO>      ampLFO;

            struct Punch {
                int   enabled;
                float initialvalue, dt, t;
            } punch;

            // Filter parameters
            unique_ptr<Filter> voiceFilterL;
            unique_ptr<Filter> voiceFilterR;

            unique_ptr<Envelope> filterEnvelope;
            unique_ptr<LFO>      filterLFO;

            // Modulator parameters
            FMTYPE fmEnabled;
            bool fmRingToSide;
            unsigned char fmFreqFixed;
            int    fmVoice;
            Samples voiceOut;          // Voice Output used by other voices if use this as modulator
            SampleHolder fmSmp;        // Wave of the Voice. Shared by sub voices.
            int    fmPhaseOffset;
            float  fmVolume;
            bool fmDetuneFromBaseOsc;  // Whether we inherit the base oscillator's detuning
            float  fmDetune; // in cents
            unique_ptr<Envelope> fmFreqEnvelope;
            unique_ptr<Envelope> fmAmpEnvelope;
        };
        ADnoteVoice NoteVoicePar[NUM_VOICES];

        // Internal values of the note and of the voices
        int tSpot; // spot noise noise interrupt time

        RandomGen paramRNG; // A preseeded random number generator, reseeded
                            // with a known seed every time parameters are
                            // updated. This allows parameters to be changed
                            // smoothly. New notes will get a new seed.
        uint32_t paramSeed; // The seed for paramRNG.

        //pinking filter (Paul Kellet)
        float pinking[NUM_VOICES][14];

        size_t unison_size[NUM_VOICES]; // the size of unison for a single voice

        float unison_stereo_spread[NUM_VOICES]; // stereo spread of unison subvoices (0.0=mono,1.0=max)


        // Array-of dynamically allocated value-Arrays [voice][unison]
        template<typename T>
        using VoiceUnisonArray = std::array<unique_ptr<T[]>, NUM_VOICES>;

        // Wavetable reading position
        // *hi = skip/slot in the base wavetable
        // *lo = fractional part / interpolation
        VoiceUnisonArray<int>   oscposhi;
        VoiceUnisonArray<float> oscposlo;

        // Frequency / Wavetable increment
        VoiceUnisonArray<int>   oscfreqhi;  // integer part (skip)
        VoiceUnisonArray<float> oscfreqlo;  // fractional part (skip)

        // Modulator calculation pos and skip (frequency)
        VoiceUnisonArray<int>   oscposhiFM;
        VoiceUnisonArray<float> oscposloFM;

        VoiceUnisonArray<int>   oscfreqhiFM;
        VoiceUnisonArray<float> oscfreqloFM;

        VoiceUnisonArray<float> unison_base_freq_rap;// the unison base_value
        VoiceUnisonArray<float> unison_freq_rap;     // how the unison subvoice's frequency is changed (1.0 for no change)
        VoiceUnisonArray<bool>  unison_invert_phase; // which unison subvoice has phase inverted

        // These are set by parent voices.
        float detuneFromParent;             // How much the voice should be detuned.
        float unisonDetuneFactorFromParent; // How much the voice should be detuned from unison.

        struct UnisonVibrato {
            float  amplitude; // amplitude which be added to unison_freq_rap
            unique_ptr<float[]> step;      // value which increments the position
            unique_ptr<float[]> position;  // between -1.0 and 1.0
        };
        UnisonVibrato unison_vibrato[NUM_VOICES];

        float oldAmplitude[NUM_VOICES];  // used to compute and interpolate the
        float newAmplitude[NUM_VOICES];  // amplitudes of voices and modulators
        float fm_oldAmplitude[NUM_VOICES];
        float fm_newAmplitude[NUM_VOICES];

        VoiceUnisonArray<float> fm_oldSmp; // used by Frequency Modulation (for integration)

        VoiceUnisonArray<float> fmfm_oldPhase; // use when rendering FM modulator with parent FM
        VoiceUnisonArray<float> fmfm_oldPMod;
        VoiceUnisonArray<float> fmfm_oldInterpPhase;

        VoiceUnisonArray<float> fm_oldOscPhase; // rendering Oscil with parent FM that will be used for FM
        VoiceUnisonArray<float> fm_oldOscPMod;
        VoiceUnisonArray<float> fm_oldOscInterpPhase;

        bool forFM; // Whether this voice will be used for FM modulation.

        unique_ptr<Samples[]> tmpwave_unison;
        size_t max_unison;

        unique_ptr<Samples[]> tmpmod_unison;
        bool freqbasedmod[NUM_VOICES];

        float globaloldamplitude; // interpolate the amplitudes
        float globalnewamplitude;

        char firsttick[NUM_VOICES]; // 1 - if it is the first tick.
                                    // used to fade in the sound

        bool portamento;            // note performs portamento starting from previous note frequency

        float bandwidthDetuneMultiplier; // factor to increase or reduce the fine detuning

        // Legato vars
        float legatoFade;
        float legatoFadeStep;

        float pangainL;
        float pangainR;

        VoiceUnisonArray<unique_ptr<ADnote>> subVoice;
        VoiceUnisonArray<unique_ptr<ADnote>> subFMVoice;

        // Proxy-sub-Voice marker: -1 for ordinary (top-level) notes;
        // otherwise the Voice within the top-level note to attach to.
        // Note: in a (proxy)-sub-Voice, only the voice corresponding to the subVoiceNr is enabled,
        //       and its oscilSmp is aliased to use the wavetable from the corresponding voice in the master
        int subVoiceNr;
        // For sub voices: The controlling top-level note that attached this sub-voice.
        ADnote *topVoice;
        // For sub voices: Pointer to the closest parent that has phase/frequency modulation.
        float *parentFMmod;
};
#endif /*ADnote.h*/

