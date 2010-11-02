/*
    Part.h - Part implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010 Alan Calvert

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

    This file is a derivative of a ZynAddSubFX original, modified November 2010
*/

#ifndef PART_H
#define PART_H

#include <list>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/SynthHelper.h"

class ADnoteParameters;
class SUBnoteParameters;
class PADnoteParameters;
class ADnote;
class SUBnote;
class PADnote;
class Controller;
class XMLwrapper;
class Microtonal;
class EffectMgr;
class FFTwrapper;

class Part : private MiscFuncs, SynthHelper
{
    public:
        Part(Microtonal *microtonal_, FFTwrapper *fft_);
        ~Part();

        // Midi commands implemented
        void NoteOn(unsigned char note, unsigned char velocity, int masterkeyshift);
        void NoteOff(unsigned char note);
        void AllNotesOff(void) { killallnotes = true; }; // panic, prepare all notes to be turned off
        void SetController(unsigned int type, int par);
        void RelaseSustainedKeys(void);
        void RelaseAllKeys(void);

        void ComputePartSmps(void);

        // instrumentonly: 0 - save all, 1 - save only instrumnet,
        //                 2 - save only instrument without the name(used in bank)
        bool saveXML(string filename); // true for load ok, otherwise false
        bool loadXMLinstrument(string filename);

        void add2XML(XMLwrapper *xml);
        void add2XMLinstrument(XMLwrapper *xml);

        void defaults(void);
        void defaultsinstrument(void);
        void applyparameters(void);
        void getfromXML(XMLwrapper *xml);
        void getfromXMLinstrument(XMLwrapper *xml);

        void cleanup(void);

        // the part's kit
        struct {
            string        Pname;
            unsigned char Penabled;
            unsigned char Pmuted;
            unsigned char Pminkey;
            unsigned char Pmaxkey;
            unsigned char Padenabled;
            unsigned char Psubenabled;
            unsigned char Ppadenabled;
            unsigned char Psendtoparteffect;
            ADnoteParameters  *adpars;
            SUBnoteParameters *subpars;
            PADnoteParameters *padpars;
        } kit[NUM_KIT_ITEMS];

        // Part parameters
        void setkeylimit(unsigned char Pkeylimit);
        void setkititemstatus(int kititem, int Penabled_);
        void setPvolume(char Pvolume);
        void setPpanning(char Ppanning);

        unsigned char Penabled;
        unsigned char Pvolume;
        unsigned char Pminkey;
        unsigned char Pmaxkey;
        unsigned char Pkeyshift;
        unsigned char Prcvchn;
        unsigned char Ppanning;
        unsigned char Pvelsns;     // velocity sensing (amplitude velocity scale)
        unsigned char Pveloffs;    // velocity offset
        unsigned char Pnoteon;     // if the part receives NoteOn messages
        unsigned char Pkitmode;    // if the kitmode is enabled
        unsigned char Pdrummode;   // if all keys are mapped and the system is 12tET (used for drums)

        unsigned char Ppolymode;   // Part mode - 0 = monophonic , 1 = polyphonic
        unsigned char Plegatomode; // 0 = normal, 1 = legato
        unsigned char Pkeylimit;   // how many keys can play simultaneously,
                                   // time 0 = off, the older will be released
        string        Pname;
        struct {
            unsigned char Ptype;
            string        Pauthor;
            string        Pcomments;
        } info;

        float *partoutl;
        float *partoutr;

        float *partfxinputl[NUM_PART_EFX + 1]; // Left and right signal that pass thru part effects
        float *partfxinputr[NUM_PART_EFX + 1]; // [NUM_PART_EFX] is for "no effect" buffer

        enum NoteStatus { KEY_OFF, KEY_PLAYING, KEY_RELASED_AND_SUSTAINED, KEY_RELASED };

        float volume;      // applied by Master,
        float oldvolumel;  //
        float oldvolumer;  //
        float panning;     //

        Controller *ctl; // Part controllers

        EffectMgr *partefx[NUM_PART_EFX];      // insertion part effects - part of the instrument
        unsigned char Pefxroute[NUM_PART_EFX]; // how the effect's output is
                                               // routed (to next effect/to out)
        bool Pefxbypass[NUM_PART_EFX + 1];     // if the effects are bypassed,
                                               // [NUM_PART_EFX] is for "no effect" buffer

        int lastnote;

    private:
        void KillNotePos(int pos);
        void RelaseNotePos(int pos);
        void MonoMemRenote(void); // MonoMem stuff.

        bool killallnotes;

        struct PartNotes {
            NoteStatus status;
            int note;          // if there is no note playing, "note" = -1
            int itemsplaying;
            struct {
                ADnote *adnote;
                SUBnote *subnote;
                PADnote *padnote;
                int sendtoparteffect;
            } kititem[NUM_KIT_ITEMS];
            int time;
        };

        int lastpos, lastposb;    // to keep track of previously used pos and posb.
        bool lastlegatomodevalid; // to keep track of previous legatomodevalid.

        // MonoMem stuff
        list<unsigned char> monomemnotes; // a list to remember held notes.
        struct {
            unsigned char velocity;
            int mkeyshift; // Not sure if masterkeyshift should be remembered.
        } monomem[256];    // 256 is to cover all possible note values. monomem[]
                           // is used in conjunction with the list to store the
                           // velocity and masterkeyshift values of a given note
                           // (the list only store note values). For example.
                           // 'monomem[note].velocity' would be the velocity
                           // value of the note 'note'.
        PartNotes partnote[POLIPHONY];
        float *tmpoutl;
        float *tmpoutr;
        float oldfreq; // for portamento
        Microtonal *microtonal;
        FFTwrapper *fft;
        
        int partMuted;
};

#endif
