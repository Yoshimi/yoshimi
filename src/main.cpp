/*
    main.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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

#include <string>
#include <iostream>
#include <getopt.h>

using namespace std;

#include "Misc/AntiDenormals.h"
#include "Misc/Util.h"
#include "Misc/Master.h"
#include "MusicIO/MusicClient.h"

#if !defined(DISABLE_GUI)
#   include "GuiThreadUI.h"
#endif

static int set_DAZ_and_FTZ(int on /*bool*/);

int main(int argc, char *argv[])   
{
    struct option opts[] = { // Command line options
        // no argument: 0, required: 1:, optional: 2::
        {"load",            1, NULL, 'l'},
        {"load-instrument", 1, NULL, 'L'},
        {"oscilsize",       1, NULL, 'o'},
        {"no-gui",          0, NULL, 'U'},
        {"alsa-midi",       2, NULL, 'a'},
        {"jack-midi",       2, NULL, 'j'},
        {"no-audio",        0, NULL, 'z'},
        {"jack-audio",      2, NULL, 'J'},
        {"autostart-jack",  0, NULL, 'k'},
        {"alsa-audio",      2, NULL, 'A'},
        {"quiet",           0, NULL, 'q'},
        {"help",            2, NULL, 'h'},
        { 0, 0, 0, 0 }
    };

    set_DAZ_and_FTZ(1);
    MusicClient *musicClient = NULL;
    int exit_status = 0;
    cerr.precision(3);
    string loadfile = string();
    string loadinstrument = string();
    int option_index = 0;
    int opt;
    bool exitwithhelp = false;
    opterr = 0;
    int tmp = 0;
    unsigned int utmp = 0;
    while (true)
    {
        opt = getopt_long(argc, argv, "a::A::j::J::l:L:o:hkqU", opts,
                          &option_index);
        char *optarguments = optarg;
        if (opt == -1)
            break;
        switch (opt)
        {
            case 'a':
                Runtime.settings.midiEngine = alsa_midi;
                if (optarguments != NULL)
                    Runtime.settings.midiDevice = string(optarguments);
                break;

            case 'A':
                Runtime.settings.audioEngine = alsa_audio;
                if (optarguments != NULL)
                    Runtime.settings.audioDevice = string(optarguments);
                Runtime.settings.audioDevice = (optarguments == NULL)
                               ? string(Runtime.settings.LinuxALSAaudioDev)
                               : string(optarguments);
                if (!Runtime.settings.audioDevice.size())
                    Runtime.settings.audioDevice = "default";
                break;

            case 'b':
                utmp = 0;
                if (optarguments != NULL)
                    utmp = atoi(optarguments);
                if (utmp >= 32 && utmp <= 8192)
                    Runtime.settings.Buffersize = tmp;
                else
                {
                    cerr << "Error, invalid buffer size specified: "
                            << optarguments << endl;
                    exit(1);
                }
                break;

            case 'h':
                exitwithhelp = true;
                break;

            case 'j':
                Runtime.settings.midiEngine = jack_midi;
                if (optarguments != NULL)
                    Runtime.settings.midiDevice = string(optarguments);
                break;

            case 'J':
                Runtime.settings.audioEngine = jack_audio;
                if (optarguments != NULL)
                    Runtime.settings.audioDevice = string(optarguments);
                else
                    Runtime.settings.audioDevice = string(Runtime.settings.LinuxJACKserver);
                if (!Runtime.settings.audioDevice.size())
                {
                    if (getenv("JACK_DEFAULT_SERVER"))
                        Runtime.settings.audioDevice = string(getenv("JACK_DEFAULT_SERVER"));
                    else
                        Runtime.settings.audioDevice = "default";
                }
                break;

            case 'k':
                if (getenv("JACK_NO_START_SERVER"))
                    cerr << "Note, jack autostart specified, but JACK_NO_START_SERVER is set"
                         << endl;
                else
                    autostart_jack = true;
                break;

            case 'l':
                if (optarguments != NULL)
                    loadfile = string(optarguments);
                break;

            case 'L':
                if (optarguments != NULL)
                    loadinstrument = string(optarguments);
                break;

            case 'o':
                {
                    utmp = 0;
                    if (optarguments != NULL)
                        utmp = atoi(optarguments);
                    unsigned int oscil_size = utmp;
                    if (oscil_size < MAX_AD_HARMONICS * 2)
                        oscil_size = MAX_AD_HARMONICS * 2;
                    oscil_size = (int)powf(2, ceil(logf(oscil_size - 1.0) / logf(2.0)));
                    if (utmp != oscil_size)
                    {
                        cerr << "Oscilsize parameter " << utmp
                             << " is wrong, too small or not power of 2\n";
                        cerr << "ie, 2^n.  Forcing it to " << oscil_size
                             << " instead" << endl;
                        Runtime.settings.Oscilsize = oscil_size;
                    }
                }
                break;

            case 'p':
                tmp = (optarguments != NULL) ? atoi(optarguments) : -1;
                if (!(tmp < 0 || tmp > 75))
                    thread_priority = tmp;
                else
                {
                    cerr << "Error, invalid thread priority specified: " << tmp << endl;
                    exit(1);
                }
                break;

            case 'r':
                tmp = 0;
                if (optarguments != NULL)
                    tmp = atoi(optarguments);
                if (tmp >= 32000 && tmp <= 128000)
                    Runtime.settings.Samplerate = tmp;
                else
                {
                    cerr << "Error, invalid samplerate specified: "
                         << optarguments << ", valid range 32000 -> 128000" << endl;
                    exit(1);
                }
                break;

            case 'U':
                Runtime.settings.showGui = false;
                break;

            case 'q':
                Runtime.settings.verbose = false;
                break;

            case '?':
                cerr << "Error, invalid command line option" << endl;
                exitwithhelp = true;
                break;
        }
    }
    if (Runtime.settings.audioEngine == no_audio
        && Runtime.settings.midiEngine == no_midi)
    {
        cerr << "Error, no audio & no midi engines nominated!" << endl;
        exitwithhelp = true;
    }
    if (exitwithhelp)
    {
        Runtime.Usage();
        exit(0);
    }
    srand(time(NULL));

    if (NULL == (zynMaster = new Master()))
    {
        cerr << "Error, failed to allocate Master" << endl;
        exit_status = 1;
        goto bail_out;
    }

    if (loadfile.size())
    {
        if (zynMaster->loadXML(loadfile.c_str()) < 0)
        {
            cerr << "Error, failed to load master file: " << loadfile << endl;
            exit(1);
        }
        else
        {
            zynMaster->applyparameters();
            if (Runtime.settings.verbose)
                cerr << "Master file " << loadfile << " loaded" << endl;
        }
    }
    if (loadinstrument.size())
    {
        int loadtopart = 0;
        if (zynMaster->part[loadtopart]->loadXMLinstrument(loadinstrument.c_str()) < 0)
        {
            cerr << "Error, failed to load instrument file: " << loadinstrument << endl;
            exit(1);
        }
        else
        {
            zynMaster->part[loadtopart]->applyparameters();
            if (Runtime.settings.verbose)
                cerr << "Instrument file " << loadinstrument << "loaded" << endl;
        }
    }

    if (NULL == (musicClient = MusicClient::newMusicClient()))
    {
        cerr << "Error, failed to instantiate MusicClient" << endl;
        exit_status = 1;
        goto bail_out;
    }

    if (!(musicClient->openAudio()))
    {
        cerr << "Error, failed to open MusicClient audio" << endl;
        exit_status = 1;
        goto bail_out;
    }

    if (!(musicClient->openMidi()))
    {
        cerr << "Error, failed to open MusicClient midi" << endl;
        exit_status = 1;
        goto bail_out;
    }

    OscilGen::tmpsmps = new float[Runtime.settings.Oscilsize];
    if (NULL == OscilGen::tmpsmps)
    {
        cerr << "Error, failed to allocate OscilGen::tmpsmps" << endl;
        exit_status = 1;
        goto bail_out;
    }
    memset(OscilGen::tmpsmps, 0, Runtime.settings.Oscilsize * sizeof(float));
    newFFTFREQS(&OscilGen::outoscilFFTfreqs, Runtime.settings.Oscilsize / 2);

    if (!zynMaster->Init(musicClient->getSamplerate(),
                         musicClient->getBuffersize(),
                         Runtime.settings.Oscilsize))
    {
        cerr << "Error, Master init failed" << endl;
        exit_status = 1;
        goto bail_out;
    }

#   if !defined(DISABLE_GUI)
        if (NULL == (guiMaster = new MasterUI()))
        {
            cerr << "Error, failed to instantiate guiMaster" << endl;
            goto bail_out;
        }
#   endif

#   if !defined(DISABLE_GUI)
        if (Runtime.settings.showGui)
            if (!startGuiThread())
            {
                cerr << "Error, failed to start gui thread" << endl;
                goto bail_out;
            }
#   endif

    if (musicClient->Start())
    {
        Runtime.StartupReport(musicClient->getSamplerate(), musicClient->getBuffersize());
        while (!Pexitprogram)
            usleep(33000); // where all the action is ...
        musicClient->Close();
#       if !defined(DISABLE_GUI)
            if (NULL != guiMaster)
            {
                stopGuiThread();
                delete guiMaster;
                guiMaster = NULL;
            }
#       endif
    }
    else
    {
        cerr << "So sad, failed to start MusicIO" << endl;
        exit_status = 1;
        goto bail_out;
    }
    set_DAZ_and_FTZ(0);
    return exit_status;

bail_out:
    cerr << "Yoshimi stages a strategic retreat :-(" << endl;

#   if !defined(DISABLE_GUI)
        if (NULL != guiMaster)
        {
            guiMaster->strategicRetreat();
            delete guiMaster;
        }
#   endif

    if (NULL != denormalkillbuf)
        delete [] denormalkillbuf;
    if (NULL != OscilGen::tmpsmps)
        delete [] OscilGen::tmpsmps;
    deleteFFTFREQS(&OscilGen::outoscilFFTfreqs);
    set_DAZ_and_FTZ(0);
    exit(exit_status);
}


