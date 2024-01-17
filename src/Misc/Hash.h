/*
    Hash.h - Helpers for working with hashes, type tags and object identities

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

#ifndef HASH_H
#define HASH_H


#include <cstddef>
#include <typeinfo>


namespace func {


namespace {
    template<bool = (sizeof(size_t) >= 8)>   // for 64bit systems
    struct HashCombineImpl
    {
        static void calc(size_t& seed, size_t hash)
        {
            seed ^= hash + 0x9e3779b97f4a7c15 + (seed << 6) + (seed >> 2);
        }             //   ^^ this is the mantissa of 1/Φ (golden ratio)
    };
    template<>
    struct HashCombineImpl<false>            // fallback: the classic boost impl for 32bit
    {
        static void calc(size_t& seed, size_t hash)
        {
            seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };
    ////////TODO C++17 : once we upgrade, we could use constexpr-if instead of this template switch
}

/**
 * Combine hash values.
 * There is still no solution available in the C++ standard and discussions are ongoing,
 * because it is hard to find a balance between good quality and performance.
 * - See ​Peter Dimov's Answer on Reddit: https://www.reddit.com/r/cpp/comments/1225m8g/comment/jdraigr/
 * - The ​»Unordered Hash Conundrum«: https://web.archive.org/web/20181003190331/https://bajamircea.github.io/coding/cpp/2017/06/09/unordered-hash.html
 * - Stackoverflow: ​boost::hash_combine (not) the best solution: https://stackoverflow.com/a/50978188/444796
 *
 * This function is essentially the boost implementation of hash_combine, with a 32bit / 64bit variant
 * See: https://stackoverflow.com/questions/5889238/why-is-xor-the-default-way-to-combine-hashes#comment83288287_27952689
 */
inline void hash_combine(size_t& seed, size_t hash)
{
    HashCombineImpl<>::calc (seed, hash);
}


/**
* @return a standard hash value, based on the full (mangled) C++ type name
*/
template<typename TY>
inline size_t getTypeHash()
{
    return typeid(TY).hash_code();
}



}//(End)namespace func
#endif /*HASH_H*/
