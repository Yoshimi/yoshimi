/*
    Bank.cpp - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/

#include <iostream>
#include <set>
#include <list>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

using namespace std;

#include "Misc/Config.h"
#include "Misc/Master.h"
#include "Misc/Bank.h"

// compare function for sorting banks
static bool bankCmp(const bankstruct_t& lhs, const bankstruct_t& rhs)
{
   return lhs.name < rhs.name;
}


Bank::Bank() :
    defaultinsname(string(" ")),
    bank_size(160),
    xizext(".xiz"),
    force_bank_dir_file(".bankdir") // if this file exists in a directory, the
                                    // directory is considered a bank, even if
                                    // it doesn't contain an instrument file
{
    for (int i = 0; i < BANK_SIZE; ++i)
    {
        bank_instrument[i].used = false;
        bank_instrument[i].filename.clear();
        bank_instrument[i].PADsynth_used = false;
    }
    dirname.clear();
    clearBank();

    for (int i = 0; i < MAX_NUM_BANKS; ++i)
    {
        banks[i].dir.clear();
        banks[i].name.clear();
    }
    bankfiletitle = string(dirname);
    loadBank(Runtime.settings.currentBankDir);
}

Bank::~Bank()
{
    clearBank();
}

// Get the name of an instrument from the bank
string Bank::getName (unsigned int ninstrument)
{
    if (emptySlot(ninstrument))
        return defaultinsname;
    return bank_instrument[ninstrument].name;
}

// Get the numbered name of an instrument from the bank
string Bank::getNameNumbered (unsigned int ninstrument)
{
    if (emptySlot(ninstrument))
        return defaultinsname;
    tmpinsname[ninstrument] = asString(ninstrument + 1) + ". "
                              + string(getName(ninstrument));
    return tmpinsname[ninstrument];
}

// Changes the name of an instrument (and the filename)
void Bank::setName(unsigned int ninstrument, string newname, int newslot)
{
    if (emptySlot(ninstrument))
        return;
    
    int slot = (newslot >= 0) ? newslot + 1 : ninstrument + 1;
    string filename = "0000" + asString(slot);
    filename = filename.substr(filename.size() - 4, 4) + "-" + newname + xizext;
    legit_filename(filename);
    string newfilepath = dirname;
    if (newfilepath.at(newfilepath.size() - 1) != '/')
        newfilepath += "/";
    newfilepath += filename;
    int chk = rename(bank_instrument[ninstrument].filename.c_str(),
                     newfilepath.c_str());
    if (chk < 0)
    {
        cerr << "Error, Bank::setName failed renaming :"
             << bank_instrument[ninstrument].filename << " -> "
             << newfilepath << " : " << strerror(errno) << endl;
    }
    bank_instrument[ninstrument].filename = newfilepath;
    bank_instrument[ninstrument].name = newname;
}

// Check if there is no instrument on a slot from the bank
bool Bank::emptySlot(unsigned int ninstrument)
{
    if (ninstrument >= BANK_SIZE)
        return false;
    if (bank_instrument[ninstrument].filename.empty())
        return true;
    if (bank_instrument[ninstrument].used)
        return false;
    return true;
}

// Removes the instrument from the bank
void Bank::clearSlot(unsigned int ninstrument)
{
    if (emptySlot(ninstrument))
        return;
    int chk = remove(bank_instrument[ninstrument].filename.c_str());
    if (chk < 0)
    {
        cerr << "clearSlot " << ninstrument << ", failed to remove "
             << bank_instrument[ninstrument].filename << " "
             << strerror(errno) << endl;
    }
    deleteFromBank(ninstrument);
}

// Save the instrument to a slot
void Bank::saveToSlot(unsigned int ninstrument, Part *part)
{
    if (ninstrument >= BANK_SIZE)
    {
        cerr << "Error, savetoslot " << ninstrument << "slot > BANK_SIZE" << endl;
        return;
    }
    clearSlot(ninstrument);
    string filename = "0000" + asString(ninstrument + 1);
    filename = filename.substr(filename.size() - 4, 4)
               + "-" + part->Pname + xizext;
    legit_filename(filename);
    string filepath = dirname;
    if (filepath.at(filepath.size() - 1) != '/')
        filepath += "/";
    filepath += filename;
    if (isRegFile(filepath))
    {
        int chk = remove(filepath.c_str());
        if (chk < 0)
            cerr << "Error, Bank::saveToSlot failed to unlink " << filepath
                 << ", " << strerror(errno) << endl;
    }
    part->saveXML(filepath);
    addToBank(ninstrument, filename, part->Pname);
}

// Loads the instrument from the bank
void Bank::loadFromSlot(unsigned int ninstrument, Part *part)
{
    if (ninstrument >= BANK_SIZE)
    {
        cerr << "Error, loadfromslot " << ninstrument << "slot > BANK_SIZE" << endl;
        return;
    }
    if (emptySlot(ninstrument))
        return;
    part->defaultsInstrument();
    part->loadXMLinstrument(bank_instrument[ninstrument].filename);
}


// Makes current a bank directory
bool Bank::loadBank(string bankdirname)
{
    DIR *dir = opendir(bankdirname.c_str());
    if (dir == NULL)
    {
        if (Runtime.settings.verbose)
            cerr << "Error, failed to open bank directory " << bankdirname << endl;
        return false;
    }
    clearBank();
    dirname = string(bankdirname);
    bankfiletitle = dirname;
    struct dirent *fn;
    struct stat st;
    string chkpath;
    string candidate;
    size_t xizpos;
    while ((fn = readdir(dir)))
    {
        candidate = string(fn->d_name);
        if (candidate == "."
            || candidate == ".."
            || candidate.size() <= (xizext.size() + 5))
            continue;
        chkpath = dirname;
        if (chkpath.at(chkpath.size() - 1) != '/')
            chkpath += "/";
        chkpath += candidate;
        lstat(chkpath.c_str(), &st);
        if (S_ISREG(st.st_mode))
        {
            if ((xizpos = candidate.rfind(xizext)) != string::npos)
            {
                if (xizext.size() == (candidate.size() - xizpos))
                {
                    // just NNNN-<name>.xiz files please
                    // sa verific daca e si extensia dorita
                    if (candidate.at(4) == '-')
                    {
                        unsigned int chk;
                        for (chk = 0; chk < 4; ++chk)
                            if (candidate.at(chk) < '0' || candidate.at(chk) > '9')
                                break;
                        if (!(chk < 4))
                        {
                            int instnum = string2int(candidate.substr(0, 4));
                            // remove "NNNN-" and .xiz extension for instrument name
                            string instname = candidate.substr(5, candidate.size() - xizext.size() - 5);
                            addToBank(instnum - 1, candidate, instname);
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
    Runtime.settings.currentBankDir = dirname;
    return true;
}

// Makes a new bank, put it on a file and makes it current bank
bool Bank::newBank(string newbankdir)
{
    if (Runtime.settings.bankRootDirlist[0].empty())
    {
        cerr << "Error, default bank root directory not set" << endl;
        return false;
    }
    string newbankpath = Runtime.settings.bankRootDirlist[0];
    if (newbankpath.at(newbankpath.size() - 1) != '/')
        newbankpath += "/";
    newbankpath += newbankdir;
    int result = mkdir(newbankpath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (result < 0)
    {
        cerr << "Error, failed to mkdir " << newbankpath << endl;
        return false;
    }
    else if (Runtime.settings.verbose)
        cerr << "mkdir " << newbankpath << " succeeded" << endl;
    string forcefile = newbankpath;
    if (forcefile.at(forcefile.size() - 1) != '/')
        forcefile += "/";
    forcefile += force_bank_dir_file;
    FILE *tmpfile = fopen(forcefile.c_str(), "w+");
    fclose(tmpfile);
    return loadBank(newbankpath);
}

// Swaps a slot with another
void Bank::swapSlot(unsigned int n1, unsigned int n2)
{
    if (n1 == n2)
        return;
    if (locked())
    {
        cerr << "Error, swapslot requested, but is locked" << endl;
        return;
    }
    if (emptySlot(n1) && emptySlot(n2))
        return;
    if (emptySlot(n1)) // make the empty slot the destination
    {   
        int tmp = n2;
        n2 = n1;
        n1 = tmp;
    }
    if (emptySlot(n2)) // this is just a movement to an empty slot
    {
        setName(n1, getName(n1), n2);
        bank_instrument[n2] = bank_instrument[n1];
        bank_instrument[n1].used = false;
        bank_instrument[n1].name.clear();
        bank_instrument[n1].filename.clear();
        bank_instrument[n1].PADsynth_used = 0;
    }
    else
    {   // if both slots are used
        if (bank_instrument[n1].name == bank_instrument[n2].name)
            // change the name of the second instrument if the name are equal
            bank_instrument[n2].name += "2";
        setName(n2, getName(n2), n1);
        setName(n1, getName(n1), n2);
        bank_instrument[n1].name.swap(bank_instrument[n2].name);
        bank_instrument[n1].filename.swap(bank_instrument[n2].filename);
        bool in_use = bank_instrument[n1].used;
        bank_instrument[n1].used = bank_instrument[n2].used;
        bank_instrument[n2].used = in_use;
        in_use = bank_instrument[n1].PADsynth_used;
        bank_instrument[n1].PADsynth_used = bank_instrument[n2].PADsynth_used;
        bank_instrument[n2].PADsynth_used = in_use;
    }
}

// Re-scan for directories containing instrument banks
void Bank::rescanBanks()
{
    set<string, less<string> > bankroots;
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        if (!Runtime.settings.bankRootDirlist[i].empty())
            bankroots.insert(Runtime.settings.bankRootDirlist[i]);
    banklist.clear();
    set<string, less<string> >::iterator dxr;
    for (dxr = bankroots.begin(); dxr != bankroots.end(); ++dxr)
        scanRootdir(*dxr);
    for (int i = 0; i < MAX_NUM_BANKS; ++i)
    {
        banks[i].dir.clear();
        banks[i].name.clear();
    }
    banklist.sort(bankCmp);
    list<bankstruct_t>::iterator x;
    int idx = 1;
    for(x = banklist.begin(); x != banklist.end() && idx < MAX_NUM_BANKS; ++x)
        banks[idx++] = *x;
    banklist.clear();
}


// private affairs

void Bank::scanRootdir(string rootdir)
{
    if (rootdir.empty())
        return;
    DIR *dir = opendir(rootdir.c_str());
    if (dir == NULL)
    {
        if (Runtime.settings.verbose)
            cerr << "No such directory, root bank entry: " << rootdir << endl;
        return;
    }
    struct dirent *fn;
    struct stat st;
    size_t xizpos;
    while ((fn = readdir(dir)))
    {
        string candidate = string(fn->d_name);
        if (candidate == "." || candidate == "..")
            continue;
        string chkdir = rootdir;
        if (chkdir.at(chkdir.size() - 1) != '/')
            chkdir += "/";
        chkdir += candidate;
        lstat(chkdir.c_str(), &st);
        if (!S_ISDIR(st.st_mode))
            continue;
        // check if directory contains an instrument or .bankdir
        DIR *d = opendir(chkdir.c_str());
        if (d == NULL)
        {
            if (Runtime.settings.verbose)
                cerr << "Error, failed to open bank directory candidate: "
                     << chkdir << endl;
            continue;
        }
        struct dirent *fname;
        int idx;
        char x;
        while ((fname = readdir(d)))
        {
            string possible = string(fname->d_name);
            if (possible == "." || possible == "..")
                continue;
            if (possible == force_bank_dir_file)
            {   // .bankdir file exists, so add the bank
                bankstruct_t newbk = { candidate, chkdir };
                banklist.push_back(newbk);
                break;
            }
            if (possible.size() <= (xizext.size() + 5))
                continue;
            // check for an instrument starting with "NNNN-" prefix
            for (idx = 0; idx < 4; ++idx)
            {
                x = possible.at(idx);
                if (x < '0' || x > '9')
                    break;
            }
            if (idx < 4 || possible.at(idx) != '-')
                continue;
            {
                string chkpath = chkdir + possible;
                lstat(chkpath.c_str(), &st);
                if (st.st_mode & (S_IFREG | S_IRGRP))
                {
                    // check for .xiz extension
                    if ((xizpos = possible.rfind(xizext)) != string::npos)
                    {
                        if (xizext.size() == (possible.size() - xizpos))
                        {   // is an instrument, so add the bank
                            bankstruct_t newbk = { candidate, chkdir };
                            banklist.push_back(newbk);
                            break;
                        }
                    }
                }
            }
        }
        closedir(d);
    }
    closedir(dir);
}


void Bank::clearBank(void)
{
    for (int i = 0; i < BANK_SIZE; ++i)
        deleteFromBank(i);
    dirname.clear();
    bankfiletitle.clear();
}

bool Bank::addToBank(int pos, const string filename, string name)
{
    if (pos >= 0 && pos < BANK_SIZE)
    {
        if (bank_instrument[pos].used)
            pos = -1; // force it to find a new free position
    }
    else if (pos >= BANK_SIZE)
        pos = -1;

    if (pos < 0)
    {   //find a free position
        for (int i = BANK_SIZE - 1; i >= 0; i--)
            if (!bank_instrument[i].used)
            {
                pos = i;
                break;
            }
    }
    if (pos < 0)
        return -1; // the bank is full

    deleteFromBank(pos);
    bank_instrument[pos].used = true;
    bank_instrument[pos].name = name;
    tmpinsname[pos] = " ";
    string filepath = dirname;
    if (filepath.at(filepath.size() - 1) != '/')
        filepath += "/";
    filepath += filename;
    bank_instrument[pos].filename = filepath;

    // see if PADsynth is used
    if (Runtime.settings.CheckPADsynth)
    {
        XMLwrapper *xml = new XMLwrapper();
        xml->checkfileinformation(bank_instrument[pos].filename.c_str());
        bank_instrument[pos].PADsynth_used = xml->information.PADsynth_used;
        delete xml;
    }
    else
        bank_instrument[pos].PADsynth_used = false;
    return 0;
}

bool Bank::isPADsynth_used(unsigned int ninstrument)
{
    if (Runtime.settings.CheckPADsynth == 0)
        return 0;
    else
        return bank_instrument[ninstrument].PADsynth_used;
}


void Bank::deleteFromBank(unsigned int pos)
{
    if (pos >= BANK_SIZE)
    {
        cerr << "Error, deletefrombank pos " << pos << " > BANK_SIZE"
             << BANK_SIZE << endl;
        return;
    }
    bank_instrument[pos].used = false;
    bank_instrument[pos].filename.clear();
    tmpinsname[pos].clear();
}
