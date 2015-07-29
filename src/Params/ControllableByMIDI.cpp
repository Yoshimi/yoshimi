

using namespace std;
#include "Params/ControllableByMIDI.h"
#include "Misc/ControllableByMIDIUI.h"
#include <iostream>
#include <list>

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

    std::cout << "valeur renvoyée: " << value << " isFloat: " << isFloat << endl;
    return value;
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
