/*
    Bank.h - Instrument Bank

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

#ifndef BANK_H
#define BANK_H

#include <list>

using namespace std;

#include "Misc/Part.h"

#define BANK_SIZE 160

#define MAX_NUM_BANKS 400

typedef struct {
    string name;
    string dir;
} bankstruct_t;

class Bank
{
    public:
        Bank();
        ~Bank();
        string getName(unsigned int ninstrument);
        string getNameNumbered(unsigned int ninstrument);
        void setName(unsigned int ninstrument, string newname, int newslot);
             // if newslot==-1 then this is ignored, else it will be put on that slot

        bool isPADsynth_used(unsigned int ninstrument);
        bool emptySlot(unsigned int ninstrument);
        void clearSlot(unsigned int ninstrument);
        void saveToSlot(unsigned int ninstrument, Part *part);
        void loadFromSlot(unsigned int ninstrument, Part *part);
        void swapSlot(unsigned int n1, unsigned int n2);
        bool loadBank(string bankdirname);
        bool newBank(string newbankdirname);
        void rescanBanks(void);
        bool locked(void) { return (dirname.size() == 0); };
             // Check if the bank is locked (i.e. the file opened was readonly)

        bankstruct_t banks[MAX_NUM_BANKS];
        string bankfiletitle; //this is shown on the UI of the bank (the title of the window)

    private:
        bool addToBank(int pos, string filename, string name);
             // add an instrument to the bank, if pos is -1 try to find a position
             // returns true if the instrument was added

        void deleteFromBank(unsigned int pos);
        void clearBank(void);
        void scanRootdir(string rootdir); // scans a root dir for banks
        bool addBank(string name, string dir);

        string dirname;
        const string defaultinsname;

        string tmpinsname[BANK_SIZE]; // this keeps the numbered names
        struct bank_instrument_t {
            bool used;
            string name;
            string filename;
            bool PADsynth_used;
        } bank_instrument[BANK_SIZE];

        list<bankstruct_t> banklist;
        const int bank_size;
        const string xizext;
        const string force_bank_dir_file;
};

#endif
