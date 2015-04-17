/*
    BodyDisposal.cpp

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

#include "Synth/BodyDisposal.h"

void BodyDisposal::addBody(Carcass *body)
{
    if (body != NULL)
        corpses.push_back(body);
}

void BodyDisposal::disposeBodies(void)
{
    for (int x = corpses.size(); x > 0; --x)
        corpses.pop_front();
}
