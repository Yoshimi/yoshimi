#ifndef RINGBUFFER_H
#define RINGBUFFER_H
/*
    Ring Buffer - all buffering operations

    Previous (2019) design Will Godfrey

    Copyright (C)  2021    Rainer Hans Liffers, Carnarvon, Western Australia
                       Email: rainer.liffers@gmail.com


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

#include <algorithm>
#include <array>
#include <atomic>

template <const size_t log2Blocks, const size_t log2Bytes>
class RingBuffer final : private std::array <char, (1 << log2Blocks) * (1 << log2Bytes)>
  {
    private:

    static constexpr size_t bytes = 1 << log2Bytes;
    static constexpr uint32_t mask = (1 << log2Blocks) * bytes - 1;

    mutable std::atomic <uint32_t> readPoint;
    std::atomic <uint32_t> writePoint;

    public:

    RingBuffer ():
               std::array <char, (1 << log2Blocks) * bytes> (),
               readPoint (0),
               writePoint (0)
      { }

    inline void init ()
      {
        this -> fill (0);
      }

    bool write (const char * writeData);

    bool read (char * readData) const;
  };


template <const size_t log2Blocks, const size_t log2Bytes>
bool RingBuffer <log2Blocks, log2Bytes>::write
                                         (const char * writeData)
  {
    bool result = false;
    uint32_t write = writePoint.load (std::memory_order_acquire);
    uint32_t read = readPoint.load (std::memory_order_relaxed);
    if (((read - bytes) & mask) != write)
      {
        write = (write + bytes) & mask;
        std::copy_n (writeData, bytes, & (* this) [write]);
        writePoint.store (write, std::memory_order_release);
        result = true;
      }
    return result;
  }


template <const size_t log2Blocks, const size_t log2Bytes>
bool RingBuffer <log2Blocks, log2Bytes>::read
                                         (char * readData) const
  {
    bool result = false;
    uint32_t write = writePoint.load (std::memory_order_relaxed);
    uint32_t read = readPoint.load (std::memory_order_acquire);
    if ((write - read) >= bytes)
      {
        read = (read + bytes) & mask;
        std::copy_n (& (* this) [read], bytes, readData);
        readPoint.store (read, std::memory_order_release);
        result = true;
      }
    return result;
  }

#endif
