/* 
 * File:   midiController.cpp
 * Author: alox
 * 
 * Created on February 15, 2011, 7:46 PM
 */

#include "Synth/LFO.h"


#include "midiController.h"
#include "Misc/SynthEngine.h"
#include "PartUI.h"
#include "Synth/PADnote.h"
#include "MasterUI.h"
#include <iostream>
#include "Params/ADnoteParameters.h"

midiController::midiController() {
}

midiController::midiController(WidgetPDial* dial) {

    DuplicatedKnobInMidiCCPanel = NULL;
    SpinnerInMidiCCPanel = NULL;
    midiChannel = 0;
    ccNumber=0;
    recording=1;

    this->knob = dial;
    this->param = whichParameterDoesThisDialControl(dial);
    if (param.partN>-1) { //it's a part specific parameter
        midiChannel = synth->part[param.partN]->Prcvchn;
    }
    customMin = this->param.min;
    customMax = this->param.max;
}
/**
 * Usually called from the synth, when it receives a midi cc signal relative to this midiController.
 * It changes the parameter controlled by this controller. In this case midiControlled=true (default)
 * 
 * This function is also called when rotating a duplicated knob in the midi controllers window. In this
 * case midiControlled=false.
 */
void midiController::execute(char val, bool midiControlled) {
    //we adjust the value according to the custom min & max
    //if the val is 127 but the max is ten, value will be 10, if val is 0 and min is 30, value will be 30
    //double value = ((customMax-customMin)*((double)val/127)+customMin)*param.max/127.0;
    double value = (customMax-customMin)*((double)val/127) + customMin;
    if (!midiControlled) value = (param.max-param.min)*((double)val/127) + param.min;

    if (midiControlled&&DuplicatedKnobInMidiCCPanel!=NULL) {
        DuplicatedKnobInMidiCCPanel->value(value); //only change the value, no callback
    }
    //if the knob is visible, just rotate it and call its callback and we're set:
    if (knob!=NULL&&knob->active_r()&&knob->visible_r()) {
        knob->value(value);
        knob->do_callback();
        goto realtimeParChange;
    }

    //if we're here it means the original knob has been destroyed, we have to change the params manually, no callback this time
    if (param.pointerType==0) { //unsigned char
        *(unsigned char*)param.paramPointer = value;
    } else if (param.pointerType==1) { //float
        *(float*)param.paramPointer = value;
    } else if (param.pointerType==2) { //custom complex callback
        doComplexCallback(value);
    } else if (param.pointerType==3) {//effect parameter
        ((EffectMgr*)param.paramPointer)->seteffectpar(param.paramNumber,value);
    }
    realtimeParChange:
    if(param.partN!=-1) synth->part[param.partN]->realtimeUpdatePar(&(this->param));
}

void midiController::rotateDial(double val) {
    if (knob!=NULL) {
        knob->value(val);
    }
    if (DuplicatedKnobInMidiCCPanel!=NULL) {
        DuplicatedKnobInMidiCCPanel->value(val);
    }
}
/**
 * called by the associated dial when it's destroyed
 */
void midiController::removeDialPointer() {
    this->knob = NULL;
}
/**
 * called by the dial widgets when they are created
 * it checks if the dial's parameters are controlled by this controller, if so sets this->knob to the newly created dial
 */
void midiController::dialCreated(WidgetPDial* dial) {
    if(dial->param.paramName==parID::PNullParam) return;
    if (this->param==dial->param) {
        this->knob = dial;
        dial->setMidiControlled(true);
   }
}

/**
 * Called when a 'duplicated' dial is created in the midi controllers window
 * @param duplicatedKnob the newly created dial
 */
void midiController::duplicatedKnobCreated(WidgetPDial* duplicatedKnob) {
    DuplicatedKnobInMidiCCPanel = duplicatedKnob;
}
/**
 * Called by the midi Controllers window, to find out the label of a certain controller
 * @return the label associated with this controller
 */
char* midiController::getLabel() {
    return param.label;
}

void midiController::setLabel(const char* str) {
    strcpy(param.label,str);
    param.label[49] = '\0';
}

/**
 * sets the midi cc number associated with this controller
 * @param n
 */
void midiController::setMidiCCNumber(int n) {
    if (n<0||n>127) return;
    this->ccNumber = n;
    //i update the spinner widget if the midiCC window is open:
    if (SpinnerInMidiCCPanel==NULL||SpinnerInMidiCCPanel->value()==n) return;
    SpinnerInMidiCCPanel->value(n);
    SpinnerInMidiCCPanel = NULL;
}

void midiController::setChannel(int ch) {
    midiChannel = ch;
}

void midiController::record(int channel,int ccN) {
    this->ccNumber = ccN;
    this->midiChannel = channel;
    MidiRackUI->Record(channel,ccN);
    this->recording = 0;
}

midiController::~midiController() {
    if (knob!=NULL) {knob->setMidiControlled(false);}
}

/**
 * Scans all the midi controllable parameters to find the one controlled by this dial
 * @param the dial
 * @return a parameterStruct that identifies the param controlled by the dial
 */
