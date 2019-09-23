/*
    FileMgr.h - all file operations

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

#ifndef FILEMGR_H
#define FILEMGR_H

#include <cerrno>
#include <fcntl.h> // this affects error reporting
#include <sys/stat.h>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <zlib.h>

#include "globals.h"

namespace EXTEN {

using std::string;

const string config =        ".config";
const string instance =      ".instance";
const string bank =          ".banks";
const string history =       ".history";
const string zynInst =       ".xiz";
const string yoshInst =      ".xiy";
const string anyInst =       ".xi*";
const string patchset =      ".xmz";
const string state =         ".state";
const string scale =         ".xsz";
const string scalaTuning =   ".scl";
const string scalaKeymap =   ".kbm";
const string vector =        ".xvy";
const string mlearn =        ".xly";
const string MSwave=         ".wav";
const string window =        ".windows";

}//(End)namespace EXTEN


namespace file {

using std::string;
using std::stringstream;


inline bool TestFunc(int result)
{
    std::cout << "***\nTest Function " << result << "\n***" << std::endl;
    return (result > 0);
}


// make a filename legal
inline void make_legit_filename(string& fname)
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
inline void make_legit_pathname(string& fname)
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


inline bool isRegularFile(string chkpath)
{
    struct stat st;
    if (!stat(chkpath.c_str(), &st))
        if (S_ISREG(st.st_mode))
            return true;
    return false;
}


inline bool isDirectory(string chkpath)
{
    struct stat st;
    if (!stat(chkpath.c_str(), &st))
        if (S_ISDIR(st.st_mode))
            return true;
    return false;
}


/*
 * This is only intended for calls on the local filesystem
 * and to known locations, so buffer size should be adequate
 * and it avoids dependency on unreliable macros.
 */
inline string findFile(string path, string filename, string extension)
{
    if (extension.at(0) != '.')
        extension = "." + extension;
    string command = "find " + path + " -name " + filename + extension + " 2>/dev/null -print -quit";
//#pragma message "Using '2>/dev/null' here suppresses *all* error messages"
    // it's done here to suppress warnings of invalid locations
    FILE *fp = popen(command.c_str(), "r");
    if (fp == NULL)
        return "";
    char line[1024];
    fscanf(fp,"%[^\n]", line);
    pclose(fp);

    string fullName(line);
    unsigned int name_start = fullName.rfind("/") + 1;
    // Extension might contain a dot, like e.g. '.pdf.gz'
    unsigned int name_end = fullName.length() - extension.length();
    fullName = fullName.substr(name_start, name_end - name_start);
    if (fullName == filename)
        return line;
    return "";
}


inline string findLeafName(string name)
{
    unsigned int name_start;
    unsigned int name_end;

    name_start = name.rfind("/");
    name_end = name.rfind(".");
    return name.substr(name_start + 1, name_end - name_start - 1);
}


// adds or replaces wrong extension with the right one.
inline string setExtension(string fname, string ext)
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


inline bool copyFile(string source, string destination)
{
    std::ifstream infile (source, std::ios::in|std::ios::binary|std::ios::ate);
    if (!infile.is_open())
        return 1;
    std::ofstream outfile (destination, std::ios::out|std::ios::binary);
    if (!outfile.is_open())
        return 1;

    std::streampos size = infile.tellg();
    char *memblock = new char [size];
    infile.seekg (0, std::ios::beg);
    infile.read(memblock, size);
    infile.close();
    outfile.write(memblock, size);
    outfile.close();
    delete[] memblock;
    return 0;
}


inline string saveGzipped(char *data, string filename, int compression)
{
    char options[10];
    snprintf(options, 10, "wb%d", compression);

    gzFile gzfile;
    gzfile = gzopen(filename.c_str(), options);
    if (gzfile == NULL)
        return "gzopen() == NULL";
    gzputs(gzfile, data);
    gzclose(gzfile);
    return "";
}


inline ssize_t saveData(char *buff, size_t bytes, string filename)
{
    //std::cout << "filename " << filename << std::endl;
    int writefile = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (writefile < 0)
    {
        //std::cout << std::strerror(errno) << std::endl;
        return 0;
    }
    ssize_t written = write(writefile, buff, bytes);
    close (writefile);
    return written;
}


inline bool saveText(string text, string filename)
{
    FILE *writefile = fopen(filename.c_str(), "w");
    if (!writefile)
        return 0;

    fputs(text.c_str(), writefile);
    fclose (writefile);
    return 1;
}


inline char * loadGzipped(string _filename, string * report)
{
    string filename = _filename;
    char *data = NULL;
    gzFile gzf  = gzopen(filename.c_str(), "rb");
    if (!gzf)
    {
        *report = ("Failed to open file " + filename + " for load: " + string(strerror(errno)));
        return NULL;
    }
    const int bufSize = 4096;
    char fetchBuf[4097];
    int this_read;
    int total_bytes = 0;
    stringstream readStream;
    for (bool quit = false; !quit;)
    {
        memset(fetchBuf, 0, sizeof(fetchBuf) * sizeof(char));
        this_read = gzread(gzf, fetchBuf, bufSize);
        if (this_read > 0)
        {
            readStream << fetchBuf;
            total_bytes += this_read;
        }
        else if (this_read < 0)
        {
            int errnum;
            *report = ("Read error in zlib: " + string(gzerror(gzf, &errnum)));
            if (errnum == Z_ERRNO)
                *report = ("Filesystem error: " + string(strerror(errno)));
            quit = true;
        }
        else if (total_bytes > 0)
        {
            data = new char[total_bytes + 1];
            if (data)
            {
                memset(data, 0, total_bytes + 1);
                memcpy(data, readStream.str().c_str(), total_bytes);
            }
            quit = true;
        }
    }
    gzclose(gzf);
    //*report = "it looks like we sucessfully loaded" + filename;
    return data;
}


inline string loadText(string filename)
{
    FILE *readfile = fopen(filename.c_str(), "r");
    if (!readfile)
        return "";

    string text = "";
    char line [1024];
    // no Yoshimi text lines should get anywhere near this!
    while (!feof(readfile))
    {
        if(fgets(line , 1024 , readfile))
            text += string(line);
    }
    fclose (readfile);
    text.erase(text.find_last_not_of(" \n\r\t")+1);
    return text;
}


inline bool createEmptyFile(string filename)
{
    std::ofstream file {filename};
    return 0; // TODO need a test for success
}


inline bool createDir(string filename)
{
    return mkdir(filename.c_str(), 0755);
}


/*
 * The following two functions are currently identical for
 * linux but that may not always be true nor possibly other
 * OSs/filers, so you should always use the correct one.
 */
inline bool deleteFile(string filename)
{
    bool isOk = remove(filename.c_str()) == 0;
    return isOk;
}


inline bool deleteDir(string filename)
{
    bool isOk = remove(filename.c_str()) == 0;
    return isOk;
}


/*
 * The following two functions are currently identical for
 * linux but that may not always be true nor possibly other
 * OSs/filers, so you should always use the correct one.
 */
inline bool renameFile(string oldname, string newname)
{
    bool isOk = rename(oldname.c_str(), newname.c_str()) == 0;
    return isOk;
}


inline bool renameDir(string oldname, string newname)
{
    bool isOk = rename(oldname.c_str(), newname.c_str()) == 0;
    return isOk;
}

// replace build directory with a different
// one in the compilation directory
inline string extendLocalPath(string leaf)
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


}//(End)namespace file
#endif /*FILEMGR_H*/
