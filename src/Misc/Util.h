/*
    Util.h - generic helpers and abbreviations

    Copyright 2024,  Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the License,
    or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License (version 2
    or later) for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef UTIL_H
#define UTIL_H


namespace util {

/** shortcut to save some typing when having to define const and non-const variants
 *  of member functions or when an information function must access non-const fields */
template<class OBJ>
inline OBJ*
unConst(const OBJ *o)
{
    return const_cast<OBJ*>(o);
}

template<class OBJ>
inline OBJ&
unConst(OBJ const &ro)
{
    return const_cast<OBJ&>(ro);
}

  
template<class N1, class N2>
inline N1 constexpr min(N1 n1, N2 n2)
{
    return n2 < n1 ? N1(n2) : n1;
}

template<class N1, class N2>
inline N1 constexpr max(N1 n1, N2 n2)
{
    return n1 < n2 ? N1(n2) : n1;
}


/** force a numeric to be within bounds, inclusively */
template<typename NUM, typename NB>
inline NUM constexpr limited(NB lowerBound, NUM val, NB upperBound)
{
    return min(max(val, lowerBound), upperBound);
}

template<typename NUM, typename NB>
inline bool constexpr isLimited(NB lowerBound, NUM val, NB upperBound)
{
    return lowerBound <= val and val <= upperBound;
}


} //(End)namespace util
#endif /*UTIL_H*/
