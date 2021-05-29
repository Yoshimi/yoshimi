/*
    TestInvoker.h - invoke sound synthesis for automated testing

    Copyright 2021, Hermann Voßeler

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
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

#ifndef TESTINVOKER_H
#define TESTINVOKER_H

#include <string>
#include <memory>
//#include <sys/types.h>
//#include <stdint.h>
//#include <cstdlib>

#include "Misc/SynthEngine.h"

//using std::memset;
using std::string;

class SynthEngine;


class TestInvoker
{
    int chan;
    int pitch;
    float duration;
    uint repetitions;
    size_t chunksize;

    using BufferHolder = std::unique_ptr<float[]>;
    
    public:
        TestInvoker();

        void performSoundCalculation(SynthEngine&);

    private:
        void awaitMute(SynthEngine&);
        void allocate(BufferHolder&);
        void pullSound(SynthEngine&, BufferHolder&);
};



#endif /*TESTINVOKER_H*/
