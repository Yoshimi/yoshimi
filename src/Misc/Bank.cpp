/*
    Bank.cpp - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2014-2019, Will Godfrey & others

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

*/

#include <set>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <iostream>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <vector>
#include <algorithm>

#include "Misc/XMLwrapper.h"
#include "Misc/Config.h"
#include "Misc/Bank.h"
#include "Misc/SynthEngine.h"
#include "Misc/TextMsgBuffer.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/FormatFuncs.h"

using file::make_legit_filename;
using file::isRegularFile;
using file::isDirectory;
using file::renameDir;
using file::deleteDir;
using file::copyFile;
using file::renameFile;
using file::deleteFile;
using file::findLeafName;
using file::setExtension;
using file::extendLocalPath;
using file::saveText;

using func::asString;
using func::string2int;
using func::findSplitPoint;

using std::to_string;
using std::string;
using std::cout;
using std::endl;


Bank::Bank(SynthEngine *_synth) :
    defaultinsname(string(" ")),
    force_bank_dir_file(".bankdir"), // if this file exists in a directory, the
                                    // directory is considered a bank, even if
                                    // it doesn't contain an instrument file
    synth(_synth)
{
    InstrumentsInBanks = 0,
    BanksInRoots = 0;
    roots.clear();

    //TestFunc(456); // just for testing
}


Bank::~Bank()
{
    roots.clear();
}


string Bank::getBankFileTitle(size_t root, size_t bank)
{
    return synth->makeUniqueName("Root " + asString(root) + ", Bank " + asString(bank) + " - " + getBankPath(root, bank));
}




string Bank::getRootFileTitle(size_t root)
{
    return synth->makeUniqueName("Root " + asString(root) + " - " + getRootPath(root));
}


int Bank::getType(unsigned int ninstrument, size_t bank, size_t root)
{
    if (emptyslot(root, bank, ninstrument))
        return -1;
    return getInstrumentReference(root, bank, ninstrument).type;
}


// Get the name of an instrument from the bank
string Bank::getname(unsigned int ninstrument, size_t bank, size_t root)
{
    if (emptyslot(root, bank, ninstrument))
        return defaultinsname;
    return getInstrumentReference(root, bank, ninstrument).name;
}


// Get the numbered name of an instrument from the bank
string Bank::getnamenumbered(unsigned int ninstrument, size_t bank, size_t root)
{
    if (emptyslot(root, bank, ninstrument))
        return defaultinsname;
    string strRet = asString(ninstrument + 1) + ". " + getname(ninstrument, bank, root);
    return strRet;
}


// Changes the instrument name in place
int Bank::setInstrumentName(string name, int slot, size_t bank, size_t root)
{
    string result;
    string slotNum = to_string(slot + 1) + ". ";
    bool fail = false;
    if (emptyslot(root, bank, slot))
    {
        result = "No instrument on slot " + slotNum;
        fail = true;
    }
    else if (!moveInstrument(slot, name, slot, bank, bank, root, root))
    {
        result = "Could not change name of slot " + slotNum;
        fail = true;
    }
    else
        result = slotNum + name;
    int msgID = synth->textMsgBuffer.push(result);
    if (fail)
        msgID |= 0xFF0000;
    return msgID;
}


// Changes the name and location of an instrument (and the filename)
bool Bank::moveInstrument(unsigned int ninstrument, string newname, int newslot, size_t oldBank, size_t newBank, size_t oldRoot, size_t newRoot)
{
    if (emptyslot(oldRoot, oldBank, ninstrument))
        return false;

    string newfilepath = getBankPath(newRoot, newBank);
    if (newfilepath.at(newfilepath.size() - 1) != '/')
        newfilepath += "/";

    int slot = (newslot >= 0) ? newslot + 1 : ninstrument + 1;
    string filename = "0000" + asString(slot);

    filename = filename.substr(filename.size() - 4, 4) + "-" + newname + EXTEN::zynInst;
    make_legit_filename(filename);

    bool chk = false;
    bool chk2 = false;
    newfilepath += filename;
    string oldfilepath = setExtension(getFullPath(oldRoot, oldBank, ninstrument), EXTEN::zynInst);
    chk = renameFile(oldfilepath, newfilepath);

    newfilepath = setExtension(newfilepath, EXTEN::yoshInst);
    oldfilepath = setExtension(oldfilepath, EXTEN::yoshInst);
    chk2 = renameFile(oldfilepath, newfilepath);

    if (chk == false && chk2 == false)
    {
        synth->getRuntime().Log("failed changing " + oldfilepath + " to " + newfilepath + ": " + string(strerror(errno)));
        return false;
    }
    InstrumentEntry &instrRef = getInstrumentReference(oldRoot, oldBank, ninstrument);
    instrRef.name = newname;
    instrRef.filename = filename;
    return true;
}


