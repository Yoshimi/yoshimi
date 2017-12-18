/*
    Part.cpp - Part implementation

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, James Morris
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2017, Will Godfrey

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

    Modified December 2017
*/

#include <cstring>
#include <cmath>
#include <iostream>

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
#include "Misc/Part.h"

Part::Part(Microtonal *microtonal_, FFTwrapper *fft_, SynthEngine *_synth) :
    microtonal(microtonal_),
    fft(fft_),
    killallnotes(false),
    synth(_synth)
{
    ctl = new Controller(synth);
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

    kit[0].adpars = new ADnoteParameters(fft, synth);
    kit[0].subpars = new SUBnoteParameters(synth);
    kit[0].padpars = new PADnoteParameters(fft, synth);

    // Part's Insertion Effects init
    for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
        partefx[nefx] = new EffectMgr(1, synth);

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
    Pkeymode = 0; // poly
    setVolume(96);
    TransVolume = 128; // ensure it always gets set
    Pkeyshift = 64;
    PmapOffset = 0;
    Prcvchn = 0;
    setPan(Ppanning = 64);
    TransPanning = 128; // ensure it always gets set
    Pvelsns = 64;
    Pveloffs = 64;
    Pkeylimit = 20;
    Pfrand = 0;
    PbreathControl = 2;
    legatoFading = 0;
    setDestination(1);
    defaultsinstrument();
    ctl->resetall();
    setNoteMap(0);
}

void Part::setNoteMap(int keyshift)
{
    for (int i = 0; i < 128; ++i)
        if (Pdrummode)
            PnoteMap[128 - PmapOffset + i] = microtonal->getFixedNoteFreq(i);
        else
            PnoteMap[128 - PmapOffset + i] = microtonal->getNoteFreq(i, keyshift + synth->Pkeyshift - 64);
}


