/*
    PADnoteParameters.cpp - Parameters for PADnote (PADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019 Will Godfrey
    Copyright 2020 Kristian Amlie & others
    Copyright 2022 Ichthyostega
    Copyright 2023 Will Godfrey and others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
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

#include <unistd.h>
#include <thread>
#include <memory>
#include <string>
#include <iostream>

#include "Misc/XMLStore.h"
#include "DSP/FFTwrapper.h"
#include "Synth/OscilGen.h"
#include "Synth/Resonance.h"
#include "Params/EnvelopeParams.h"
#include "Params/LFOParams.h"
#include "Params/FilterParams.h"
#include "Params/PADStatus.h"
#include "Misc/SynthEngine.h"
#include "Misc/FileMgrFuncs.h"
#include "Misc/NumericFuncs.h"
#include "Params/PADnoteParameters.h"
#include "Misc/WavFile.h"

using std::string;
using std::vector;
using file::saveData;
using func::setAllPan;
using func::power;

namespace{ // Implementation helpers...

    // normalise the numbers in the table to 0.0 .. 1.0
    inline void normaliseMax(vector<float>& table)
    {
        double max = 0.0;
        for (float const& val : table)
            if (val > max)
                max = val;

        if (max >= 0.000001)
            for (float& val : table)
                val = float(double(val) / max);
    }

    // normalise a float array to RMS
    inline void normaliseSpectrumRMS(fft::Waveform& data)
    {
        auto sqr = [](double val){ return val*val; };

        double rms = 0.0;
        for (size_t i = 0; i < data.size(); ++i)
        {
            rms += sqr(data[i]);
        }
        rms = sqrt(rms);
        if (rms < 0.000001)
            rms = 1.0;
        rms *= sqrt(double(1024 * 256) / data.size()) / 50.0;
        // TODO can we explain those magical numbers??
        // (Likely due to the fact we're still pre-IFFT here)
        // Are those numbers exact, or just some "I don't care it seems to work" approximation?
        //
        // TODO Maybe related: http://fftw.org/fftw3_doc/Complex-DFTs.html at the bottom
        // "FFTW computes an unnormalized transform: computing a forward followed by a backward transform
        //  (or vice versa) will result in the original data multiplied by the size of the transform (the product of the dimensions).
        //
        for (size_t i = 0; i < data.size(); ++i)
            data[i] = float(double(data[i]) / rms);
    }
}//(End)ImplHelpers



PADnoteParameters::PADnoteParameters(uchar pID, uchar kID, SynthEngine& _synth)
    : ParamBase(_synth)
    , Pmode{0}
    , Pquality{}
    , PProfile{}
    , Pbandwidth{500}
    , Pbwscale{0}
    , Phrpos{}
    , Pfixedfreq{0}
    , PfixedfreqET{0}
    , PBendAdjust{88}
    , POffsetHz{64}
    , PDetune{8192}  // fine detune "zero"
    , PCoarseDetune{0}
    , PDetuneType{1}

    // base Waveform
    , fft(synth.oscilsize)
    , POscil{new OscilParameters(fft, synth)}
    , resonance{new Resonance(synth)}
    , oscilgen{new OscilGen(fft, resonance.get(), &synth, POscil.get())}
    , FreqEnvelope{new EnvelopeParams(0, 0, synth)}
    , FreqLfo{new LFOParams(70, 0, 64, 0, 0, 0, false, 0, synth)}

    // Amplitude parameters
    , PStereo{1}
    , PPanning{64}
    , PRandom{false}
    , PWidth{63}
    , pangainL{0.7}
    , pangainR{0.7}
    , PVolume{90}
    , PAmpVelocityScaleFunction{64}
    , AmpEnvelope{new EnvelopeParams(64, 1, synth)}
    , AmpLfo{new LFOParams(80, 0, 64, 0, 0, 0, false, 1, synth)}

    , Fadein_adjustment{FADEIN_ADJUSTMENT_SCALE}
    , PPunchStrength{0}
    , PPunchTime{60}
    , PPunchStretch{64}
    , PPunchVelocitySensing{72}

    // Filter Parameters
    , GlobalFilter{new FilterParams(2, 94, 40, 0, synth)}
    , PFilterVelocityScale{64}
    , PFilterVelocityScaleFunction{64}
    , FilterEnvelope{new EnvelopeParams(0, 1, synth)}
    , FilterLfo{new LFOParams(80, 0, 64, 0, 0, 0, false, 2, synth)}

    // random walk re-Trigger
    , PrebuildTrigger{0}
    , PrandWalkDetune{0}
    , PrandWalkBandwidth{0}
    , PrandWalkFilterFreq{0}
    , PrandWalkProfileWidth{0}
    , PrandWalkProfileStretch{0}
    , randWalkDetune{wavetablePhasePrng}
    , randWalkBandwidth{wavetablePhasePrng}
    , randWalkFilterFreq{wavetablePhasePrng}
    , randWalkProfileWidth{wavetablePhasePrng}
    , randWalkProfileStretch{wavetablePhasePrng}

    // Wavetable building
    , xFade{}
    , PxFadeUpdate{0}
    , waveTable(Pquality)
    , futureBuild(task::BuildScheduler<PADTables>::wireBuildFunction
                 ,BuildOperation([this](){ return render_wavetable(); }))

    , partID{pID}
    , kitID{kID}
    , sampleTime{0}
    , wavetablePhasePrng{}
{
    FreqEnvelope->ASRinit(64, 50, 64, 60);
    AmpEnvelope->ADSRinit_dB(0, 40, 127, 25);
    FilterEnvelope->ADSRinit_filter(64, 40, 64, 70, 60, 64);

    defaults();
}


void PADnoteParameters::HarmonicProfile::defaults()
{
    base.type = 0;
    base.pwidth = 80;
    freqmult = 0;
    modulator.pstretch = 0;
    modulator.freq = 30;
    width = 127;
    amp.type = 0;
    amp.mode = 0;
    amp.par1 = 80;
    amp.par2 = 64;
    autoscale = true;
    onehalf = 0;
}

void PADnoteParameters::HarmonicPos::defaults()
{
    type = 0;
    par1 = 64;
    par2 = 64;
    par3 = 0;
}

void PADnoteParameters::defaults()
{
    Pmode = 0;
    Pquality.resetToDefaults();
    PProfile.defaults();
    Phrpos.defaults();

    Pbandwidth = 500;
    Pbwscale = 0;

    resonance->defaults();
    oscilgen->defaults();
    waveTable.reset();   // zero sound

    // By default set the oscil to max phase randomness.
    // Remark: phase randomness (and in fact oscil phase information)
    // is ignored altogether by PADsynth, but this default setting
    // can be useful in case the oscil is imported to an ADsynth
    // Historical Remark: in the original code base, this was
    // controlled by the "ADDvsPAD" setting.
    POscil->Prand = 127;

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
    PStereo = 1; // stereo
    setPan(PPanning = 64, synth.getRuntime().panLaw); // center
    PRandom = false;
    PWidth = 63;
    PAmpVelocityScaleFunction = 64;
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

    PxFadeUpdate = XFADE_UPDATE_DEFAULT; // 200ms crossfade after updating wavetables
    PrebuildTrigger = 0;                 // by default not auto-self-retrigger
    PrandWalkDetune = 0;         randWalkDetune.reset();
    PrandWalkBandwidth = 0;      randWalkBandwidth.reset();
    PrandWalkFilterFreq = 0;     randWalkFilterFreq.reset();
    PrandWalkProfileWidth = 0;   randWalkProfileWidth.reset();
    PrandWalkProfileStretch = 0; randWalkProfileStretch.reset();

    // reseed OscilGen and wavetable phase randomisation
    reseed(synth.randomINT());
    sampleTime = 0;
}


void PADnoteParameters::reseed(int seed)
{
    wavetablePhasePrng.init(seed);
    oscilgen->reseed(seed);
}


/* derive number of Wavetables for the desired octave coverage */
size_t PADTables::calcNumTables(PADQuality const& quality)
{
    int tables = quality.oct + 1;
    int smpoct = quality.smpoct;
    if (smpoct == 5)
        smpoct = 6;
    else
    if (smpoct == 6)
        smpoct = 12;
    if (smpoct != 0)
        tables *= smpoct;
    else
        tables = tables / 2 + 1;
    if (tables == 0)
        tables = 1;
    return tables;
}

