/*
    Microtonal.cpp - Tuning settings and microtonal capabilities

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2016 Will Godfrey

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

    This file is derivative of original ZynAddSubFX code, modified October 2016
*/

#include <cmath>
#include <iostream>

#include "Misc/Config.h"
#include "Misc/XMLwrapper.h"
#include "Misc/Microtonal.h"
#include "Misc/SynthEngine.h"

#define MAX_LINE_SIZE 80

void Microtonal::defaults(void)
{
    Pinvertupdown = 0;
    Pinvertupdowncenter = 60;
    octavesize = 12;
    Penabled = 0;
    PAnote = 69;
    PAfreq = 440.0f;
    Pscaleshift = 64;

    Pfirstkey = 0;
    Plastkey = 127;
    Pmiddlenote = 60;
    Pmapsize = 12;
    Pmappingenabled = 0;

    for (int i = 0; i < 128; ++i)
        Pmapping[i] = i;

    for (int i = 0; i < MAX_OCTAVE_SIZE; ++i)
    {
        octave[i].tuning = tmpoctave[i].tuning = powf(2.0f, (i % octavesize + 1) / 12.0f);
        octave[i].type = tmpoctave[i].type = 1;
        octave[i].x1 = tmpoctave[i].x1 = (i % octavesize + 1) * 100;
        octave[i].x2 = tmpoctave[i].x2 = 0;
    }
    octave[11].type = 2;
    octave[11].x1 = 2;
    octave[11].x2 = 1;
    Pname = string("12tET");
    Pcomment = string("Equal Temperament 12 notes per octave");
    Pglobalfinedetune = 64.0;
}

void Microtonal::setPartMaps(void)
{
    synth->setAllPartMaps();
}

// Get the frequency according to the note number
float Microtonal::getNoteFreq(int note, int keyshift)
{
    // in this function will appears many times things like this:
    // var=(a+b*100)%b
    // I had written this way because if I use var=a%b gives unwanted results when a<0
    // This is the same with divisions.

    if ((Pinvertupdown != 0) && ((Pmappingenabled == 0) || (Penabled == 0)))
        note = Pinvertupdowncenter * 2 - note;

    // compute global fine detune, -64.0 .. 63.0 cents
    float globalfinedetunerap =
        (Pglobalfinedetune > 64.0f || Pglobalfinedetune < 64.0f)
            ? powf(2.0f, (Pglobalfinedetune - 64.0f) / 1200.0f)
            : 1.0f;
    // was float globalfinedetunerap = powf(2.0f, (Pglobalfinedetune - 64.0f) / 1200.0f);

    if (!Penabled)
        return getFixedNoteFreq(note + keyshift) * globalfinedetunerap;


    int scaleshift = (Pscaleshift - 64 + octavesize * 100) % octavesize;

    // compute the keyshift
    float rap_keyshift = 1.0f;
    if (keyshift)
    {
        int kskey = (keyshift + octavesize * 100) % octavesize;
        int ksoct = (keyshift + octavesize * 100) / octavesize - 100;
        rap_keyshift  = (!kskey) ? 1.0f : (octave[kskey - 1].tuning);
        rap_keyshift *= powf(octave[octavesize - 1].tuning, ksoct);
    }

    // if the mapping is enabled
    if (Pmappingenabled)
    {
        if ((note < Pfirstkey) || (note > Plastkey))
            return -1.0f;
        // Compute how many mapped keys are from middle note to reference note
        // and find out the proportion between the freq. of middle note and "A" note
        int tmp = PAnote - Pmiddlenote, minus = 0;
        if (tmp < 0)
        {
            tmp   = -tmp;
            minus = 1;
        }
        int deltanote = 0;
        for (int i = 0; i < tmp; ++i)
            if (Pmapping[i % Pmapsize] >= 0)
                deltanote++;
        float rap_anote_middlenote =
            (deltanote == 0) ? (1.0f) : (octave[(deltanote - 1) % octavesize].tuning);
        if(deltanote != 0)
            rap_anote_middlenote *= powf(octave[octavesize - 1].tuning,
                                         (deltanote - 1) / octavesize);
        if (minus)
            rap_anote_middlenote = 1.0f / rap_anote_middlenote;

        // Convert from note (midi) to degree (note from the tunning)
        int degoct = (note - Pmiddlenote + Pmapsize * 200)
                      / Pmapsize - 200;
        int degkey = (note - Pmiddlenote + Pmapsize * 100) % Pmapsize;
        degkey = Pmapping[degkey];
        if (degkey < 0) // this key is not mapped
            return -1.0f;

        // invert the keyboard upside-down if it is asked for
        // TODO: do the right way by using Pinvertupdowncenter
        if (Pinvertupdown)
        {
            degkey = octavesize - degkey - 1;
            degoct = -degoct;
        }
        // compute the frequency of the note
        degkey  = degkey + scaleshift;
        degoct += degkey / octavesize;
        degkey %= octavesize;

        float freq = (degkey == 0) ? (1.0f) : octave[degkey - 1].tuning;
        freq *= powf(octave[octavesize - 1].tuning, degoct);
        freq *= PAfreq / rap_anote_middlenote;
        freq *= globalfinedetunerap;
        if(scaleshift != 0)
            freq /= octave[scaleshift - 1].tuning;
        return freq * rap_keyshift;
    }
    else // if the mapping is disabled
    {
        int nt = note - PAnote + scaleshift;
        int ntkey = (nt + octavesize * 100) % octavesize;
        int ntoct = (nt - ntkey) / octavesize;

        float oct  = octave[octavesize - 1].tuning;
        float freq = octave[(ntkey + octavesize - 1) % octavesize].tuning
                     * powf(oct, ntoct) * PAfreq;
        if (ntkey == 0)
            freq /= oct;
        if (scaleshift != 0)
            freq /= octave[scaleshift - 1].tuning;
//	fprintf(stderr,"note=%d freq=%.3f cents=%d\n",note,freq,(int)floor(log(freq/PAfreq)/log(2.0)*1200.0+0.5));
        freq *= globalfinedetunerap;
        return freq * rap_keyshift;
    }
}

