/*
    Microtonal.cpp - Tuning settings and microtonal capabilities

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

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

    This file is derivative of original ZynAddSubFX code, modified January 2011
*/

#include <iostream>
#include <fenv.h>
#include <cmath>

#include "Misc/Config.h"
#include "Misc/XMLwrapper.h"
#include "Misc/Microtonal.h"

#define MAX_LINE_SIZE 80

float Microtonal::note_12et[128] = {
    8.175798416137695312, 8.661956787109375, 9.177021980285644531, 9.72271728515625,
    10.30086135864257812, 10.91337966918945312, 11.56232547760009766, 12.24985790252685547,
    12.97827339172363281, 13.75, 14.56761741638183594, 15.43385601043701172,

    16.35159683227539062, 17.32391357421875, 18.3540496826171875, 19.4454345703125,
    20.60172271728515625, 21.82676887512207031, 23.12465095520019531, 24.49971580505371094,
    25.95654678344726562, 27.5, 29.13523292541503906, 30.86770439147949219,
    32.70319366455078125, 34.64782333374023438, 36.70809173583984375, 38.890869140625,
    41.20344161987304688, 43.65353012084960938, 46.24930191040039062, 48.99942398071289062,
    51.91308975219726562,
    
    55, 58.27046585083007812, 61.73540878295898438, 65.4063873291015625,
    69.29564666748046875, 73.4161834716796875, 77.78173828125, 82.406890869140625,
    87.30706024169921875, 92.49860382080078125, 97.99886322021484375, 103.8261795043945312,
    
    110, 116.5409317016601562, 123.4708099365234375, 130.812774658203125,
    138.59130859375, 146.832366943359375, 155.5634765625, 164.81378173828125,
    174.614105224609375, 184.9972076416015625, 195.997711181640625, 207.6523590087890625,
    
    220, 233.0818634033203125, 246.9416351318359375, 261.62554931640625,
    277.182586669921875, 293.66473388671875, 311.126953125, 329.6275634765625,
    349.22821044921875, 369.994415283203125, 391.99542236328125, 415.3046875,
    
    440, 466.163726806640625, 493.883270263671875, 523.2510986328125,
    554.36517333984375, 587.3294677734375, 622.25390625, 659.255126953125,
    698.4564208984375, 739.98883056640625, 783.9908447265625, 830.609375,
    
    880, 932.32745361328125, 987.7664794921875, 1046.502197265625, 1108.73046875,
    1174.658935546875, 1244.5078125, 1318.51025390625, 1396.912841796875,
    1479.9776611328125, 1567.981689453125,
    
    1661.2186279296875, 1760, 1864.6549072265625, 1975.532958984375,
    2093.00439453125, 2217.460693359375, 2349.317626953125, 2489.015625,
    2637.020263671875, 2793.825927734375, 2959.955322265625, 3135.963134765625,

    3322.437744140625, 3520, 3729.309814453125, 3951.066162109375,
    4186.0087890625, 4434.92138671875, 4698.6357421875, 4978.03125,
    5274.04052734375, 5587.65185546875, 5919.91064453125, 6271.92626953125,
    
    6644.87548828125, 7040, 7458.61767578125, 7902.1318359375,
    8372.017578125, 8869.841796875, 9397.2705078125, 9956.0625,
    10548.0791015625, 11175.30078125, 21839.8212890625, 12543.8505859375
};

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


// Get the frequency according the note number
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
            ? pow(2.0f, (Pglobalfinedetune - 64.0f) / 1200.0f)
            : 1.0f;
    // was float globalfinedetunerap = powf(2.0f, (Pglobalfinedetune - 64.0f) / 1200.0f);

    if (!Penabled)
    {
        cerr << "note " << note << "\tkeyshift " << keyshift
             << " \tglobalfinedetunerap " << globalfinedetunerap << "\t"
             << getNoteFreq(note + keyshift) * globalfinedetunerap << endl;
        return getNoteFreq(note + keyshift) * globalfinedetunerap;
    }
    
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
            x2 = lrintf(floorf(tmp * 1e6f));
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
        snprintf(line, maxn, "%d.%d", octave[n].x1,octave[n].x2);
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
                    octave[i].type = 2;
                else
                    octave[i].type = 1;

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
                Pmapping[i] = xml->getpar127("degree", Pmapping[i]);
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }
}


bool Microtonal::saveXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper();

    xml->beginbranch("MICROTONAL");
    add2XML(xml);
    xml->endbranch();

    bool result = xml->saveXMLfile(filename);
    delete xml;
    return result;
}

bool Microtonal::loadXML(string filename)
{
    XMLwrapper *xml = new XMLwrapper();
    if (NULL == xml)
    {
        Runtime.Log("Microtonal loadXML fails to instantiate new XMLwrapper");
        return false;
    }
    if (!xml->loadXMLfile(filename))
    {
        delete xml;
        return false;
    }
    if (!xml->enterbranch("MICROTONAL"))
    {
        Runtime.Log("Error, " + filename + " is not a scale file");
        return false;
    }
    getfromXML(xml);
    xml->exitbranch();
    delete xml;
    return true;
}
