/*
    PADnoteParameters.cpp - Parameters for PADnote (PADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019 Will Godfrey
    Copyright 2020 Kristian Amlie & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of a ZynAddSubFX original.

*/

#include <memory>

#include "Misc/XMLwrapper.h"
#include "DSP/FFTwrapper.h"
#include "Synth/OscilGen.h"
#include "Synth/Resonance.h"
#include "Params/EnvelopeParams.h"
#include "Params/LFOParams.h"
#include "Params/FilterParams.h"
#include "Misc/SynthEngine.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/NumericFuncs.h"
#include "Params/PADnoteParameters.h"
#include "Misc/WavFile.h"

using file::saveData;
using func::setAllPan;

PADnoteParameters::PADnoteParameters(FFTwrapper *fft_, SynthEngine *_synth) : Presets(_synth)
{
    setpresettype("Ppadsyth");
    fft = fft_;

    resonance = new Resonance(synth);
    POscil = new OscilParameters(synth);
    POscil->ADvsPAD = true;
    oscilgen = new OscilGen(fft_, resonance, synth, POscil);

    FreqEnvelope = new EnvelopeParams(0, 0, synth);
    FreqEnvelope->ASRinit(64, 50, 64, 60);
    FreqLfo = new LFOParams(70, 0, 64, 0, 0, 0, 0, 0, synth);

    AmpEnvelope = new EnvelopeParams(64, 1, synth);
    AmpEnvelope->ADSRinit_dB(0, 40, 127, 25);
    AmpLfo = new LFOParams(80, 0, 64, 0, 0, 0, 0, 1, synth);

    GlobalFilter = new FilterParams(2, 94, 40, 0, synth);
    FilterEnvelope = new EnvelopeParams(0, 1, synth);
    FilterEnvelope->ADSRinit_filter(64, 40, 64, 70, 60, 64);
    FilterLfo = new LFOParams(80, 0, 64, 0, 0, 0, 0, 2, synth);

    for (int i = 0; i < PAD_MAX_SAMPLES; ++i)
        sample[i].smp = NULL;
    newsample.smp = NULL;
    defaults();
}


PADnoteParameters::~PADnoteParameters()
{
    deletesamples();
    delete oscilgen;
    delete POscil;
    delete resonance;
    delete FreqEnvelope;
    delete FreqLfo;
    delete AmpEnvelope;
    delete AmpLfo;
    delete GlobalFilter;
    delete FilterEnvelope;
    delete FilterLfo;
}


void PADnoteParameters::defaults(void)
{
    Pmode = 0;
    Php.base.type = 0;
    Php.base.par1 = 80;
    Php.freqmult = 0;
    Php.modulator.par1 = 0;
    Php.modulator.freq = 30;
    Php.width = 127;
    Php.amp.type = 0;
    Php.amp.mode = 0;
    Php.amp.par1 = 80;
    Php.amp.par2 = 64;
    Php.autoscale = true;
    Php.onehalf = 0;

    setPbandwidth(500);
    Pbwscale = 0;

    resonance->defaults();
    oscilgen->defaults();

    Phrpos.type = 0;
    Phrpos.par1 = 64;
    Phrpos.par2 = 64;
    Phrpos.par3 = 0;

    Pquality.samplesize = 3;
    Pquality.basenote = 4;
    Pquality.oct = 3;
    Pquality.smpoct = 2;

    PStereo = 1; // stereo
    // Frequency Global Parameters
    Pfixedfreq = 0;
    PfixedfreqET = 0;
    PBendAdjust = 88; // 64 + 24
    POffsetHz = 64;
    PDetune = 8192; // zero
    PCoarseDetune = 0;
    PDetuneType = 1;
    FreqEnvelope->defaults();
    FreqLfo->defaults();

    // Amplitude Global Parameters
    PVolume = 90;
    setPan(PPanning = 64, synth->getRuntime().panLaw); // center
    PAmpVelocityScaleFunction = 64;
    PRandom = false;
    PWidth = 63;
    AmpEnvelope->defaults();
    AmpLfo->defaults();
    Fadein_adjustment = FADEIN_ADJUSTMENT_SCALE;
    PPunchStrength = 0;
    PPunchTime = 60;
    PPunchStretch = 64;
    PPunchVelocitySensing = 72;

    // Filter Global Parameters
    PFilterVelocityScale = 64;
    PFilterVelocityScaleFunction = 64;
    GlobalFilter->defaults();
    FilterEnvelope->defaults();
    FilterLfo->defaults();
    deletesamples();
    Papplied = false;
}


void PADnoteParameters::deletesample(int n)
{
    if (n < 0 || n >= PAD_MAX_SAMPLES)
        return;
    if (sample[n].smp != NULL)
    {
        delete [] sample[n].smp;
        sample[n].smp = NULL;
    }
    sample[n].size = 0;
    sample[n].basefreq = 440.0f;
}


void PADnoteParameters::deletesamples(void)
{
    for (int i = 0; i < PAD_MAX_SAMPLES; ++i)
        deletesample(i);
}


