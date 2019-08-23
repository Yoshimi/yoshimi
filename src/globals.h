/*
    globals.h - general static definitions

    Copyright 2018-2019, Will Godfrey

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later  for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef GLOBALS_H
#define GLOBALS_H

#include <cstdint>
#include <cstddef>

/*
 * For test purposes where you want guaranteed identical results, enable the
 * #define below.
 * Be aware this does strange things to both subSynth and padSynth as they
 * require randomness to produce normal sounds.
 */
//#define NORANDOM ON

// math
#define PI 3.1415926536f
#define TWOPI 6.28318530718f
#define HALFPI 1.57079632679f
#define LOG_2 0.693147181f

/*
 * we only use 23 bits as with 24 there is risk of
 * an overflow when making float/int conversions
 */
#define Fmul2I 1073741823
#define Cshift2I 23

/*
 * proposed conversions from float to hi res int
 * multiplier is 1000000
 *
 * for LFO freq turns actual value 85.25 into 85250000
 * current step size 0.06 becomes 6000
 *
 * scales A frequency now restricted to +- 0.5 octave
 * 660Hz becomes 660000000
 * At 329Hz resolution is still better than 1/10000 cent
 * Assumed detectable interval is 5 cents
 *
 * also use for integers that need higher resolution
 * such as unspecified 0-127 integers
*/

// many of the following are for convenience and consistency
// changing them is likely to have unpredicable consequences

// sizes
const unsigned char COMMAND_SIZE = 252;
const unsigned char MAX_HISTORY = 25;
const int MAX_PRESETS = 1000;
const unsigned char MAX_PRESET_DIRS = 128;
const unsigned char MAX_BANK_ROOT_DIRS = 128;
const unsigned char MAX_BANKS_IN_ROOT = 128;
const unsigned char MAX_INSTRUMENTS_IN_BANK = 160;
const unsigned char MAX_AD_HARMONICS = 128;
const unsigned char MAX_SUB_HARMONICS = 64;
const unsigned char PAD_MAX_SAMPLES = 96;
const unsigned char NUM_MIDI_PARTS = 64;
const unsigned char PART_POLY = 0;
const unsigned char PART_MONO = 1;
const unsigned char PART_LEGATO = 2;
const unsigned char MIDI_NOT_LEGATO = 3;
const unsigned char MIDI_LEGATO = 4;
const unsigned char NUM_MIDI_CHANNELS = 16;
const unsigned char MIDI_LEARN_BLOCK = 200;
const int MAX_ENVELOPE_POINTS = 40;
const int MIN_ENVELOPE_DB = -60;
const int MAX_RESONANCE_POINTS = 256;
const int MAX_KEY_SHIFT = 36;
const int MIN_KEY_SHIFT = -36;
const float A_MIN = 329.0f;
const float A_MAX = 660.0f;


const unsigned int MIN_OSCIL_SIZE = 256; // MAX_AD_HARMONICS * 2
const unsigned int MAX_OSCIL_SIZE = 16384;
const unsigned int MIN_BUFFER_SIZE = 16;
const unsigned int MAX_BUFFER_SIZE = 8192;
const unsigned char NO_MSG = 255; // these two may become different
const unsigned char UNUSED = 255;

// GUI colours
const unsigned int ADD_COLOUR = 0xdfafbf00;
const unsigned int BASE_COLOUR = 0xbfbfbf00;
const unsigned int SUB_COLOUR = 0xafcfdf00;
const unsigned int PAD_COLOUR = 0xcfdfaf00;
const unsigned int YOSHI_COLOUR = 0x0000e100;
const unsigned int EXTOSC_COLOUR = 0x8fbfdf00;
const unsigned int EXTVOICE_COLOUR = 0x9fdf8f00;
const unsigned int MODOFF_COLOUR = 0x80808000;