// Check if there is no instrument on a slot from the bank
bool Bank::emptyslot(size_t rootID, size_t bankID, unsigned int ninstrument)
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
string Bank::clearslot(unsigned int ninstrument, size_t rootID, size_t bankID)
{
    bool chk = true;
    bool chk2 = true; // to stop complaints
    if (emptyslot(rootID, bankID, ninstrument)) // this is not an error
        return (". None found at slot " + to_string(ninstrument + 1));

    string tmpfile = setExtension(getFullPath(rootID, bankID, ninstrument), EXTEN::yoshInst);
    if (isRegularFile(tmpfile))
        chk = deleteFile(tmpfile);

    tmpfile = setExtension(tmpfile, EXTEN::zynInst);
    if (isRegularFile(tmpfile))
        chk2 = deleteFile(tmpfile);
    string instName = getname(ninstrument, bankID, rootID);
    string result;
    if (chk && chk2)
    {
        deletefrombank(rootID, bankID, ninstrument);
        result = "d ";
    }
    else
    {
        result = " FAILED Could not delete ";
        if (chk && !chk2)
            instName += EXTEN::zynInst;
        else if (!chk && chk2)
            instName += EXTEN::yoshInst;
        /*
         * done this way so that if only one type fails
         * it is identified, but if both are present and
         * can't be deleted it doesn't mark the extension.
         */
    }
    return (result + "'" + instName + "' from slot " + to_string(ninstrument + 1));
}


bool Bank::savetoslot(size_t rootID, size_t bankID, int ninstrument, int npart)
{
    string filepath = getBankPath(rootID, bankID);
    string name = synth->part[npart]->Pname;
    if (filepath.at(filepath.size() - 1) != '/')
        filepath += "/";
    clearslot(ninstrument, rootID, bankID);
    string filename = "0000" + asString(ninstrument + 1);
    filename = filename.substr(filename.size() - 4, 4)
               + "-" + name + EXTEN::zynInst;
    make_legit_filename(filename);

    string fullpath = filepath + filename;
    bool ok1 = true;
    bool ok2 = true;
    int saveType = synth->getRuntime().instrumentFormat;
    if (isRegularFile(fullpath))
    {
        if (!deleteFile(fullpath))
        {
            synth->getRuntime().Log("saveToSlot failed to unlink " + fullpath);
            return false;
        }
    }
    if (saveType & 1) // legacy
        ok2 = synth->part[npart]->saveXML(fullpath, false);

    fullpath = setExtension(fullpath, EXTEN::yoshInst);
    if (isRegularFile(fullpath))
    {
        if (!deleteFile(fullpath))
        {
            synth->getRuntime().Log("saveToSlot failed to unlink " + fullpath);
            return false;
        }
    }

    if (saveType & 2) // Yoshimi format
        ok1 = synth->part[npart]->saveXML(fullpath, true);
    if (!ok1 || !ok2)
        return false;

    saveText(string(YOSHIMI_VERSION), filepath + force_bank_dir_file);
    addtobank(rootID, bankID, ninstrument, filename, name);
    return true;
}


//Gets a bank name
string Bank::getBankName(int bankID, size_t rootID)
{
    if (rootID > 0x7f)
        rootID = synth->getRuntime().currentRoot;
    if (roots [rootID].banks.count(bankID) == 0)
        return "";
    return string(roots [rootID].banks [bankID].dirname);
}


bool Bank::isDuplicateBankName(size_t rootID, string name)
{
    //if(roots.count(rootID) == 0)
        //return false;
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
    {
        string check = getBankName(i,rootID);
        if (check > "" && check == name)
            return true;
        if (check > "")
            cout << check << endl;
    }
    return false;
}


// finds the number of instruments in a bank
int Bank::getBankSize(int bankID)
{
    int found = 0;

    for (int i = 0; i < BANK_SIZE; ++ i)
        if (!roots [synth->getRuntime().currentRoot].banks [bankID].instruments [i].name.empty())
            found += 1;
    return found;
}


