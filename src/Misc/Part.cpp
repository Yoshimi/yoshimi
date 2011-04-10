/*
    Part.cpp - Part implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, James Morris
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified April 2011
*/

#include <cstring>
#include <cmath>

using namespace std;

#include "Params/ADnoteParameters.h"
#include "Params/SUBnoteParameters.h"
#include "Params/PADnoteParameters.h"
#include "Synth/ADnote.h"
#include "Synth/SUBnote.h"
#include "Synth/PADnote.h"
#include "Params/Controller.h"
#include "Effects/EffectMgr.h"
#include "DSP/FFTwrapper.h"
#include "Misc/Microtonal.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Synth/Resonance.h"
#include "Synth/BodyDisposal.h"
#include "Misc/Part.h"

Part::Part(Microtonal *microtonal_, FFTwrapper *fft_) :
    microtonal(microtonal_),
    fft(fft_),
    partMuted(0),
    killallnotes(false)
{
    ctl = new Controller();
    partoutl = (float*)fftwf_malloc(synth->bufferbytes);
    memset(partoutl, 0, synth->bufferbytes);
    partoutr = (float*)fftwf_malloc(synth->bufferbytes);
    memset(partoutr, 0, synth->bufferbytes);
    tmpoutl = (float*)fftwf_malloc(synth->bufferbytes);
    memset(tmpoutl, 0, synth->bufferbytes);
    tmpoutr = (float*)fftwf_malloc(synth->bufferbytes);
    memset(tmpoutr, 0, synth->bufferbytes);

    for (int n = 0; n < NUM_KIT_ITEMS; ++n)
    {
        kit[n].Pname.clear();
        kit[n].adpars = NULL;
        kit[n].subpars = NULL;
        kit[n].padpars = NULL;
    }

    kit[0].adpars = new ADnoteParameters(microtonal, fft);
    kit[0].subpars = new SUBnoteParameters();
    kit[0].padpars = new PADnoteParameters(fft);

    // Part's Insertion Effects init
    for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
        partefx[nefx] = new EffectMgr(1);

    for (int n = 0; n < NUM_PART_EFX + 1; ++n)
    {
        partfxinputl[n] = (float*)fftwf_malloc(synth->bufferbytes);
        memset(partfxinputl[n], 0, synth->bufferbytes);
        partfxinputr[n] = (float*)fftwf_malloc(synth->bufferbytes);
        memset(partfxinputr[n], 0, synth->bufferbytes);
        Pefxbypass[n] = false;
    }

    oldfreq = -1.0f;

    int i, j;
    for (i = 0; i < POLIPHONY; ++i)
    {
        partnote[i].status = KEY_OFF;
        partnote[i].note = -1;
        partnote[i].itemsplaying = 0;
        for (j = 0; j < NUM_KIT_ITEMS; ++j)
        {
            partnote[i].kititem[j].adnote = NULL;
            partnote[i].kititem[j].subnote = NULL;
            partnote[i].kititem[j].padnote = NULL;
        }
        partnote[i].time = 0;
    }
    cleanup();
    Pname.clear();

    oldvolumel = oldvolumer = 0.5f;
    lastnote = -1;
    lastpos = 0; // lastpos will store previously used NoteOn(...)'s pos.
    lastlegatomodevalid = false; // To store previous legatomodevalid value.
    defaults();
}


void Part::defaults(void)
{
    Penabled = 0;
    Pminkey = 0;
    Pmaxkey = 127;
    Pnoteon = 1;
    Ppolymode = 1;
    Plegatomode = 0;
    setVolume(96);
    Pkeyshift = 64;
    Prcvchn = 0;
    setPan(Ppanning = 64);
    Pvelsns = 64;
    Pveloffs = 64;
    Pkeylimit = 15;
    defaultsinstrument();
    ctl->defaults();
}

void Part::defaultsinstrument(void)
{
    Pname.clear();

    info.Ptype = 0;
    info.Pauthor.clear();
    info.Pcomments.clear();

    Pkitmode = 0;
    Pdrummode = 0;

    for (int n = 0; n < NUM_KIT_ITEMS; ++n)
    {
        kit[n].Penabled = 0;
        kit[n].Pmuted = 0;
        kit[n].Pminkey = 0;
        kit[n].Pmaxkey = 127;
        kit[n].Padenabled = 0;
        kit[n].Psubenabled = 0;
        kit[n].Ppadenabled = 0;
        kit[n].Pname.clear();
        kit[n].Psendtoparteffect = 0;
        if (n != 0)
            setkititemstatus(n, 0);
    }
    kit[0].Penabled = 1;
    kit[0].Padenabled = 1;
    kit[0].adpars->defaults();
    kit[0].subpars->defaults();
    kit[0].padpars->defaults();

    for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
    {
        partefx[nefx]->defaults();
        Pefxroute[nefx] = 0; // route to next effect
    }
}


// Cleanup the part
void Part::cleanup(void)
{
    Mute();
    for (int k = 0; k < POLIPHONY; ++k)
        KillNotePos(k);
    memset(partoutl, 0, synth->bufferbytes);
    memset(partoutr, 0, synth->bufferbytes);
    memset(tmpoutl, 0, synth->bufferbytes);
    memset(tmpoutr, 0, synth->bufferbytes);

    ctl->resetall();
    for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
        partefx[nefx]->cleanup();
    for (int n = 0; n < NUM_PART_EFX + 1; ++n)
    {
        memset(partfxinputl[n], 0, synth->bufferbytes);
        memset(partfxinputr[n], 0, synth->bufferbytes);

    }
    Unmute();
}