/* derive size of single wavetable for the desired quality settings */
size_t PADTables::calcTableSize(PADQuality const& quality)
{
    return size_t(1) << (quality.samplesize + 14);
}




// Get the harmonic profile (i.e. the frequency distribution of a single harmonic)
// returns the profile normalised to 0..1, with resolution as requested by the size.
vector<float> PADnoteParameters::buildProfile(size_t size)
{
    vector<float> profile(size, 0.0); // zero-init

    float lineWidth = power<2>(((1.0f - PProfile.base.pwidth / 127.0f) * 12.0f));
    float freqmult = floorf(power<2>((PProfile.freqmult / 127.0f * 5.0f)) + 0.000001f);

    float modfreq  = floorf(power<2>((PProfile.modulator.freq / 127.0f * 5.0f)) + 0.000001f);
    float modStrch = powf((PProfile.modulator.pstretch / 127.0f), 4.0f) * 5.0 / sqrtf(modfreq);
    float amppar1 = power<2>(powf((PProfile.amp.par1 / 127.0f), 2.0f) * 10.0f) - 0.999f;
    float amppar2 = (1.0f - PProfile.amp.par2 / 127.0f) * 0.998f + 0.001f;
    float width = powf((150.0f / (PProfile.width + 22.0f)), 2.0f);

    // possibly apply a random walk
    lineWidth *= randWalkProfileWidth.getFactor();
    modStrch *= randWalkProfileStretch.getFactor();

    for (size_t i = 0; i < size * PROFILE_OVERSAMPLING; ++i)
    {
        bool makezero = false;
        float x = i * 1.0f / (size * float(PROFILE_OVERSAMPLING));
        float origx = x;
        // do the sizing (width)
        x = (x - 0.5f) * width + 0.5f;
        if (x < 0.0f)
        {
            x = 0.0f;
            makezero = true;
        }
        else
        {
            if (x >1.0f)
            {
                x = 1.0f;
                makezero = true;
            }
        }
        // compute the full profile or one half
        switch (PProfile.onehalf)
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
        x += sinf(x_before_freq_mult * PI * modfreq) * modStrch;

        x = fmodf(x + 1000.0f, 1.0f) * 2.0f - 1.0f;
        // this is the base function of the profile
        float f;
        switch (PProfile.base.type)
        {
        case 1:
            f = expf(-(x * x) * lineWidth);
            if (f < 0.4f)
                f = 0.0f;
            else
                f = 1.0f;
            break;

        case 2:
            f = expf(-(fabsf(x)) * sqrtf(lineWidth));
            break;

        default:
            f = expf(-(x * x) * lineWidth);
            break;
        }
        if (makezero)
            f = 0.0f;
        float amp = 1.0f;
        origx = origx * 2.0f - 1.0f;
        // compute the amplitude multiplier
        switch (PProfile.amp.type)
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
        if (PProfile.amp.type != 0)
        {
            switch (PProfile.amp.mode)
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
        profile[i / PROFILE_OVERSAMPLING] += finalsmp / PROFILE_OVERSAMPLING;
    }

    // normalise the profile to 0.0 .. 1.0
    normaliseMax(profile);
    return profile;
}


// calculate relative factor 0.0 ..1.0 to estimate the perceived bandwidth
float PADnoteParameters::calcProfileBandwith(vector<float> const& profile)
{
    if (!PProfile.autoscale)
        return 0.5f;

    size_t size = profile.size();
    auto sqrSlot = [&](size_t i){ return profile[i]*profile[i]; };

    // compute the estimated perceptual bandwidth
    float sum = 0.0f;
    size_t i;
    for (i = 0; i < size / 2 - 2; ++i)
    {
        sum += sqrSlot(i) + sqrSlot(size-1 - i);
        if (sum >= 4.0f)
            break;
    }
    return 1.0 - 2.0 * i / double(size);
}


// Convert the bandwidth parameter into cents
float PADnoteParameters::getBandwithInCent()
{
    float currBandwidth = std::min(1000.0f, Pbandwidth * randWalkBandwidth.getFactor());
    float result = powf(currBandwidth / 1000.0f, 1.1f);
    result = power<10>(result * 4.0f) * 0.25f;
    return result;
}


// Frequency factor for the position of each harmonic; allows for distorted non-harmonic spectra.
// Input is the number of the harmonic. n=0 is the fundamental, above are the overtones.
// Returns a frequency factor relative to the undistorted frequency of the fundamental.
float PADnoteParameters::calcHarmonicPositionFactor(float n)
{
    float par1 = power<10>(-(1.0f - Phrpos.par1 / 255.0f) * 3.0f);
    float par2 = Phrpos.par2 / 255.0f;

    float scale  = 1.0;
    float thresh = 0.0;

    float offset = 0.0;
    switch (Phrpos.type)
    {
    case 1: //  "ShiftU"
        thresh = int(par2 * par2 * 100.0f);
        if (n < thresh)
            offset = n;
        else
            offset = n + (n - thresh) * par1 * 8.0f;
        break;

    case 2: //  "ShiftL"
        thresh = int(par2 * par2 * 100.0f);
        if (n < thresh)
            offset = n;
        else
            offset = n - (n - thresh) * par1 * 0.90f;
        break;

    case 3: //  "PowerU"
        scale = par1 * 100.0f + 1.0f;
        offset = powf(n / scale, (1.0f - par2 * 0.8f)) * scale;
        break;

    case 4: //  "PowerL"
        offset = n * (1.0f - par1) + powf(n * 0.1f, par2 * 3.0f + 1.0f) * par1 * 10.0f;
        break;

    case 5: //  "Sine"
        offset = n + sinf(n * par2 * par2 * PI * 0.999f) * sqrtf(par1) * 2.0f;
        break;

    case 6: //  "Power"
        scale = powf((par2 * 2.0f), 2.0f) + 0.1f;
        offset = n * powf(1.0f + par1 * powf(n * 0.8f, scale), scale);
        break;

    case 7: //  "Shift"
        scale = 1.0f + Phrpos.par1 / 255.0f;
        offset = n / scale;
        break;

    default://  "Harmonic"
           //    undistorted. n=0 => factor=1.0 (corresponding to the base frequency)
        offset = n;
        break;
    }
    float result = 1.0f + offset;
    float par3 = Phrpos.par3 / 255.0f;
    float iresult = floorf(result + 0.5f);
    float dresult = result - iresult;
    result = iresult + (1.0f - par3) * dresult;
    if (result < 0.0f) result = 0.0f;
    return result;
}


// Generates the long spectrum for Bandwidth mode (only amplitudes are generated;
// phases will be random)
vector<float> PADnoteParameters::generateSpectrum_bandwidthMode(float basefreq, size_t spectrumSize,
                                                                vector<float> const& profile)
{
    assert(spectrumSize > 1);
    vector<float> spectrum(spectrumSize, 0.0f); // zero-init

    // get the harmonic structure from the oscillator
    vector<float> harmonics(oscilgen->getSpectrumForPAD(basefreq));
    normaliseMax(harmonics); // within 0.0 .. 1.0

    // derive the "perceptual" bandwidth for the given profile (a value 0 .. 1)
    float bwadjust = calcProfileBandwith(profile);

    assert(harmonics.size() == fft.spectrumSize());
    for (size_t nh = 0; nh+1 < fft.spectrumSize(); ++nh)
    {   //for each harmonic
        float realfreq = calcHarmonicPositionFactor(nh) * basefreq;
        if (realfreq > synth.samplerate_f * 0.49999f)
            break;
        if (realfreq < 20.0f)
            break;
        if (harmonics[nh] < 1e-5f)
            continue;
        //compute the bandwidth of each harmonic
        float bw = (power<2>(getBandwithInCent() / 1200.0f) - 1.0f) * basefreq / bwadjust;
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
        size_t ibw = 1 + size_t(bw / (synth.samplerate_f * 0.5f) * spectrumSize);
        float amp = harmonics[nh];
        if (resonance->Penabled)
            amp *= resonance->getfreqresponse(realfreq);
        size_t profilesize = profile.size();
        if (ibw > profilesize)
        {   // if the bandwidth is larger than the profilesize
            float rap = sqrtf(float(profilesize) / float(ibw));
            int cfreq = int(realfreq / (synth.halfsamplerate_f) * spectrumSize) - ibw / 2;
            for (size_t i = 0; i < ibw; ++i)
            {
                int src = int(i * rap * rap);
                int spfreq = i + cfreq;
                if (spfreq < 0)
                    continue;
                if (spfreq >= int(spectrumSize))
                    break;
                spectrum[spfreq] += amp * profile[src] * rap;
            }
        }
        else
        {   // if the bandwidth is smaller than the profilesize
            float rap = sqrtf(float(ibw) / float(profilesize));
            float ibasefreq = realfreq / (synth.halfsamplerate_f) * spectrumSize;
            for (size_t i = 0; i < profilesize; ++i)
            {
                float idfreq = i / (float)profilesize - 0.5f;
                idfreq *= ibw;
                int spfreq = (int)(idfreq + ibasefreq);
                float fspfreq = fmodf(idfreq + ibasefreq, 1.0f);
                if (spfreq <= 0)
                    continue;
                if (spfreq >= int(spectrumSize) - 1)
                    break;
                spectrum[spfreq] += amp * profile[i] * rap * (1.0f - fspfreq);
                spectrum[spfreq + 1] += amp * profile[i] * rap * fspfreq;
            }
        }
    }
    return spectrum;
}


// Generates the long spectrum for non-Bandwidth modes (only amplitudes are generated; phases will be random)
vector<float> PADnoteParameters::generateSpectrum_otherModes(float basefreq, size_t spectrumSize)
{
    assert(spectrumSize > 1);
    vector<float> spectrum(spectrumSize, 0.0f); // zero-init

    // get the harmonic structure from the oscillator
    vector<float> harmonics(oscilgen->getSpectrumForPAD(basefreq));
    normaliseMax(harmonics); // within 0.0 .. 1.0

    for (size_t nh = 0; nh+1 < fft.spectrumSize(); ++nh)
    {   //for each harmonic
        float realfreq = calcHarmonicPositionFactor(nh) * basefreq;

        ///sa fac aici interpolarea si sa am grija daca frecv descresc
        //[Romanian, from original Author] "do the interpolation here and be careful if they decrease frequency"

        if (realfreq > synth.samplerate_f * 0.49999f)
            break;
        if (realfreq < 20.0f)
            break;

        float amp = harmonics[nh];
        if (resonance->Penabled)
            amp *= resonance->getfreqresponse(realfreq);
        int cfreq = int(realfreq / (synth.halfsamplerate_f) * spectrumSize);
        spectrum[cfreq] = amp + 1e-9f;
    }

    if (Pmode != 1)
    {// if not "discrete", i.e. render "continuous" spectrum
        size_t old = 0;
        for (size_t k = 1; k < spectrumSize; ++k)
        {
            if ((spectrum[k] > 1e-10f) || (k == (spectrumSize - 1)))
            {
                assert(k > old);
                size_t delta = k - old;
                float val1 = spectrum[old];
                float val2 = spectrum[k];
                float idelta = 1.0f / delta;
                for (size_t i = 0; i < delta; ++i)
                {
                    float x = idelta * i;
                    spectrum[old+i] = val1 * (1.0f - x) + val2 * x;
                }
                old = k;
            }
        }
    }
    return spectrum;
}


void PADnoteParameters::buildNewWavetable(bool blocking)
{
    PADStatus::mark(PADStatus::DIRTY, synth.interchange, partID,kitID);
    if (synth.getRuntime().useLegacyPadBuild())
        mute_and_rebuild_synchronous();
    else
    if (not blocking)
        futureBuild.requestNewBuild();
    else
    {   // Guarantee to invoke a new build NOW and block until it is ready...
        // This is tricky, since new builds can be triggered any time from the GUI
        // and also the SynthEngine might pick up the result concurrently.

        // (1) Attempt to get hold of a running build triggered earlier (with old parameters)
        futureBuild.blockingWait();

        // (2) when we trigger now, we can be sure the current state of parameters will be used
        futureBuild.requestNewBuild();

        // (3) again wait for this build to complete...
        //     Note: Result will be published to SynthEngine -- unless a new build was triggered
        futureBuild.blockingWait(true);
    }
}


namespace { auto& NO_RESULT = std::nullopt; }

// This is the heart of the PADSynth: generate a set of perfectly looped wavetables,
// based on rendering a harmonic profile for each line of the base waveform spectrum.
// Each table is generated by a single inverse FFT, but using a high resolution spectrum.
// Note: when returning the NoResult marker, the build shall be aborted and restarted.
optional<PADTables> PADnoteParameters::render_wavetable()
{
    PADTables newTable(Pquality);
    const size_t spectrumSize = newTable.tableSize / 2;
    PADStatus::mark(PADStatus::BUILDING, synth.interchange, partID,kitID);

    // prepare storage for a very large spectrum and FFT transformer
    fft::Calc fft{newTable.tableSize};
    fft::Spectrum fftCoeff(spectrumSize);

    // (in »bandwidth mode«) build harmonic profile used for each line
    vector<float> profile = Pmode == 0? buildProfile(SIZE_HARMONIC_PROFILE)
                                      : vector<float>(); // empty dummy

    if (futureBuild.shallRebuild())
        return NO_RESULT;

    float baseNoteFreq = 65.406f * power<2>(Pquality.basenote / 2);
    if (Pquality.basenote %2 == 1)
        baseNoteFreq *= 1.5;

    float adj[newTable.numTables]; // used to compute frequency relation to the base note frequency
    for (size_t tabNr = 0; tabNr < newTable.numTables; ++tabNr)
        adj[tabNr] = (Pquality.oct + 1.0f) * (float)tabNr / newTable.numTables;

    for (size_t tabNr = 0; tabNr < newTable.numTables; ++tabNr)
    {
        float tmp = adj[tabNr] - adj[newTable.numTables - 1] * 0.5f;
        float basefreqadjust = power<2>(tmp);
        float basefreq = baseNoteFreq *  basefreqadjust;

        newTable.basefreq[tabNr] = basefreq;

        vector<float> spectrum =
            Pmode == 0? generateSpectrum_bandwidthMode(basefreq, spectrumSize, profile)
                      : generateSpectrum_otherModes(basefreq, spectrumSize);

        for (size_t i = 1; i < spectrumSize; ++i)
        {   // Note: each wavetable uses differently randomised phases
            float phase = wavetablePhasePrng.numRandom() * 6.29f;
            fftCoeff.c(i) = spectrum[i] * cosf(phase);
            fftCoeff.s(i) = spectrum[i] * sinf(phase);
        }

        if (futureBuild.shallRebuild())
            return NO_RESULT;

        fft::Waveform& newsmp = newTable[tabNr];
        newsmp[0] = 0.0f;                ///TODO 12/2021 (why) is this necessary? Doesn't the IFFT generate a full waveform?

        fft.freqs2smps(fftCoeff, newsmp);
        // that's all; here is the only IFFT for the whole sample; no windows are used ;-) (Comment by original author)

        normaliseSpectrumRMS(newsmp);

        // prepare extra samples used by the linear or cubic interpolation
        newsmp.fillInterpolationBuffer();
    }

    PADStatus::mark(PADStatus::PENDING, synth.interchange, partID,kitID);
    return newTable;
}


/* called once before each buffer compute cycle;
 * possibly pick up results from background wavetable build.
 * WARNING: while FutureBuild::isReady() is reliable and airtight, the remaining logic
 *          within the body is not thread-safe. FutureBuild::swap() does not cover all corner cases
 *          when re-scheduling and thus should not be called concurrently. And the ref-count in xFade
 *          is *deliberately without thread synchronisation* (since we're on the hot audio codepath).
 *          If we ever consider processing SynthEngine concurrently, this logic must be revised.
 *          (comment by Ichthyo, 3/2022)
 */
void PADnoteParameters::activate_wavetable()
{
    if (futureBuild.isReady()
        and (PxFadeUpdate == 0 or xFade.startXFade(waveTable)))
    {                          // Note: don't pick up new waveTable while fading
        PADStatus::mark(PADStatus::CLEAN, synth.interchange, partID,kitID);
        futureBuild.swap(waveTable);
        paramsChanged();
        sampleTime = 0;
    }
    else
        maybeRetrigger();
}


/* automatic self-retrigger: if activated, a new wavetable background build is launched
 * after a given amount of "sample time" has passed. Moreover, some parameters may perform
 * a »random walk« by applying a small random offset on each rebuild, within a given spread.
 */
void PADnoteParameters::maybeRetrigger()
{
    if (PrebuildTrigger == 0 or synth.getRuntime().useLegacyPadBuild())
        return; // this feature requires a background build of the wavetable.

    if (sampleTime < PrebuildTrigger)
    {
        sampleTime += synth.buffersize_f / synth.samplerate_f * 1000;
        return;
    }
    else if (not futureBuild.isUnderway())
    {
        randWalkDetune.walkStep();
        randWalkBandwidth.walkStep();
        randWalkFilterFreq.walkStep();
        randWalkProfileWidth.walkStep();
        randWalkProfileStretch.walkStep();
        futureBuild.requestNewBuild();
    }
}


/* Legacy mode: rebuild the PAD wavetable immediately,
 * without any background thread scheduling. */
void PADnoteParameters::mute_and_rebuild_synchronous()
{
    waveTable.reset();   // mute by zeroing
    auto result = render_wavetable();
    if (result)
    {
        using std::swap;
        swap(waveTable, *result);
        paramsChanged();
        sampleTime = 0;
    }
}



void PADnoteParameters::setPan(char pan, uchar panLaw)
{
    PPanning = pan;
    if (!PRandom)
        setAllPan(PPanning, pangainL, pangainR, panLaw);
    else
        pangainL = pangainR = 0.7f;
}


bool PADnoteParameters::export2wav(string basefilename)
{
    string type;
    if (synth.getRuntime().isLittleEndian)
        type = "RIFF"; // default wave format
    else
        type = "RIFX";

    basefilename += "--sample-";
    bool isOK = true;
    for (size_t tab = 0; tab < waveTable.numTables; ++tab)
    {
        char tmpstr[22];
        snprintf(tmpstr, 22, "-%02zu", tab + 1);
        string filename = basefilename + string(tmpstr) + EXTEN::MSwave;
        uint   block;
        ushort sBlock;
        uint  buffSize = 44 + sizeof(short int) * waveTable.tableSize; // total size
        char* buffer = (char*) malloc (buffSize);
        strcpy(buffer, type.c_str());
        block = waveTable.tableSize * 4 + 36; // 2 channel shorts + part header
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
        block = synth.samplerate;
        memcpy(buffer + 24, &block, 4);
        block = synth.samplerate * 2; // ByteRate
                // (SampleRate * NumChannels * BitsPerSample) / 8
        memcpy(buffer + 28, &block, 4);
        sBlock = 2; // BlockAlign
                // (bitsPerSample * channels) / 8
        memcpy(buffer + 32, &sBlock, 2);
        sBlock = 16; // BitsPerSample
        memcpy(buffer + 34, &sBlock, 2);
        temp = "data";
        strcpy(buffer + 36, temp.c_str());
        block = waveTable.tableSize * 2; // data size
        memcpy(buffer + 40, &block, 4);
        for (size_t smp = 0; smp < waveTable.tableSize; ++smp)
        {
            sBlock = (waveTable[tab][smp] * 32767.0f);
            buffer [44 + smp * 2] = sBlock & 0xff;
            buffer [45 + smp * 2] = (sBlock >> 8) & 0xff;
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


void PADnoteParameters::add2XML(XMLtree& xmlPadSynth)
{
    xmlPadSynth.addPar_bool("stereo"         , PStereo);
    xmlPadSynth.addPar_int ("mode"           , Pmode);
    xmlPadSynth.addPar_int ("bandwidth"      , Pbandwidth);
    xmlPadSynth.addPar_int ("bandwidth_scale", Pbwscale);
    xmlPadSynth.addPar_uint("xfade_update"   , PxFadeUpdate);

    XMLtree xmlPrf = xmlPadSynth.addElm("HARMONIC_PROFILE");
        xmlPrf.addPar_int ("base_type"                , PProfile.base.type);
        xmlPrf.addPar_int ("base_par1"                , PProfile.base.pwidth);
        xmlPrf.addPar_int ("frequency_multiplier"     , PProfile.freqmult);
        xmlPrf.addPar_int ("modulator_par1"           , PProfile.modulator.pstretch);
        xmlPrf.addPar_int ("modulator_frequency"      , PProfile.modulator.freq);
        xmlPrf.addPar_int ("width"                    , PProfile.width);
        xmlPrf.addPar_int ("amplitude_multiplier_type", PProfile.amp.type);
        xmlPrf.addPar_int ("amplitude_multiplier_mode", PProfile.amp.mode);
        xmlPrf.addPar_int ("amplitude_multiplier_par1", PProfile.amp.par1);
        xmlPrf.addPar_int ("amplitude_multiplier_par2", PProfile.amp.par2);
        xmlPrf.addPar_bool("autoscale"                , PProfile.autoscale);
        xmlPrf.addPar_int ("one_half"                 , PProfile.onehalf);

    XMLtree xmlOscil = xmlPadSynth.addElm("OSCIL");
        POscil->add2XML(xmlOscil);

    XMLtree xmlRes = xmlPadSynth.addElm("RESONANCE");
        resonance->add2XML(xmlRes);

    XMLtree xmlHPos = xmlPadSynth.addElm("HARMONIC_POSITION");
        xmlHPos.addPar_int ("type"      , Phrpos.type);
        xmlHPos.addPar_int ("parameter1", Phrpos.par1);
        xmlHPos.addPar_int ("parameter2", Phrpos.par2);
        xmlHPos.addPar_int ("parameter3", Phrpos.par3);

    XMLtree xmlQual = xmlPadSynth.addElm("SAMPLE_QUALITY");
        xmlQual.addPar_int ("samplesize", Pquality.samplesize);
        xmlQual.addPar_int ("basenote"  , Pquality.basenote);
        xmlQual.addPar_int ("octaves"   , Pquality.oct);
        xmlQual.addPar_int ("samples_per_octave",Pquality.smpoct);

    XMLtree xmlAmp = xmlPadSynth.addElm("AMPLITUDE_PARAMETERS");
    {
        xmlAmp.addPar_int ("volume"      , PVolume);
        // Yoshimi format for random panning
        xmlAmp.addPar_int ("pan_pos"     , PPanning);
        xmlAmp.addPar_bool("random_pan"  , PRandom);
        xmlAmp.addPar_int ("random_width", PWidth);

        // support legacy format
        if (PRandom)
            xmlAmp.addPar_int ("panning", 0);
        else
            xmlAmp.addPar_int ("panning", PPanning);

        xmlAmp.addPar_int ("velocity_sensing"      , PAmpVelocityScaleFunction);
        xmlAmp.addPar_int ("fadein_adjustment"     , Fadein_adjustment);
        xmlAmp.addPar_int ("punch_strength"        , PPunchStrength);
        xmlAmp.addPar_int ("punch_time"            , PPunchTime);
        xmlAmp.addPar_int ("punch_stretch"         , PPunchStretch);
        xmlAmp.addPar_int ("punch_velocity_sensing", PPunchVelocitySensing);

        XMLtree xmlEnv = xmlAmp.addElm("AMPLITUDE_ENVELOPE");
            AmpEnvelope->add2XML(xmlEnv);

        XMLtree xmlLFO = xmlAmp.addElm("AMPLITUDE_LFO");
            AmpLfo->add2XML(xmlLFO);
    }

    XMLtree xmlFreq = xmlPadSynth.addElm("FREQUENCY_PARAMETERS");
    {
        xmlFreq.addPar_int("fixed_freq"   , Pfixedfreq);
        xmlFreq.addPar_int("fixed_freq_et", PfixedfreqET);
        xmlFreq.addPar_int("bend_adjust"  , PBendAdjust);
        xmlFreq.addPar_int("offset_hz"    , POffsetHz);
        xmlFreq.addPar_int("detune"       , PDetune);
        xmlFreq.addPar_int("coarse_detune", PCoarseDetune);
        xmlFreq.addPar_int("detune_type"  , PDetuneType);

        XMLtree xmlEnv = xmlFreq.addElm("FREQUENCY_ENVELOPE");
            FreqEnvelope->add2XML(xmlEnv);

        XMLtree xmlLFO = xmlFreq.addElm("FREQUENCY_LFO");
            FreqLfo->add2XML(xmlLFO);
    }

    XMLtree xmlFilterParams = xmlPadSynth.addElm("FILTER_PARAMETERS");
    {
        xmlFilterParams.addPar_int("velocity_sensing_amplitude", PFilterVelocityScale);
        xmlFilterParams.addPar_int("velocity_sensing",   PFilterVelocityScaleFunction);

        XMLtree xmlFilt = xmlFilterParams.addElm("FILTER");
            GlobalFilter->add2XML(xmlFilt);

        XMLtree xmlEnv = xmlFilterParams.addElm("FILTER_ENVELOPE");
            FilterEnvelope->add2XML(xmlEnv);

        XMLtree xmlLFO = xmlFilterParams.addElm("FILTER_LFO");
            FilterLfo->add2XML(xmlLFO);
    }

    XMLtree xmlRand = xmlPadSynth.addElm("RANDOM_WALK");
        xmlRand.addPar_uint("rebuild_trigger"    , PrebuildTrigger);
        xmlRand.addPar_int("rand_detune"         , PrandWalkDetune);
        xmlRand.addPar_int("rand_bandwidth"      , PrandWalkBandwidth);
        xmlRand.addPar_int("rand_filter_freq"    , PrandWalkFilterFreq);
        xmlRand.addPar_int("rand_profile_width"  , PrandWalkProfileWidth);
        xmlRand.addPar_int("rand_profile_stretch", PrandWalkProfileStretch);
}


void PADnoteParameters::getfromXML(XMLtree& xmlPadSynth)
{
    assert(xmlPadSynth);
    Pmode        = xmlPadSynth.getPar_127 ("mode"           , 0);
    PStereo      = xmlPadSynth.getPar_bool("stereo"         , PStereo);
    Pbandwidth   = xmlPadSynth.getPar_int ("bandwidth"      , Pbandwidth  , 0,1000);
    Pbwscale     = xmlPadSynth.getPar_127 ("bandwidth_scale", Pbwscale);
    PxFadeUpdate = xmlPadSynth.getPar_uint("xfade_update"   , PxFadeUpdate, 0,XFADE_UPDATE_MAX);

    if (XMLtree xmlPrf = xmlPadSynth.getElm("HARMONIC_PROFILE"))
    {
        PProfile.base.type   = xmlPrf.getPar_127("base_type"                , PProfile.base.type);
        PProfile.base.pwidth = xmlPrf.getPar_127("base_par1"                , PProfile.base.pwidth);
        PProfile.freqmult    = xmlPrf.getPar_127("frequency_multiplier"     , PProfile.freqmult);
        PProfile.modulator.pstretch=xmlPrf.getPar_127("modulator_par1"      , PProfile.modulator.pstretch);
        PProfile.modulator.freq    =xmlPrf.getPar_127("modulator_frequency" , PProfile.modulator.freq);
        PProfile.width       = xmlPrf.getPar_127("width"                    , PProfile.width);
        PProfile.amp.type    = xmlPrf.getPar_127("amplitude_multiplier_type", PProfile.amp.type);
        PProfile.amp.mode    = xmlPrf.getPar_127("amplitude_multiplier_mode", PProfile.amp.mode);
        PProfile.amp.par1    = xmlPrf.getPar_127("amplitude_multiplier_par1", PProfile.amp.par1);
        PProfile.amp.par2    = xmlPrf.getPar_127("amplitude_multiplier_par2", PProfile.amp.par2);
        PProfile.autoscale   = xmlPrf.getPar_bool("autoscale"               , PProfile.autoscale);
        PProfile.onehalf     = xmlPrf.getPar_127("one_half"                 , PProfile.onehalf);
    }

    if (XMLtree xmlOscil = xmlPadSynth.getElm("OSCIL"))
        POscil->getfromXML(xmlOscil);
    else
        POscil->defaults();

    if (XMLtree xmlRes = xmlPadSynth.getElm("RESONANCE"))
        resonance->getfromXML(xmlRes);
    else
        resonance->defaults();

    if (XMLtree xmlHPos = xmlPadSynth.getElm("HARMONIC_POSITION"))
    {
        Phrpos.type = xmlHPos.getPar_127("type"      , Phrpos.type);
        Phrpos.par1 = xmlHPos.getPar_int("parameter1", Phrpos.par1, 0,255);
        Phrpos.par2 = xmlHPos.getPar_int("parameter2", Phrpos.par2, 0,255);
        Phrpos.par3 = xmlHPos.getPar_int("parameter3", Phrpos.par3, 0,255);
    }

    if (XMLtree xmlQual = xmlPadSynth.getElm("SAMPLE_QUALITY"))
    {
        Pquality.samplesize = xmlQual.getPar_127("samplesize", Pquality.samplesize);
        Pquality.basenote   = xmlQual.getPar_127("basenote"  , Pquality.basenote);
        Pquality.oct        = xmlQual.getPar_127("octaves"   , Pquality.oct);
        Pquality.smpoct     = xmlQual.getPar_127("samples_per_octave",Pquality.smpoct);
    }

    if (XMLtree xmlAmp = xmlPadSynth.getElm("AMPLITUDE_PARAMETERS"))
    {
        PVolume = xmlAmp.getPar_127("volume"      , PVolume);
        int val = xmlAmp.getPar_127("random_width", UNUSED);
        if (val < 64)
        {// new Yoshimi format
            PWidth = val;
            setPan(xmlAmp.getPar_127("pan_pos",PPanning), synth.getRuntime().panLaw);
            PRandom = xmlAmp.getPar_bool("random_pan", PRandom);
        }
        else
        {// legacy
            setPan(xmlAmp.getPar_127("panning",PPanning), synth.getRuntime().panLaw);
            if (PPanning == 0)
            {
                PPanning = 64;
                PRandom = true;
                PWidth = 63;
            }
            else
                PRandom = false;
        }
        PAmpVelocityScaleFunction = xmlAmp.getPar_127("velocity_sensing"      , PAmpVelocityScaleFunction);
        Fadein_adjustment         = xmlAmp.getPar_127("fadein_adjustment"     , Fadein_adjustment);
        PPunchStrength            = xmlAmp.getPar_127("punch_strength"        , PPunchStrength);
        PPunchTime                = xmlAmp.getPar_127("punch_time"            , PPunchTime);
        PPunchStretch             = xmlAmp.getPar_127("punch_stretch"         , PPunchStretch);
        PPunchVelocitySensing     = xmlAmp.getPar_127("punch_velocity_sensing", PPunchVelocitySensing);

        if (XMLtree xmlEnv = xmlAmp.getElm("AMPLITUDE_ENVELOPE"))
            AmpEnvelope->getfromXML(xmlEnv);
        else
            AmpEnvelope->defaults();

        if (XMLtree xmlLFO = xmlAmp.getElm("AMPLITUDE_LFO"))
            AmpLfo->getfromXML(xmlLFO);
        else
            AmpLfo->defaults();
    }

    if (XMLtree xmlFreq = xmlPadSynth.getElm("FREQUENCY_PARAMETERS"))
    {
        Pfixedfreq    = xmlFreq.getPar_127("fixed_freq"   , Pfixedfreq);
        PfixedfreqET  = xmlFreq.getPar_127("fixed_freq_et", PfixedfreqET);
        PBendAdjust   = xmlFreq.getPar_127("bend_adjust"  , PBendAdjust);
        POffsetHz     = xmlFreq.getPar_127("offset_hz"    , POffsetHz);
        PDetune       = xmlFreq.getPar_int("detune"       , PDetune      , 0,16383);
        PCoarseDetune = xmlFreq.getPar_int("coarse_detune", PCoarseDetune, 0,16383);
        PDetuneType   = xmlFreq.getPar_127("detune_type"  , PDetuneType);

        if (XMLtree xmlEnv = xmlFreq.getElm("FREQUENCY_ENVELOPE"))
            FreqEnvelope->getfromXML(xmlEnv);

        if (XMLtree xmlLFO = xmlFreq.getElm("FREQUENCY_LFO"))
            FreqLfo->getfromXML(xmlLFO);
    }

    if (XMLtree xmlFilterParams = xmlPadSynth.getElm("FILTER_PARAMETERS"))
    {
        PFilterVelocityScale = xmlFilterParams.getPar_127("velocity_sensing_amplitude", PFilterVelocityScale);
        PFilterVelocityScaleFunction = xmlFilterParams.getPar_127("velocity_sensing", PFilterVelocityScaleFunction);

        if (XMLtree xmlFilt = xmlFilterParams.getElm("FILTER"))
            GlobalFilter->getfromXML(xmlFilt);
        else
            GlobalFilter->defaults();

        if (XMLtree xmlEnv = xmlFilterParams.getElm("FILTER_ENVELOPE"))
            FilterEnvelope->getfromXML(xmlEnv);
        else
            FilterEnvelope->defaults();

        if (XMLtree xmlLFO = xmlFilterParams.getElm("FILTER_LFO"))
            FilterLfo->getfromXML(xmlLFO);
        else
            FilterLfo->defaults();
    }

    if (XMLtree xmlRand = xmlPadSynth.getElm("RANDOM_WALK"))
    {
        PrebuildTrigger        =xmlRand.getPar_uint("rebuild_trigger"    ,PrebuildTrigger, 0,REBUILDTRIGGER_MAX);
        PrandWalkDetune        =xmlRand.getPar_127("rand_detune"         ,PrandWalkDetune);
        PrandWalkBandwidth     =xmlRand.getPar_127("rand_bandwidth"      ,PrandWalkBandwidth);
        PrandWalkFilterFreq    =xmlRand.getPar_127("rand_filter_freq"    ,PrandWalkFilterFreq);
        PrandWalkProfileWidth  =xmlRand.getPar_127("rand_profile_width"  ,PrandWalkProfileWidth);
        PrandWalkProfileStretch=xmlRand.getPar_127("rand_profile_stretch",PrandWalkProfileStretch);
        randWalkDetune         .setSpread(PrandWalkDetune);
        randWalkBandwidth      .setSpread(PrandWalkBandwidth);
        randWalkFilterFreq     .setSpread(PrandWalkFilterFreq);
        randWalkProfileWidth   .setSpread(PrandWalkProfileWidth);
        randWalkProfileStretch .setSpread(PrandWalkProfileStretch);
    }
    // trigger re-build of the wavetable as background task...
    waveTable.reset();           // silence existing sound from previous instruments using the same part
    futureBuild.blockingWait();  // possibly retrieve result of ongoing build without publishing (Note: blocks consecutive instrument loads from MIDI)
    buildNewWavetable();         // launch rebuild of wavetables for the new instrument (background task)
    // result will be picked up from PADnote::noteout() when ready
}


float PADnoteParameters::getLimits(CommandBlock *getData)
{
    float value = getData->data.value;
    int request = int(getData->data.type & TOPLEVEL::type::Default);
    int control = getData->data.control;

    uchar type = 0;

    // padnote defaults
    int min = 0;
    int def = 64;
    int max = 127;
    type |= TOPLEVEL::type::Integer;
    uchar learnable = TOPLEVEL::type::Learnable;
    switch (control)
    {
        case PADSYNTH::control::volume:
            type |= learnable;
            def = 90;
            break;

        case PADSYNTH::control::velocitySense:
            type |= learnable;
            def = 64;
            break;

        case PADSYNTH::control::panning:
            type |= learnable;
            break;

        case PADSYNTH::control::enableRandomPan:
            max = 1;
            break;

        case PADSYNTH::control::randomWidth:
            type |= learnable;
            def = 63;
            max = 63;
            break;

        case PADSYNTH::control::bandwidth:
            type |= learnable;
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

        case PADSYNTH::control::xFadeUpdate:
            type |= learnable;
            def = 200;
            max = 20000;
            break;

        case PADSYNTH::control::rebuildTrigger:
            type |= learnable;
            def = 0;
            max = 60000;
            break;

        case PADSYNTH::control::randWalkDetune:
            type |= learnable;
            def = 0;
            max = 127;
            break;

        case PADSYNTH::control::randWalkBandwidth:
            type |= learnable;
            def = 0;
            max = 127;
            break;

        case PADSYNTH::control::randWalkFilterFreq:
            type |= learnable;
            def = 0;
            max = 127;
            break;

        case PADSYNTH::control::randWalkProfileWidth:
            type |= learnable;
            def = 0;
            max = 127;
            break;

        case PADSYNTH::control::randWalkProfileStretch:
            type |= learnable;
            def = 0;
            max = 127;
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
            def = 1;
            max = 1;
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
