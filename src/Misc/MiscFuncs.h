/*
    MiscFuncs.h

    Copyright 2010, Alan Calvert
    Copyright 2014-2019, Will Godfrey

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

    Modified April 2019
*/

#ifndef MISCFUNCS_H
#define MISCFUNCS_H

#include <cmath>
#include <string>
#include <list>
#include <semaphore.h>

#include "globals.h"

static std::list<std::string> miscList;

class MiscFuncs
{
    public:
        MiscFuncs() {sem_init(&miscmsglock, 0, 1);}
        ~MiscFuncs() {sem_destroy(&miscmsglock);}
        std::string asString(int n);
        std::string asString(long long n);
        std::string asString(unsigned long n);
        std::string asString(long n);
        std::string asString(unsigned int n, unsigned int width = 0);
        std::string asString(unsigned char c);// { return asString((unsigned int)c); }
        std::string asString(float n);
        std::string asLongString(float n);
        std::string asHexString(int x);
        std::string asHexString(unsigned int x);
        std::string asAlignedString(int n, int len);

        static float string2float(std::string str);
        static double string2double(std::string str);
        static int string2int(std::string str);
        static int string2int127(std::string str);
        static unsigned int string2uint(std::string str);

        int stringNumInList(std::string toFind, std::string *listname, int convert = 0);

        bool isFifo(std::string chkpath);
        int findSplitPoint(std::string name);

        char *skipSpace(char *buf);
        char *skipChars(char *buf);
        int matchWord(int numChars, char *point, const char *word);
        bool matchnMove(int num, char *&pnt, const char *word);

        void miscMsgInit(void);
        void miscMsgClear(void);
        int miscMsgPush(std::string text);
        std::string miscMsgPop(int pos);

        unsigned int nearestPowerOf2(unsigned int x, unsigned int min, unsigned int max);
        float limitsF(float value, float min, float max);

        unsigned int bitFindHigh(unsigned int value);
        void bitSet(unsigned int& value, unsigned int bit);
        void bitClear(unsigned int& value, unsigned int bit);
        void bitClearHigh(unsigned int& value);
        void bitClearAbove(unsigned int& value, int bitLevel);
        bool bitTest(unsigned int value, unsigned int bit);
        std::string lineInText(std::string text, size_t &point);
        void C_lineInText(std::string text, size_t &point, char *line);

        float dB2rap(float dB);
        float rap2dB(float rap);
        sem_t miscmsglock;
};

void invSignal(float *sig, size_t len);

template<class T>
T limit(T val, T min, T max)
{
    return val < min ? min : (val > max ? max : val);
}

inline float MiscFuncs::dB2rap(float dB) {
#if defined(HAVE_EXP10F)
    return exp10f((dB) / 20.0f);
#else
    return powf(10.0, (dB) / 20.0f);
#endif
}
inline float MiscFuncs::rap2dB(float rap) { return 20.0f * log10f(rap); }

#endif