parameterStruct midiController::whichParameterDoesThisDialControl(WidgetPDial* d) {
    
    parameterStruct rparam;
    double oldValue = d->value();
    d->value(1);
    d->do_callback();

    if(checkAgainst(&rparam,d,&synth->Pvolume,parID::PMasterVolume)) {
        sprintf(rparam.label,"Master Volume");
        goto resetDialAndReturn;
    }
    if(checkAgainst(&rparam,d,&synth->microtonal.Pglobalfinedetune,parID::PMasterDetune)) {
        sprintf(rparam.label,"Master Detune");
        goto resetDialAndReturn;
    }
    
    //check for part specific parameters
    
    for (int i=0;i<NUM_MIDI_PARTS;i++) {
        //ONLY CHECK ACTIVE PARTS
        if(synth->part[i]->Penabled||(guiMaster->partui->npart==i)) { //only if the part is enabled or shown
            rparam.partN = i;
            if (checkAgainst(&rparam,d,&synth->part[i]->Ppanning,parID::PPartPanning)) {
                rparam.pointerType = 2;
                sprintf(rparam.label,"Panning, part:%d",rparam.partN+1);
                goto resetDialAndReturn;
            }
            if (checkAgainst(&rparam,d,&synth->part[i]->Pvolume,parID::PPartVolume)) {
                rparam.pointerType = 2;
                sprintf(rparam.label,"Volume, part:%d",rparam.partN+1);
                goto resetDialAndReturn;
            }

            //Sys effects send:
            for (int e=0;e<NUM_SYS_EFX;e++) {
                rparam.effN = e;
                if (checkAgainst(&rparam,d,&synth->Psysefxvol[e][i],parID::PsysEfxSend)) {
                    sprintf(rparam.label,"Sys effect send, part:%d, effect:%d",rparam.partN+1,rparam.effN+1); goto resetDialAndReturn;
                }
            }
            rparam.effN = -1;

            //Controllers:
            if (checkAgainst(&rparam,d,&synth->part[i]->ctl->portamento.time,parID::PContrPortamentoTime)) {
                sprintf(rparam.label,"Portamento time, part:%d",rparam.partN+1); goto resetDialAndReturn;
            }
            if (checkAgainst(&rparam,d,&synth->part[i]->ctl->portamento.updowntimestretch,parID::PContrPortamentoUpDn)) {
                sprintf(rparam.label,"Portamento Dn/Up, part:%d",rparam.partN+1); goto resetDialAndReturn;
            }
            if (checkAgainst(&rparam,d,&synth->part[i]->ctl->resonancecenter.depth,parID::PContrResonanceDepth)) {
                sprintf(rparam.label,"Resonance center depth, part:%d",rparam.partN+1); goto resetDialAndReturn;
            }
            if (checkAgainst(&rparam,d,&synth->part[i]->ctl->resonancebandwidth.depth,parID::PContrResonanceBand)) {
                sprintf(rparam.label,"Resonance bandwidth depth, part:%d",rparam.partN+1); goto resetDialAndReturn;
            }
            if (checkAgainst(&rparam,d,&synth->part[i]->ctl->bandwidth.depth,parID::PContrBandwidthBand)) {
                sprintf(rparam.label,"Bandwidth depth, part:%d",rparam.partN+1); goto resetDialAndReturn;
            }
            if (checkAgainst(&rparam,d,&synth->part[i]->ctl->modwheel.depth,parID::PContrModwheelDepth)) {
                sprintf(rparam.label,"Modwheel depth, part:%d",rparam.partN+1); goto resetDialAndReturn;
            }
            if (checkAgainst(&rparam,d,&synth->part[i]->ctl->panning.depth,parID::PContrPanningDepth)) {
                sprintf(rparam.label,"Panning depth, part:%d",rparam.partN+1); goto resetDialAndReturn;
            }
            if (checkAgainst(&rparam,d,&synth->part[i]->ctl->filterq.depth,parID::PContrFilterQDepth)) {
                sprintf(rparam.label,"Filter Q depth, part:%d",rparam.partN+1); goto resetDialAndReturn;
            }
            if (checkAgainst(&rparam,d,&synth->part[i]->ctl->filtercutoff.depth,parID::PContrFiltercutoffDepth)) {
                sprintf(rparam.label,"Filter cutoff depth, part:%d",rparam.partN+1); goto resetDialAndReturn;
            }


            //check for AddSynthParameters
            
            for (int k=0;k<NUM_KIT_ITEMS;k++) {
                rparam.kitItemN = k;
                //only if the kit item is initialized (usually just number0 is initialized):
                //ONLY CHECK ACTIVE KIT ITEMS
                if(synth->part[rparam.partN]->kit[rparam.kitItemN].adpars) {
                    ADnoteGlobalParam* Gpar = &synth->part[rparam.partN]->kit[rparam.kitItemN].adpars->GlobalPar;
                    if (checkAgainst(&rparam,d,&Gpar->PPanning,parID::PAddSynthPan)) {
                        sprintf(rparam.label,"AddSynth panning, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->PPunchStrength,parID::PAddSynthPunchStrength)) {
                        sprintf(rparam.label,"Punch Strength, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->PPunchTime,parID::PAddSynthPunchTime)) {
                        sprintf(rparam.label,"Punch Time, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->PPunchStretch,parID::PAddSynthPunchStretch)) {
                        sprintf(rparam.label,"Punch Stretch, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->PPunchVelocitySensing,parID::PAddSynthPunchVelocity)) {
                        sprintf(rparam.label,"Punch VelocitySensing, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    
                    //Amp envelope
                    if (checkAgainst(&rparam,d,&Gpar->AmpEnvelope->PA_dt ,parID::PAddSynthAmpEnv1)) {
                        sprintf(rparam.label,"AddSynth AmpEnv A_dt, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->AmpEnvelope->PD_dt ,parID::PAddSynthAmpEnv2)) {
                        sprintf(rparam.label,"AddSynth AmpEnv D_dt, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->AmpEnvelope->PS_val ,parID::PAddSynthAmpEnv3)) {
                        sprintf(rparam.label,"AddSynth AmpEnv S_Val, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->AmpEnvelope->PR_dt ,parID::PAddSynthAmpEnv4)) {
                        sprintf(rparam.label,"AddSynth AmpEnv R_dt, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->AmpEnvelope->Penvstretch ,parID::PAddSynthAmpEnvStretch)) {
                        sprintf(rparam.label,"AddSynth AmpEnv stretch, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    //Amp LFO    pars->GlobalPar.AmpLfo
                    rparam.pointerType = 1; //ampLfo freq is float
                    if (checkAgainst(&rparam,d,&Gpar->AmpLfo->Pfreq ,parID::PAddSynthAmpLfoFreq)) {
                        sprintf(rparam.label,"AmpLfo freq, part:%d",rparam.partN+1);
                        rparam.min = 0; rparam.max = 1;
                        goto resetDialAndReturn;
                    }
                    rparam.pointerType = 0; //back to normal
                    if (checkAgainst(&rparam,d,&Gpar->AmpLfo->Pintensity ,parID::PAddSynthAmpLfoIntensity)) {
                        sprintf(rparam.label,"AmpLfo Depth, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->AmpLfo->Pstartphase ,parID::PAddSynthAmpLfoStart)) {
                        sprintf(rparam.label,"AmpLfo Start, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->AmpLfo->Pdelay ,parID::PAddSynthAmpLfoDelay)) {
                        sprintf(rparam.label,"AmpLfo Delay, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->AmpLfo->Pstretch ,parID::PAddSynthAmpLfoStretch)) {
                        sprintf(rparam.label,"AmpLfo Stretch, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->AmpLfo->Prandomness ,parID::PAddSynthAmpLfoRand)) {
                        sprintf(rparam.label,"AmpLfo Randomness, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->AmpLfo->Pfreqrand ,parID::PAddSynthAmpLfoFreqRand)) {
                        sprintf(rparam.label,"AmpLfo Freq. rand., part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    //freq LFO
                    rparam.pointerType = 1; //FreqLfo freq is float
                    if (checkAgainst(&rparam,d,&Gpar->FreqLfo->Pfreq ,parID::PAddSynthFreqLfoFreq)) {
                        sprintf(rparam.label,"FreqLfo freq, part:%d",rparam.partN+1);
                        rparam.min = 0; rparam.max = 1;
                        goto resetDialAndReturn;
                    }
                    rparam.pointerType = 0; //back to normal
                    if (checkAgainst(&rparam,d,&Gpar->FreqLfo->Pintensity ,parID::PAddSynthFreqLfoIntensity)) {
                        sprintf(rparam.label,"FreqLfo Depth, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FreqLfo->Pstartphase ,parID::PAddSynthFreqLfoStart)) {
                        sprintf(rparam.label,"FreqLfo Start, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FreqLfo->Pdelay ,parID::PAddSynthFreqLfoDelay)) {
                        sprintf(rparam.label,"FreqLfo Delay, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FreqLfo->Pstretch ,parID::PAddSynthFreqLfoStretch)) {
                        sprintf(rparam.label,"FreqLfo Stretch, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FreqLfo->Prandomness ,parID::PAddSynthFreqLfoRand)) {
                        sprintf(rparam.label,"FreqLfo Randomness, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FreqLfo->Pfreqrand ,parID::PAddSynthFreqLfoFreqRand)) {
                        sprintf(rparam.label,"FreqLfo Freq. rand., part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    //filter LFO
                    rparam.pointerType = 1; //FreqLfo freq is float
                    if (checkAgainst(&rparam,d,&Gpar->FilterLfo->Pfreq ,parID::PAddSynthFilterLfoFreq)) {
                        sprintf(rparam.label,"FilterLfo freq, part:%d",rparam.partN+1);
                        rparam.min = 0; rparam.max = 1;
                        goto resetDialAndReturn;
                    }
                    rparam.pointerType = 0; //back to normal
                    if (checkAgainst(&rparam,d,&Gpar->FilterLfo->Pintensity ,parID::PAddSynthFilterLfoIntensity)) {
                        sprintf(rparam.label,"FilterLfo Depth, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterLfo->Pstartphase ,parID::PAddSynthFilterLfoStart)) {
                        sprintf(rparam.label,"FilterLfo Start, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterLfo->Pdelay ,parID::PAddSynthFilterLfoDelay)) {
                        sprintf(rparam.label,"FilterLfo Delay, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterLfo->Pstretch ,parID::PAddSynthFilterLfoStretch)) {
                        sprintf(rparam.label,"FilterLfo Stretch, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterLfo->Prandomness ,parID::PAddSynthFilterLfoRand)) {
                        sprintf(rparam.label,"FilterLfo Randomness, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterLfo->Pfreqrand ,parID::PAddSynthFilterLfoFreqRand)) {
                        sprintf(rparam.label,"FilterLfo Freq. rand., part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    //Freq Envelope: (GlobalPar.freqEnvelope)
                    if (checkAgainst(&rparam,d,&Gpar->FreqEnvelope->PA_val ,parID::PAddSynthFreqEnv1)) {
                        sprintf(rparam.label,"FreqEnvelope Start Val, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FreqEnvelope->PA_dt ,parID::PAddSynthFreqEnv2)) {
                        sprintf(rparam.label,"FreqEnvelope Attack, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FreqEnvelope->PR_dt ,parID::PAddSynthFreqEnv3)) {
                        sprintf(rparam.label,"FreqEnvelope Release, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FreqEnvelope->PR_val ,parID::PAddSynthFreqEnv4)) {
                        sprintf(rparam.label,"FreqEnvelope Release Val, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FreqEnvelope->Penvstretch ,parID::PAddSynthFreqEnv5)) {
                        sprintf(rparam.label,"FreqEnvelope Stretch, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    //Global Filter
                    if (checkAgainst(&rparam,d,&Gpar->GlobalFilter->Pfreq ,parID::PAddFilter1)) {
                        sprintf(rparam.label,"GlobalFilter C.Freq., part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->GlobalFilter->Pq ,parID::PAddFilter2)) {
                        sprintf(rparam.label,"GlobalFilter Q, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    //GlobalPar.PFilterVelocityScale vsnsA velocity sensing
                    if (checkAgainst(&rparam,d,&Gpar->PFilterVelocityScale ,parID::PAddFilter3)) {
                        sprintf(rparam.label,"GlobalFilter VelocitySensing, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->PFilterVelocityScaleFunction ,parID::PAddFilter4)) {
                        sprintf(rparam.label,"GlobalFilter VelocityFunction, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->GlobalFilter->Pfreqtrack ,parID::PAddFilter5)) {
                        sprintf(rparam.label,"GlobalFilter Freq.Track, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->GlobalFilter->Pgain ,parID::PAddFilter6)) {
                        sprintf(rparam.label,"GlobalFilter gain, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    //filter envelope:
                    if (checkAgainst(&rparam,d,&Gpar->FilterEnvelope->PA_val ,parID::PAddFilterEnv1)) {
                        sprintf(rparam.label,"GlobalFilterEnvelope A.val, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterEnvelope->PA_dt ,parID::PAddFilterEnv2)) {
                        sprintf(rparam.label,"GlobalFilterEnvelope A.dt, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterEnvelope->PD_val ,parID::PAddFilterEnv3)) {
                        sprintf(rparam.label,"GlobalFilterEnvelope D.val, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterEnvelope->PD_dt ,parID::PAddFilterEnv4)) {
                        sprintf(rparam.label,"GlobalFilterEnvelope D.dt, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterEnvelope->PR_dt ,parID::PAddFilterEnv5)) {
                        sprintf(rparam.label,"GlobalFilterEnvelope R.dt, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterEnvelope->PR_val ,parID::PAddFilterEnv6)) {
                        sprintf(rparam.label,"GlobalFilterEnvelope R.val, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    if (checkAgainst(&rparam,d,&Gpar->FilterEnvelope->Penvstretch ,parID::PAddFilterEnv7)) {
                        sprintf(rparam.label,"GlobalFilterEnvelope Stretch, part:%d",rparam.partN+1);
                        goto resetDialAndReturn;
                    }
                    
                    //voice-specific parameters:
                    for (int v=0;v<NUM_VOICES;v++) {
                        ADnoteVoiceParam* Adpar= &synth->part[rparam.partN]->kit[rparam.kitItemN].adpars->VoicePar[v];
                        if (Adpar->Enabled) {
                            rparam.voiceN = v;
                            if (checkAgainst(&rparam,d,&Adpar->PPanning ,parID::PAddVPanning)) {
                                sprintf(rparam.label,"ADVoice panning, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                goto resetDialAndReturn;
                            }
                            if (checkAgainst(&rparam,d,&Adpar->Unison_stereo_spread ,parID::PAddVStereoSpread)) {
                                sprintf(rparam.label,"ADVoice Stereo_spread, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                goto resetDialAndReturn;
                            }
                            if (checkAgainst(&rparam,d,&Adpar->Unison_vibratto ,parID::PAddVVibratto)) {
                                sprintf(rparam.label,"ADVoice Vibratto, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                goto resetDialAndReturn;
                            }
                            if (checkAgainst(&rparam,d,&Adpar->Unison_vibratto_speed ,parID::PAddVVibSpeed)) {
                                sprintf(rparam.label,"ADVoice Vibratto Speed, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                goto resetDialAndReturn;
                            }
                            //AmpEnvelope:
                            if (Adpar->PAmpEnvelopeEnabled) {
                                if (checkAgainst(&rparam,d,&Adpar->AmpEnvelope->PA_dt ,parID::PaddVAmpEnv1)) {
                                    sprintf(rparam.label,"AddSynthV AmpEnv A_dt, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->AmpEnvelope->PD_dt ,parID::PaddVAmpEnv2)) {
                                    sprintf(rparam.label,"AddSynthV AmpEnv D_dt, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->AmpEnvelope->PS_val ,parID::PaddVAmpEnv3)) {
                                    sprintf(rparam.label,"AddSynthV AmpEnv S_Val, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->AmpEnvelope->PR_dt ,parID::PaddVAmpEnv4)) {
                                    sprintf(rparam.label,"AddSynthV AmpEnv R_dt, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->AmpEnvelope->Penvstretch ,parID::PaddVAmpEnvStretch)) {
                                    sprintf(rparam.label,"AddSynthV AmpEnv stretch, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                            }
                            //Amp LFO    AmpLfo
                            if (Adpar->PAmpLfoEnabled) {
                                rparam.pointerType = 1; //ampLfo freq is float
                                if (checkAgainst(&rparam,d,&Adpar->AmpLfo->Pfreq ,parID::PAddVoiceAmpLfoFreq)) {
                                    sprintf(rparam.label,"AmpLfo freq, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    rparam.min = 0; rparam.max = 1;
                                    goto resetDialAndReturn;
                                }
                                rparam.pointerType = 0; //back to normal
                                if (checkAgainst(&rparam,d,&Adpar->AmpLfo->Pintensity ,parID::PAddVoiceAmpLfoIntensity)) {
                                    sprintf(rparam.label,"AmpLfo Depth, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->AmpLfo->Pstartphase ,parID::PAddVoiceAmpLfoStart)) {
                                    sprintf(rparam.label,"AmpLfo Start, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->AmpLfo->Pdelay ,parID::PAddVoiceAmpLfoDelay)) {
                                    sprintf(rparam.label,"AmpLfo Delay, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->AmpLfo->Pstretch ,parID::PAddVoiceAmpLfoStretch)) {
                                    sprintf(rparam.label,"AmpLfo Stretch, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->AmpLfo->Prandomness ,parID::PAddVoiceAmpLfoRand)) {
                                    sprintf(rparam.label,"AmpLfo Randomness, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->AmpLfo->Pfreqrand ,parID::PAddVoiceAmpLfoFreqRand)) {
                                    sprintf(rparam.label,"AmpLfo Freq. rand., part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                            }
                            //Voice Filter
                            if (Adpar->PFilterEnabled) {
                                if (checkAgainst(&rparam,d,&Adpar->VoiceFilter->Pfreq ,parID::PAddVFilter1)) {
                                    sprintf(rparam.label,"VoiceFilter C.Freq., part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->VoiceFilter->Pq ,parID::PAddVFilter2)) {
                                    sprintf(rparam.label,"VoiceFilter Q, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->VoiceFilter->Pfreqtrack ,parID::PAddVFilter3)) {
                                    sprintf(rparam.label,"VoiceFilter Freq.Track, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->VoiceFilter->Pgain ,parID::PAddVFilter4)) {
                                    sprintf(rparam.label,"VoiceFilter gain, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                //filter envelope:
                                if (Adpar->PFilterEnvelopeEnabled) {
                                    if (checkAgainst(&rparam,d,&Adpar->FilterEnvelope->PA_val ,parID::PaddVFilterEnv1)) {
                                        sprintf(rparam.label,"ADVoice FilterEnv. A.val, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterEnvelope->PA_dt ,parID::PaddVFilterEnv2)) {
                                        sprintf(rparam.label,"ADVoice FilterEnv. A.dt, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterEnvelope->PD_val ,parID::PaddVFilterEnv3)) {
                                        sprintf(rparam.label,"ADVoice FilterEnv. D.val, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterEnvelope->PD_dt ,parID::PaddVFilterEnv4)) {
                                        sprintf(rparam.label,"ADVoice FilterEnv. D.dt, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterEnvelope->PR_dt ,parID::PaddVFilterEnv5)) {
                                        sprintf(rparam.label,"ADVoice FilterEnv. R.dt, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterEnvelope->PR_val ,parID::PaddVFilterEnv6)) {
                                        sprintf(rparam.label,"ADVoice FilterEnv. R.val, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterEnvelope->Penvstretch ,parID::PaddVFilterEnv7)) {
                                        sprintf(rparam.label,"ADVoice FilterEnv. Stretch, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                }
                                //filter LFO
                                if (Adpar->PFilterEnvelopeEnabled) {
                                    rparam.pointerType = 1; //FreqLfo freq is float
                                    if (checkAgainst(&rparam,d,&Adpar->FilterLfo->Pfreq ,parID::PaddVFilterLfoFreq)) {
                                        sprintf(rparam.label,"AdVoice FilterLfo freq, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        rparam.min = 0; rparam.max = 1;
                                        goto resetDialAndReturn;
                                    }
                                    rparam.pointerType = 0; //back to normal
                                    if (checkAgainst(&rparam,d,&Adpar->FilterLfo->Pintensity ,parID::PaddVFilterLfoIntensity)) {
                                        sprintf(rparam.label,"AdVoice FilterLfo Depth, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterLfo->Pstartphase ,parID::PaddVFilterLfoStart)) {
                                        sprintf(rparam.label,"AdVoice FilterLfo Start, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterLfo->Pdelay ,parID::PaddVFilterLfoDelay)) {
                                        sprintf(rparam.label,"AdVoice FilterLfo Delay, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterLfo->Pstretch ,parID::PaddVFilterLfoStretch)) {
                                        sprintf(rparam.label,"AdVoice FilterLfo Stretch, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterLfo->Prandomness ,parID::PaddVFilterLfoRand)) {
                                        sprintf(rparam.label,"AdVoice FilterLfo Randomness, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                    if (checkAgainst(&rparam,d,&Adpar->FilterLfo->Pfreqrand ,parID::PaddVFilterLfoFreqRand)) {
                                        sprintf(rparam.label,"AdVoice FilterLfo Freq. rand., part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                        goto resetDialAndReturn;
                                    }
                                }
                            }
                            if (Adpar->PFreqEnvelopeEnabled) {
                                //Freq Envelope:
                                if (checkAgainst(&rparam,d,&Adpar->FreqEnvelope->PA_val ,parID::PaddVFreqEnv1)) {
                                    sprintf(rparam.label,"FreqEnvelope Start Val, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FreqEnvelope->PA_dt ,parID::PaddVFreqEnv2)) {
                                    sprintf(rparam.label,"FreqEnvelope Attack, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FreqEnvelope->PR_dt ,parID::PaddVFreqEnv3)) {
                                    sprintf(rparam.label,"FreqEnvelope Release, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FreqEnvelope->PR_val ,parID::PaddVFreqEnv4)) {
                                    sprintf(rparam.label,"FreqEnvelope Release Val, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FreqEnvelope->Penvstretch ,parID::PaddVFreqEnv5)) {
                                    sprintf(rparam.label,"FreqEnvelope Stretch, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                            }
                            if (Adpar->PFreqLfoEnabled) {
                                //freq LFO
                                rparam.pointerType = 1; //FreqLfo freq is float
                                if (checkAgainst(&rparam,d,&Adpar->FreqLfo->Pfreq ,parID::PaddVFreqLfoFreq)) {
                                    sprintf(rparam.label,"FreqLfo freq, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    rparam.min = 0; rparam.max = 1;
                                    goto resetDialAndReturn;
                                }
                                rparam.pointerType = 0; //back to normal
                                if (checkAgainst(&rparam,d,&Adpar->FreqLfo->Pintensity ,parID::PaddVFreqLfoIntensity)) {
                                    sprintf(rparam.label,"FreqLfo Depth, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FreqLfo->Pstartphase ,parID::PaddVFreqLfoStart)) {
                                    sprintf(rparam.label,"FreqLfo Start, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FreqLfo->Pdelay ,parID::PaddVFreqLfoDelay)) {
                                    sprintf(rparam.label,"FreqLfo Delay, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FreqLfo->Pstretch ,parID::PaddVFreqLfoStretch)) {
                                    sprintf(rparam.label,"FreqLfo Stretch, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FreqLfo->Prandomness ,parID::PaddVFreqLfoRand)) {
                                    sprintf(rparam.label,"FreqLfo Randomness, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FreqLfo->Pfreqrand ,parID::PaddVFreqLfoFreqRand)) {
                                    sprintf(rparam.label,"FreqLfo Freq. rand., part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                            }
                            if (Adpar->PFMAmpEnvelopeEnabled) {
                                //Amp envelope
                                if (checkAgainst(&rparam,d,&Adpar->FMAmpEnvelope->PA_dt ,parID::PaddModAmpEnv1)) {
                                    sprintf(rparam.label,"addMod AmpEnv A_dt, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FMAmpEnvelope->PD_dt ,parID::PaddModAmpEnv2)) {
                                    sprintf(rparam.label,"addMod AmpEnv D_dt, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FMAmpEnvelope->PS_val ,parID::PaddModAmpEnv3)) {
                                    sprintf(rparam.label,"addMod AmpEnv S_Val, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FMAmpEnvelope->PR_dt ,parID::PaddModAmpEnv4)) {
                                    sprintf(rparam.label,"addMod AmpEnv R_dt, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FMAmpEnvelope->Penvstretch ,parID::PaddModAmpEnvStretch)) {
                                    sprintf(rparam.label,"addMod AmpEnv stretch, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                            }
                            if (Adpar->PFMFreqEnvelopeEnabled) {
                                //Freq Envelope: (GlobalPar.freqEnvelope)
                                if (checkAgainst(&rparam,d,&Adpar->FMFreqEnvelope->PA_val ,parID::PaddModFreqEnv1)) {
                                    sprintf(rparam.label,"Mod. FreqEnvelope Start Val, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FMFreqEnvelope->PA_dt ,parID::PaddModFreqEnv2)) {
                                    sprintf(rparam.label,"Mod. FreqEnvelope Attack, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FMFreqEnvelope->PR_dt ,parID::PaddModFreqEnv3)) {
                                    sprintf(rparam.label,"Mod. FreqEnvelope Release, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FMFreqEnvelope->PR_val ,parID::PaddModFreqEnv4)) {
                                    sprintf(rparam.label,"Mod. FreqEnvelope Release Val, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                                if (checkAgainst(&rparam,d,&Adpar->FMFreqEnvelope->Penvstretch ,parID::PaddModFreqEnv5)) {
                                    sprintf(rparam.label,"Mod. FreqEnvelope Stretch, part:%d, voice:%d",rparam.partN+1,rparam.voiceN+1);
                                    goto resetDialAndReturn;
                                }
                            }
                        }
                    }
                }
            }
            for (int e=0;e<NUM_PART_EFX;e++) {
                rparam.effN = e;
                EffectMgr* fx = synth->part[i]->partefx[e];
                if (checkAgainstEffects(&rparam,d,fx)) goto resetDialAndReturn;
            }
        } //if part enabled
    } //for loop

    //it's not a part-related parameter, gotta set partN back to -1
    rparam.partN = -1;

    //System effects
    //scan the enabled sysEffects:
    for (int e=0;e<NUM_SYS_EFX;e++) {
        rparam.effN = e;
        EffectMgr* fx = synth->sysefx[e];
        if (checkAgainstEffects(&rparam,d,fx)) goto resetDialAndReturn;
    }

    //default:
    rparam.paramPointer = NULL;
    rparam.paramName = parID::PNullParam;
    
    resetDialAndReturn:
    d->value(oldValue);
    d->do_callback();
    return rparam;
}

/**
 * Sets p->parName to parname and then checks if dial controlls the parameter p
 *
 * before calling this function be sure to set the correct values in the parameterStruct* p
 * p->pointerType is generally 0 (meaning the parameter is stored as an unsigned char)
 * but be sure to set it to 1 if it's a float for example (check parameterStruct for more info)
 *
 * @param p the parameterStruct to populate with the paramName
 * @param dial the dial to check
 * @param original a pointer to actual the parameter value stored in the synth
 * @param parName an id identifying the parameter (eg. parID::PMasterVolume)
 * @return true if dial controls the parameter specified by p and parName
 */
bool midiController::checkAgainst(parameterStruct* p, WidgetPDial* dial, void* original, int parName) {

    if (dial==NULL) return false;
    p->paramPointer = original;
    p->paramName = parName;
    //if it's an unsigned char:
    if (p->pointerType==0) {
        if(*(unsigned char*)p->paramPointer==(unsigned char)dial->value()) {
            dial->value(2);dial->do_callback();
            if(*(unsigned char*)p->paramPointer==(unsigned char)dial->value()) {
                 dial->value(1);dial->do_callback();
                 return true;
            }
            dial->value(1);dial->do_callback();
        }
    } else if (p->pointerType==1) {
        if(*(float*)p->paramPointer==(float)dial->value()) {
            dial->value(2);dial->do_callback();
            if(*(float*)p->paramPointer==(float)dial->value()) {
                 dial->value(1);dial->do_callback();
                 return true;
            }
            dial->value(1);dial->do_callback();
        }
    }
    return false;
}

bool midiController::checkAgainstEffects(parameterStruct* p, WidgetPDial* dial, EffectMgr* fx) {

    //if it's an effect the pointer will point to the effect manager itself, to call seteffectpar()
    switch (fx->geteffect()) {
        case 0: //No effect
            break;
        case 1: //Reverb
            if (checkAgainst(p,dial,&((Reverb*)(fx->efx))->Pvolume, parID::PReverb0)) {
                p->pointerType = 3; p->paramNumber=0;sprintf(p->label,"Reverb Volume");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Reverb*)(fx->efx))->Ppanning, parID::PReverb1)) {
                p->pointerType = 3; p->paramNumber=1;sprintf(p->label,"Reverb panning");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Reverb*)(fx->efx))->Ptime, parID::PReverb2)) {
                p->pointerType = 3; p->paramNumber=2;sprintf(p->label,"Reverb Time");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Reverb*)(fx->efx))->Pidelay, parID::PReverb3)) {
                p->pointerType = 3; p->paramNumber=3;sprintf(p->label,"Reverb delay");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Reverb*)(fx->efx))->Pidelayfb, parID::PReverb4)) {
                p->pointerType = 3; p->paramNumber=4;sprintf(p->label,"Reverb delay fb");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Reverb*)(fx->efx))->Plpf, parID::PReverb7)) {
                p->pointerType = 3; p->paramNumber=7;sprintf(p->label,"Reverb LPF");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Reverb*)(fx->efx))->Phpf, parID::PReverb8)) {
                p->pointerType = 3; p->paramNumber=8;sprintf(p->label,"Reverb HPF");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Reverb*)(fx->efx))->Plohidamp, parID::PReverb9)) {
                p->pointerType = 3; p->paramNumber=9;sprintf(p->label,"Reverb Damp");p->paramPointer = (void*)fx;return true;
            }

        case 2: //Echo
            if (checkAgainst(p,dial,&((Echo*)(fx->efx))->Pvolume, parID::PEcho0)) {
                p->pointerType = 3; p->paramNumber=0;sprintf(p->label,"Echo Volume");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Echo*)(fx->efx))->Ppanning, parID::PEcho1)) {
                p->pointerType = 3; p->paramNumber=1;sprintf(p->label,"Echo Panning");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Echo*)(fx->efx))->Pdelay, parID::PEcho2)) {
                p->pointerType = 3; p->paramNumber=2;sprintf(p->label,"Echo Delay");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Echo*)(fx->efx))->Plrdelay, parID::PEcho3)) {
                p->pointerType = 3; p->paramNumber=3;sprintf(p->label,"Echo L/R difference");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Echo*)(fx->efx))->Plrcross, parID::PEcho4)) {
                p->pointerType = 3; p->paramNumber=4;sprintf(p->label,"Echo L/R mixing");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Echo*)(fx->efx))->Pfb, parID::PEcho5)) {
                p->pointerType = 3; p->paramNumber=5;sprintf(p->label,"Echo Feedback");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Echo*)(fx->efx))->Phidamp, parID::PEcho6)) {
                p->pointerType = 3; p->paramNumber=6;sprintf(p->label,"Echo Dampening");p->paramPointer = (void*)fx;return true;
            }
            break;
        case 3: //Chorus
            if (checkAgainst(p,dial,&((Chorus*)(fx->efx))->Pvolume, parID::PChorus0)) {
                p->pointerType = 3; p->paramNumber=0;sprintf(p->label,"Chorus Volume");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Chorus*)(fx->efx))->Ppanning, parID::PChorus1)) {
                p->pointerType = 3; p->paramNumber=1;sprintf(p->label,"Chorus Panning");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Chorus*)(fx->efx))->lfo.Pfreq, parID::PChorus2)) {
                p->pointerType = 3; p->paramNumber=2;sprintf(p->label,"Chorus freq.");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Chorus*)(fx->efx))->lfo.Prandomness, parID::PChorus3)) {
                p->pointerType = 3; p->paramNumber=3;sprintf(p->label,"Chorus randomness");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Chorus*)(fx->efx))->lfo.Pstereo, parID::PChorus5)) {
                p->pointerType = 3; p->paramNumber=5;sprintf(p->label,"Chorus L/R phase shift");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Chorus*)(fx->efx))->Pdepth, parID::PChorus6)) {
                p->pointerType = 3; p->paramNumber=6;sprintf(p->label,"Chorus depth");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Chorus*)(fx->efx))->Pdelay, parID::PChorus7)) {
                p->pointerType = 3; p->paramNumber=7;sprintf(p->label,"Chorus delay");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Chorus*)(fx->efx))->Pfb, parID::PChorus8)) {
                p->pointerType = 3; p->paramNumber=8;sprintf(p->label,"Chorus feedback");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Chorus*)(fx->efx))->Plrcross, parID::PChorus9)) {
                p->pointerType = 3; p->paramNumber=9;sprintf(p->label,"Chorus L/R cross");p->paramPointer = (void*)fx;return true;
            }
            break;
        case 4: //Phaser
            if (checkAgainst(p,dial,&((Phaser*)(fx->efx))->Pvolume, parID::PPhaser0)) {
                p->pointerType = 3; p->paramNumber=0;sprintf(p->label,"Phaser Volume");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Phaser*)(fx->efx))->Ppanning, parID::PPhaser1)) {
                p->pointerType = 3; p->paramNumber=1;sprintf(p->label,"Phaser panning");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Phaser*)(fx->efx))->lfo.Pfreq , parID::PPhaser2)) {
                p->pointerType = 3; p->paramNumber=2;sprintf(p->label,"Phaser freq.");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Phaser*)(fx->efx))->lfo.Prandomness, parID::PPhaser3)) {
                p->pointerType = 3; p->paramNumber=3;sprintf(p->label,"Phaser randomness");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Phaser*)(fx->efx))->lfo.Pstereo, parID::PPhaser5)) {
                p->pointerType = 3; p->paramNumber=5;sprintf(p->label,"Phaser L/R phase shift");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Phaser*)(fx->efx))->Pdepth, parID::PPhaser6)) {
                p->pointerType = 3; p->paramNumber=6;sprintf(p->label,"Phaser depth");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Phaser*)(fx->efx))->Pfb, parID::PPhaser7)) {
                p->pointerType = 3; p->paramNumber=7;sprintf(p->label,"Phaser Feedback");p->paramPointer = (void*)fx;return true;
            }
            if (checkAgainst(p,dial,&((Phaser*)(fx->efx))->Plrcross, parID::PPhaser9)) {
                p->pointerType = 3; p->paramNumber=9;sprintf(p->label,"Phaser L/R routing");p->paramPointer = (void*)fx;return true;
            }
            break;
        case 5: //AlienWah
            if (checkAgainst(p,dial,&((Alienwah*)(fx->efx))->Pvolume, parID::PsysAlien0)) {
                p->pointerType = 3; p->paramNumber=0;sprintf(p->label,"Alien Volume");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Alienwah*)(fx->efx))->Ppanning, parID::PsysAlien1)) {
                p->pointerType = 3; p->paramNumber=1;sprintf(p->label,"Alien Panning");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Alienwah*)(fx->efx))->lfo.Pfreq, parID::PsysAlien2)) {
                p->pointerType = 3; p->paramNumber=2;sprintf(p->label,"Alien Freq");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Alienwah*)(fx->efx))->lfo.Prandomness, parID::PsysAlien3)) {
                p->pointerType = 3; p->paramNumber=3;sprintf(p->label,"Alien Randomness");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Alienwah*)(fx->efx))->lfo.Pstereo, parID::PsysAlien5)) {
                p->pointerType = 3; p->paramNumber=5;sprintf(p->label,"Alien L/R phase shift");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Alienwah*)(fx->efx))->Pdepth, parID::PsysAlien6)) {
                p->pointerType = 3; p->paramNumber=6;sprintf(p->label,"Alien Depth");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Alienwah*)(fx->efx))->Pfb, parID::PsysAlien7)) {
                p->pointerType = 3; p->paramNumber=7;sprintf(p->label,"Alien Feedback");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Alienwah*)(fx->efx))->Plrcross, parID::PsysAlien9)) {
                p->pointerType = 3; p->paramNumber=9;sprintf(p->label,"Alien L/R");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Alienwah*)(fx->efx))->Pphase, parID::PsysAlien10)) {
                p->pointerType = 3; p->paramNumber=10;sprintf(p->label,"Alien Phase");p->paramPointer = (void*)fx;return true;
                return true;
            }
            break;
        case 6: //distorsion
            if (checkAgainst(p,dial,&((Distorsion*)(fx->efx))->Pvolume, parID::PsysDis1)) {
                p->pointerType = 3; p->paramNumber=0;sprintf(p->label,"Distorsion Volume");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Distorsion*)(fx->efx))->Ppanning, parID::PsysDis2)) {
                p->pointerType = 3; p->paramNumber=1;sprintf(p->label,"Distorsion Panning");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Distorsion*)(fx->efx))->Plrcross, parID::PsysDis3)) {
                p->pointerType = 3; p->paramNumber=2;sprintf(p->label,"Distorsion L/R cross");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Distorsion*)(fx->efx))->Pdrive, parID::PsysDis4)) {
                p->pointerType = 3; p->paramNumber=3;sprintf(p->label,"Distorsion Drive");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Distorsion*)(fx->efx))->Plevel, parID::PsysDis5)) {
                p->pointerType = 3; p->paramNumber=4;sprintf(p->label,"Distorsion Level");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Distorsion*)(fx->efx))->Plpf, parID::PsysDis6)) {
                p->pointerType = 3; p->paramNumber=7;sprintf(p->label,"Distorsion LPFilter");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((Distorsion*)(fx->efx))->Phpf, parID::PsysDis7)) {
                p->pointerType = 3; p->paramNumber=8;sprintf(p->label,"Distorsion HPFilter");p->paramPointer = (void*)fx;return true;
                return true;
            }
            break;
        case 7: //EQ
            if (checkAgainst(p,dial,&((EQ*)(fx->efx))->Pvolume, parID::PsysEQgain)) {
                p->pointerType = 3; p->paramNumber=0;sprintf(p->label,"EQ gain");p->paramPointer = (void*)fx;return true;
                return true;
            }
            //check the 3 eqband specific knobs
            for (int b=0;b<MAX_EQ_BANDS;b++) {
                p->EQbandN = b;
                int npb = b*5+10;
                if (fx->geteffectpar(npb)!=0) { //this EQ band is activated
                    if (checkAgainst(p,dial,&((EQ*)(fx->efx))->filter[b].Pfreq, parID::PsysEQBfreq)) {
                        p->pointerType = 3; p->paramNumber=npb+1;sprintf(p->label,"EQ band freq");p->paramPointer = (void*)fx;return true;
                        return true;
                    }
                    if (checkAgainst(p,dial,&((EQ*)(fx->efx))->filter[b].Pgain, parID::PsysEQBgain)) {
                        p->pointerType = 3; p->paramNumber=npb+2;sprintf(p->label,"EQ Band gain");p->paramPointer = (void*)fx;return true;
                        return true;
                    }
                    if (checkAgainst(p,dial,&((EQ*)(fx->efx))->filter[b].Pq, parID::PsysEQBq)) {
                        p->pointerType = 3; p->paramNumber=npb+3;sprintf(p->label,"EQ Band Q");p->paramPointer = (void*)fx;return true;
                        return true;
                    }
                }
            }
            break;
        case 8: //Dynamic filter
            if (checkAgainst(p,dial,&((DynamicFilter*)(fx->efx))->Pvolume, parID::PDynFilter0)) {
                p->pointerType = 3; p->paramNumber=0;sprintf(p->label,"DynFilter volume");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((DynamicFilter*)(fx->efx))->Ppanning, parID::PDynFilter1)) {
                p->pointerType = 3; p->paramNumber=1;sprintf(p->label,"DynFilter panning");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((DynamicFilter*)(fx->efx))->lfo.Pfreq, parID::PDynFilter2)) {
                p->pointerType = 3; p->paramNumber=2;sprintf(p->label,"DynFilter freq");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((DynamicFilter*)(fx->efx))->lfo.Prandomness, parID::PDynFilter3)) {
                p->pointerType = 3; p->paramNumber=3;sprintf(p->label,"DynFilter randomness");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((DynamicFilter*)(fx->efx))->lfo.Pstereo, parID::PDynFilter5)) {
                p->pointerType = 3; p->paramNumber=5;sprintf(p->label,"DynFilter L/R phase shift");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((DynamicFilter*)(fx->efx))->Pdepth, parID::PDynFilter6)) {
                p->pointerType = 3; p->paramNumber=6;sprintf(p->label,"DynFilter depth");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((DynamicFilter*)(fx->efx))->Pampsns, parID::PDynFilter7)) {
                p->pointerType = 3; p->paramNumber=7;sprintf(p->label,"DynFilter amp. sns");p->paramPointer = (void*)fx;return true;
                return true;
            }
            if (checkAgainst(p,dial,&((DynamicFilter*)(fx->efx))->Pampsmooth, parID::PDynFilter9)) {
                p->pointerType = 3; p->paramNumber=9;sprintf(p->label,"DynFilter amp. smooth");p->paramPointer = (void*)fx;return true;
                return true;
            }
            break;
    }
    //it's not an effect, the search failed :(
    p->paramPointer = NULL;
    return false;
}