// these were previously (pointlessly) user configurable
const unsigned char NUM_VOICES = 8;
const unsigned char POLIPHONY = 80;
const unsigned char PART_POLIPHONY = 60;
const unsigned char NUM_SYS_EFX = 4;
const unsigned char NUM_INS_EFX = 8;
const unsigned char NUM_PART_EFX = 3;
const unsigned char NUM_KIT_ITEMS = 16;
const float VELOCITY_MAX_SCALE = 8.0f;
const unsigned char FADEIN_ADJUSTMENT_SCALE = 20;
const unsigned char MAX_EQ_BANDS = 8;  // MAX_EQ_BANDS must be less than 20
const unsigned char MAX_FILTER_STAGES = 5;
const unsigned char FF_MAX_VOWELS = 6;
const unsigned char FF_MAX_FORMANTS = 12;
const unsigned char FF_MAX_SEQUENCE = 8;
const unsigned char MAX_PHASER_STAGES = 12;
const unsigned char MAX_ALIENWAH_DELAY = 100;

namespace YOSH
{
    // float to bool done this way to ensure consistency
    inline bool F2B(float value) {return value > 0.5f;}
}

/*
 * for many of the following, where they are in groups the
 * group order must not change, but the actual values can
 * and new entries can be added between the group ends
 */


namespace ENVMODE
{
    const unsigned char amplitudeLin = 1;
    const unsigned char amplitudeLog = 2;
    const unsigned char frequency = 3;
    const unsigned char filter = 4;
    const unsigned char bandwidth = 5;
}

namespace TOPLEVEL // usage TOPLEVEL::section::vector
{
    enum section: unsigned char {
        part1 = 0,
        part64 = 63,
        copyPaste = 72, // 48 (not yet!)
        vector = 192, // CO
        midiLearn = 216, // D8
        midiIn,
        scales = 232, // E8
        main = 240, // F0
        systemEffects,
        insertEffects,
        bank = 244, // F4
        config = 248 // F8
    };

    namespace type {
        // bits 0, 1
        const unsigned char Adjust = 0; // return value adjusted within limits
        const unsigned char Read = 0; // i.e. !write
        const unsigned char Minimum = 1; // return this value
        const unsigned char Maximum = 2; // return this value
        const unsigned char Default = 3; // return this value
        // remaining used bit-wise
        const unsigned char Limits = 4;
        const unsigned char Error = 8;
        const unsigned char LearnRequest = 16;
        const unsigned char Learnable = 32;
        const unsigned char Write = 64;
        const unsigned char Integer = 128; // false = float
    }

    namespace action {
        // bits 0 to 3
        const unsigned char toAll = 0; // except MIDI
        const unsigned char fromMIDI = 1;
        const unsigned char fromCLI = 2;
        const unsigned char fromGUI = 3;
        const unsigned char noAction = 15; // internal use
        // remaining used bit-wise
        const unsigned char forceUpdate = 32;
        const unsigned char loop = 64; // internal use
        const unsigned char lowPrio = 128;
        const unsigned char muteAndLoop = 192;
    }

    enum control : unsigned char {
        // insert any new entries here
        textMessage = 254 // FE
    };

    enum muted : unsigned char {
        stopSound = 1,
        masterReset,
        patchsetLoad,
        vectorLoad,
        stateLoad,
        listLoad
    };

    // inserts are here as they are split between many
    // sections but must remain distinct.
    enum insert : unsigned char {
        LFOgroup = 0,
        filterGroup,
        envelopeGroup,
        envelopePoints, // this should be split in two
        envelopePointChange,
        oscillatorGroup, // 5
        harmonicAmplitude,
        harmonicPhaseBandwidth, // this should also be split in two
        resonanceGroup,
        resonanceGraphInsert,
        systemEffectSend = 16,
        partEffectSelect,
        kitGroup = 32
    };

    enum insertType : unsigned char {
        amplitude = 0,
        frequency,
        filter,
        bandwidth
    };

    enum XML : unsigned char { // file and history types
        Instrument = 0, // individual externally sourced Instruments
        Patch, //      full instrument Patch Sets
        Scale, //      complete Microtonal settings
        State, //      entire system State
        Vector, //     per channel Vector settings
        // insert any new lists here
        MLearn, //     learned MIDI CC lists
        Config, // only file types from here onwards
        Presets,
        Bank,
        History,
    };
}

