/*
    Fader.cpp

    Copyright 2009, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>

using namespace std;

#include "Effects/Fader.h"
#include "Misc/Util.h"

// Pseudo logarithmic volume control, with ack to
// http://www.maazl.de/project/pm123/index.html#logvolum_1.0.
// Scaling factor should not exceed sqrt(10) (+10dB)
// maxmultiplier 2.0 => 0 .. +6db gain
// maxmultiplier 4.0 => 0 .. +12db gain
// ...
Fader::Fader(double maxmultiplier) :
    scalefactor(3.16227766)
{
    double xval = 0.0;
    double step = 1.0 / 127.0; // control range is 0..127
    for (int x = 0; x < 128; ++x)
    {
        scaler[x] = (maxmultiplier * xval / (1.0 + scalefactor * (1.0 - xval)));
        xval += step;
    }
    // assert top & bottom
    scaler[0] = 0.0;
    scaler[127] = maxmultiplier;
}