Part::~Part()
{
    cleanup();
    for (int n = 0; n < NUM_KIT_ITEMS; ++n)
    {
        if (kit[n].adpars)
            delete kit[n].adpars;
        if (kit[n].subpars)
            delete kit[n].subpars;
        if (kit[n].padpars)
            delete kit[n].padpars;
    }
    fftwf_free(partoutl);
    fftwf_free(partoutr);
    fftwf_free(tmpoutl);
    fftwf_free(tmpoutr);
    for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
    {
        if (partefx[nefx])
            delete partefx[nefx];
    }
    for (int n = 0; n < NUM_PART_EFX + 1; ++n)
    {
        if (partfxinputl[n])
            fftwf_free(partfxinputl[n]);
        if (partfxinputr[n])
            fftwf_free(partfxinputr[n]);
    }
    if (ctl)
        delete ctl;
}


// Note On Messages
void Part::NoteOn(int note, int velocity, int masterkeyshift)
{
    if (!Pnoteon || note < Pminkey || note > Pmaxkey)
        return;

    // Legato and MonoMem used vars:
    int posb = POLIPHONY - 1;     // Just a dummy initial value.
    bool legatomodevalid = false;   // true when legato mode is determined applicable.
    bool doinglegato = false;     // true when we determined we do a legato note.
    bool ismonofirstnote = false; // (In Mono/Legato) true when we determined
				                  // no other notes are held down or sustained.*/
    int lastnotecopy = lastnote;  // Useful after lastnote has been changed.

    // MonoMem stuff:
    if (!Ppolymode) // if Poly is off
    {
        monomemnotes.push_back(note);            // Add note to the list.
        monomem[note].velocity = velocity;       // Store this note's velocity.
        monomem[note].mkeyshift = masterkeyshift;
        if (partnote[lastpos].status != KEY_PLAYING
            && partnote[lastpos].status != KEY_RELASED_AND_SUSTAINED)
        {
            ismonofirstnote = true; // No other keys are held or sustained.
        }
    }
    else // Poly mode is On, so just make sure the list is empty.
    {
        if (not monomemnotes.empty())
            monomemnotes.clear();
    }
    lastnote = note;
    int pos = -1;
    for (int i = 0; i < POLIPHONY; ++i)
    {
        if (partnote[i].status == KEY_OFF)
        {
            pos = i;
            break;
        }
    }
    if (Plegatomode && !Pdrummode)
    {
        if (Ppolymode)
        {
            Runtime.Log("Warning, poly and legato modes are both on.");
            Runtime.Log("That should not happen, so disabling legato mode");
            Plegatomode = 0;
        }
        else
        {
            // Legato mode is on and applicable.
            legatomodevalid = true;
            if ((not ismonofirstnote) && lastlegatomodevalid)
            {
                // At least one other key is held or sustained, and the
                // previous note was played while in valid legato mode.
                doinglegato = true; // So we'll do a legato note.
                pos = lastpos; // A legato note uses same pos as previous..
                posb = lastposb; // .. same goes for posb.
            }
            else
            {
                // Legato mode is valid, but this is only a first note.
                for (int i = 0; i < POLIPHONY; ++i)
                    if (partnote[i].status == KEY_PLAYING
                        || partnote[i].status == KEY_RELASED_AND_SUSTAINED)
                        RelaseNotePos(i);

                // Set posb
                posb = (pos + 1) % POLIPHONY; // We really want it (if the following fails)
                for (int i = 0; i < POLIPHONY; ++i)
                {
                    if (partnote[i].status == KEY_OFF && pos != i)
                    {
                        posb = i;
                        break;
                    }
                }
            }
            lastposb = posb;// Keep a trace of used posb
        }
    }
    else
    {
        // Legato mode is either off or non-applicable.
        if (Ppolymode == 0)
        {   // if the mode is 'mono' turn off all other notes
            for (int i = 0; i < POLIPHONY; ++i)
                if (partnote[i].status == KEY_PLAYING)
                RelaseNotePos(i);
            RelaseSustainedKeys();
        }
    }
    lastlegatomodevalid = legatomodevalid;

    if (pos == -1)
    {
        // test
        Runtime.Log("Too may notes - notes > poliphony, PartNoteOn()");
    }
    else
    {
        // start the note
        partnote[pos].status = KEY_PLAYING;
        partnote[pos].note = note;
        if (legatomodevalid) {
            partnote[posb].status = KEY_PLAYING;
            partnote[posb].note = note;
        }

        // compute the velocity offset
        float vel = velF(velocity / 127.0f, Pvelsns) + (Pveloffs - 64.0f) / 64.0f;
        vel = (vel < 0.0f) ? 0.0f : vel;
        vel = (vel > 1.0f) ? 1.0f : vel;

        // compute the keyshift
        int partkeyshift = (int)Pkeyshift - 64;
        int keyshift = masterkeyshift + partkeyshift;

        // initialise note frequency
        float notebasefreq;
        if (!Pdrummode)
        {
            if ((notebasefreq = microtonal->getNoteFreq(note, keyshift)) < 0.0f)
                return; // the key is not mapped
        }
        else
            notebasefreq = microtonal->getNoteFreq(note);

        // Portamento
        if (oldfreq < 1.0f)
            oldfreq = notebasefreq; // this is only the first note is played

        // For Mono/Legato: Force Portamento Off on first
        // notes. That means it is required that the previous note is
        // still held down or sustained for the Portamento to activate
        // (that's like Legato).
        int portamento = 0;
        if (Ppolymode || !ismonofirstnote)
        {
            // I added a third argument to the
            // ctl->initportamento(...) function to be able
            // to tell it if we're doing a legato note.
            portamento = ctl->initportamento(oldfreq, notebasefreq, doinglegato);
        }

        if (portamento)
            ctl->portamento.noteusing = pos;
        oldfreq = notebasefreq;
        lastpos = pos; // Keep a trace of used pos.
        if (doinglegato)
        {
            // Do Legato note
            if (!Pkitmode)
            {   // "normal mode" legato note
                if ((kit[0].Padenabled)
                    && (partnote[pos].kititem[0].adnote)
                    && (partnote[posb].kititem[0].adnote))
                {
                    partnote[pos].kititem[0].adnote->
                        ADlegatonote(notebasefreq, vel, portamento, note, true);
                    partnote[posb].kititem[0].adnote->
                        ADlegatonote(notebasefreq, vel, portamento, note, true);
                            // 'true' is to tell it it's being called from here.
                }

                if ((kit[0].Psubenabled)
                    && (partnote[pos].kititem[0].subnote)
                    && (partnote[posb].kititem[0].subnote))
                {
                    partnote[pos].kititem[0].subnote->
                        SUBlegatonote(notebasefreq, vel, portamento, note, true);
                    partnote[posb].kititem[0].subnote->
                        SUBlegatonote(notebasefreq, vel, portamento, note, true);
                }

                if ((kit[0].Ppadenabled)
                    && (partnote[pos].kititem[0].padnote)
                    && (partnote[posb].kititem[0].padnote))
                {
                    partnote[pos].kititem[0].padnote->
                        PADlegatonote(notebasefreq, vel, portamento, note, true);
                    partnote[posb].kititem[0].padnote->
                        PADlegatonote(notebasefreq, vel, portamento, note, true);
                }

            }
            else
            {   // "kit mode" legato note
                int ci = 0;
                for (int item = 0; item < NUM_KIT_ITEMS; ++item)
                {
                    if (kit[item].Pmuted)
                        continue;
                    if ((note < kit[item].Pminkey) || (note > kit[item].Pmaxkey))
                        continue;

                    if ((lastnotecopy < kit[item].Pminkey)
                        || (lastnotecopy > kit[item].Pmaxkey))
                        continue; // We will not perform legato across 2 key regions.

                    partnote[pos].kititem[ci].sendtoparteffect =
                        ( kit[item].Psendtoparteffect < NUM_PART_EFX)
                            ? kit[item].Psendtoparteffect
                            : NUM_PART_EFX; // if this parameter is 127 for "unprocessed"
                    partnote[posb].kititem[ci].sendtoparteffect =
                        ( kit[item].Psendtoparteffect < NUM_PART_EFX)
                            ? kit[item].Psendtoparteffect
                            : NUM_PART_EFX;

                    if ((kit[item].Padenabled)
                        && (kit[item].adpars)
                        && (partnote[pos].kititem[ci].adnote)
                        && (partnote[posb].kititem[ci].adnote))
                    {
                        partnote[pos].kititem[ci].adnote->
                            ADlegatonote(notebasefreq, vel, portamento, note, true);
                        partnote[posb].kititem[ci].adnote->
                            ADlegatonote(notebasefreq, vel, portamento, note, true);
                    }
                    if ((kit[item].Psubenabled)
                        && (kit[item].subpars)
                        && (partnote[pos].kititem[ci].subnote)
                        && (partnote[posb].kititem[ci].subnote))
                    {
                        partnote[pos].kititem[ci].subnote->
                            SUBlegatonote(notebasefreq, vel, portamento, note, true);
                        partnote[posb].kititem[ci].subnote->
                            SUBlegatonote(notebasefreq, vel, portamento, note, true);
                    }
                    if ((kit[item].Ppadenabled)
                        && (kit[item].padpars)
                        && (partnote[pos].kititem[ci].padnote)
                        && (partnote[posb].kititem[ci].padnote))
                    {
                        partnote[pos].kititem[ci].padnote->
                            PADlegatonote(notebasefreq, vel, portamento, note, true);
                        partnote[posb].kititem[ci].padnote->
                            PADlegatonote(notebasefreq, vel, portamento, note, true);
                    }

                    if ((kit[item].adpars)
                        || (kit[item].subpars)
                        || (kit[item].padpars))
                    {
                        ci++;
                        if (Pkitmode == 2
                            && (kit[item].Padenabled
                                || kit[item].Psubenabled
                                || kit[item].Ppadenabled))
                        break;
                    }
                }
                if (ci == 0)
                {
                    // No legato were performed at all, so pretend nothing happened:
                    monomemnotes.pop_back(); // Remove last note from the list.
                    lastnote = lastnotecopy; // Set lastnote back to previous value.
                }
            }
            return; // Ok, Legato note done, return.
        }

        partnote[pos].itemsplaying = 0;
        if (legatomodevalid)
            partnote[posb].itemsplaying = 0;

        if (!Pkitmode)
        {   // init the notes for the "normal mode"
            partnote[pos].kititem[0].sendtoparteffect = 0;
            if (kit[0].Padenabled)
                partnote[pos].kititem[0].adnote =
                    new ADnote(kit[0].adpars, ctl, vel, portamento, note, false ); // not silent
            if (kit[0].Psubenabled)
                partnote[pos].kititem[0].subnote =
                    new SUBnote(kit[0].subpars, ctl, notebasefreq, vel,
                                portamento, note, false);
            if (kit[0].Ppadenabled)
                partnote[pos].kititem[0].padnote =
                    new PADnote(kit[0].padpars, ctl, notebasefreq, vel,
                                portamento, note, false);
            if (kit[0].Padenabled || kit[0].Psubenabled || kit[0].Ppadenabled)
                partnote[pos].itemsplaying++;

            // Spawn another note (but silent) if legatomodevalid==true
            if (legatomodevalid)
            {
                partnote[posb].kititem[0].sendtoparteffect = 0;
                if (kit[0].Padenabled)
                    partnote[posb].kititem[0].adnote =
                        new ADnote(kit[0].adpars, ctl, vel, portamento, note, true); // silent
                if (kit[0].Psubenabled)
                    partnote[posb].kititem[0].subnote =
                        new SUBnote(kit[0].subpars, ctl, notebasefreq, vel,
                                    portamento, note, true);
                if (kit[0].Ppadenabled)
                    partnote[posb].kititem[0].padnote =
                        new PADnote(kit[0].padpars, ctl, notebasefreq, vel,
                                    portamento, note, true);
                if (kit[0].Padenabled || kit[0].Psubenabled || kit[0].Ppadenabled)
                    partnote[posb].itemsplaying++;
            }
        }
        else
        { // init the notes for the "kit mode"
            for (int item = 0; item < NUM_KIT_ITEMS; ++item)
            {
                if (kit[item].Pmuted)
                    continue;
                if (note < kit[item].Pminkey
                    || note>kit[item].Pmaxkey)
                    continue;

                int ci = partnote[pos].itemsplaying; // ci=current item

                partnote[pos].kititem[ci].sendtoparteffect =
                    (kit[item].Psendtoparteffect < NUM_PART_EFX)
                        ? kit[item].Psendtoparteffect
                        : NUM_PART_EFX; // if this parameter is 127 for "unprocessed"

                if (kit[item].adpars && kit[item].Padenabled)
                {
                    partnote[pos].kititem[ci].adnote =
                        new ADnote(kit[item].adpars, ctl, vel, portamento, note, false); // not silent
                }
                if (kit[item].subpars && kit[item].Psubenabled)
                    partnote[pos].kititem[ci].subnote =
                        new SUBnote(kit[item].subpars, ctl,notebasefreq, vel,
                                    portamento, note, false);

                if (kit[item].padpars && kit[item].Ppadenabled)
                    partnote[pos].kititem[ci].padnote =
                        new PADnote(kit[item].padpars, ctl, notebasefreq, vel,
                                    portamento, note, false);

                // Spawn another note (but silent) if legatomodevalid==true
                if (legatomodevalid)
                {
                    partnote[posb].kititem[ci].sendtoparteffect =
                        (kit[item].Psendtoparteffect < NUM_PART_EFX)
                            ? kit[item].Psendtoparteffect
                            : NUM_PART_EFX; // if this parameter is 127 for "unprocessed"

                    if (kit[item].adpars && kit[item].Padenabled)
                    {
                        partnote[posb].kititem[ci].adnote =
                            new ADnote(kit[item].adpars, ctl, vel, portamento, note, true); // silent
                    }
                    if (kit[item].subpars && kit[item].Psubenabled)
                        partnote[posb].kititem[ci].subnote =
                            new SUBnote(kit[item].subpars, ctl, notebasefreq,
                                        vel, portamento, note, true);
                    if (kit[item].padpars && kit[item].Ppadenabled)
                        partnote[posb].kititem[ci].padnote =
                            new PADnote(kit[item].padpars, ctl, notebasefreq,
                                        vel, portamento, note, true);

                    if (kit[item].adpars || kit[item].subpars)
                        partnote[posb].itemsplaying++;
                }

                if (kit[item].adpars || kit[item].subpars)
                {
                    partnote[pos].itemsplaying++;
                    if (Pkitmode == 2 && (kit[item].Padenabled
                                          || kit[item].Psubenabled
                                          || kit[item].Ppadenabled))
                        break;
                }
            }
        }
    }

    // this only relase the keys if there is maximum number of keys allowed
    setkeylimit(Pkeylimit);
}

