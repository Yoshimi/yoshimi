/*
    Bank.h - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2014-2021 Will Godfrey & others

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

#ifndef BANK_H
#define BANK_H

#include "Misc/Part.h"
#include "Misc/FormatFuncs.h"

#include <optional>
#include <string>
#include <map>

using std::string;


/** Entry for one instrument in a bank,
 *  with instrument metadata.
 */
struct InstrumentEntry
{
    string name;
    string filename;
    bool used;
    int  instType;
    bool yoshiFormat;
    bool ADDsynth_used;
    bool SUBsynth_used;
    bool PADsynth_used;

    InstrumentEntry()
        : name{}
        , filename{}
        , used{false}
        , instType{-1}
        , yoshiFormat{false}
        , ADDsynth_used{false}
        , SUBsynth_used{false}
        , PADsynth_used{false}
        { }

    void clear()
    {
        used = false;
        name.clear();
        filename.clear();
        PADsynth_used = false;
        ADDsynth_used = false;
        SUBsynth_used = false;
        yoshiFormat   = false;
    }
};

/** Maps instrument id to instrument entry. */
using InstrumentEntryMap = std::map<int, InstrumentEntry> ;

/** Describes a Bank
 *   - directory name
 *   - instrument map for this directory
 */
struct BankEntry
{
    string dirname;
    InstrumentEntryMap instruments;
};

/** Maps bank id to bank entry. */
using BankEntryMap = std::map<size_t, BankEntry>;

/** Contains the root path and the bank map of the root. */
struct RootEntry
{
    string path;
    size_t bankIdStep;
    BankEntryMap banks;

    RootEntry()
        : path{}
        , bankIdStep{1}
        , banks{}
        { }
};

/** Maps root id to root entry. */
using RootEntryMap = std::map<size_t, RootEntry>;


class SynthEngine;
class XMLtree;


class Bank
{
    friend class SynthEngine;

    public:
        Bank(SynthEngine&);
        // shall not be copied nor moved
        Bank(Bank&&)                 = delete;
        Bank(Bank const&)            = delete;
        Bank& operator=(Bank&&)      = delete;
        Bank& operator=(Bank const&) = delete;

        int getType(uint ninstrument, size_t bank, size_t root);
        string getname(uint ninstrument, size_t bank, size_t root);
        string getnamenumbered(uint ninstrument, size_t bank, size_t root);
        int  setInstrumentName(string const& name, int slot, size_t bank, size_t root);
        bool moveInstrument(uint ninstrument, string const& newname, int newslot, size_t oldBank, size_t newBank, size_t oldRoot, size_t newRoot);

        int engines_used(size_t rootID, size_t bankID, uint ninstrument);
        bool emptyslot(size_t rootID, size_t bankID, uint ninstrument);
        string clearslot(uint ninstrument, size_t rootID, size_t bankID);
        bool savetoslot(size_t rootID, size_t bankID, int ninstrument, int npart);
        string swapslot(uint n1, uint n2, size_t bank1, size_t bank2, size_t root1, size_t root2);
        string swapbanks(uint firstID, uint secondID, size_t firstRoot, size_t secondRoot);
        string getBankName(int bankID, size_t rootID);
        bool isDuplicateBankName(size_t rootID, string const& name);
        int getBankSize(int bankID, size_t rootID);
        int changeBankName(size_t rootID, size_t bankID, string const& newName);
        void checkbank(size_t rootID, size_t banknum);
        bool loadbank(size_t rootID, size_t banknum);
        string exportBank(string const& exportdir, size_t rootID, uint bankID);
        string importBank(string importdir, size_t rootID, uint bankID);
        bool isDuplicate(size_t rootID, size_t bankID, int pos, string filename);
        bool newIDbank(string const& newbankdir, uint bankID, size_t rootID = 0xff);
        bool newbankfile(string const& newbankdir, size_t rootID);
        string removebank(uint bankID, size_t rootID = 0xff);
        bool removeRoot(size_t rootID);
        bool changeRootID(size_t oldID, size_t newID);

        bool setCurrentRootID(size_t newRootID);
        uint findFirstBank(size_t newRootID);
        bool setCurrentBankID(size_t newBankID, bool ignoreMissing = true);
        size_t addRootDir(string const& newRootDir);
        bool establishBanks(std::optional<string> bankFile);
        bool installRoots();
        bool installNewRoot(size_t rootID, string rootdir, bool reload = false);
        void saveToConfigFile(XMLtree&);
        void loadFromConfigFile(XMLtree&);

        string getBankPath(size_t rootID, size_t bankID);
        string getRootPath(size_t rootID);
        string getFullPath(size_t rootID, size_t bankID, size_t ninstrument);

        const BankEntryMap& getBanks(size_t rootID);
        const RootEntryMap& getRoots();
        const BankEntry& getBank(size_t bankID, size_t rootID = UNUSED);

        string getBankFileTitle(size_t root, size_t bank);
        string getRootFileTitle(size_t root);
        void checkLocalBanks();
        size_t generateSingleRoot(string const& newRoot, bool clear = true);

        uint getVersion() {return version; }

    private:
        uint version;
        uint banksInRoots;
        uint instrumentsInBanks;

        const string defaultInsName;
        string foundLocal;

        RootEntryMap  roots;

        SynthEngine& synth;


        bool addtobank(size_t rootID, size_t bankID, int pos, const string filename, const string name);
        void deletefrombank(size_t rootID, size_t bankID, uint pos);
        bool isOccupiedRoot(string rootCandidate);
        bool isValidBank(string chkdir);

        InstrumentEntry& getInstrumentReference(size_t rootID, size_t bankID, size_t ninstrument);
        void updateShare(string bankdirs[], string baseDir, string shareID);
        void checkShare(string sourceDir, string destinationDir);
        bool transferDefaultDirs(string bankdirs[]);
        bool transferOneDir(string bankdirs[], int baseNumber, int listNumber);

        void addDefaultRootDirs(string bankdirs[]);

        size_t getNewRootIndex();
        size_t getNewBankIndex(size_t rootID);
};

#endif /*BANK_H*/
