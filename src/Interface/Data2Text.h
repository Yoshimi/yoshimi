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
        std::string resolveVector(CommandBlock *getData, bool addValue);
        std::string resolveMicrotonal(CommandBlock *getData, bool addValue);
        std::string resolveConfig(CommandBlock *getData, bool addValue);
        std::string resolveBank(CommandBlock *getData, bool addValue);
        std::string resolveMain(CommandBlock *getData, bool addValue);
        std::string resolvePart(CommandBlock *getData, bool addValue);
        std::string resolveAdd(CommandBlock *getData, bool addValue);
        std::string resolveAddVoice(CommandBlock *getData, bool addValue);
        std::string resolveSub(CommandBlock *getData, bool addValue);
        std::string resolvePad(CommandBlock *getData, bool addValue);
        std::string resolveOscillator(CommandBlock *getData, bool addValue);
        std::string resolveResonance(CommandBlock *getData, bool addValue);
        std::string resolveLFO(CommandBlock *getData, bool addValue);
        std::string resolveFilter(CommandBlock *getData, bool addValue);
        std::string resolveEnvelope(CommandBlock *getData, bool addValue);
        std::string resolveEffects(CommandBlock *getData, bool addValue);
        bool showValue;
};
#endif
