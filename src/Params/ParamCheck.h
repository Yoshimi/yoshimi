/*
    ParamCheck.h - Checks control changes and updates respective parameters

    Copyright 2018-2023, Kristian Amlie, Will Godfrey

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

    This file is derivative of ZynAddSubFX original code.
*/

#ifndef PARAMCHECK_H
#define PARAMCHECK_H

//#include "Misc/XMLwrapper.h"
//#include <vector>

class SynthEngine;

struct Note
{
    int midi;
    float freq;
    float vel;

    Note(int midiNote, float freq, float velocity)
        : midi{midiNote}
        , freq{freq}
        , vel{limitVelocity(velocity)}
    { }
    // copyable and assignable

    Note withFreq(float changedFreq)
    { return Note{midi,changedFreq,vel}; }

private:
    static float limitVelocity(float rawVal)
    { return std::max(0.0f, std::min(rawVal, 1.0f)); }
};


class ParamBase
{
    public:
        ParamBase(SynthEngine *_synth);
        virtual ~ParamBase() { }
        SynthEngine *getSynthEngine() {return synth;}

    private:
        virtual void defaults(int /* n */) { }
        int nelement;

    protected:
        SynthEngine *synth;

    private:
        int updatedAt; // Monotonically increasing counter that tracks last
                       // change.  Users of the parameters compare their last
                       // update to this counter. This can overflow, what's
                       // important is that it's different.

    public:
        class ParamsUpdate
        {
            public:
                ParamsUpdate(ParamBase const& params_) :
                    params(&params_),
                    lastUpdated(params->updatedAt)
                {}

                // Checks if params have been updated and resets counter.
                bool checkUpdated()
                {
                    bool result = params->updatedAt != lastUpdated;
                    lastUpdated = params->updatedAt;
                    return result;
                }

                void forceUpdate()
                {
                    lastUpdated = params->updatedAt - 1;
                }

                void changeParams(ParamBase const& params_)
                {
                    if (params != &params_)
                    {
                        params = &params_;
                        forceUpdate();
                    }
                }

            private:
                const ParamBase *params;
                int lastUpdated;
        };

        void paramsChanged()
        {
            updatedAt++;
            //std::cout << "update " << updatedAt << std::endl;
        }
};

#endif
