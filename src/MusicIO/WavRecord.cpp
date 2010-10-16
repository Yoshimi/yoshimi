/*
    WavRecord.h

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

#include <iostream>

#include <errno.h>
#include <stdio_ext.h>
#include <sys/stat.h>
#include <clocale>
#include <ctime>
#include <cstdlib>
#include <sndfile.h>

#include "Misc/Config.h"
#include "MusicIO/MusicIO.h"
#include "MusicIO/WavRecord.h"

WavRecord::WavRecord() :
    recordState(nada),
    samplerate(0),
    buffersize(0),
    wavFile(string()),
    float32bit(true),
    wavOutsnd(NULL),
    pThread(0),
    threadRun(false),
    toFifo(NULL),
    fromFifoSndfle(NULL),
    tferSamples(512 / sizeof(float))
{ }


WavRecord::~WavRecord()
{
    if (fromFifoSndfle)
        sf_close(fromFifoSndfle);
    if (wavOutsnd)
        sf_close(wavOutsnd);
    if (!recordFifo.empty() && isFifo(recordFifo))
        unlink(recordFifo.c_str());
}


bool WavRecord::Start(unsigned int sample_rate, int buffer_size)
{
    samplerate = sample_rate;
    buffersize = buffer_size;
    if (!Runtime.startThread(&pThread, _recorderThread, this, false, false))
    {
        Runtime.Log("Failed to start record thread");
        return false;
    }
    return true;
}


void WavRecord::StartRecord(void)
{
    switch (recordState)
    {
        case ready:
            __fpurge(toFifo);
            recordState = recording;
            recordLog("Record start");
            break;

        case recording:
        case nada:
        default:
            break;
    }
}


void WavRecord::StopRecord(void)
{
    switch (recordState)
    {
        case recording:
            sf_command(wavOutsnd, SFC_UPDATE_HEADER_NOW, NULL, 0);
            sf_write_sync(wavOutsnd);
            __fpurge(toFifo);
            recordState = ready;
            recordLog("Record stop");
            break;

        case ready:
        case nada:
        default:
            break;
    }
}


void WavRecord::Close(void)
{
    threadRun = false;
    if (wavOutsnd)
    {
        if (sf_close(wavOutsnd))
            Runtime.Log("Error closing wav file " + wavFile
                        + string(sf_strerror(wavOutsnd)), false);
        else
            recordLog("Close");
        wavOutsnd = NULL;
    }

    if (toFifo)
    {
        if (fclose(toFifo))
            Runtime.Log("Closing fifo feed: " + string(strerror(errno)), false);
        toFifo = NULL;
    }

    if (fromFifoSndfle)
    {
        if (sf_close(fromFifoSndfle))
            Runtime.Log("Error closing fifo read sndfle: "
                        + string(sf_strerror(fromFifoSndfle)), false);
        fromFifoSndfle = NULL;
    }

    if (!recordFifo.empty() && Runtime.isFifo(recordFifo))
    {
        unlink(recordFifo.c_str());
        recordFifo.clear();
    }
}


bool WavRecord::SetFile(string fpath, string& errmsg)
{
    int chk;
    string xfile = string(fpath);
    if (xfile.empty())
    {
        errmsg = "Empty file path";
        return false;
    }
    if (wavOutsnd)
    {
        if (wavFile == xfile)
            return true;
        else
        {
            sf_close(wavOutsnd);
            wavOutsnd = NULL;
        }
    }
    wavFile = xfile;
    if (isRegFile(wavFile))
    {
        // ah, an existing wav file
        wavOutsnd = sf_open(wavFile.c_str(), SFM_RDWR, &wavOutInfo) ;
        if (NULL == wavOutsnd)
        {
            errmsg = "Error opening " + wavFile + ": "
                     + string(sf_strerror(wavOutsnd));
            goto bail_out;
        }
        if (!((chk = wavOutInfo.format) & SF_FORMAT_WAV))
        {
            errmsg = wavFile + " is not a wav format file";
            goto bail_out;
        }
        chk &= SF_FORMAT_SUBMASK;
        if (!(chk == SF_FORMAT_FLOAT || chk == SF_FORMAT_PCM_16))
        {
            errmsg = wavFile + " is an incompatible wav format";
            goto bail_out;
        }
        if (wavOutInfo.samplerate != (int)samplerate || wavOutInfo.channels != 2)
        {
            errmsg = wavFile + " has incompatible samplerate or channels,\n"
                + "Yoshimi setting " + asString(Runtime.Samplerate)
                + "/2 != wav file " + asString(wavOutInfo.samplerate) + "/"
                + asString(wavOutInfo.channels);
            goto bail_out;
        }
        if (sf_seek(wavOutsnd, 0, SFM_WRITE | SEEK_END) < 0)
        {
            errmsg = "Error seeking to end of " + wavFile + ": "
                     + string(sf_strerror(wavOutsnd));
            goto bail_out;
        }
        recordLog("Open existing");
    }
    else
    {
        wavOutInfo.samplerate = samplerate;
        wavOutInfo.channels = 2;
        wavOutInfo.format = SF_FORMAT_WAV;
        if (Runtime.Float32bitWavs)
        {
            wavOutInfo.format |= SF_FORMAT_FLOAT;
            float32bit = true;
        }
        else
        {
            wavOutInfo.format |= SF_FORMAT_PCM_16;
            float32bit = false;
        }
        wavOutsnd = sf_open(wavFile.c_str(), SFM_WRITE, &wavOutInfo);
        if (NULL == wavOutsnd)
        {
            errmsg = "Error opening new wav file " + wavFile
                     + " : " + string(sf_strerror(NULL));
            goto bail_out;
        }
        recordLog("Open new");
    }
    recordState = ready;
    return true;

bail_out:
    recordState = nada;
    wavFile.clear();
    if (NULL != wavOutsnd)
        sf_close(wavOutsnd);
    wavOutsnd = NULL;
    return false;
}


bool WavRecord::SetOverwrite(string& errmsg)
{
    if (recordState != ready)
        return false;
    if (wavOutsnd)
    {
        sf_close(wavOutsnd);
        unlink(wavFile.c_str());
    }
    wavOutsnd = sf_open(wavFile.c_str(), SFM_WRITE, &wavOutInfo);
    if (wavOutsnd)
    {
        recordLog("Overwrite");
        return true;
    }
    errmsg = "Error opening new wav file " + wavFile + " : " + string(sf_strerror(NULL));
    return false;
}


void WavRecord::Feed(float* samples_left, float *samples_right)
{
    int idx = 0;
    unsigned int bufferframes = buffersize;
    for (unsigned int frame = 0; frame < bufferframes; ++frame)
    {   // interleave floats
        interleavedFloats[idx++] = samples_left[frame];
        interleavedFloats[idx++] = samples_right[frame];
    }
    size_t wrote = fwrite_unlocked(interleavedFloats.get(), sizeof(float) * 2,
                                   bufferframes, toFifo);
    if (wrote != bufferframes)
        Runtime.Log("Short write in feedRecord, "
                    + asString((unsigned int)wrote) + " / " + asString(bufferframes));
}


void *WavRecord::_recorderThread(void *arg)
{
    return static_cast<WavRecord*>(arg)->recorderThread();
}


void *WavRecord::recorderThread(void)
{
    boost::shared_array<float> tferBuf = boost::shared_array<float>(new float[tferSamples]);
    interleavedFloats = boost::shared_array<float>(new float[sizeof(float) * buffersize * 2]);
    if (!tferBuf || !interleavedFloats)
    {
        Runtime.Log("Failed to allocate WavRecord buffers");
        pthread_exit(NULL);
    }
    memset(interleavedFloats.get(), 0,  sizeof(float) * buffersize * 2);
    if (!tferBuf)
    {
        Runtime.Log("Record thread transfer buffer unavailable");
        pthread_exit(NULL);
    }
    char fifoname[] = { YOSHI_FIFO_DIR "/record.yoshimi.XXXXXX" };
    int chk = mkstemp(fifoname);
    if (chk < 0)
    {
        Runtime.Log("Failed to create fifoname " + string(fifoname));
        return false;
    }
    close(chk);
    unlink(fifoname);
    recordFifo = string(fifoname);
    if ((chk = mkfifo(fifoname, S_IRUSR | S_IWRITE)) < 0)
    {
        Runtime.Log("Failed to create fifo: " + recordFifo + " - " + string(strerror(-chk)));
        pthread_exit(NULL);
    }
    if (NULL != toFifo)
        fclose(toFifo);
    toFifo = fopen(fifoname, "r+");
    if (!toFifo)
    {
        Runtime.Log("Error, failed to open toFifo: "+ string(strerror(errno)), true);
        pthread_exit(NULL);
    }
    if (setvbuf(toFifo, NULL, _IOFBF, 1024 * 256))
    {
        Runtime.Log("Error setting buffering on toFifo");
        pthread_exit(NULL);
    }
    fromFifoInfo.samplerate = samplerate;
    fromFifoInfo.channels = 2;
    fromFifoInfo.format = SF_FORMAT_RAW | SF_FORMAT_FLOAT;
    fromFifoSndfle = sf_open(fifoname, SFM_READ, &fromFifoInfo);
    if (!fromFifoSndfle)
    {
        Runtime.Log("Error opening fifo [" + string(fifoname) + "] for input: "
                    + string(sf_strerror(fromFifoSndfle)));
        pthread_exit(NULL);
    }
    sf_count_t samplesRead;
    sf_count_t wroteSamples;
    pthread_cleanup_push(NULL, NULL);
    while (Runtime.runSynth  && threadRun)
    {
        pthread_testcancel();
        samplesRead = sf_read_float(fromFifoSndfle, tferBuf.get(), tferSamples);
        if (recordState == recording)
        {
            if (tferSamples != samplesRead)
            {
                Runtime.Log("Dodgy read from recordFifo, read "
                            + asString((unsigned int)samplesRead) + " of "
                            + asString((unsigned int)tferSamples) + " frames");
            }
            pthread_testcancel();
            if (samplesRead > 0)
            {
                wroteSamples = sf_write_float(wavOutsnd, tferBuf.get(), samplesRead);
                if (wroteSamples != samplesRead)
                    Runtime.Log("Dodgy write to wav file"
                                + asString((unsigned int)wroteSamples)
                                + " / " + asString((unsigned int)samplesRead) + " frames");
            }
        }
    }
    recordState = nada;
    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}


void WavRecord::recordLog(string tag)
{
    static char stamp[12];
    time_t ts = time(0);
    strftime(stamp, 12, "%H:%M:%S ", localtime(&ts));
    Runtime.Log(string(stamp) + tag + " " + wavFile);
}

