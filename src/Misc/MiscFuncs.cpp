/*
    MiscFuncs.cpp

    Copyright 2010, Alan Calvert
    Copyright 2014-2018, Will Godfrey

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

    Modifed March 2018
*/

//#define REPORT_MISCMSG

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string.h>
#include <mutex>
#include <limits.h>

using namespace std;

#include "Misc/MiscFuncs.h"

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
   oss.precision(9);
   oss.width(9);
   oss << n;
   return oss.str();
}


string MiscFuncs::asHexString(int x)
{
   ostringstream oss;
   oss << hex << x;
   string res = string(oss.str());
   if (res.length() & 1)
       return "0"+res;
   return res;
}


string MiscFuncs::asHexString(unsigned int x)
{
   ostringstream oss;
   oss << hex << x;
   string res = string(oss.str());
   if (res.length() & 1)
       return "0"+res;
   return res;
}


float MiscFuncs::string2float(string str)
{
    istringstream machine(str);
    float fval;
    machine >> fval;
    return fval;
}


double MiscFuncs::string2double(string str)
{
    istringstream machine(str);
    double dval;
    machine >> dval;
    return dval;
}


int MiscFuncs::string2int(string str)
{
    istringstream machine(str);
    int intval;
    machine >> intval;
    return intval;
}


// ensures MIDI compatible numbers without errors
int MiscFuncs::string2int127(string str)
{
    istringstream machine(str);
    int intval;
    machine >> intval;
    if (intval < 0)
        intval = 0;
    else if (intval > 127)
        intval = 127;
    return intval;
}


unsigned int MiscFuncs::string2uint(string str)
{
    istringstream machine(str);
    unsigned int intval;
    machine >> intval;
    return intval;
}


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


// make a complete path extra legal
void MiscFuncs::legit_pathname(string& fname)
{
    for (unsigned int i = 0; i < fname.size(); ++i)
    {
        char c = fname.at(i);
        if (!((c >= '0' && c <= '9')
              || (c >= 'A' && c <= 'Z')
              || (c >= 'a' && c <= 'z')
              || c == '-'
              || c == '/'
              || c == '.'))
            fname.at(i) = '_';
    }
}

/*
 * This only intended for calls on the local filesystem
 * and to known locations, so buffer size should be adequate
 * and avoids dependency on unreliable macros.
 */
string MiscFuncs::findfile(string path, string filename, string extension)
{
    if (extension.at(0) != '.')
        extension = "." + extension;
    string command = "find " + path + " -name " + filename + extension + " 2>/dev/null -print -quit";
#pragma message "Using '2>/dev/null' here suppresses *all* error messages"
    // it's done here to suppress warnings of invalid locations
    FILE *fp = popen(command.c_str(), "r");
    if (fp == NULL)
        return "";
    char line[1024];
    fscanf(fp,"%[^\n]", line);
    pclose(fp);

    if (findleafname(line) == filename)
        return line;
    return "";
}


string MiscFuncs::findleafname(string name)
{
    unsigned int name_start;
    unsigned int name_end;

    name_start = name.rfind("/");
    name_end = name.rfind(".");
    return name.substr(name_start + 1, name_end - name_start - 1);
}


int MiscFuncs::findSplitPoint(string name)
{
    int chk = 0;
    char ch = name.at(chk);
    while (ch >= '0' and ch <= '9' and chk < 4)
    {
        chk += 1;
        ch = name.at(chk);
    }
    if (ch != '-')
        chk = 0;
    return chk;
}