int Bank::changeBankName(size_t rootID, size_t bankID, string newName)
{
    std::string filename = newName;
    std::string oldName = getBankName(bankID, rootID);
    make_legit_filename(filename);
    std::string newfilepath = getRootPath(synth->getRuntime().currentRoot) + "/" + filename;
    std::string reply = "";
    bool failed = false;
    if (!renameDir(getBankPath(synth->getRuntime().currentRoot,bankID), newfilepath))
    {
        reply = "Could not change bank '" + oldName + "' in root " + to_string(rootID);
        failed = true;
    }
    else
    {
        roots [synth->getRuntime().currentRoot].banks [bankID].dirname = newName;
        reply = "Changed " + oldName + " to " + newName;
    }

    int msgID = synth->textMsgBuffer.push(reply);
    if (failed)
        msgID |= 0xFF0000;
    return msgID;
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
            || candidate.size() <= (EXTEN::zynInst.size() + 2)) // actually a 3 char filename!
            continue;
        chkpath = bankdirname;
        if (chkpath.at(chkpath.size() - 1) != '/')
            chkpath += "/";
        chkpath += candidate;
        if (isRegularFile(chkpath))
        {
            if (chkpath.rfind(EXTEN::zynInst) != string::npos && isRegularFile(setExtension(chkpath, EXTEN::yoshInst)))
                continue; // don't want .xiz if there is .xiy

            xizpos = candidate.rfind(EXTEN::yoshInst);
            if (xizpos == string::npos)
                xizpos = candidate.rfind(EXTEN::zynInst);

            if (xizpos != string::npos)
            {
                if (EXTEN::zynInst.size() == (candidate.size() - xizpos))
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
                        string instname = candidate.substr(chk + 1, candidate.size() - EXTEN::zynInst.size() - chk - 1);
                        addtobank(rootID, banknum, instnum - 1, candidate, instname);
                    }
                    else
                    {
                        string instname = candidate.substr(0, candidate.size() -  EXTEN::zynInst.size());
                        addtobank(rootID, banknum, -1, candidate, instname);
                    }
                }
            }
        }
    }
    closedir(dir);
    return true;
}


// Creates an external bank and copies in the contents of the IDd one
string Bank::exportBank(string exportdir, size_t rootID, unsigned int bankID)
{
    string name = "";
    string sourcedir = "";
    bool ok = true;
    if (roots.count(rootID) == 0)
    {
        name = "Root ID " + to_string(int(rootID)) + " doesn't exist";
        ok = false;
    }
    if (ok && roots [rootID].banks.count(bankID) == 0)
    {
        name = "Bank " + to_string(bankID) + " is empty";
        ok = false;
    }
    else
        sourcedir = getRootPath(rootID) + "/" + getBankName(bankID, rootID);

    if (ok && isDirectory(exportdir))
    {
        ok = false;
        name = "Can't overwrite existing directory";
    }
    if (ok)
    {
        int result = mkdir(exportdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (result < 0)
        {
            name = "Can't create external bank " + findLeafName(exportdir);
            ok = false;
        }
        else
        {
            DIR *dir = opendir(sourcedir.c_str());
            struct dirent *fn;
            int count = 0;
            int missing = 0;
            while ((fn = readdir(dir)))
            {
                string nextfile = string(fn->d_name);
                if (copyFile(sourcedir + "/" + nextfile, exportdir + "/" + nextfile))
                    ++missing;
                else
                    ++count;
            }
            if (count > 0)
            {
                name = "Copied out " + to_string(count) + " files to " + exportdir + " ";
                if (missing > 2) // seem to get 2 phantoms :(
                    name +=( "but failed to transfer" + to_string(missing));
            }
            else
            {
                name = "to transfer to " + exportdir; // failed
                ok = false;
            }
        }
    }

    if (ok)
        name = " " + name; // need the extra space
    else
        name = " FAILED " + name;
    return name;
}


// Creates a new bank and copies in the contents of the external one
string Bank::importBank(string importdir, size_t rootID, unsigned int bankID)
{
    string name = "";
    bool ok = true;
    bool partial = false;
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
            string bankname = findLeafName(importdir);
            int repeats = 0;
            string suffix = "";
            while (isDirectory(getRootPath(rootID) + "/" + bankname + suffix))
            {
                ++repeats;
                suffix = "~" + to_string(repeats);
            }
            bankname += suffix;
            if (!newIDbank(bankname, bankID, rootID))
            {
                name = "Can't create bank " + bankname + " in root " + getRootPath(rootID);
                ok = false;
            }
            else
            {
                int count = 0;
                int total = -2; // seem to get 2 phantoms :(
                bool missing = false;
                struct dirent *fn;
                string exportfile = getRootPath(rootID) + "/" + getBankName(bankID, rootID);
                while ((fn = readdir(dir)))
                {
                    string nextfile = string(fn->d_name);                    if (nextfile.rfind(".bankdir") != string::npos)
                        continue; // new version will be generated
                    ++total;
                    if (nextfile.rfind(EXTEN::yoshInst) != string::npos || nextfile.rfind(EXTEN::zynInst) != string::npos)
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
                            stub = findLeafName(nextfile).substr(hyphen + 1);
                        else
                            stub = findLeafName(nextfile);
                        if (!isDuplicate(rootID, bankID, pos, nextfile))
                        {
                            if (addtobank(rootID, bankID, pos, nextfile, stub))
                                missing = true;
                        }
                    }
                }
                name = importdir;
                if (count == 0)
                {
                    partial = true;
                    name += " : No valid instruments found";
                }
                else if (missing)
                {
                    partial = true;
                    name += " : Failed to copy some instruments";
                }
                else if (count < total)
                {
                    partial = true;
                    name = name + " : Ignored " + to_string(total - count)  + " unrecognised items";
                }
            }
        }
    }

    if (!ok)
        name = " FAILED " + name;
    else if (!partial)
        name = " ed" + name;
    return name;
}