// Get the harmonic profile (i.e. the frequency distributio of a single harmonic)
float PADnoteParameters::getprofile(float *smp, int size)
{
    for (int i = 0; i < size; ++i)
        smp[i] = 0.0f;

    const int supersample = 16;
    float basepar = powf(2.0f, ((1.0f - Php.base.par1 / 127.0f) * 12.0f));
    float freqmult = floorf(powf(2.0f, (Php.freqmult / 127.0f * 5.0f)) + 0.000001f);

    float modfreq = floorf(powf(2.0f, (Php.modulator.freq / 127.0f * 5.0f)) + 0.000001f);
    float modpar1 = powf((Php.modulator.par1 / 127.0f), 4.0f) * 5.0 / sqrtf(modfreq);
    float amppar1 = powf(2.0f, powf((Php.amp.par1 / 127.0f), 2.0f) * 10.0f) - 0.999f;
    float amppar2 = (1.0f - Php.amp.par2 / 127.0f) * 0.998f + 0.001f;
    float width = powf((150.0f / (Php.width + 22.0f)), 2.0f);

    for (int i = 0; i < size * supersample; ++i)
    {
        bool makezero = false;
        float x = i * 1.0f / (size * (float)supersample);
        float origx = x;
        // do the sizing (width)
        x = (x - 0.5f) * width + 0.5f;
        if (x < 0.0f)
        {
            x = 0.0f;
            makezero = true;
        } else {
            if (x >1.0f)
            {
                x = 1.0f;
                makezero = true;
            }
        }
        // compute the full profile or one half
        switch (Php.onehalf)
        {
        case 1:
            x = x * 0.5f + 0.5f;
            break;

        case 2:
            x = x * 0.5f;
            break;
        }

        float x_before_freq_mult = x;
        // do the frequency multiplier
        x *= freqmult;

        // do the modulation of the profile
        x += sinf(x_before_freq_mult * PI * modfreq) * modpar1;

        x = fmodf(x + 1000.0f, 1.0f) * 2.0f - 1.0f;
        // this is the base function of the profile
        float f;
        switch (Php.base.type)
        {
        case 1:
            f = expf(-(x * x) * basepar);
            if (f < 0.4f)
                f = 0.0f;
            else
                f = 1.0f;
            break;

        case 2:
            f = expf(-(fabsf(x)) * sqrtf(basepar));
            break;

        default:
            f = expf(-(x * x) * basepar);
            break;
        }
        if (makezero)
            f = 0.0f;
        float amp = 1.0f;
        origx = origx * 2.0f - 1.0f;
        // compute the amplitude multiplier
        switch (Php.amp.type)
        {
        case 1:
            amp = expf(-(origx * origx) * 10.0f * amppar1);
            break;

        case 2:
            amp = 0.5f * (1.0f + cosf(PI * origx * sqrtf(amppar1 * 4.0f + 1.0f)));
            break;

        case 3:
            amp = 1.0f / (powf(origx * (amppar1 * 2.0f + 0.8f), 14.0f) + 1.0f);
            break;
        }
        // apply the amplitude multiplier
        float finalsmp = f;
        if (Php.amp.type != 0)
        {
            switch (Php.amp.mode)
            {
            case 0:
                finalsmp = amp * (1.0f - amppar2) + finalsmp * amppar2;
                break;
            case 1:
                finalsmp *= amp * (1.0f - amppar2) + amppar2;
                break;

            case 2:
                finalsmp =
                    finalsmp / (amp + powf(amppar2, 4.0f) * 20.0f + 0.0001f);
                break;

            case 3:
                finalsmp =
                    amp / (finalsmp + powf(amppar2, 4.0f) * 20.0f + 0.0001f);
                break;
            }
        }
        smp[i / supersample] += finalsmp / supersample;
    }
    // normalize the profile (make the max. to be equal to 1.0)
    float max = 0.0f;
    for (int i = 0; i < size; ++i)
    {
        if (smp[i] > max)
            max = smp[i];
    }
    if (max < 0.00001f)
        max = 1.0f;
    for (int i = 0; i < size; ++i)
        smp[i] /= max;

    if (!Php.autoscale)
        return 0.5f;
    // compute the estimated perceived bandwidth
    float sum = 0.0f;
    int i;
    for (i = 0; i < size / 2 - 2; ++i)
    {
        sum += smp[i] * smp[i] + smp[size - i -1] * smp[size - i - 1];
        if (sum >= 4.0f)
            break;
    }
    float result = 1.0f - 2.0f * i / (float)size;
    return result;
}


// Compute the real bandwidth in cents and returns it
// Also, sets the bandwidth parameter
float PADnoteParameters::setPbandwidth(int Pbandwidth)
{
    this->Pbandwidth = Pbandwidth;
    float result = powf(Pbandwidth / 1000.0f, 1.1f);
    result = powf(10.0f, result * 4.0f) * 0.25f;
    return result;
}


