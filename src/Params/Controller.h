/*
    Controller.h - (Midi) Controllers implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2017-2018, Will Godfrey

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

    This file is a derivative of a ZynAddSubFX original.

    Modified February 2018
*/


#ifndef CONTROLLER_H
#define CONTROLLER_H

class XMLwrapper;

class SynthEngine;

class Controller
{
    public:
        Controller(SynthEngine *_synth);
        ~Controller() { };
        void resetall(void);

        void add2XML(XMLwrapper *xml);
        void defaults(void);
        void getfromXML(XMLwrapper *xml);

        // Controllers functions
        void setpitchwheel(int value);
        void setpitchwheelbendrange(unsigned short int value);
        void setexpression(int value);
        void setfiltercutoff(int value);
        void setfilterq(int value);
        void setbandwidth(int value);
        void setmodwheel(int value);
        void setfmamp(int value);
        void setvolume(int value);
        void setsustain(int value);
        void setportamento(int value);
        void portamentosetup();
        void setresonancecenter(int value);
        void setresonancebw(int value);
        void setPanDepth(char par) { panning.depth = par;}
        int initportamento(float oldfreq, float newfreq, bool in_progress);
        // returns 1 if the portamento's conditions are true, else return 0
        void updateportamento(void); // update portamento values
        float getLimits(CommandBlock *getData);

        // Controllers values
        struct { // Pitch Wheel
            int data;
            short int bendrange; // bendrange is in cents
            float relfreq; // the relative frequency (default is 1.0)
        } pitchwheel;

        struct { // Expression
            int data;
            float relvolume;
            unsigned char receive;
        } expression;

        struct { // Panning
            int data;
            char depth;
        } panning;

        struct { // Filter cutoff
            int data;
            float relfreq;
            unsigned char depth;
        } filtercutoff;

        struct { // Filter Q
            int data;
            float relq;
            unsigned char depth;
        } filterq;

        struct { // Bandwidth
            int data;
            float relbw;
            unsigned char depth;
            unsigned char exponential;
        } bandwidth;

        struct { // Modulation Wheel
            int data;
            float relmod;
            unsigned char depth;
            unsigned char exponential;
        } modwheel;

        struct { // FM amplitude
            int data;
            float relamp;
            unsigned char receive;
        } fmamp;

        struct { // Volume
            int data;
            float volume;
            unsigned char receive;
        } volume;

        struct { // Sustain
            int data,sustain;
            unsigned char receive;
        } sustain;

        struct { // Portamento
            // parameters
            int data;
            unsigned char portamento;

            unsigned char receive;
            unsigned char time;
            unsigned char proportional;
            unsigned char propRate;
            unsigned char propDepth;
            unsigned char pitchthresh;
            unsigned char pitchthreshtype;

            unsigned char updowntimestretch;

            float freqrap;
            int noteusing;
            int used;
            // internal data
            float x;  // x is from 0.0 (start portamento) to 1.0 (finished portamento),
            float dx; // dx is x increment
            float origfreqrap; // this is used for computing oldfreq value from x
        } portamento;

        struct { // Resonance Center Frequency
            int data;
            float relcenter;
            unsigned char depth;
        } resonancecenter;

        struct { // Resonance Bandwidth
            int data;
            float relbw;
            unsigned char depth;
        } resonancebandwidth;
private:
        SynthEngine *synth;
};

#endif

