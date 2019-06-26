/*
 * Data2Text.h
 */

#ifndef DATATEXT_H
#define DATATEXT_H

#include <string>

#include "globals.h"
#include "Misc/MiscFuncs.h"

class SynthEngine;
class MiscFuncs;

class DataText : public MiscFuncs
{

    private:
        SynthEngine *synth;
    public:
        DataText(){ }
        ~DataText(){ };
        std::string resolveAll(SynthEngine *_synth, CommandBlock *getData, bool addValue);
    private:
        std::string resolveVector(CommandBlock *getData);
        std::string resolveMicrotonal(CommandBlock *getData);
        std::string resolveConfig(CommandBlock *getData);
        std::string resolveBank(CommandBlock *getData);
        std::string resolveMain(CommandBlock *getData);
        std::string resolvePart(CommandBlock *getData);
        std::string resolveAdd(CommandBlock *getData);
        std::string resolveAddVoice(CommandBlock *getData);
        std::string resolveSub(CommandBlock *getData);
        std::string resolvePad(CommandBlock *getData);
        std::string resolveOscillator(CommandBlock *getData);
        std::string resolveResonance(CommandBlock *getData);
        std::string resolveLFO(CommandBlock *getData);
        std::string resolveFilter(CommandBlock *getData);
        std::string resolveEnvelope(CommandBlock *getData);
        std::string resolveEffects(CommandBlock *getData);
        bool showValue;
};
#endif
