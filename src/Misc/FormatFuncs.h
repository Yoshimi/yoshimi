/*
    FormatFuncs.h

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

    Modified August 2019
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


}//(End)namespace func
#endif /*FORMATFUNCS_H*/
