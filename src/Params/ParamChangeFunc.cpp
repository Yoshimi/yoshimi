#include "ParamChangeFunc.h"
#include "midiController.h"
#include "Misc/SynthEngine.h"
#include "Misc/Part.h"
#include "ADnoteParameters.h"

parameterStruct::parameterStruct() {
    paramName = parID::PNullParam;
    partN = -1;
    kitItemN = 0;
    voiceN = 0;
    effN = -1;
    EQbandN = 0;
    duplicated = 0;
    label[0] = '\0';
    min = 0;
    max = 127;
    paramPointer = NULL;
    pointerType = 0; //unsigned char*
    paramNumber = 0;
}

//we redefine the == operator, to be able to compare parameterStructs
bool parameterStruct::operator ==(parameterStruct other) {
    if (other.paramName!=this->paramName) return false;
    if (other.partN!=this->partN) return false;
    if (other.kitItemN!=this->kitItemN) return false;
    if (other.voiceN!=this->voiceN) return false;
    if (other.duplicated!=this->duplicated) return false;
    if (other.effN!=this->effN) return false;
    if (other.EQbandN!=this->EQbandN) return false;
    return true;
}

void parameterStruct::add2XML(XMLwrapper* xml) {
    xml->addpar("paramName",paramName);
    xml->addpar("partN",partN);
    xml->addpar("kitItemN",kitItemN);
    xml->addpar("voiceN",voiceN);
    xml->addpar("effN",effN);
    xml->addpar("EQBandN",EQbandN);
    //xml->addpar("duplicated",duplicated); useless
    //xml->addpar("",paramPointer); can't save a pointer
    xml->addpar("pointerType",pointerType);
    xml->addparcharpointer("label",label);
    xml->addpar("paramNumber",paramNumber);
    xml->addpar("min",min);
    xml->addpar("max",max);
}

void parameterStruct::loadFromXML(XMLwrapper *xml) {
    ///TODO:
    paramName = xml->getpar("paramName",-1,-1,999);
    partN = xml->getpar("partN",-1,-1,NUM_MIDI_PARTS);
    kitItemN = xml->getpar("kitItemN",0,0,NUM_KIT_ITEMS);
    voiceN = xml->getpar("voiceN",0,0,NUM_VOICES);
    effN = xml->getpar("effN",-1,-1,NUM_SYS_EFX);
    EQbandN = xml->getpar("EQBandN",0,0,MAX_EQ_BANDS);
    strcpy(label,xml->getparstr("label").c_str());
    min = xml->getpar127("min",0);
    max = xml->getpar127("max",127);
    pointerType = xml->getpar("pointerType",0,-100,100);
    paramNumber = xml->getpar("paramNumber",0,0,999);

    this->setPointerBasedOnParams();

}