// Note Off Messages
void Part::NoteOff(int note) //relase the key
{
    int i;

    // This note is released, so we remove it from the list.
    if (not monomemnotes.empty())
        monomemnotes.remove(note);

    for ( i = POLIPHONY - 1; i >= 0; i--)
    {   //first note in, is first out if there are same note multiple times
        if (partnote[i].status == KEY_PLAYING && partnote[i].note == note)
        {
            if (!ctl->sustain.sustain)
            {   //the sustain pedal is not pushed
                if (!Ppolymode && (not monomemnotes.empty()))
                    MonoMemRenote(); // To play most recent still held note.
                else
                    RelaseNotePos(i);
            }
            else
            {   // the sustain pedal is pushed
                partnote[i].status = KEY_RELASED_AND_SUSTAINED;
            }
        }
    }
}


// Controllers
void Part::SetController(unsigned int type, int par)
{
    switch (type)
    {
        case C_pitchwheel:
            ctl->setpitchwheel(par);
            break;
        case C_expression:
            ctl->setexpression(par);
            setVolume(Pvolume);
            break;
        case C_portamento:
            ctl->setportamento(par);
            break;
        case C_panning:
            setPan(par);
            break;
        case C_filtercutoff:
            ctl->setfiltercutoff(par);
            break;
        case C_filterq:
            ctl->setfilterq(par);
            break;
        case C_bandwidth:
            ctl->setbandwidth(par);
            break;
        case C_modwheel:
            ctl->setmodwheel(par);
            break;
        case C_fmamp:
            ctl->setfmamp(par);
            break;
        case C_volume:
            ctl->setvolume(par);
            if (ctl->volume.receive)
                volume = ctl->volume.volume;
            else
                setVolume(Pvolume);
            break;
        case C_sustain:
            ctl->setsustain(par);
            if (!ctl->sustain.sustain)
                RelaseSustainedKeys();
            break;
        case C_allsoundsoff:
            AllNotesOff(); // Panic
            break;
        case C_resetallcontrollers:
            ctl->resetall();
            RelaseSustainedKeys();
            if (ctl->volume.receive)
                volume = ctl->volume.volume;
            setVolume(Pvolume);
            setPan(Ppanning);

            for (int item = 0; item < NUM_KIT_ITEMS; ++item)
            {
                if (!kit[item].adpars)
                    continue;
                kit[item].adpars->GlobalPar.Reson->sendcontroller(C_resonance_center, 1.0);
                kit[item].adpars->GlobalPar.Reson->sendcontroller(C_resonance_bandwidth, 1.0);
            }
            // more update to add here if I add controllers
            break;
        case C_allnotesoff:
            RelaseAllKeys();
            break;
        case C_resonance_center:
            ctl->setresonancecenter(par);
            for (int item = 0; item < NUM_KIT_ITEMS; ++item)
            {
                if (!kit[item].adpars)
                    continue;
                kit[item].adpars->GlobalPar.Reson->sendcontroller(C_resonance_center,
                                                                  ctl->resonancecenter.relcenter);
            }
            break;
        case C_resonance_bandwidth:
            ctl->setresonancebw(par);
            kit[0].adpars->GlobalPar.Reson->sendcontroller(C_resonance_bandwidth,
                                                           ctl->resonancebandwidth.relbw);
            break;
    }
}