/**
 * called by midiController::execute() when changing the parameter needs more complex
 * operations rather than just changing a number.
 * @param val
 */
void midiController::doComplexCallback(double val) {
    //THIS FUNCTION IS NEVER USED ATM
    int np;
    switch(param.paramName) {
        case parID::PPartVolume:
            synth->part[param.partN]->setVolume((char)val);
            break;
        case parID::PPartPanning:
            synth->part[param.partN]->setPan((char)val);
            break;
        //EQ:
        case parID::PsysEQgain:
            np = param.EQbandN*5+10;
            synth->sysefx[param.effN]->seteffectpar(np,lrintf((unsigned char)val));
            /*if (knob!=NULL) { //if the eq graph is shown:
                ((EffUI*)knob->parent()->parent())->eqgraph->redraw();
            }*/
            break;
        case parID::PsysEQBfreq:
            np = param.EQbandN*5+11;
            synth->sysefx[param.effN]->seteffectpar(np,lrintf((unsigned char)val));
            /*if (knob!=NULL) { //if the eq graph is shown:
                ((EffUI*)knob->parent()->parent()->parent())->eqgraph->redraw();
            }*/
            break;
        case parID::PsysEQBgain:
            np = param.EQbandN*5+12;
            synth->sysefx[param.effN]->seteffectpar(np,lrintf((unsigned char)val));
            /*
            if (knob!=NULL) { //if the eq graph is shown:
                ((EffUI*)knob->parent()->parent()->parent())->eqgraph->redraw();
            }*/
            break;
         case parID::PsysEQBq:
            np = param.EQbandN*5+13;
            synth->sysefx[param.effN]->seteffectpar(np,lrintf((unsigned char)val));
            /*if (knob!=NULL) { //if the eq graph is shown:
                ((EffUI*)knob->parent()->parent()->parent())->eqgraph->redraw();
            }*/
            break;
         //DISTORSION:
        case parID::PsysDis1: ((Distorsion*)synth->sysefx[param.effN]->efx)->changepar(0,val);break;
        case parID::PsysDis2: ((Distorsion*)synth->sysefx[param.effN]->efx)->changepar(1,val);break;
        case parID::PsysDis3: ((Distorsion*)synth->sysefx[param.effN]->efx)->changepar(2,val);break;
        case parID::PsysDis6: ((Distorsion*)synth->sysefx[param.effN]->efx)->changepar(5,val);break;
        case parID::PsysDis7: ((Distorsion*)synth->sysefx[param.effN]->efx)->changepar(6,val);break;
        //ALIEN WAH
        case parID::PsysAlien0: ((Alienwah*)synth->sysefx[param.effN]->efx)->changepar(0,val);break;
        case parID::PsysAlien1: ((Alienwah*)synth->sysefx[param.effN]->efx)->changepar(1,val);break;
        case parID::PsysAlien2: ((Alienwah*)synth->sysefx[param.effN]->efx)->changepar(2,val);break;
        case parID::PsysAlien3: ((Alienwah*)synth->sysefx[param.effN]->efx)->changepar(3,val);break;
        case parID::PsysAlien5: ((Alienwah*)synth->sysefx[param.effN]->efx)->changepar(5,val);break;
        case parID::PsysAlien6: ((Alienwah*)synth->sysefx[param.effN]->efx)->changepar(6,val);break;
        case parID::PsysAlien7: ((Alienwah*)synth->sysefx[param.effN]->efx)->changepar(7,val);break;
        case parID::PsysAlien9: ((Alienwah*)synth->sysefx[param.effN]->efx)->changepar(9,val);break;
        case parID::PsysAlien10: ((Alienwah*)synth->sysefx[param.effN]->efx)->changepar(10,val);break;
    }
}
/**
 * sets the custom Max
 * @param v
 */
void midiController::setMax(double v) {
    if (v>param.max) v=param.max; 
    customMax = v;
}
/**
 * sets the custom Min
 * @param v
 */
void midiController::setMin(double v) {
    if (v<param.min) v=param.min;
    customMin = v;
}

void midiController::add2XML(XMLwrapper *xml) {

    xml->addpar("midiChannel",midiChannel);
    xml->addpar("ccNumber",ccNumber);
    //xml->addparcharpointer("label",label); label is never used!
    xml->addpar("customMin",customMin);
    xml->addpar("customMax",customMax);
    param.add2XML(xml);
}

/**
 * When loading a state from an xml file, we need to create the midiControllers based on the xml
 * @param xml
 */
midiController::midiController(XMLwrapper *xml) {
    DuplicatedKnobInMidiCCPanel = NULL;
    SpinnerInMidiCCPanel = NULL;
    midiChannel = xml->getpar127("midiChannel",0);
    ccNumber=xml->getpar127("ccNumber", 0);
    recording=0;
    customMin = xml->getpar127("customMin", 0);
    customMax = xml->getpar127("customMax", 127);

    this->knob = NULL;
    this->param.loadFromXML(xml);
}