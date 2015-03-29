/*
    Bank.cpp - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2014-2015, Will Godfrey & others

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

    This file is a derivative of a ZynAddSubFX original, last modified January 2015
*/

#include <set>
#include <list>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <vector>
#include <algorithm>

using namespace std;

#include "Misc/XMLwrapper.h"
#include "Misc/Config.h"
#include "Misc/Bank.h"
#include "Misc/SynthEngine.h"

Bank::Bank(SynthEngine *_synth) :
    defaultinsname(string(" ")),
    xizext(".xiz"),
    force_bank_dir_file(".bankdir"), // if this file exists in a directory, the
                                    // directory is considered a bank, even if
                                    // it doesn't contain an instrument file
    synth(_synth),
    currentRootID(0),
    currentBankID(0)
{
    roots.clear();
    //addDefaultRootDirs();
}


Bank::~Bank()
{
    roots.clear();
}


bool Bank::readOnlyInstrument(int ninstrument)
{
    if (readOnlyBank(currentBankID))
        return true; // root and bank not writable
    if (emptyslot(ninstrument)) // nothing there so must be OK to write
        return false;
    const char * file = getFullPath(currentRootID, currentBankID, ninstrument).c_str();
    return access(file, W_OK);
}


// Get the name of an instrument from the bank
string Bank::getname(unsigned int ninstrument)
{
    if (emptyslot(ninstrument))
        return defaultinsname;
    return getInstrumentReference(ninstrument).name;
}


// Get the numbered name of an instrument from the bank
string Bank::getnamenumbered(unsigned int ninstrument)
{
    if (emptyslot(ninstrument))
        return defaultinsname;
    string strRet = asString(ninstrument + 1) + ". " + getname(ninstrument);
    return strRet;
}


// Changes the name of an instrument (and the filename)
bool Bank::setname(unsigned int ninstrument, string newname, int newslot)
{
    if (emptyslot(ninstrument))
        return false;

    int slot = (newslot >= 0) ? newslot + 1 : ninstrument + 1;
    string filename = "0000" + asString(slot);
    filename = filename.substr(filename.size() - 4, 4) + "-" + newname + xizext;
    legit_filename(filename);
    string newfilepath = getBankPath(currentRootID, currentBankID);
    if (newfilepath.at(newfilepath.size() - 1) != '/')
        newfilepath += "/";
    newfilepath += filename;
    InstrumentEntry &instrRef = getInstrumentReference(currentRootID, currentBankID, ninstrument);
    int chk = rename(getFullPath(currentRootID, currentBankID, ninstrument).c_str(), newfilepath.c_str());
    if (chk < 0)
    {
        synth->getRuntime().Log("Bank: setName failed renaming "
                    + getFullPath(currentRootID, currentBankID, ninstrument) + " -> "
                    + newfilepath + ": " + string(strerror(errno)));
        return false;
    }    
    instrRef.name = newname;
    instrRef.filename = filename;
    return true;
}


// Check if there is no instrument on a slot from the bank
bool Bank::emptyslotWithID(size_t rootID, size_t bankID, unsigned int ninstrument)
{
    if(roots.count(rootID) == 0 || roots [rootID].banks.count(bankID) == 0)
        return true;
    InstrumentEntry &instr = roots [rootID].banks [bankID].instruments [ninstrument];
    if (!instr.used)
        return true;
    if (instr.name.empty() || instr.filename.empty())
        return true;
    return false;
}


// Removes the instrument from the bank
void Bank::clearslot(unsigned int ninstrument)
{
    if (emptyslot(ninstrument))
        return;
    int chk = remove(getFullPath(currentRootID, currentBankID, ninstrument).c_str());
    if (chk < 0)
    {
        synth->getRuntime().Log("clearSlot: " + asString(ninstrument) + " Failed to remove "
                     + getFullPath(currentRootID, currentBankID, ninstrument) + " "
                     + string(strerror(errno)));
    }
    deletefrombank(currentRootID, currentBankID, ninstrument);
}