// Get the harmonic(overtone) position
float PADnoteParameters::getNhr(int n)
{
    float result = 1.0;
    float par1 = powf(10.0f, -(1.0f - Phrpos.par1 / 255.0f) * 3.0f);
    float par2 = Phrpos.par2 / 255.0f;

    float n0 = n - 1.0f;
    float tmp = 0.0f;
    int thresh = 0;
    switch (Phrpos.type)
    {
    case 1:
        thresh = int(par2 * par2 * 100.0f) + 1;
        if (n < thresh)
            result = n;
        else
            result = 1.0f + n0 + (n0 - thresh + 1.0f) * par1 * 8.0f;
        break;

    case 2:
        thresh = int(par2 * par2 * 100.0f) + 1;
        if (n < thresh)
            result = n;
        else
            result = 1.0f + n0 - (n0 - thresh + 1.0f) * par1 * 0.90f;
        break;

    case 3:
        tmp = par1 * 100.0f + 1.0f;
        result = powf(n0 / tmp, (1.0f - par2 * 0.8f)) * tmp + 1.0f;
        break;

    case 4:
        result = n0 * (1.0f - par1) + powf(n0 * 0.1f, par2 * 3.0f + 1.0f) * par1 * 10.0f + 1.0f;
        break;

    case 5:
        result = n0 + sinf(n0 * par2 * par2 * PI * 0.999f) * sqrtf(par1) * 2.0f + 1.0f;
        break;

    case 6:
        tmp = powf((par2 * 2.0f), 2.0f) + 0.1f;
        result = n0 * powf(1.0f + par1 * powf(n0 * 0.8f, tmp), tmp) + 1.0f;
        break;

        case 7:
            result = (n + Phrpos.par1 / 255.0f) / (Phrpos.par1 / 255.0f + 1);
            break;

    default:
        result=n;
        break;
    }
    float par3 = Phrpos.par3 / 255.0f;
    float iresult = floorf(result + 0.5f);
    float dresult = result - iresult;
    result = iresult + (1.0f - par3) * dresult;
    return result;
}


// Generates the long spectrum for Bandwidth mode (only amplitudes are generated;
// phases will be random)
void PADnoteParameters::generatespectrum_bandwidthMode(float *spectrum,
                                                       int size,
                                                       float basefreq,
                                                       float *profile,
                                                       int profilesize,
                                                       float bwadjust)
{
    memset(spectrum, 0, sizeof(float) * size);

    //float harmonics[synth->getOscilsize() / 2];
    float harmonics[synth->halfoscilsize];
    memset(harmonics, 0, synth->halfoscilsize * sizeof(float));

    // get the harmonic structure from the oscillator (I am using the frequency amplitudes, only)
    oscilgen->get(harmonics, basefreq, false);

    // normalize
    float max = 0.0f;
    for (int i = 0; i < synth->halfoscilsize; ++i)
        if (harmonics[i] > max)
            max = harmonics[i];
    if (max < 0.000001f)
        max = 1;
    for (int i = 0; i < synth->halfoscilsize; ++i)
        harmonics[i] /= max;
    for (int nh = 1; nh < synth->halfoscilsize; ++nh)
    {   //for each harmonic
        float realfreq = getNhr(nh) * basefreq;
        if (realfreq > synth->samplerate_f * 0.49999f)
            break;
        if (realfreq < 20.0f)
            break;
        if (harmonics[nh - 1] < 1e-4f)
            continue;
        //compute the bandwidth of each harmonic
        float bandwidthcents = setPbandwidth(Pbandwidth);
        float bw = (powf(2.0f, bandwidthcents / 1200.0f) - 1.0f) * basefreq / bwadjust;
        float power = 1.0f;
        switch (Pbwscale)
        {
        case 0:
            power = 1.0f;
            break;

        case 1:
            power = 0.0f;
            break;

        case 2:
            power = 0.25f;
            break;

        case 3:
            power = 0.5f;
            break;

        case 4:
            power = 0.75f;
            break;

        case 5:
            power = 1.5f;
            break;

        case 6:
            power = 2.0f;
            break;

        case 7:
            power = -0.5;
            break;
        }
        bw = bw * powf(realfreq / basefreq, power);
        int ibw = int((bw / (synth->samplerate_f * 0.5f) * size)) + 1;
        float amp = harmonics[nh - 1];
        if (resonance->Penabled)
            amp *= resonance->getfreqresponse(realfreq);
        if (ibw > profilesize)
        {   // if the bandwidth is larger than the profilesize
            float rap = sqrtf((float)profilesize / (float)ibw);
            int cfreq = int(realfreq / (synth->halfsamplerate_f) * size) - ibw / 2;
            for (int i = 0; i < ibw; ++i)
            {
                int src = int(i * rap * rap);
                int spfreq = i + cfreq;
                if (spfreq < 0)
                    continue;
                if (spfreq >= size)
                    break;
                spectrum[spfreq] += amp * profile[src] * rap;
            }
        }
        else
        {   // if the bandwidth is smaller than the profilesize
            float rap = sqrtf((float)ibw / (float)profilesize);
            float ibasefreq = realfreq / (synth->halfsamplerate_f) * size;
            for (int i = 0; i < profilesize; ++i)
            {
                float idfreq = i / (float)profilesize - 0.5f;
                idfreq *= ibw;
                int spfreq = (int)(idfreq + ibasefreq);
                float fspfreq = fmodf(idfreq + ibasefreq, 1.0f);
                if (spfreq <= 0)
                    continue;
                if (spfreq >= size - 1)
                    break;
                spectrum[spfreq] += amp * profile[i] * rap * (1.0f - fspfreq);
                spectrum[spfreq + 1] += amp * profile[i] * rap * fspfreq;
            }
        }
    }
}