namespace CONFIG // usage CONFIG::control::oscillatorSize
{
    enum control : unsigned char {
        oscillatorSize = 0,
        bufferSize,
        padSynthInterpolation,
        virtualKeyboardLayout,
        XMLcompressionLevel,
        reportsDestination,
        savedInstrumentFormat,
        showEnginesTypes,
        defaultStateStart = 16,
        hideNonFatalErrors,
        showSplash,
        logInstrumentLoadTimes,
        logXMLheaders,
        saveAllXMLdata,
        enableGUI,
        enableCLI,
        enableAutoInstance,
        enableSinglePath,
        historyLock,
        exposeStatus, // CLI only

        // start of engine controls
        jackMidiSource = 32,
        jackPreferredMidi,
        jackServer,
        jackPreferredAudio,
        jackAutoConnectAudio,
        alsaMidiSource = 48,
        alsaPreferredMidi,
        alsaAudioDevice,
        alsaPreferredAudio,
        alsaSampleRate,
        // end of engine controls
        addPresetRootDir = 60,
        removePresetRootDir,
        currentPresetRoot,
        bankRootCC = 65,
        bankCC = 67,
        enableProgramChange,
        instChangeEnablesPart,
        extendedProgramChangeCC = 71,
        ignoreResetAllCCs,
        logIncomingCCs,
        showLearnEditor,
        enableNRPNs,
        saveCurrentConfig = 80
    };
}

namespace BANK // usage BANK::control::
{
    enum control : unsigned char {
        selectInstrument = 0, // not yet
        findInstrumentName,
        renameInstrument, // not yet
        saveInstrument, // not yet
        deleteInstrument,
        selectFirstInstrumentToSwap,
        selectSecondInstrumentAndSwap,

        selectBank = 16, // not yet
        renameBank, // not yet
        saveBank, // not yet
        createBank, // not yet
        deleteBank, // not yet
        selectFirstBankToSwap,
        selectSecondBankAndSwap,
        importBank, // not yet
        exportBank, // not yet

        selectRoot = 32, // not yet
        changeRootId // not yet
    };
}

namespace VECTOR // usage VECTOR::control::name
{
    enum control : unsigned char {
        undefined = 0,
        name = 8,
        Xcontroller = 16,
        XleftInstrument,
        XrightInstrument,
        Xfeature0, // volume
        Xfeature1, // default panning
        Xfeature2, // default filter cutoff
        Xfeature3, // default modulation
        Ycontroller = 32,
        YupInstrument,
        YdownInstrument,
        Yfeature0, // volume
        Yfeature1, // default panning
        Yfeature2, // default filter cutoff
        Yfeature3, // default modulation
        erase = 96
    };
}

namespace COPYPASTE // usage COPYPASTE::control::toClipboard
{
    enum control : unsigned char {
        toClipboard = 0,
        toFile,
        fromClipboard,
        fromFile
    };
}

namespace MIDILEARN // usage MIDILEARN::control::block
{
    enum control : unsigned char {
        block = 0,
        limit,
        mute,
        nrpn, // auto
        sevenBit,
        minimum,
        maximum,
        ignoreMove,
        deleteLine,
        nrpnDetected,
        showGUI = 14,
        hideGUI,
        CCorChannel = 16, // should probably split these
        findSize = 20, // not used yet
        sendLearnMessage, // currently GUI only
        sendRefreshRequest, // currently GUI only
        reportActivity = 24,
        clearAll = 96,
        loadList = 241,
        loadFromRecent,
        saveList = 245,
        cancelLearn = 255
    };
}

