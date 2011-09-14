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
#include <boost/interprocess/sync/interprocess_mutex.hpp>

using namespace std;

#include "Misc/MiscFuncs.h"
#include "Misc/Part.h"

class ProgramBanks:private MiscFuncs
{
    public:
        ProgramBanks();
        ~ProgramBanks() {
            if(dbConn)
                sqlite3_close(dbConn);
        }
        int Setup(void);
        bool newDatabase(bool load_instruments = true);
        bool updateBank(unsigned char bank, string name);
        bool updateProgram(unsigned char bank,
                           unsigned char prog,
                           string name,
                           string xmldata);
        void loadBankList(void);
        string programXml(unsigned char bk, unsigned char prog);
        void loadProgramList(unsigned char bk);

        map<unsigned char, string> bankList;
        map<unsigned char, string> programList;

    private:
        bool loadInstrumentDatabase(void);
        bool sqlStepExecute(string location, string qry);

        bool sqlPrep(string location, string qry, sqlite3_stmt **stmt);
        bool sqlDoStep(string location, sqlite3_stmt *stmt);
        bool sqlDoStepRow(string location, sqlite3_stmt *stmt);
        bool sqlFinalize(string location, sqlite3_stmt **stmt);
        string dbQuoteSingles(string txt);
        string readXmlFile(const string filename);
        void dbErrorLog(string msg);
        const string dbFile;
        const string banksDir;
        const string presetsDir;
        sqlite3     *dbConn;
        boost::interprocess::interprocess_mutex dbLoadMutex;
};

extern ProgramBanks *progBanks;

#endif