bool Bank::isDuplicate(size_t rootID, size_t bankID, int pos, const string filename)
{
    //cout << filename << " count " << roots [rootID].banks.count(bankID) << endl;
    string path = getRootPath(rootID) + "/" + getBankName(bankID, rootID) + "/" + filename;
    if (isRegularFile(setExtension(path, EXTEN::yoshInst)) && filename.rfind(EXTEN::zynInst) < string::npos)
        return 1;
    if (isRegularFile(setExtension(path, EXTEN::zynInst)) && filename.rfind(EXTEN::yoshInst) < string::npos)
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
    if (rootID == UNUSED)
        rootID = synth->getRuntime().currentRoot; // shouldn't be needed!

    if (!newbankfile(newbankdir, rootID))
        return false;
    roots [synth->getRuntime().currentRoot].banks [bankID].dirname = newbankdir;
    hints [synth->getRuntime().currentRoot] [newbankdir] = bankID; // why do we need this?
    return true;
}


// Performs the actual file operation for new banks
bool Bank::newbankfile(string newbankdir, size_t rootID)
{
     if (getRootPath(synth->getRuntime().currentRoot).empty())
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
    saveText(string(YOSHIMI_VERSION), forcefile + force_bank_dir_file);
    return true;
}


// Removes a bank and all its contents
string Bank::removebank(unsigned int bankID, size_t rootID)
{
    if (rootID == UNUSED)
        rootID = synth->getRuntime().currentRoot;
    if (roots.count(rootID) == 0) // not an error
        return ("Root " + to_string(int(rootID)) + " is empty!");

    string bankName = getBankPath(rootID, bankID);
    // ID bank and test for writeable
    string IDfile = bankName + "/.bankdir";
    if (!saveText(string(YOSHIMI_VERSION), IDfile))
        return (" FAILED Can't delete from this location.");

    bool ck1 = true;
    bool ck2 = true;
    int chk = 0;
    string name;
    string failed;
    for (int inst = 0; inst < BANK_SIZE; ++ inst)
    {
        if (!roots [rootID].banks [bankID].instruments [inst].name.empty())
        {
            name = setExtension(getFullPath(synth->getRuntime().currentRoot, bankID, inst), EXTEN::yoshInst);
            if (isRegularFile(name))
                ck1 = deleteFile(name);
            else
                ck1 = true;

            name = setExtension(name, EXTEN::zynInst);
            if (isRegularFile(name))
                ck2 = deleteFile(name);
            else
                ck2 = true;

            if (ck1 == true && ck2 == true)
                deletefrombank(rootID, bankID, inst);
            else
            {
                ++ chk;
                if (chk == 0) // only want to name one entry
                    failed = (" FAILED Can't remove " + findLeafName(name) + ". Others may also still exist.");
            }
        }
    }
    if (chk > 0)
        return failed;

    // ID file only removed when bank cleared
    if (deleteFile(IDfile))
        chk = 1;

    if (chk > 0)
    chk = deleteDir(bankName);
    if (chk == 0)
        return (" FAILED Can't remove " + bankName + ". Unrecognised contents may still exist.");

    roots [rootID].banks.erase(bankID);
    if (rootID == synth->getRuntime().currentRoot && bankID == synth->getRuntime().currentBank)
        setCurrentBankID(0);
    return ("d " + bankName);
}


