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
    interleavedFloats(NULL),
    float32bit(true),
    recordFifo(string()),
    toFifo(NULL),
    fromFifoSndfle(NULL),
    tferSamples(0),
    tferBuf(NULL),
    wavFile(string()),
    wavOutsnd(NULL),
    pThread(0)
{
    setlocale( LC_TIME, "" ); // use compiler's native locale
}


WavRecord::~WavRecord()
{
    Stop();
    if (NULL != fromFifoSndfle)
        sf_close(fromFifoSndfle);
    if (NULL != wavOutsnd)
        sf_close(wavOutsnd);
    if (!recordFifo.empty() && isFifo(recordFifo))
        unlink(recordFifo.c_str());
    if (NULL != interleavedFloats)
        delete [] interleavedFloats;
    interleavedFloats = NULL;

}

bool WavRecord::Prep(unsigned int sample_rate, int buffer_size)
{
    samplerate = sample_rate;
    buffersize = buffer_size;
    int chk;
    char fifoname[] = { YOSHI_FIFO_DIR "/record.yoshimi.XXXXXX" };

    tferSamples = 512 / sizeof(float);
    tferBuf = new float[tferSamples];
    interleavedFloats = new float[sizeof(float) * buffersize * 2];
    if (NULL == tferBuf || NULL == interleavedFloats)
    {
        Runtime.Log("Failed to allocate WavRecord buffers");
        goto bail_out;
    }
    memset(interleavedFloats, 0,  sizeof(float) * buffersize * 2);
    if (string(YOSHI_FIFO_DIR).empty() || !isDirectory(string(YOSHI_FIFO_DIR)))
    {
        Runtime.Log("Invalid record fifo directory: " + string(YOSHI_FIFO_DIR));
        return false;
    }
    chk = mkstemp(fifoname);
    if (chk < 0)
    {
        Runtime.Log("Failed to create fifoname ");
        return false;
    }
    close(chk);
    unlink(fifoname);
    if ((chk = mkfifo(fifoname, S_IRUSR | S_IWRITE)))
    {
        Runtime.Log("Failed to create fifo: " + recordFifo + " - "
                    + string(strerror(chk)));
        return false;
    } 
    recordFifo = string(fifoname);

    pthread_attr_t attr;
    if ((chk = pthread_attr_init(&attr)))
    {
        Runtime.Log("Failed to initialise record thread attributes: " + asString(chk));
        goto bail_out;
    }
    if ((chk = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)))
    {
        Runtime.Log("Failed to set record thread detach state: " + asString(chk));
        goto bail_out;
    }

    chk = pthread_create(&pThread, &attr, _recorderThread, this);
    if (chk)
    {
        Runtime.Log("Failed to start record thread: " + asString(chk));
        goto bail_out;
    }
    if (NULL != toFifo)
        fclose(toFifo);
    toFifo = fopen(recordFifo.c_str(), "w");
    if (NULL == toFifo)
    {
        Runtime.Log("Error, failed to open toFifo: "+ string(strerror(errno)));
        goto bail_out;
    }
    if (setvbuf(toFifo, NULL, _IOFBF, 1024 * 256))
    {
        Runtime.Log("Error setting buffering on toFifo");
        goto bail_out;
    }
    return true;

bail_out:
    Close();
    if (NULL != tferBuf)
        delete [] tferBuf;
    tferBuf = NULL;
    if (!recordFifo.empty())
        unlink(recordFifo.c_str());
    recordFifo.clear();
    return false;
}


void WavRecord::Start(void)
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


void WavRecord::Stop(void)
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
    recordState = nada;
    if (NULL != wavOutsnd)
    {
        if (sf_close(wavOutsnd))
            Runtime.Log("Error closing wav file " + wavFile
                        + string(sf_strerror(wavOutsnd)));
        else
            recordLog("Close");
        wavOutsnd = NULL;
    }

    if (NULL != toFifo)
    {
        if (fclose(toFifo))
            Runtime.Log("Closing fifo feed: " + string(strerror(errno)));
        toFifo = NULL;
    }

    if (NULL != fromFifoSndfle)
    {
        if (sf_close(fromFifoSndfle))
            Runtime.Log("Error closing fifo read sndfle: "
                        + string(sf_strerror(fromFifoSndfle)));
        fromFifoSndfle = NULL;
    }

    if (!recordFifo.empty())
        unlink(recordFifo.c_str());
    else
        Runtime.Log("Ooops, recordFifo is empty at unlink time");
    recordFifo.clear();
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
    if (NULL != wavOutsnd)
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
    if (NULL != wavOutsnd)
    {
        sf_close(wavOutsnd);
        unlink(wavFile.c_str());
    }
    wavOutsnd = sf_open(wavFile.c_str(), SFM_WRITE, &wavOutInfo);
    if (NULL != wavOutsnd)
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
    size_t wrote = fwrite_unlocked(interleavedFloats, sizeof(float) * 2,
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
    if (NULL == tferBuf)
    {
        Runtime.Log("Record thread transfer buffer unavailable");
        pthread_exit(NULL);
    }

    fromFifoInfo.samplerate = samplerate;
    fromFifoInfo.channels = 2;
    fromFifoInfo.format = SF_FORMAT_RAW | SF_FORMAT_FLOAT;
    fromFifoSndfle = sf_open(recordFifo.c_str(), SFM_READ, &fromFifoInfo);
    if (NULL == fromFifoSndfle)
    {
        Runtime.Log("Error opening fifo for input: "
                    + string(sf_strerror(fromFifoSndfle)));
        pthread_exit(NULL);
    }
    sf_count_t samplesRead;
    sf_count_t wroteSamples;
    pthread_cleanup_push(NULL, NULL);
    while (Runtime.runSynth)
    {
        pthread_testcancel();
        samplesRead = sf_read_float(fromFifoSndfle, tferBuf, tferSamples);
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
                wroteSamples = sf_write_float(wavOutsnd, tferBuf, samplesRead);
                if (wroteSamples != samplesRead)
                    Runtime.Log("Dodgy write to wav file"
                                + asString((unsigned int)wroteSamples)
                                + " / " + asString((unsigned int)samplesRead) + " frames");
            }
        }
    }
    pthread_cleanup_pop(0);
    return NULL;
}


void WavRecord::recordLog(string tag)
{
    static char stamp[12];
    time_t ts = time(0);
    strftime(stamp, 12, "%H:%M:%S ", localtime(&ts));
    Runtime.Log(string(stamp) + tag + " " + wavFile);
}