// Save the instrument to a slot
void Bank::savetoslot(unsigned int ninstrument, Part *part)
{
    if (ninstrument >= BANK_SIZE)
    {
        synth->getRuntime().Log("savetoslot: Saved " + asString(ninstrument) + ", slot > BANK_SIZE");
        return;
    }
    clearslot(ninstrument);
    string filename = "0000" + asString(ninstrument + 1);
    filename = filename.substr(filename.size() - 4, 4)
               + "-" + part->Pname + xizext;
    legit_filename(filename);
    string filepath = getBankPath(currentRootID, currentBankID);
    if (filepath.at(filepath.size() - 1) != '/')
        filepath += "/";
    filepath += filename;
    if (isRegFile(filepath))
    {
        int chk = remove(filepath.c_str());
        if (chk < 0)
            synth->getRuntime().Log("Bank saveToSlot failed to unlink " + filepath
                        + ", " + string(strerror(errno)));
    }
    part->saveXML(filepath);
    addtobank(currentRootID, currentBankID, ninstrument, filename, part->Pname);
}


// Loads the instrument from the bank
bool Bank::loadfromslot(unsigned int ninstrument, Part *part)
{
    bool flag = false;
    if (!emptyslot(ninstrument))
        flag = part->loadXMLinstrument(getFullPath(currentRootID, currentBankID, ninstrument));
    return flag;
}


bool Bank::readOnlyBank(int bankID)
{
    const char * root = getRootPath(currentRootID).c_str();
    if (access(root, W_OK)) // root not writable
        return true;
    if (getBankName(bankID).empty())
        return false; // not there so must be OK to write
    const char * file = getBankPath(currentRootID, bankID).c_str();
    return access(file, W_OK);
}


//Gets a bank name
string Bank::getBankName(int bankID)
{
    if (roots [currentRootID].banks.count(bankID) == 0)
        return "";
    return string(roots [currentRootID].banks [bankID].dirname);
}


//Gets a bank name with ID
string Bank::getBankIDname(int bankID)
{
    string retname = getBankName(bankID);
    if (!retname.empty())
        retname = asString(bankID) + ". " + retname;
    return retname;
}


// finds the number of instruments in a bank
int Bank::getBankSize(int bankID)
{
    int found = 0;
    for (int i = 0; i < BANK_SIZE; ++ i)
        if (!roots [currentRootID].banks [bankID].instruments [i].name.empty())
            found += 1;
    return found;
}

// Changes a bank name 'in place' and updates the filename
bool Bank::setbankname(unsigned int bankID, string newname)
{
    string filename = newname;
    legit_filename(filename);
    string newfilepath = getRootPath(currentRootID) + "/" + filename;
    int chk = rename(getBankPath(currentRootID,bankID).c_str(),
                     newfilepath.c_str());
    if (chk < 0)
    {
        synth->getRuntime().Log("Bank: Failed to rename " + getBankName(bankID)
                               + " to " + newname);
        return false;
    }
    synth->getRuntime().Log("Renaming " + getBankName(bankID)
                               + " to " + newname);
    
    roots [currentRootID].banks [bankID].dirname = newname;
    return true;
}


