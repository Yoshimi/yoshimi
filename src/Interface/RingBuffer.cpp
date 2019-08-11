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

*/

#include <atomic>
#include <stdlib.h>
#include <iostream>
#include "string.h" // needed for memcpy

#include "Interface/RingBuffer.h"

/*
 * WARNING it is ESSENTIAL that all buffers and
 * data blocks are powers of 2
 *
 * NOTE
 * buffer size is in terms of block size, not bytes
 * while block size is in bytes
 */
ringBuff::ringBuff(uint32_t _bufferSize, uint32_t _blockSize):
    bufferSize(_bufferSize * _blockSize),
    blockSize(_blockSize)
{
    mask = bufferSize - 1;
    buffer = new char[bufferSize];
    //std::cout << "buffer size " << int(bufferSize) << "   block " << int(blockSize) << std::endl;
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
    if (((read - blockSize) & mask) == write)
        return false;
    //std::cout << "write " << write << "  read " << read << std::endl;
    write = (write + blockSize) & mask;
    memcpy((buffer + write), writeData, blockSize);
    writePoint.store(write, std::memory_order_release);
    return true;
}

bool ringBuff::read(char *readData)
{
    uint32_t write = writePoint.load(std::memory_order_relaxed);
    uint32_t read = readPoint.load(std::memory_order_acquire);
    if ((write - read) < blockSize)
        return false;
    //std::cout << "read " << read << "  write " << write << std::endl;
    read = (read + blockSize) & mask;
    memcpy(readData, (buffer + read), blockSize);
    readPoint.store(read, std::memory_order_release);
    return true;
}
