/*
    Vectors.h - Vector control

    Copyright 2024, Will Godfrey

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

#ifndef VECTORS_H
#define VECTORS_H

#include "globals.h"

class SynthEngine;
class XMLtree;

using std::string;


class Vectors
{
    public:
        Vectors(SynthEngine*);
        CommandBlock data;//commandData;

        uchar loadVectorAndUpdate(uchar baseChan, string const& name);
        uchar loadVector(uchar baseChan, string const& name, bool full);
        uchar extractVectorData(uchar baseChan, XMLtree&, string const& name);

        uchar saveVector(uchar baseChan, string const& name, bool full);
        bool  insertVectorData(uchar baseChan, bool full, XMLtree&, string const& name);

        float getVectorLimits(CommandBlock *getData);

    private:
        SynthEngine& synth;
};

#endif /*VECTORS_H*/