// Makes current a bank directory
bool Bank::loadbank(size_t rootID, size_t banknum)
{
    string bankdirname = getBankPath(rootID, banknum);
    if(bankdirname.empty())
    {
        return false;
    }
    DIR *dir = opendir(bankdirname.c_str());
    if (dir == NULL)
    {
        synth->getRuntime().Log("Failed to open bank directory " + bankdirname);
        return false;
    }
    roots [rootID].banks [banknum].instruments.clear();

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
            || candidate.size() <= (xizext.size() + 2)) // actually a 3 char filename!
            continue;
        chkpath = bankdirname;
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

                    // sorry Cal. They insisted :(

                    int chk = 0;
                    char ch = candidate.at(chk);
                    while (ch >= '0' and ch <= '9' and chk < 4){
                        chk += 1;
                        ch = candidate.at(chk);
                    }
                    if (ch == '-')
                    {
                        int instnum = string2int(candidate.substr(0, 4));
                        // remove "NNNN-" and .xiz extension for instrument name
                        string instname = candidate.substr(5, candidate.size() - xizext.size() - 5);
                        addtobank(rootID, banknum, instnum - 1, candidate, instname);
                    }
                    else
                    {
                        string instname = candidate.substr(0, candidate.size() -  xizext.size());
                        addtobank(rootID, banknum, -1, candidate, instname);
                    }
                }
            }
        }
    }
    closedir(dir);
    return true;
}


// Makes a new bank, put it on a file and makes it current bank
bool Bank::newbank(string newbankdir)
{
    if (!newbankfile(newbankdir))
        return false;
    currentBankID = add_bank(newbankdir, newbankdir, currentRootID);
    return true;
}


// Makes a new bank with known ID. Does *not* make it current
bool Bank::newIDbank(string newbankdir, unsigned int bankID)
{
    if (!newbankfile(newbankdir))
        return false;
    roots [currentRootID].banks [bankID].dirname = newbankdir;
    hints [currentRootID] [newbankdir] = bankID; // why do we need this?
    return true;
}


