/*
    MiscFuncs.h

    Copyright 2010, Alan Calvert
    Copyright 2014-2017, Will Godfrey

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

    Modifed February 2017
*/

#ifndef MISCFUNCS_H
#define MISCFUNCS_H

#include <cmath>
#include <string>
#include <list>
#include <semaphore.h>

using namespace std;

static list<string> miscList;

class MiscFuncs
{
    public:
        MiscFuncs() {sem_init(&miscmsglock, 0, 1);}
        ~MiscFuncs() {sem_destroy(&miscmsglock);}
        string asString(int n);
        string asString(long long n);
        string asString(unsigned long n);
        string asString(long n);
        string asString(unsigned int n, unsigned int width = 0);
        string asString(unsigned char c);// { return asString((unsigned int)c); }
        string asString(float n);
        string asLongString(float n);
        string asHexString(int x);
        string asHexString(unsigned int x);

        static float string2float(string str);
        static double string2double(string str);
        static int string2int(string str);
        static int string2int127(string str);
        static unsigned int string2uint(string str);

        bool isRegFile(string chkpath);
        bool isDirectory(string chkpath);
        bool isFifo(string chkpath);
        void legit_filename(string& fname);
        void legit_pathname(string& fname);
        string findfile(string path, string filename, string extension);
        string findleafname(string name);
        int findSplitPoint(string name);
        string setExtension(string fname, string ext);
        string localPath(string leaf);

        char *skipSpace(char *buf);
        char *skipChars(char *buf);
        int matchWord(int numChars, char *point, const char *word);
        bool matchnMove(int num, char *&pnt, const char *word);

        void miscMsgInit(void);
        int miscMsgPush(string text);
        string miscMsgPop(int pos);

        unsigned int nearestPowerOf2(unsigned int x, unsigned int min, unsigned int max);
        float limitsF(float value, float min, float max);

        unsigned int bitFindHigh(unsigned int value);
        void bitSet(unsigned int& value, unsigned int bit);
        void bitClear(unsigned int& value, unsigned int bit);
        bool bitTest(unsigned int value, unsigned int bit);

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

union CommandBlock{
    struct{
        float value;
        unsigned char type;
        unsigned char control;
        unsigned char part;
        unsigned char kit;
        unsigned char engine;
        unsigned char insert;
        unsigned char parameter;
        unsigned char par2;
    } data;
    struct{
        float value;
        unsigned char type;
        unsigned char control;
        short int min;
        short int max;
        short int def;
    } limits;
    char bytes [sizeof(data)];
};

#endif
