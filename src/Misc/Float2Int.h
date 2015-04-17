/*
    Float2Int.h

    Copyright 2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FLOAT2INT_H
#define FLOAT2INT_H

//#include <cmath>

class Float2Int {
    public:
        Float2Int() { }
        ~Float2Int() { }

    protected:
        const int float2int(const float val) const;
};

inline const int Float2Int::float2int(const float val) const
    { return (int)((isgreater(val, 0.0f)) ? (int)truncf(val) : (int)truncf((val - 1.0f))); }
    // for rationale, see <http://www.mega-nerd.com/FPcast/>

#endif

//{ return (int)((val > 0.0f) ? (int)truncf(val) : (int)truncf((val - 1.0f))); }
