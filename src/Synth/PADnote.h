/*
    PADnote.h - The "pad" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010 Alan Calvert
    Copyright 2017 Will Godfrey & others
    Copyright 2020 Kristian Amlie

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

    This file is a derivative of the ZynAddSubFX original

*/

#ifndef PAD_NOTE_H
#define PAD_NOTE_H

#include <memory>

using std::unique_ptr;

class PADnoteParameters;
class WaveInterpolator;
class Controller;
class Envelope;
class LFO;
class Filter;
class Controller;

class SynthEngine;

class PADnote
{
    public:
        PADnote(PADnoteParameters& parameters, Controller& ctl_, Note, bool portamento_);
        PADnote(const PADnote &orig);
       ~PADnote();

        // shall not be moved or assigned
        PADnote(PADnote&&)                 = delete;
        PADnote& operator=(PADnote&&)      = delete;
        PADnote& operator=(PADnote const&) = delete;

        void performPortamento(Note);
        void legatoFadeIn(Note);
        void legatoFadeOut();

        void noteout(float *outl,float *outr);
        bool finished() const { return noteStatus == NOTE_DISABLED; }
        void releasekey(void);

    private:
        void fadein(float *smps);
        bool isWavetableChanged(size_t tableNr);
        WaveInterpolator* buildInterpolator(size_t tableNr);
        WaveInterpolator* setupCrossFade(WaveInterpolator*);
        void computeNoteParameters();
        void computecurrentparameters();
        void setupBaseFreq();
        bool isLegatoFading() const { return legatoFadeStep != 0.0f; };


        SynthEngine& synth;
        PADnoteParameters& pars;
        ParamBase::ParamsUpdate padSynthUpdate;
        Controller& ctl;

        enum NoteStatus {
            NOTE_DISABLED,
            NOTE_ENABLED,
            NOTE_LEGATOFADEOUT
        } noteStatus;

        unique_ptr<WaveInterpolator> waveInterpolator;

        Note note;
        float realfreq;
        float BendAdjust;
        float OffsetHz;
        bool firsttime;
        bool released;

        bool portamento;

        int Compute_Linear(float *outl, float *outr, int freqhi,
                           float freqlo);
        int Compute_Cubic(float *outl, float *outr, int freqhi,
                          float freqlo);


        struct PADnoteGlobal {
            //****************************
            // FREQUENCY GLOBAL PARAMETERS
            //****************************
            float detune; // in cents

            unique_ptr<Envelope> freqEnvelope;
            unique_ptr<LFO>      freqLFO;

            //****************************
            // AMPLITUDE GLOBAL PARAMETERS
            //****************************
            float volume;  // [ 0 .. 1 ]
            float panning; // [ 0 .. 1 ]
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
        };

        PADnoteGlobal noteGlobal;

        float globaloldamplitude;
        float globalnewamplitude;
        float randpanL;
        float randpanR;

        // Legato vars
        float legatoFade;
        float legatoFadeStep;
};
#endif /*PADnote.h*/

