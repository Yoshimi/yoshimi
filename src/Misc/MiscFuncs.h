/*
    MiscFuncs.h

    Copyright 2010, Alan Calvert

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

#ifndef MISCFUNCS_H
#define MISCFUNCS_H

#include <cmath>
#include <string>

using namespace std;

class MiscFuncs
{
    public:
        MiscFuncs() { }
        ~MiscFuncs() { };
        string asString(int n);
        string asString(long long n);
        string asString(long n);
        string asString(unsigned int n, unsigned int width = 0);
        string asString(unsigned char c) { return asString((unsigned int)c); }
        string asString(float n);
        string asHexString(int x);
        string asHexString(unsigned int x);
        float string2float(string str);
        int string2int(string str);
        bool isRegFile(string chkpath);
        bool isDirectory(string chkpath);
        bool isFifo(string chkpath);
        void legit_filename(string& fname);
        float dB2rap(float dB);
        float rap2dB(float rap);
};

inline float MiscFuncs::dB2rap(float dB) { return exp10f((dB) / 20.0f); }
inline float MiscFuncs::rap2dB(float rap) { return 20.0f * log10f(rap); }

#endif
