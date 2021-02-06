/*
    SUBnote.h - The subtractive synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010 Alan Calvert
    Copyright 2014-2017 Will Godfrey & others
    Copyright 2020 Kristian Amlie & others

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

#ifndef SUB_NOTE_H
#define SUB_NOTE_H

class SUBnoteParameters;
class Controller;
class Envelope;
class Filter;

class SynthEngine;

class SUBnote
{
    public:
        SUBnote(SUBnoteParameters *parameters, Controller *ctl_,
                float freq_, float velocity_, int portamento_,
                int midinote_, SynthEngine *_synth);
        SUBnote(const SUBnote &rhs);
        ~SUBnote();

        void legatoFadeIn(float basefreq_, float velocity_, int portamento_, int midinote_);
        void legatoFadeOut(const SUBnote &syncwith);

        int noteout(float *outl,float *outr); // note output, return 0 if the
                                              // note is finished
        void releasekey(void);
        bool finished() const
        {
            return NoteStatus == NOTE_DISABLED ||
                (NoteStatus != NOTE_KEEPALIVE && legatoFade == 0.0f);
        }

        // Whether the note has samples to output.
        // Currently only used for dormant legato notes.
        bool ready() { return legatoFade != 0.0f || legatoFadeStep != 0.0f; };

    private:
        void computecurrentparameters(void);
        void initparameters(float freq);
        void KillNote(void);
        void updatefilterbank(void);

        SUBnoteParameters *pars;

        bool stereo;
        int pos[MAX_SUB_HARMONICS]; // chart of non-zero harmonic locations
        int numstages; // number of stages of filters
        int numharmonics; // number of harmonics (after the too higher hamonics are removed)
        int start; // how the harmonics start
        float basefreq;
        float notefreq;
        float velocity;
        int portamento;
        int midinote;
        float BendAdjust;
        float OffsetHz;
        float randpanL;
        float randpanR;

        Envelope *AmpEnvelope;
        Envelope *FreqEnvelope;
        Envelope *BandWidthEnvelope;

        Filter *GlobalFilterL,*GlobalFilterR;

        Envelope *GlobalFilterEnvelope;

        // internal values
        enum {
            NOTE_DISABLED,
            NOTE_ENABLED,
            NOTE_KEEPALIVE
        } NoteStatus;
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
        void computeNoteFreq();
        void filter(bpfilter &filter, float *smps);
        void filterVarRun(bpfilter &filter, float *smps);
        float getHgain(int harmonic);

        bpfilter *lfilter;
        bpfilter *rfilter;

        float overtone_rolloff[MAX_SUB_HARMONICS];
        float overtone_freq[MAX_SUB_HARMONICS];

        float *tmpsmp;
        float *tmprnd; // this is filled with random numbers

        Controller *ctl;
        int oldpitchwheel;
        int oldbandwidth;

        // Legato vars
        float legatoFade;
        float legatoFadeStep;

        Presets::PresetsUpdate subNoteChange;

        SynthEngine *synth;
        int filterStep;
};

#endif