// Convert a line to tunings; returns -1 if it ok
int Microtonal::linetotunings(unsigned int nline, const char *line)
{
    int x1 = -1, x2 = -1, type = -1;
    float x = -1.0f, tmp, tuning = 1.0f;

    if (strstr(line, "/") == NULL)
    {
        if (strstr(line, ".") == NULL)
        {   // M case (M=M/1)
            sscanf(line, "%d", &x1);
            x2 = 1;
            type = 2; // division
        }
        else
        {   // float number case
            sscanf(line, "%f", &x);
            if (x < 0.000001f)
                return 1;
            type = 1; // float type(cents)
        }
    }
    else
    {   // M/N case
        sscanf(line, "%d/%d", &x1, &x2);
        if (x1 < 0 || x2 < 0)
            return 1;
        if (!x2)
            x2 = 1;
        type = 2; // division
    }

    if (x1 <= 0)
        x1 = 1; // not allow zero frequency sounds (consider 0 as 1)

    // convert to float if the number are too big
    if ((type==2) && ((x1 > (128 * 128 * 128 - 1)) || (x2 > (128 * 128* 128 - 1))))
    {
        type = 1;
        x = ((float)x1) / x2;
    }
    switch (type)
    {
        case 1:
            x1 = (int) floorf(x);
            tmp = fmodf(x, 1.0f);
            x2 = (int)truncf(floorf(tmp * 1e6f));
            tuning = powf(2.0f, x / 1200.0f);
            break;
        case 2:
            x = ((float)x1) / x2;
            tuning = x;
            break;
    }

    tmpoctave[nline].tuning = tuning;
    tmpoctave[nline].type = type;
    tmpoctave[nline].x1 = x1;
    tmpoctave[nline].x2 = x2;

    return -1; // ok
}