// Swaps a slot with another
string Bank::swapslot(unsigned int n1, unsigned int n2, size_t bank1, size_t bank2, size_t root1, size_t root2)
{
    if (n1 == n2 && bank1 == bank2 && root1 == root2)
        return " Can't swap with itself!";

    //std::cout << "first " << getname(n1, bank1, root1) << "   second " << getname(n2, bank2, root2) << endl;
    /*
     * path entries will always have either .xiy or .xiz
     * otherwise they would not have been seen at all
     * however we test for, and move both if they exist
     */
    //std::cout << "first ref" << getFullPath(root1, bank1, n1) << endl;
    string message = "";
    bool ok = true;

    if (emptyslot(root1, bank1, n1) && emptyslot(root2, bank2, n2))
        return " Nothing to swap!";

    if (emptyslot(root1, bank1, n1) || emptyslot(root2, bank2, n2))
    { // this is just a movement to an empty slot
        if (emptyslot(root1, bank1, n1)) // make the empty slot the destination
        {
            if (!moveInstrument(n2, getname(n2, bank2, root2), n1, bank2, bank1, root2, root1))
            {
                ok = false;
                message = " Can't write to " + getname(n2, bank2, root2);
            }
            else
                message = std::to_string(n2) + " " + getname(n2, bank2, root2);
            getInstrumentReference(root1, bank1, n1) = getInstrumentReference(root2, bank2, n2);
            getInstrumentReference(root2, bank2, n2).clear();
        }
        else
        {
            if (!moveInstrument(n1, getname(n1, bank1, root1), n2, bank1, bank2, root1, root2))
            {
                ok = false;
                message = " Can't write to " + getname(n1, bank1, root1);
            }
            else
                message = std::to_string(n2) + " " + getname(n1, bank1, root1);
            getInstrumentReference(root2, bank2, n2) = getInstrumentReference(root1, bank1, n1);
            getInstrumentReference(root1, bank1, n1).clear();
        }
        if (!ok)
        {
            rescanforbanks(); // might have corrupted it
            return (" FAILED" + message);
        }
        else
            return (" Moved to " + message);
    }


    // if both slots are used
    string firstName = getname(n1, bank1, root1);
    string secondName = getname(n2, bank2, root2);
    if (firstName == secondName)
        return " Can't swap instruments with identical names.";

    InstrumentEntry &instrRef1 = getInstrumentReference(root1, bank1, n1);
    InstrumentEntry &instrRef2 = getInstrumentReference(root2, bank2, n2);

    if (!moveInstrument(n2, secondName, n1, bank2, bank1, root2, root1))
    {
        ok = false;
        message = " Can't change " + secondName;
    }

    if (!moveInstrument(n1, firstName, n2, bank1, bank2, root1, root2))
    {
        ok = false;
        message = " Can't change " + firstName;
    }
    else
    {
        InstrumentEntry instrTmp = instrRef1;
        instrRef1 = instrRef2;
        instrRef2 = instrTmp;
    }

    if (!ok)
    {
        rescanforbanks(); // might have corrupted it
        return (" FAILED" + message);
    }

    return ("ped " + firstName + " with " + secondName);
}


