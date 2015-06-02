/* 
 * File:   midiController.h
 * Author: alox
 *
 * Created on February 15, 2011, 7:46 PM
 */

#ifndef MIDICONTROLLER_H
#define	MIDICONTROLLER_H

#include <WidgetPDialUI.h>
#include <FL/Fl_Spinner.H>
#include "ParamChangeFunc.h"
#include "Effects/EffectMgr.h"

//forward declaration of the midiCCrack
class MidiCCRack;

class midiController {
public:
    midiController();
    midiController(WidgetPDial* dial);
    midiController(XMLwrapper *xml);
    virtual ~midiController();

    void execute(char val, bool midiControlled = true);
    void doComplexCallback(double val);
    void removeDialPointer();
    void dialCreated(WidgetPDial* dial);
    void duplicatedKnobCreated(WidgetPDial* duplicatedKnob);
    void setMidiCCNumber(int n);
    char* getLabel();
    void setLabel(const char* str);
    static parameterStruct whichParameterDoesThisDialControl(WidgetPDial* d) ;
    void add2XML(XMLwrapper *xml);

    void setMin(double v);
    void setMax(double v);
    void setChannel(int ch);
    void record(int channel,int ccN);

    int midiChannel;
    int ccNumber;
    parameterStruct param;

    bool recording;
    char label[30];
    double customMin;double customMax;

    WidgetPDial* knob;
    WidgetPDial* DuplicatedKnobInMidiCCPanel;
    Fl_Spinner* SpinnerInMidiCCPanel;
    MidiCCRack* MidiRackUI;

private:
    void rotateDial(double val);
    static bool checkAgainst(parameterStruct* p, WidgetPDial* dial, void* original, int parName);
    static bool checkAgainstEffects(parameterStruct* p, WidgetPDial* dial, EffectMgr* fx);
};

class parID {
public:
    static const int PNullParam = 0;
    static const int PMasterVolume = 1;
    static const int PMasterDetune = 2;
    static const int PPartPanning = 3;
    static const int PPartVolume = 4;
    static const int PAddSynthPan = 5;
    static const int PAddSynthPunchStrength = 6;
    static const int PAddPunchTime = 7;
    static const int PAddSynthPunchTime = 8;
    static const int PAddSynthPunchStretch = 9;
    static const int PAddSynthPunchVelocity = 10;
    static const int PAddSynthAmpEnv1 = 11;
    static const int PAddSynthAmpEnv2 = 12;
    static const int PAddSynthAmpEnv3 = 13;
    static const int PAddSynthAmpEnv4 = 14;
    static const int PAddSynthAmpEnvStretch = 15;
    static const int PAddSynthAmpLfoFreq = 16;
    static const int PAddSynthAmpLfoIntensity = 17;
    static const int PAddSynthAmpLfoStart = 18;
    static const int PAddSynthAmpLfoDelay = 19;
    static const int PAddSynthAmpLfoStretch = 20;
    static const int PAddSynthAmpLfoRand = 21;
    static const int PAddSynthAmpLfoFreqRand = 22;

    static const int PAddSynthFreqLfoFreq = 23;
    static const int PAddSynthFreqLfoIntensity = 24;
    static const int PAddSynthFreqLfoStart = 25;
    static const int PAddSynthFreqLfoDelay = 26;
    static const int PAddSynthFreqLfoStretch = 27;
    static const int PAddSynthFreqLfoRand = 28;
    static const int PAddSynthFreqLfoFreqRand = 29;


    static const int PAddSynthFilterLfoFreq = 30;
    static const int PAddSynthFilterLfoIntensity = 31;
    static const int PAddSynthFilterLfoStart = 32;
    static const int PAddSynthFilterLfoDelay = 33;
    static const int PAddSynthFilterLfoStretch = 34;
    static const int PAddSynthFilterLfoRand = 35;
    static const int PAddSynthFilterLfoFreqRand = 36;

    static const int PsysEfxSend = 37;

    static const int PAddSynthFreqEnv1 = 40;
    static const int PAddSynthFreqEnv2 = 41;
    static const int PAddSynthFreqEnv3 = 42;
    static const int PAddSynthFreqEnv4 = 43;
    static const int PAddSynthFreqEnv5 = 44;

    static const int PAddFilter1 = 51;
    static const int PAddFilter2 = 52;
    static const int PAddFilter3 = 53;
    static const int PAddFilter4 = 54;
    static const int PAddFilter5 = 55;
    static const int PAddFilter6 = 56;

    static const int  PAddFilterEnv1 = 60;
    static const int  PAddFilterEnv2 = 61;
    static const int  PAddFilterEnv3 = 62;
    static const int  PAddFilterEnv4 = 63;
    static const int  PAddFilterEnv5 = 64;
    static const int  PAddFilterEnv6 = 65;
    static const int  PAddFilterEnv7 = 66;

    static const int PAddVPanning = 69;
    static const int PAddVStereoSpread = 70;
    static const int PAddVVibratto = 71;
    static const int PAddVVibSpeed = 72;

    static const int PaddVAmpEnv1 = 80;
    static const int PaddVAmpEnv2 = 81;
    static const int PaddVAmpEnv3 = 82;
    static const int PaddVAmpEnv4 = 83;
    static const int PaddVAmpEnvStretch = 84;

    static const int PaddVFilterEnv1 = 90;
    static const int PaddVFilterEnv2 = 91;
    static const int PaddVFilterEnv3 = 92;
    static const int PaddVFilterEnv4 = 93;
    static const int PaddVFilterEnv5 = 94;
    static const int PaddVFilterEnv6 = 95;
    static const int PaddVFilterEnv7 = 96;