// Convert the text to tunnings
int Microtonal::texttotunings(const char *text)
{
    int i;
    unsigned int k = 0, nl = 0;
    char *lin;

    lin = new char[MAX_LINE_SIZE + 1];
    while (k < strlen(text))
    {
        for (i = 0; i < MAX_LINE_SIZE; ++i)
        {
            lin[i] = text[k++];
            if (lin[i] < 0x20)
                break;
        }
        lin[i] = '\0';
        if (!strlen(lin))
            continue;
        int err = linetotunings(nl, lin);
        if (err != -1)
        {
            delete [] lin;
            return nl; // Parse error
        }
        nl++;
    }
    delete [] lin;
    if (nl > MAX_OCTAVE_SIZE)
        nl = MAX_OCTAVE_SIZE;
    if (!nl)
        return -2; // the input is empty
    octavesize = nl;
    for (i = 0; i < octavesize; ++i)
    {
        octave[i].tuning = tmpoctave[i].tuning;
        octave[i].type = tmpoctave[i].type;
        octave[i].x1 = tmpoctave[i].x1;
        octave[i].x2 = tmpoctave[i].x2;
    }
    return -1; // ok
}


// Convert the text to mapping
void Microtonal::texttomapping(const char *text)
{
    unsigned int i, k = 0;
    char *lin;

    lin = new char[MAX_LINE_SIZE + 1];
    for (i = 0; i < 128; ++i)
        Pmapping[i] = -1;
    int tx = 0;
    while (k < strlen(text))
    {
        for (i = 0; i < MAX_LINE_SIZE; ++i)
        {
            lin[i] = text[k++];
            if (lin[i] < 0x20)
                break;
        }
        lin[i] = '\0';
        if (!strlen(lin))
            continue;

        int tmp = 0;
        if (!sscanf(lin, "%d", &tmp))
            tmp = -1;
        if (tmp < -1)
            tmp = -1;
        Pmapping[tx] = tmp;

        if ((tx++) > 127)
            break;
    }
    delete [] lin;

    if (!tx)
        tx = 1;
    Pmapsize = tx;
}


// Convert tunning to text line
void Microtonal::tuningtoline(int n, char *line, int maxn)
{
    if (n > octavesize || n > MAX_OCTAVE_SIZE)
    {
        line[0] = '\0';
        return;
    }
    if (octave[n].type == 1)
        snprintf(line, maxn, "%04d.%06d", octave[n].x1,octave[n].x2);
    if (octave[n].type == 2)
        snprintf(line, maxn, "%d/%d", octave[n].x1, octave[n].x2);
}


int Microtonal::loadline(FILE *file, char *line)
{
    do {
        if (!fgets(line, 500, file))
            return 1;
    } while (line[0] == '!');
    return 0;
}


// Loads the tunnings from a scl file
int Microtonal::loadscl(string filename)
{
    FILE *file = fopen(filename.c_str(), "r");
    char tmp[500];

    fseek(file, 0, SEEK_SET);
    // loads the short description
    if (loadline(file, &tmp[0]))
        return 2;
    for (int i = 0; i < 500; ++i)
        if (tmp[i] < 32)
            tmp[i] = 0;
    Pname = string(tmp);
    Pcomment = string(tmp);
    // loads the number of the notes
    if (loadline(file, &tmp[0]))
        return 2;
    int nnotes = MAX_OCTAVE_SIZE;
    sscanf(&tmp[0], "%d", &nnotes);
    if (nnotes > MAX_OCTAVE_SIZE)
        return 2;
    // load the tunnings
    for (int nline = 0; nline < nnotes; ++nline)
    {
        if (loadline(file, &tmp[0]))
            return 2;
        linetotunings(nline, &tmp[0]);
    }
    fclose(file);

    octavesize = nnotes;
    for (int i = 0; i < octavesize; ++i)
    {
        octave[i].tuning = tmpoctave[i].tuning;
        octave[i].type = tmpoctave[i].type;
        octave[i].x1 = tmpoctave[i].x1;
        octave[i].x2 = tmpoctave[i].x2;
    }
    setPartMaps();
    return 0;
}


