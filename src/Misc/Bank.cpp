/*
    Bank.cpp - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2014-2018, Will Godfrey & others

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

    This file is a derivative of a ZynAddSubFX original.

    Modified January 2018
*/

#include <set>
#include <list>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <iostream>

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
#include "Misc/MiscFuncs.h"
#include "Misc/SynthEngine.h"

Bank::Bank(SynthEngine *_synth) :
    defaultinsname(string(" ")),
    xizext(".xiz"),
    xiyext(".xiy"),
    force_bank_dir_file(".bankdir"), // if this file exists in a directory, the
                                    // directory is considered a bank, even if
                                    // it doesn't contain an instrument file
    synth(_synth),
    currentRootID(0),
    currentBankID(0)
{
    roots.clear();
}


Bank::~Bank()
{
    roots.clear();
}


string Bank::getBankFileTitle()
{
    return synth->makeUniqueName("Root " + asString(currentRootID) + ", Bank " + asString(currentBankID) + " - " + getBankPath(currentRootID, currentBankID));
}


string Bank::getRootFileTitle()
{
    return synth->makeUniqueName("Root " + asString(currentRootID) + " - " + getRootPath(currentRootID));
}


// Get the name of an instrument from the bank
string Bank::getname(unsigned int ninstrument)
{
    if (emptyslot(ninstrument))
        return defaultinsname;
    return getInstrumentReference(ninstrument).name;
}


