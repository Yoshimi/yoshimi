/*
    Microtonal.h - Tuning settings and microtonal capabilities

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified January 2011
*/

#ifndef MICROTONAL_H
#define MICROTONAL_H

#include <string>

using namespace std;

#include "Misc/MiscFuncs.h"

class XMLwrapper;

#define MAX_OCTAVE_SIZE 128

class Microtonal : private MiscFuncs
{
    public:
        Microtonal() { defaults(); }
        ~Microtonal() { }
        void defaults(void);
        float getNoteFreq(int note);
        float getNoteFreq(int note, int keyshift);

        // Parameters
        unsigned char Pinvertupdown;
        int           Pinvertupdowncenter;
        unsigned char Penabled;
        int           PAnote;
        int           Pscaleshift;
        float         PAfreq;

        // first and last key (to retune)
        int Pfirstkey;
        int Plastkey;

        // The middle note where scale degree 0 is mapped to
        int Pmiddlenote;

        // Map size
        int Pmapsize;

        unsigned char Pmappingenabled; // Mapping ON/OFF
        int Pmapping[128];             // Mapping (keys)

        float Pglobalfinedetune;

        int getoctavesize(void);
        void tuningtoline(int n, char *line, int maxn);
        int loadscl(string filename); // load the tunnings from a .scl file
        int loadkbm(string filename); // load the mapping from .kbm file
        int texttotunings(const char *text);
        void texttomapping(const char *text);

        string Pname;
        string Pcomment;

        void add2XML(XMLwrapper *xml);
        void getfromXML(XMLwrapper *xml);
        bool saveXML(string filename);
        bool loadXML(string filename);

    private:
        int linetotunings(unsigned int nline, const char *line);
        int loadline(FILE *file, char *line); // loads a line from the text file,
                                              // ignoring the lines beggining with "!"
        int octavesize;

        struct {
            unsigned char type; // 1 for cents or 2 for division
            float tuning;       // the real tuning (eg. +1.05946 for one halftone)
                                // or 2.0 for one octave
            unsigned int x1; // the real tunning is x1 / x2
            unsigned int x2;
        } octave[MAX_OCTAVE_SIZE],
          tmpoctave[MAX_OCTAVE_SIZE];

        static float note_12et[128];
};

inline int Microtonal::getoctavesize(void)
{
    return ((Penabled != 0) ? octavesize : 12);
}

inline float Microtonal::getNoteFreq(int note)
{
    return powf(2.0f, (float)(note - PAnote) / 12.0f) * PAfreq;
}

#endif