// adds or replaces wrong extension with the right one.
string MiscFuncs::setExtension(string fname, string ext)
{
    if (ext.at(0) != '.')
        ext = "." + ext;
    string tmp;                         // return value
    size_t ext_pos = fname.rfind('.');  // period, if any
    size_t slash_pos = fname.rfind('/'); // UNIX path-separator
    if (slash_pos == string::npos)
    {
        // There are no slashes in the string, therefore the last period, if
        // any, must be at the position of the extension period.

        ext_pos = fname.rfind('.');
        if (ext_pos == string::npos || ext_pos == 0)
        {
            // There is no period, therefore there is no extension.
            // Append the extension.

            tmp = fname + ext;
        }
        else
        {
            // Replace everything after the last period.

            tmp = fname.substr(0, ext_pos);
            tmp += ext;
        }
    }
    else
    {
        // If the period precedes the slash, then it is not the extension.
        // Add the whole extension.  Otherwise, replace the extension.

        if (slash_pos > ext_pos)
            tmp = fname + ext;
        else
        {
            tmp = fname.substr(0, ext_pos);
            tmp += ext;
        }
    }
    return tmp;
}


bool MiscFuncs::copyFile(string source, string destination)
{
    ifstream infile (source, ios::in|ios::binary|ios::ate);
    if (!infile.is_open())
        return 1;
    ofstream outfile (destination, ios::out|ios::binary);
    if (!outfile.is_open())
        return 1;

    streampos size = infile.tellg();
    char *memblock = new char [size];
    infile.seekg (0, ios::beg);
    infile.read(memblock, size);
    infile.close();
    outfile.write(memblock, size);
    outfile.close();
    delete memblock;
    return 0;
}


bool MiscFuncs::saveText(string text, string filename)
{
    FILE *writefile = fopen(filename.c_str(), "w");
    if (!writefile)
        return 0;

    fputs(text.c_str(), writefile);
    fclose (writefile);
    return 1;
}


int MiscFuncs::loadText(string filename)
{
    FILE *readfile = fopen(filename.c_str(), "r");
    if (!readfile)
        return 0xffff;

    string text = "";
    char line [1024];
    while (!feof(readfile))
    {
        if(fgets(line , 1024 , readfile))
            text += string(line);
    }
    fclose (readfile);
    text.erase(text.find_last_not_of(" \n\r\t")+1);
    return miscMsgPush(text);
}


// replace build directory with a different
// one in the compilation directory
string MiscFuncs::localPath(string leaf)
{
    char *tmpath = getcwd (NULL, 0);
    if (tmpath == NULL)
       return "";

    string path = (string) tmpath;
    free(tmpath);
    size_t found = path.rfind("yoshimi");
    if (found == string::npos)
        return "";

    size_t next = path.find('/', found);
    if (next == string::npos)
        return "";

    return path.substr(0, next) + leaf;
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
    while (buf[newp] > 0x20 && buf[newp] < 0x7f && newp < size && (buf[newp] | 0x20) == (word[newp] | 0x20))
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
    for (int i = 0; i < 255; ++i)
        miscList.push_back("");
    // we use 255 to denote an invalid entry
}

int MiscFuncs::miscMsgPush(string _text)
{
    if (_text.empty())
        return 255;
    sem_wait(&miscmsglock);

    string text = _text;
    list<string>::iterator it = miscList.begin();
    int idx = 0;

    while(it != miscList.end())
    {
        if ( *it == "")
        {
            *it = text;
#ifdef REPORT_MISCMSG
            cout << "Msg In " << int(idx) << " >" << text << "<" << endl;
#endif
            break;
        }
        ++ it;
        ++ idx;
    }
    if (it == miscList.end())
    {
        cerr << "miscMsg list full :(" << endl;
        idx = -1;
    }

    int result = idx; // in case of a new entry before return
    sem_post(&miscmsglock);
    return result;
}


string MiscFuncs::miscMsgPop(int _pos)
{
    if (_pos >= 255)
        return "";
    sem_wait(&miscmsglock);

    int pos = _pos;
    list<string>::iterator it = miscList.begin();
    int idx = 0;

    while(it != miscList.end())
    {
        if (idx == pos)
        {
#ifdef REPORT_MISCMSG
            cout << "Msg Out " << int(idx) << " >" << *it << "<" << endl;
#endif
            break;
        }
        ++ it;
        ++ idx;
    }
    string result = "";
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
    unsigned int bit = 0;
    while (value >>= 1)
    {
        bit ++;
    }
    return bit;
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