namespace MIDI // usage MIDI::control::noteOn
{
    enum control : unsigned char {
        noteOn = 0,
        noteOff,
        controller,
        instrument = 7,
        bankChange = 8
    };
// the following are actual MIDI numbers
// not to be confused with part controls!
    enum CC : unsigned short int {
        bankSelectMSB = 0,
        modulation,
        breath,
        dataMSB = 6,
        volume,
        panning = 10,
        expression,
        bankSelectLSB = 32,
        dataLSB = 38,
        sustain = 64,
        portamento,
        legato = 68,
        filterQ = 71,
        filterCutoff = 74,
        bandwidth,
        fmamp,
        resonanceCenter,
        resonanceBandwidth,
        dataINC = 96,
        dataDEC,
        nrpnLSB,
        nrpnMSB,
        allSoundOff = 120,
        resetAllControllers,
        allNotesOff = 123,

        pitchWheelInner = 128,
        channelPressureInner,
        keyPressureInner,

        pitchWheel = 640,
        channelPressure,
        keyPressure,

        programchange = 999,
        null
    };
}

namespace SCALES // usage SCALES::control::Afrequency
{
    enum control : unsigned char {
        Afrequency = 0,
        Anote,
        invertScale,
        invertedScaleCenter,
        scaleShift,
        enableMicrotonal = 8,
        enableKeyboardMap = 16,
        lowKey,
        middleKey,
        highKey,
        tuning = 32,
        keyboardMap,
        importScl = 48,
        importKbm,
        name = 64,
        comment,
        retune = 80, // GUI only
        clearAll = 96
    };
}

namespace MAIN // usage MAIN::control::volume
{
    enum control : unsigned char {
        volume = 0,
        partNumber = 14,
        availableParts,
        detune = 32,
        keyShift = 35,
        mono,
        soloType = 48,
        soloCC,

        addNamedRoot = 56, // some of these should be in 'bank'
        delistRootId,
        changeRootId,
        exportBank,
        importBank,
        deleteBank,
        //addEmptyBank,
        //importInstrument,
        //deleteInstrument,

        setCurrentRootBank = 75,
        loadInstrumentFromBank,
        loadInstrumentByName,
        saveInstrument,
        saveNamedInstrument,
        loadNamedPatchset,
        saveNamedPatchset,
        loadNamedVector = 84,
        saveNamedVector,
        loadNamedScale = 88,
        saveNamedScale,
        loadNamedState = 92,
        saveNamedState,
        loadFileFromList,
        exportPadSynthSamples,

        masterReset = 96,
        masterResetAndMlearn,
        openManualPDF = 100,
        startInstance = 104,
        stopInstance,
        stopSound = 128,
        readPartPeak = 200,
        readMainLRpeak,
        readMainLRrms
    };
}

namespace PART // usage PART::control::volume
{
    enum control : unsigned char {
        volume = 0,
        velocitySense,
        panning,
        velocityOffset = 4,
        midiChannel,
        keyMode,
        portamento,
        enable,
        kitItemMute,
        minNote = 16,
        maxNote,
        minToLastKey,
        maxToLastKey,
        resetMinMaxKey,
        kitEffectNum = 24,
        maxNotes = 33,
        keyShift = 35,
        partToSystemEffect1 = 40,
        partToSystemEffect2,
        partToSystemEffect3,
        partToSystemEffect4,
        humanise = 48,
        humanvelocity,
        drumMode = 57,
        kitMode,
        effectNumber = 64,
        effectType,
        effectDestination,
        effectBypass,
        defaultInstrument = 96,
        padsynthParameters = 104,
        audioDestination = 120,

    // start of controllers
        volumeRange = 128,
        volumeEnable,
        panningWidth,
        modWheelDepth,
        exponentialModWheel,
        bandwidthDepth,
        exponentialBandwidth,
        expressionEnable,
        FMamplitudeEnable,
        sustainPedalEnable,
        pitchWheelRange,
        filterQdepth,
        filterCutoffDepth,
        breathControlEnable,
        resonanceCenterFrequencyDepth = 144,
        resonanceBandwidthDepth,
        portamentoTime = 160,
        portamentoTimeStretch,
        portamentoThreshold,
        portamentoThresholdType,
        enableProportionalPortamento,
        proportionalPortamentoRate,
        proportionalPortamentoDepth,
        receivePortamento = 168,
    // end of controllers