// Relase the sustained keys
void Part::RelaseSustainedKeys(void)
{
    // Let's call MonoMemRenote() on some conditions:
    if (Ppolymode == 0 && (not monomemnotes.empty()))
        if (monomemnotes.back() != lastnote)
            // Sustain controller manipulation would cause repeated same note
            // respawn without this check.
            MonoMemRenote(); // To play most recent still held note.

    for (int i = 0; i < POLIPHONY; ++i)
        if (partnote[i].status == KEY_RELASED_AND_SUSTAINED)
            RelaseNotePos(i);
}


// Relase all keys
void Part::RelaseAllKeys(void)
{
    for (int i = 0; i < POLIPHONY; ++i)
    {
        if (partnote[i].status != KEY_RELASED
            && partnote[i].status != KEY_OFF) //thanks to Frank Neumann
            RelaseNotePos(i);
    }
}


// Call NoteOn(...) with the most recent still held key as new note
// (Made for Mono/Legato).
void Part::MonoMemRenote(void)
{
    unsigned char mmrtempnote = monomemnotes.back(); // Last list element.
    monomemnotes.pop_back(); // We remove it, will be added again in NoteOn(...).
    if (Pnoteon == 0)
        RelaseNotePos(lastpos);
    else
        NoteOn(mmrtempnote, monomem[mmrtempnote].velocity,
               monomem[mmrtempnote].mkeyshift);
}


