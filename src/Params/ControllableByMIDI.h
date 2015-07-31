/*
    ControllableByMIDI.cpp - Abstract Method to control elements

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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

    This file is derivative of ZynAddSubFX original code, modified October 2009
*/
#ifndef CONTROLLABLEBYMYDI_H
#define CONTROLLABLEBYMYDI_H

#include <iostream>
#include <list>

using namespace std;

class ControllableByMIDIUI;
class ControllableByMIDI;
class XMLwrapper;
class SynthEngine;

// Midi Learn
struct midiControl {
    int ccNbr;
    int channel;
    int min;
    int max;
    ControllableByMIDI *controller;
    ControllableByMIDIUI *ui;
    int par;
    bool recording;
    bool isFloat;

    midiControl(): controller(NULL), ui(NULL) {}
    midiControl(int ccNbr, int channel, int min, int max, ControllableByMIDI *controller, ControllableByMIDIUI *ui, int par, int isFloat): ccNbr(ccNbr), channel(channel), min(min), max(max), controller(controller), ui(ui), par(par), isFloat(isFloat), recording(false) {}
    midiControl(ControllableByMIDI *controller, ControllableByMIDIUI *ui, int par, int isFloat): ccNbr(-1), channel(-1), min(0), max(127), controller(controller), ui(ui), par(par), isFloat(isFloat), recording(true) {}
    ~midiControl();
    void changepar(int value);
    float getpar();
};

class ControllableByMIDI
{
public:
    virtual void changepar(int npar, double value) = 0;
    virtual unsigned char getparChar(int npar) = 0;
    virtual float getparFloat(int npar) = 0;

    /**
    void changepar(int npar, double value);
    unsigned char getparChar(int npar);
    float getparFloat(int npar);
    **/

    void reassignUIControls(ControllableByMIDIUI *ctrl);
    void unassignUIControls();
    void addMidiController(midiControl *ctrl);
    void removeMidiController(midiControl *ctrl);
    void removeAllMidiControllers(SynthEngine *synth);

    midiControl *hasMidiController(int par);

    void add2XMLMidi(XMLwrapper *xml);
    void getfromXMLMidi(XMLwrapper *xml, SynthEngine *synth);

    ControllableByMIDI(): isControlled(false) {}
    ~ControllableByMIDI(){}
private:
    list<midiControl*> controllers;
    bool isControlled;
};



#endif