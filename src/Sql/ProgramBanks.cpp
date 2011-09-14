/*
    ProgramBanks.cpp

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

#include <sstream>
#include <list>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <zlib.h>
#include <boost/shared_ptr.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

using namespace std;

#include "MusicIO/Midi.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "Misc/Part.h"
#include "Sql/ProgramBanks.h"
#include "MasterUI.h"

ProgramBanks *progBanks = NULL;

ProgramBanks::ProgramBanks()
    :dbFile(Runtime.DataDirectory + string("/yoshimi.db")),
      banksDir(Runtime.DataDirectory + string("/banks")),
      presetsDir(Runtime.DataDirectory + string("/presets")),
      dbConn(NULL)
{ }


int ProgramBanks::Setup(void)
{
    string instrumentsTarFile = string(BASE_INSTALL_DIR) + string(
        "/share/yoshimi/yoshimi-instruments.tar.gz");
    if(!isDirectory(banksDir)) {
        if(!isRegFile(instrumentsTarFile)) {
            Runtime.Log(
                "Default instrument tar file " + instrumentsTarFile
                + " not found");
            return -1;
        }
        string cmd = string("cd ") + Runtime.DataDirectory + string(
            " && tar xzf ")
                     + instrumentsTarFile + string(" banks");
        if(system(cmd.c_str()) < 0) {
            Runtime.Log("Failed to install instrument files to " + banksDir,
                        true);
            return -1;
        }
        else
            Runtime.Log("Instrument files installed in " + banksDir);
    }

    if(!isDirectory(presetsDir)) {
        if(!isRegFile(instrumentsTarFile)) {
            Runtime.Log(
                "Default instrument tar file " + instrumentsTarFile
                + " not found");
            return -1;
        }
        string cmd = string("cd ") + Runtime.DataDirectory + string(
            " && tar xzf ")
                     + instrumentsTarFile + string(" presets");
        if(system(cmd.c_str()) < 0) {
            Runtime.Log("Failed to install presets to " + presetsDir, true);
            return -1;
        }
        else
            Runtime.Log("Instrument presets installed in " + presetsDir);
    }
    if(isRegFile(dbFile)) {
        if(SQLITE_OK !=
           sqlite3_open_v2(dbFile.c_str(), &dbConn, SQLITE_OPEN_READWRITE,
                           NULL)) {
            dbErrorLog("open database " + dbFile + " failed");
            sqlite3_close(dbConn);
            dbConn = NULL;
            return -2;
        }
        sqlite3_extended_result_codes(dbConn, 1);
        sqlite3_limit(dbConn, SQLITE_LIMIT_VARIABLE_NUMBER, 50);
    }
    else {
        if(Runtime.showGui)
            return 1;
        if(!newDatabase(true))
            return -1;
    }
    loadBankList();
    return 0;
}


bool ProgramBanks::newDatabase(bool load_instruments)
{
    dbLoadMutex.lock();
    sqlite3_close(dbConn);
    if(isRegFile(dbFile))
        unlink(dbFile.c_str());
    if(SQLITE_OK !=
       sqlite3_open_v2(dbFile.c_str(), &dbConn, SQLITE_OPEN_READWRITE
                       | SQLITE_OPEN_CREATE, NULL)) {
        dbErrorLog("Create database " + dbFile + " failed");
        dbLoadMutex.unlock();
        return false;
    }
    sqlite3_extended_result_codes(dbConn, 1);
    sqlite3_limit(dbConn, SQLITE_LIMIT_VARIABLE_NUMBER, 50);
    string newdbsql[] = {
        "create table banks (row INTEGER PRIMARY KEY AUTOINCREMENT, banknum TINYINT, name VARCHAR(80))",
        "create unique index idx on banks (row, banknum)",
        "create table programs (row INTEGER PRIMARY KEY AUTOINCREMENT, banknum TINYINT, prognum TINYINT, name VARCHAR(80), xml TEXT)",
        "create unique index bankprogidx on programs (row, banknum, prognum)",
        string()
    };
    for(int i = 0; newdbsql[i].size() > 0; ++i)
        if(!sqlStepExecute("newDatabase()", newdbsql[i]))
            goto not_good;
    Runtime.Log("Empty program bank database created.");
    if(load_instruments) {
        const string xizext  = ".xiz";
        string       rootdir = Runtime.DataDirectory + "/banks/";
        DIR *rootDIR = opendir(rootdir.c_str());
        if(!rootDIR) {
            Runtime.Log("Failed to open bank root directory");
            goto not_good;
        }
        boost::shared_ptr<XMLwrapper> xmlwrap = boost::shared_ptr<XMLwrapper>(
            new XMLwrapper());
        struct dirent *dent;
        list<string>::iterator progitx;
        string qry;
        Runtime.Log("Loading program bank database from base .xiz files");
        for(unsigned char bank = 0;
            bank < BANK_LIMIT && (dent = readdir(rootDIR)) != NULL;
           ) {
            string chkbank = string(dent->d_name);
            if(chkbank[0] == '.')
                continue;
            string chkdir = rootdir + chkbank;
            if(!isDirectory(chkdir))
                continue;
            DIR *chkDIR = opendir(chkdir.c_str());
            if(!chkDIR) {
                Runtime.Log(
                    "Failed to open bank directory candidate: " + chkdir);
                continue;
            }
            qry = string("insert into banks (banknum, name) values (")
                  + asString((int)bank) + string(", ")
                  + dbQuoteSingles(chkbank) + string(")");
            if(!sqlStepExecute("newDatabase()", qry))
                goto not_good;
            list<string>   progfiles;
            struct dirent *subdent;
            while((subdent = readdir(chkDIR)) != NULL)
                progfiles.push_back(string(subdent->d_name));
            progfiles.sort();
            int prognum = 0;
            for(progitx = progfiles.begin();
                progitx != progfiles.end() && prognum < BANK_LIMIT;
                ++progitx) {
                string chkfile = *progitx;
                if(chkfile[0] == '.')
                    continue;
                size_t xizpos  = chkfile.rfind(xizext); // check for .xiz extension
                string chkpath = chkdir + "/" + chkfile;
                if((xizpos != string::npos)
                   && ((xizpos + xizext.size()) == chkfile.size())) {
                    if(!xmlwrap->loadXMLfile(chkpath)) {
                        Runtime.Log("Failed to xml->load file " + chkpath);
                        continue;
                    }
                    string progname;
                    if(xmlwrap->enterbranch("INSTRUMENT")
                       && xmlwrap->enterbranch("INFO"))
                        progname = xmlwrap->getparstr("name");
                    else {
                        Runtime.Log(
                            "Weird parse on file, can't get program name: "
                            + chkpath);
                        continue;
                    }
                    qry = string(
                        "insert into programs (banknum, prognum, name, xml) values (")
                          + asString((int)bank) + string(", ")
                          + asString((int)prognum) + string(", ")
                          + dbQuoteSingles(progname) + string(", ")
                          + dbQuoteSingles(string(xmlwrap->xmlData)) + string(
                        ")");
                    if(!sqlStepExecute("newDatabase()", qry))
                        goto not_good;
                    ++prognum;
                }
                else {
                    Runtime.Log("Not an xiz: " + chkpath);
                    continue;
                }
            }
            closedir(chkDIR);
            Runtime.Log("Bank " + asString(++bank) + string(
                            " ") + chkbank + string(" processed."));
        }
        closedir(rootDIR);
        Runtime.Log("Loading of program bank database complete.");
    }
    dbLoadMutex.unlock();
    return true;

not_good:
    dbLoadMutex.unlock();
    return false;
}


void ProgramBanks::loadBankList(void)
{
    bankList.clear();
    for(int i = 0; i < BANK_LIMIT; ++i)
        bankList[i] = string();
    string qry = string("select banknum, name from banks order by banknum");
    sqlite3_stmt *stmt = NULL;
    dbLoadMutex.lock();
    if(SQLITE_OK !=
       sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
        dbErrorLog(string("loadBankList: ") + qry);
    else
        while(SQLITE_ROW == sqlite3_step(stmt))
            bankList[sqlite3_column_int(stmt, 0)] = string(
                (const char *)sqlite3_column_text(stmt, 1));
    if(SQLITE_OK != sqlite3_finalize(stmt))
        dbErrorLog(string("Finalize failed: ") + qry);
    dbLoadMutex.unlock();
}


void ProgramBanks::loadProgramList(unsigned char bk)
{
    programList.clear();
    for(int i = 0; i < BANK_LIMIT; ++i)
        programList[i] = string();
    string qry = string("select prognum, name from programs where banknum=")
                 + asString(bk) + string(" order by prognum");
    sqlite3_stmt *stmt = NULL;
    dbLoadMutex.lock();
    if(SQLITE_OK !=
       sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
        dbErrorLog(string("loadProgramList() prep failed: ") + qry);
    else
        while(SQLITE_ROW == sqlite3_step(stmt))
            programList[sqlite3_column_int(stmt, 0)] = string(
                (const char *)sqlite3_column_text(stmt, 1));
    if(SQLITE_OK != sqlite3_finalize(stmt))
        dbErrorLog(string("loadProgramList() finalize failed: ") + qry);
    dbLoadMutex.unlock();
}


bool ProgramBanks::updateBank(unsigned char bank, string name)
{
    string qry = string("update banks set name=") + dbQuoteSingles(name)
                 + string(" where banknum = ") + asString(bank);
    dbLoadMutex.lock();
    bool ok = sqlStepExecute("updateBank()", qry);
    dbLoadMutex.unlock();
    return ok;
}


bool ProgramBanks::updateProgram(unsigned char bank,
                                 unsigned char prog,
                                 string name,
                                 string xmldata)
{
    string qry = string("update programs set name=") + dbQuoteSingles(name)
                 + string(",xml=") + dbQuoteSingles(xmldata)
                 + string(" where banknum=") + asString((int)bank)
                 + string(" and prognum = ") + asString((int)prog);
    dbLoadMutex.lock();
    bool ok = sqlStepExecute("updateBank()", qry);
    dbLoadMutex.unlock();
    return ok;
}


string ProgramBanks::readXmlFile(const string filename)
{
    string xmldata = string();
    gzFile gzf     = gzopen(filename.c_str(), "rb");
    if(gzf != NULL) {
        const int    bufSize = 4096;
        char         fetchBuf[4097];
        int          this_read;
        int          total_bytes = 0;
        stringstream readStream;
        for(bool quit = false; !quit;) {
            memset(fetchBuf, 0, sizeof(fetchBuf) * sizeof(char));
            this_read = gzread(gzf, fetchBuf, bufSize);
            if(this_read > 0) {
                readStream << fetchBuf;
                total_bytes += this_read;
            }
            else
            if(this_read < 0) {
                int errnum;
                Runtime.Log("Read error in zlib: "
                            + string(gzerror(gzf, &errnum)));
                if(errnum == Z_ERRNO)
                    Runtime.Log("Filesystem error: " + string(strerror(errno)));
                quit = true;
            }
            else
            if(total_bytes > 0) {
                xmldata = readStream.str();
                quit    = true;
            }
        }
        gzclose(gzf);
    }
    else
        Runtime.Log(
            "Failed to open xml file " + filename + " for load, errno: "
            + asString(errno) + "  " + string(strerror(errno)));
    return xmldata;
}


string ProgramBanks::dbQuoteSingles(string txt)
{
    string quoted = "'";
    for(unsigned int x = 0; x < txt.length(); ++x) {
        quoted.append(txt.substr(x, 1));
        if(txt.substr(x, 1) == "'")
            quoted.append("'");
    }
    quoted.append("'");
    return quoted;
}


string ProgramBanks::programXml(unsigned char bank, unsigned char prog)
{
    sqlite3_stmt *stmt = NULL;
    string xml;
    string qry = string("select xml from programs where banknum=")
                 + asString(bank) + string(" and prognum=")
                 + asString(prog);
    dbLoadMutex.lock();
    if(SQLITE_OK !=
       sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
        dbErrorLog(string("programName prep: ") + qry);
    else
    if(SQLITE_ROW == sqlite3_step(stmt))
        xml = string((const char *)sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    dbLoadMutex.unlock();
    return xml;
}


void ProgramBanks::dbErrorLog(string msg)
{
    Runtime.Log(msg + ": " + asString(sqlite3_extended_errcode(dbConn))
                + string(sqlite3_errmsg(dbConn)), true);
}

bool ProgramBanks::sqlStepExecute(string location, string qry)
{
    bool ok = false;
    sqlite3_stmt *stmt = NULL;
    string errmsg;
    if(SQLITE_OK ==
       sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)) {
        if(SQLITE_DONE == sqlite3_step(stmt))
            ok = true;
        else
            errmsg = location + string(", bad step: ");
    }
    else
        errmsg = location + string(", bad prepare: ");
    if((SQLITE_OK != sqlite3_finalize(stmt)) && ok) {
        ok     = false;
        errmsg = location + string(", bad finalize: ");
    }
    if(!ok)
        dbErrorLog(errmsg + qry);
    return ok;
}
