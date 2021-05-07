/*
    Part.h - Part implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011 Alan Calvert
    Copyright 2014-2021, Will Godfrey

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

    This file is derivative of original ZynAddSubFX code.

*/

#ifndef PART_H
#define PART_H

#include <list>
#include <string>
#include "globals.h"

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

class SynthEngine;

class Part
{
    public:
        enum NoteStatus { KEY_OFF, KEY_PLAYING, KEY_RELEASED_AND_SUSTAINED, KEY_RELEASED };

        Part(Microtonal *microtonal_, FFTwrapper *fft_, SynthEngine *_synth);
        ~Part();
        inline float pannedVolLeft(void) { return volume * pangainL; }
        inline float pannedVolRight(void) { return volume * pangainR; }
        void defaults(void);
        void setNoteMap(int keyshift);
        void defaultsinstrument(void);
        void cleanup(void);

        // Midi commands implemented
        void setChannelAT(int type, int value);
        void setKeyAT(int note, int type, int value);
        void NoteOn(int note, int velocity, bool renote = false);
        void NoteOff(int note);
        void AllNotesOff(void) { killallnotes = true; }; // panic, prepare all notes to be turned off
        void SetController(unsigned int type, int par);
        void ReleaseSustainedKeys(void);
        void ReleaseAllKeys(void);
        void ComputePartSmps(void);

        bool saveXML(std::string filename, bool yoshiFormat); // result true for load ok, otherwise false
        int loadXMLinstrument(std::string filename);
        void add2XML(XMLwrapper *xml, bool subset = false);
        void add2XMLinstrument(XMLwrapper *xml);
        void getfromXML(XMLwrapper *xml);
        void getfromXMLinstrument(XMLwrapper *xml);
        float getLimits(CommandBlock *getData);

        Controller *ctl;

        // part's kit
        struct {
            std::string   Pname;
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
        void enforcekeylimit(void);
        void setkititemstatus(int kititem, int Penabled_);
        void setVolume(float value);
        void checkVolume(float step);
        void setDestination(int value);
        void checkPanning(float step, unsigned char panLaw);

        SynthEngine *getSynthEngine() {return synth;}

        bool PyoshiType;
        int PmapOffset;
        float PnoteMap[256];
        float         Pvolume;
        float         TransVolume;
        float         Ppanning;
        float         TransPanning;
        char Penabled; // this *must* be signed
        unsigned char Pminkey;
        unsigned char Pmaxkey;
        unsigned char Pkeyshift;
        unsigned char Prcvchn;
        unsigned char Pvelsns;     // velocity sensing (amplitude velocity scale)
        unsigned char Pveloffs;    // velocity offset
        unsigned char Pkitmode;    // if the kitmode is enabled
        bool          Pkitfade;    // enables cross fading
        unsigned char Pdrummode;   // if all keys are mapped and the system is 12tET (used for drums)
        unsigned char Pkeymode;    // 0 = poly, 1 = mono, > 1 = legato;
        unsigned int  PchannelATchoice;
        unsigned int  PkeyATchoice;
        unsigned char Pkeylimit;   // how many keys can play simultaneously,
                                   // time 0 = off, the older will be released
        float         Pfrand;      // Part random frequency content
        float         Pvelrand;    // Part random velocity content
        unsigned char PbreathControl;
        unsigned char Peffnum;
        int           Paudiodest;  // jack output routing
        std::string   Pname;
        std::string   Poriginal;

        struct Info {
            unsigned char Ptype;
            std::string   Pauthor;
            std::string   Pcomments;
        };
        Info info;

        float *partoutl;
        float *partoutr;

        float *partfxinputl[NUM_PART_EFX + 1]; // Left and right signal that pass-through part effects
        float *partfxinputr[NUM_PART_EFX + 1]; // [NUM_PART_EFX] is for "no effect" buffer

        unsigned char Pefxroute[NUM_PART_EFX]; // how the effect's output is
                                               // routed (to next effect/to out)
        bool Pefxbypass[NUM_PART_EFX + 1];     // if the effects are bypassed,
                                               // [NUM_PART_EFX] is for "no effect" buffer
        EffectMgr *partefx[NUM_PART_EFX];      // insertion part effects - part of the instrument

        float volume;      // applied by MasterAudio
        float pangainL;
        float pangainR;
        int lastnote;
        bool busy;


    private:
        void KillNotePos(int pos);
        void ReleaseNotePos(int pos);
        void MonoMemRenote(void); // MonoMem stuff.
        void setPan(float value);

        Microtonal *microtonal;
        FFTwrapper *fft;

        struct PartNotes {
            NoteStatus status;
            int note;          // if there is no note playing, "note" = -1
            int itemsplaying;
            int keyATtype;
            int keyATvalue;
            struct Kititem {
                ADnote *adnote;
                SUBnote *subnote;
                PADnote *padnote;
                int sendtoparteffect;
            };
            Kititem kititem[NUM_KIT_ITEMS];
            int time;
        };

        PartNotes partnote[POLIPHONY];

        int lastpos;              // previous pos and posb.
        int lastposb;             // ^^
        bool lastlegatomodevalid; // previous legatomodevalid.

        int oldFilterState; // these for channel aftertouch
        int oldFilterQstate;
        int oldBendState;
        float oldVolumeState;
        float oldVolumeAdjust;
        int oldModulationState;

        float *tmpoutl;
        float *tmpoutr;
        float oldfreq; // for portamento
//        int partMuted;  // **** RHL ****
        bool killallnotes;

        // MonoMem stuff
        std::list<unsigned char> monomemnotes; // held notes.
        struct {
            unsigned char velocity;
        } monomem[256];    // 256 is to cover all possible note values. monomem[]
                           // is used in conjunction with the list to store the
                           // velocity value of a given note
                           // (the list only store note values). For example.
                           // 'monomem[note].velocity' would be the velocity
                           // value of the note 'note'.

        SynthEngine *synth;
};

#endif
