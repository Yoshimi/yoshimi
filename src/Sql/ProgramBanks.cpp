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

#include <iostream>

#include <sstream>
#include <set>
#include <list>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <zlib.h>
#include <boost/shared_ptr.hpp>

using namespace std;

#include "MusicIO/Midi.h"
#include "Misc/XMLwrapper.h"
#include "Misc/SynthEngine.h"
#include "Misc/Config.h"
#include "Misc/Part.h"
#include "Sql/ProgramBanks.h"

ProgramBanks *progBanks = NULL;

ProgramBanks::ProgramBanks() :
    bankLsb(Runtime.currentBank),
    bankMsb(0),
    xizext(".xiz"),
    dbConn(NULL)
{ }


ProgramBanks::~ProgramBanks() { if (dbConn) sqlite3_close(dbConn); }


bool ProgramBanks::Setup(void)
{
    if (Runtime.DbFile.empty() || !isRegFile(Runtime.DbFile))
    {
        Runtime.Log("Database file [" + Runtime.DbFile + "] not found!");
        return false;
    }
    if (SQLITE_OK != sqlite3_open_v2(Runtime.DbFile.c_str(), &dbConn,
                                     SQLITE_OPEN_READWRITE, NULL))
    {
        dbErrorLog("open database " + Runtime.DbFile + " failed");
        if (dbConn)
            sqlite3_close(dbConn);
        dbConn = NULL;
        return false;
    }
    sqlite3_extended_result_codes(dbConn, 1);
    sqlite3_limit(dbConn, SQLITE_LIMIT_VARIABLE_NUMBER, 50);
    loadBankList();
    setBank(Runtime.currentBank);
    return true;
}


void ProgramBanks::dbErrorLog(string msg)
{
    Runtime.Log(msg + ": " + asString(sqlite3_extended_errcode(dbConn))
                + string(sqlite3_errmsg(dbConn)), true);
}


void ProgramBanks::scanInstrumentFiles(void)
{
    string rootdir = Runtime.DataDir + "/banks/";
    DIR *rootDIR = opendir(rootdir.c_str());
    if (rootDIR == NULL)
    {
        Runtime.Log("Failed to open bank root directory");
        return;
    }

    string qry = string("begin transaction");
    sqlite3_stmt *stmt = NULL;
    if (!(SQLITE_OK == sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)
          && SQLITE_DONE == sqlite3_step(stmt)))
    {
        dbErrorLog(qry);
        sqlite3_finalize(stmt);
        return;
    }
    qry = string("delete from instrument where 1 = 1; delete from programbank where 1 = 1");
    if (!(SQLITE_OK == sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)
          && SQLITE_DONE == sqlite3_step(stmt)))
    {
        dbErrorLog(qry);
        sqlite3_finalize(stmt);
        return;
    }
    qry = string("commit transaction");
    if (!(SQLITE_OK == sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)
        && SQLITE_DONE == sqlite3_step(stmt)))
    {
        dbErrorLog(qry);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    boost::shared_ptr<XMLwrapper> xmlwrap = boost::shared_ptr<XMLwrapper>(new XMLwrapper());
    string chkdir;
    struct dirent *dent;
    struct dirent *subdent;
    DIR *chkDIR;
    size_t xizpos;
    string chkbank;
    string chkpath;
    string chkfile;
    string msg;

    for (unsigned char bank = 0; bank < BANK_LIMIT && (dent = readdir(rootDIR)) != NULL;)
    {
        chkbank = string(dent->d_name);
        if (chkbank == "." || chkbank == "..")
            continue;
        chkdir = rootdir + chkbank;
        if (!isDirectory(chkdir))
            continue;
        chkDIR = opendir(chkdir.c_str());
        if (!chkDIR)
        {
            Runtime.Log("Failed to open bank directory candidate: " + chkdir);
            continue;
        }
        if (!addBank(bank, chkbank, chkdir))
        {
            Runtime.Log("Failed addBank " + asString(bank) + ", dir " + chkbank + " failed");
            continue;
        }

        string progname;
        for (unsigned char prognum = 0; prognum < BANK_LIMIT && (subdent = readdir(chkDIR)) != NULL;)
        {
            chkfile = string(subdent->d_name);
            if (chkfile == "." || chkfile == ".." || chkfile.size() <= (xizext.size() + 5))
                continue;
            chkpath = chkdir + "/" + chkfile;
            if (isRegFile(chkpath)
                && (xizpos = chkfile.rfind(xizext)) != string::npos
                && xizext.size() == (chkfile.size() - xizpos)) // just <name>.xiz files please
            {                                                  // sa verific daca e si extensia dorita
                if (!xmlwrap->loadXMLfile(chkpath))
                {
                    Runtime.Log("Failed to xml->load file " + chkpath);
                    continue;
                }
                if (xmlwrap->enterbranch("INSTRUMENT"))
                {
                    if (xmlwrap->enterbranch("INFO"))
                    {
                        progname = xmlwrap->getparstr("name");
                        xmlwrap->exitbranch();
                    }
                    xmlwrap->exitbranch();
                }
                else
                {
                    Runtime.Log("Weird parse on file " + chkpath);
                    continue;
                }
                if (addProgram(bank, prognum, progname, xmlwrap->xmldata))
                    Runtime.Log("Bank " + asString(bank + 1) + " Program " + asString(++prognum)
                                + " : " +  progname + " => " + chkfile);
                else
                    Runtime.Log("Failed to add program " + chkpath);
            }
        }
        closedir(chkDIR);
        ++bank;
    }
    Runtime.Log("Bank rescan complete");
    closedir(rootDIR);
}