// Generates the long spectrum for non-Bandwidth modes (only amplitudes are generated; phases will be random)
void PADnoteParameters::generatespectrum_otherModes(float *spectrum,
                                                    int size,
                                                    float basefreq)
{
    memset(spectrum, 0, sizeof(float) * size);

    float harmonics[synth->halfoscilsize];
    memset(harmonics, 0, synth->halfoscilsize * sizeof(float));

    // get the harmonic structure from the oscillator (I am using the frequency
    // amplitudes, only)
    oscilgen->get(harmonics, basefreq, false);

    // normalize
    float max = 0.0f;
    for (int i = 0; i < synth->halfoscilsize; ++i)
        if (harmonics[i] > max)
            max = harmonics[i];
    if (max < 0.000001f)
        max = 1;
    for (int i = 0; i < synth->halfoscilsize; ++i)
        harmonics[i] /= max;

    for (int nh = 1; nh < synth->halfoscilsize; ++nh)
    {   //for each harmonic
        float realfreq = getNhr(nh) * basefreq;

        ///sa fac aici interpolarea si sa am grija daca frecv descresc

        if (realfreq > synth->samplerate_f * 0.49999f)
            break;
        if (realfreq < 20.0f)
            break;
//	if (harmonics[nh-1]<1e-4) continue;

        float amp = harmonics[nh - 1];
        if (resonance->Penabled)
            amp *= resonance->getfreqresponse(realfreq);
        int cfreq = int(realfreq / (synth->halfsamplerate_f) * size);
        spectrum[cfreq] = amp + 1e-9f;
    }

    if (Pmode != 1)
    {
        int old = 0;
        for (int k = 1; k < size; ++k)
        {
            if ((spectrum[k] > 1e-10f) || (k == (size - 1)))
            {
                int delta = k - old;
                float val1 = spectrum[old];
                float val2 = spectrum[k];
                float idelta = 1.0f / delta;
                for (int i = 0; i < delta; ++i)
                {
                    float x = idelta * i;
                    spectrum[old+i] = val1 * (1.0f - x) + val2 * x;
                }
                old = k;
            }
        }
    }
}


// Applies the parameters (i.e. computes all the samples, based on parameters);
void PADnoteParameters::applyparameters()
{
    const int samplesize = (((int)1) << (Pquality.samplesize + 14));
    int spectrumsize = samplesize / 2;
    // spectrumsize can be quite large (up to 2MiB) and this is not a hot
    // function, so allocate this on the heap
    std::unique_ptr<float[]> spectrum(new float[spectrumsize]);
    int profilesize = 512;
    float profile[profilesize];

    float bwadjust = getprofile(profile, profilesize);
//    for (int i=0;i<profilesize;i++) profile[i]*=profile[i];
    float basefreq = 65.406f * powf(2.0f, Pquality.basenote / 2);
    if (Pquality.basenote %2 == 1)
        basefreq *= 1.5;

    int samplemax = Pquality.oct + 1;
    int smpoct = Pquality.smpoct;
    if (Pquality.smpoct == 5)
        smpoct = 6;
    if (Pquality.smpoct == 6)
        smpoct = 12;
    if (smpoct != 0)
        samplemax *= smpoct;
    else
        samplemax = samplemax / 2 + 1;
    if (samplemax == 0)
        samplemax = 1;

    // prepare a BIG FFT stuff
    FFTwrapper fft = FFTwrapper(samplesize);
    FFTFREQS fftfreqs;
    FFTwrapper::newFFTFREQS(&fftfreqs, samplesize / 2);

    float adj[samplemax]; // this is used to compute frequency relation to the base frequency
    for (int nsample = 0; nsample < samplemax; ++nsample)
        adj[nsample] = (Pquality.oct + 1.0f) * (float)nsample / samplemax;
    for (int nsample = 0; nsample < samplemax; ++nsample)
    {
        float tmp = adj[nsample] - adj[samplemax - 1] * 0.5f;
        float basefreqadjust = powf(2.0f, tmp);

        if (Pmode == 0)
            generatespectrum_bandwidthMode(&spectrum[0], spectrumsize,
                                           basefreq * basefreqadjust, profile,
                                           profilesize, bwadjust);
        else
            generatespectrum_otherModes(&spectrum[0], spectrumsize,
                                        basefreq * basefreqadjust);

        const int extra_samples = 5; // the last samples contains the first
                                     // samples (used for linear/cubic interpolation)
        newsample.smp = new float[samplesize + extra_samples];

        newsample.smp[0] = 0.0;
        for (int i = 1; i < spectrumsize; ++i)
        {   // randomize the phases
            float phase = synth->numRandom() * 6.29f;
            fftfreqs.c[i] = spectrum[i] * cosf(phase);
            fftfreqs.s[i] = spectrum[i] * sinf(phase);
        }
        fft.freqs2smps(&fftfreqs, newsample.smp);
        // that's all; here is the only ifft for the whole sample; no windows are used ;-)

        // normalize(rms)
        float rms = 0.0;
        for (int i = 0; i < samplesize; ++i)
            rms += newsample.smp[i] * newsample.smp[i];
        rms = sqrtf(rms);
        if (rms < 0.000001)
            rms = 1.0;
        rms *= sqrtf(float(1024 * 256) / samplesize);
        for (int i = 0; i < samplesize; ++i)
            newsample.smp[i] *= 1.0f / rms * 50.0f;

        // prepare extra samples used by the linear or cubic interpolation
        for (int i = 0; i < extra_samples; ++i)
            newsample.smp[i + samplesize] = newsample.smp[i];

        // replace the current sample with the new computed sample
        deletesample(nsample);
        sample[nsample].smp = newsample.smp;
        sample[nsample].size = samplesize;
        sample[nsample].basefreq = basefreq * basefreqadjust;
        newsample.smp = NULL;
    }
    FFTwrapper::deleteFFTFREQS(&fftfreqs);

    // delete the additional samples that might exists and are not useful
    for (int i = samplemax; i < PAD_MAX_SAMPLES; ++i)
        deletesample(i);
    Papplied = true;
}


