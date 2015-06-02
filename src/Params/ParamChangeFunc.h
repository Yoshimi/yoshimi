/* 
 * File:   paramChangeFunc.h
 * Author: alox
 *
 * Created on February 24, 2011, 3:38 PM
 */

#ifndef PARAMCHANGEFUNC_H
#define	PARAMCHANGEFUNC_H

#include "Misc/XMLwrapper.h"

//temp:
#include <stdio.h>

struct parameterStruct {
    int paramName; //numbers corresponding to parameters are defined in midiController,h
    int partN;
    int kitItemN;
    int voiceN;
    int effN;
    int EQbandN;
    int duplicated; //is 0 for normal knobs, 1 for the duplicated knobs in the midiCCrack, in order to differentiate them
    void* paramPointer; //ponter to the actual 'number' in the synth parameters to be modified
    int pointerType; //0=unsigned char*, 1=float*, 2=complex callback, 3 effect changepar()
    int paramNumber; //useful at times, for example for pointerType=3 (effect parameters)
    //min and max of the dial
    float min;
    float max;

    char label[50];

    parameterStruct();
    //we redefine the == operator, to be able to compare parameterStructs
    bool operator ==(parameterStruct other);

    void add2XML(XMLwrapper *xml);
    void loadFromXML(XMLwrapper *xml);
    void setPointerBasedOnParams();
};

#endif	/* PARAMCHANGEFUNC_H */