void ProgramBanks::loadBankList(void)
{
    bankList.clear();
    for (unsigned char u = 0; u < BANK_LIMIT; ++u)
        bankList[u] = string();
    string qry = string("select banknumber, name from programbank order by banknumber");
    sqlite3_stmt *stmt = NULL;
    if (SQLITE_OK != sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
        dbErrorLog(qry);
    else while (SQLITE_ROW == sqlite3_step(stmt))
        bankList[sqlite3_column_int(stmt, 0)] = string((const char*)sqlite3_column_text(stmt, 1));
    sqlite3_finalize(stmt);
}


void ProgramBanks::loadProgramList(unsigned char bk)
{
    programList.clear();
    for (unsigned char u = 0; u < BANK_LIMIT; ++u)
        programList[u] = string();
    string qry = string("select prognumber, name from instrument where banknumber=")
                 + asString(bk) + string(" order by prognumber");
    sqlite3_stmt *stmt = NULL;
    if (SQLITE_OK != sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
        dbErrorLog(qry);
    else while (SQLITE_ROW == sqlite3_step(stmt))
        programList[sqlite3_column_int(stmt, 0)] = string((const char*)sqlite3_column_text(stmt, 1));
    sqlite3_finalize(stmt);
}


bool ProgramBanks::addBank(unsigned char bank, string name, string dir)
{
    bool ok = false;
    sqlite3_int64 bankrow = -1;

    string qry = string("begin transaction");
    sqlite3_stmt *stmt = NULL;
    if (!(SQLITE_OK == sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)
          && SQLITE_DONE == sqlite3_step(stmt)))
    {
        dbErrorLog(qry);
        goto endgame;
    }

    qry = string("select rowid from programbank where banknumber=") + asString((int)bank);
    if (SQLITE_OK != sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
    {
        dbErrorLog(qry);
        goto endgame;
    }
    if (SQLITE_ROW == sqlite3_step(stmt))
    {
        bankrow = sqlite3_column_int64(stmt, 0);
        qry = string("update programbank set banknumber=") + asString((int)bank)
              + string(",name=") + dbQuoteSingles(name)
              + string(",dir=") + dbQuoteSingles(dir)
              + string(" where row = ") + asString((long long)bankrow);
        if (SQLITE_OK != sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
        {
            dbErrorLog(qry);
            goto endgame;
        }
        if (SQLITE_DONE != sqlite3_step(stmt))
        {
            dbErrorLog(qry);
            goto endgame;
        }
    }
    else
    {
        qry = string("insert into programbank (banknumber, name, dir) values (")
              + asString((int)bank) + string(", ") + dbQuoteSingles(name) + string(", ")
              + dbQuoteSingles(dir) + string(")");
        if (!(SQLITE_OK == sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)
              && SQLITE_DONE == sqlite3_step(stmt)))
        {
            dbErrorLog(qry);
            goto endgame;
        }
    }
    qry = string("commit transaction");
    if (SQLITE_OK == sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)
        && SQLITE_DONE == sqlite3_step(stmt))
        ok = true;

endgame:
    sqlite3_finalize(stmt);
    return ok;
}


bool ProgramBanks::addProgram(unsigned char bank, unsigned char prog, string name, string xmldata)
{
    bool ok = false;
    sqlite3_int64 progrow = -1;
    string qry = string("begin transaction");
    sqlite3_stmt *stmt = NULL;
    if (!(SQLITE_OK == sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)
          && SQLITE_DONE == sqlite3_step(stmt)))
    {
        dbErrorLog(qry);
        goto endgame;
    }
    qry = string("select rowid from instrument where banknumber=") + asString((int)bank)
          + string(" and prognumber = ") + asString((int)prog);
    if (SQLITE_OK != sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
    {
        dbErrorLog(qry);
        goto endgame;
    }
    if (SQLITE_ROW == sqlite3_step(stmt))
    {
        progrow = sqlite3_column_int64(stmt, 0);
        qry = string("update instrument set name=") + dbQuoteSingles(name)
                     + string(",xml=") + dbQuoteSingles(xmldata)
                     + string(" where row = ") + asString((long long)progrow);
        if (SQLITE_OK != sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
        {
            dbErrorLog(qry);
            goto endgame;
        }
        if (SQLITE_DONE == sqlite3_step(stmt))
            ok = true;
        else
            dbErrorLog(qry);
    }
    else
    {
        qry = string("insert into instrument (banknumber, prognumber, name, xml) values (")
              + asString((int)bank) + string(", ")
              + asString((int)prog) + string(", ")
              + dbQuoteSingles(name) + string(", ")
              + dbQuoteSingles(string(xmldata)) + string(")");

        if (!(SQLITE_OK == sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)
              && SQLITE_DONE == sqlite3_step(stmt)))
        {
            dbErrorLog(qry);
            goto endgame;
        }
    }
    qry = string("commit transaction");
    if (SQLITE_OK == sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL)
        && SQLITE_DONE == sqlite3_step(stmt))
        ok = true;
    else
        dbErrorLog(qry);

endgame:
    sqlite3_finalize(stmt);
    return ok;
}