void PADnoteParameters::setPan(char pan, unsigned char panLaw)
{
    PPanning = pan;
    if (!PRandom)
        setAllPan(PPanning, pangainL, pangainR, panLaw);
    else
        pangainL = pangainR = 0.7f;
}


bool PADnoteParameters::export2wav(std::string basefilename)
{
    string type;
    if (synth->getRuntime().isLittleEndian)
        type = "RIFF"; // default wave format
    else
        type = "RIFX";

    basefilename += "--sample-";
    bool isOK = true;
    for (int k = 0; k < PAD_MAX_SAMPLES; ++k)
    {
        if (sample[k].smp == NULL)
            continue;
        char tmpstr[20];
        snprintf(tmpstr, 20, "-%02d", k + 1);
        string filename = basefilename + string(tmpstr) + EXTEN::MSwave;
        int nsmps = sample[k].size;
        unsigned int block;
        unsigned short int sBlock;
        unsigned int buffSize = 44 + sizeof(short int) * nsmps; // total size
        char *buffer = (char*) malloc (buffSize);
        strcpy(buffer, type.c_str());
        block = nsmps * 4 + 36; // 2 channel shorts + part header
        buffer[4] = block & 0xff;
        buffer[5] = (block >> 8) & 0xff;
        buffer[6] = (block >> 16) & 0xff;
        buffer[7] = (block >> 24) & 0xff;
        string temp = "WAVEfmt ";
        strcpy(buffer + 8, temp.c_str());
        block = 16; // subchunk size
        memcpy(buffer + 16, &block, 4);
        sBlock = 1; // AudioFormat uncompressed
        memcpy(buffer + 20, &sBlock, 2);
        sBlock = 1; // NumChannels mono
        memcpy(buffer + 22, &sBlock, 2);
        block = synth->samplerate;
        memcpy(buffer + 24, &block, 4);
        block = synth->samplerate * 2; // ByteRate
                // (SampleRate * NumChannels * BitsPerSample) / 8
        memcpy(buffer + 28, &block, 4);
        sBlock = 2; // BlockAlign
                // (bitsPerSample * channels) / 8
        memcpy(buffer + 32, &sBlock, 2);
        sBlock = 16; // BitsPerSample
        memcpy(buffer + 34, &sBlock, 2);
        temp = "data";
        strcpy(buffer + 36, temp.c_str());
        block = nsmps * 2; // data size
        memcpy(buffer + 40, &block, 4);
        for (int i = 0; i < nsmps; ++i)
        {
            sBlock = (sample[k].smp[i] * 32767.0f);
            buffer [44 + i * 2] = sBlock & 0xff;
            buffer [45 + i * 2] = (sBlock >> 8) & 0xff;
        }
        /*
         * The file manager can return a negative number on error,
         * so the comparison in the line below must be as integers.
         * This is safe here as the maximum possible buffer size
         * is well below the size of an integer.
         */
        isOK = (saveData(buffer, buffSize, filename) == int(buffSize));
        free (buffer);

    }
    return isOK;
}