// Loads the mapping from a kbm file
int Microtonal::loadkbm(string filename)
{
    FILE *file = fopen(filename.c_str(), "r");
    int x;
    char tmp[500];

    fseek(file, 0, SEEK_SET);
    // loads the mapsize
    if (loadline(file,&tmp[0]))
        return 2;

    if (!sscanf(&tmp[0], "%d",&x))
        return 2;

    if (x < 1)
        x = 0;
    if (x > 127)
        x = 127; // just in case...
    Pmapsize = x;
    // loads first MIDI note to retune
    if (loadline(file, &tmp[0]))
        return 2;

    if (!sscanf(&tmp[0], "%d", &x))
        return 2;

    if (x < 1)
        x = 0;
    if (x > 127)
        x = 127; // just in case...
    Pfirstkey = x;
    // loads last MIDI note to retune
    if (loadline(file, &tmp[0]))
        return 2;

    if (!sscanf(&tmp[0], "%d", &x))
        return 2;

    if (x < 1)
        x = 0;
    if (x > 127)
        x = 127; // just in case...
    Plastkey = x;
    // loads last the middle note where scale fro scale degree=0
    if (loadline(file, &tmp[0]))
        return 2;

    if (!sscanf(&tmp[0], "%d", &x))
        return 2;

    if (x < 1)
        x = 0;
    if (x > 127)
        x = 127; // just in case...
    Pmiddlenote = x;
    // loads the reference note
    if (loadline(file, &tmp[0]))
        return 2;

    if (!sscanf(&tmp[0], "%d", &x))
        return 2;

    if (x < 1)
        x = 0;
    if (x > 127)
        x = 127; // just in case...
    PAnote = x;
    // loads the reference freq.
    if (loadline(file, &tmp[0]))
        return 2;

    float tmpPAfreq = 440.0f;
    if (!sscanf(&tmp[0], "%f", &tmpPAfreq))
        return 2;

    PAfreq = tmpPAfreq;

    // the scale degree(which is the octave) is not loaded, it is obtained by the tunnings with getoctavesize() method
    if (loadline(file, &tmp[0]))
        return 2;

    // load the mappings
    if (Pmapsize)
    {
        for (int nline = 0; nline < Pmapsize; ++nline)
        {
            if (loadline(file, &tmp[0]))
                return 2;
            if (!sscanf(&tmp[0], "%d", &x))
                x = -1;
            Pmapping[nline] = x;
        }
        Pmappingenabled = 1;
    }
    else
    {
        Pmappingenabled = 0;
        Pmapping[0] = 0;
        Pmapsize = 1;
    }
    fclose(file);

    return 0;
}


void Microtonal::add2XML(XMLwrapper *xml)
{
    xml->addparstr("name", Pname.c_str());
    xml->addparstr("comment", Pcomment.c_str());

    xml->addparbool("invert_up_down", Pinvertupdown);
    xml->addparbool("invert_up_down_center", Pinvertupdowncenter);

    xml->addparbool("enabled", Penabled);
    xml->addpar("global_fine_detune", lrint(Pglobalfinedetune));

    xml->addpar("a_note", PAnote);
    xml->addparreal("a_freq", PAfreq);

    if (!Penabled && xml->minimal)
        return;

    xml->beginbranch("SCALE");
        xml->addpar("scale_shift", Pscaleshift);
        xml->addpar("first_key", Pfirstkey);
        xml->addpar("last_key", Plastkey);
        xml->addpar("middle_note", Pmiddlenote);

        xml->beginbranch("OCTAVE");
        xml->addpar("octave_size", octavesize);
        for (int i = 0; i < octavesize; ++i)
        {
            xml->beginbranch("DEGREE", i);
            if (octave[i].type == 1)
            {
                xml->addparreal("cents", octave[i].tuning);
            }
            if (octave[i].type == 2)
            {
                xml->addpar("numerator", octave[i].x1);
                xml->addpar("denominator", octave[i].x2);
            }
            xml->endbranch();
        }
        xml->endbranch();

        xml->beginbranch("KEYBOARD_MAPPING");
        xml->addpar("map_size", Pmapsize);
        xml->addpar("mapping_enabled", Pmappingenabled);
        for (int i = 0; i < Pmapsize; ++i)
        {
            xml->beginbranch("KEYMAP", i);
            xml->addpar("degree", Pmapping[i]);
            xml->endbranch();
        }
        xml->endbranch();
    xml->endbranch();
}


