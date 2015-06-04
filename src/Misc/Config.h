/*
  Config.h - Configuration file functions

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

#ifndef CONFIG_H
#define CONFIG_H

#include <string>

using namespace std;

#include "globals.h"
#include "MusicIO/MusicClient.h"

#define MAX_STRING_SIZE 4000
#define MAX_BANK_ROOT_DIRS 100



extern int thread_priority; // default 50;

extern bool autostart_jack; // default false

class Config; extern Config Runtime;

class Config
{
    public:
        Config();
        ~Config() { };

        struct {
            unsigned int  Samplerate;
            unsigned int  Buffersize;
            unsigned int  Oscilsize;
            bool          showGui;
            bool          verbose;
            int           UserInterfaceMode;
            int           VirKeybLayout;
            audio_drivers audioEngine;
            midi_drivers  midiEngine;
            string        audioDevice;
            string        midiDevice;

            string        LinuxALSAaudioDev;
            string        LinuxALSAmidiDev;
            string        LinuxJACKserver;
            int           BankUIAutoClose;
            int           GzipCompression;
            int           Interpolation;
            char         *bankRootDirList[MAX_BANK_ROOT_DIRS];
            char         *currentBankDir;
            char         *presetsDirList[MAX_BANK_ROOT_DIRS];
            int           CheckPADsynth;
        } settings;

        void clearbankrootdirlist(void);
        void clearpresetsdirlist(void);
        void Save(void) { saveConfig(); };
        void StartupReport(unsigned int samplerate, int buffersize);
        void Usage(void);

    private:
        void readConfig(void);
        void saveConfig(void);
        string getConfigFilename(bool for_save);
};

#endif
