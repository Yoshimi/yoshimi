/*
    Bank.cpp - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2014-2023, Will Godfrey & others

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

    This file is a derivative of a ZynAddSubFX original.

*/

#include <set>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

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

const int BANKS_VERSION = 11;

using file::make_legit_filename;
using file::isRegularFile;
using file::isDirectory;
using file::renameDir;
using file::createDir;
using file::deleteDir;
using file::listDir;
using file::copyDir;
using file::copyFile;
using file::renameFile;
using file::deleteFile;
using file::countDir;
using file::findLeafName;
using file::setExtension;
using file::extendLocalPath;
using file::loadText;
using file::saveText;

using func::asString;
using func::string2int;
using func::findSplitPoint;
using func::isDigits;

using std::list;
using std::optional;
using std::to_string;
using std::string;


Bank::Bank(SynthEngine *_synth) :
    defaultinsname(string(" ")),
    synth(_synth)
{
    BanksVersion = 10;
    InstrumentsInBanks = 0,
    BanksInRoots = 0;
    foundLocal = file::localDir() + "/found/";
}

string Bank::getBankFileTitle(size_t root, size_t bank)
{
    string name = synth->makeUniqueName("Root " + asString(root) + ", Bank " + asString(bank) + " - " + getBankPath(root, bank));
    return name;
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
int Bank::setInstrumentName(const string& name, int slot, size_t bank, size_t root)
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
bool Bank::moveInstrument(unsigned int ninstrument, const string& newname, int newslot, size_t oldBank, size_t newBank, size_t oldRoot, size_t newRoot)
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
    if (roots.count(rootID) == 0 || roots [rootID].banks.count(bankID) == 0)
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

    saveText(string(YOSHIMI_VERSION), filepath + EXTEN::validBank);
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


bool Bank::isDuplicateBankName(size_t rootID, const string& name)
{
    for (int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
    {
        string check = getBankName(i,rootID);
        if (check > "" && check == name)
            return true;
    }
    return false;
}


// finds the number of instruments in a bank
int Bank::getBankSize(int bankID, size_t rootID)
{
    int found = 0;

    for (int i = 0; i < MAX_INSTRUMENTS_IN_BANK; ++ i)
        if (!roots [rootID].banks [bankID].instruments [i].name.empty())
            found += 1;
    return found;
}


int Bank::changeBankName(size_t rootID, size_t bankID, const string& newName)
{
    string filename = newName;
    string oldName = getBankName(bankID, rootID);
    make_legit_filename(filename);
    string newfilepath = getRootPath(synth->getRuntime().currentRoot) + "/" + filename;
    string reply = "";
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


void Bank::checkbank(size_t rootID, size_t banknum)
{
    string bankdirname = getBankPath(rootID, banknum);

    if (bankdirname.empty())
    {
        return;
    }

    string chkpath;
    list<string> thisBank;
    uint32_t found = listDir(&thisBank, bankdirname);
    if (found == 0xffffffff)
    {
        synth->getRuntime().Log("Failed to open bank directory " + bankdirname);
        thisBank.clear();
        return;
    }
    if (found == 0)
    {
        roots [rootID].banks.erase(banknum);
        thisBank.clear();
        return;
    }

    string path = getBankPath(rootID, banknum) + "/";
    // clear missing/removed entries
    for (int pos = 0; pos < MAX_INSTRUMENTS_IN_BANK; ++pos)
    {
        if (!emptyslot(rootID, banknum, pos))
        {
            string chkpath = path + getInstrumentReference(rootID, banknum, pos).filename;
            if (!isRegularFile(chkpath))
            {
                getInstrumentReference(rootID, banknum, pos).clear();
            }
        }
    }

    for (list<string>::iterator it = thisBank.begin(); it != thisBank.end(); ++it)
    {
        string candidate = *it;

        if (candidate.size() <= EXTEN::zynInst.size()) // at least a single char filename
            *it = "";
        else
        {
            chkpath = bankdirname + "/" + candidate;
            if (!isRegularFile(chkpath))
                *it = "";
            else
            {
                string exten = file::findExtension(chkpath);
                if (exten != EXTEN::yoshInst && exten != EXTEN::zynInst)
                    *it = "";
                else
                {
                    string instname = candidate.substr(0, candidate.size() -  exten.size());
                    int instnum = -1;
                    splitNumFromName(instnum, instname);
                    if (instnum >= 0 && !emptyslot(rootID, banknum, instnum)) // a recognised location
                    {
                        int othernum = -1;
                        string othername = roots [rootID].banks [banknum].instruments [instnum].filename;
                        othername = othername.substr(0, othername.size() - exten.size());
                        splitNumFromName(othernum, othername);
                        if (instname == othername)
                        {
                            if (exten == EXTEN::yoshInst) // yoshiType takes priority
                                getInstrumentReference(rootID, banknum, instnum).yoshiType = true;
                            *it = "";
                        }
                    }
                }
            }
        }
    }

    string candidate;
    for (list<string>::iterator it = thisBank.begin(); it != thisBank.end(); ++it)
    {
        candidate = *it;

        if (!candidate.empty())
        {
            // remove extension
            string exten = file::findExtension(chkpath);
            string instname = candidate.substr(0, candidate.size() -  exten.size());
            int instnum = -1;
            splitNumFromName(instnum, instname);
            addtobank(rootID, banknum, instnum, candidate, instname);
        }
    }

    thisBank.clear();
}


// Makes current a bank directory
bool Bank::loadbank(size_t rootID, size_t banknum)
{
    string bankdirname = getBankPath(rootID, banknum);

    if (bankdirname.empty())
        return false;

    roots [rootID].banks [banknum].instruments.clear();

    string chkpath;
    string candidate;
    list<string> thisBank;
    uint32_t found = listDir(&thisBank, bankdirname);
    if (found == 0xffffffff)
    {
        synth->getRuntime().Log("Failed to open bank directory " + bankdirname);
        return false;
    }

    if (bankdirname.at(bankdirname.size() - 1) != '/')
        bankdirname += '/';
    for (list<string>::iterator it = thisBank.begin(); it != thisBank.end(); ++ it)
    {
        candidate = *it;
        if (candidate.size() <= (EXTEN::zynInst.size())) // at least a 1 char filename!
            continue;

        chkpath = bankdirname + candidate;
        if (!isRegularFile(chkpath))
            continue;

        string exten = file::findExtension(chkpath);
        if (exten != EXTEN::yoshInst && exten != EXTEN::zynInst)
            continue;

        if (exten == EXTEN::zynInst && isRegularFile(setExtension(chkpath, EXTEN::yoshInst)))
            continue; // don't want .xiz if there is .xiy

        // remove extension
        string instname = candidate.substr(0, candidate.size() -  exten.size());
        int instnum = -1;
        splitNumFromName(instnum, instname);
        addtobank(rootID, banknum, instnum, candidate, instname);
        InstrumentsInBanks += 1;
    }
    thisBank.clear();
    return true;
}


// Creates an external bank and copies in the contents of the IDd one
string Bank::exportBank(const string& exportdir, size_t rootID, unsigned int bankID)
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
        int result = createDir(exportdir);
        if (result != 0)
        {
            name = "Can't create external bank " + findLeafName(exportdir);
            ok = false;
        }
        else
        {
            uint32_t result = copyDir(sourcedir, exportdir, 0);

            if (result != 0)
            {
                name = "Copied out " + to_string(result & 0xffff) + " files to " + exportdir + ". ";
                result = result >> 16;
                if (result > 0)
                    name += ("Failed to transfer" + to_string(result));
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
        list<string> thisBank;
        uint32_t found = listDir(&thisBank, importdir);
        if (found == 0xffffffff)
        {
            synth->getRuntime().Log("Can't find " + importdir);
            ok = false;
        }
        else
        {
            if (importdir.back() == '/')
                importdir = importdir.substr(0, importdir.length() - 1);
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
                int total = 0;
                bool missing = false;
                string externfile = getRootPath(rootID) + "/" + getBankName(bankID, rootID);
                for (list<string>::iterator it = thisBank.begin(); it != thisBank.end(); ++ it)
                {
                    string nextfile = *it;
                    if (nextfile.rfind(EXTEN::validBank) != string::npos)
                        continue; // new version will be generated

                    string exten = file::findExtension(nextfile);
                    if (exten == EXTEN::yoshInst || exten == EXTEN::zynInst)
                    {
                        if (copyFile(importdir + "/" + nextfile, externfile + "/" + nextfile, 0))
                            missing = true;
                        // remove extension
                        string instname = nextfile.substr(0, nextfile.size() -  exten.size());
                        int instnum = -2; // no number prefix
                        splitNumFromName(instnum, instname);
                        if (instnum == -1) //  we don't accept a displayed prefix of zero
                            instnum = MAX_INSTRUMENTS_IN_BANK -1;
                        if (!isDuplicate(rootID, bankID, instnum, nextfile))
                        {
                            if (addtobank(rootID, bankID, instnum, nextfile, instname))
                                missing = true;
                        }
                        ++count;
                    }
                    ++total;
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
        thisBank.clear();
    }

    if (!ok)
        name = " FAILED " + name;
    else if (!partial)
        name = "ed " + name;
    return name;
}


bool Bank::isDuplicate(size_t rootID, size_t bankID, int pos, const string filename)
{
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
bool Bank::newIDbank(const string& newbankdir, unsigned int bankID, size_t rootID)
{
    if (rootID == UNUSED)
        rootID = synth->getRuntime().currentRoot; // shouldn't be needed!

    if (!newbankfile(newbankdir, rootID))
        return false;
    roots [synth->getRuntime().currentRoot].banks [bankID].dirname = newbankdir;
    return true;
}


// Performs the actual file operation for new banks
bool Bank::newbankfile(const string& newbankdir, size_t rootID)
{
     if (getRootPath(synth->getRuntime().currentRoot).empty())
        return false;

    string newbankpath = getRootPath(rootID);
    if (newbankpath.at(newbankpath.size() - 1) != '/')
        newbankpath += "/";
    newbankpath += newbankdir;
    int result = createDir(newbankpath);
    if (result != 0)
        return false;

    string forcefile = newbankpath;
    if (forcefile.at(forcefile.size() - 1) != '/')
        forcefile += "/";
    saveText(string(YOSHIMI_VERSION), forcefile + EXTEN::validBank);
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
    string IDfile = bankName + "/" + EXTEN::validBank;
    if (!saveText(string(YOSHIMI_VERSION), IDfile))
        return (" FAILED Can't delete from this location.");

    bool ck1 = true;
    bool ck2 = true;
    int chk = 0;
    string name;
    string failed;
    for (int inst = 0; inst < MAX_INSTRUMENTS_IN_BANK; ++ inst)
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
    {
        chk = 1;
        deleteDir(bankName);
    }

    roots [rootID].banks.erase(bankID);
    if (rootID == synth->getRuntime().currentRoot && bankID == synth->getRuntime().currentBank)
        setCurrentBankID(0, false);
    int remainder = countDir(bankName);
    if (remainder > 0)
        return(" FAILED Bank " + bankName + " unlinked but " + to_string(remainder) + " unrecognised objects still exist.");
    return ("d " + bankName);
}


// Swaps a slot with another
string Bank::swapslot(unsigned int n1, unsigned int n2, size_t bank1, size_t bank2, size_t root1, size_t root2)
{
    if (n1 == n2 && bank1 == bank2 && root1 == root2)
        return " Can't swap with itself!";

    /*
     * path entries will always have either .xiy or .xiz
     * otherwise they would not have been seen at all
     * however we test for, and move both if they exist
     */
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
                message = to_string(n2) + " " + getname(n2, bank2, root2);
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
                message = to_string(n2) + " " + getname(n1, bank1, root1);
            getInstrumentReference(root2, bank2, n2) = getInstrumentReference(root1, bank1, n1);
            getInstrumentReference(root1, bank1, n1).clear();
        }
        if (!ok)
            return (" FAILED" + message);
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
        return (" FAILED" + message);

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
                synth->getRuntime().Log("move to " + to_string(secondRoot) + ": " + string(strerror(errno)), _SYS_::LogNotSerious);
                return (" FAILED Can't move from root " + to_string(firstRoot) + " to " + to_string(secondRoot));
            }
        }
        else if (firstBankPath == "") // move only
        {
            if (!renameDir(secondBankPath, (getRootPath(firstRoot) + "/" + secondname)))
            {
                synth->getRuntime().Log("move to " + to_string(firstRoot) + ": " + string(strerror(errno)), _SYS_::LogNotSerious);
                return (" FAILED Can't move from root " + to_string(secondRoot) + " to " + to_string(firstRoot));
            }
        }
        else // actual swap
        {
            // due to possible identical names we need to go via a temp file
            deleteDir(tempBankPath); // just to be sure
            if (!renameDir(firstBankPath, tempBankPath))
            {
                synth->getRuntime().Log("failed move to temp dir", _SYS_::LogNotSerious);
                return(" FAILED Can't move from root " + to_string(firstRoot) + " to temp dir");
            }

            if (!renameDir(secondBankPath,newsecondBankPath))
            {
                synth->getRuntime().Log("failed move to " + to_string(firstRoot), _SYS_::LogNotSerious);
                return(" FAILED Can't move from root " + to_string(secondRoot) + " to " + to_string(firstRoot));
            }


            if (!renameDir(tempBankPath, newfirstBankPath))
            {
                synth->getRuntime().Log("failed move to " + to_string(secondRoot), _SYS_::LogNotSerious);
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

        for (int pos = 0; pos < MAX_INSTRUMENTS_IN_BANK; ++ pos)
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
    else if (secondRoot == synth->getRuntime().currentBank)
        synth->getRuntime().currentBank = firstRoot;
    if (firstID == synth->getRuntime().currentBank)
        synth->getRuntime().currentBank = secondID;
    else if (secondID == synth->getRuntime().currentBank)
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

// private affairs

bool Bank::isValidBank(string chkdir)
{
    if (!isDirectory(chkdir))
        return false;
    // check if directory contains an instrument or EXTEN::validBank
    list<string> tryBank;
    uint32_t tried = listDir(&tryBank, chkdir);
    if (tried == 0xffffffff)
    {
        synth->getRuntime().Log("Failed to open bank directory candidate " + chkdir);
        return false;
    }
    chkdir += "/";
    for (list<string>::iterator it_b = tryBank.begin(); it_b != tryBank.end(); ++ it_b)
    {
        string chkpath = chkdir + *it_b;
        if (isRegularFile(chkpath))
        {
            string tryext = file::findExtension(chkpath);
            if (tryext == EXTEN::validBank || tryext == EXTEN::yoshInst || tryext == EXTEN::zynInst)
                return true;
        }
    }
    return false;
}


bool Bank::addtobank(size_t rootID, size_t bankID, int _pos, string _filename, const string name)
{
    if (_pos < 0 || _pos >= MAX_INSTRUMENTS_IN_BANK)
        return -1; //invalid location

    int pos = _pos;
    string filename = _filename;

    BankEntry &bank = roots [rootID].banks [bankID];
    string exten = file::findExtension(filename);

    if (bank.instruments [pos].used)
    {
        string oldName = getname(pos, bankID, rootID);
        /*
            * We test the internal name. The file name could have been
            * changed, but if the internal one is changed it is most
            * likely to be a modified instrument.
            */
        if (name == oldName) // duplicate
        {
            if (exten == EXTEN::yoshInst) // yoshiType takes priority
                getInstrumentReference(rootID, bankID, pos).yoshiType = true;
            return 0; // no actual insertion necessary
        }
        pos = -1; // location occupied so find a new free position
    }

    bool wanted = (pos >=0);
    if (pos < 0)
    {
        pos = MAX_INSTRUMENTS_IN_BANK;
        while (pos > 0 && !wanted)
        {
            --pos;
            if (emptyslot(rootID, bankID, pos))
                wanted = true;
            else if (name == getname(pos, bankID, rootID))
            {
                pos = -1;
            }
        }
    }
    if (!wanted)
        return -1; // duplicated or the bank is full
    string bankdirname = getBankPath(rootID, bankID) + "/";
    string prefix = "0000" + to_string(pos + 1);
    prefix = prefix.substr(prefix.size() - 4);
    string newfile = prefix + "-" + name + exten;
    /*
     * If we are repositioning this file because it has the same
     * ID as an existing one but is in a non-writable location
     * we store its original filename while showing an offset ID.
     * If the location is writable we move the file.
     */
    if (renameFile(bankdirname + filename, bankdirname + newfile))
        filename = newfile;

    deletefrombank(rootID, bankID, pos); // is this actually needed?
    InstrumentEntry &instrRef = getInstrumentReference(rootID, bankID, pos);
    instrRef.used = true;
    instrRef.name = name;
    instrRef.filename = filename;
    instrRef.PADsynth_used = 0;
    instrRef.ADDsynth_used = 0;
    instrRef.SUBsynth_used = 0;
    instrRef.yoshiType = 0;

    string checkfile = setExtension(getFullPath(rootID, bankID, pos), EXTEN::yoshInst);
    if (!isRegularFile(checkfile))
        checkfile = setExtension(getFullPath(rootID, bankID, pos), EXTEN::zynInst);
    unsigned int names = 0;
    int type = 0;
    auto xml{std::make_unique<XMLwrapper>(*synth, true, false)};
    xml->checkfileinformation(checkfile, names, type);

    instrRef.type = type;
    instrRef.ADDsynth_used = (names & 1);
    instrRef.SUBsynth_used = (names & 2) >> 1;
    instrRef.PADsynth_used = (names & 4) >> 2;
    instrRef.yoshiType = (exten == EXTEN::yoshInst);
    return 0;
}


void Bank::deletefrombank(size_t rootID, size_t bankID, unsigned int pos)
{
    if (pos >= MAX_INSTRUMENTS_IN_BANK)
    {
        synth->getRuntime().Log("Error, deletefrombank pos " + asString(pos) + " > MAX_INSTRUMENTS_IN_BANK"
                    + asString(MAX_INSTRUMENTS_IN_BANK));
        return;
    }
    getInstrumentReference(rootID, bankID, pos).clear();
}


InstrumentEntry &Bank::getInstrumentReference(size_t rootID, size_t bankID, size_t ninstrument)
{
    return roots [rootID].banks [bankID].instruments [ninstrument];
}


void Bank::updateShare(string bankdirs[], string baseDir, string shareID)
{
    saveText(to_string(synth->getRuntime().build_ID), shareID);
    string next = "/Will_Godfrey_Companion";
    string destinationDir = baseDir + "yoshimi/banks/Will_Godfrey_Companion"; // currently only concerned with this one.
    if (!isDirectory(destinationDir))
        return;
    if (isDirectory(bankdirs[1] + next))
        checkShare(bankdirs[1] + next, destinationDir);

    if (isDirectory(bankdirs[2] + next))
     checkShare(bankdirs[2] + next, destinationDir);
}


void Bank::checkShare(string sourceDir, string destinationDir)
{
    copyDir(sourceDir, destinationDir, 0);
}


bool Bank::transferDefaultDirs(string bankdirs[])
{
    if (!isDirectory(foundLocal))
        return false;
    bool found = false;
    // always want these
    if (isDirectory(foundLocal + "yoshimi"))
        found = true;
    else
    {
        createDir(foundLocal + "yoshimi");
        createDir(foundLocal + "yoshimi/banks");
        if (isDirectory(bankdirs[6]))
            if (transferOneDir(bankdirs, 0, 6))
                found = true;
        if (isDirectory(bankdirs[1]) || isDirectory(bankdirs[2]))
        {
            if (transferOneDir(bankdirs, 0, 1))
                found = true;
            if (transferOneDir(bankdirs, 0, 2))
                found = true;
        }
    }

    //might not have these
    if (isDirectory(foundLocal + "zynaddsubfx"))
        found = true;
    else
    {
        if (isDirectory(bankdirs[3]) || isDirectory(bankdirs[4]))
        {
            createDir(foundLocal + "zynaddsubfx");
            createDir(foundLocal + "zynaddsubfx/banks");
            if (transferOneDir(bankdirs, 5, 3))
                found = true;
            if (transferOneDir(bankdirs, 5, 4))
                found = true;
        }
    }
    return found;
}


bool Bank::transferOneDir(string bankdirs[], int baseNumber, int listNumber)
{
    bool found = false;
    list<string> thisBankDir;
    uint32_t copyList = listDir(& thisBankDir, bankdirs[listNumber]);
    if (copyList > 0 && copyList < 0xffffffff)
    {
        for (list<string>::iterator it = thisBankDir.begin(); it != thisBankDir.end(); ++ it)
        {
            string oldBank = bankdirs[listNumber] + "/" + *it;
            string newBank = bankdirs[baseNumber] + "/" + *it;
            createDir(newBank);
            uint32_t inside = copyDir(oldBank, newBank, 1);
            if (inside > 0 && inside < 0xffffffff)
                found = true;
        }
        thisBankDir.clear();
    }
return found;
}


void Bank::checkLocalBanks()
{
    if (isDirectory(foundLocal + "yoshimi/banks")) // yoshi
        addRootDir(foundLocal + "yoshimi/banks");

    if (isDirectory(foundLocal + "zynaddsubfx/banks"))
        addRootDir(foundLocal + "zynaddsubfx/banks"); // zyn
}

void Bank::addDefaultRootDirs(string bankdirs[])
{
    int tot = 0;
    int i = 0;
    while (bankdirs[i] != "@end")
    {
        if (isDirectory(bankdirs[i]))
        {
            addRootDir(bankdirs [i]);
            ++tot;
        }
        ++ i;
    }

    for (int i = tot; i > 0; --i)
        changeRootID(i, i * 5);
}


size_t Bank::generateSingleRoot(const string& newRoot, bool clear)
{
    createDir(newRoot);

    // add bank
    string newBank = newRoot + "newBank";
    createDir(newBank);
    string toSave = newBank + "/" + EXTEN::validBank;
    saveText(string(YOSHIMI_VERSION), toSave);
    // now generate and save an instrument
    int npart = 0;
    string instrumentName = "First Instrument";
    synth->interchange.generateSpecialInstrument(npart, instrumentName);

    string filename = newBank + "/" + "0005-" + instrumentName + EXTEN::zynInst;
    synth->part[npart]->saveXML(filename, false);

    // set root and tidy up
    size_t idx = addRootDir(newRoot);

    if (clear)
        synth->part[npart]->defaultsinstrument();

    synth->maybePublishEffectsToGui();
    return idx;
}


size_t Bank::getNewRootIndex()
{
    size_t pos = 1;
    if (roots.empty())
        return pos;
    while (roots.count(pos) != 0)
        ++ pos;
    return pos;
}


size_t Bank::getNewBankIndex(size_t rootID)
{
    if (roots [rootID].banks.empty())
    {
        if (roots [rootID].bankIdStep <= 1)
        {
            return 0;
        }

        return roots [rootID].bankIdStep;
    }

    size_t idStep = 1;

    if (roots [rootID].bankIdStep == 0)
    {
        size_t startId = 127;
        size_t i;
        for (i = startId; i > 0; --i)
        {
            if (roots [rootID].banks.count(i) == 0)
            {
                break;
            }
        }
        if (i > 0) //id found
        {
            return i;
        }
    }
    else
    {
        idStep = roots [rootID].bankIdStep;
    }
    return idStep;
}


string Bank::getBankPath(size_t rootID, size_t bankID)
{
    if (roots.count(rootID) == 0 || roots [rootID].banks.count(bankID) == 0)
    {
        return string("");
    }
    if (roots [rootID].path.empty())
    {
        return string("");
    }
    string chkdir = getRootPath(rootID) + string("/") + roots [rootID].banks [bankID].dirname;
    if (chkdir.at(chkdir.size() - 1) == '/')
    {
        chkdir = chkdir.substr(0, chkdir.size() - 1);
    }
    return chkdir;
}


string Bank::getRootPath(size_t rootID)
{
    if (roots.count(rootID) == 0 || roots [rootID].path.empty())
        return string("");

    string chkdir = roots [rootID].path;
    if (chkdir.at(chkdir.size() - 1) == '/')
        chkdir = chkdir.substr(0, chkdir.size() - 1);

    return chkdir;
}


string Bank::getFullPath(size_t rootID, size_t bankID, size_t ninstrument)
{
    string bankPath = getBankPath(rootID, bankID);
    if (!bankPath.empty())
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


bool Bank::removeRoot(size_t rootID)
{
    if (rootID == synth->getRuntime().currentRoot)
    {
        synth->getRuntime().currentRoot = 0;
    }
    else if (roots [rootID].path.empty())
        return true;
    roots.erase(rootID);
    synth->getRuntime().currentRoot = roots.begin()->first;
    setCurrentRootID(synth->getRuntime().currentRoot);
    return false;
}


bool Bank::changeRootID(size_t oldID, size_t newID)
{
    RootEntry oldRoot = roots [oldID];
    roots [oldID] = roots [newID];
    roots [newID] = oldRoot;
    setCurrentRootID(newID);
    RootEntryMap::iterator it = roots.begin();
    while (it != roots.end())
    {
        if (it->second.path.empty())
            roots.erase(it++);
        else
            ++it;
    }

    return true;
}


bool Bank::setCurrentRootID(size_t newRootID)
{
    size_t oldRoot = synth->getRuntime().currentRoot;
    if (roots.count(newRootID) == 0)
        return false;
    else
        synth->getRuntime().currentRoot = newRootID;
    for (size_t id = 0; id < MAX_BANKS_IN_ROOT; ++id)
    {
        if (roots [newRootID].banks.count(id) == 0)
        {
            findFirstBank(newRootID);
            return true;
        }
        if (roots [newRootID].banks.count(id) == 1)
        {
            if (roots [newRootID].banks [id].dirname.empty())
            {
                findFirstBank(newRootID);
                return true;
            }
        }
    }
    if (synth->getRuntime().currentRoot != oldRoot)
        findFirstBank(newRootID);
    return true;
}

unsigned int Bank::findFirstBank(size_t newRootID)
{
    for (size_t i = 0; i < MAX_BANKS_IN_ROOT; ++i)
    {
        if (roots [newRootID].banks.count(i) != 0)
        {
            if (!roots [newRootID].banks [i].dirname.empty())
            {
                synth->getRuntime().currentBank = i;
                break;
            }
        }
    }
    return 0;
}


bool Bank::setCurrentBankID(size_t newBankID, bool ignoreMissing)
{
    if (roots [synth->getRuntime().currentRoot].banks.count(newBankID) == 0)
    {
        if (ignoreMissing)
            return false;
        else
            newBankID = roots [synth->getRuntime().currentRoot].banks.begin()->first;
    }
    synth->getRuntime().currentBank = newBankID;
    return true;
}


size_t Bank::addRootDir(const string& newRootDir)
{
   // we need the size check to prevent weird behaviour if the name is just ./
    if (!isDirectory(newRootDir) || newRootDir.length() < 4)
        return 0;
    size_t newIndex = getNewRootIndex();
    roots [newIndex].path = newRootDir;
    return newIndex;
}


bool Bank::establishBanks(optional<string> bankFile)
{
    /*
     * This list is used in transferDefaultDirs( to find and copy
     * bank lists into $HOME/.local.yoshimi
     * This is refreshed at each startup to update existing entries
     * or add new ones.
     *
     * It is also used by addDefaultRootDirs( to populate the bank
     * roots, in the event of a missing list.
     *
     * The list is in the order the roots will appear to the user,
     * and the numbering in addDefaultRootDirs is the same.
     */

    string bankdirs[] = {
        foundLocal + "yoshimi/banks",
        "/usr/share/yoshimi/banks",
        "/usr/local/share/yoshimi/banks",
        "/usr/share/zynaddsubfx/banks",
        "/usr/local/share/zynaddsubfx/banks",
        foundLocal + "zynaddsubfx/banks",
        extendLocalPath("/banks"),
        "@end"
    };

    bool rootsFound = transferDefaultDirs(bankdirs);

    bool newRoots = true;
    roots.clear();

    if (bankFile)
    {
        auto xml{std::make_unique<XMLwrapper>(*synth)};
        if (xml->loadXMLfile(*bankFile))
        {
            if (xml->enterbranch("INFORMATION"))
            { // going negative to catch all previous versions and to be backward compatible
                writeVersion(xml->getpar("Banks_Version", 0, 0, 999));
                xml->exitbranch();
            }
            if (xml->enterbranch("BANKLIST"))
            {
                loadFromConfigFile(*xml);
                newRoots = false;
            }
        }
    }

    if (newRoots)
    {
        roots.clear();
        if (rootsFound)
            addDefaultRootDirs(bankdirs);
        else
        {
            string newRoot = foundLocal + "yoshimi/banks";
            size_t idx = generateSingleRoot(newRoot);
            changeRootID(idx, 5);
            synth->getRuntime().currentRoot = idx;
            synth->getRuntime().currentBank = 5;

        }
        synth->getRuntime().currentRoot = 5;
        synth->getRuntime().banksChecked = true;
    }

    if (!synth->getRuntime().rootDefine.empty())
    {
        string found = synth->getRuntime().rootDefine;
        synth->getRuntime().rootDefine = "";
    }
    installRoots();

    if (isDirectory(foundLocal))
    {
        string shareID = foundLocal + "version";
        if (loadText(shareID) != to_string(synth->getRuntime().build_ID))
            updateShare(bankdirs, foundLocal, shareID);
    }
    return newRoots;
}


bool Bank::installRoots()
{
    RootEntryMap::const_iterator it;
    for (it = roots.begin(); it != roots.end(); ++it)
    {
        size_t rootID = it->first;
        string rootdir = roots [rootID].path;

        // the directory has been removed since the bank root was created
        if (!rootdir.size() || !isDirectory(rootdir))
            continue;
        installNewRoot(rootID, rootdir, true);
    }
    return true;
}


bool Bank::installNewRoot(size_t rootID, string rootdir, bool reload)
{
    list<string> thisRoot;
    uint32_t found = listDir(&thisRoot, rootdir);
    if (found == 0xffffffff)
    { // should never see this!
        synth->getRuntime().Log("No such directory, root bank entry " + rootdir);
        return false;
    }

    if (rootdir.at(rootdir.size() - 1) != '/')
        rootdir += '/';

    // it's a completely new root
    if (!reload)
        roots [rootID].banks.clear();

    map<string, string> bankDirsMap;

    // thin out invalid directories
    int validBanks = 0;
    list<string>::iterator r_it = thisRoot.end();
    while (r_it != thisRoot.begin())
    {
        string candidate = *--r_it;
        string chkdir = rootdir + candidate;
        if (isValidBank(chkdir))
            ++validBanks;
        else
            r_it = thisRoot.erase(r_it);
    }
    bool result = true;
    if (validBanks >= MAX_BANKS_IN_ROOT)
        synth->getRuntime().Log("Warning: There are " + to_string(validBanks - MAX_BANKS_IN_ROOT) + " too many valid bank candidates");

    bool banksSet[MAX_BANKS_IN_ROOT];
    int banksFound = 0;

    for (int i = 0; i < MAX_BANKS_IN_ROOT; ++i)
        banksSet[i] = false;

    // install previously seen banks to the same references
    if (reload)
    {
        list<string>::iterator b_it = thisRoot.end();
        while (b_it != thisRoot.begin())
        {
            string trybank = *--b_it;
            for (size_t id = 0; id < MAX_BANKS_IN_ROOT; ++id)
            {
                if (roots [rootID].banks.count(id) == 0)
                    continue;
                if (roots[rootID].banks[id].dirname == trybank)
                {
                    banksSet[id] = true;

                    if (BanksVersion == BANKS_VERSION) // all we need to do!
                    {
                        checkbank(rootID, id);
                        InstrumentsInBanks += getBankSize(id, rootID);
                    }
                    else
                    {
                        roots [rootID].banks [id].dirname = trybank;
                        loadbank(rootID, id);
                    }
                    b_it = thisRoot.erase(b_it);
                    ++banksFound;
                    break;
                }
            }
            if (banksFound >= MAX_BANKS_IN_ROOT)
            {
                result = false;
                break;
            }
        }
    }
    BanksInRoots += banksFound;
    size_t toFetch = thisRoot.size();
    if (toFetch > 0)
    {
        synth->getRuntime().Log("Found " + to_string(toFetch) + " new banks in root " + roots [rootID].path);
    }

    if (thisRoot.size() != 0)
    {
        /*
         * install completely new banks
         *
         * This sequence spreads new banks as evenly as possible
         * through the root, avoiding collisions with possible
         * existing banks, and at the same time ensuring that
         * ID zero is the last possible entry.
         */
        size_t idStep = 5;
        size_t newIndex = idStep;

        // try to keep new banks in a sensible order
        thisRoot.sort();

        for (list<string>::iterator it = thisRoot.begin(); it != thisRoot.end(); ++it)
        {
            if (banksFound >= MAX_BANKS_IN_ROOT)
            {
                result = false;
                break; // root is full!
            }
            while (banksSet[newIndex] == true)
            {
                newIndex += idStep;
                newIndex &= (MAX_BANKS_IN_ROOT - 1);
            }
            roots [rootID].banks [newIndex].dirname = *it;
            loadbank(rootID, newIndex);
            banksSet[newIndex] = true;
            ++ banksFound;
            BanksInRoots += 1; // this is the total of all banks
        }
    }

    // remove orphans
    for (size_t id = 0; id < MAX_BANKS_IN_ROOT; ++id)
    {
        if (roots [rootID].banks.count(id) == 1)
        {
            if (roots [rootID].banks [id].dirname.empty())
            {
                roots [rootID].banks.erase(id);
            }
            else if (!banksSet[id])
            {
                synth->getRuntime().Log("Removed orphan bank " +to_string(id) + " in root " + to_string(rootID) + " " + roots [rootID].banks [id].dirname);
                roots [rootID].banks.erase(id);
            }
        }
    }
    if (thisRoot.size())
        thisRoot.clear(); // leave it tidy
    return result;
}


void Bank::saveToConfigFile(XMLwrapper& xml)
{
    writeVersion(BANKS_VERSION); // set current format
    for (size_t i = 0; i < MAX_BANK_ROOT_DIRS; i++)
    {
        if (roots.count(i) > 0 && !roots [i].path.empty())
        {
            string nodename = "BANKROOT";

            xml.beginbranch(nodename, i);
            xml.addparstr("bank_root", roots [i].path);
            BankEntryMap::const_iterator it;
            for (it = roots [i].banks.begin(); it != roots [i].banks.end(); ++it)
            {
                xml.beginbranch("bank_id", it->first);
                xml.addparstr("dirname", it->second.dirname);
                BankEntry &bank = roots [i].banks [it->first];
                size_t pos = 0;

                while (pos < MAX_INSTRUMENTS_IN_BANK)
                {
                    if (bank.instruments [pos].used)
                    {
                        xml.beginbranch("instrument_id", pos);
                        xml.addparbool("isUsed", bank.instruments [pos].used);
                        xml.addparstr("listname", bank.instruments [pos].name);
                        xml.addparstr("filename", bank.instruments [pos].filename);
                        xml.addpar("type", bank.instruments [pos].type);
                        xml.addparbool("ADDsynth", bank.instruments [pos].ADDsynth_used);
                        xml.addparbool("SUBsynth", bank.instruments [pos].SUBsynth_used);
                        xml.addparbool("PADsynth", bank.instruments [pos].PADsynth_used);
                        xml.addparbool("Yoshimi", bank.instruments [pos].yoshiType);
                        xml.endbranch();
                    }
                    ++pos;
                }
                xml.endbranch();
            }

            xml.endbranch();
        }
    }
}


void Bank::loadFromConfigFile(XMLwrapper& xml)
{
    string nodename = "BANKROOT";
    for (size_t i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
    {
        if (xml.enterbranch(nodename, i))
        {
            string dir = xml.getparstr("bank_root");
            if (!dir.empty())
            {
                size_t newIndex = addRootDir(dir);
                if (newIndex != i)
                {
                    changeRootID(newIndex, i);
                }
                for (size_t k = 0; k < MAX_INSTRUMENTS_IN_BANK; k++)
                {
                    if (xml.enterbranch("bank_id", k))
                    {
                        string bankDirname = xml.getparstr("dirname");
                        roots[i].banks[k].dirname = bankDirname;
                        BankEntry &bank = roots [i].banks [k];
                        size_t pos = 0;
                        while (pos < MAX_INSTRUMENTS_IN_BANK)
                        {
                            if (xml.enterbranch("instrument_id", pos))
                            {
                                bank.instruments[pos].used     = xml.getparbool("isUsed", false);
                                bank.instruments[pos].name     = xml.getparstr("listname");
                                bank.instruments[pos].filename = xml.getparstr("filename");
                                bank.instruments[pos].type     = xml.getpar("type",0 , -50, 100);
                                bank.instruments[pos].ADDsynth_used = xml.getparbool("ADDsynth", false);
                                bank.instruments[pos].SUBsynth_used = xml.getparbool("SUBsynth", false);
                                bank.instruments[pos].PADsynth_used = xml.getparbool("PADsynth", false);
                                bank.instruments[pos].yoshiType = xml.getparbool("Yoshimi", false);
                                xml.exitbranch();
                            }
                            ++pos;
                        }
                        xml.exitbranch();
                    }
                }
            }
            xml.exitbranch();
        }
    }
    xml.exitbranch();
}
