/*
    Part.h - Part implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011 Alan Calvert
    Copyright 2014-2024, Will Godfrey

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

    This file is derivative of original ZynAddSubFX code.

*/

#ifndef PART_H
#define PART_H

#include "globals.h"
#include "DSP/FFTwrapper.h"
#include "Params/ParamCheck.h"
#include "Misc/Alloc.h"

#include <memory>
#include <string>
#include <list>

using std::string;

class ADnoteParameters;
class SUBnoteParameters;
class PADnoteParameters;
class ADnote;
class SUBnote;
class PADnote;
class Controller;
class Microtonal;
class EffectMgr;
class XMLtree;

class SynthEngine;

class Part
{
    public:
        enum NoteStatus { KEY_OFF, KEY_PLAYING, KEY_RELEASED_AND_SUSTAINED, KEY_RELEASED };

        enum class Omni
        {
            NotSet,
            Enabled,
            Disabled,
        };

       ~Part();
        Part(uchar id, Microtonal*, fft::Calc&, SynthEngine&);

        // shall not be copied or moved
        Part(Part&&)                 = delete;
        Part(Part const&)            = delete;
        Part& operator=(Part&&)      = delete;
        Part& operator=(Part const&) = delete;

        inline float pannedVolLeft()  { return volume * pangainL; }
        inline float pannedVolRight() { return volume * pangainR; }
        void reset(int npart);
        void defaults(int npart);
        void setNoteMap(int keyshift);
        void defaultsinstrument();
        void cleanup();

        // Midi commands implemented
        void setChannelAT(int type, int value);
        void setKeyAT(int note, int type, int value);
        void NoteOn(int note, int velocity, bool renote = false);
        void NoteOff(int note);
        void AllNotesOff() { killallnotes = true; }; // panic, prepare all notes to be turned off
        void SetController(unsigned int type, int par);
        void ReleaseSustainedKeys();
        void ReleaseAllKeys();
        void ComputePartSmps();
        void resetOmniCC() { omniByCC = Omni::NotSet; }
        bool isOmni()
        {
            return omniByCC == Omni::Enabled or (omniByCC == Omni::NotSet and Pomni);
        }

        bool saveXML(string filename, bool yoshiFormat); // result true for load ok, otherwise false
        int  loadXML(string filename);
        void add2XML_YoshimiPartSetup(XMLtree&);
        void add2XML_YoshimiInstrument(XMLtree&);
        void getfromXML(XMLtree&);
        float getLimits(CommandBlock* getData);

        std::unique_ptr<Controller> ctl;

        // part's kit
        struct KitItem
        {
            string Pname;
            uchar  Penabled;
            uchar  Pmuted;
            uchar  Pminkey;
            uchar  Pmaxkey;
            uchar  Padenabled;
            uchar  Psubenabled;
            uchar  Ppadenabled;
            uchar  Psendtoparteffect;
            ADnoteParameters  *adpars;
            SUBnoteParameters *subpars;
            PADnoteParameters *padpars;
        };
        KitItem kit[NUM_KIT_ITEMS];

        // Part parameters
        void enforcekeylimit();
        void setkititemstatus(int kititem, int Penabled_);
        void setVolume(float value);
        void checkVolume(float step);
        void setDestination(int value);
        void checkPanning(float step, uchar panLaw);

        bool   PyoshiType;
        float  PnoteMap[MAX_OCTAVE_SIZE];
        float  Pvolume;
        float  TransVolume;
        float  Ppanning;
        float  TransPanning;
        char   Penabled;       // this *must* be signed
        uchar  Pminkey;
        uchar  Pmaxkey;
        uchar  Pkeyshift;
        uchar  Prcvchn;
        bool   Pomni;
        uchar  Pvelsns;        // velocity sensing (amplitude velocity scale)
        uchar  Pveloffs;       // velocity offset
        uchar  Pkitmode;       // Part uses kit mode: 0 == off, 1 == on, 2 == "Single": only first applicable kit item can play
        uchar  PkitfadeType;   // type of cross fade, 0 off (multi)
        uchar  Pdrummode;      // if all keys are mapped and the system is 12tET (used for drums)
        uchar  Pkeymode;       // 0 = poly, 1 = mono, > 1 = legato;
        uint   PchannelATchoice;
        uint   PkeyATchoice;
        uchar  Pkeylimit;      // how many keys can play simultaneously,
                               // time 0 = off, the older will be released
        float  Pfrand;         // Part random frequency content
        float  Pvelrand;       // Part random velocity content
        uchar  PbreathControl;
        uchar  Peffnum;
        int    Paudiodest;     // jack output routing
        string Pname;
        string Poriginal;

