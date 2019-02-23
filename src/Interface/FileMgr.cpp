/*
    MidiDecode.cpp

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

#include <iostream>
#include <unistd.h>
#include <string>
#include <unistd.h>

using namespace std;

#include "Interface/FileMgr.h"

bool FileMgr::TestFunc(int result)
{
    cout << "***\nTest Function " << result << "\n***" << endl;
    return (result > 0);
}


// make a filename legal
void FileMgr::legit_filename(string& fname)
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
void FileMgr::legit_pathname(string& fname)
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
 * This is only intended for calls on the local filesystem
 * and to known locations, so buffer size should be adequate
 * and avoids dependency on unreliable macros.
 */
string FileMgr::findfile(string path, string filename, string extension)
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

    string fullName(line);
    unsigned int name_start = fullName.rfind("/") + 1;
    // Extension might contain a dot, like e.g. '.pdf.gz'
    unsigned int name_end = fullName.length() - extension.length();
    fullName = fullName.substr(name_start, name_end - name_start);
    if (fullName == filename)
        return line;
    return "";
}


string FileMgr::findleafname(string name)
{
    unsigned int name_start;
    unsigned int name_end;

    name_start = name.rfind("/");
    name_end = name.rfind(".");
    return name.substr(name_start + 1, name_end - name_start - 1);
}


// adds or replaces wrong extension with the right one.
string FileMgr::setExtension(string fname, string ext)
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