// Release note at position
void Part::RelaseNotePos(int pos)
{

    for (int j = 0; j < NUM_KIT_ITEMS; ++j)
    {
        if (partnote[pos].kititem[j].adnote)
            if (partnote[pos].kititem[j].adnote)
                partnote[pos].kititem[j].adnote->relasekey();

        if (partnote[pos].kititem[j].subnote)
            if (partnote[pos].kititem[j].subnote)
                partnote[pos].kititem[j].subnote->relasekey();

        if (partnote[pos].kititem[j].padnote)
            if (partnote[pos].kititem[j].padnote)
                partnote[pos].kititem[j].padnote->relasekey();
    }
    partnote[pos].status = KEY_RELASED;
}


// Kill note at position
void Part::KillNotePos(int pos)
{
    partnote[pos].status = KEY_OFF;
    partnote[pos].note = -1;
    partnote[pos].time = 0;
    partnote[pos].itemsplaying = 0;

    for (int j = 0; j < NUM_KIT_ITEMS; ++j)
    {
        if (partnote[pos].kititem[j].adnote)
        {
            Runtime.deadObjects->addBody(partnote[pos].kititem[j].adnote);
            partnote[pos].kititem[j].adnote = NULL;
        }
        if (partnote[pos].kititem[j].subnote)
        {
            Runtime.deadObjects->addBody(partnote[pos].kititem[j].subnote);
            partnote[pos].kititem[j].subnote = NULL;
        }
        if (partnote[pos].kititem[j].padnote)
        {
            Runtime.deadObjects->addBody(partnote[pos].kititem[j].padnote);
            partnote[pos].kititem[j].padnote = NULL;
        }
    }
    if (pos == ctl->portamento.noteusing)
    {
        ctl->portamento.noteusing = -1;
        ctl->portamento.used = 0;
    }
}


// Set Part's key limit
void Part::setkeylimit(unsigned char Pkeylimit_)
{
    Pkeylimit = Pkeylimit_;
    int keylimit = Pkeylimit;
    if (!keylimit)
        keylimit = POLIPHONY - 5;

    // release old keys if the number of notes>keylimit
    if (Ppolymode)
    {
        int notecount = 0;
        for (int i = 0; i < POLIPHONY; ++i)
        {
            if (partnote[i].status == KEY_PLAYING
                || partnote[i].status == KEY_RELASED_AND_SUSTAINED)
                notecount++;
        }
        int oldestnotepos = -1, maxtime = 0;
        if (notecount > keylimit)
        {   // find out the oldest note
            for (int i = 0; i < POLIPHONY; ++i)
            {
                if ((partnote[i].status == KEY_PLAYING
                    || partnote[i].status == KEY_RELASED_AND_SUSTAINED)
                        && partnote[i].time > maxtime)
                {
                    maxtime = partnote[i].time;
                    oldestnotepos = i;
                }
            }
        }
        if (oldestnotepos != -1)
            RelaseNotePos(oldestnotepos);
    }
}