// Performs the actual file operation for new banks
bool Bank::newbankfile(string newbankdir)
{
     if (getRootPath(currentRootID).empty())
    {
        synth->getRuntime().Log("Current bank root directory not set");
        return false;
    }
    string newbankpath = getRootPath(currentRootID);
    if (newbankpath.at(newbankpath.size() - 1) != '/')
        newbankpath += "/";
    newbankpath += newbankdir;
    int result = mkdir(newbankpath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (result < 0)
    {
        synth->getRuntime().Log("Bank: Failed to mkdir " + newbankpath);
        return false;
    }
    else
        synth->getRuntime().Log("mkdir " + newbankpath + " succeeded");
    string forcefile = newbankpath;
    if (forcefile.at(forcefile.size() - 1) != '/')
        forcefile += "/";
    forcefile += force_bank_dir_file;
    FILE *tmpfile = fopen(forcefile.c_str(), "w+");
    fclose(tmpfile);
    return true;
}


// Removes a bank and all its contents
bool Bank::removebank(unsigned int bankID)
{
    int chk;

    for (int inst = 0; inst < BANK_SIZE; ++ inst)
        if (!roots [currentRootID].banks [bankID].instruments [inst].name.empty())
        {
            chk = remove(getFullPath(currentRootID, bankID, inst).c_str());
            if (chk < 0)
            {
                synth->getRuntime().Log("removebank: "
                                        + asString(inst) + " Failed to remove "
                                        + getFullPath(currentRootID, bankID, inst) + " "
                                        + string(strerror(errno)));
                return false;
            }
            deletefrombank(currentRootID, bankID, inst);
        }
        string tmp = getBankPath(currentRootID, bankID)+"/.bankdir";
        chk = remove(tmp.c_str());
        if (chk < 0)
        {
            synth->getRuntime().Log("removebank: Failed to remove bank ID file"
                                    + string(strerror(errno)));
            return false;
        }
        chk = remove(getBankPath(currentRootID, bankID).c_str());
        if (chk < 0)
        {
            synth->getRuntime().Log("removebank: Failed to remove bank"
                                    + asString(bankID) + ": "
                                    + string(strerror(errno)));
            return false;
        }
        roots [currentRootID].banks.erase(bankID);
   return true;
}


// Swaps a slot with another
void Bank::swapslot(unsigned int n1, unsigned int n2)
{
    if (n1 == n2)
        return;

    if (emptyslot(n1) && emptyslot(n2))
        return;
    if (emptyslot(n1)) // make the empty slot the destination
    {
        setname(n2, getname(n2), n1);
        getInstrumentReference(n1) = getInstrumentReference(n2);
        getInstrumentReference(n2).clear();
    }
    else if (emptyslot(n2)) // this is just a movement to an empty slot
    {
        setname(n1, getname(n1), n2);
        getInstrumentReference(n2) = getInstrumentReference(n1);
        getInstrumentReference(n1).clear();
    }
    else
    {   // if both slots are used
        InstrumentEntry &instrRef1 = getInstrumentReference(n1);
        InstrumentEntry &instrRef2 = getInstrumentReference(n2);
        if (instrRef1.name == instrRef2.name)
        {
            // change the name of the second instrument if the name are equal
            instrRef2.name += "2";
        }
        setname(n2, getname(n2), n1);
        setname(n1, getname(n1), n2);
        InstrumentEntry instrTmp = instrRef1;
        instrRef1 = instrRef2;
        instrRef2 = instrTmp;
    }
}


// Intelligently moves or swaps banks preserving instrument details
void Bank::swapbanks(unsigned int firstID, unsigned int secondID)
{
    if (firstID == secondID)
    {
        synth->getRuntime().Log("Nothing to move!");
        return;
    }
        
    string firstname = getBankName(firstID); // this needs improving
    string secondname = getBankName(secondID);
    if (firstname.empty() and secondname.empty())
    {
        synth->getRuntime().Log("Nothing to move!");
        return;
    }
    if (secondname.empty())
    {
        synth->getRuntime().Log("Moving " + firstname);
        roots [currentRootID].banks [secondID] = roots [currentRootID].banks [firstID];
        roots [currentRootID].banks.erase(firstID);
    }
    else if (firstname.empty())
    {
        synth->getRuntime().Log("Moving " + secondname);
        roots [currentRootID].banks [firstID] = roots [currentRootID].banks [secondID];
        roots [currentRootID].banks.erase(secondID);
    }
    else
    {
        synth->getRuntime().Log("Swapping " + firstname + " with " + secondname );
        roots [currentRootID].banks [firstID].dirname = secondname;
        roots [currentRootID].banks [secondID].dirname = firstname;
        hints [currentRootID] [secondname] = firstID; // why do we need these?
        hints [currentRootID] [firstname] = secondID;

        for(int pos = 0; pos < BANK_SIZE; ++ pos)
        {
            InstrumentEntry &instrRef_1 = getInstrumentReference(currentRootID, firstID, pos);
            InstrumentEntry &instrRef_2 = getInstrumentReference(currentRootID, secondID, pos);

            InstrumentEntry tmp = instrRef_2;
            
            if (instrRef_1.name == "")
                roots [currentRootID].banks [secondID].instruments.erase(pos);
            else
                instrRef_2 = instrRef_1;
            
            if (tmp.name == "")
                roots [currentRootID].banks [firstID].instruments.erase(pos);
            else
                instrRef_1 = tmp;
        }
    }
    
    if (firstID == currentBankID)
        currentBankID = secondID;
    else if(secondID == currentBankID)
        currentBankID = firstID;
}


// Re-scan for directories containing instrument banks
void Bank::rescanforbanks(void)
{    
    RootEntryMap::const_iterator it;
    for (it = roots.begin(); it != roots.end(); ++it)
    {
        scanrootdir(it->first);
    }


}

// private affairs

void Bank::scanrootdir(int root_idx)
{
    map<string, string> bankDirsMap;
    string rootdir = roots [root_idx].path;
    if (rootdir.empty() || !isDirectory(rootdir))
        return;
    DIR *dir = opendir(rootdir.c_str());
    if (dir == NULL)
    {
        synth->getRuntime().Log("No such directory, root bank entry " + rootdir);
        return;
    }
    struct dirent *fn;
    struct stat st;
    size_t xizpos;
    roots [root_idx].banks.clear();
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
            synth->getRuntime().Log("scanrootdir: Failed to open bank directory candidate " + chkdir);
            continue;
        }
        struct dirent *fname;
        //int idx;
        //char x;
        while ((fname = readdir(d)))
        {
            string possible = string(fname->d_name);
            if (possible == "." || possible == "..")
                continue;
            if (possible == force_bank_dir_file)
            {   // .bankdir file exists, so add the bank
                bankDirsMap [candidate] = chkdir;
                break;
            }
            string chkpath = chkdir + "/" + possible;
            lstat(chkpath.c_str(), &st);
            if (st.st_mode & (S_IFREG | S_IRGRP))
            {
                // check for .xiz extension
                if ((xizpos = possible.rfind(xizext)) != string::npos)
                {
                    if (xizext.size() == (possible.size() - xizpos))
                    {   // is an instrument, so add the bank
                        bankDirsMap [candidate] = chkdir;
                        break;
                    }
                }
            }
        }
        closedir(d);
    }
    closedir(dir);
    size_t idStep = (size_t)128 / (bankDirsMap.size() + 2);
    if(idStep > 1)
    {
        roots [root_idx].bankIdStep = idStep;
    }

    map<string, string>::iterator it;
    for(it = bankDirsMap.begin(); it != bankDirsMap.end(); ++it)
    {
        add_bank(it->first, it->second, root_idx);
    }
    roots [root_idx].bankIdStep = 0;
}