void Microtonal::getfromXML(XMLwrapper *xml)
{
    Pname = xml->getparstr("name");
    Pcomment = xml->getparstr("comment");

    Pinvertupdown=xml->getparbool("invert_up_down", Pinvertupdown);
    Pinvertupdowncenter=xml->getparbool("invert_up_down_center", Pinvertupdowncenter);

    Penabled=xml->getparbool("enabled", Penabled);
    Pglobalfinedetune = xml->getpar127("global_fine_detune", Pglobalfinedetune);

    PAnote = xml->getpar127("a_note", PAnote);
    PAfreq = xml->getparreal("a_freq", PAfreq, 1.0, 10000.0);

    if (xml->enterbranch("SCALE"))
    {
        Pscaleshift = xml->getpar127("scale_shift", Pscaleshift);
        Pfirstkey = xml->getpar127("first_key", Pfirstkey);
        Plastkey = xml->getpar127("last_key", Plastkey);
        Pmiddlenote = xml->getpar127("middle_note", Pmiddlenote);

        if (xml->enterbranch("OCTAVE"))
        {
            octavesize = xml->getpar127("octave_size", octavesize);
            for (int i = 0; i < octavesize; ++i)
            {
                if (!xml->enterbranch("DEGREE", i))
                    continue;
                octave[i].x2 = 0;
                octave[i].tuning = xml->getparreal("cents", octave[i].tuning);
                octave[i].x1 = xml->getpar127("numerator", octave[i].x1);
                octave[i].x2 = xml->getpar127("denominator", octave[i].x2);

                if (octave[i].x2)
                {
                    octave[i].type = 2;
                    octave[i].tuning = ((float)octave[i].x1) / octave[i].x2;
                }
                else {
                    octave[i].type = 1;
                    //populate fields for display
                    float x = logf(octave[i].tuning) / LOG_2 * 1200.0f;
                    octave[i].x1 = (int) floor(x);
                    octave[i].x2 = (int) (floor(fmodf(x, 1.0f) * 1e6));
                }
                xml->exitbranch();
            }
            xml->exitbranch();
        }

        if (xml->enterbranch("KEYBOARD_MAPPING"))
        {
            Pmapsize = xml->getpar127("map_size", Pmapsize);
            Pmappingenabled = xml->getpar127("mapping_enabled", Pmappingenabled);
            for (int i = 0; i < Pmapsize; ++i)
            {
                if (!xml->enterbranch("KEYMAP", i))
                    continue;
                Pmapping[i] = xml->getpar("degree", Pmapping[i], -1, 127);
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }
}


bool Microtonal::saveXML(string filename)
{
    synth->getRuntime().xmlType = XML_MICROTONAL;
    XMLwrapper *xml = new XMLwrapper(synth);

    xml->beginbranch("MICROTONAL");
    add2XML(xml);
    xml->endbranch();

    bool result = xml->saveXMLfile(filename);
    delete xml;
    return result;
}


bool Microtonal::loadXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper(synth);
    if (NULL == xml)
    {
        synth->getRuntime().Log("Microtonal: loadXML failed to instantiate new XMLwrapper");
        return false;
    }
    if (!xml->loadXMLfile(filename))
    {
        delete xml;
        return false;
    }
    if (!xml->enterbranch("MICROTONAL"))
    {
        synth->getRuntime().Log(filename + " is not a scale file");
        return false;
    }
    getfromXML(xml);
    setPartMaps();
    xml->exitbranch();
    delete xml;
    return true;
}
