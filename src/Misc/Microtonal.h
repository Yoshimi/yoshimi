/*
    Microtonal.h - Tuning settings and microtonal capabilities

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2016-2019, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code.

    Modified February 2019
*/

#ifndef MICROTONAL_H
#define MICROTONAL_H

#include <cmath>
#include <string>
#include "globals.h"

class SynthEngine;
class XMLwrapper;

using std::string;

const size_t MAX_OCTAVE_SIZE = 128;


class Microtonal
{
    public:
        Microtonal(SynthEngine *_synth): synth(_synth) { defaults(); }
        ~Microtonal() { }
        void defaults(void);
        float getNoteFreq(int note, int keyshift);
        float getFixedNoteFreq(int note);
        float getLimits(CommandBlock *getData);

        // Parameters
        unsigned char Pinvertupdown;
        int           Pinvertupdowncenter;
        unsigned char Penabled;
        int           PrefNote;
        int           Pscaleshift;
        float         PrefFreq;

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
        void tuningtoline(unsigned int n, char *line, int maxn);
        string tuningtotext(void);
        string keymaptotext(void);
        int loadscl(const string& filename); // load the tunings from a .scl file
        int loadkbm(const string& filename); // load the mapping from .kbm file
        int texttotunings(const char *text);
        int texttomapping(const char *text);

        string Pname;
        string Pcomment;

        void add2XML(XMLwrapper *xml);
        void getfromXML(XMLwrapper *xml);
        bool saveXML(const string& filename);
        bool loadXML(const string& filename);

    private:
        string reformatline(string text);
        bool validline(const char *line);
        int linetotunings(unsigned int nline, const char *line);
        int loadLine(const string& text, size_t &point, char *line, size_t maxlen);
        // loads a line from the text file,
        // ignoring the lines beginning with "!"
        size_t octavesize;

        struct Octave {
            unsigned char type; // 1 for cents or 2 for division
            double tuning;       // the real tuning (eg. +1.05946 for one halftone)
                                // or 2.0 for one octave
            unsigned int x1; // the real tuning is x1 / x2
            unsigned int x2;
            string text;
        };
        Octave octave[MAX_OCTAVE_SIZE];
        Octave tmpoctave[MAX_OCTAVE_SIZE];

        SynthEngine *synth;
};

inline int Microtonal::getoctavesize(void)
{
    return ((Penabled != 0) ? octavesize : 12);
}

inline float Microtonal::getFixedNoteFreq(int note)
{
    return powf(2.0f, (float)(note - PrefNote) / 12.0f) * PrefFreq;
}


#endif