    static const int PAddVFilter1 = 97;
    static const int PAddVFilter2 = 98;
    static const int PAddVFilter3 = 99;
    static const int PAddVFilter4 = 100;

    static const int PAddVoiceAmpLfoFreq = 101;
    static const int PAddVoiceAmpLfoIntensity = 102;
    static const int PAddVoiceAmpLfoStart = 103;
    static const int PAddVoiceAmpLfoDelay = 104;
    static const int PAddVoiceAmpLfoStretch = 105;
    static const int PAddVoiceAmpLfoRand = 106;
    static const int PAddVoiceAmpLfoFreqRand = 107;

    static const int PaddModAmpEnv1 = 111;
    static const int PaddModAmpEnv2 = 112;
    static const int PaddModAmpEnv3 = 113;
    static const int PaddModAmpEnv4 = 114;
    static const int PaddModAmpEnvStretch = 115;

    static const int PaddVFreqLfoFreq = 123;
    static const int PaddVFreqLfoIntensity = 124;
    static const int PaddVFreqLfoStart = 125;
    static const int PaddVFreqLfoDelay = 126;
    static const int PaddVFreqLfoStretch = 127;
    static const int PaddVFreqLfoRand = 128;
    static const int PaddVFreqLfoFreqRand = 129;

    static const int PaddVFilterLfoFreq = 130;
    static const int PaddVFilterLfoIntensity = 131;
    static const int PaddVFilterLfoStart = 132;
    static const int PaddVFilterLfoDelay = 133;
    static const int PaddVFilterLfoStretch = 134;
    static const int PaddVFilterLfoRand = 135;
    static const int PaddVFilterLfoFreqRand = 136;

    static const int PaddVFreqEnv1 = 140;
    static const int PaddVFreqEnv2 = 141;
    static const int PaddVFreqEnv3 = 142;
    static const int PaddVFreqEnv4 = 143;
    static const int PaddVFreqEnv5 = 144;

    static const int PaddModFreqEnv1 = 150;
    static const int PaddModFreqEnv2 = 151;
    static const int PaddModFreqEnv3 = 152;
    static const int PaddModFreqEnv4 = 153;
    static const int PaddModFreqEnv5 = 154;

    //effects:
    static const int PsysEQgain = 300;
    static const int PsysEQBfreq = 301;
    static const int PsysEQBgain = 302;
    static const int PsysEQBq = 303;

    static const int PsysDis1 = 304;
    static const int PsysDis2 = 305;
    static const int PsysDis3 = 306;
    static const int PsysDis4 = 307;
    static const int PsysDis5 = 308;
    static const int PsysDis6 = 309;
    static const int PsysDis7 = 310;

    static const int PsysAlien0 = 320;
    static const int PsysAlien1 = 321;
    static const int PsysAlien2 = 322;
    static const int PsysAlien3 = 323;
    //static const int PsysAlien4 = 324;
    static const int PsysAlien5 = 325;
    static const int PsysAlien6 = 326;
    static const int PsysAlien7 = 327;
    static const int PsysAlien9 = 329;
    static const int PsysAlien10 = 3210;

    static const int PDynFilter0 = 330;
    static const int PDynFilter1 = 331;
    static const int PDynFilter2 = 332;
    static const int PDynFilter3 = 333;
    static const int PDynFilter4 = 334;
    static const int PDynFilter5 = 335;
    static const int PDynFilter6 = 336;
    static const int PDynFilter7 = 337;
    static const int PDynFilter8 = 338;
    static const int PDynFilter9 = 339;

    static const int PEcho0 = 340;
    static const int PEcho1 = 341;
    static const int PEcho2 = 342;
    static const int PEcho3 = 343;
    static const int PEcho4 = 344;
    static const int PEcho5 = 345;
    static const int PEcho6 = 346;

    static const int PChorus0 = 350;
    static const int PChorus1 = 351;
    static const int PChorus2 = 352;
    static const int PChorus3 = 353;
    static const int PChorus4 = 354;
    static const int PChorus5 = 355;
    static const int PChorus6 = 356;
    static const int PChorus7 = 357;
    static const int PChorus8 = 358;
    static const int PChorus9 = 359;

    static const int PPhaser0 = 360;
    static const int PPhaser1 = 361;
    static const int PPhaser2 = 362;
    static const int PPhaser3 = 363;
    static const int PPhaser4 = 364;
    static const int PPhaser5 = 365;
    static const int PPhaser6 = 366;
    static const int PPhaser7 = 367;
    static const int PPhaser8 = 368;
    static const int PPhaser9 = 369;

    static const int PContrPortamentoTime = 370;
    static const int PContrPortamentoUpDn = 371;
    static const int PContrResonanceDepth = 372;
    static const int PContrResonanceBand = 373;
    static const int PContrBandwidthBand = 374;
    static const int PContrModwheelDepth = 375;
    static const int PContrPanningDepth = 376;
    static const int PContrFilterQDepth = 377;
    static const int PContrFiltercutoffDepth = 378;

    static const int PReverb0 = 380;
    static const int PReverb1 = 381;
    static const int PReverb2 = 382;
    static const int PReverb3 = 383;
    static const int PReverb4 = 384;
    static const int PReverb5 = 385;
    static const int PReverb6 = 386;
    static const int PReverb7 = 387;
    static const int PReverb8 = 388;
    static const int PReverb9 = 389;

};

#endif	/* MIDICONTROLLER_H */

