/*
    SUBnote.h - The subtractive synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010 Alan Calvert
    Copyright 2014-2017 Will Godfrey & others
    Copyright 2020 Kristian Amlie & others

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

#ifndef SUB_NOTE_H
#define SUB_NOTE_H

#include "globals.h"
#include "Misc/Alloc.h"
#include "Params/Presets.h"

#include <memory>

using std::unique_ptr;

class SUBnoteParameters;
class Controller;
class Envelope;
class Filter;

class SynthEngine;

class SUBnote
{
    public:
        SUBnote(SUBnoteParameters& parameters, Controller& ctl_, Note, bool portamento_);
        SUBnote(SUBnote const&);
       ~SUBnote();

        // shall not be moved or assigned
        SUBnote(SUBnote&&)                 = delete;
        SUBnote& operator=(SUBnote&&)      = delete;
        SUBnote& operator=(SUBnote const&) = delete;

        void performPortamento(Note);
        void legatoFadeIn(Note);
        void legatoFadeOut();

        void noteout(float *outl,float *outr);
        void releasekey(void);
        bool finished() const { return noteStatus == NOTE_DISABLED; }

    private:
        void computecurrentparameters(void);
        void initparameters(float freq);
        void killNote(void);
        void updatefilterbank(void);


        SynthEngine& synth;
        SUBnoteParameters& pars;
        Presets::PresetsUpdate subNoteChange;
        Controller& ctl;

        Note note;
        bool stereo;
        float realfreq;
        bool portamento;
        int numstages;              // number of stages of filters
        int numharmonics;           // number of harmonics (after the too higher harmonics are removed)
        int start;                  // how the harmonics start
        int pos[MAX_SUB_HARMONICS]; // chart of non-zero harmonic locations
        float bendAdjust;
        float offsetHz;
        float randpanL;
        float randpanR;

        unique_ptr<Envelope> ampEnvelope;
        unique_ptr<Envelope> freqEnvelope;
        unique_ptr<Envelope> bandWidthEnvelope;
        unique_ptr<Envelope> globalFilterEnvelope;

        unique_ptr<Filter> globalFilterL;
        unique_ptr<Filter> globalFilterR;


        // internal values
        enum NoteStatus {
            NOTE_DISABLED,
            NOTE_ENABLED,
            NOTE_LEGATOFADEOUT
        } noteStatus;

        int firsttick;
        float volume;
        float oldamplitude;
        float newamplitude;

        struct bpfilter {
            float freq;
            float bw;
            float amp;   // filter parameters
            float a1;
            float a2;
            float b0;
            float b2;    // filter coefs. b1=0
            float xn1;
            float xn2;
            float yn1;
            float yn2;   // filter internal values
        };

        // Returns the number of new filters created
        int createNewFilters();

        void initfilters(int startIndex);
        void initfilter(bpfilter &filter, float mag);
        float computerolloff(float freq);
        void computeallfiltercoefs();
        void computefiltercoefs(bpfilter &filter, float freq, float bw, float gain);
        void computeNoteParameters();
        float computeRealFreq();
        void filter(bpfilter &filter, float *smps);
        void filterVarRun(bpfilter &filter, float *smps);
        float getHgain(int harmonic);

        unique_ptr<bpfilter[]> lfilter;
        unique_ptr<bpfilter[]> rfilter;

        float overtone_rolloff[MAX_SUB_HARMONICS];
        float overtone_freq[MAX_SUB_HARMONICS];

        Samples& tmpsmp;
        Samples& tmprnd; // this is filled with random numbers

        int oldpitchwheel;
        int oldbandwidth;

        // Legato vars
        float legatoFade;
        float legatoFadeStep;

        int filterStep;
};
#endif /*SUB_NOTE_H*/