    // start of midi controls
        midiModWheel = 192,
        midiBreath,
        midiExpression,
        midiSustain,
        midiPortamento,
        midiFilterQ,
        midiFilterCutoff,
        midiBandwidth,
    // end of midi controls

        instrumentCopyright = 220,
        instrumentComments,
        instrumentName,
        defaultInstrumentCopyright, // this needs to be split into two for load/save
        resetAllControllers, // this needs to bump up 1 to make space
        partBusy = 252 // internally generated - read only
    };

    enum kitType : unsigned char {
        Off = 0,
        Multi,
        Single,
        CrossFade
    };

    enum engine : unsigned char {
        addSynth = 0,
        subSynth,
        padSynth,

    // addVoice and addMod must be consecutive
        addVoice1 = 128,
        addVoice2,
        addVoice3,
        addVoice4,
        addVoice5,
        addVoice6,
        addVoice7,
        addVoice8,
        addMod1 = 192,
        addMod2,
        addMod3,
        addMod4,
        addMod5,
        addMod6,
        addMod7,
        addMod8
    };
}

namespace ADDSYNTH // usage ADDSYNTH::control::volume
{
    enum control : unsigned char {
        volume = 0,
        velocitySense,
        panning,

        enable = 8,

        detuneFrequency = 32,
        octave = 35,
        detuneType, // L35 cents, L10 cents, E100 cents, E1200 cents
        coarseDetune,
        relativeBandwidth = 39,

        stereo = 112,
        randomGroup,

        dePop = 120,
        punchStrength,
        punchDuration,
        punchStretch,
        punchVelocity
    };
}

namespace ADDVOICE // usage ADDVOICE::control::volume
{
    enum control : unsigned char {
        volume = 0,
        velocitySense,
        panning,
        invertPhase = 4,
        enableAmplitudeEnvelope = 7,
        enableVoice,
        enableAmplitudeLFO,

        modulatorType = 16, // Off, Morph, Ring, PM, FM, PWM
        externalModulator, // -1 local,  'n' voice

        detuneFrequency = 32,
        equalTemperVariation,
        baseFrequencyAs440Hz,
        octave,
        detuneType, // Default, L35 cents, L10 cents, E100 cents, E1200 cents
        coarseDetune,
        pitchBendAdjustment,
        pitchBendOffset,

        enableFrequencyEnvelope = 40,
        enableFrequencyLFO,

        unisonFrequencySpread = 48,
        unisonPhaseRandomise,
        unisonStereoSpread,
        unisonVibratoDepth,
        unisonVibratoSpeed,
        unisonSize,
        unisonPhaseInvert, // None, Random, 50%, 33%, 25%, 20%
        enableUnison = 56,

        bypassGlobalFilter = 64, // TODO not seen on return?
        enableFilter = 68,
        enableFilterEnvelope = 72,
        enableFilterLFO,

        modulatorAmplitude = 80,
        modulatorVelocitySense,
        modulatorHFdamping,
        enableModulatorAmplitudeEnvelope = 88,
        modulatorDetuneFrequency = 96,
        modulatorDetuneFromBaseOsc,
        modulatorFrequencyAs440Hz,
        modulatorOctave,
        modulatorDetuneType, // Default, L35 cents, L10 cents, E100 cents, E1200 cents
        modulatorCoarseDetune,
        enableModulatorFrequencyEnvelope = 104,
        modulatorOscillatorPhase = 112,
        modulatorOscillatorSource, // -1 internal, 'n' external modulator

        delay = 128,
        enableResonance = 130, // for this voice
        voiceOscillatorPhase = 132,
        externalOscillator, // -1 local,  'n' voice
        voiceOscillatorSource, // - 1 internal, 'n' external voice
        soundType // Oscillator, White noise, Pink noise, Spot noise
    };
}

namespace SUBSYNTH // usage SUBSYNTH::control::volume
{
    enum control : unsigned char {
        volume = 0,
        velocitySense,
        panning,

        enable = 8,

        bandwidth = 16,
        bandwidthScale,
        enableBandwidthEnvelope,

