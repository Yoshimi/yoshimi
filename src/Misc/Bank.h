/*
    Bank.h - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2014-2020 Will Godfrey & others

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

#ifndef BANK_H
#define BANK_H

#include "Misc/Part.h"

#include <string>
#include <map>

using std::string;
using std::map;

typedef struct _InstrumentEntry
{
    string name;
    string filename;
    int type;
    bool used;
    unsigned char PADsynth_used;
    unsigned char ADDsynth_used;
    unsigned char SUBsynth_used;
    bool yoshiType;
    _InstrumentEntry()
        :name(""),
         filename(""),
         type(0),
         used(false),
         PADsynth_used(0),
         ADDsynth_used(0),
         SUBsynth_used(0),
         yoshiType(false)
    {

    }
    void clear()
    {
        used = false;
        name.clear();
        filename.clear();
        PADsynth_used = 0;
        ADDsynth_used = 0;
        SUBsynth_used = 0;
        yoshiType = false;
    }
} InstrumentEntry; // Contains the leafname of the instrument.

typedef map<int, InstrumentEntry> InstrumentEntryMap; // Maps instrument id to instrument entry.

typedef struct _BankEntry
{
    string dirname;
    InstrumentEntryMap instruments;
} BankEntry; // Contains the bank directory name and the instrument map of the bank.

typedef map<size_t, BankEntry> BankEntryMap; // Maps bank id to bank entry.


typedef struct _RootEntry
{
    string path;
    BankEntryMap banks;
    size_t bankIdStep;
    _RootEntry(): bankIdStep(1)
    {}
} RootEntry; // Contains the root path and the bank map of the root.

typedef map<size_t, RootEntry> RootEntryMap; // Maps root id to root entry.

class SynthEngine;

class Bank
{
    friend class SynthEngine;

    public:
        Bank(SynthEngine *_synth);
        int getType(unsigned int ninstrument, size_t bank, size_t root);
        string getname(unsigned int ninstrument, size_t bank, size_t root);
        string getnamenumbered(unsigned int ninstrument, size_t bank, size_t root);
        int setInstrumentName(const string& name, int slot, size_t bank, size_t root);
        bool moveInstrument(unsigned int ninstrument, const string& newname, int newslot, size_t oldBank, size_t newBank, size_t oldRoot, size_t newRoot);
             // if newslot==-1 then this is ignored, else it will be put on that slot

        int engines_used(size_t rootID, size_t bankID, unsigned int ninstrument);
        bool emptyslot(size_t rootID, size_t bankID, unsigned int ninstrument);
        std::string clearslot(unsigned int ninstrument, size_t rootID, size_t bankID);
        bool savetoslot(size_t rootID, size_t bankID, int ninstrument, int npart);
        std::string swapslot(unsigned int n1, unsigned int n2, size_t bank1, size_t bank2, size_t root1, size_t root2);
        std::string swapbanks(unsigned int firstID, unsigned int secondID, size_t firstRoot, size_t secondRoot);
        string getBankName(int bankID, size_t rootID);
        bool isDuplicateBankName(size_t rootID, const string& name);
        int getBankSize(int bankID, size_t rootID);
        int changeBankName(size_t rootID, size_t bankID, const string& newName);
        bool loadbank(size_t rootID, size_t banknum);
        std::string exportBank(const string& exportdir, size_t rootID, unsigned int bankID);
        std::string importBank(string importdir, size_t rootID, unsigned int bankID);
        bool isDuplicate(size_t rootID, size_t bankID, int pos, const string filename);
        bool newIDbank(const string& newbankdir, unsigned int bankID, size_t rootID = 0xff);
        bool newbankfile(const string& newbankdir, size_t rootID);
        std::string removebank(unsigned int bankID, size_t rootID = 0xff);
        bool removeRoot(size_t rootID);
        bool changeRootID(size_t oldID, size_t newID);

        bool setCurrentRootID(size_t newRootID);
        unsigned int findFirstBank(size_t newRootID);
        bool setCurrentBankID(size_t newBankID, bool ignoreMissing = true);
        size_t addRootDir(const string& newRootDir);
        bool parseBanksFile(XMLwrapper *xml);
        bool installRoots();
        bool installNewRoot(size_t rootID, string rootdir, bool reload = false);
        void saveToConfigFile(XMLwrapper *xml);

        string getBankPath(size_t rootID, size_t bankID);
        string getRootPath(size_t rootID);
        string getFullPath(size_t rootID, size_t bankID, size_t ninstrument);

        const BankEntryMap &getBanks(size_t rootID);
        const RootEntryMap &getRoots();
        const BankEntry &getBank(size_t bankID, size_t rootID = UNUSED);

        string getBankFileTitle(size_t root, size_t bank);
        string getRootFileTitle(size_t root);
        int InstrumentsInBanks;
        int BanksInRoots;
        int readVersion(void)
            {return BanksVersion;}
        void writeVersion(int version)
            {BanksVersion = version;}
        int BanksVersion;
        void checkLocalBanks(void);
        size_t generateSingleRoot(const string& newRoot, bool clear = true);

    private:
        bool addtobank(size_t rootID, size_t bankID, int pos, const string filename, const string name);
             // add an instrument to the bank, if pos is -1 try to find a position
             // returns true if the instrument was added

        void deletefrombank(size_t rootID, size_t bankID, unsigned int pos);
        bool isValidBank(string chkdir);

        //string dirname;
        const string defaultinsname;
        SynthEngine *synth;

        RootEntryMap  roots;

        InstrumentEntry &getInstrumentReference(size_t rootID, size_t bankID, size_t ninstrument );
        void updateShare(string bankdirs[], string localDir, string shareID);
        void checkShare(string sourceDir, string destinationDir);
        bool transferDefaultDirs(string bankdirs[]);
        bool transferOneDir(string bankdirs[], int baseNumber, int listNumber);

        void addDefaultRootDirs(string bankdirs[]);

        size_t getNewRootIndex();
        size_t getNewBankIndex(size_t rootID);
};

#endif /*BANK_H*/
