/*
    MusicClient.h

    Copyright 2009-2011, Alan Calvert
    Copyright 2016-2019, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

    Modified May 2019
*/

#include "MusicIO/MusicClient.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/AlsaEngine.h"
#include "MusicIO/JackEngine.h"
#include <iostream>
#include <stdlib.h>
#include <set>
#include <unistd.h>

std::string audio_drivers_str [] = {"no_audio", "jack_audio"
#if defined(HAVE_ALSA)
                               , "alsa_audio"
#endif
                              };
std::string midi_drivers_str [] = {"no_midi", "jack_midi"
#if defined(HAVE_ALSA)
                              , "alsa_midi"
#endif
                             };

MusicClient *MusicClient::newMusicClient(SynthEngine *_synth)
{
    std::set<music_clients> clSet;
    music_clients c1 = {0, _synth->getRuntime().audioEngine, _synth->getRuntime().midiEngine};
    clSet.insert(c1);
    music_clients c2 = {1, jack_audio, jack_midi};
    clSet.insert(c2);
    music_clients c3 = {2, jack_audio, alsa_midi};
    clSet.insert(c3);
    music_clients c4 = {3, alsa_audio, alsa_midi};
    clSet.insert(c4);
    music_clients c5 = {4, jack_audio, no_midi};
    clSet.insert(c5);
    music_clients c6 = {5, alsa_audio, no_midi};
    clSet.insert(c6);
    music_clients c7 = {6, no_audio, no_midi}; //this one always will do the work :)
    clSet.insert(c7);

    for (std::set<music_clients>::iterator it = clSet.begin(); it != clSet.end(); ++it)
    {
        MusicClient *client = new MusicClient(_synth, it->audioDrv, it->midiDrv);
        if (client)
        {
            if (client->Open()) //found working client combination
            {
                if (it != clSet.begin())
                    _synth->getRuntime().configChanged = true;
                _synth->getRuntime().runSynth = true; //reset to true
                _synth->getRuntime().audioEngine = it->audioDrv;
                _synth->getRuntime().midiEngine = it->midiDrv;
                _synth->getRuntime().Log("Using " + audio_drivers_str [it->audioDrv] + " for audio and " + midi_drivers_str [it->midiDrv] + " for midi", 1);
                return client;
            }
            delete client;
        }
    }

    return 0;
}


void *MusicClient::timerThread_fn(void *arg)
{
    MusicClient *nmc = (MusicClient *)arg;
    useconds_t sleepInterval = (useconds_t)(1000000.0f * (double)nmc->synth->getRuntime().Buffersize / nmc->synth->getRuntime().Samplerate);//(double)NMC_SRATE);
    nmc->timerWorking = true;
    while (nmc->timerWorking)
    {
        nmc->synth->MasterAudio(nmc->buffersL, nmc->buffersR);
        usleep(sleepInterval);
    }
    return 0;
}


MusicClient::MusicClient(SynthEngine *_synth, audio_drivers _audioDrv, midi_drivers _midiDrv)
    :synth(_synth), timerThreadId(0), timerWorking(false),
    audioDrv(_audioDrv), midiDrv(_midiDrv), audioIO(0), midiIO(0)
{
    for (int i = 0; i < NUM_MIDI_PARTS + 1; i++)
    {
        buffersL [i] = new float [synth->getRuntime().Buffersize];
        if (buffersL [i] == 0)
        {
            abort();
        }
        buffersR [i] = new float [synth->getRuntime().Buffersize];
        if (buffersL [i] == 0)
        {
            abort();
        }
    }

    if (audioDrv == jack_audio && midiDrv == jack_midi)
        beatTracker = new SinglethreadedBeatTracker;
    else
        beatTracker = new MultithreadedBeatTracker;

    switch(audioDrv)
    {
        case jack_audio:
            audioIO = new JackEngine(synth, beatTracker);
            break;
#if defined(HAVE_ALSA)
        case alsa_audio:
            audioIO = new AlsaEngine(synth, beatTracker);
            break;
#endif

        default:
            break;
    }

    switch(midiDrv)
    {
        case jack_midi:
            if (audioDrv == jack_audio)
            {
                midiIO = audioIO;
            }
            else
                midiIO = new JackEngine(synth, beatTracker);
            break;
#if defined(HAVE_ALSA)
        case alsa_midi:
            if (audioDrv == alsa_audio)
            {
                midiIO = audioIO;
            }
            else
                midiIO = new AlsaEngine(synth, beatTracker);
            break;
#endif

        default:
            break;
    }

    if (audioDrv != no_audio)
    {
        if (!audioIO)
        {
            abort();
        }
    }
    if (midiDrv != no_midi)
    {
        if (!midiIO)
        {
            abort();
        }
    }
}


MusicClient::~MusicClient()
{
    Close();
    if (audioIO)
    {
        if (audioIO != midiIO)
            delete audioIO;
        audioIO = 0;
    }

    if (midiIO)
    {
        delete midiIO;
        midiIO = 0;
    }

    delete beatTracker;

    for (int i = 0; i < NUM_MIDI_PARTS + 1; i++)
    {
        delete [] buffersL [i];
        delete [] buffersR [i];
    }
}


bool MusicClient::Open()
{
    bool bAudio = true;
    bool bMidi = true;
    if (audioIO)
    {
        bAudio = audioIO->openAudio();
    }
    if (midiIO)
    {
        bMidi = midiIO->openMidi();
    }
    if (bAudio && bMidi)
    {
        synth->getRuntime().audioEngine = audioDrv;
        synth->getRuntime().midiEngine = midiDrv;
    }

    return bAudio && bMidi;
}


bool MusicClient::Start()
{
    bool bAudio = true;
    bool bMidi = true;

    if (audioIO)
    {
        bAudio = audioIO->Start();
    }
    else
    {
        if (timerThreadId != 0 || timerWorking)
        {
            return true;
        }
        bAudio = synth->getRuntime().startThread(&timerThreadId, MusicClient::timerThread_fn, this, false, 0, "Timer?");
    }

    if (midiIO && midiIO != audioIO)
    {
        bMidi = midiIO->Start();
    }
    return bAudio && bMidi;
}


void MusicClient::Close()
{
    if (midiIO && midiIO != audioIO)
    {
        midiIO->Close();
    }

    if (audioIO)
    {
        audioIO->Close();
    }
    else
    {
        if (timerThreadId == 0 || timerWorking == false)
            return;
        timerWorking = false;
        void *ret = 0;
        pthread_join(timerThreadId, &ret);
        timerThreadId = 0;
    }
}


unsigned int MusicClient::getSamplerate()
{
    if (audioIO)
    {
        return audioIO->getSamplerate();
    }
    return NMC_SRATE;
}


int MusicClient::getBuffersize()
{
    if (audioIO)
    {
        return audioIO->getBuffersize();
    }

    return synth->getRuntime().Buffersize;
}


std::string MusicClient::audioClientName()
{
    if (audioIO)
    {
        return audioIO->audioClientName();
    }

    return "null_audio";
}


std::string MusicClient::midiClientName()
{
    if (midiIO)
    {
        return midiIO->midiClientName();
    }
    return "null_midi";
}


int MusicClient::audioClientId()
{
    if (audioIO)
    {
        return audioIO->audioClientId();
    }
    return 0;

}


int MusicClient::midiClientId()
{
    if (midiIO)
    {
        return midiIO->midiClientId();
    }
    return 0;

}


void MusicClient::registerAudioPort(int portnum)
{
    if (audioIO)
    {
        audioIO->registerAudioPort(portnum);
    }
}
