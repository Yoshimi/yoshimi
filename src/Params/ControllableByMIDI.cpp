

using namespace std;
#include "Misc/SynthEngine.h"
#include "Params/ControllableByMIDI.h"
#include "Misc/ControllableByMIDIUI.h"
#include "Misc/XMLwrapper.h"
#include <iostream>
#include <list>

midiControl::~midiControl(){
    if(controller)
        controller->removeMidiController(this);
}

void midiControl::changepar(int value){
    std::cout << "Par changed, par: " << par << ", value: " << value << endl;
    controller->changepar(par, value);
}

float midiControl::getpar(){
    float value;
    if(!controller) return 0;
    if(isFloat){
        value=controller->getparFloat(par);
    }
    else {
        value = (float)controller->getparChar(par);
    }
    return value;
}

/*ControllableByMIDI::~ControllableByMIDI(){
    removeAllMidiControllers();
}*/

void ControllableByMIDI::removeAllMidiControllers(SynthEngine *synth){
    if(isControlled){
        list<midiControl*>::iterator i;
        std::cout << "controllers to delete: " << controllers.size() << endl;
        for(i=controllers.begin(); controllers.size() > 0 && i != controllers.end();i++){
            synth->removeMidiControl(*i);
            i--;
        }
        isControlled = false;
    }
}

void ControllableByMIDI::reassignUIControls(ControllableByMIDIUI *ctrl){
    if(isControlled){
        list<midiControl*>::iterator i;
        for(i=controllers.begin(); i != controllers.end();i++){
            (*i)->ui = ctrl;
        }
    }
}

void ControllableByMIDI::unassignUIControls(){
    if(isControlled){
        list<midiControl*>::iterator i;
        for(i=controllers.begin(); i != controllers.end();i++){
            (*i)->ui = NULL;
        }
    }
}

void ControllableByMIDI::addMidiController(midiControl *ctrl){
    isControlled = true;
    list<midiControl*>::iterator i;
    for(i=controllers.begin(); i != controllers.end();i++){
        if((*i) == ctrl){
            return;
        }
    }
    controllers.push_back(ctrl);
}

void ControllableByMIDI::removeMidiController(midiControl *ctrl){
    list<midiControl*>::iterator i;
    for(i=controllers.begin(); i != controllers.end();i++){
        if((*i) == ctrl){
            controllers.erase(i);
            if(controllers.size() == 0){
                isControlled = false;
            }
            return;
        }
    }
}

midiControl *ControllableByMIDI::hasMidiController(int par){
    list<midiControl*>::iterator i;
    for(i=controllers.begin(); i != controllers.end();i++){
        if((*i)->par == par){
            return (*i);
        }
    }
    return NULL;
}

void ControllableByMIDI::add2XMLMidi(XMLwrapper *xml){
    if(controllers.size() == 0)
        return;
    xml->beginbranch("MIDI_CONTROLLERS");
    list<midiControl*>::iterator i;
    int cpt = 0;
    for(i = controllers.begin(); i != controllers.end(); i++){
        cout << "Controller écrit " << (*i)->channel << " " << (*i)->ccNbr << endl;
        xml->beginbranch("CONTROLLER", cpt);
        xml->addpar("ccNbr", (*i)->ccNbr);
        xml->addpar("channel", (*i)->channel);
        xml->addpar("min", (*i)->min);
        xml->addpar("max", (*i)->max);
        xml->addpar("par", (*i)->par);
        xml->addparbool("isFloat", (*i)->isFloat);
        xml->endbranch();
        cpt++;
    }
    xml->endbranch();
};
void ControllableByMIDI::getfromXMLMidi(XMLwrapper *xml, SynthEngine *synth){
    if(!xml->enterbranch("MIDI_CONTROLLERS"))
        return;
    int cpt = 0;
    while(xml->enterbranch("CONTROLLER", cpt) != false){
        midiControl *mc = new midiControl(
                xml->getpar127("ccNbr", -1),
                xml->getpar127("channel", -1),
                xml->getpar127("min", 0),
                xml->getpar127("max", 127),
                this,
                NULL,
                xml->getpar("par", -1, 0, 30),
                xml->getparbool("isFloat", 1)
            );
        xml->exitbranch();
        synth->addMidiControl(mc);
        cout << "Controller lu (" << cpt << ") " << mc->channel << " " << mc->ccNbr << endl;
        cpt++;
    }
    xml->exitbranch();
};