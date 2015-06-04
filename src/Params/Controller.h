/*
    Controller.h - (Midi) Controllers implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "globals.h"
#include "Misc/XMLwrapper.h"
#include "Effects/Fader.h"

class Controller
{
    public:
        Controller();
        ~Controller() { };
        void resetall();

        void add2XML(XMLwrapper *xml);
        void defaults();
        void getfromXML(XMLwrapper *xml);

        // Controllers functions
        void setpitchwheel(int value);
        void setpitchwheelbendrange(unsigned short int value);
        void setexpression(int value);
        void setpanning(int value);
        void setfiltercutoff(int value);
        void setfilterq(int value);
        void setbandwidth(int value);
        void setmodwheel(int value);
        void setfmamp(int value);
        void setvolume(int value);
        void setsustain(int value);
        void setportamento(int value);
        void setresonancecenter(int value);
        void setresonancebw(int value);


        void setparameternumber(unsigned int type, int value); // used for RPN and NRPN's
        int getnrpn(int *parhi, int *parlo, int *valhi, int *vallo);

        int initportamento(float oldfreq, float newfreq, bool legatoflag);
        // returns 1 if the portamento's conditions are true, else return 0

        void updateportamento(); //update portamento values

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

