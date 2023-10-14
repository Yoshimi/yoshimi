/*
    Microtonal.cpp - Tuning settings and microtonal capabilities

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2023, Will Godfrey

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

#include <cmath>
#include <iostream>
#include <algorithm>
#include <limits.h>

#include "Misc/Config.h"
#include "Misc/XMLwrapper.h"
#include "Misc/Microtonal.h"
#include "Misc/SynthEngine.h"
#include "Misc/NumericFuncs.h"
#include "Misc/FormatFuncs.h"
#include "Misc/FileMgrFuncs.h"

using func::power;
using file::loadText;
using file::findLeafName;
using std::cout;
using std::endl;
using std::string;
using std::to_string;

namespace { // local implementation details

    inline void splitLine(string& page, string& line)
    {
        size_t pos = page.find('\n');
        if (pos != string::npos)
        {
            line = page.substr(0, pos);
            page = page.substr(pos + 1, page.length());
        }
        else
        {
            line = page;
            page = "";
            func::trimEnds(line);
        }
        return;
    }

    inline bool validline(string line)
    {
        size_t idx = 0;
        bool ok = true;
        while (ok && idx < line.length() && line[idx] > '!')
        {
            char chr = line[idx];
            if (chr != ' ' && chr != '.' && chr != '/' && (chr < '0' || chr > '9'))
            {
                ok = false;
            }
            ++ idx;
        }
        return ok;
    }
}


int Microtonal::getLineFromText(string& page, string& line)
{
    line = "";
    do {
        splitLine(page, line);
    } while (line[0] == '!'); // don't want comment lines
    if (line.length() < 1)
        return SCALES::errors::missingEntry;
    return 0;
}


string Microtonal::reformatline(string text)
{
    string formattext = "";
    char Chr;
    for (size_t i = 0; i < text.length(); ++i)
    {
        Chr = text[i];
        if (Chr == '.' || Chr == '/' || (Chr >= '0' && Chr <= '9'))
            formattext += Chr;
    }
    //cout << "Formatted >" << formattext << endl;
    size_t found;
    found = formattext.find('.');
    if (found < 4)
    {
        string tmp (4 - found, '0'); // leading zeros
        formattext = tmp + formattext;
    }
    found = formattext.size();
    if (found < 11)
    {
        string tmp  (11 - found, '0'); // trailing zeros
        formattext += tmp;
    }
    return formattext;
}


void Microtonal::defaults(int type)
{
    if (type != 2) // not map
    {
        Pinvertupdown = 0;
        Pinvertupdowncenter = 60;
        octavesize = 12;
        Penabled = 0;
        PrefNote = 69;
        PrefFreq = 440.0f;
        Pscaleshift = 64;
        octave[11].type = 2;
        octave[11].x1 = 2;
        octave[11].x2 = 1;
        Pname = string("12tET");
        Pcomment = string("Default Tuning");

    }
    if (type != 1) // not tuning
    {
        Pfirstkey = 0;
        Plastkey = MAX_OCTAVE_SIZE - 1;
        Pmiddlenote = 60;
        Pmapsize = 12;
        PformalOctaveSize = 12;
        Pmappingenabled = 0;

        // suppress 'unused' warning till we know what to do with it!
        PformalOctaveSize = PformalOctaveSize;

        for (int i = 0; i < 128; ++i)
        {
            Pmapping[i] = i;
            PmapComment[i] = "";
        }
        Pcomment = string("Default Map");
    }

    for (size_t i = 0; i < MAX_OCTAVE_SIZE; ++i)
    {
        octave[i].text = reformatline(to_string((i % octavesize + 1) * 100)+ ".0");
        octave[i].tuning = pow(2.0, (i % octavesize + 1) / 12.0);
        octave[i].type = 1;
        octave[i].x1 = (i % octavesize + 1) * 100;
        octave[i].x2 = 0;
        octave[i].comment = "";
    }
    if (type == 0)
    {
        octave[11].type = 2;
        octave[11].x1 = 2;
        octave[11].x2 = 1;
        Pname = string("12tET");
        Pcomment = string("Equal Temperament 12 notes per octave");
    }
    setglobalfinedetune(64.0); // always set this
}

void Microtonal::setglobalfinedetune(float control)
{
    Pglobalfinedetune = control;
    // compute global fine detune, -64.0 .. 63.0 cents
    globalfinedetunerap =
        (Pglobalfinedetune > 64.0f || Pglobalfinedetune < 64.0f)
            ? power<2>((Pglobalfinedetune - 64.0f) / 1200.0f)
            : 1.0f;
    // was float globalfinedetunerap = powf(2.0f, (Pglobalfinedetune - 64.0f) / 1200.0f);
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

    float freq;
    if (Pmappingenabled && Pmapsize > 0) // added check to stop crash till it's sorted properly
    {
        // Compute how many mapped keys are from middle note to reference note
        // and find out the proportion between the freq. of middle note and "A" note
        int tmp = PrefNote - Pmiddlenote;
        int minus = 0;
        if (tmp < 0)
        {
            tmp   = -tmp;
            minus = 1;
        }
        int deltanote = 0;
        for (int i = 0; i < tmp; ++i)
        {
            if (Pmapping[i % Pmapsize] >= 0)
                deltanote++;
        }
        float rap_anote_middlenote =
            (deltanote == 0) ? (1.0f) : (octave[(deltanote - 1) % octavesize].tuning);
        if (deltanote != 0)
            rap_anote_middlenote *= powf(octave[octavesize - 1].tuning,
                                         (deltanote - 1) / octavesize);
        if (minus)
            rap_anote_middlenote = 1.0f / rap_anote_middlenote;

        // Convert from note (midi) to degree (note from the tuning)
        int degoct = (note - Pmiddlenote + Pmapsize * 200)
                      / Pmapsize - 200;
        int degkey = (note - Pmiddlenote + Pmapsize * 100) % Pmapsize;
        degkey = Pmapping[degkey];
        if (degkey < 0) // this key is not mapped
        {
            return -1.0f;
        }

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

        freq = (degkey == 0) ? (1.0f) : octave[degkey - 1].tuning;
        freq *= powf(octave[octavesize - 1].tuning, degoct);
        freq *= PrefFreq / rap_anote_middlenote;
    }
    else // if the mapping is disabled
    {
        int nt = note - PrefNote + scaleshift;
        int ntkey = (nt + octavesize * 100) % octavesize;
        // cast octavesize to a signed type so the expression stays signed
        int ntoct = (nt - ntkey) / int(octavesize);

        float oct  = octave[octavesize - 1].tuning;
        freq = octave[(ntkey + octavesize - 1) % octavesize].tuning
               * powf(oct, ntoct) * PrefFreq;
        if (ntkey == 0)
            freq /= oct;
    }
    if (scaleshift != 0)
        freq /= octave[scaleshift - 1].tuning;
    freq *= globalfinedetunerap;
    return freq * rap_keyshift;
}


// Convert a line to tunings; returns 0 if ok
int Microtonal::linetotunings(unsigned int nline, string text)
{
    text = func::trimEnds(text); // just to be sure
    size_t pos = text.find_first_of(" !"); // pull out any comment first
    if (pos != string::npos)
    {
        if (text[pos + 1] == '!')
                pos += 1; // don't want 2 comment markers
        string last = text.substr(pos + 1, text.length());
        int i = 0;
        while (last[i] <= '!')
            ++i;
        if (i > 0)
            last = last.substr(i, text.length());
        octave[nline].comment = func::trimEnds(last);
    }
    else
        octave[nline].comment = "";

    if (!validline(text))
        return SCALES::errors::badNumbers;

    int x1 = -1, x2 = -1, type = -1;
    double x = -1.0;
    double tuning = 1.0;

    if (text.find('.') != string::npos)
    {
        x = stod(text);
        //printf(">%f\n",x);
        if (x < 0.000001)
            return SCALES::errors::valueTooSmall;
        type = 1; // double type(cents)
        x1 = int(floor(x));
        double tmp = fmod(x, 1.0);
        x2 = int(floor(tmp * 1e6));
        tuning = pow(2.0, x / 1200.0);
        octave[nline].text = reformatline(text);
    }
    else
    {
        x1 = func::string2int(text);
        if (x1 < 1)
            x1 = 1;
        size_t found = text.find('/');
        if (found != string::npos && found < (text.length()))
        {
            if (text.length() > found + 1)
                x2 = func::string2int(text.substr(found + 1, text.length()));
        }
        else
            x2 = 1;
        if (x2 < 1)
            x2 = 1;
        type = 2; // division

        tuning = double(x1) / x2;
    }

    octave[nline].tuning = tuning;
    octave[nline].type = type;
    octave[nline].x1 = x1;
    octave[nline].x2 = x2;
    return 0; // ok
}


// Convert the text to tunings
int Microtonal::texttotunings(string page)
{
    size_t nl = 0;
    string line;
    while (!page.empty())
    {
        size_t pos = page.find('\n');
        if (pos != string::npos)
        {
            line = page.substr(0, pos);
            page = page.substr(pos + 1, page.length());
        }
        else
        {
            line = page;
            page = "";
        }

        int err = linetotunings(nl, line);
        if (err != 0)
           return err; // Parse error
        ++ nl;
    }


    if (nl > MAX_OCTAVE_SIZE)
        nl = MAX_OCTAVE_SIZE;
    if (!nl)
        return 0; // the input is empty
    octavesize = nl;
    synth->setAllPartMaps();
    return octavesize; // ok
}


// Convert the text to mapping
int Microtonal::texttomapping(string page)
{
    size_t pos = page.find_last_not_of(" \t\n");
    if (pos != string::npos)
        page.erase(pos + 1);
    int tx = 0;
    if (page[0] >= ' ' && Pmapsize > 0)
    {
        string line;
        while (!page.empty())
        {
            splitLine(page, line);
            size_t pos = line.find('!');
            if (pos != string::npos)
            {
                PmapComment[tx] = func::trimEnds(line.substr(pos + 1, line.length()));
            }
            else
                PmapComment[tx] = "";

            if (line.empty() || line[0] < '0' || line[0] > '9')
            {
                line = 'x';
                Pmapping[tx] = -1;
            }
            else
                Pmapping[tx] = stoi(line);
            ++tx;
        }
    }
    while (tx < Pmapsize)
    {
        Pmapping[tx] = -1;
        ++tx;
    }
    synth->setAllPartMaps();
    return tx;
}


string Microtonal::keymaptotext(void)
{
    string text = "";
    if (Pmapsize > 0)
    {
        for (int i = 0; i < Pmapsize; ++i)
        {
            if (i > 0)
                text += "\n";
            if (Pmapping[i] == -1)
                text += "x";
            else
                text += to_string(Pmapping[i]);
            if (!PmapComment[i].empty())
            {
                text += " ! ";
                text += PmapComment[i];
            }
        }
    }
    return text;
}


// Convert tuning to text line
void Microtonal::tuningtoline(unsigned int n, string& line)
{
    line = "";
    if (n > octavesize || n > MAX_OCTAVE_SIZE)
        return;

    string text = octave[n].text;
    if (octave[n].type == 2)
    {
        line = (to_string(octave[n].x1) + "/" + to_string(octave[n].x2));
    }
    else if (octave[n].type == 1)
    {
        //printf(">%f\n",octave[n].tuning);
        if (text > " ")
            line = text;
        else
            line = (to_string(octave[n].x1) + "." + to_string(octave[n].x2));
    }
}


string Microtonal::tuningtotext()
{
    string text;
    string line;
    for (size_t i = 0; i < octavesize; ++i)
    {
        if (i > 0)
            text += "\n";
        tuningtoline(i, line);
        text += line;
        if (!octave[i].comment.empty())
        {
            text += " ! ";
            text += octave[i].comment;
        }
    }
    synth->setAllPartMaps();
    return text;
}


// Loads the tunings from a scl file
int Microtonal::loadscl(const string& filename)
{
    string text = loadText(filename);
    if (text == "")
        return SCALES::errors::noFile;
    string line = "";
    int err = 0;
    int nnotes = 0;
    // loads the short description
    if (getLineFromText(text, line))
    {
        err = SCALES::errors::emptyFile;
    }
    if (err == 0)
    {
        Pname = findLeafName(filename);
        Pcomment = string(line);
        // loads the number of the notes
        if (getLineFromText(text, line))
            err = SCALES::errors::badFile;
    }
    if (err == 0)
    {
        nnotes = MAX_OCTAVE_SIZE;
        nnotes = func::string2int(line);
        if (size_t(nnotes) > MAX_OCTAVE_SIZE || nnotes < 2)
            err = SCALES::errors::badOctaveSize;
    }
    if (err == 0)
    {
    // load the tunings
        for (int nline = 0; nline < nnotes; ++nline)
        {
            err = getLineFromText(text, line);
            if (err == 0)
                err = linetotunings(nline, line);
            if (err < 0)
                break;
        }
    }
    if (err < 0)
        return err;

    octavesize = nnotes;
    synth->setAllPartMaps();
    synth->addHistory(filename, TOPLEVEL::XML::ScalaTune);
    return nnotes;
}


// Loads the mapping from a kbm file
int Microtonal::loadkbm(const string& filename)
{
    string text = loadText(filename);
    if (text == "")
        return SCALES::errors::noFile;

    string line = "";
    // loads the mapsize
    if (getLineFromText(text, line))
        return SCALES::errors::badFile;
    int tmpMapSize = func::string2int(line);
    if (tmpMapSize < 0 || tmpMapSize >= MAX_OCTAVE_SIZE)
            return SCALES::errors::badMapSize;

    int tmpFirst = 0;
    // loads first MIDI note to retune
    if (getLineFromText(text, line))
        return SCALES::errors::badFile;
    tmpFirst = func::string2int(line);
    if (tmpFirst < 0 || tmpFirst >= MAX_OCTAVE_SIZE)
        return SCALES::errors::badNoteNumber;

    int tmpLast;
        // loads last MIDI note to retune
    if (getLineFromText(text, line))
        return SCALES::errors::badFile;
    tmpLast = func::string2int(line);
    if (tmpLast < 0 || tmpLast >= MAX_OCTAVE_SIZE)
        return SCALES::errors::badNoteNumber;

    int tmpMid;
        // loads the middle note where scale from scale degree=0
    if (getLineFromText(text, line))
        return SCALES::errors::badFile;
    tmpMid = func::string2int(line);
    if (tmpMid < 0 || tmpMid >= MAX_OCTAVE_SIZE)
        return SCALES::errors::badNoteNumber;

    int tmpRefNote;
    // loads the reference note
    if (getLineFromText(text, line))
    return SCALES::errors::badFile;
    tmpRefNote = func::string2int(line);
    if (tmpRefNote < 0 || tmpRefNote >= MAX_OCTAVE_SIZE)
        return SCALES::errors::badNoteNumber;

    float tmpRefFreq;
    // loads the reference freq.
    if (getLineFromText(text, line))
        return SCALES::errors::badFile;
    tmpRefFreq = func::string2float(line);
    if (tmpRefFreq < 1)
        return SCALES::errors::valueTooSmall;
    if (tmpRefFreq > 20000)
        return SCALES::errors::valueTooBig;

    Pmappingenabled = 1;
    Pmapsize = tmpMapSize;
    Pfirstkey = tmpFirst;
    Plastkey = tmpLast;
    Pmiddlenote = tmpMid;
    PrefNote = tmpRefNote;
    PrefFreq = tmpRefFreq;
    if (getLineFromText(text, line))
        return SCALES::errors::badMapSize;
    PformalOctaveSize = func::string2int(line);
    if (tmpMapSize == 0)
    {
        synth->setAllPartMaps();
        synth->addHistory(filename, TOPLEVEL::XML::ScalaMap);
        return 1;
    }

    // the scale degree(which is the octave) is not loaded
    // it is obtained by the tunings with getoctavesize() method
    // TODO this is wrong!
    int x = 0;
    int err = 0;
    for (int nline = 0; nline < tmpMapSize; ++nline)
    {
        if (getLineFromText(text, line)) // EOF
        {
        // It's permissible for source file to have fewer entries than the
        // map size so we fill the rest as silent.
            Pmapping[nline] = -1;
            PmapComment[nline] = "";
            continue;
        }

        else
        {
            if (line[0] < '0' || line[0] > '9') // catches all possibilities!
                x = -1;
            else
            {
                x = std::stoi(line);
                if (x >= tmpMapSize)
                {
                    err = SCALES::errors::valueTooBig;
                    break;
                }
            }
        }
        Pmapping[nline] = x;
        size_t pos = line.find_first_of(" !");
        if (pos != std::string::npos)
        {
            if (line[pos + 1] == '!')
                pos += 1; // don't want 2 comment markers
            PmapComment[nline] = func::trimEnds(line.substr(pos + 1, line.length()));
        }
        else
        {
            PmapComment[nline] = "";
        }
    }

    if (err < 0)
        return err;

    Pmapsize = tmpMapSize;
    synth->setAllPartMaps();
    synth->addHistory(filename, TOPLEVEL::XML::ScalaMap);
    return tmpMapSize;
}


string Microtonal::scale2scl()
{
    string text = "! ";
    text += synth->microtonal.Pname;
    text += "\n!\n ";
    text += synth->microtonal.Pcomment;
    text += "\n ";
    text += to_string(synth->microtonal.octavesize);
    text += "\n!\n";
    for (size_t i = 0; i < synth->microtonal.octavesize; ++ i)
    {
        text += " ";
        if (octave[i].type == 1)
            text += synth->microtonal.octave[i].text;
        else
        {
            text+= to_string(octave[i].x1);
            text += "/";
            text+= to_string(octave[i].x2);
        }
        if (!octave[i].comment.empty())
        {
            text += " ! ";
            text += octave[i].comment;
        }
        text += "\n";
    }
    return text;
}

string Microtonal::map2kbm()
{
    string text = "! Scala keymap\n";
    text += "!\n";
//    text += "! map size\n";
    text += to_string(Pmapsize);
    text += "\n!\n";
//    text += "! first note\n";
    text += to_string(Pfirstkey);
    text += "\n!\n";
//    text += "! last note\n";
    text += to_string(Plastkey);
    text += "\n!\n";
//    text += "! middle note\n";
    text += to_string(Pmiddlenote);
    text += "\n!\n";
//    text += "! reference note\n";
    text += to_string(PrefNote);
    text += "\n!\n";
//    text += "! reference frequency\n";
    text += to_string(PrefFreq);
    text += "\n!\n";
//    text += "! formal octave\n";
    text += to_string(PformalOctaveSize);
    text += "\n";
    if (Pmapsize != 0)
    {
        text += "!\n";
        text += "! mapped notes\n";
        text += keymaptotext();
        text += "\n";
    }
    return text;
}


void Microtonal::add2XML(XMLwrapper *xml)
{
    xml->addparstr("name", Pname);
    xml->addparstr("comment", Pcomment);

    xml->addparbool("invert_up_down", Pinvertupdown);
    xml->addpar("invert_up_down_center", Pinvertupdowncenter);

    xml->addparbool("enabled", Penabled);
    xml->addparcombi("global_fine_detune", Pglobalfinedetune);

    xml->addpar("a_note", PrefNote);
    xml->addparreal("a_freq", PrefFreq);

    if (!Penabled && xml->minimal)
        return;

    xml->beginbranch("SCALE");
        xml->addpar("scale_shift", Pscaleshift);
        xml->addpar("first_key", Pfirstkey);
        xml->addpar("last_key", Plastkey);
        xml->addpar("middle_note", Pmiddlenote);

        xml->beginbranch("OCTAVE");
        xml->addpar("octave_size", octavesize);
        for (size_t i = 0; i < octavesize; ++i)
        {
            xml->beginbranch("DEGREE", i);
            if (octave[i].type == 1)
            {
                xml->addparstr("cents_text",octave[i].text);
                xml->addparreal("cents", octave[i].tuning);
                /*
                 * This is downgraded to preserve compatibility
                 * with both Zyn and older Yoshi versions
                 */
            }
            if (octave[i].type == 2)
            {
                xml->addparstr("cents_text",octave[i].text);
                xml->addpar("numerator", octave[i].x1);
                xml->addpar("denominator", octave[i].x2);;
            }
            xml->addparstr("comment" , octave[i].comment);
            xml->endbranch();
        }
        xml->endbranch();

        xml->beginbranch("KEYBOARD_MAPPING");
        xml->addpar("map_size", Pmapsize);
        xml->addpar("formal_octave_size", PformalOctaveSize);
        xml->addpar("mapping_enabled", Pmappingenabled);
        for (int i = 0; i < Pmapsize; ++i)
        {
            xml->beginbranch("KEYMAP", i);
            xml->addpar("degree", Pmapping[i]);
            xml->addparstr("comment", PmapComment[i]);
            xml->endbranch();
        }
        xml->endbranch();
    xml->endbranch();
}