// Compute Part samples and store them in the partoutl[] and partoutr[]
void Part::ComputePartSmps(void)
{
    if (isMuted())
    {
        memset(partoutl, 0, synth->bufferbytes);
        memset(partoutr, 0, synth->bufferbytes);
        return;
    }

    int k;
    int noteplay; // 0 if there is nothing activated
    for (int nefx = 0; nefx < NUM_PART_EFX + 1; ++nefx)
    {
        memset(partfxinputl[nefx], 0, synth->bufferbytes);
        memset(partfxinputr[nefx], 0, synth->bufferbytes);
    }

    for (k = 0; k < POLIPHONY; ++k)
    {
        if (partnote[k].status == KEY_OFF)
            continue;
        noteplay = 0;
        partnote[k].time++;
        // get the sampledata of the note and kill it if it's finished
        for (int item = 0; item < partnote[k].itemsplaying; ++item)
        {
            int sendcurrenttofx = partnote[k].kititem[item].sendtoparteffect;
            ADnote *adnote = partnote[k].kititem[item].adnote;
            SUBnote *subnote = partnote[k].kititem[item].subnote;
            PADnote *padnote = partnote[k].kititem[item].padnote;
            // get from the ADnote
            if (adnote)
            {
                noteplay++;
                if (adnote->ready)
                    adnote->noteout(tmpoutl, tmpoutr);
                else
                {
                    memset(tmpoutl, 0, synth->bufferbytes);
                    memset(tmpoutr, 0, synth->bufferbytes);
                }
                if (adnote->finished())
                {
                    Runtime.deadObjects->addBody(partnote[k].kititem[item].adnote);
                    partnote[k].kititem[item].adnote = NULL;
                }
                for (int i = 0; i < synth->buffersize; ++i)
                {   // add the ADnote to part(mix)
                    partfxinputl[sendcurrenttofx][i] += tmpoutl[i];
                    partfxinputr[sendcurrenttofx][i] += tmpoutr[i];
                }
            }
            // get from the SUBnote
            if (subnote)
            {
                noteplay++;
                if (subnote->ready)
                    subnote->noteout(tmpoutl, tmpoutr);
                else
                {
                    memset(tmpoutl, 0, synth->bufferbytes);
                    memset(tmpoutr, 0, synth->bufferbytes);
                }
                for (int i = 0; i < synth->buffersize; ++i)
                {   // add the SUBnote to part(mix)
                    partfxinputl[sendcurrenttofx][i] += tmpoutl[i];
                    partfxinputr[sendcurrenttofx][i] += tmpoutr[i];
                }
                if (subnote->finished())
                {
                    Runtime.deadObjects->addBody(partnote[k].kititem[item].subnote);
                    partnote[k].kititem[item].subnote = NULL;
                }
            }
            // get from the PADnote
            if (padnote)
            {
                noteplay++;
                if (padnote->ready)
                {
                    padnote->noteout(tmpoutl, tmpoutr);
                }
                else
                {
                    memset(tmpoutl, 0, synth->bufferbytes);
                    memset(tmpoutr, 0, synth->bufferbytes);
                }
                if (padnote->finished())
                {
                    Runtime.deadObjects->addBody(partnote[k].kititem[item].padnote);
                    partnote[k].kititem[item].padnote = NULL;
                }
                for (int i = 0 ; i < synth->buffersize; ++i)
                {   // add the PADnote to part(mix)
                    partfxinputl[sendcurrenttofx][i] += tmpoutl[i];
                    partfxinputr[sendcurrenttofx][i] += tmpoutr[i];
                }
            }
        }
        // Kill note if there is no synth on that note
        if (noteplay == 0)
            KillNotePos(k);
    }

    // Apply part's effects and mix them
    for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
    {
        if (!Pefxbypass[nefx])
        {
            partefx[nefx]->out(partfxinputl[nefx], partfxinputr[nefx]);
            if (Pefxroute[nefx] == 2)
            {
                for (int i = 0; i < synth->buffersize; ++i)
                {
                    partfxinputl[nefx + 1][i] += partefx[nefx]->efxoutl[i];
                    partfxinputr[nefx + 1][i] += partefx[nefx]->efxoutr[i];
                }
            }
        }
        int routeto = (Pefxroute[nefx] == 0) ? nefx + 1 : NUM_PART_EFX;
        for (int i = 0; i < synth->buffersize; ++i)
        {
            partfxinputl[routeto][i] += partfxinputl[nefx][i];
            partfxinputr[routeto][i] += partfxinputr[nefx][i];
        }
    }
    memcpy(partoutl, partfxinputl[NUM_PART_EFX], synth->bufferbytes);
    memcpy(partoutr, partfxinputr[NUM_PART_EFX], synth->bufferbytes);

    // Kill All Notes if killallnotes true
    if (killallnotes)
    {
        for (int i = 0; i < synth->buffersize; ++i)
        {
            float tmp = (synth->buffersize - i) / synth->buffersize_f;
            partoutl[i] *= tmp;
            partoutr[i] *= tmp;
        }
        memset(tmpoutl, 0, synth->bufferbytes);
        memset(tmpoutr, 0, synth->bufferbytes);

        for (int k = 0; k < POLIPHONY; ++k)
            KillNotePos(k);
        killallnotes = 0;
        for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
            partefx[nefx]->cleanup();
    }
    ctl->updateportamento();
}


