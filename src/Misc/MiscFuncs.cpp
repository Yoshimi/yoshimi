/*
    MiscFuncs.cpp

    Copyright 2010, Alan Calvert

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

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sstream>
#include <string.h>

using namespace std;

#include "Misc/MiscFuncs.h"

bool MiscFuncs::isRegFile(string chkpath)
{
    struct stat st;
    if (!lstat(chkpath.c_str(), &st))
        if (S_ISREG(st.st_mode))
            return true;
    return false;
}


bool MiscFuncs::isDirectory(string chkpath)
{
    struct stat st;
    if (!lstat(chkpath.c_str(), &st))
        if (S_ISDIR(st.st_mode))
            return true;
    return false;
}


bool MiscFuncs::isFifo(string chkpath)
{
    struct stat st;
    if (!lstat(chkpath.c_str(), &st))
        if (S_ISFIFO(st.st_mode))
            return true;
    return false;
}


float MiscFuncs::string2float(string str)
{
    istringstream machine(str);
    float fval;
    machine >> fval;
    return fval;
}

int MiscFuncs::string2int(string str)
{
    istringstream machine(str);
    int intval;
    machine >> intval;
    return intval;
}

unsigned int MiscFuncs::string2uint(string str)
{
    istringstream machine(str);
    unsigned int intval;
    machine >> intval;
    return intval;
}

// make a filename legal
void MiscFuncs::legit_filename(string& fname)
{
    for (unsigned int i = 0; i < fname.size(); ++i)
    {
        char c = fname.at(i);
        if (!((c >= '0' && c <= '9')
              || (c >= 'A' && c <= 'Z')
              || (c >= 'a' && c <= 'z')
              || c == '-'
              || c == ' '
              || c == '.'))
            fname.at(i) = '_';
    }
}


// replace 'src' with a different one in the compilation directory
string MiscFuncs::localPath(string leaf)
{
    char *tmpath;
    tmpath = (char*) malloc(PATH_MAX);
    getcwd (tmpath, PATH_MAX);
    string path = (string) tmpath;
    size_t found = path.rfind("/src");
    if (found != string::npos)
        path.replace (found,4,leaf);
    else
        path = "";
    free(tmpath);
    return path;
}


void invSignal(float *sig, size_t len)
{
    for(size_t i = 0; i < len; ++i)
        sig[i] *= -1.0f;
}


string MiscFuncs::asString(int n)
{
   ostringstream oss;
   oss << n;
   return string(oss.str());
}


string MiscFuncs::asString(long long n)
{
   ostringstream oss;
   oss << n;
   return string(oss.str());
}


string MiscFuncs::asString(unsigned long n)
{
    ostringstream oss;
    oss << n;
    return string(oss.str());
}
//#endif

string MiscFuncs::asString(long n)
{
   ostringstream oss;
   oss << n;
   return string(oss.str());
}


string MiscFuncs::asString(unsigned int n, unsigned int width)
{
    ostringstream oss;
    oss << n;
    string val = string(oss.str());
    if (width && val.size() < width)
    {
        val = string("000000000") + val;
        return val.substr(val.size() - width);
    }
    return val;
}


string MiscFuncs::asString(unsigned char c)
{
    ostringstream oss;
    oss.width(1);
    oss << c;
    return oss.str();
}

string MiscFuncs::asString(float n)
{
   ostringstream oss;
   oss.precision(3);
   oss.width(3);
   oss << n;
   return oss.str();
}


string MiscFuncs::asLongString(float n)
{
   ostringstream oss;
   oss << n;
   return oss.str();
}


string MiscFuncs::asHexString(int x)
{
   ostringstream oss;
   oss << hex << x;
   return string(oss.str());
}


string MiscFuncs::asHexString(unsigned int x)
{
   ostringstream oss;
   oss << hex << x;
   return string(oss.str());
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
    if (buf[0] == 0x20)
        buf = skipSpace(buf);
    return buf;
}


int MiscFuncs::matchWord(char *buf, const char *word)
{
    int newp = 0;
    int size = strlen(word);
    while (buf[newp] > 0x20 && buf[newp] < 0x7f && newp < size && (buf[newp] | 0x20) == word[newp])
            ++ newp;
    if (newp > 0 && (buf[newp] <= 0x20 || buf[newp] >= 0x7f))
        return newp;
    return 0;
}