        detuneFrequency = 32,
        equalTemperVariation,
        baseFrequencyAs440Hz,
        octave,
        detuneType, // L35 cents, L10 cents, E100 cents, E1200 cents
        coarseDetune,
        pitchBendAdjustment,
        pitchBendOffset,

        enableFrequencyEnvelope = 40,

        overtoneParameter1 = 48,
        overtoneParameter2,
        overtoneForceHarmonics,
        overtonePosition, // Harmonic, ShiftU, ShiftL, PowerU, PowerL, Sine, Power, Shift

        enableFilter = 64,
        filterStages = 80,
        magType, // Linear, -40dB, -60dB, -80dB, -100dB
        startPosition, // Zero, Random, Maximum
        clearHarmonics = 96,
        stereo = 112
    };
}

namespace PADSYNTH // usage PADSYNTH::control::volume
{
    enum control : unsigned char {
        volume = 0,
        velocitySense,
        panning,

        enable = 8,

        bandwidth = 16,
        bandwidthScale,
        spectrumMode = 19, // Bandwidth, Discrete, Continuous

        detuneFrequency = 32,
        equalTemperVariation,
        baseFrequencyAs440Hz,
        octave,
        detuneType, // L35 cents, L10 cents, E100 cents, E1200 cents
        coarseDetune,
        pitchBendAdjustment,
        pitchBendOffset,

        overtoneParameter1 = 48,
        overtoneParameter2,
        overtoneForceHarmonics,
        overtonePosition, // Harmonic, ShiftU, ShiftL, PowerU, PowerL, Sine, Power, Shift

        baseWidth = 64,
        frequencyMultiplier,
        modulatorStretch,
        modulatorFrequency,
        size,
        baseType, // Gauss, Square, Double Exponential
        harmonicSidebands, // Full, Upper half, Lower half
        spectralWidth,
        spectralAmplitude,
        amplitudeMultiplier, // Off, Gauss, Sine, Flat
        amplitudeMode, // Sum, Multiply, Divide 1, Divide 2
        autoscale,

        harmonicBase = 80, // C-2, G-2, C-3, G-3, C-4, G-4, C-5, G-5, G-6
        samplesPerOctave, // 0.5, 1, 2, 3, 4, 6, 12
        numberOfOctaves, // 1 - 8
        sampleSize, // 16k, 32k, 64k, 128k, 256k, 512k, 1M
        applyChanges = 104,
        stereo = 112,

        dePop = 120,
        punchStrength,
        punchDuration,
        punchStretch,
        punchVelocity
    };
}

namespace OSCILLATOR // usage OSCILLATOR::control::phaseRandomness
{
    enum control : unsigned char {
        phaseRandomness = 0,
        magType, // Linear, -40dB, -60dB, -80dB, -100dB
        harmonicAmplitudeRandomness,
        harmonicRandomnessType, // None, Power, Sine

        baseFunctionParameter = 16,
        baseFunctionType, // Sine, Triangle, Pulse, Saw, Power, Gauss, Diode, AbsSine,
            // PulseSine, StrchSine, Chirp, AbsStrSine, Chebyshev, Sqr, Spike, Circle
        baseModulationParameter1 = 18,
        baseModulationParameter2,
        baseModulationParameter3,
        baseModulationType, // None, Rev, Sine, Pow

        autoClear = 32, // not used
        useAsBaseFunction, // if 'value' is 1 assume autoclear set
        waveshapeParameter,
        waveshapeType, //  None, Atan, Asym1, Pow, Sine Qnts, Zigzag, Lmt, LmtU, LmtL, Ilmt, Clip, Asym2, Pow2, Sgm
        filterParameter1,
        filterParameter2,
        filterBeforeWaveshape,
        filterType, // None, LP, HP1a, HP1b, BP1, BS1, LP2, HP2, BP2, BS2, Cos, Sin, Lsh, Sgm
        modulationParameter1,
        modulationParameter2,
        modulationParameter3,
        modulationType, // None, Rev, Sine, Pow
        spectrumAdjustParameter,
        spectrumAdjustType, // None, Pow, ThrsD, ThrsU