void Part::defaultsinstrument(void)
{
    Pname = "Simple Sound";
    PyoshiType = 0;
    info.Ptype = 0;
    info.Pauthor.clear();
    info.Pcomments.clear();

    Pkitmode = 0;
    Pkitfade = false;
    Pdrummode = 0;
    Pfrand = 0;

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
    int enablepart = Penabled;
    Penabled = 0;
    for (int k = 0; k < POLIPHONY; ++k)
        KillNotePos(k);
    memset(partoutl, 0, synth->bufferbytes);
    memset(partoutr, 0, synth->bufferbytes);
    memset(tmpoutl, 0, synth->bufferbytes);
    memset(tmpoutr, 0, synth->bufferbytes);

    for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
        partefx[nefx]->cleanup();
    for (int n = 0; n < NUM_PART_EFX + 1; ++n)
    {
        memset(partfxinputl[n], 0, synth->bufferbytes);
        memset(partfxinputr[n], 0, synth->bufferbytes);

    }
    Penabled = enablepart;
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
void Part::NoteOn(int note, int velocity, bool renote)
{
    if (note < Pminkey || note > Pmaxkey)
        return;
    /*
     * In legato mode we only ever hear the newest
     * note played, so it is acceptable to lose
     * intemediate ones while going through a
     * legato fade between held and newest note.
     */
    if (Pkeymode > 1 && legatoFading > 0)
        return;
    // Legato and MonoMem used vars:
    int posb = POLIPHONY - 1;     // Just a dummy initial value.
    bool legatomodevalid = false; // true when legato mode is determined applicable.
    bool doinglegato = false;     // true when we determined we do a legato note.
    bool ismonofirstnote = false; // (In Mono/Legato) true when we determined
                                  // no other notes are held down or sustained.*/
    int lastnotecopy = lastnote;  // Useful after lastnote has been changed.

    // MonoMem stuff:
    if (Pkeymode > 0) // if Poly is off
    {
        if (!renote)
            monomemnotes.push_back(note);        // Add note to the list.
        monomem[note].velocity = velocity;       // Store this note's velocity.
        if (partnote[lastpos].status != KEY_PLAYING
            && partnote[lastpos].status != KEY_RELASED_AND_SUSTAINED)
        {
            ismonofirstnote = true; // No other keys are held or sustained.
        }
    }
    else // Poly mode is On, so just make sure the list is empty.
    {
        if (!monomemnotes.empty())
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
    if (Pkeymode > 1 && !Pdrummode)
    {
        // Legato mode is on and applicable.
        legatomodevalid = true;
        if (!ismonofirstnote && lastlegatomodevalid)
        {
            // At least one other key is held or sustained, and the
            // previous note was played while in valid legato mode.
            doinglegato = true; // So we'll do a legato note.
            pos = lastpos;      // A legato note uses same pos as previous..
            posb = lastposb;    // .. same goes for posb.
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
    else
    {
        // Legato mode is either off or non-applicable.
        if ((Pkeymode & 3) == 1)
        {   // if the mode is 'mono' turn off all other notes
            for (int i = 0; i < POLIPHONY; ++i)
            {
                if (partnote[i].status == KEY_PLAYING)
                    RelaseNotePos(i);
            }
            RelaseSustainedKeys();
        }
    }
    lastlegatomodevalid = legatomodevalid;

    if (pos == -1)
    {
        // test
        synth->getRuntime().Log("Too many notes - notes > poliphony");
    }
    else
    {
        // start the note
        partnote[pos].status = KEY_PLAYING;
        partnote[pos].note = note;
        if (legatomodevalid)
        {
            partnote[posb].status = KEY_PLAYING;
            partnote[posb].note = note;
        }

        // compute the velocity offset
        float vel = velF(velocity / 127.0f, Pvelsns) + (Pveloffs - 64.0f) / 64.0f;
        vel = (vel < 0.0f) ? 0.0f : vel;
        vel = (vel > 1.0f) ? 1.0f : vel;

        // initialise note frequency
        float notebasefreq;
        if ((notebasefreq = PnoteMap[PmapOffset + note]) < 0.0f)
            return; // the key is not mapped

        // Humanise
        // cout << "\n" << notebasefreq << endl;
        if (!Pdrummode && Pfrand >= 1) // otherwise 'off'
            // this is an approximation to keep the math simple and is
            // about 1 cent out at 50 cents
            notebasefreq *= (1.0f + ((synth->numRandom() - 0.5f) * Pfrand * 0.00115f));
        // cout << notebasefreq << endl;

        // Portamento
        if (oldfreq < 1.0f)
            oldfreq = notebasefreq; // this is only the first note is played

        // For Mono/Legato: Force Portamento Off on first
        // notes. That means it is required that the previous note is
        // still held down or sustained for the Portamento to activate
        // (that's like Legato).
        int portamento = 0;
        if (Pkeymode == 0 || !ismonofirstnote)
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
        legatoFading = 0; // just to be sure
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
                        legatoFading |= 1;
                }

                if ((kit[0].Psubenabled)
                    && (partnote[pos].kititem[0].subnote)
                    && (partnote[posb].kititem[0].subnote))
                {
                    partnote[pos].kititem[0].subnote->
                        SUBlegatonote(notebasefreq, vel, portamento, note, true);
                    partnote[posb].kititem[0].subnote->
                        SUBlegatonote(notebasefreq, vel, portamento, note, true);
                    legatoFading |= 2;
                }

                if ((kit[0].Ppadenabled)
                    && (partnote[pos].kititem[0].padnote)
                    && (partnote[posb].kititem[0].padnote))
                {
                    partnote[pos].kititem[0].padnote->
                        PADlegatonote(notebasefreq, vel, portamento, note, true);
                    partnote[posb].kititem[0].padnote->
                        PADlegatonote(notebasefreq, vel, portamento, note, true);
                    legatoFading |= 4;
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
                        legatoFading |= 1;
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
                        legatoFading |= 2;
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
                        legatoFading |= 4;
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
                    new ADnote(kit[0].adpars, ctl, notebasefreq, vel,
                                portamento, note, false, synth); // not silent
            if (kit[0].Psubenabled)
                partnote[pos].kititem[0].subnote =
                    new SUBnote(kit[0].subpars, ctl, notebasefreq, vel,
                                portamento, note, false, synth);
            if (kit[0].Ppadenabled)
                partnote[pos].kititem[0].padnote =
                    new PADnote(kit[0].padpars, ctl, notebasefreq, vel,
                                portamento, note, false, synth);
            if (kit[0].Padenabled || kit[0].Psubenabled || kit[0].Ppadenabled)
                partnote[pos].itemsplaying++;

            // Spawn another note (but silent) if legatomodevalid==true
            if (legatomodevalid)
            {
                partnote[posb].kititem[0].sendtoparteffect = 0;
                if (kit[0].Padenabled)
                    partnote[posb].kititem[0].adnote =
                        new ADnote(kit[0].adpars, ctl, notebasefreq, vel,
                                    portamento, note, true, synth); // silent
                if (kit[0].Psubenabled)
                    partnote[posb].kititem[0].subnote =
                        new SUBnote(kit[0].subpars, ctl, notebasefreq, vel,
                                    portamento, note, true, synth);
                if (kit[0].Ppadenabled)
                    partnote[posb].kititem[0].padnote =
                        new PADnote(kit[0].padpars, ctl, notebasefreq, vel,
                                    portamento, note, true, synth);
                if (kit[0].Padenabled || kit[0].Psubenabled || kit[0].Ppadenabled)
                    partnote[posb].itemsplaying++;
            }
        }
        else
        { // init the notes for the "kit mode"
            float truevel = vel; // we need this as cross fade modifies the value
            for (int item = 0; item < NUM_KIT_ITEMS; ++item)
            {
                if (kit[item].Pmuted)
                    continue;
                if (note < kit[item].Pminkey || note>kit[item].Pmaxkey)
                    continue;


                // cross fade on multi
                if (Pkitfade)
                {
                    vel = truevel; // always start with correct value
                    int range = 0;
                    int position;
                    if ((item & 1) == 0 && kit[item + 1].Penabled) // crossfade lower item of pair
                    {
                        if (kit[item].Pmaxkey > kit[item + 1].Pminkey && kit[item].Pmaxkey < kit[item + 1].Pmaxkey)
                        {
                            if (note >= kit[item + 1].Pminkey)
                            {
                                range = kit[item].Pmaxkey - kit[item + 1].Pminkey;
                                position = kit[item].Pmaxkey - note;
                            }
                        }
                        else if (kit[item + 1].Pmaxkey > kit[item].Pminkey && kit[item + 1].Pmaxkey < kit[item].Pmaxkey ) // eliminate equal state
                        {
                            if (note <= kit[item + 1].Pmaxkey)
                            {
                                range = kit[item + 1].Pmaxkey - kit[item].Pminkey;
                                position = (note - kit[item].Pminkey);
                            }
                        }
                    }
                    else if ((item & 1) == 1 && kit[item - 1].Penabled) // crossfade upper item of pair
                    {

                        if (kit[item - 1].Pmaxkey > kit[item ].Pminkey && kit[item - 1].Pmaxkey < kit[item ].Pmaxkey)
                        {
                            if (note <= kit[item - 1].Pmaxkey)
                            {
                                range = kit[item - 1].Pmaxkey - kit[item].Pminkey;
                                position = (note - kit[item].Pminkey);
                            }
                        }
                        else if (kit[item].Pmaxkey > kit[item - 1].Pminkey && kit[item].Pmaxkey < kit[item - 1].Pmaxkey) // eliminate equal state
                        {
                            if (note >= kit[item - 1].Pminkey)
                            {
                                range = kit[item].Pmaxkey - kit[item - 1].Pminkey;
                                position = kit[item].Pmaxkey - note;
                            }
                        }
                    }
                    if (range)
                    {
                        vel = truevel * (float(position) / float(range));
                        //cout << item << "  " << vel << endl;
                    }
                }
                // end of cross fade


                int ci = partnote[pos].itemsplaying; // ci=current item

                partnote[pos].kititem[ci].sendtoparteffect =
                    (kit[item].Psendtoparteffect < NUM_PART_EFX)
                        ? kit[item].Psendtoparteffect
                        : NUM_PART_EFX; // if this parameter is 127 for "unprocessed"

                if (kit[item].adpars && kit[item].Padenabled)
                {
                    partnote[pos].kititem[ci].adnote =
                        new ADnote(kit[item].adpars, ctl, notebasefreq, vel,
                                    portamento, note, false, synth); // not silent
                }
                if (kit[item].subpars && kit[item].Psubenabled)
                    partnote[pos].kititem[ci].subnote =
                        new SUBnote(kit[item].subpars, ctl, notebasefreq, vel,
                                    portamento, note, false, synth);

                if (kit[item].padpars && kit[item].Ppadenabled)
                    partnote[pos].kititem[ci].padnote =
                        new PADnote(kit[item].padpars, ctl, notebasefreq, vel,
                                    portamento, note, false, synth);

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
                            new ADnote(kit[item].adpars, ctl, notebasefreq,
                                        vel, portamento, note, true, synth); // silent
                    }
                    if (kit[item].subpars && kit[item].Psubenabled)
                        partnote[posb].kititem[ci].subnote =
                            new SUBnote(kit[item].subpars, ctl, notebasefreq,
                                        vel, portamento, note, true, synth);
                    if (kit[item].padpars && kit[item].Ppadenabled)
                        partnote[posb].kititem[ci].padnote =
                            new PADnote(kit[item].padpars, ctl, notebasefreq,
                                        vel, portamento, note, true, synth);

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

    // this only release the keys if there is maximum number of keys allowed
    setkeylimit(Pkeylimit);
}


// Note Off Messages
void Part::NoteOff(int note) //relase the key
{
    int i;
    // This note is released, so we remove it from the list.
    if (!monomemnotes.empty())
        monomemnotes.remove(note);

    for ( i = POLIPHONY - 1; i >= 0; i--)
    {   //first note in, is first out if there are same note multiple times
        if (partnote[i].status == KEY_PLAYING && partnote[i].note == note)
        {
            if (!ctl->sustain.sustain)
            {   //the sustain pedal is not pushed
                if (Pkeymode > 0  && !Pdrummode && !monomemnotes.empty())
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
            par = 64 + (par - 64) * (ctl->panning.depth / 64.0); // force float during calculation
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
            if (ctl->volume.receive)
                setVolume(par * ctl->volume.volume);
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
            setVolume(Pvolume);
            setPan(Ppanning);
            Pkeymode &= 3; // clear temporary legato mode
            legatoFading = 0;

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
    if ((Pkeymode < 1 || Pkeymode > 2)&& (!monomemnotes.empty()))
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
    NoteOn(mmrtempnote, monomem[mmrtempnote].velocity, true);
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
            delete partnote[pos].kititem[j].adnote;
            partnote[pos].kititem[j].adnote = NULL;
        }
        if (partnote[pos].kititem[j].subnote)
        {
            delete partnote[pos].kititem[j].subnote;
            partnote[pos].kititem[j].subnote = NULL;
        }
        if (partnote[pos].kititem[j].padnote)
        {
            delete partnote[pos].kititem[j].padnote;
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

    // release old keys if the number of notes>keylimit
    if (Pkeymode == 0)
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
    int k;
    int noteplay; // 0 if there is nothing activated
    for (int nefx = 0; nefx < NUM_PART_EFX + 1; ++nefx)
    {
        memset(partfxinputl[nefx], 0, synth->p_bufferbytes);
        memset(partfxinputr[nefx], 0, synth->p_bufferbytes);
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
                    memset(tmpoutl, 0, synth->p_bufferbytes);
                    memset(tmpoutr, 0, synth->p_bufferbytes);
                }
                if (adnote->finished())
                {
                    delete partnote[k].kititem[item].adnote;
                    partnote[k].kititem[item].adnote = NULL;
                }
                for (int i = 0; i < synth->p_buffersize; ++i)
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
                    memset(tmpoutl, 0, synth->p_bufferbytes);
                    memset(tmpoutr, 0, synth->p_bufferbytes);
                }
                for (int i = 0; i < synth->p_buffersize; ++i)
                {   // add the SUBnote to part(mix)
                    partfxinputl[sendcurrenttofx][i] += tmpoutl[i];
                    partfxinputr[sendcurrenttofx][i] += tmpoutr[i];
                }
                if (subnote->finished())
                {
                    delete partnote[k].kititem[item].subnote;
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
                    memset(tmpoutl, 0, synth->p_bufferbytes);
                    memset(tmpoutr, 0, synth->p_bufferbytes);
                }
                if (padnote->finished())
                {
                    delete partnote[k].kititem[item].padnote;
                    partnote[k].kititem[item].padnote = NULL;
                }
                for (int i = 0 ; i < synth->p_buffersize; ++i)
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

    for (int item = 0; item < NUM_KIT_ITEMS; ++item)
    {
        if (kit[item].adpars)
            kit[item].adpars->postrender();
        if (kit[item].subpars)
            kit[item].subpars->postrender();
        if (kit[item].padpars)
            kit[item].padpars->postrender();
    }
    // Apply part's effects and mix them
    for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
    {
        if (!Pefxbypass[nefx])
        {
            partefx[nefx]->out(partfxinputl[nefx], partfxinputr[nefx]);
            if (Pefxroute[nefx] == 2)
            {
                for (int i = 0; i < synth->p_buffersize; ++i)
                {
                    partfxinputl[nefx + 1][i] += partefx[nefx]->efxoutl[i];
                    partfxinputr[nefx + 1][i] += partefx[nefx]->efxoutr[i];
                }
            }
        }
        int routeto = (Pefxroute[nefx] == 0) ? nefx + 1 : NUM_PART_EFX;
        for (int i = 0; i < synth->p_buffersize; ++i)
        {
            partfxinputl[routeto][i] += partfxinputl[nefx][i];
            partfxinputr[routeto][i] += partfxinputr[nefx][i];
        }
    }
    memcpy(partoutl, partfxinputl[NUM_PART_EFX], synth->p_bufferbytes);
    memcpy(partoutr, partfxinputr[NUM_PART_EFX], synth->p_bufferbytes);

    // Kill All Notes if killallnotes true
    if (killallnotes)
    {
        for (int i = 0; i < synth->p_buffersize; ++i)
        {
            float tmp = (synth->p_buffersize - i) / synth->p_buffersize_f;
            partoutl[i] *= tmp;
            partoutr[i] *= tmp;
        }
        memset(tmpoutl, 0, synth->p_bufferbytes);
        memset(tmpoutr, 0, synth->p_bufferbytes);

        for (int k = 0; k < POLIPHONY; ++k)
            KillNotePos(k);
        killallnotes = 0;
        for (int nefx = 0; nefx < NUM_PART_EFX; ++nefx)
            partefx[nefx]->cleanup();
    }
    ctl->updateportamento();
}


// Parameter control
void Part::setVolume(float value)
{
    Pvolume = value;
}


void Part::checkVolume(float step)
{
    TransVolume += step;
    volume = dB2rap((TransVolume - 96.0f) / 96.0f * 40.0f);
}


void Part::setDestination(int value)
{
    Paudiodest = value;
}


void Part::setPan(float value)
{
    Ppanning = value;
}


void Part::checkPanning(float step)
{
    float t;
    TransPanning += step;
    t = ((TransPanning > 0) ? (TransPanning - 1) : 0.0f) / 126.0f;
    pangainL = cosf(t * HALFPI);
    pangainR = cosf((1.0f - t) * HALFPI);
}


// Enable or disable a kit item
void Part::setkititemstatus(int kititem, int Penabled_)
{
    if (kititem == 0 || kititem >= NUM_KIT_ITEMS)
        return; // nonexistent kit item and the first kit item is always enabled
    kit[kititem].Penabled = Penabled_;

    bool resetallnotes = false;
    if (!Penabled_)
    {
        kit[kititem].Pmuted = 0;
        kit[kititem].Padenabled = 0;
        kit[kititem].Psubenabled = 0;
        kit[kititem].Ppadenabled = 0;
        kit[kititem].Pname.clear();
        kit[kititem].Psendtoparteffect = 0;
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
            kit[kititem].adpars = new ADnoteParameters(fft, synth);
        if (!kit[kititem].subpars)
            kit[kititem].subpars = new SUBnoteParameters(synth);
        if (!kit[kititem].padpars)
            kit[kititem].padpars = new PADnoteParameters(fft, synth);
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
    if (Pname == "Simple Sound")
        return;


    xml->beginbranch("INSTRUMENT_KIT");
    xml->addpar("kit_mode", Pkitmode);
    xml->addparbool("kit_crossfade", Pkitfade);
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


void Part::add2XML(XMLwrapper *xml, bool subset)
{
    // parameters
    if (!subset)
    {
        xml->addparbool("enabled", (Penabled == 1));

        xml->addpar("volume", Pvolume);
        xml->addpar("panning", Ppanning);

        xml->addpar("min_key", Pminkey);
        xml->addpar("max_key", Pmaxkey);
        xml->addpar("key_shift", Pkeyshift);
        xml->addpar("rcv_chn", Prcvchn);

        xml->addpar("velocity_sensing", Pvelsns);
        xml->addpar("velocity_offset", Pveloffs);
    // the following two lines maintain backward compatibility
        xml->addparbool("poly_mode", (Pkeymode & 3) == 0);
        xml->addpar("legato_mode", (Pkeymode & 3) == 2);
        xml->addpar("key_limit", Pkeylimit);
        xml->addpar("random_detune", Pfrand);
        xml->addpar("destination", Paudiodest);
    }
    xml->beginbranch("INSTRUMENT");
    add2XMLinstrument(xml);
    if (subset)
    {
        xml->addpar("key_mode", Pkeymode & 3);
        xml->addpar("random_detune", Pfrand);
        xml->addparbool("breath_disable", PbreathControl != 2);
    }
    xml->endbranch();

    xml->beginbranch("CONTROLLER");
    ctl->add2XML(xml);
    xml->endbranch();
}


bool Part::saveXML(string filename, bool yoshiFormat)
{
    synth->getRuntime().xmlType = XML_INSTRUMENT;
    XMLwrapper *xml = new XMLwrapper(synth, yoshiFormat);
    if (!xml)
    {
        synth->getRuntime().Log("Part: saveXML failed to instantiate new XMLwrapper");
        return false;
    }
    if (Pname < "!") // this shouldn't be possible
        Pname = "No Title";

    if (yoshiFormat)
    {
        filename = setExtension(filename, "xiy");
        add2XML(xml, yoshiFormat);
    }
    else
    {
        filename = setExtension(filename, "xiz");
        xml->beginbranch("INSTRUMENT");
        add2XMLinstrument(xml);
        xml->endbranch();
    }
    bool result = xml->saveXMLfile(filename);
    delete xml;
    return result;
}


int Part::loadXMLinstrument(string filename)
{
    bool hasYoshi = true;
    filename = setExtension(filename, "xiy");
    if (!isRegFile(filename))
    {
        hasYoshi = false;
        filename = setExtension(filename, "xiz");
    }

    XMLwrapper *xml = new XMLwrapper(synth, hasYoshi);
    if (!xml)
    {
        synth->getRuntime().Log("Part: loadXML failed to instantiate new XMLwrapper");
        return 0;
    }
    if (!xml->loadXMLfile(filename))
    {
        synth->getRuntime().Log("Part: loadXML failed to load instrument file " + filename);
        delete xml;
        return 0;
    }
    if (xml->enterbranch("INSTRUMENT") == 0)
    {
        synth->getRuntime().Log(filename + " is not an instrument file");
        return 0;
    }
    defaultsinstrument();
    PyoshiType = xml->information.yoshiType;
    Pname = findleafname(filename); // in case there's no internal
    int chk = findSplitPoint(Pname);
    if (chk > 0)
        Pname = Pname.substr(chk + 1, Pname.size() - chk - 1);
    getfromXMLinstrument(xml);
    if (hasYoshi)
    {
        Pkeymode = xml->getpar("key_mode", Pkeymode, 0, 4);
        Pfrand = xml->getpar127("random_detune", Pfrand);
        if (Pfrand > 50)
            Pfrand = 50;
        PbreathControl = xml->getparbool("breath_disable", PbreathControl);
        if (PbreathControl)
            PbreathControl = 255; // impossible value
        else
            PbreathControl = 2;
    }
    applyparameters();
    xml->exitbranch();
    if (xml->enterbranch("CONTROLLER"))
    {
        ctl->getfromXML(xml);
        xml->exitbranch();
    }
    xml->exitbranch();
    delete xml;
    return 1;
}


void Part::applyparameters(void)
{
    for (int n = 0; n < NUM_KIT_ITEMS; ++n)
        if (kit[n].Ppadenabled && kit[n].padpars != NULL)
            kit[n].padpars->applyparameters(true);
}


void Part::getfromXMLinstrument(XMLwrapper *xml)
{
    string tempname;
    if (xml->enterbranch("INFO"))
    {
        tempname = xml->getparstr("name");
        //synth->getRuntime().Log("name <" + tempname + ">");
        if (tempname > "!")
            Pname = tempname;
        if (Pname <= "!" || Pname == "Simple Sound")
            Pname = "No Title";
        info.Pauthor = xml->getparstr("author");
        info.Pcomments = xml->getparstr("comments");
        info.Ptype = xml->getpar("type", info.Ptype, 0, 16);
        xml->exitbranch();
    }

    if (!xml->enterbranch("INSTRUMENT_KIT"))
    {
        defaultsinstrument();
        return;
    }
    else
    {
        Pkitmode = xml->getpar127("kit_mode", Pkitmode);
        Pkitfade = xml->getparbool("kit_crossfade", Pkitfade);
        Pdrummode = xml->getparbool("drum_mode", Pdrummode);

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
    Penabled = (xml->getparbool("enabled", Penabled) == 1);

    setVolume(xml->getpar127("volume", Pvolume));
    setPan(xml->getpar127("panning", Ppanning));

    Pminkey = xml->getpar127("min_key", Pminkey);
    Pmaxkey = xml->getpar127("max_key", Pmaxkey);
    Pkeyshift = xml->getpar("key_shift", Pkeyshift, MIN_KEY_SHIFT + 64, MAX_KEY_SHIFT + 64);

    Prcvchn = xml->getpar127("rcv_chn", Prcvchn);

    Pvelsns = xml->getpar127("velocity_sensing", Pvelsns);
    Pveloffs = xml->getpar127("velocity_offset", Pveloffs);

    bool Ppolymode = 1;
    bool Plegatomode = 0;
    Ppolymode = xml->getparbool("poly_mode", Ppolymode);
    Plegatomode = xml->getparbool("legato_mode", Plegatomode); // older versions
    if (!Plegatomode)
        Plegatomode = xml->getpar127("legato_mode", Plegatomode);
    if (Plegatomode) // these lines are for backward compatibility
        Pkeymode = 2;
    else if (Ppolymode)
        Pkeymode = 0;
    else
        Pkeymode = 1;

    Pkeylimit = xml->getpar127("key_limit", Pkeylimit);
    if (Pkeylimit < 1)
    {
        Pkeylimit = POLIPHONY - 20;
    }
    else if(Pkeylimit > (POLIPHONY - 20))
    {
        Pkeylimit = POLIPHONY - 20;
    }
    Pfrand = xml->getpar127("random_detune", Pfrand);
    if (Pfrand > 50)
        Pfrand = 50;
    setDestination(xml->getpar127("destination", Paudiodest));

    if (xml->enterbranch("INSTRUMENT"))
    {
        Pname = ""; // clear out any previous name
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


void Part::getLimits(CommandBlock *getData)
{
    unsigned int type = getData->data.type;
    int control = getData->data.control;
    int npart = getData->data.part;

    // defaults
    int min = 0;
    int def = 640;
    int max = 127;
    //cout << "part control " << to_string(control) << endl;
    if ((control >= 128 && control <= 168) || control == 224)
    {
        ctl->getLimits(getData);
        return;
    }

    switch (control)
    {
        case 0:
            type &= 0x3f;
            type |= 0x40;
            def = 960;
            break;

        case 1:
        case 4:
            type |= 0x40;
            break;

        case 2:
            type &= 0x3f;
            type |= 0x40;
            break;

        case 5:
            min = 1;
            def = 10;
            max = 16;
            break;

        case 6:
            def = 0;
            max = 2;
            break;

        case 7:
            type |= 0x40;
        case 57:
            def = 0;
            max = 1;
            break;

        case 8:
            if (npart == 0)
                def = 10;
            else
                def = 0;
            max = 1;
            break;

        case 9:
            def = 0;
            max = 1;
            break;

        case 16:
            def = 0;
            break;

        case 17:
            def = 1270;
            break;

        case 18:
        case 19:
        case 20:
        case 96:
            min = 0;
            def = 0;
            max = 0;
            break;

        case 33:
            def = 200;
            max = 60;
            break;

        case 35:
            min = -36;
            def = 0;
            max = 36;
            break;

        case 40:
        case 41:
        case 42:
        case 43:
            type |= 0x40;
            def = 0;
            break;

        case 48:
            def = 0;
            max = 50;
            break;

        case 58:
            def = 0;
            max = 3;
            break;
        case 120:
            min = 1;
            def = 10;
            max = 3;
            break;

        // the following are learnable MIDI controllers
        case 130:
            max = 64;
            type |= 0x40;
            break;

        case 131:
            def = 800;
            type |= 0x40;
            break;

        case 133:
            type |= 0x40;
            break;

        case 138:
            min = -6400;
            def = 0;
            max = 6400;
            type |= 0x40;
            break;

        case 139:
        case 140:
        case 144:
        case 145:
            type |= 0x40;
            break;

        case 160:
        case 161:
            type |= 0x40;
            break;

        case 162:
            def = 800;
            type |= 0x40;
            break;

        case 166:
            def = 900;
            type |= 0x40;
            break;

        case 192:
        case 197:
        case 198:
        case 199:
            type |= 0x40;
            break;

        case 194:
            type |= 0x40;
            def = 1270;
            break;

        // these haven't been done
        case 193:
            break;
        case 195:
            break;
        case 196:
            break;

        case 255: // number of parts!
            min = 16;
            def = 160;
            max = 64;
            break;

        default:
            min = -1;
            def = -10;
            max = -1;
            break;
    }
    getData->data.type = type;
    getData->limits.min = min;
    getData->limits.def = def;
    getData->limits.max = max;
}