bool Bank::addtobank(size_t rootID, size_t bankID, int pos, const string filename, const string name)
{    
    BankEntry &bank = roots [rootID].banks [bankID];
    if (pos >= 0 && pos < BANK_SIZE)
    {
        if (bank.instruments [pos].used)
        {
            pos = -1; // force it to find a new free position
        }
    }
    else if (pos >= BANK_SIZE)
        pos = -1;

    if (pos < 0)
    {

        if(!bank.instruments.empty() && bank.instruments.size() > BANK_SIZE)
        {
            pos = bank.instruments.rbegin()->first + 1;
        }
        else
        {
            pos = BANK_SIZE-1;
            while (!emptyslotWithID(rootID, bankID, pos))
            {
                pos -= 1;
                if(pos < 0)
                {
                    break;
                }
            }
        }
    }
    if (pos < 0)
        return -1; // the bank is full

    deletefrombank(rootID, bankID, pos);
    InstrumentEntry &instrRef = getInstrumentReference(rootID, bankID, pos);
    instrRef.used = true;
    instrRef.name = name;
    instrRef.filename = filename;
    instrRef.PADsynth_used = false;

    // see if PADsynth is used
    if (synth->getRuntime().CheckPADsynth)
    {
        XMLwrapper *xml = new XMLwrapper(synth);
        xml->checkfileinformation(getFullPath(rootID, bankID, pos));
        instrRef.PADsynth_used = xml->information.PADsynth_used;
        delete xml;
    }
    return 0;
}


void Bank::deletefrombank(size_t rootID, size_t bankID, unsigned int pos)
{
    if (pos >= BANK_SIZE)
    {
        synth->getRuntime().Log("Error, deletefrombank pos " + asString(pos) + " > BANK_SIZE"
                    + asString(BANK_SIZE));
        return;
    }
    getInstrumentReference(rootID, bankID, pos).clear();
}


size_t Bank::add_bank(string name, string , size_t rootID)
{
    size_t newIndex = getNewBankIndex(rootID);
    map<string, size_t>::iterator it = hints [rootID].find(name);
    if(it != hints [rootID].end())
    {
        size_t hintIndex = it->second;
        if(roots [rootID].banks.count(hintIndex) == 0) //don't use hint if bank id is already used
        {
            newIndex = hintIndex;
        }
    }
    else //add bank name to hints map
    {
        hints [rootID] [name] = newIndex;
    }

    roots [rootID].banks [newIndex].dirname = name;

    loadbank(rootID, newIndex);
    return newIndex;
}