void parameterStruct::setPointerBasedOnParams() {
    ADnoteGlobalParam* Gpar = NULL;
    ADnoteVoiceParam* Adpar = NULL;
    if (this->partN!=-1) {
        Gpar = &synth->part[this->partN]->kit[this->kitItemN].adpars->GlobalPar;
        Adpar= &synth->part[this->partN]->kit[this->kitItemN].adpars->VoicePar[this->voiceN];
    }

    EffectMgr* fx;
    //if it's an effect it can be both a global effect or a part effect
    if (this->effN!=-1) { //it's an effect
        if (this->partN==-1) { //it's a global effect
            fx = synth->sysefx[this->effN];
        } else {
            fx = synth->part[this->partN]->partefx[this->effN];
        }
        paramPointer = fx; return;
    }

    /* This switch is generated with the help of this bash script:
     *
     * #!/bin/bash
     * IFS=`echo -en "\n\b"`
     *
     * for line in $(grep -E -i -o '[^,]+,[ ]*parID::[A-Z0-9]+' ./yoshimi/src/Params/midiController.cpp);
     *  do
     *   pointer=$(echo "$line" | grep -E -i -o '&[^,]+?')
     *   parName=$(echo "$line" | grep -E -i -o 'parID.+')
     *   echo "case $parName :    paramPointer = $pointer;break;"
     * done
     */

    switch(paramName) {
        case parID::PMasterVolume :    paramPointer = &synth->Pvolume;break;
        case parID::PMasterDetune :    paramPointer = &synth->microtonal.Pglobalfinedetune;break;
        case parID::PPartPanning :    paramPointer = &synth->part[this->partN]->Ppanning;break;
        case parID::PPartVolume :    paramPointer = &synth->part[this->partN]->Pvolume;break;
        case parID::PAddSynthPan :    paramPointer = &Gpar->PPanning;break;
        case parID::PAddSynthPunchStrength :    paramPointer = &Gpar->PPunchStrength;break;
        case parID::PAddSynthPunchTime :    paramPointer = &Gpar->PPunchTime;break;
        case parID::PAddSynthPunchStretch :    paramPointer = &Gpar->PPunchStretch;break;
        case parID::PAddSynthPunchVelocity :    paramPointer = &Gpar->PPunchVelocitySensing;break;
        case parID::PAddSynthAmpEnv1 :    paramPointer = &Gpar->AmpEnvelope->PA_dt ;break;
        case parID::PAddSynthAmpEnv2 :    paramPointer = &Gpar->AmpEnvelope->PD_dt ;break;
        case parID::PAddSynthAmpEnv3 :    paramPointer = &Gpar->AmpEnvelope->PS_val ;break;
        case parID::PAddSynthAmpEnv4 :    paramPointer = &Gpar->AmpEnvelope->PR_dt ;break;
        case parID::PAddSynthAmpEnvStretch :    paramPointer = &Gpar->AmpEnvelope->Penvstretch ;break;
        case parID::PAddSynthAmpLfoFreq :    paramPointer = &Gpar->AmpLfo->Pfreq ;break;
        case parID::PAddSynthAmpLfoIntensity :    paramPointer = &Gpar->AmpLfo->Pintensity ;break;
        case parID::PAddSynthAmpLfoStart :    paramPointer = &Gpar->AmpLfo->Pstartphase ;break;
        case parID::PAddSynthAmpLfoDelay :    paramPointer = &Gpar->AmpLfo->Pdelay ;break;
        case parID::PAddSynthAmpLfoStretch :    paramPointer = &Gpar->AmpLfo->Pstretch ;break;
        case parID::PAddSynthAmpLfoRand :    paramPointer = &Gpar->AmpLfo->Prandomness ;break;
        case parID::PAddSynthAmpLfoFreqRand :    paramPointer = &Gpar->AmpLfo->Pfreqrand ;break;
        case parID::PAddSynthFreqLfoFreq :    paramPointer = &Gpar->FreqLfo->Pfreq ;break;
        case parID::PAddSynthFreqLfoIntensity :    paramPointer = &Gpar->FreqLfo->Pintensity ;break;
        case parID::PAddSynthFreqLfoStart :    paramPointer = &Gpar->FreqLfo->Pstartphase ;break;
        case parID::PAddSynthFreqLfoDelay :    paramPointer = &Gpar->FreqLfo->Pdelay ;break;
        case parID::PAddSynthFreqLfoStretch :    paramPointer = &Gpar->FreqLfo->Pstretch ;break;
        case parID::PAddSynthFreqLfoRand :    paramPointer = &Gpar->FreqLfo->Prandomness ;break;
        case parID::PAddSynthFreqLfoFreqRand :    paramPointer = &Gpar->FreqLfo->Pfreqrand ;break;
        case parID::PAddSynthFilterLfoFreq :    paramPointer = &Gpar->FilterLfo->Pfreq ;break;
        case parID::PAddSynthFilterLfoIntensity :    paramPointer = &Gpar->FilterLfo->Pintensity ;break;
        case parID::PAddSynthFilterLfoStart :    paramPointer = &Gpar->FilterLfo->Pstartphase ;break;
        case parID::PAddSynthFilterLfoDelay :    paramPointer = &Gpar->FilterLfo->Pdelay ;break;
        case parID::PAddSynthFilterLfoStretch :    paramPointer = &Gpar->FilterLfo->Pstretch ;break;
        case parID::PAddSynthFilterLfoRand :    paramPointer = &Gpar->FilterLfo->Prandomness ;break;
        case parID::PAddSynthFilterLfoFreqRand :    paramPointer = &Gpar->FilterLfo->Pfreqrand ;break;
        case parID::PAddSynthFreqEnv1 :    paramPointer = &Gpar->FreqEnvelope->PA_val ;break;
        case parID::PAddSynthFreqEnv2 :    paramPointer = &Gpar->FreqEnvelope->PA_dt ;break;
        case parID::PAddSynthFreqEnv3 :    paramPointer = &Gpar->FreqEnvelope->PR_dt ;break;
        case parID::PAddSynthFreqEnv4 :    paramPointer = &Gpar->FreqEnvelope->PR_val ;break;
        case parID::PAddSynthFreqEnv5 :    paramPointer = &Gpar->FreqEnvelope->Penvstretch ;break;
        case parID::PAddFilter1 :    paramPointer = &Gpar->GlobalFilter->Pfreq ;break;
        case parID::PAddFilter2 :    paramPointer = &Gpar->GlobalFilter->Pq ;break;
        case parID::PAddFilter3 :    paramPointer = &Gpar->PFilterVelocityScale ;break;
        case parID::PAddFilter4 :    paramPointer = &Gpar->PFilterVelocityScaleFunction ;break;
        case parID::PAddFilter5 :    paramPointer = &Gpar->GlobalFilter->Pfreqtrack ;break;
        case parID::PAddFilter6 :    paramPointer = &Gpar->GlobalFilter->Pgain ;break;
        case parID::PAddFilterEnv1 :    paramPointer = &Gpar->FilterEnvelope->PA_val ;break;
        case parID::PAddFilterEnv2 :    paramPointer = &Gpar->FilterEnvelope->PA_dt ;break;
        case parID::PAddFilterEnv3 :    paramPointer = &Gpar->FilterEnvelope->PD_val ;break;
        case parID::PAddFilterEnv4 :    paramPointer = &Gpar->FilterEnvelope->PD_dt ;break;
        case parID::PAddFilterEnv5 :    paramPointer = &Gpar->FilterEnvelope->PR_dt ;break;
        case parID::PAddFilterEnv6 :    paramPointer = &Gpar->FilterEnvelope->PR_val ;break;
        case parID::PAddFilterEnv7 :    paramPointer = &Gpar->FilterEnvelope->Penvstretch ;break;
        case parID::PAddVPanning :    paramPointer = &Adpar->PPanning ;break;
        case parID::PAddVStereoSpread :    paramPointer = &Adpar->Unison_stereo_spread ;break;
        case parID::PAddVVibratto :    paramPointer = &Adpar->Unison_vibratto ;break;
        case parID::PAddVVibSpeed :    paramPointer = &Adpar->Unison_vibratto_speed ;break;
        case parID::PaddVAmpEnv1 :    paramPointer = &Adpar->AmpEnvelope->PA_dt ;break;
        case parID::PaddVAmpEnv2 :    paramPointer = &Adpar->AmpEnvelope->PD_dt ;break;
        case parID::PaddVAmpEnv3 :    paramPointer = &Adpar->AmpEnvelope->PS_val ;break;
        case parID::PaddVAmpEnv4 :    paramPointer = &Adpar->AmpEnvelope->PR_dt ;break;
        case parID::PaddVAmpEnvStretch :    paramPointer = &Adpar->AmpEnvelope->Penvstretch ;break;
        case parID::PAddVoiceAmpLfoFreq :    paramPointer = &Adpar->AmpLfo->Pfreq ;break;
        case parID::PAddVoiceAmpLfoIntensity :    paramPointer = &Adpar->AmpLfo->Pintensity ;break;
        case parID::PAddVoiceAmpLfoStart :    paramPointer = &Adpar->AmpLfo->Pstartphase ;break;
        case parID::PAddVoiceAmpLfoDelay :    paramPointer = &Adpar->AmpLfo->Pdelay ;break;
        case parID::PAddVoiceAmpLfoStretch :    paramPointer = &Adpar->AmpLfo->Pstretch ;break;
        case parID::PAddVoiceAmpLfoRand :    paramPointer = &Adpar->AmpLfo->Prandomness ;break;
        case parID::PAddVoiceAmpLfoFreqRand :    paramPointer = &Adpar->AmpLfo->Pfreqrand ;break;
        case parID::PAddVFilter1 :    paramPointer = &Adpar->VoiceFilter->Pfreq ;break;
        case parID::PAddVFilter2 :    paramPointer = &Adpar->VoiceFilter->Pq ;break;
        case parID::PAddVFilter3 :    paramPointer = &Adpar->VoiceFilter->Pfreqtrack ;break;
        case parID::PAddVFilter4 :    paramPointer = &Adpar->VoiceFilter->Pgain ;break;
        case parID::PaddVFilterEnv1 :    paramPointer = &Adpar->FilterEnvelope->PA_val ;break;
        case parID::PaddVFilterEnv2 :    paramPointer = &Adpar->FilterEnvelope->PA_dt ;break;
        case parID::PaddVFilterEnv3 :    paramPointer = &Adpar->FilterEnvelope->PD_val ;break;
        case parID::PaddVFilterEnv4 :    paramPointer = &Adpar->FilterEnvelope->PD_dt ;break;
        case parID::PaddVFilterEnv5 :    paramPointer = &Adpar->FilterEnvelope->PR_dt ;break;
        case parID::PaddVFilterEnv6 :    paramPointer = &Adpar->FilterEnvelope->PR_val ;break;
        case parID::PaddVFilterEnv7 :    paramPointer = &Adpar->FilterEnvelope->Penvstretch ;break;
        case parID::PaddVFilterLfoFreq :    paramPointer = &Adpar->FilterLfo->Pfreq ;break;
        case parID::PaddVFilterLfoIntensity :    paramPointer = &Adpar->FilterLfo->Pintensity ;break;
        case parID::PaddVFilterLfoStart :    paramPointer = &Adpar->FilterLfo->Pstartphase ;break;
        case parID::PaddVFilterLfoDelay :    paramPointer = &Adpar->FilterLfo->Pdelay ;break;
        case parID::PaddVFilterLfoStretch :    paramPointer = &Adpar->FilterLfo->Pstretch ;break;
        case parID::PaddVFilterLfoRand :    paramPointer = &Adpar->FilterLfo->Prandomness ;break;
        case parID::PaddVFilterLfoFreqRand :    paramPointer = &Adpar->FilterLfo->Pfreqrand ;break;
        case parID::PaddVFreqEnv1 :    paramPointer = &Adpar->FreqEnvelope->PA_val ;break;
        case parID::PaddVFreqEnv2 :    paramPointer = &Adpar->FreqEnvelope->PA_dt ;break;
        case parID::PaddVFreqEnv3 :    paramPointer = &Adpar->FreqEnvelope->PR_dt ;break;
        case parID::PaddVFreqEnv4 :    paramPointer = &Adpar->FreqEnvelope->PR_val ;break;
        case parID::PaddVFreqEnv5 :    paramPointer = &Adpar->FreqEnvelope->Penvstretch ;break;
        case parID::PaddVFreqLfoFreq :    paramPointer = &Adpar->FreqLfo->Pfreq ;break;
        case parID::PaddVFreqLfoIntensity :    paramPointer = &Adpar->FreqLfo->Pintensity ;break;
        case parID::PaddVFreqLfoStart :    paramPointer = &Adpar->FreqLfo->Pstartphase ;break;
        case parID::PaddVFreqLfoDelay :    paramPointer = &Adpar->FreqLfo->Pdelay ;break;
        case parID::PaddVFreqLfoStretch :    paramPointer = &Adpar->FreqLfo->Pstretch ;break;
        case parID::PaddVFreqLfoRand :    paramPointer = &Adpar->FreqLfo->Prandomness ;break;
        case parID::PaddVFreqLfoFreqRand :    paramPointer = &Adpar->FreqLfo->Pfreqrand ;break;
        case parID::PaddModAmpEnv1 :    paramPointer = &Adpar->FMAmpEnvelope->PA_dt ;break;
        case parID::PaddModAmpEnv2 :    paramPointer = &Adpar->FMAmpEnvelope->PD_dt ;break;
        case parID::PaddModAmpEnv3 :    paramPointer = &Adpar->FMAmpEnvelope->PS_val ;break;
        case parID::PaddModAmpEnv4 :    paramPointer = &Adpar->FMAmpEnvelope->PR_dt ;break;
        case parID::PaddModAmpEnvStretch :    paramPointer = &Adpar->FMAmpEnvelope->Penvstretch ;break;
        case parID::PaddModFreqEnv1 :    paramPointer = &Adpar->FMFreqEnvelope->PA_val ;break;
        case parID::PaddModFreqEnv2 :    paramPointer = &Adpar->FMFreqEnvelope->PA_dt ;break;
        case parID::PaddModFreqEnv3 :    paramPointer = &Adpar->FMFreqEnvelope->PR_dt ;break;
        case parID::PaddModFreqEnv4 :    paramPointer = &Adpar->FMFreqEnvelope->PR_val ;break;
        case parID::PaddModFreqEnv5 :    paramPointer = &Adpar->FMFreqEnvelope->Penvstretch ;break;

    }
}