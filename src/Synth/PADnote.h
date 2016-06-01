/*
    PADnote.h - The "pad" synthesizer

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010 Alan Calvert

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

    This file is a derivative of the ZynAddSubFX original, modified October 2010
*/

#ifndef PAD_NOTE_H
#define PAD_NOTE_H

#include "Synth/Carcass.h"
#include "Misc/SynthHelper.h"
#include "Synth/LegatoTypes.h"

class PADnoteParameters;
class Controller;
class Envelope;
class LFO;
class Filter;
class Controller;

class SynthEngine;

class PADnote : public Carcass, private SynthHelper
{
    public:
        PADnote(PADnoteParameters *parameters, Controller *ctl_, float freq,
                float velocity, int portamento_, int midinote, bool besilent, SynthEngine *_synth);
        ~PADnote();

        void PADlegatonote(float freq, float velocity,
                           int portamento_, int midinote, bool externcall);

        int noteout(float *outl,float *outr);
        bool finished(void) { return finished_; };
        void relasekey(void);

        bool ready;

    private:
        void fadein(float *smps);
        void computecurrentparameters();
        bool finished_;
        PADnoteParameters *pars;

        int poshi_l;
        int poshi_r;
        float poslo;

        float basefreq;
        float BendAdjust;
        float OffsetHz;
        bool firsttime;
        bool released;

        int nsample, portamento;

        int Compute_Linear(float *outl, float *outr, int freqhi,
                           float freqlo);
        int Compute_Cubic(float *outl, float *outr, int freqhi,
                          float freqlo);


        struct {
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

            float Fadein_adjustment;
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

            float FilterCenterPitch;//octaves
            float FilterQ;
            float FilterFreqTracking;

            Envelope *FilterEnvelope;

            LFO *FilterLfo;
        } NoteGlobalPar;


        Controller *ctl;
        float globaloldamplitude;
        float globalnewamplitude;
        float velocity;
        float realfreq;
//        float *tmpwave;
        float randpanL;
        float randpanR;

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

        SynthEngine *synth;
};

#endif
