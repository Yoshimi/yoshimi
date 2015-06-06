/*
    Controller.h - (Midi) Controllers implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/


#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "Misc/XMLwrapper.h"
#include "Effects/Fader.h"

class Controller
{
    public:
        Controller();
        ~Controller() { };
        void resetAll();

        void add2XML(XMLwrapper *xml);
        void setDefaults();
        void getfromXML(XMLwrapper *xml);

        // Controllers functions
        void setPitchwheel(int value);
        void setPitchwheelBendrange(unsigned short int value);
        void setExpression(int value);
        void setPanning(int value);
        void setFilterCutoff(int value);
        void setFilterQ(int value);
        void setBandwidth(int value);
        void setModwheel(int value);
        void setFmAmp(int value);
        void setVolume(int value);
        void setSustain(int value);
        void setPortamento(int value);
        void setResonanceCenter(int value);
        void setResonanceBw(int value);


        void setParameterNumber(unsigned int type, int value); // used for RPN and NRPN's
        int getNrpn(int *parhi, int *parlo, int *valhi, int *vallo);

        int initPortamento(float oldfreq, float newfreq, bool legatoflag);
        // returns 1 if the portamento's conditions are true, else return 0

        void updatePortamento(); //update portamento values

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
            float pan;
            unsigned char depth;
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
            Fader *volControl;
            unsigned char receive;
        } volume;

        struct { // Sustain
            int data,sustain;
            unsigned char receive;
        } sustain;

        struct {
            // parameters
            int data;
            unsigned char portamento;

            unsigned char receive, time;
            unsigned char pitchthresh;
            unsigned char pitchthreshtype;

            unsigned char updowntimestretch;

            float freqrap;
            int noteusing;
            int used;
            // internal data
            float x,dx; // x is from 0.0 (start portamento) to 1.0
                              // (finished portamento), dx is x increment
            float origfreqrap; // this is used for computing oldfreq value
                                     // from x
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

        struct { // nrpn
            int parhi,parlo;
            int valhi,vallo;
            unsigned char receive; // this is saved to disk by Master
        } NRPN;
};

#endif

