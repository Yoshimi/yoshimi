/*
    MiscFuncs.cpp

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
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>

    Modifed April 2019
*/

//#define REPORT_MISCMSG
// for testing message list leaks

#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <string.h>
#include <limits.h>

#include "Misc/MiscFuncs.h"

std::string MiscFuncs::asString(int n)
{
   std::ostringstream oss;
   oss << n;
   return std::string(oss.str());
}


std::string MiscFuncs::asString(long long n)
{
   std::ostringstream oss;
   oss << n;
   return std::string(oss.str());
}


std::string MiscFuncs::asString(unsigned long n)
{
    std::ostringstream oss;
    oss << n;
    return std::string(oss.str());
}


std::string MiscFuncs::asString(long n)
{
   std::ostringstream oss;
   oss << n;
   return std::string(oss.str());
}


std::string MiscFuncs::asString(unsigned int n, unsigned int width)
{
    std::ostringstream oss;
    oss << n;
    std::string val = std::string(oss.str());
    if (width && val.size() < width)
    {
        val = std::string("000000000") + val;
        return val.substr(val.size() - width);
    }
    return val;
}


std::string MiscFuncs::asString(unsigned char c)
{
    std::ostringstream oss;
    oss.width(1);
    oss << c;
    return oss.str();
}


std::string MiscFuncs::asString(float n)
{
   std::ostringstream oss;
   oss.precision(3);
   oss.width(3);
   oss << n;
   return oss.str();
}


std::string MiscFuncs::asLongString(float n)
{
   std::ostringstream oss;
   oss.precision(9);
   oss.width(9);
   oss << n;
   return oss.str();
}


std::string MiscFuncs::asHexString(int x)
{
   std::ostringstream oss;
   oss << std::hex << x;
   std::string res = std::string(oss.str());
   if (res.length() & 1)
       return "0"+res;
   return res;
}


std::string MiscFuncs::asHexString(unsigned int x)
{
   std::ostringstream oss;
   oss << std::hex << x;
   std::string res = std::string(oss.str());
   if (res.length() & 1)
       return "0"+res;
   return res;
}


std::string MiscFuncs::asAlignedString(int n, int len)
{
    std::string res = std::to_string(n);
    int size = res.length();
    if (size < len)
    {
        for (int i = size; i < len; ++ i)
            res = " " + res;
    }
    return res;
}


float MiscFuncs::string2float(std::string str)
{
    std::istringstream machine(str);
    float fval;
    machine >> fval;
    return fval;
}


double MiscFuncs::string2double(std::string str)
{
    std::istringstream machine(str);
    double dval;
    machine >> dval;
    return dval;
}


int MiscFuncs::string2int(std::string str)
{
    std::istringstream machine(str);
    int intval;
    machine >> intval;
    return intval;
}


// ensures MIDI compatible numbers without errors
int MiscFuncs::string2int127(std::string str)
{
    std::istringstream machine(str);
    int intval;
    machine >> intval;
    if (intval < 0)
        intval = 0;
    else if (intval > 127)
        intval = 127;
    return intval;
}


unsigned int MiscFuncs::string2uint(std::string str)
{
    std::istringstream machine(str);
    unsigned int intval;
    machine >> intval;
    return intval;
}


int MiscFuncs::stringNumInList(std::string toFind, std::string *listname, int convert)
{
    std::string copy = "";
    switch (convert)
    {
        case -1:
            for (std::string::size_type i = 0; i < toFind.length(); ++i)
                copy += (char) tolower(toFind[i]);
            break;
        case 1:
            for (std::string::size_type i = 0; i < toFind.length(); ++i)
                copy+= (char) toupper(toFind[i]);
            break;
        default:
            copy = toFind;
            break; // change nothing
    }
    int count = -1;
    std::string name;
    bool found = false;
    do
    {
        ++ count;
        name = listname[count];
        if (copy == name)
            found = true;
    }
    while (!found && name != "end");
    if (name == "end")
        return -1;
    return count;
}


// This is never called !
bool MiscFuncs::isFifo(std::string chkpath)
{
    struct stat st;
    if (!stat(chkpath.c_str(), &st))
        if (S_ISFIFO(st.st_mode))
            return true;
    return false;
}


// this is not actualy a file operation so stays here
int MiscFuncs::findSplitPoint(std::string name)
{
    unsigned int chk = 0;
    char ch = name.at(chk);
    unsigned int len =  name.length() - 1;
    while (ch >= '0' and ch <= '9' and chk < len)
    {
        chk += 1;
        ch = name.at(chk);
    }
    if (chk >= len)
        return 0;
    if (ch != '-')
        return 0;
    return chk;
}


char *MiscFuncs::skipSpace(char *buf)
{
    while (buf[0] == 0x20)
    {
        ++ buf;
    }
    return buf;
}


char *MiscFuncs::skipChars(char *buf)
{
    while (buf[0] > 0x20) // will also stop on line ends
    {
        ++ buf;
    }
    if (buf[0] == 0x20) // now find the next word (if any)
        buf = skipSpace(buf);
    return buf;
}


