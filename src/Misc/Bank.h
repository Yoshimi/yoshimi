/*
    Bank.h - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2014-2015 Will Godfrey & others

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
    Modified December 2017
*/

#ifndef BANK_H
#define BANK_H

#include <list>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/Part.h"
#include <map>
#include <vector>

#define ROOT_SIZE 128
#define BANK_SIZE 160


typedef struct _InstrumentEntry
{
    string name;
    string filename;
    bool used;
    bool PADsynth_used;
    bool ADDsynth_used;
    bool SUBsynth_used;
    bool yoshiType;
    _InstrumentEntry()
        :name(""),
         filename(""),
         used(false),
         PADsynth_used(false),
         ADDsynth_used(false),
         SUBsynth_used(false),
         yoshiType(false)
    {

    }
    void clear()
    {
        used = false;
        name.clear();
        filename.clear();
        PADsynth_used = false;
        ADDsynth_used = false;
        SUBsynth_used = false;
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

typedef map<size_t, map<string, size_t> > BankHintsMap;

class SynthEngine;

class Bank : private MiscFuncs
{
#ifdef YOSHIMI_LV2_PLUGIN
    friend class YoshimiLV2Plugin;
#endif
    friend class SynthEngine;
    public:
        Bank(SynthEngine *_synth);
        ~Bank();
        string getname(unsigned int ninstrument);
        string getfilename(unsigned int ninstrument);
        string getnamenumbered(unsigned int ninstrument);
        bool setname(unsigned int ninstrument, string newname, int newslot);
             // if newslot==-1 then this is ignored, else it will be put on that slot

        int engines_used(unsigned int ninstrument);
        bool emptyslotWithID(size_t rootID, size_t bankID, unsigned int ninstrument);
        bool emptyslot(unsigned int ninstrument) { return emptyslotWithID(currentRootID, currentBankID, ninstrument); }
        bool clearslot(unsigned int ninstrument);
        bool savetoslot(size_t rootID, size_t bankID, int ninstrument, int npart);
        bool swapslot(unsigned int n1, unsigned int n2);
        void swapbanks(unsigned int firstID, unsigned int secondID);
        string getBankName(int bankID, size_t rootID = 0xff);
        string getBankIDname(int bankID);
        int getBankSize(int bankID);
        bool setbankname(unsigned int BankID, string newname);
        bool loadbank(size_t rootID, size_t banknum);
        unsigned int importBank(string importdir, size_t rootID, unsigned int bankID);
        bool isDuplicate(size_t rootID, size_t bankID, int pos, const string filename);
        bool newIDbank(string newbankdir, unsigned int bankID, size_t rootID = 0xff);
        bool newbankfile(string newbankdir, size_t rootID);
        unsigned int removebank(unsigned int bankID, size_t rootID = 255);
        void rescanforbanks(void);
        void clearBankrootDirlist(void);
        void removeRoot(size_t rootID);
        bool changeRootID(size_t oldID, size_t newID);

        bool setCurrentRootID(size_t newRootID);
        bool setCurrentBankID(size_t newBankID, bool ignoreMissing = false);
        size_t getCurrentRootID() {return currentRootID;}
        size_t getCurrentBankID() {return currentBankID;}
        size_t addRootDir(string newRootDir);
        void parseConfigFile(XMLwrapper *xml);
        void saveToConfigFile(XMLwrapper *xml);

        string getBankPath(size_t rootID, size_t bankID);
        string getRootPath(size_t rootID);
        string getFullPath(size_t rootID, size_t bankID, size_t ninstrument);

        const BankEntryMap &getBanks(size_t rootID);
        const RootEntryMap &getRoots();
        const BankEntry &getBank(size_t bankID);

        string getBankFileTitle();
        string getRootFileTitle();

    private:
        bool addtobank(size_t rootID, size_t bankID, int pos, const string filename, const string name);
             // add an instrument to the bank, if pos is -1 try to find a position
             // returns true if the instrument was added

        void deletefrombank(size_t rootID, size_t bankID, unsigned int pos);
        void scanrootdir(int root_idx); // scans a root dir for banks
        size_t add_bank(string name, string, size_t rootID);
        bool check_bank_duplicate(string alias);

        //string dirname;
        const string defaultinsname;
        const string xizext;
        const string force_bank_dir_file;
        SynthEngine *synth;

        size_t        currentRootID;
        size_t        currentBankID;

        RootEntryMap  roots;
        BankHintsMap hints;

        InstrumentEntry &getInstrumentReference(size_t ninstrument );
        InstrumentEntry &getInstrumentReference(size_t rootID, size_t bankID, size_t ninstrument );

        void addDefaultRootDirs();

        size_t getNewRootIndex();
        size_t getNewBankIndex(size_t rootID);
};

#endif
