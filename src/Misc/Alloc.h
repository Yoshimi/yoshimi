/*
    Alloc.h - managing size allocations

    Copyright 2022,  Ichthyostega

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

#ifndef MISC_ALLOC_H
#define MISC_ALLOC_H

#include <memory>


/* ===== Managing Sample Buffers with unique ownership ===== */

/* Explanation: this is a "smart-handle" to manage the allocation of sample data.
 * - Can be used as *drop-in replacement* for a bare float* or float[]
 * - Can not be copied, only moved. This enforces a single owner of the allocation.
 * - A class holding this handle can likewise not be copied, unless explicitly coded.
 * - Usually, it should be created with a given size, causing appropriate allocation.
 * - By default the handle is created *empty*; this can be tested by bool evaluation.
 * - The function reset(size_t) discards the existing allocation and possibly allocates
 *   a new buffer of the given size (or returns to empty state)
 * - Provides an overloaded subscript operator for array-style access;
 *   the embedded raw pointer can be retrieved with get()
 * - Automatically deallocates memory when instance goes out of scope, for whatever reason.
 *
 * The implementation is based on std::unique_ptr and is thus zero-overhead in comparison
 * to a bare pointer, when compiled with optimisation. Note however that the buffer is
 * always zero initialised (the existing code used to do that after allocation anyway)
 */

class Samples
    : public std::unique_ptr<float[]>
{
    using _unique_ptr = std::unique_ptr<float[]>;

    static float* allocate(size_t elemCnt)
    {
        return elemCnt == 0? nullptr // allow to create empty Data holder
                           : new float[elemCnt]{0};  // NOTE: zero-init
    }

public:
    Samples(size_t buffSize =0)
        : _unique_ptr{allocate(buffSize)}
    { }
    // can be moved, but not copied or assigned
    Samples(Samples&&)                 = default;
    Samples(Samples const&)            = delete;
    Samples& operator=(Samples&&)      = delete;
    Samples& operator=(Samples const&) = delete;

    /** discard existing allocation and possibly create/manage new allocation */
    void reset(size_t newSize =0)
    {
        _unique_ptr::reset(allocate(newSize));
    }
};


#endif /*MISC_ALLOC_H*/