InstrumentEntry &Bank::getInstrumentReference(size_t ninstrument)
{
    return getInstrumentReference(currentRootID, currentBankID, ninstrument);
}

InstrumentEntry &Bank::getInstrumentReference(size_t rootID, size_t bankID, size_t ninstrument)
{
    return roots [rootID].banks [bankID].instruments [ninstrument];
}

void Bank::addDefaultRootDirs()
{
    string bankdirs[] = {
        "/usr/share/yoshimi/banks",
        "/usr/local/share/yoshimi/banks",
        "/usr/share/zynaddsubfx/banks",
        "/usr/local/share/zynaddsubfx/banks",
        string(getenv("HOME")) + "/banks",
        "../banks"
    };
    for (unsigned int i = 0; i < (sizeof(bankdirs) / sizeof(bankdirs [0])); ++i)
    {
        addRootDir(bankdirs [i]);
    }
}


bool bankEntrySortFn(const BankEntry &e1, const BankEntry &e2)
{
    string d1 = e1.dirname;
    string d2 = e2.dirname;
    transform(d1.begin(), d1.end(), d1.begin(), ::toupper);
    transform(d2.begin(), d2.end(), d2.begin(), ::toupper);
    return d1 < d2;
}

size_t Bank::getNewRootIndex()
{
    if(roots.empty())
    {
        return 0;
    }

    return roots.rbegin()->first + 1;


}

size_t Bank::getNewBankIndex(size_t rootID)
{
    if(roots [rootID].banks.empty())
    {
        if(roots [rootID].bankIdStep <= 1)
        {
            return 0;
        }

        return roots [rootID].bankIdStep;
    }

    size_t idStep = 1;

    if(roots [rootID].bankIdStep == 0)
    {
        size_t startId = 127;
        size_t i;
        for(i = startId; i > 0; --i)
        {
            if(roots [rootID].banks.count(i) == 0)
            {
                break;
            }
        }
        if(i > 0) //id found
        {
            return i;
        }
    }
    else
    {
        idStep = roots [rootID].bankIdStep;
    }

    return roots [rootID].banks.rbegin()->first + idStep;
}

string Bank::getBankPath(size_t rootID, size_t bankID)
{
    if(roots.count(rootID) == 0 || roots [rootID].banks.count(bankID) == 0)
    {
        return string("");
    }
    if(roots [rootID].path.empty() || roots [rootID].banks [bankID].dirname.empty())
    {
        return string("");
    }
    string chkdir = getRootPath(rootID) + string("/") + roots [rootID].banks [bankID].dirname;
    if(chkdir.at(chkdir.size() - 1) == '/')
    {
        chkdir = chkdir.substr(0, chkdir.size() - 1);
    }
    return chkdir;
}

string Bank::getRootPath(size_t rootID)
{
    if(roots.count(rootID) == 0 || roots [rootID].path.empty())
    {
        return string("");
    }
    string chkdir = roots [rootID].path;
    if(chkdir.at(chkdir.size() - 1) == '/')
    {
        chkdir = chkdir.substr(0, chkdir.size() - 1);
    }

    return chkdir;
}

string Bank::getFullPath(size_t rootID, size_t bankID, size_t ninstrument)
{
    string bankPath = getBankPath(rootID, bankID);
    if(!bankPath.empty())
    {
        string instrFname = getInstrumentReference(rootID, bankID, ninstrument).filename;
        return bankPath + string("/") + instrFname;
    }
    return string("");

}

const BankEntryMap &Bank::getBanks(size_t rootID)
{
    return roots [rootID].banks;
}

const RootEntryMap &Bank::getRoots()
{
    return roots;
}

const BankEntry &Bank::getBank(size_t bankID)
{
    return roots [currentRootID].banks [bankID];
}