void PADnoteParameters::add2XML(XMLwrapper *xml)
{
    // currently not used
    // bool yoshiFormat = synth->usingYoshiType;
    xml->information.PADsynth_used = 1;

    xml->addparbool("stereo", PStereo);
    xml->addpar("mode",Pmode);
    xml->addpar("bandwidth",Pbandwidth);
    xml->addpar("bandwidth_scale",Pbwscale);

    xml->beginbranch("HARMONIC_PROFILE");
        xml->addpar("base_type",Php.base.type);
        xml->addpar("base_par1",Php.base.par1);
        xml->addpar("frequency_multiplier",Php.freqmult);
        xml->addpar("modulator_par1",Php.modulator.par1);
        xml->addpar("modulator_frequency",Php.modulator.freq);
        xml->addpar("width",Php.width);
        xml->addpar("amplitude_multiplier_type",Php.amp.type);
        xml->addpar("amplitude_multiplier_mode",Php.amp.mode);
        xml->addpar("amplitude_multiplier_par1",Php.amp.par1);
        xml->addpar("amplitude_multiplier_par2",Php.amp.par2);
        xml->addparbool("autoscale",Php.autoscale);
        xml->addpar("one_half",Php.onehalf);
    xml->endbranch();

    xml->beginbranch("OSCIL");
        POscil->add2XML(xml);
    xml->endbranch();

    xml->beginbranch("RESONANCE");
        resonance->add2XML(xml);
    xml->endbranch();

    xml->beginbranch("HARMONIC_POSITION");
        xml->addpar("type",Phrpos.type);
        xml->addpar("parameter1",Phrpos.par1);
        xml->addpar("parameter2",Phrpos.par2);
        xml->addpar("parameter3",Phrpos.par3);
    xml->endbranch();

    xml->beginbranch("SAMPLE_QUALITY");
        xml->addpar("samplesize",Pquality.samplesize);
        xml->addpar("basenote",Pquality.basenote);
        xml->addpar("octaves",Pquality.oct);
        xml->addpar("samples_per_octave",Pquality.smpoct);
    xml->endbranch();

    xml->beginbranch("AMPLITUDE_PARAMETERS");
        xml->addpar("volume",PVolume);
        // new yoshi type
        xml->addpar("pan_pos", PPanning);
        xml->addparbool("random_pan", PRandom);
        xml->addpar("random_width", PWidth);

        // legacy
        if (PRandom)
            xml->addpar("panning", 0);
        else
            xml->addpar("panning",PPanning);

        xml->addpar("velocity_sensing",PAmpVelocityScaleFunction);
        xml->addpar("fadein_adjustment", Fadein_adjustment);
        xml->addpar("punch_strength",PPunchStrength);
        xml->addpar("punch_time",PPunchTime);
        xml->addpar("punch_stretch",PPunchStretch);
        xml->addpar("punch_velocity_sensing",PPunchVelocitySensing);

        xml->beginbranch("AMPLITUDE_ENVELOPE");
            AmpEnvelope->add2XML(xml);
        xml->endbranch();

        xml->beginbranch("AMPLITUDE_LFO");
            AmpLfo->add2XML(xml);
        xml->endbranch();
    xml->endbranch();

    xml->beginbranch("FREQUENCY_PARAMETERS");
        xml->addpar("fixed_freq",Pfixedfreq);
        xml->addpar("fixed_freq_et",PfixedfreqET);
        xml->addpar("bend_adjust", PBendAdjust);
        xml->addpar("offset_hz", POffsetHz);
        xml->addpar("detune",PDetune);
        xml->addpar("coarse_detune",PCoarseDetune);
        xml->addpar("detune_type",PDetuneType);

        xml->beginbranch("FREQUENCY_ENVELOPE");
            FreqEnvelope->add2XML(xml);
        xml->endbranch();

        xml->beginbranch("FREQUENCY_LFO");
            FreqLfo->add2XML(xml);
        xml->endbranch();
    xml->endbranch();

    xml->beginbranch("FILTER_PARAMETERS");
        xml->addpar("velocity_sensing_amplitude",PFilterVelocityScale);
        xml->addpar("velocity_sensing",PFilterVelocityScaleFunction);

        xml->beginbranch("FILTER");
            GlobalFilter->add2XML(xml);
        xml->endbranch();

        xml->beginbranch("FILTER_ENVELOPE");
            FilterEnvelope->add2XML(xml);
        xml->endbranch();

        xml->beginbranch("FILTER_LFO");
            FilterLfo->add2XML(xml);
        xml->endbranch();
    xml->endbranch();
}

