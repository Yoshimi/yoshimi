/*
  Config.h - Configuration file functions

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

#ifndef CONFIG_H
#define CONFIG_H

#include <cstring>

using namespace std;

#include "MusicIO/MusicClient.h"

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
            string        nameTag;

            string        LinuxALSAaudioDev;
            string        LinuxALSAmidiDev;
            string        LinuxJACKserver;
            int           BankUIAutoClose;
            int           GzipCompression;
            int           Interpolation;
            string        bankRootDirlist[MAX_BANK_ROOT_DIRS];
            string        currentBankDir;
            string        presetsDirlist[MAX_BANK_ROOT_DIRS];
            int           CheckPADsynth;
        } settings;

        void clearBankrootDirlist(void);
        void clearPresetsDirlist(void);
        void Save(void) { saveConfig(); };
        void Announce(void);
        void StartupReport(unsigned int samplerate, int buffersize);
        void Usage(void);

    private:
        void readConfig(void);
        void saveConfig(void);
        string getConfigFilename(bool for_save);
};

#endif
