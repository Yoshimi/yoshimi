/*
    Data2Text.h - conversion of commandBlock entries to text

    Copyright 2019-2020, Will Godfrey
    Copyright 2024, Kristian Amlie, Will Godfrey

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
 */

#ifndef DATATEXT_H
#define DATATEXT_H

#include <optional>
#include <string>

#include "globals.h"

class SynthEngine;
class TextMsgBuffer;


class DataText
{

    private:
        bool showValue;
        bool yesno;
    protected:
        TextMsgBuffer& textMsgBuffer;

    public:
        DataText();

        std::string resolveAll(SynthEngine&, CommandBlock&, bool addValue);
    private:
        std::string withValue(std::string resolved, uchar type, bool showValue, bool addValue, float value, std::optional<float> extra_value = std::nullopt);
        std::string resolveVector(CommandBlock&, bool addValue);
        std::string resolveMicrotonal(CommandBlock&, bool addValue);
        std::string resolveConfig(SynthEngine&, CommandBlock&, bool addValue);
        std::string resolveBank(CommandBlock&, bool addValue);
        std::string resolveMain(CommandBlock&, bool addValue);
        std::string resolveAftertouch(bool type, int value, bool addValue);
        std::string resolvePart(CommandBlock&, bool addValue);
        std::string resolveAdd(CommandBlock&, bool addValue);
        std::string resolveAddVoice(CommandBlock&, bool addValue);
        std::string resolveSub(CommandBlock&, bool addValue);
        std::string resolvePad(SynthEngine&, CommandBlock&, bool addValue);
        std::string resolveOscillator(SynthEngine&, CommandBlock&, bool addValue);
        std::string resolveResonance(SynthEngine&, CommandBlock&, bool addValue);
        std::string resolveLFO(CommandBlock&, bool addValue);
        std::string resolveFilter(CommandBlock&, bool addValue);
        std::string filterControl(CommandBlock&, bool addValue);
        std::string resolveEnvelope(CommandBlock&, bool addValue);
        std::string resolveEffects(CommandBlock&, bool addValue);

        int mapFromEffectNumber(int effectIndex, const int list []);
};
#endif
