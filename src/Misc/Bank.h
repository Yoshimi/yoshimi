/*
    Bank.h - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

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

    This file is a derivative of a ZynAddSubFX original, modified October 2010
*/

#ifndef BANK_H
#define BANK_H

#include <list>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/Part.h"

#define BANK_SIZE 160

#define MAX_NUM_BANKS 400

typedef struct {
    string name;
    string alias;
    string dir;
} bankstruct_t;

class Bank : private MiscFuncs
{
    public:
        Bank();
        ~Bank();
        string getname(unsigned int ninstrument);
        string getnamenumbered(unsigned int ninstrument);
        void setname(unsigned int ninstrument, string newname, int newslot);
             // if newslot==-1 then this is ignored, else it will be put on that slot

        bool isPADsynth_used(unsigned int ninstrument);
        bool emptyslot(unsigned int ninstrument);
        void clearslot(unsigned int ninstrument);
        void savetoslot(unsigned int ninstrument, Part *part);
        void loadfromslot(unsigned int ninstrument, Part *part);
        void swapslot(unsigned int n1, unsigned int n2);
        bool loadbank(string bankdirname);
        bool newbank(string newbankdirname);
        void rescanforbanks(void);
        bool locked(void) { return (dirname.size() == 0); };
             // Check if the bank is locked (i.e. the file opened was readonly)

        bankstruct_t banks[MAX_NUM_BANKS];
        string bankfiletitle; //this is shown on the UI of the bank (the title of the window)

    private:
        bool addtobank(int pos, string filename, string name);
             // add an instrument to the bank, if pos is -1 try to find a position
             // returns true if the instrument was added

        void deletefrombank(unsigned int pos);
        void clearbank(void);
        void scanrootdir(string rootdir); // scans a root dir for banks
        static bool bankCmp(bankstruct_t lhs, bankstruct_t rhs)
            { return lhs.name < rhs.name; };
        void add_bank(string name, string dir);
        bool check_bank_duplicate(string alias);

        string dirname;
        const string defaultinsname;

        string tmpinsname[BANK_SIZE]; // this keeps the numbered names
        struct bank_instrument_t {
            string name;
            string filename;
            bool used;
            unsigned char PADsynth_used;
        } bank_instrument[BANK_SIZE];

        list<bankstruct_t> bank_dir_list;
        const int bank_size;
        const string xizext;
        const string force_bank_dir_file;
};

#endif