        harmonicShift = 64,
        clearHarmonicShift,
        shiftBeforeWaveshapeAndFilter,
        adaptiveHarmonicsParameter,
        adaptiveHarmonicsBase,
        adaptiveHarmonicsPower,
        adaptiveHarmonicsType, // Off, On, Square, 2xSub, 2xAdd, 3xSub, 3xAdd, 4xSub, 4xAdd

        clearHarmonics = 96,
        convertToSine
    };
}

namespace RESONANCE // usage RESONANCE::control::maxDb
{
    enum control : unsigned char {
        maxDb = 0,
        centerFrequency,
        octaves,
        enableResonance = 8,
        randomType = 10, // coarse, medium, fine
        interpolatePeaks = 20, // smooth, linear
        protectFundamental,
        clearGraph = 96,
        smoothGraph,
        graphPoint
    };
}

namespace LFOINSERT // usage LFOINSERT::control::speed
{
    enum control : unsigned char {
        speed = 0,
        depth,
        delay,
        start,
        amplitudeRandomness,
        type, // Sine, Tri, Sqr, R.up, R.dn, E1dn, E2dn
        continuous,
        frequencyRandomness,
        stretch
    };
}

namespace FILTERINSERT // usage FILTERINSERT::control::centerFrequency
{
    enum control : unsigned char {
        centerFrequency = 0,
        Q,
        frequencyTracking,
        velocitySensitivity,
        velocityCurve,
        gain,
        stages, // x1, x2, x3, x4, x5
        baseType, // analog, formant, state variable
        analogType, // LPF1, HPF1, LPF2, HPF2, BPF2, NF2, PkF2, LSh2, HSh2
        stateVariableType, // LPF, HPF, BPF, NF
        frequencyTrackingRange,
        formantSlowness = 16,
        formantClearness,
        formantFrequency,
        formantQ,
        formantAmplitude,
        formantStretch,
        formantCenter,
        formantOctave,
        numberOfFormants = 32,
        vowelNumber, // local to GUI
        formantNumber, // local to GUI
        sequenceSize,
        sequencePosition, // local to GUI
        vowelPositionInSequence,
        negateInput, // bypass LFOs, envelopes etc.
    };
}

namespace ENVELOPEINSERT // usage ENVELOPEINSERT::control::attackLevel
{
    enum control : unsigned char {
        attackLevel = 0,
        attackTime,
        decayLevel,
        decayTime,
        sustainLevel,
        releaseTime,
        releaseLevel,
        stretch,
        forcedRelease = 16,
        linearEnvelope,
        edit = 24, // local to GUI

        enableFreeMode = 32,
        points = 34, // local to GUI
        sustainPoint
    };
}

namespace EFFECT // usage EFFECT::type::none
{
    enum type : unsigned char {
        none = 128, // must be higher than normal kits
        reverb,
        echo,
        chorus,
        phaser,
        alienWah,
        distortion,
        eq,
        dynFilter
    };

    enum control : unsigned char {
        level = 0, // volume, wet/dry, gain for EQ
        panning, // band for EQ
        frequency, // time reverb, delay echo, L/R-mix dist, Not EQ
        preset = 16, // not EQ
        changed = 129 // not EQ
    };

    enum sysIns : unsigned char {
        toEffect1 = 1, // system only
        toEffect2, // system only
        toEffect3, // system only
        effectNumber,
        effectType,
        effectDestination, // insert only
        effectEnable // system only
    };
}

union CommandBlock{
    struct{
        union{
            float F;
            int32_t I;
        } value;
        unsigned char type;
        unsigned char source;
        unsigned char control;
        unsigned char part;
        unsigned char kit;
        unsigned char engine;
        unsigned char insert;
        unsigned char parameter;
        unsigned char offset;
        unsigned char miscmsg;
        unsigned char spare1;
        unsigned char spare0;
    } data;
    char bytes [sizeof(data)];
};
/*
 * it is ESSENTIAL that this is a power of 2
 */
const size_t commandBlockSize = sizeof(CommandBlock);

#endif
