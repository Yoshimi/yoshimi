/*
    Bank.h - Instrument Bank

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BANK_H
#define BANK_H

#include "globals.h"
#include "Misc/XMLwrapper.h"
#include "Misc/Part.h"

#define BANK_SIZE 160

#define MAX_NUM_BANKS 400

class Bank
{
    public:
        Bank();
        ~Bank();
        string getname(unsigned int ninstrument);
        string getnamenumbered(unsigned int ninstrument);
        void setname(unsigned int ninstrument, string newname, int newslot);
             // if newslot==-1 then this is ignored, else it will be put on that slot

        bool isPADsynth_used(unsigned int ninstrument);
        int emptyslot(unsigned int ninstrument);
        void clearslot(unsigned int ninstrument);
        void savetoslot(unsigned int ninstrument,Part *part);
        void loadfromslot(unsigned int ninstrument,Part *part);
        void swapslot(unsigned int n1,unsigned int n2);
        int loadbank(const char *bankdirname);
        int newbank(const char *newbankdirname);
        void rescanforbanks();
        int locked();

        char *bankfiletitle; //this is shown on the UI of the bank (the title of the window)
        struct bankstruct {
            char *dir;
            char *name;
        };
        bankstruct banks[MAX_NUM_BANKS];

    private:
        // it adds a filename to the bank
        // if pos is -1 it try to find a position
        // returns -1 if the bank is full, or 0 if the instrument was added
        int addtobank(int pos, const char* filename, const char* name);
        void deletefrombank(int pos);
        void clearbank();
        void scanrootdir(char *rootdir);//scans a root dir for banks

        string defaultinsname;
        //char tmpinsname[BANK_SIZE][PART_MAX_NAME_LEN + 20]; // this keeps the numbered names
        string tmpinsname[BANK_SIZE]; // this keeps the numbered names
        struct ins_t {
            bool used;
            char name[PART_MAX_NAME_LEN + 1];
            char *filename;
            struct {
                bool PADsynth_used;
            } info;
        } ins[BANK_SIZE];

        char *dirname;
};

#endif