int Microtonal::getfromXML(XMLwrapper *xml)
{
    int err = 0;
    Pname = xml->getparstr("name");
    Pcomment = xml->getparstr("comment");

    Pinvertupdown=xml->getparbool("invert_up_down", Pinvertupdown);
    Pinvertupdowncenter=xml->getpar127("invert_up_down_center", Pinvertupdowncenter);

    Penabled=xml->getparbool("enabled", Penabled);
    setglobalfinedetune(xml->getparcombi("global_fine_detune", Pglobalfinedetune, 0, 127));

    PrefNote = xml->getpar127("a_note", PrefNote);
    PrefFreq = xml->getparreal("a_freq", PrefFreq, 1.0, 10000.0);

    if (xml->enterbranch("SCALE"))
    {
        Pscaleshift = xml->getpar127("scale_shift", Pscaleshift);
        Pfirstkey = xml->getpar127("first_key", Pfirstkey);
        Plastkey = xml->getpar127("last_key", Plastkey);
        Pmiddlenote = xml->getpar127("middle_note", Pmiddlenote);

        if (xml->enterbranch("OCTAVE"))
        {
            octavesize = xml->getpar127("octave_size", octavesize);
            for (size_t i = 0; i < octavesize; ++i)
            {
                octave[i].text = "";
                if (!xml->enterbranch("DEGREE", i))
                    continue;
                string text = xml->getparstr("cents_text");
                octave[i].x2 = 0;
                if (text > " ")
                {
                    octave[i].text = reformatline(text);
                    octave[i].tuning = pow(2.0, stod(text) / 1200.0);
                }
                else
                {
                    octave[i].text = "";
                    octave[i].tuning = xml->getparreal("cents", octave[i].tuning);
                }
                octave[i].x1 = xml->getpar("numerator", octave[i].x1, 0, INT_MAX);
                octave[i].x2 = xml->getpar("denominator", octave[i].x2, 0, INT_MAX);

                if (octave[i].x2)
                {
                    octave[i].text = text;
                    octave[i].type = 2;
                    octave[i].tuning = double(octave[i].x1) / octave[i].x2;
                }
                else {
                    octave[i].type = 1;
                    //populate fields for display
                    double x = (log(octave[i].tuning) / LOG_2) * 1200.0;
                    octave[i].x1 = int(floor(x));
                    // this is a fudge to get round wierd values of x2
                    // it's only used if we don't have the text stored
                    double tmp = fmod(x, 1.0);
                    if (tmp < 0.0001)
                        octave[i].x2 = 0;
                    else if (tmp > 0.9999)
                    {
                        octave[i].x2 = 0;
                        octave[i].x1 += 1;
                    }
                    else
                        octave[i].x2 = int(floor(tmp * 1e6));

                    //octave[i].x2 = int(floor(fmod(x, 1.0) * 1e6));
                }
                octave[i].comment = "";
                octave[i].comment = xml->getparstr("comment");
                xml->exitbranch();
            }
            xml->exitbranch();
        }

        if (xml->enterbranch("KEYBOARD_MAPPING"))
        {
            Pmapsize = xml->getpar127("map_size", Pmapsize);
            PformalOctaveSize = xml->getpar127("formal_octave_size", PformalOctaveSize);
            Pmappingenabled = xml->getpar127("mapping_enabled", Pmappingenabled);
            for (int i = 0; i < Pmapsize; ++i)
            {
                if (!xml->enterbranch("KEYMAP", i))
                    continue;
                Pmapping[i] = xml->getpar("degree", Pmapping[i], -1, 127);
                PmapComment[i] = xml->getparstr("comment");
                if (Pmapping[i] >= Pmapsize)
                {
                    err = SCALES::errors::valueTooBig;
                    break;
                }
                xml->exitbranch();
            }
            xml->exitbranch();
        }
        xml->exitbranch();
    }
    return err;
}


