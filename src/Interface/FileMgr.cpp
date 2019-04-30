/*
    FileMgr.cpp - all file operations

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

    Created February 2019
*/

#include <cerrno>
#include <iostream>
#include <zlib.h>
#include <sstream>
#include <fstream>
#include <fcntl.h> // this affects error reporting
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <cstring>

#include "Interface/FileMgr.h"
#include "Misc/MiscFuncs.h"
#include "Misc/SynthEngine.h"

bool FileMgr::TestFunc(int result)
{
    std::cout << "***\nTest Function " << result << "\n***" << std::endl;
    return (result > 0);
}


// make a filename legal
void FileMgr::legit_filename(std::string& fname)
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
void FileMgr::legit_pathname(std::string& fname)
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


bool FileMgr::isRegFile(std::string chkpath)
{
    struct stat st;
    if (!stat(chkpath.c_str(), &st))
        if (S_ISREG(st.st_mode))
            return true;
    return false;
}


bool FileMgr::isDirectory(std::string chkpath)
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
std::string FileMgr::findfile(std::string path, std::string filename, std::string extension)
{
    if (extension.at(0) != '.')
        extension = "." + extension;
    std::string command = "find " + path + " -name " + filename + extension + " 2>/dev/null -print -quit";
//#pragma message "Using '2>/dev/null' here suppresses *all* error messages"
    // it's done here to suppress warnings of invalid locations
    FILE *fp = popen(command.c_str(), "r");
    if (fp == NULL)
        return "";
    char line[1024];
    fscanf(fp,"%[^\n]", line);
    pclose(fp);

    std::string fullName(line);
    unsigned int name_start = fullName.rfind("/") + 1;
    // Extension might contain a dot, like e.g. '.pdf.gz'
    unsigned int name_end = fullName.length() - extension.length();
    fullName = fullName.substr(name_start, name_end - name_start);
    if (fullName == filename)
        return line;
    return "";
}


std::string FileMgr::findleafname(std::string name)
{
    unsigned int name_start;
    unsigned int name_end;

    name_start = name.rfind("/");
    name_end = name.rfind(".");
    return name.substr(name_start + 1, name_end - name_start - 1);
}


// adds or replaces wrong extension with the right one.
std::string FileMgr::setExtension(std::string fname, std::string ext)
{
    if (ext.at(0) != '.')
        ext = "." + ext;
    std::string tmp;                         // return value
    size_t ext_pos = fname.rfind('.');  // period, if any
    size_t slash_pos = fname.rfind('/'); // UNIX path-separator
    if (slash_pos == std::string::npos)
    {
        // There are no slashes in the std::string, therefore the last period, if
        // any, must be at the position of the extension period.

        ext_pos = fname.rfind('.');
        if (ext_pos == std::string::npos || ext_pos == 0)
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


bool FileMgr::copyFile(std::string source, std::string destination)
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


std::string FileMgr::saveGzipped(char *data, std::string filename, int compression)
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


ssize_t FileMgr::saveData(char *buff, size_t bytes, std::string filename)
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


bool FileMgr::saveText(std::string text, std::string filename)
{
    FILE *writefile = fopen(filename.c_str(), "w");
    if (!writefile)
        return 0;

    fputs(text.c_str(), writefile);
    fclose (writefile);
    return 1;
}


char *FileMgr::loadGzipped(std::string _filename, std::string *report)
{
    std::string filename = _filename;
    char *data = NULL;
    gzFile gzf  = gzopen(filename.c_str(), "rb");
    if (!gzf)
    {
        *report = ("Failed to open file " + filename + " for load: " + std::string(strerror(errno)));
        return NULL;
    }
    const int bufSize = 4096;
    char fetchBuf[4097];
    int this_read;
    int total_bytes = 0;
    std::stringstream readStream;
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
            *report = ("Read error in zlib: " + std::string(gzerror(gzf, &errnum)));
            if (errnum == Z_ERRNO)
                *report = ("Filesystem error: " + std::string(strerror(errno)));
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


std::string FileMgr::loadText(std::string filename)
{
    FILE *readfile = fopen(filename.c_str(), "r");
    if (!readfile)
        return "";

    std::string text = "";
    char line [1024];
    // no Yoshimi text lines should get anywhere near this!
    while (!feof(readfile))
    {
        if(fgets(line , 1024 , readfile))
            text += std::string(line);
    }
    fclose (readfile);
    text.erase(text.find_last_not_of(" \n\r\t")+1);
    return text;
}


bool FileMgr::createEmptyFile(std::string filename)
{
    std::ofstream file {filename};
    return 0; // TODO need a test for sucess
}


/*
 * The following two functions are currently identical for
 * linux but that may not always be true nor possibly other
 * OSs/filers, so you should always use the correct one.
 */
bool FileMgr::deleteFile(std::string filename)
{
    bool isOk = remove(filename.c_str()) == 0;
    return isOk;
}


bool FileMgr::deleteDir(std::string filename)
{
    bool isOk = remove(filename.c_str()) == 0;
    return isOk;
}


/*
 * The following two functions are currently identical for
 * linux but that may not always be true nor possibly other
 * OSs/filers, so you should always use the correct one.
 */
bool FileMgr::renameFile(std::string oldname, std::string newname)
{
    bool isOk = rename(oldname.c_str(), newname.c_str()) == 0;
    return isOk;
}


bool FileMgr::renameDir(std::string oldname, std::string newname)
{
    bool isOk = rename(oldname.c_str(), newname.c_str()) == 0;
    return isOk;
}

// replace build directory with a different
// one in the compilation directory
std::string FileMgr::localPath(std::string leaf)
{
    char *tmpath = getcwd (NULL, 0);
    if (tmpath == NULL)
       return "";

    std::string path = (std::string) tmpath;
    free(tmpath);
    size_t found = path.rfind("yoshimi");
    if (found == std::string::npos)
        return "";

    size_t next = path.find('/', found);
    if (next == std::string::npos)
        return "";

    return path.substr(0, next) + leaf;
}