// Parameter control
void Part::setVolume(char value)
{
    Pvolume = value;
    volume  = dB2rap((Pvolume - 96.0f) / 96.0f * 40.0f) * ctl->expression.relvolume;
}


void Part::setPan(char value)
{
    Ppanning = value;
    float t = ((Ppanning > 0) ? (float)(Ppanning - 1) : 0.0f) / 126.0f;
    pangainL = cosf(t * PI / 2.0f);
    pangainR = cosf((1.0f - t) * PI / 2.0f);
}


// Enable or disable a kit item
void Part::setkititemstatus(int kititem, int Penabled_)
{
    if (kititem == 0 && kititem >= NUM_KIT_ITEMS)
        return; // nonexistent kit item and the first kit item is always enabled
    kit[kititem].Penabled = Penabled_;

    bool resetallnotes = false;
    if (!Penabled_)
    {
        kit[kititem].Pname.clear();
        if (kit[kititem].adpars)
        {
            delete kit[kititem].adpars;
            kit[kititem].adpars = NULL;
        }
        if (kit[kititem].subpars)
        {
            delete kit[kititem].subpars;
            kit[kititem].subpars = NULL;
        }
        if (kit[kititem].padpars)
        {
            delete kit[kititem].padpars;
            kit[kititem].padpars = NULL;
            resetallnotes = true;
        }
    }
    else
    {
        if (!kit[kititem].adpars)
            kit[kititem].adpars = new ADnoteParameters(microtonal, fft);
        if (!kit[kititem].subpars)
            kit[kititem].subpars = new SUBnoteParameters();
        if (!kit[kititem].padpars)
            kit[kititem].padpars = new PADnoteParameters(fft);
    }

    if (resetallnotes)
        for (int k = 0; k < POLIPHONY; ++k)
            KillNotePos(k);
}


void Part::add2XMLinstrument(XMLwrapper *xml)
{
    xml->beginbranch("INFO");
    xml->addparstr("name", Pname);
    xml->addparstr("author", info.Pauthor);
    xml->addparstr("comments", info.Pcomments);
    xml->addpar("type",info.Ptype);
    xml->endbranch();


    xml->beginbranch("INSTRUMENT_KIT");
    xml->addpar("kit_mode", Pkitmode);
    xml->addparbool("drum_mode", Pdrummode);

    for (int i = 0; i < NUM_KIT_ITEMS; ++i)
    {
        xml->beginbranch("INSTRUMENT_KIT_ITEM",i);
        xml->addparbool("enabled", kit[i].Penabled);
        if (kit[i].Penabled)
        {
            xml->addparstr("name", kit[i].Pname.c_str());

            xml->addparbool("muted", kit[i].Pmuted);
            xml->addpar("min_key", kit[i].Pminkey);
            xml->addpar("max_key", kit[i].Pmaxkey);

            xml->addpar("send_to_instrument_effect", kit[i].Psendtoparteffect);

            xml->addparbool("add_enabled", kit[i].Padenabled);
            if (kit[i].Padenabled && kit[i].adpars)
            {
                xml->beginbranch("ADD_SYNTH_PARAMETERS");
                kit[i].adpars->add2XML(xml);
                xml->endbranch();
            }

            xml->addparbool("sub_enabled", kit[i].Psubenabled);
            if (kit[i].Psubenabled && kit[i].subpars)
            {
                xml->beginbranch("SUB_SYNTH_PARAMETERS");
                kit[i].subpars->add2XML(xml);
                xml->endbranch();
            }

            xml->addparbool("pad_enabled", kit[i].Ppadenabled);
            if (kit[i].Ppadenabled && kit[i].padpars)
            {
                xml->beginbranch("PAD_SYNTH_PARAMETERS");
                kit[i].padpars->add2XML(xml);
                xml->endbranch();
            }
        }
        xml->endbranch();
    }
    xml->endbranch();

    xml->beginbranch("INSTRUMENT_EFFECTS");
    for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
    {
        xml->beginbranch("INSTRUMENT_EFFECT",nefx);
        xml->beginbranch("EFFECT");
        partefx[nefx]->add2XML(xml);
        xml->endbranch();

        xml->addpar("route", Pefxroute[nefx]);
        partefx[nefx]->setdryonly(Pefxroute[nefx] == 2);
        xml->addparbool("bypass",Pefxbypass[nefx]);
        xml->endbranch();
    }
    xml->endbranch();
}


void Part::add2XML(XMLwrapper *xml)
{
    // parameters
    xml->addparbool("enabled", Penabled);
    if (!Penabled && xml->minimal)
        return;

    xml->addpar("volume", Pvolume);
    xml->addpar("panning", Ppanning);

    xml->addpar("min_key", Pminkey);
    xml->addpar("max_key", Pmaxkey);
    xml->addpar("key_shift", Pkeyshift);
    xml->addpar("rcv_chn", Prcvchn);

    xml->addpar("velocity_sensing", Pvelsns);
    xml->addpar("velocity_offset", Pveloffs);

    xml->addparbool("note_on", Pnoteon);
    xml->addparbool("poly_mode", Ppolymode);
    xml->addpar("legato_mode", Plegatomode);
    xml->addpar("key_limit", Pkeylimit);

    xml->beginbranch("INSTRUMENT");
    add2XMLinstrument(xml);
    xml->endbranch();

    xml->beginbranch("CONTROLLER");
    ctl->add2XML(xml);
    xml->endbranch();
}