// Intelligently moves or swaps banks preserving instrument details
string Bank::swapbanks(unsigned int firstID, unsigned int secondID, size_t firstRoot, size_t secondRoot)
{
    string firstname;
    string secondname;
    int moveType = 0;

    if (firstID == secondID && firstRoot == secondRoot)
        return " Can't swap with itself!";

    firstname = getBankName(firstID, firstRoot);
    secondname = getBankName(secondID, secondRoot);
    if (firstname.empty() && secondname.empty())
        return " Nothing to swap!";


    if (firstRoot != secondRoot)
    {
        if (isDuplicateBankName(firstRoot, secondname))
            return (" FAILED " + secondname + " already exists in root " + to_string(firstRoot));

        if (isDuplicateBankName(secondRoot, firstname))
            return (" FAILED " + firstname + " already exists in root " + to_string(secondRoot));
    }

    if (firstRoot != secondRoot) // do physical move first
    {
        string firstBankPath = getBankPath(firstRoot, firstID);
        string secondBankPath = getBankPath(secondRoot, secondID);
        string newfirstBankPath = getRootPath(secondRoot) + "/" + firstname;
        string newsecondBankPath = getRootPath(firstRoot) + "/" + secondname;
        string tempBankPath = getRootPath(firstRoot) + "/tempfile";
        if (secondBankPath == "") // move only
        {
            if (!renameDir(firstBankPath, (getRootPath(secondRoot) + "/" + firstname)))
            {
                synth->getRuntime().Log("move to " + to_string(secondRoot) + ": " + string(strerror(errno)), 2);
                return (" FAILED Can't move from root " + to_string(firstRoot) + " to " + to_string(secondRoot));
            }
        }
        else if(firstBankPath == "") // move only
        {
            if (!renameDir(secondBankPath, (getRootPath(firstRoot) + "/" + secondname)))
            {
                synth->getRuntime().Log("move to " + to_string(firstRoot) + ": " + string(strerror(errno)), 2);
                return (" FAILED Can't move from root " + to_string(secondRoot) + " to " + to_string(firstRoot));
            }
        }
        else // actual swap
        {
            // due to possible identical names we need to go via a temp file

            //std::cout << "first " << firstBankPath << std::endl;
            //std::cout << "second " << secondBankPath << std::endl;
            //std::cout << "newfirst " << newfirstBankPath << std::endl;
            //std::cout << "newsecond " << newsecondBankPath << std::endl;
            deleteDir(tempBankPath); // just to be sure
            if (!renameDir(firstBankPath, tempBankPath))
            {
                synth->getRuntime().Log("failed move to temp dir", 2);
                return(" FAILED Can't move from root " + to_string(firstRoot) + " to temp dir");
            }

            if (!renameDir(secondBankPath,newsecondBankPath))
            {
                synth->getRuntime().Log("failed move to " + to_string(firstRoot), 2);
                return(" FAILED Can't move from root " + to_string(secondRoot) + " to " + to_string(firstRoot));
            }


            if (!renameDir(tempBankPath, newfirstBankPath))
            {
                synth->getRuntime().Log("failed move to " + to_string(secondRoot), 2);
                return (" FAILED Can't move from temp dir to " + to_string(secondRoot));
            }
        }
    }

    // update banks
    if (secondname.empty())
    {
        moveType = 1;
        roots [secondRoot].banks [secondID] = roots [firstRoot].banks [firstID];
        roots [firstRoot].banks.erase(firstID);
    }
    else if (firstname.empty())
    {
        moveType = 2;
        roots [firstRoot].banks [firstID] = roots [secondRoot].banks [secondID];
        roots [secondRoot].banks.erase(secondID);
    }
    else
    {
        roots [firstRoot].banks [firstID].dirname = secondname;
        roots [secondRoot].banks [secondID].dirname = firstname;
        hints [secondRoot] [secondname] = firstID; // why do we need these?
        hints [firstRoot] [firstname] = secondID;

        for(int pos = 0; pos < BANK_SIZE; ++ pos)
        {
            InstrumentEntry &instrRef_1 = getInstrumentReference(firstRoot, firstID, pos);
            InstrumentEntry &instrRef_2 = getInstrumentReference(secondRoot, secondID, pos);

            InstrumentEntry tmp = instrRef_2;

            if (instrRef_1.name == "")
                roots [secondRoot].banks [secondID].instruments.erase(pos);
            else
                instrRef_2 = instrRef_1;

            if (tmp.name == "")
                roots [firstRoot].banks [firstID].instruments.erase(pos);
            else
                instrRef_1 = tmp;
        }
    }

    if (firstRoot == synth->getRuntime().currentRoot)
        synth->getRuntime().currentRoot = secondRoot;
    else if(secondRoot == synth->getRuntime().currentBank)
        synth->getRuntime().currentBank = firstRoot;
    if (firstID == synth->getRuntime().currentBank)
        synth->getRuntime().currentBank = secondID;
    else if(secondID == synth->getRuntime().currentBank)
        synth->getRuntime().currentBank = firstID;

    if (moveType == 0)
        return ("ped " + firstname + " with " + secondname);

    int destination;
    string type = "slot ";
    if (firstRoot == secondRoot)
    {
        if (moveType == 1)
            destination = secondID;
        else
            destination = firstID;
    }
    else
    {
        type = "root ";
        if (moveType == 1)
            destination = secondRoot;
        else
            destination = firstRoot;
    }

    if (moveType == 2)
        return (" Moved " + secondname + " to " + type + to_string(destination));

    return (" Moved " + firstname + " to " + type + to_string(destination));
}