int MiscFuncs::matchWord(int numChars, char *buf, const char *word)
{
    int newp = 0;
    int size = strlen(word);
    while (buf[newp] > 0x20 && buf[newp] < 0x7f && newp < size && (tolower(buf[newp])) == (tolower(word[newp])))
            ++ newp;
    if (newp >= numChars && (buf[newp] <= 0x20 || buf[newp] >= 0x7f))
        return newp;
    return 0;
}


bool MiscFuncs::matchnMove(int num , char *&pnt, const char *word)
{
 bool found = matchWord(num, pnt, word);
 if (found)
     pnt = skipChars(pnt);
 return found;
}


std::string MiscFuncs::lineInText(std::string text, size_t &point)
{
    size_t len = text.length();
    if (point >= len - 1)
        return "";
    size_t it = 0;
    while (it < len - point && text.at(point + it) >= ' ')
        ++it;
    std::string line = text.substr(point, it);
    point += (it + 1);
    return line;
}


void MiscFuncs::C_lineInText(std::string text, size_t &point, char *line)
{
    std::string found = lineInText(text, point);
    if (found == "")
        line[0] = 0;
    else
        strcpy(line, found.c_str());
}


/*
 * These functions provide a transparent text messaging system.
 * Calling functions only need to recognise integers and strings.
 *
 * Pop is destructive. No two functions should ever have been given
 * the same 'live' ID, but if they do, the second one will get an
 * empty string.
 *
 * Both calls will block, but should be very quick;
 *
 * Normally a message will clear before the next one arrives so the
 * message numbers should remain very low even over multiple instances.
 */
void MiscFuncs::miscMsgInit()
{
    for (int i = 0; i < NO_MSG; ++i)
        miscList.push_back("");
    // we use 255 to denote an invalid entry
}

void MiscFuncs::miscMsgClear()
{ // catches messge leaks - shirley knot :@)
    std::list<std::string>::iterator it = miscList.begin();
    for (it = miscList.begin(); it != miscList.end(); ++it)
        *it = "";
}

int MiscFuncs::miscMsgPush(std::string _text)
{
    if (_text.empty())
        return NO_MSG;
    sem_wait(&miscmsglock);

    std::string text = _text;
    std::list<std::string>::iterator it = miscList.begin();
    int idx = 0;

    while(it != miscList.end())
    {
        if ( *it == "")
        {
            *it = text;
#ifdef REPORT_MISCMSG
            std::cout << "Msg In " << int(idx) << " >" << text << "<" << std::endl;
#endif
            break;
        }
        ++ it;
        ++ idx;
    }
    if (it == miscList.end())
    {
        std::cerr << "miscMsg list full :(" << std::endl;
        idx = -1;
    }

    int result = idx; // in case of a new entry before return
    sem_post(&miscmsglock);
    return result;
}


std::string MiscFuncs::miscMsgPop(int _pos)
{
    if (_pos >= NO_MSG)
        return "";
    sem_wait(&miscmsglock);

    int pos = _pos;
    std::list<std::string>::iterator it = miscList.begin();
    int idx = 0;

    while(it != miscList.end())
    {
        if (idx == pos)
        {
#ifdef REPORT_MISCMSG
            std::cout << "Msg Out " << int(idx) << " >" << *it << "<" << std::endl;
#endif
            break;
        }
        ++ it;
        ++ idx;
    }
    std::string result = "";
    if (idx == pos)
    {
        swap (result, *it); // in case of a new entry before return
    }
    sem_post(&miscmsglock);
    return result;
}


// no more than 32 bit please!
unsigned int MiscFuncs::nearestPowerOf2(unsigned int x, unsigned int min, unsigned int max)
{
    if (x <= min)
        return min;
    if (x >= max)
        return max;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}


float MiscFuncs::limitsF(float value, float min, float max)
{
    if (value > max)
        value = max;
    else if (value < min)
        value = min;
    return value;
}


unsigned int MiscFuncs::bitFindHigh(unsigned int value)
{
    int bit = 32;
    while (bit >= 0)
    {
        bit --;
        if ((value >> bit) == 1)
            return bit;
    }
    return 0xff;
}


void MiscFuncs::bitSet(unsigned int& value, unsigned int bit)
{
    value |= (1 << bit);
}


void MiscFuncs::bitClear(unsigned int& value, unsigned int bit)
{
    unsigned int mask = -1;
    mask ^= (1 << bit);
    value &= mask;
}


void MiscFuncs::bitClearHigh(unsigned int& value)
{
    bitClear(value, bitFindHigh(value));
}


void MiscFuncs::bitClearAbove(unsigned int& value, int bitLevel)
{
    unsigned int mask = (0xffffffff << bitLevel);
    value = (value & ~mask);
}

bool MiscFuncs::bitTest(unsigned int value, unsigned int bit)
{
    if (value & (1 << bit))
        return true;
    return false;
}


void invSignal(float *sig, size_t len)
{
    for(size_t i = 0; i < len; ++i)
        sig[i] *= -1.0f;
}
