/*
    FileMgr.h - all file operations

    Copyright 2019-2024 Will Godfrey and others.

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
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
#include <sys/types.h>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <list>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <zlib.h>

#include "globals.h"

#define OUR_PATH_MAX 4096
/*
 * PATH_MAX is a poorly defined constant, and not very
 * portable. As this is only used for a simple tightly
 * defined purpose we set a value to replace it that
 * should be safe for any reasonable architecture.
 */


/*
 * If changing filesystems it may be necessary to provide
 * a translation of directory and file leaf separators as
 * well as file extension markers.
 * These are used as spilt points in other code areas.
 */

namespace EXTEN {

using std::string;

const string config =        ".config";
const string instance =      ".instance";
const string validBank =     ".bankdir";
const string history =       ".history";
const string zynInst =       ".xiz";
const string yoshInst =      ".xiy";
const string anyInst =       ".xi*";
const string patchset =      ".xmz";
const string state =         ".state";
const string presets =       ".xpz";
const string scale =         ".xsz";
const string scalaTuning =   ".scl";
const string scalaKeymap =   ".kbm";
const string vector =        ".xvy";
const string mlearn =        ".xly";
const string MSwave =        ".wav";
const string lists =         ".list";
const string theme =         ".clr";

}//(End)namespace EXTEN