        struct Info {
            uchar  Ptype;
            string Pauthor;
            string Pcomments;
        };
        Info info;
        const uchar partID;

        Samples partoutl;
        Samples partoutr;

        Samples partfxinputl[NUM_PART_EFX + 1]; // Left and right signal that pass-through part effects
        Samples partfxinputr[NUM_PART_EFX + 1]; // [NUM_PART_EFX] is for "no effect" buffer

        uchar Pefxroute[NUM_PART_EFX];         // how the effect's output is
                                               // routed (to next effect/to out)
        bool Pefxbypass[NUM_PART_EFX + 1];     // if the effects are bypassed,
                                               // [NUM_PART_EFX] is for "no effect" buffer
        EffectMgr *partefx[NUM_PART_EFX];      // insertion part effects - part of the instrument

        float volume;   // applied by MasterAudio
        float pangainL;
        float pangainR;
        bool  busy;

        int getLastNote()  const { return this->prevNote; }
        SynthEngine& getSynthEngine() const {return synth;}

    private:
        void getfromXML_InstrumentData(XMLtree&);
        void add2XML_InstrumentData(XMLtree&);
        void add2XML_synthUsage(XMLtree&);

        void setPan(float value);
        void KillNotePos(int pos);
        void ReleaseNotePos(int pos);
        void monoNoteHistoryRecall();

        void startNewNotes        (int pos, size_t item, size_t currItem, Note, bool portamento);
        void startLegato          (int pos, size_t item, size_t currItem, Note);
        void startLegatoPortamento(int pos, size_t item, size_t currItem, Note);
        float computeKitItemCrossfade(size_t item, int midiNote);
        void incrementItemsPlaying(int pos, size_t currItem);

        Samples& tmpoutl;
        Samples& tmpoutr;

        Microtonal* microtonal;
        fft::Calc&  fft;

        struct PartNotes {
            NoteStatus status;
            int note;          // if there is no note playing, "note" = -1
            int time;
            int keyATtype;
            int keyATvalue;
            size_t itemsplaying;

            struct KitItemNotes {
                ADnote* adnote;
                SUBnote* subnote;
                PADnote* padnote;
                int sendtoparteffect;
            };
            KitItemNotes kitItem[NUM_KIT_ITEMS];
        };                     // Note: kitItems are "packed", not using the same Index as in KitItem-array

        PartNotes partnote[POLYPHONY];

        int   prevNote;        // previous MIDI note
        int   prevPos;         // previous note pos
        float prevFreq;        // frequency of previous note (for portamento)
        bool  prevLegatoMode;  // previous note hat legato mode activated

        bool  killallnotes;    // "panic" switch

        int   oldFilterState;  // these for channel aftertouch
        int   oldFilterQstate;
        int   oldBendState;
        float oldVolumeState;
        float oldVolumeAdjust;
        int   oldModulationState;

        // MonoNote stuff
        std::list<uchar> monoNoteHistory; // held notes.
        struct {
            float velocity;
            float noteVolume;
        } monoNote[256];   // 256 is to cover all possible note values. monoNote[]
                           // is used in conjunction with the list to store the velocity value of a given note
                           // (the list only store note values). For example:
                           // 'monoNote[note].velocity' would be the velocity value of the note 'note'.

        Omni omniByCC;

        SynthEngine& synth;
};

#endif /*PART_H*/