void PADnoteParameters::getfromXML(XMLwrapper *xml)
{
    PStereo=xml->getparbool("stereo",PStereo);
    Pmode=xml->getpar127("mode",0);
    Pbandwidth=xml->getpar("bandwidth",Pbandwidth,0,1000);
    Pbwscale=xml->getpar127("bandwidth_scale",Pbwscale);

    if (xml->enterbranch("HARMONIC_PROFILE"))
    {
        Php.base.type=xml->getpar127("base_type",Php.base.type);
        Php.base.par1=xml->getpar127("base_par1",Php.base.par1);
        Php.freqmult=xml->getpar127("frequency_multiplier",Php.freqmult);
        Php.modulator.par1=xml->getpar127("modulator_par1",Php.modulator.par1);
        Php.modulator.freq=xml->getpar127("modulator_frequency",Php.modulator.freq);
        Php.width=xml->getpar127("width",Php.width);
        Php.amp.type=xml->getpar127("amplitude_multiplier_type",Php.amp.type);
        Php.amp.mode=xml->getpar127("amplitude_multiplier_mode",Php.amp.mode);
        Php.amp.par1=xml->getpar127("amplitude_multiplier_par1",Php.amp.par1);
        Php.amp.par2=xml->getpar127("amplitude_multiplier_par2",Php.amp.par2);
        Php.autoscale=xml->getparbool("autoscale",Php.autoscale);
        Php.onehalf=xml->getpar127("one_half",Php.onehalf);
        xml->exitbranch();
    }

    if (xml->enterbranch("OSCIL"))
    {
        POscil->getfromXML(xml);
        xml->exitbranch();
    }

    if (xml->enterbranch("RESONANCE"))
    {
        resonance->getfromXML(xml);
        xml->exitbranch();
    }

    if (xml->enterbranch("HARMONIC_POSITION"))
    {
        Phrpos.type=xml->getpar127("type",Phrpos.type);
        Phrpos.par1=xml->getpar("parameter1",Phrpos.par1,0,255);
        Phrpos.par2=xml->getpar("parameter2",Phrpos.par2,0,255);
        Phrpos.par3=xml->getpar("parameter3",Phrpos.par3,0,255);
        xml->exitbranch();
    }

    if (xml->enterbranch("SAMPLE_QUALITY"))
    {
        Pquality.samplesize=xml->getpar127("samplesize",Pquality.samplesize);
        Pquality.basenote=xml->getpar127("basenote",Pquality.basenote);
        Pquality.oct=xml->getpar127("octaves",Pquality.oct);
        Pquality.smpoct=xml->getpar127("samples_per_octave",Pquality.smpoct);
        xml->exitbranch();
    }

    if (xml->enterbranch("AMPLITUDE_PARAMETERS"))
    {
        PVolume=xml->getpar127("volume",PVolume);
        int test = xml->getpar127("random_width", UNUSED);
        if (test < 64) // new Yoshi type
        {
            PWidth = test;
            setPan(xml->getpar127("pan_pos",PPanning), synth->getRuntime().panLaw);
            PRandom = xml->getparbool("random_pan", PRandom);
        }
        else // legacy
        {
            setPan(xml->getpar127("panning",PPanning), synth->getRuntime().panLaw);
            if (PPanning == 0)
            {
                PPanning = 64;
                PRandom = true;
                PWidth = 63;
            }
        }

        PAmpVelocityScaleFunction=xml->getpar127("velocity_sensing",PAmpVelocityScaleFunction);
        Fadein_adjustment = xml->getpar127("fadein_adjustment", Fadein_adjustment);
        PPunchStrength=xml->getpar127("punch_strength",PPunchStrength);
        PPunchTime=xml->getpar127("punch_time",PPunchTime);
        PPunchStretch=xml->getpar127("punch_stretch",PPunchStretch);
        PPunchVelocitySensing=xml->getpar127("punch_velocity_sensing",PPunchVelocitySensing);

        xml->enterbranch("AMPLITUDE_ENVELOPE");
            AmpEnvelope->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("AMPLITUDE_LFO");
            AmpLfo->getfromXML(xml);
        xml->exitbranch();

        xml->exitbranch();
    }

    if (xml->enterbranch("FREQUENCY_PARAMETERS"))
    {
        Pfixedfreq=xml->getpar127("fixed_freq",Pfixedfreq);
        PfixedfreqET=xml->getpar127("fixed_freq_et",PfixedfreqET);
        PBendAdjust=xml->getpar127("bend_adjust", PBendAdjust);
        POffsetHz =xml->getpar127("offset_hz", POffsetHz);
        PDetune=xml->getpar("detune",PDetune,0,16383);
        PCoarseDetune=xml->getpar("coarse_detune",PCoarseDetune,0,16383);
        PDetuneType=xml->getpar127("detune_type",PDetuneType);

        xml->enterbranch("FREQUENCY_ENVELOPE");
            FreqEnvelope->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("FREQUENCY_LFO");
            FreqLfo->getfromXML(xml);
        xml->exitbranch();

        xml->exitbranch();
    }

    if (xml->enterbranch("FILTER_PARAMETERS"))
    {
        PFilterVelocityScale=xml->getpar127("velocity_sensing_amplitude",PFilterVelocityScale);
        PFilterVelocityScaleFunction=xml->getpar127("velocity_sensing",PFilterVelocityScaleFunction);

        xml->enterbranch("FILTER");
            GlobalFilter->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("FILTER_ENVELOPE");
            FilterEnvelope->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("FILTER_LFO");
            FilterLfo->getfromXML(xml);
        xml->exitbranch();

        xml->exitbranch();
    }
    applyparameters();
}


