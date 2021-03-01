/*
    FormatFuncs.h

    Copyright 2010, Alan Calvert
    Copyright 2014-2021, Will Godfrey and others.

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

#ifndef FORMATFUNCS_H
#define FORMATFUNCS_H

#include <cmath>
#include <string>
#include <sstream>
#include <cstring>


namespace func {


inline std::string asString(int n)
{
   std::ostringstream oss;
   oss << n;
   return std::string(oss.str());
}


inline std::string asString(long long n)
{
   std::ostringstream oss;
   oss << n;
   return std::string(oss.str());
}


inline std::string asString(unsigned long n)
{
    std::ostringstream oss;
    oss << n;
    return std::string(oss.str());
}


inline std::string asString(long n)
{
   std::ostringstream oss;
   oss << n;
   return std::string(oss.str());
}


inline std::string asString(unsigned int n)
{
   std::ostringstream oss;
   oss << n;
   return std::string(oss.str());
}


inline std::string asString(unsigned int n, unsigned int width)
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


inline std::string asString(unsigned char c)
{
    std::ostringstream oss;
    oss.width(1);
    oss << c;
    return oss.str();
}


inline std::string asString(float n)
{
   std::ostringstream oss;
   oss.precision(3);
   oss.width(3);
   oss << n;
   return oss.str();
}


inline std::string asLongString(float n)
{
   std::ostringstream oss;
   oss.precision(9);
   oss.width(9);
   oss << n;
   return oss.str();
}


inline std::string asHexString(int x)
{
   std::ostringstream oss;
   oss << std::hex << x;
   std::string res = std::string(oss.str());
   if (res.length() & 1)
       return "0"+res;
   return res;
}


inline std::string asHexString(unsigned int x)
{
   std::ostringstream oss;
   oss << std::hex << x;
   std::string res = std::string(oss.str());
   if (res.length() & 1)
       return "0"+res;
   return res;
}



inline float string2float(std::string str)
{
    std::istringstream machine(str);
    float fval;
    machine >> fval;
    return fval;
}


inline double string2double(std::string str)
{
    std::istringstream machine(str);
    double dval;
    machine >> dval;
    return dval;
}


inline int string2int(std::string str)
{
    std::istringstream machine(str);
    int intval;
    machine >> intval;
    return intval;
}


/* ensures MIDI compatible numbers without errors */
inline int string2int127(std::string str)
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


inline unsigned int string2uint(std::string str)
{
    std::istringstream machine(str);
    unsigned int intval;
    machine >> intval;
    return intval;
}


inline string stringCaps(std::string str, int count)
{
    int idx = 0;
    char c;
    while (str[idx])
    {
        c = str[idx];
        if (idx < count)
            str.replace(idx, 1, 1, toupper(c));
        else
            str.replace(idx, 1, 1, tolower(c));
        idx ++;
    }
    return str;
}


/* this is not actually a file operation so we keep it here */
inline int findSplitPoint(std::string name)
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

/*
 * This is principally used to format strings for the GUI
 * where they are fitted into windows with limited width.
 * However, it may be useful elsewhere.
 */
inline std::string formatTextLines(std::string text, size_t maxLen)
{
    size_t totalLen = text.length();
    if (totalLen < maxLen)
        return text;
    size_t pos = 0;
    size_t ref = 0;
    size_t setLen = totalLen;
    while (pos < setLen) // split overlong words first
    {
        if (text.at(pos) == '\n' || text.at(pos) == ' ')
            ref = pos;
        if ((pos - ref) > maxLen)
        {
            text.insert(pos, 1, '\n');
            ++setLen;
            ref = pos;
        }
        ++pos;
    }
    pos = 0;
    ref = 0;
    size_t last = 0;
    while (pos < totalLen)
    {
        if (text.at(pos) == '\n') // skip over existing newlines
        {
            ref = pos;
            ++pos;
        }
        size_t found = text.find(' ', pos);
        if (found >= totalLen)
            return text;
        if ((found - ref) >= maxLen)
        {
            text.replace(last, 1, 1, '\n');
            ref = last;
        }
        last = found;
        ++ pos;
    }
    return text;
}


inline std::string nextLine(std::string& list) // this is destructive
{ // currently only used in main
    size_t pos = list.find('\n');
    std::string line = "";
    if (pos == std::string::npos)
    {
        line = list;
        list = "";
    }
    else
    {
        line = list.substr(0, pos);
        ++pos;
        if (pos > list.size())
            list = "";
        else
            list = list.substr(pos);
    }
    return line;
}

}//(End)namespace func
#endif /*FORMATFUNCS_H*/
