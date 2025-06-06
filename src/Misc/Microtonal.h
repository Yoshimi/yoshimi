/*
    Microtonal.h - Tuning settings and microtonal capabilities

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2016-2023, Will Godfrey and others

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

    This file is derivative of ZynAddSubFX original code.
*/

#ifndef MICROTONAL_H
#define MICROTONAL_H

#include <cmath>
#include <string>
#include "globals.h"
#include "Misc/NumericFuncs.h"

class SynthEngine;
class XMLtree;

using std::string;
using func::power;

class Microtonal
{
    public:
       ~Microtonal() = default;
        Microtonal(SynthEngine *_synth): synth(_synth) { defaults(); }
        void  defaults(int type = 0);
        float getNoteFreq(int note, int keyshift);
        float getFixedNoteFreq(int note);
        float getLimits(CommandBlock *getData);

        // Parameters
        uchar Pinvertupdown;
        int   Pinvertupdowncenter;
        uchar Penabled;
        int   PrefNote;
        int   Pscaleshift;
        float PrefFreq;

        // first and last key (to retune)
        int Pfirstkey;
        int Plastkey;

        // The middle note where scale degree 0 is mapped to
        int Pmiddlenote;

        // Map size
        int Pmapsize;

        int PformalOctaveSize;         //////////////TODO seems not to be actually used as of 9/2024

        uchar Pmappingenabled; // Mapping ON/OFF
        int Pmapping[MAX_OCTAVE_SIZE];             // Mapping (keys)
        string PmapComment[MAX_OCTAVE_SIZE];       // comments for mapping (if they exist)

        float Pglobalfinedetune;
        float globalfinedetunerap;
        void setglobalfinedetune(float control);

        int getoctavesize();
        void tuningtoline(uint n, string& line);
        string tuningtotext();
        string keymaptotext();
        int loadscl(string const& filename); // load the tunings from a .scl file
        int loadkbm(string const& filename); // load the mapping from .kbm file
        int texttotunings(string page);
        int texttomapping(string page);

        string scale2scl();
        string map2kbm();

        string Pname;
        string Pcomment;

        void add2XML(XMLtree&);
        int  getfromXML(XMLtree&);
        bool saveXML(string const& filename);
        int  loadXML(string const& filename);

    private:
        int getLineFromText(string& page, string& line);
        string reformatline(string text);
        int linetotunings(uint nline, string text);
        // extracts a line from a text file, ignoring the lines beginning with "!"

    public:
        // TODO made these public until we have better ways to transfer data to/from GUI
        size_t octavesize;

        struct Octave {
            uchar type;    // 1 for cents or 2 for division
            double tuning; // the real tuning (eg. +1.05946 for one halftone) or 2.0 for one octave
            uint x1;       // the real tuning is x1 / x2
            uint x2;
            string text;
            string comment;
        };
        Octave octave[MAX_OCTAVE_SIZE];

        SynthEngine *synth;
};

inline int Microtonal::getoctavesize()
{
    return ((Penabled != 0) ? octavesize : 12);
}

inline float Microtonal::getFixedNoteFreq(int note)
{
    return power<2>(float(note - PrefNote) / 12.0f) * PrefFreq;
}


#endif
