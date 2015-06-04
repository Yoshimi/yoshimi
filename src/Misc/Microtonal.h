/*
    Microtonal.h - Tuning settings and microtonal capabilities

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

#ifndef MICROTONAL_H
#define MICROTONAL_H

#include "globals.h"
#include "Misc/XMLwrapper.h"

#define MAX_OCTAVE_SIZE 128
#define MICROTONAL_MAX_NAME_LEN 120

class Microtonal
{
    public:
        Microtonal();
        ~Microtonal();
        void defaults();
        float getnotefreq(int note,int keyshift);

        // Parameters
        unsigned char Pinvertupdown;
        unsigned char Pinvertupdowncenter;
        unsigned char Penabled;
        unsigned char PAnote;
        unsigned char Pscaleshift;
        float PAfreq;

        // first and last key (to retune)
        unsigned char Pfirstkey;
        unsigned char Plastkey;

        // The middle note where scale degree 0 is mapped to
        unsigned char Pmiddlenote;

        // Map size
        unsigned char Pmapsize;

        // Mapping ON/OFF
        unsigned char Pmappingenabled;
        // Mapping (keys)
        short int Pmapping[128];

        unsigned char Pglobalfinedetune;

        // Functions
        unsigned char getoctavesize();
        void tuningtoline(int n, char *line, int maxn);
        int loadscl(const char *filename); // load the tunnings from a .scl file
        int loadkbm(const char *filename); // load the mapping from .kbm file
        int texttotunings(const char *text);
        void texttomapping(const char *text);
        unsigned char *Pname;
        unsigned char *Pcomment;

        void add2XML(XMLwrapper *xml);
        void getfromXML(XMLwrapper *xml);
        int saveXML(char *filename);
        int loadXML(char *filename);

    private:
        int linetotunings(unsigned int nline, const char *line);
        int loadline(FILE *file, char *line);
            // loads a line from the text file,
            // ignoring the lines beggining with "!"
        unsigned char octavesize;
        struct {
            unsigned char type; // 1 for cents or 2 for division
            // the real tuning (eg. +1.05946 for one halftone)
            // or 2.0 for one octave
            float tuning;
            unsigned int x1, x2; // the real tunning is x1/x2
        } octave[MAX_OCTAVE_SIZE],
          tmpoctave[MAX_OCTAVE_SIZE];

      unsigned int samplerate;
      int buffersize;
      int oscilsize;
};

#endif
