/*
    Part.h - Part implementation

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

#ifndef PART_H
#define PART_H

#include <list> // For the monomemnotes list.

using namespace std;

#include "globals.h"
#include "Params/ADnoteParameters.h"
#include "Params/SUBnoteParameters.h"
#include "Params/PADnoteParameters.h"
#include "Synth/ADnote.h"
#include "Synth/SUBnote.h"
#include "Synth/PADnote.h"
#include "Params/Controller.h"
#include "Misc/Microtonal.h"
#include "DSP/FFTwrapper.h"
#include "Effects/EffectMgr.h"
#include "Misc/XMLwrapper.h"
#include "Effects/Fader.h"

#define MAX_INFO_TEXT_SIZE 1000

class Part
{
    public:
        Part(Microtonal *microtonal_, FFTwrapper *fft_);
        ~Part();

        // Midi commands implemented
        void NoteOn(unsigned char note, unsigned char velocity,
                        int masterkeyshift);
        void NoteOff(unsigned char note);
        void AllNotesOff() { killallnotes = true; }; // panic, prepare all notes
                                                     // to be turned off
        void SetController(unsigned int type, int par);
        void RelaseSustainedKeys(); // this is called when the sustain pedal is relased
        void RelaseAllKeys();       // this is called on AllNotesOff controller

        /* The synthesizer part output */
        void ComputePartSmps(); // Part output

        // instrumentonly: 0 - save all, 1 - save only instrumnet,
        //                 2 - save only instrument without the name(used in bank)

        //saves the instrument settings to a XML file
        //returns 0 for ok or <0 if there is an error
        int saveXML(char *filename);
        int loadXMLinstrument(const char *filename);

        void add2XML(XMLwrapper *xml);
        void add2XMLinstrument(XMLwrapper *xml);

        void defaults();
        void defaultsinstrument();

        void applyparameters();

        void getfromXML(XMLwrapper *xml);
        void getfromXMLinstrument(XMLwrapper *xml);

        void cleanup();

    //      ADnoteParameters *ADPartParameters;
    //      SUBnoteParameters *SUBPartParameters;

        // the part's kit
        struct {
            unsigned char Penabled, Pmuted, Pminkey, Pmaxkey;
            string        Pname;
            unsigned char Padenabled, Psubenabled, Ppadenabled;
            unsigned char Psendtoparteffect;
            ADnoteParameters *adpars;
            SUBnoteParameters *subpars;
            PADnoteParameters *padpars;
        } kit[NUM_KIT_ITEMS];

        // Part parameters
        void setkeylimit(unsigned char Pkeylimit);
        void setkititemstatus(int kititem,int Penabled_);

        unsigned char Penabled;
        unsigned char Pvolume;
        unsigned char Pminkey;
        unsigned char Pmaxkey;     // the maximum key that the part receives noteon messages
        void setPvolume(char Pvolume);
        unsigned char Pkeyshift;   // Part keyshift
        unsigned char Prcvchn;     // from what midi channel it receive commnads
        unsigned char Ppanning;    // part panning
        void setPpanning(char Ppanning);
        unsigned char Pvelsns;     // velocity sensing (amplitude velocity scale)
        unsigned char Pveloffs;    // velocity offset
        unsigned char Pnoteon;     // if the part receives NoteOn messages
        unsigned char Pkitmode;    // if the kitmode is enabled
        unsigned char Pdrummode;   // if all keys are mapped and the system is 12tET (used for drums)

        unsigned char Ppolymode;   // Part mode - 0=monophonic , 1=polyphonic
        unsigned char Plegatomode; // 0=normal, 1=legato
        unsigned char Pkeylimit;   // how many keys are alowed to be played same
                                   // time (0=off), the older will be relased

        string        Pname;       // name of the instrument
        struct {                   // instrument additional information
            unsigned char Ptype;
            string        Pauthor;
            string        Pcomments;
        } info;

        float *partoutl; // Left channel output of the part
        float *partoutr; // Right channel output of the part

        // Left and right signal that pass thru part effects
        // [NUM_PART_EFX] is for "no effect" buffer
        float *partfxinputl[NUM_PART_EFX + 1];
        float *partfxinputr[NUM_PART_EFX + 1];

        enum NoteStatus { KEY_OFF, KEY_PLAYING, KEY_RELASED_AND_SUSTAINED, KEY_RELASED };

        float volume;      // applied by Master,
        float oldvolumel;  //
        float oldvolumer;  //
        float panning;     //

        Controller ctl; // Part controllers

        // insertion part effects - part of the instrument
        EffectMgr *partefx[NUM_PART_EFX];
        // how the effect's output is routed (to next effect/to out)
        unsigned char Pefxroute[NUM_PART_EFX];
        // if the effects are bypassed, [NUM_PART_EFX] is for "no effect" buffer
        bool Pefxbypass[NUM_PART_EFX + 1];

        int lastnote;

    private:
        void KillNotePos(int pos);
        void RelaseNotePos(int pos);
        void MonoMemRenote(); // MonoMem stuff.

        bool killallnotes; // true if I want to kill all notes (ie, panic)

        struct PartNotes {
            NoteStatus status;
            int note; // if there is no note playing, the "note"=-1
            int itemsplaying;
            struct {
                ADnote *adnote;
                SUBnote *subnote;
                PADnote *padnote;
                int sendtoparteffect;
            } kititem[NUM_KIT_ITEMS];
            int time;
        };

        int lastpos, lastposb;    // To keep track of previously used pos and posb.
        bool lastlegatomodevalid; // To keep track of previous legatomodevalid.

        // MonoMem stuff
        list<unsigned char> monomemnotes; // A list to remember held notes.
        struct {
            unsigned char velocity;
            int mkeyshift; // I'm not sure masterkeyshift should be remembered.
        } monomem[256];
        /* 256 is to cover all possible note values. monomem[] is used in
           conjunction with the list to store the velocity and masterkeyshift
           values of a given note (the list only store note values).
           For example 'monomem[note].velocity' would be the velocity value of
           the note 'note'.
        */

        PartNotes partnote[POLIPHONY];

        float *tmpoutl; // used to get the note
        float *tmpoutr;

        float oldfreq; // this is used for portamento
        Microtonal *microtonal;
        FFTwrapper *fft;
        int buffersize;
        Fader *volControl;
};

#endif
