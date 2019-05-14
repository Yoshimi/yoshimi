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

    Created April 2019
*/

#ifndef FILEMGR_H
#define FILEMGR_H

#include <string>

#include "globals.h"

namespace EXTEN
{
    const std::string config =        ".config";
    const std::string instance =      ".instance";
    const std::string bank =          ".banks";
    const std::string history =       ".history";
    const std::string zynInst =       ".xiz";
    const std::string yoshInst =      ".xiy";
    const std::string anyInst =       ".xi*";
    const std::string patchset =      ".xmz";
    const std::string state =         ".state";
    const std::string scale =         ".xsz";
    const std::string scalaTuning =   ".scl";
    const std::string scalaKeymap =   ".kbm";
    const std::string vector =        ".xvy";
    const std::string mlearn =        ".xly";
    const std::string MSwave=         ".wav";
    const std::string window =        ".windows";
}

class SynthEngine;

class FileMgr
{
    public:
        FileMgr(){ }
        ~FileMgr(){ };
        bool TestFunc(int result);

        void legit_filename(std::string& fname);
        void legit_pathname(std::string& fname);
        bool isRegFile(std::string chkpath);
        bool isDirectory(std::string chkpath);
        std::string findfile(std::string path, std::string filename, std::string extension);
        std::string findleafname(std::string name);
        std::string setExtension(std::string fname, std::string ext);
        bool copyFile(std::string source, std::string destination);
        std::string localPath(std::string leaf);
        std::string saveGzipped(char *data, std::string filename, int compression);
        ssize_t saveData(char *buff, size_t bytes, std::string filename);
        bool saveText(std::string text, std::string filename);
        char *loadGzipped(std::string _filename, std::string *report);
        std::string loadText(std::string filename);
        bool createEmptyFile(std::string filename);
        bool deleteFile(std::string filename);
        bool deleteDir(std::string filename);
        bool renameFile(std::string oldname, std::string newname);
        bool renameDir(std::string oldname, std::string newname);
};

#endif
