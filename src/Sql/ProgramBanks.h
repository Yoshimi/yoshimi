/*
    ProgramBanks.h

    Copyright 2010, Alan Calvert

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

#include <map>
#include <sqlite3.h>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/Part.h"

class ProgramBanks : private MiscFuncs
{
    public:
        ProgramBanks();
        ~ProgramBanks();
        bool Setup(void);
        void scanInstrumentFiles(void);
        bool addBank(unsigned char bank, string name, string dir);
        void setBank(unsigned char bk) { loadProgramList(bankLsb = bk); }
        void loadBankList(void);
        bool addProgram(unsigned char bk, unsigned char prog, string name, string xmldata);
        string programXml(unsigned char bk, unsigned char prog);
        void loadProgramList(unsigned char bk);

        map<unsigned char, string> bankList;
        map<unsigned char, string> programList;

        unsigned char bankLsb;
        const unsigned char bankMsb;

    private:
        string dbQuoteSingles(string txt);
        string readXmlFile(const string filename);
        void dbErrorLog(string msg);

        const string xizext;
        sqlite3 *dbConn;
};

extern ProgramBanks *progBanks;

#endif