namespace file {

using std::string;
using std::stringstream;

// Marker used for instance config when started as LV2 plugin
const string LV2_INSTANCE = "LV2";

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


/*
 * tries to find build time doc directory
 * currently only used to find the latest user guide
 */
inline string localPath()
{
    char *tmpath;
    tmpath = (char*) malloc(OUR_PATH_MAX);
    getcwd (tmpath, OUR_PATH_MAX);
    string path = string(tmpath);
    free(tmpath);
    size_t found = path.rfind("/");
    if (found != string::npos)
        path = path.substr(0, found + 1) + "doc";
    else
        path = "";
    return path;
}

/*
 * this now returns the last modified time which can
 * never be zero so can represent 'true'
 */
inline size_t isRegularFile(string const& chkpath)
{
    struct stat st;
    if (!stat(chkpath.c_str(), &st))
        if (S_ISREG(st.st_mode))
            return st.st_mtime;
    return 0;
}


/*
 * this now returns the last modified time which can
 * never be zero so can represent 'true'
*/
inline size_t isDirectory(string const& chkpath)
{
    struct stat st;
    if (!stat(chkpath.c_str(), &st))
    {
        if (S_ISDIR(st.st_mode))
            return st.st_mtime;
    }
    return false;
}


/* performs specific OS command
 * optionally returning a multi-line response
 */

inline bool cmd2string(std::string cmd)
{
    FILE* fp = popen(cmd.c_str(), "r");
    if(fp)
    {
        pclose(fp);
        return true;
    }
    return false;
}

inline bool cmd2string(std::string cmd, string& result)
{
    bool isok = false;
    FILE* fp = popen(cmd.c_str(), "r");
    if(fp)
    {
        std::vector<char> buffer(OUR_PATH_MAX);
        std::size_t n = fread(buffer.data(), 1, buffer.size(), fp);
        if(n && n < buffer.size())
        {
            buffer.data()[n] = 0;
            result = buffer.data();
        }
        pclose(fp);
        isok = true;
    }
    return isok;
}

/*
 * This performs the actual file operations for preset management
 */
inline bool presetsList(string dirname, string const& type, std::vector<string>& presets)
{
    int presetk = 0;
    string ftype = "." + type + EXTEN::presets;
    presets.clear();

    DIR *dir = opendir(dirname.c_str());
    if (dir == NULL)
        return false;

    struct dirent *fn;
    while ((fn = readdir(dir)))
    {
        string filename = string(fn->d_name);
        if (filename.find(ftype) == string::npos)
        {
            continue;
        }
        if (dirname.at(dirname.size() - 1) != '/')
            dirname += "/";
        presets.push_back(dirname + filename);
        presetk++;
        if (presetk >= MAX_PRESETS)
        {
            closedir(dir);
            return true;
        }
    }
    closedir(dir);

    return true;
}


/*
 * This is only intended for calls on the local filesystem
 * and to known locations, so buffer size should be adequate
 * and it avoids dependency on unreliable macros.
 */
inline string findFile(string const& path, string const& filename, string extension)
{
    if (extension.at(0) != '.')
        extension = "." + extension;
    string command = "find " + path + " -name " + filename + extension + " 2>/dev/null -print -quit";
//#pragma message "Using '2>/dev/null' here suppresses *all* error messages"
    // it's done here to suppress warnings of invalid locations
    FILE *fp = popen(command.c_str(), "r");
    if (fp == NULL)
        return "";
    char line[OUR_PATH_MAX];
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

inline string findLeafName(string const& name)
{
    unsigned int name_start;
    unsigned int name_end;

    name_start = name.rfind("/");
    name_end = name.rfind(".");
    return name.substr(name_start + 1, name_end - name_start - 1);
}

inline string findExtension(string const& name)
{
    size_t point = name.rfind('.');
    if (point == string::npos)
        return "";
    string exten = name.substr(point);
    if (exten.find('/') != string::npos)
        return ""; // not acceptable as an extension!
    return exten;
}


// adds or replaces wrong extension with the right one.
inline string setExtension(string const& fname, string ext)
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


inline bool copyFile(string const& source, string const& destination, char option)
{
    // options
    // 0 = only write if not already present
    // 1 = always write / overwrite
    // 2 = only write if newer

    if (option == 0)
    {
        if (isRegularFile(destination))
        {
            return 0; // treated as a successful write when copying banks
        }
    }

    struct stat sourceInfo;
    stat(source.c_str(), &sourceInfo);
    if (option == 2)
    {
        if (isRegularFile(destination))
        {
            struct stat destInfo;
            stat(destination.c_str(), &destInfo);
            if (sourceInfo.st_mtime <= destInfo.st_mtime)
            {
                return 0; // it's already the newest
            }
        }
    }

    std::ifstream infile (source, std::ios::in|std::ios::binary|std::ios::ate);
    if (!infile.is_open())
        return 1;
    std::ofstream outfile (destination, std::ios::out|std::ios::binary);
    if (!outfile.is_open())
        return 3;

    std::streampos size = infile.tellg();
    char *memblock = new char [size];
    infile.seekg (0, std::ios::beg);
    infile.read(memblock, size);
    infile.close();
    outfile.write(memblock, size);
    outfile.close();
    delete[] memblock;

    if (option == 2)
    {
        struct timespec ts[2];
        ts[1].tv_sec = (sourceInfo.st_mtime % 10000000000);
        ts[1].tv_nsec = (sourceInfo.st_mtime / 10000000000);
        utimensat(0, destination.c_str(), ts, 0);
    }
    return 0;
}


inline uint32_t copyDir(string const& source, string const& destination, char option)
{
    DIR *dir = opendir(source.c_str());
    if (dir == NULL)
        return 0xffffffff;
    struct dirent *fn;
    int count = 0;
    int missing = 0;
    while ((fn = readdir(dir)))
    {
        string nextfile = string(fn->d_name);
        if (!isRegularFile(source + "/" + nextfile))
            continue;
        if (nextfile == "." || nextfile == "..")
            continue;
        if (copyFile(source + "/" + nextfile, destination + "/" + nextfile, option))
            ++missing;
        else
            ++count;
    }
    closedir(dir);
    return count | (missing << 16);
}

/*
 * this fills the given list with all contents removing the
 * directory management from the calling functions.
 */
inline int listDir(std::list<string>* dirList, string const& dirName)
{
    DIR *dir = opendir(dirName.c_str());
    if (dir == NULL)
        return 0xffffffff;
    struct dirent *fn;
    int count = 0;
    while ((fn = readdir(dir)))
    {
        string name = fn->d_name;
        if (!name.empty() && name != "." && name != "..")
        {
            dirList->push_back(name);
            ++count;
        }
    }
    closedir(dir);
    return count;
}


/*
 * Counts all the objects within the directory.
 */

inline int countDir(const std::string dirName)
{
    DIR *dir = opendir(dirName.c_str());
    if (dir == NULL)
        return -1;
    struct dirent *fn;
    int count = 0;
    char dir1[2] = {'.', 0};
    char dir2[3] = {'.', '.', 0};
    while ((fn = readdir(dir)))
    {
        if (fn->d_type == DT_DIR)
        {
            if (strcmp(fn->d_name, dir1) !=0 && strcmp(fn->d_name, dir2) !=0)
                ++ count;
        }
    }
    closedir(dir);
    return count;
}


/*
 * We return the contents as sorted, sequential lists in directories
 * and files of the required type as a series of leaf names (as the
 * root directory is already known). This reduces the size of the
 * string to a manageable length.
 * Directories are prefixed to make them easier to identify.
 */
inline void dir2string(string &wanted, string currentDir, string exten, int opt = 0)
{
    // options
    // &1 allow hidden dirs
    // &2 allow hidden files
    // &4 allow wildcards
    // &8 hide all subdirectories
    // &16 hide files (just looking for dirs)
    std::list<string> build;
    wanted = "";
    uint32_t found = listDir(&build, currentDir);
    if (found == 0xffffffff)
        return;

    if (build.size() > 1)
        build.sort();
   if(currentDir.back() != '/')
        currentDir += '/';
    string line;
    if (!(opt & 8))
    {
        for (std::list<string>::iterator it = build.begin(); it != build.end(); ++it)
        { // get directories
            if ((opt & 1) || string(*it).front() != '.') // no hidden dirs
            {
                line = *it;
                if (line.back() != '/')
                    line += '/';
                if (isDirectory(currentDir + line))
                    wanted += ("Dir: " + line + "\n");
            }
        }
    }
    if (opt & 16)
    {
        build.clear();
        return;
    }

    // int to suppress clang compiler warning
    bool instype = (int(exten == ".xiz") | (exten == ".xiy")  | (exten == ".xi*"));
    string last;
    last.clear();
    for (std::list<string>::iterator it = build.begin(); it != build.end(); ++it)
    { // get files
        if ((opt & 2) || string(*it).front() != '.') // no hidden files
        {
            string next;
            line = currentDir + *it;
            if (isRegularFile(line))
            {
                next.clear();
                if ((opt & 4))
                {
                    next = *it;
                    if (!next.empty())
                        wanted += (next + "\n");
                }
                else
                {
                    if (instype)
                    {
                        if (findExtension(line) == ".xiy" || findExtension(line) == ".xiz")
                            next = *it;
                    }
                    else
                    {
                        if (findExtension(line) == exten)
                            next = *it;
                    }

                    // remove the extension, the source knows what it is
                    // and it must exist to have been found!
                    if (!next.empty())
                    {
                        size_t pos = next.rfind('.');
                        next = next.substr(0, pos);
                        // also remove instrument type duplicates
                        if (next != last)
                        {
                            last = next;
                            wanted += (next + "\n");
                        }
                    }
                }
            }
        }
    }
    build.clear();
}


inline string saveGzipped(const char* data, string const& filename, int compression)
{
    char options[10];
    snprintf(options, 10, "wb%d", compression);

    gzFile gzfile;
    gzfile = gzopen(filename.c_str(), options);
    if (gzfile == NULL)
        return "gzopen() == NULL";
    int res = gzputs(gzfile, data);
    gzclose(gzfile);
    return res<0? "Failure writing compressed data": "";
}


inline ssize_t saveData(char *buff, size_t bytes, string const& filename)
{
    int writefile = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (writefile < 0)
    {
        return 0;
    }
    ssize_t written = write(writefile, buff, bytes);
    close (writefile);
    return written;
}


inline bool saveText(string const& text, string const& filename)
{
    FILE *writefile = fopen(filename.c_str(), "w");
    if (!writefile)
        return 0;

    fputs(text.c_str(), writefile);
    fclose (writefile);
    return 1;
}


inline string loadGzipped(string const& filename, string& report)
{
    const int BUF_SIZE = 4096;
    stringstream readStream;
    gzFile gzf  = gzopen(filename.c_str(), "rb");
    if (not gzf)
        report = ("Failed to open file " + filename + " for load: " + string(strerror(errno)));
    else
    {
        int readFlag{1};
        while (readFlag > 0)
        {
            std::array<char, BUF_SIZE+1> fetchBuf{0};
            readFlag = gzread(gzf, fetchBuf.data(), BUF_SIZE);
            if (readFlag > 0)
                readStream << fetchBuf.data();
        }
        if (readFlag < 0)
        {
            int errnum{0};
            report = ("Read error in zlib: " + string(gzerror(gzf, &errnum)));
            if (errnum == Z_ERRNO)
                report = ("Filesystem error: " + string(strerror(errno)));
        }
        gzclose(gzf);
    }
    return readStream.str();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////TODO 4/25 : old version --> obsolete after switch to XMLStore
inline char * loadGzipped_OBSOLETE_(string const& _filename, string * report)
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
    //*report = "it looks like we successfully loaded" + filename;
    return data;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////TODO 4/25 : old version --> obsolete after switch to XMLStore

/*
 * This is used for text files, preserving individual lines. These can
 * then be split up by the receiving functions without needing a file
 * handle, or any knowledge of the file system.
 *
 * All leading and trailing whitespace is removed, as well as line-end
 * variations. Blank lines are removed, and a single newline character
 * is added to the end of each remaining line.
 */
inline string loadText(string const& filename)
{
    FILE *readfile = fopen(filename.c_str(), "r");
    if (!readfile)
        return "";

    string text = "";
    char line [OUR_PATH_MAX];
    // no Yoshimi input text lines should get anywhere near this!
    while (!feof(readfile))
    {
        string rawline = "";
        if (fgets(line , OUR_PATH_MAX , readfile))
        {
            rawline = string(line);
            while (rawline.length() > 0 && rawline[0] < '!')
                rawline.erase(0, 1);
            if(rawline.length() > 0)
            {
                while (rawline.length() > 0 && rawline[rawline.length() - 1] < ' ')
                {
                    rawline.pop_back();
                }
                text += (rawline + '\n');
            }
        }
    }
    fclose (readfile);
    return text;
}


inline bool createEmptyFile(string const& filename)
{ // not currently used now
    std::fstream file;
    file.open(filename, std::ios::out);
    if (!file)
        return false;
    file.close();
    return true;
}


inline bool createDir(string const& dirname)
{
    if (isDirectory(dirname))
        return false; // don't waste time. it's already here!
    size_t pos = 1;
    size_t oldPos = pos;
    string nextDir;
    bool failed = false;
    while (pos != string::npos && failed == false)
    {

        pos = dirname.find("/", oldPos);
        if (pos == string::npos)
            nextDir = dirname;
        else
        {
            nextDir = dirname.substr(0, pos).c_str();
            oldPos = pos + 1;
        }
        if (!isDirectory(nextDir))
            failed = mkdir(nextDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    return failed;
}


/*
 * The following two functions are currently identical for
 * linux but that may not always be true nor possibly other
 * OSs/filers, so you should always use the correct one.
 */
inline bool deleteFile(string const& filename)
{
    bool isOk = remove(filename.c_str()) == 0;
    return isOk;
}


inline bool deleteDir(string const& filename)
{
    bool isOk = remove(filename.c_str()) == 0;
    return isOk;
}


/*
 * The following two functions are currently identical for
 * linux but that may not always be true nor possibly other
 * OSs/filers, so you should always use the correct one.
 */
inline bool renameFile(string const& oldname, string const& newname)
{
    bool isOk = rename(oldname.c_str(), newname.c_str()) == 0;
    return isOk;
}


inline bool renameDir(string const& oldname, string const& newname)
{
    bool isOk = rename(oldname.c_str(), newname.c_str()) == 0;
    return isOk;
}

// replace build directory with a different
// one in the compilation directory
inline string extendLocalPath(string const& leaf)
{
    char* tmpath = getcwd (NULL, 0);
    if (tmpath == NULL)
       return "";

    string path(tmpath);
    free(tmpath);
    size_t found = path.rfind("yoshimi");
    if (found == string::npos)
        return "";

    size_t next = path.find('/', found);
    if (next == string::npos)
        return "";

    return path.substr(0, next) + leaf;
}

inline string userHome()
{
    string home = string(getenv("HOME"));
    if (home.empty() || !isDirectory(home))
        home = string("/tmp");
return home + '/';
}

inline string localDir()
{
    string local = userHome() + ".local/share/" + YOSHIMI;
    if (!isDirectory(local))
    {
        if (createDir(local))
            local = "";
    }
    return local;
}

inline string configDir()
{
    string config = userHome() + string(EXTEN::config) + "/" + YOSHIMI;
    if (!isDirectory(config))
    {
        if (createDir(config))
            config = "";
    }
    return config;
}

/*
 * Tries to find the most relevant entry for example files.
 * "leafname" can include a subdirectory such as "themes/demo"
 * The build path is likely to be the most recent but either
 * of .local or the distro install path could be next.
 */
inline string findExampleFile(string leafname)
{
    string dir = localPath();
    string fullname = "";
    if (!dir.empty())
    {
        string tmp = dir + "/examples/" + leafname;
        if (isRegularFile(tmp))
            fullname = tmp;
    }
    if (fullname.empty())
    {
        dir = localDir();
        if (!dir.empty())
        {
            string tmp = dir + "/themes/" + leafname;
            if (isRegularFile(tmp))
                fullname = tmp;
        }
        if (fullname.empty())
        {
            string tmp = "/usr/local/share/" + string(YOSHIMI) + "/examples/" + leafname;
            if (isRegularFile(tmp))
                fullname = tmp;
        }
        if (fullname.empty())
        {
            string tmp = "/usr/share/" + string(YOSHIMI) + "/examples/" + leafname;
            if (isRegularFile(tmp))
                fullname = tmp;
        }
    }
    return fullname;
}


}//(End)namespace file
#endif /*FILEMGR_H*/