bool Bank::isPADsynth_used(unsigned int ninstrument)
{
    return getInstrumentReference(ninstrument).PADsynth_used;
}


void Bank::clearBankrootDirlist(void)
{
    roots.clear();
}


void Bank::removeRoot(size_t rootID)
{
    if(rootID == currentRootID)
    {
        currentRootID = 0;
    }
    roots.erase(rootID);
    setCurrentRootID(currentRootID);
}


bool Bank::changeRootID(size_t oldID, size_t newID)
{
    RootEntry oldRoot = roots [oldID];
    roots [oldID] = roots [newID];
    roots [newID] = oldRoot;
    setCurrentRootID(newID);
    RootEntryMap::iterator it;
    for(it = roots.begin(); it != roots.end(); ++it)
    {
        if(it->second.path.empty())
        {
            roots.erase(it);
        }
    }

    return true;
}

bool Bank::setCurrentRootID(size_t newRootID)
{
    if(roots.count(newRootID) == 0)
    {
        if(roots.size() == 0)
        {
            return false;
        }
        else
        {
            currentRootID = roots.begin()->first;
        }
    }
    else
    {
        currentRootID = newRootID;
    }

    setCurrentBankID(0);
    return true;
}

bool Bank::setCurrentBankID(size_t newBankID, bool ignoreMissing)
{
    if(roots [currentRootID].banks.count(newBankID) == 0)
    {
        if((roots [currentRootID].banks.size() == 0) || ignoreMissing)
        {
            return false;
        }
        else
        {
            newBankID = roots [currentRootID].banks.begin()->first;
        }
    }
    currentBankID = newBankID;
    return true;
}

size_t Bank::addRootDir(string newRootDir)
{
   // we need the size check to prevent weird behaviour if the name is just ./
    if(!isDirectory(newRootDir) || newRootDir.length() < 4)
    {
        return 0;
    }
    size_t newIndex = getNewRootIndex();
    roots [newIndex].path = newRootDir;
    return newIndex;
}

void Bank::parseConfigFile(XMLwrapper *xml)
{
    roots.clear();
    hints.clear();
    string nodename = "BANKROOT";
    for (size_t i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
    {

        if (xml->enterbranch(nodename, i))
        {
            string dir = xml->getparstr("bank_root");
            if(!dir.empty())
            {
                size_t newIndex = addRootDir(dir);
                if(newIndex != i)
                {
                    changeRootID(newIndex, i);
                }
                for(size_t k = 0; k < BANK_SIZE; k++)
                {
                    if(xml->enterbranch("bank_id", k))
                    {
                        string bankDirname = xml->getparstr("dirname");
                        hints [i] [bankDirname] = k;
                        xml->exitbranch();
                    }
                }
            }
            xml->exitbranch();
        }
    }

    if (roots.size() == 0)
    {
        addDefaultRootDirs();
    }

    rescanforbanks();

    setCurrentRootID(xml->getpar("root_current_ID", 0, 0, 127));
    setCurrentBankID(xml->getpar("bank_current_ID", 0, 0, 127));
}

void Bank::saveToConfigFile(XMLwrapper *xml)
{
    for (size_t i = 0; i < MAX_BANK_ROOT_DIRS; i++)
    {
        if (roots.count(i) > 0 && !roots [i].path.empty())
        {
            string nodename = "BANKROOT";

            xml->beginbranch(nodename, i);
            xml->addparstr("bank_root", roots [i].path);
            BankEntryMap::const_iterator it;
            for(it = roots [i].banks.begin(); it != roots [i].banks.end(); ++it)
            {
                xml->beginbranch("bank_id", it->first);
                xml->addparstr("dirname", it->second.dirname);
                xml->endbranch();
            }

            xml->endbranch();
        }
    }
    xml->addpar(string("root_current_ID"), currentRootID);
    xml->addpar(string("bank_current_ID"), currentBankID);

}