string ProgramBanks::readXmlFile(const string filename)
{
    string xmldata = string();
    gzFile gzf  = gzopen(filename.c_str(), "rb");
    if (gzf != NULL)
    {
        const int bufSize = 4096;
        char fetchBuf[4097];
        int this_read;
        int total_bytes = 0;
        stringstream readStream;
        for (bool quit = false; !quit;)
        {
            memset(fetchBuf, 0, sizeof(fetchBuf) * sizeof(char));
            this_read = gzread(gzf, fetchBuf, bufSize);
            if (this_read > 0)
            {
                readStream << fetchBuf;
                total_bytes += this_read;
            }
            else if (this_read < 0)
            {
                int errnum;
                Runtime.Log("Read error in zlib: " + string(gzerror(gzf, &errnum)));
                if (errnum == Z_ERRNO)
                    Runtime.Log("Filesystem error: " + string(strerror(errno)));
                quit = true;
            }
            else if (total_bytes > 0)
            {
                xmldata = readStream.str();
                quit = true;
            }
        }
        gzclose(gzf);
    }
    else
        Runtime.Log("Failed to open xml file " + filename + " for load, errno: "
                    + asString(errno) + "  " + string(strerror(errno)));
    return xmldata;
}


string ProgramBanks::dbQuoteSingles(string txt)
{
    string quoted = "'";
    for (unsigned int x = 0; x < txt.length(); ++x)
    {
        quoted.append(txt.substr(x, 1));
        if (txt.substr(x, 1) == "'")
            quoted.append("'");
    }
    quoted.append("'");
    return quoted;
}


string ProgramBanks::programXml(unsigned char bank, unsigned char prog)
{
    sqlite3_stmt *stmt = NULL;
    string xml;
    string qry = string("select xml from instrument where banknumber=")
                 + asString(bank) + string(" and prognumber=")
                 + asString(prog);
    if (SQLITE_OK != sqlite3_prepare_v2(dbConn, qry.c_str(), qry.size() + 1, &stmt, NULL))
        dbErrorLog(string("programName prep: ") + qry);
    else if (SQLITE_ROW == sqlite3_step(stmt))
        xml = string((const char*)sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return xml;
}

