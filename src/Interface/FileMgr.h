/*
    fileMgr.h

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

#ifndef FILEMGR_H
#define FILEMGR_H

#include <string>

using namespace std;

#include "globals.h"

namespace EXTEN
{
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
}

class SynthEngine;

class FileMgr
{
    public:
        FileMgr(){ }
        ~FileMgr(){ };
        bool TestFunc(int result);

        void legit_filename(string& fname);
        void legit_pathname(string& fname);
        bool isRegFile(string chkpath);
        bool isDirectory(string chkpath);
        string findfile(string path, string filename, string extension);
        string findleafname(string name);
        string setExtension(string fname, string ext);
        bool copyFile(string source, string destination);
        string localPath(string leaf);
        string saveGzipped(char *data, string filename, int compression);
        ssize_t saveData(char *buff, size_t bytes, string filename);
        bool saveText(string text, string filename);
        char *loadGzipped(string _filename, string *report);
        string loadText(string filename);
        bool deleteFile(string filename);
        bool deleteDir(string filename);
        bool renameFile(string oldname, string newname);
        bool renameDir(string oldname, string newname);
};

#endif