// Denormal protection
// Reference <http://lists.linuxaudio.org/pipermail/linux-audio-dev/2009-August/024707.html>

#include <xmmintrin.h>

#define CPUID(f,ax,bx,cx,dx) __asm__ __volatile__ \
("cpuid": "=a" (ax), "=b" (bx), "=c" (cx), "=d" (dx) : "a" (f))

static int set_DAZ_and_FTZ(int on /*bool*/)
{
   int sse_level = 0;

   if(on)
   {
      unsigned long ax, bx, cx, dx;

      CPUID(0x00,ax,bx,cx,dx);
      CPUID(0x01,ax,bx,cx,dx);

      if (dx & 0x02000000)
      {
	 sse_level = 1;
	 // set FLUSH_TO_ZERO to ON and
	 // set round towards zero (RZ)
	 _mm_setcsr(_mm_getcsr() | 0x8000|0x6000);

	 if (dx & 0x04000000)
	 {
	    sse_level = 2;

	    if (cx & 0x00000001)
	    {
	       sse_level = 3;
	       // set DENORMALS_ARE_ZERO to ON
	       _mm_setcsr(_mm_getcsr() | 0x0040);
	    }
	    // we should have checked for AMD K8 without SSE3 here:
	    // if(AMD_K8_NO_SSE3)
            // .. but I can't recall how to that :-/
	 }
      }
   } else
      // clear underflow and precision flags
      // and set DAZ and FTZ to OFF
      // and restore round to nearest (RN)
      _mm_setcsr(_mm_getcsr() & ~(0x0030|0x8000|0x0040|0x6000));

   return sse_level;
}
#undef CPUID