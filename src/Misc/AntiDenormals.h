/*
    AntiDenormals.h

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

// For de-denormalling volume controls, ie, greater than/equal to zero -
inline float dedenormVol(float x) { return (x < 1e-10f) ? 0.0f : x; }

inline float FlushToZero( volatile float f )
{
    f += 9.8607615E-32f;
    return f - 9.8607615E-32f;
}