bool Part::saveXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper();
    if (!xml)
    {
        Runtime.Log("Error, Part::saveXML failed to instantiate new XMLwrapper");
        return false;
    }
    xml->beginbranch("INSTRUMENT");
    add2XMLinstrument(xml);
    xml->endbranch();
    bool result = xml->saveXMLfile(filename);
    delete xml;
    return result;
}


bool Part::loadXMLinstrument(string filename)
{
    XMLwrapper *xml = new XMLwrapper();
    if (!xml)
    {
        Runtime.Log("Error, Part failed to instantiate new XMLwrapper");
        return false;
    }

    if (!xml->loadXMLfile(filename))
    {
        Runtime.Log("Error, Part failed to load instrument file " + filename);
        delete xml;
        return false;
    }
    if (xml->enterbranch("INSTRUMENT") == 0)
    {
        Runtime.Log(filename + " is not an instrument file");
        return false;
    }
    Mute();
    defaultsinstrument();
    getfromXMLinstrument(xml);
    applyparameters();
    Unmute();
    xml->exitbranch();
    delete xml;
    return true;
}


void Part::applyparameters(void)
{
    for (int n = 0; n < NUM_KIT_ITEMS; ++n)
        if (kit[n].Ppadenabled && kit[n].padpars != NULL)
            kit[n].padpars->applyparameters(true);
}


void Part::getfromXMLinstrument(XMLwrapper *xml)
{
    if (xml->enterbranch("INFO"))
    {
        Pname = xml->getparstr("name");
        info.Pauthor = xml->getparstr("author");
        info.Pcomments = xml->getparstr("comments");
        info.Ptype = xml->getpar("type", info.Ptype, 0, 16);
        xml->exitbranch();
    }

    if (xml->enterbranch("INSTRUMENT_KIT"))
    {
        Pkitmode = xml->getpar127("kit_mode", Pkitmode);
        Pdrummode = xml->getparbool("drum_mode", Pdrummode);
        setkititemstatus(0, 0);
        for (int i = 0; i < NUM_KIT_ITEMS; ++i)
        {
            if (!xml->enterbranch("INSTRUMENT_KIT_ITEM", i))
                continue;
            setkititemstatus(i, xml->getparbool("enabled", kit[i].Penabled));
            if (!kit[i].Penabled)
            {
                xml->exitbranch();
                continue;
            }
            kit[i].Pname = xml->getparstr("name");
            kit[i].Pmuted = xml->getparbool("muted", kit[i].Pmuted);
            kit[i].Pminkey = xml->getpar127("min_key", kit[i].Pminkey);
            kit[i].Pmaxkey = xml->getpar127("max_key", kit[i].Pmaxkey);
            kit[i].Psendtoparteffect = xml->getpar127("send_to_instrument_effect",
                                                      kit[i].Psendtoparteffect);
            kit[i].Padenabled = xml->getparbool("add_enabled", kit[i].Padenabled);
            if (xml->enterbranch("ADD_SYNTH_PARAMETERS"))
            {
                kit[i].adpars->getfromXML(xml);
                xml->exitbranch();
            }
            kit[i].Psubenabled = xml->getparbool("sub_enabled", kit[i].Psubenabled);
            if (xml->enterbranch("SUB_SYNTH_PARAMETERS"))
            {
                kit[i].subpars->getfromXML(xml);
                xml->exitbranch();
            }
            kit[i].Ppadenabled = xml->getparbool("pad_enabled", kit[i].Ppadenabled);
            if (xml->enterbranch("PAD_SYNTH_PARAMETERS"))
            {
                kit[i].padpars->getfromXML(xml);
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }
    if (xml->enterbranch("INSTRUMENT_EFFECTS"))
    {
        for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
        {
            if (!xml->enterbranch("INSTRUMENT_EFFECT", nefx))
                continue;
            if (xml->enterbranch("EFFECT"))
            {
                partefx[nefx]->getfromXML(xml);
                xml->exitbranch();
            }
            Pefxroute[nefx] = xml->getpar("route", Pefxroute[nefx], 0, NUM_PART_EFX);
            partefx[nefx]->setdryonly(Pefxroute[nefx] == 2);
            Pefxbypass[nefx] = xml->getparbool("bypass", Pefxbypass[nefx]);
            xml->exitbranch();
        }
        xml->exitbranch();
    }
}


void Part::getfromXML(XMLwrapper *xml)
{
    Penabled = xml->getparbool("enabled", Penabled);

    setVolume(xml->getpar127("volume", Pvolume));
    setPan(xml->getpar127("panning", Ppanning));

    Pminkey = xml->getpar127("min_key", Pminkey);
    Pmaxkey = xml->getpar127("max_key", Pmaxkey);
    Pkeyshift = xml->getpar127("key_shift", Pkeyshift);
    Prcvchn = xml->getpar127("rcv_chn", Prcvchn);

    Pvelsns = xml->getpar127("velocity_sensing", Pvelsns);
    Pveloffs = xml->getpar127("velocity_offset", Pveloffs);

    Pnoteon = xml->getparbool("note_on", Pnoteon);
    Ppolymode = xml->getparbool("poly_mode", Ppolymode);
    Plegatomode = xml->getparbool("legato_mode", Plegatomode); // older versions
    if (!Plegatomode)
        Plegatomode = xml->getpar127("legato_mode", Plegatomode);
    Pkeylimit = xml->getpar127("key_limit", Pkeylimit);
    if (xml->enterbranch("INSTRUMENT"))
    {
        getfromXMLinstrument(xml);
        xml->exitbranch();
        applyparameters();
    }
    if (xml->enterbranch("CONTROLLER"))
    {
        ctl->getfromXML(xml);
        xml->exitbranch();
    }
}