float PADnoteParameters::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    unsigned char type = 0;

    // padnote defaults
    int min = 0;
    int def = 64;
    int max = 127;
    type |= TOPLEVEL::type::Integer;
    unsigned char learnable = TOPLEVEL::type::Learnable;
    switch (control)
    {
        case PADSYNTH::control::volume:
            type |= learnable;
            def = 90;
            break;

        case PADSYNTH::control::velocitySense:
            type |= learnable;
            def = 72;
            break;

        case PADSYNTH::control::panning:
            type |= learnable;
            break;

        case PADSYNTH::control::enableRandomPan:
            max = 1;
            break;

        case PADSYNTH::control::randomWidth:
            def = 63;
            max = 63;
            break;

        case PADSYNTH::control::bandwidth:
            def = 500;
            max = 1000;
            break;

        case PADSYNTH::control::bandwidthScale:
            def = 0;
            max = 7;
            break;

        case PADSYNTH::control::spectrumMode:
            def = 0;
            max = 2;
            break;

        case PADSYNTH::control::detuneFrequency:
            type |= learnable;
            min = -8192;
            def = 0;
            max = 8191;
            break;

        case PADSYNTH::control::equalTemperVariation:
            type |= learnable;
            def = 0;
            break;

        case PADSYNTH::control::baseFrequencyAs440Hz:
            def = 0;
            max = 1;
            break;

        case PADSYNTH::control::octave:
            type |= learnable;
            min = -8;
            def = 0;
            max = 7;
            break;

        case PADSYNTH::control::detuneType:
            def = 1;
            max = 4;
            break;

        case PADSYNTH::control::coarseDetune:
            min = -64;
            def = 0;
            max = 63;
            break;

        case PADSYNTH::control::pitchBendAdjustment:
            type |= learnable;
            def = 88;
            break;

        case PADSYNTH::control::pitchBendOffset:
            type |= learnable;
            break;


        case PADSYNTH::control::overtoneParameter1:
            type |= learnable;
            max = 255;
            break;

        case PADSYNTH::control::overtoneParameter2:
            type |= learnable;
            max = 255;
            break;

        case PADSYNTH::control::overtoneForceHarmonics:
            type |= learnable;
            def = 0;
            max = 255;
            break;

        case PADSYNTH::control::overtonePosition:
            def = 0;
            max = 7;
            break;

        case PADSYNTH::control::baseWidth:
            type |= learnable;
            def = 80;
            break;

        case PADSYNTH::control::frequencyMultiplier:
            type |= learnable;
            def = 0;
            break;

        case PADSYNTH::control::modulatorStretch:
            type |= learnable;
            def = 0;
            break;

        case PADSYNTH::control::modulatorFrequency:
            type |= learnable;
            def = 30;
            break;

        case PADSYNTH::control::size:
            type |= learnable;
            def = 127;
            break;

        case PADSYNTH::control::baseType:
            def = 0;
            max = 2;
            break;

        case PADSYNTH::control::harmonicSidebands:
            def = 0;
            max = 2;
            break;

        case PADSYNTH::control::spectralWidth:
            type |= learnable;
            def = 80;
            break;

        case PADSYNTH::control::spectralAmplitude:
            type |= learnable;
            break;

        case PADSYNTH::control::amplitudeMultiplier:
            def = 0;
            max = 3;
            break;

        case PADSYNTH::control::amplitudeMode:
            def = 0;
            max = 3;
            break;

        case PADSYNTH::control::autoscale:
            def = 1;
            max = 1;
            break;

        case PADSYNTH::control::harmonicBase:
            def = 4;
            max = 8;
            break;

        case PADSYNTH::control::samplesPerOctave:
            def = 2;
            max = 6;
            break;

        case PADSYNTH::control::numberOfOctaves:
            def = 3;
            max = 7;
            break;

        case PADSYNTH::control::sampleSize:
            def = 3;
            max = 6;
            break;

        case PADSYNTH::control::applyChanges:
            min = 0;
            def = 0;
            max = 0;
            break;

        case PADSYNTH::control::stereo:
            type |= learnable;
            def = 1;
            max = 1;
            break;

        case PADSYNTH::control::dePop:
            type |= learnable;
            def = FADEIN_ADJUSTMENT_SCALE;
            break;

        case PADSYNTH::control::punchStrength:
            type |= learnable;
            def = 0;
            break;

        case PADSYNTH::control::punchDuration:
            type |= learnable;
            def = 60;
            break;

        case PADSYNTH::control::punchStretch:
            type |= learnable;
            break;

        case PADSYNTH::control::punchVelocity:
            type |= learnable;
            def = 72;
            break;

        default:
            type |= TOPLEVEL::type::Error; // error
            break;
    }
    getData->data.type = type;
    if (type & TOPLEVEL::type::Error)
        return 1;

    switch (request)
    {
        case TOPLEVEL::type::Adjust:
            if (value < min)
                value = min;
            else if (value > max)
                value = max;
        break;
        case TOPLEVEL::type::Minimum:
            value = min;
            break;
        case TOPLEVEL::type::Maximum:
            value = max;
            break;
        case TOPLEVEL::type::Default:
            value = def;
            break;
    }
    return value;
}