bool Microtonal::saveXML(const string& filename)
{
    synth->getRuntime().xmlType = TOPLEVEL::XML::Scale;
    XMLwrapper *xml = new XMLwrapper(synth);

    xml->beginbranch("MICROTONAL");
    add2XML(xml);
    xml->endbranch();

    bool result = xml->saveXMLfile(filename);
    delete xml;
    return result;
}


int Microtonal::loadXML(const string& filename)
{
    int err = 0;
    XMLwrapper *xml = new XMLwrapper(synth);
    if (NULL == xml)
    {
        synth->getRuntime().Log("Microtonal: loadXML failed to instantiate new XMLwrapper", _SYS_::LogError);
        return 1;
    }
    if (!xml->loadXMLfile(filename))
    {
        delete xml;
        return 1;
    }
    if (!xml->enterbranch("MICROTONAL"))
    {
        synth->getRuntime().Log(filename + " is not a scale file", _SYS_::LogError);
        delete xml;
        return 1;
    }
    err = getfromXML(xml);
    if (err != 0)
    {
        delete xml;
        return err;
    }
    xml->exitbranch();
    delete xml;
    synth->setAllPartMaps();
    return 0;
}

float Microtonal::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    unsigned char type = 0;

    // microtonal defaults
    int min = 0;
    float def = 0;
    int max = MAX_OCTAVE_SIZE - 1;
    type |= TOPLEVEL::type::Integer;
    unsigned char learnable = TOPLEVEL::type::Learnable;

    switch (control)
    {
        case SCALES::control::refFrequency:
            min = A_MIN;
            def = A_DEF;
            max = A_MAX;
            break;
        case SCALES::control::refNote:
            min = 24;
            def = 69;
            max = 84;
            type |= learnable;
            break;
        case SCALES::control::invertScale:
            max = 1;
            type |= learnable;
            break;
        case SCALES::control::invertedScaleCenter:
            def = 60;
            type |= learnable;
            break;
        case SCALES::control::scaleShift:
            min = -63;
            max = 64;
            type |= learnable;
            break;

        case SCALES::control::enableMicrotonal:
            max = 1;
            type |= learnable;
            break;

        case SCALES::control::enableKeyboardMap:
            max = 1;
            type |= learnable;
            break;
        case SCALES::control::lowKey:
            type |= learnable;
            break;
        case SCALES::control::middleKey:
            def = 60;
            type |= learnable;
            break;
        case SCALES::control::highKey:
            def = MAX_OCTAVE_SIZE - 1;
            type |= learnable;
            break;

        case SCALES::control::tuning:
            max = 1;
            break;
        case SCALES::control::keyboardMap:
            max = 1;
            break;
        case SCALES::control::importScl:
            max = 1;
            break;
        case SCALES::control::importKbm:
            max = 1;
            break;
        case SCALES::control::name:
            max = 1;
            break;
        case SCALES::control::comment:
            max = 1;
            break;
        case SCALES::control::clearAll:
            max = 1;
            break;

        default:
            type |= TOPLEVEL::type::Error;
            break;
    }
    getData->data.type = type;
    if (type & TOPLEVEL::type::Error)
        return 1;

    switch (request)
    {
        case TOPLEVEL::type::Adjust:
            if (value < min)
                value = min;
            else if (value > max)
                value = max;
        break;
        case TOPLEVEL::type::Minimum:
            value = min;
            break;
        case TOPLEVEL::type::Maximum:
            value = max;
            break;
        case TOPLEVEL::type::Default:
            value = def;
            break;
    }
    return value;
}