// Get the full path of an instrument from the current bank
string Bank::getfilename(unsigned int ninstrument)
{
    string fname = "";

    if (!emptyslot(ninstrument))
        fname = getFullPath(currentRootID, currentBankID, ninstrument);
    return fname;
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

    string newfilepath = getBankPath(currentRootID, currentBankID);
    if (newfilepath.at(newfilepath.size() - 1) != '/')
        newfilepath += "/";

    int slot = (newslot >= 0) ? newslot + 1 : ninstrument + 1;
    string filename = "0000" + asString(slot);

    filename = filename.substr(filename.size() - 4, 4) + "-" + newname + xizext;
    legit_filename(filename);

    int chk = -1;
    int chk2 = -1;
    newfilepath += filename;
    string oldfilepath = setExtension(getFullPath(currentRootID, currentBankID, ninstrument), xizext);
    chk = rename(oldfilepath.c_str(), newfilepath.c_str());
    if (chk < 0)
    {
        synth->getRuntime().Log("setName failed renaming "
                + oldfilepath + " -> "
                + newfilepath + ": " + string(strerror(errno)));
    }

    newfilepath = setExtension(newfilepath, xiyext);
    oldfilepath = setExtension(oldfilepath, xiyext);
    chk2 = rename(oldfilepath.c_str(), newfilepath.c_str());
    if (chk2 < 0)
    {
        synth->getRuntime().Log("setName failed renaming "
                + oldfilepath + " -> "
                + newfilepath + ": " + string(strerror(errno)));
    }

    if (chk < 0 && chk2 < 0)
        return false;
    InstrumentEntry &instrRef = getInstrumentReference(currentRootID, currentBankID, ninstrument);
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
bool Bank::clearslot(unsigned int ninstrument)
{
    int chk = 0;
    int chk2 = 0; // to stop complaints
    if (emptyslot(ninstrument))
        return true;
    string tmpfile = setExtension(getFullPath(currentRootID, currentBankID, ninstrument), xiyext);

    if (isRegFile(tmpfile))
    {
        chk = remove(tmpfile.c_str());
        if (chk < 0)
            synth->getRuntime().Log(asString(ninstrument) + " Failed to remove " + tmpfile);
    }
    tmpfile = setExtension(tmpfile, xizext);
    if (isRegFile(tmpfile))
    {
        chk2 = remove(tmpfile.c_str());
        if (chk2 < 0)
            synth->getRuntime().Log(asString(ninstrument) + " Failed to remove " + tmpfile);
    }
    if (chk < 0 || chk2 < 0)
        return false;

    deletefrombank(currentRootID, currentBankID, ninstrument);
    return true;
}


bool Bank::savetoslot(size_t rootID, size_t bankID, int ninstrument, int npart)
{
    string filepath = getBankPath(rootID, bankID);
    string name = synth->part[npart]->Pname;
    if (filepath.at(filepath.size() - 1) != '/')
        filepath += "/";
    clearslot(ninstrument);
    string filename = "0000" + asString(ninstrument + 1);
    filename = filename.substr(filename.size() - 4, 4)
               + "-" + name + xizext;
    legit_filename(filename);

    string fullpath = filepath + filename;
    bool ok1 = true;
    bool ok2 = true;
    int saveType = synth->getRuntime().instrumentFormat;
    if (isRegFile(fullpath))
    {
        int chk = remove(fullpath.c_str());
        if (chk < 0)
        {
            synth->getRuntime().Log("saveToSlot failed to unlink " + fullpath
                        + ", " + string(strerror(errno)));
            return false;
        }
    }
    if (saveType & 1) // legacy
        ok2 = synth->part[npart]->saveXML(fullpath, false);

    fullpath = setExtension(fullpath, xiyext);
    if (isRegFile(fullpath))
    {
        int chk = remove(fullpath.c_str());
        if (chk < 0)
        {
            synth->getRuntime().Log("saveToSlot failed to unlink " + fullpath
                        + ", " + string(strerror(errno)));
            return false;
        }
    }

    if (saveType & 2) // Yoshimi format
        ok1 = synth->part[npart]->saveXML(fullpath, true);
    if (!ok1 || !ok2)
        return false;

    filepath += force_bank_dir_file;
    FILE *tmpfile = fopen(filepath.c_str(), "w+");
    fputs (YOSHIMI_VERSION, tmpfile);
    fclose(tmpfile);
    addtobank(rootID, bankID, ninstrument, filename, name);
    return true;
}


//Gets a bank name
string Bank::getBankName(int bankID, size_t rootID)
{
    if (rootID > 0x7f)
        rootID = currentRootID;
    if (roots [rootID].banks.count(bankID) == 0)
        return "";
    return string(roots [rootID].banks [bankID].dirname);
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
        synth->getRuntime().Log("Failed to rename " + getBankName(bankID)
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
        if (isRegFile(chkpath))
        {
            if (chkpath.rfind(".xiz") != string::npos && isRegFile(setExtension(chkpath, xiyext)))
                continue; // don't want .xiz if there is .xiy

            xizpos = candidate.rfind(".xiy");
            if (xizpos == string::npos)
                xizpos = candidate.rfind(xizext);

            if (xizpos != string::npos)
            {
                if (xizext.size() == (candidate.size() - xizpos))
                {
                    // just NNNN-<name>.xiz files please
                    // sa verific daca e si extensia dorita

                    // sorry Cal. They insisted :(
                    int chk = findSplitPoint(candidate);
                    if (chk > 0)
                    {
                        int instnum = string2int(candidate.substr(0, chk));
                        // remove "NNNN-" and .xiz extension for instrument name
                        // modified for numbered instruments with < 4 digits
                        string instname = candidate.substr(chk + 1, candidate.size() - xizext.size() - chk - 1);
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

// Creates a new bank and copies in the contents of the external one
unsigned int Bank::importBank(string importdir, size_t rootID, unsigned int bankID)
{
    if (rootID > 0x7f)
        rootID = currentRootID;
    string name = "";
    bool ok = true;
    if (roots.count(rootID) == 0)
    {
        name = "Root ID " + to_string(int(rootID)) + " doesn't exist";
        ok = false;
    }

    if (ok && roots [rootID].banks.count(bankID) != 0)
    {
        name = "Bank " + to_string(bankID) + " already contains " + getBankName(bankID, rootID);
        ok = false;
    }

    if (ok)
    {
        DIR *dir = opendir(importdir.c_str());
        if (dir == NULL)
        {
            name = "Can't find " + importdir;
            ok = false;
        }
        else
        {
            if (!newIDbank(findleafname(importdir), bankID, rootID))
            {
                name = "Can't create bank " + findleafname(importdir);
                ok = false;
            }
            else
            {
                int count = 0;
                bool missing = false;
                struct dirent *fn;
                string exportfile = getRootPath(rootID) + "/" + getBankName(bankID, rootID);
                while ((fn = readdir(dir)))
                {
                    string nextfile = string(fn->d_name);
                    if (nextfile.rfind(".xiy") != string::npos || nextfile.rfind(".xiz") != string::npos)
                    {
                        ++count;
                        int pos = -1; // default for un-numbered
                        int slash = nextfile.rfind("/") + 1;
                        int hyphen = nextfile.rfind("-");
                        if (hyphen > slash && (hyphen - slash) <= 4)
                            pos = stoi(nextfile.substr(slash, hyphen)) - 1;

                        if (copyFile(importdir + "/" + nextfile, exportfile + "/" + nextfile))
                            missing = true;
                        string stub;
                        if (pos >= -1)
                            stub = findleafname(nextfile).substr(hyphen + 1);
                        else
                            stub = findleafname(nextfile);
                        if (!isDuplicate(rootID, bankID, pos, nextfile))
                        {
                            if (addtobank(rootID, bankID, pos, nextfile, stub))
                                missing = true;
                        }
                    }
                }
                name = importdir;
                if (count == 0)
                    name += " but no valid instruments found";
                else if (missing)
                    name += " but failed to copy some instruments";
            }
        }
    }
    unsigned int msgID = miscMsgPush(name);
    if (!ok)
        msgID |= 0x1000;
    return msgID;
}


bool Bank::isDuplicate(size_t rootID, size_t bankID, int pos, const string filename)
{
    cout << filename << " count " << roots [rootID].banks.count(bankID) << endl;
    string path = getRootPath(rootID) + "/" + getBankName(bankID, rootID) + "/" + filename;
    if (isRegFile(setExtension(path, xiyext)) && filename.rfind(xizext) < string::npos)
        return 1;
    if (isRegFile(setExtension(path, xizext)) && filename.rfind(xiyext) < string::npos)
    {
        InstrumentEntry &Ref = getInstrumentReference(rootID, bankID, pos);
        Ref.yoshiType = true;
        return 1;
    }
    return 0;
}


// Makes a new bank with known ID. Does *not* make it current
bool Bank::newIDbank(string newbankdir, unsigned int bankID, size_t rootID)
{
    if (!newbankfile(newbankdir, rootID))
        return false;
    roots [currentRootID].banks [bankID].dirname = newbankdir;
    hints [currentRootID] [newbankdir] = bankID; // why do we need this?
    return true;
}


// Performs the actual file operation for new banks
bool Bank::newbankfile(string newbankdir, size_t rootID)
{
     if (getRootPath(currentRootID).empty())
    {
        synth->getRuntime().Log("Current bank root directory not set");
        return false;
    }
    string newbankpath = getRootPath(rootID);
    if (newbankpath.at(newbankpath.size() - 1) != '/')
        newbankpath += "/";
    newbankpath += newbankdir;
    int result = mkdir(newbankpath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (result < 0)
    {
        synth->getRuntime().Log("Failed to mkdir " + newbankpath);
        return false;
    }
    else
        synth->getRuntime().Log("mkdir " + newbankpath + " succeeded");
    string forcefile = newbankpath;
    if (forcefile.at(forcefile.size() - 1) != '/')
        forcefile += "/";
    forcefile += force_bank_dir_file;
    FILE *tmpfile = fopen(forcefile.c_str(), "w+");
    fputs (YOSHIMI_VERSION, tmpfile);
    fclose(tmpfile);
    return true;
}


// Removes a bank and all its contents
unsigned int Bank::removebank(unsigned int bankID, size_t rootID)
{
    int chk = 0;
    if (rootID == 255)
        rootID = currentRootID;
    if (roots.count(rootID) == 0)
    {
        chk = 0x1000 | miscMsgPush("Root " + to_string(int(rootID)) + " is empty!");
        return chk;
    }
    string bankName = getBankPath(rootID, bankID);
    string IDfile = bankName + "/.bankdir";
    FILE *tmpfile = fopen(IDfile.c_str(), "w+");
    if (!tmpfile)
        chk = 0x1000 | miscMsgPush("Can't delete from this location.");
    else
        fclose(tmpfile);

    int ck1 = 0;
    int ck2 = 0;
    string name;
    for (int inst = 0; inst < BANK_SIZE; ++ inst)
    {
        if (!roots [rootID].banks [bankID].instruments [inst].name.empty())
        {
            name = setExtension(getFullPath(currentRootID, bankID, inst), xiyext);
            if (isRegFile(name))
                ck1 = remove(name.c_str());
            else
                ck1 = 0;

            name = setExtension(name, xizext);
            if (isRegFile(name))
                ck2 = remove(name.c_str());
            else
                ck2 = 0;

            if (ck1 == 0 && ck2 == 0)
                deletefrombank(rootID, bankID, inst);
            else if (chk == 0) // only want to name one entry
                    chk = 0x1000 | miscMsgPush(findleafname(name) + ". Others may also still exist.");
        }
    }
    if (chk > 0)
        return chk;

    if (isRegFile(IDfile))
    { // only removed when bank cleared
        if (remove(IDfile.c_str()) != 0)
        {
            chk = 0x1000 | miscMsgPush(findleafname(name));
            return chk;
        }
    }

    if (remove(bankName.c_str()) != 0)
    {
        chk = 0x1000 | miscMsgPush(bankName + ". Unrecognised contents still exist.");
        return chk;
    }

    roots [rootID].banks.erase(bankID);
    if (rootID == currentRootID && bankID == currentBankID)
        setCurrentBankID(0);
    chk = miscMsgPush(bankName);
    return chk;
}


// Swaps a slot with another
bool Bank::swapslot(unsigned int n1, unsigned int n2)
{
    if (n1 == n2)
        return true;

    if (emptyslot(n1) && emptyslot(n2))
        return true;
    if (emptyslot(n1)) // make the empty slot the destination
    {
        if (!setname(n2, getname(n2), n1))
            return false;
        getInstrumentReference(n1) = getInstrumentReference(n2);
        getInstrumentReference(n2).clear();
    }
    else if (emptyslot(n2)) // this is just a movement to an empty slot
    {
        if (!setname(n1, getname(n1), n2))
            return false;
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
        if (!setname(n2, getname(n2), n1))
            return false;
        if (!setname(n1, getname(n1), n2))
            return false;
        InstrumentEntry instrTmp = instrRef1;
        instrRef1 = instrRef2;
        instrRef2 = instrTmp;
    }
    return true;
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
            synth->getRuntime().Log("Failed to open bank directory candidate " + chkdir);
            continue;
        }
        struct dirent *fname;
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
    instrRef.ADDsynth_used = false;
    instrRef.SUBsynth_used = false;
    instrRef.yoshiType = false;

    // see which engines are used
    if (synth->getRuntime().checksynthengines)
    {
        string checkfile = setExtension(getFullPath(rootID, bankID, pos), xiyext);
        if (!isRegFile(checkfile))
            checkfile = setExtension(getFullPath(rootID, bankID, pos), xizext);
        XMLwrapper *xml = new XMLwrapper(synth);
        xml->checkfileinformation(checkfile);
        instrRef.PADsynth_used = xml->information.PADsynth_used;
        instrRef.ADDsynth_used = xml->information.ADDsynth_used;
        instrRef.SUBsynth_used = xml->information.SUBsynth_used;
        instrRef.yoshiType = xml->information.yoshiType;
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
        localPath("/banks"),
        "end"
    };
    int i = 0;

    while (bankdirs [i] != "end")
    {
        addRootDir(bankdirs [i]);
        ++ i;
    }

    while ( i >= 0)
    {
        changeRootID(i, (i * 5) + 5);
        -- i;
    }
    rescanforbanks();
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


int Bank::engines_used(unsigned int ninstrument)
{
    int tmp = getInstrumentReference(ninstrument).ADDsynth_used
            | (getInstrumentReference(ninstrument).SUBsynth_used << 1)
            | (getInstrumentReference(ninstrument).PADsynth_used << 2)
            | (getInstrumentReference(ninstrument).yoshiType << 3);
    return tmp;
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
    RootEntryMap::iterator it = roots.begin();
    while(it != roots.end())
    {
        if(it->second.path.empty())
        {
            roots.erase(it++);
        }
        else
        {
            ++it;
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
}