// Re-scan for directories containing instrument banks
void Bank::rescanforbanks(void)
{
    RootEntryMap::const_iterator it;
    InstrumentsInBanks = 0;
    BanksInRoots = 0;
    for (it = roots.begin(); it != roots.end(); ++it)
    {
        scanrootdir(it->first);
    }
    //cout << "ins " << InstrumentsInBanks << "  Ban " << BanksInRoots << endl;
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
                if ((xizpos = possible.rfind(EXTEN::zynInst)) != string::npos)
                {
                    if (EXTEN::zynInst.size() == (possible.size() - xizpos))
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
        BanksInRoots += 1;
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
            while (!emptyslot(rootID, bankID, pos))
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
        string checkfile = setExtension(getFullPath(rootID, bankID, pos), EXTEN::yoshInst);
        if (!isRegularFile(checkfile))
            checkfile = setExtension(getFullPath(rootID, bankID, pos), EXTEN::zynInst);
        unsigned int names = 0;
        int type = 0;
        XMLwrapper *xml = new XMLwrapper(synth, true, false);
        xml->checkfileinformation(checkfile, names, type);
        delete xml;

        instrRef.type = type;
        instrRef.ADDsynth_used = (names & 1) > 0;
        instrRef.SUBsynth_used = (names & 2) > 0;
        instrRef.PADsynth_used = (names & 4) > 0;
        instrRef.yoshiType = (names & 8) > 0;//xml->information.yoshiType;
    }
    InstrumentsInBanks += 1;
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
        extendLocalPath("/banks"),
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


const BankEntry &Bank::getBank(size_t bankID, size_t rootID)
{
    if (rootID == UNUSED)
        rootID = synth->getRuntime().currentRoot;
    return roots[rootID].banks[bankID];
}


int Bank::engines_used(size_t rootID, size_t bankID, unsigned int ninstrument)
{
    int tmp = getInstrumentReference(rootID, bankID, ninstrument).ADDsynth_used
            | (getInstrumentReference(rootID, bankID, ninstrument).SUBsynth_used << 1)
            | (getInstrumentReference(rootID, bankID, ninstrument).PADsynth_used << 2)
            | (getInstrumentReference(rootID, bankID, ninstrument).yoshiType << 3);
    return tmp;
}


void Bank::clearBankrootDirlist(void)
{
    roots.clear();
}


void Bank::removeRoot(size_t rootID)
{
    if(rootID == synth->getRuntime().currentRoot)
    {
        synth->getRuntime().currentRoot = 0;
    }
    roots.erase(rootID);
    setCurrentRootID(synth->getRuntime().currentRoot);
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
            synth->getRuntime().currentRoot = roots.begin()->first;
        }
    }
    else
    {
        synth->getRuntime().currentRoot = newRootID;
    }
    setCurrentBankID(0);
    return true;
}


bool Bank::setCurrentBankID(size_t newBankID, bool ignoreMissing)
{
    if(roots [synth->getRuntime().currentRoot].banks.count(newBankID) == 0)
    {
        if((roots [synth->getRuntime().currentRoot].banks.size() == 0) || ignoreMissing)
        {
            return false;
        }
        else
        {
            newBankID = roots [synth->getRuntime().currentRoot].banks.begin()->first;
        }
    }
    synth->getRuntime().currentBank = newBankID;
    return true;
}


size_t Bank::getCurrentBankID()
{// This is only used by the root section of BankUI
    return synth->getRuntime().currentBank;

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
