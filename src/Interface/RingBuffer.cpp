/*
    RingBuffer.cpp - all buffering

    Copyright 2019 Will Godfrey

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

    Created April 2019
*/

#include <atomic>
#include <stdlib.h>
#include "string.h" // needed for memcpy

#include "Interface/RingBuffer.h"

/*
 * it is ESSENTIAL that all buffers and
 * data blocks are powers of 2
 */
ringBuff::ringBuff(uint32_t _bufferSize, uint32_t _blockSize):
    bufferSize(_bufferSize),
    blockSize(_blockSize)
{
    mask = bufferSize - 1;
    buffer = new char[bufferSize * blockSize];
    //std::cout << "block size " << int(blockSize) << endl;
}

ringBuff::~ringBuff()
{
    delete [] buffer;
    buffer = NULL;
}

bool ringBuff::write(char *writeData)
{
    uint32_t write = writePoint.load(std::memory_order_acquire);
    uint32_t read = readPoint.load(std::memory_order_relaxed);
    if ((write - read) > (bufferSize - blockSize))
        return false;
    //std::cout << "write " << write << "  read " << read << endl;
    memcpy(buffer + blockSize + (write & mask), writeData, blockSize);
    writePoint.store(write + blockSize, std::memory_order_release);
    return true;
}

bool ringBuff::read(char *readData)
{
    uint32_t write = writePoint.load(std::memory_order_relaxed);
    uint32_t read = readPoint.load(std::memory_order_acquire);
    if ((write - read) < blockSize)
        return false;
    //std::cout << "read " << read << "  write " << write << endl;
    memcpy(readData, buffer + blockSize + (read & mask), blockSize);
    readPoint.store(read + blockSize, std::memory_order_release);
    return true;
}
