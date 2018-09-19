/*
    globals.h - general static definitions

    Copyright 2018, Will Godfrey

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

    Created June 2018
*/

#ifndef GLOBALS_H
#define GLOBALS_H

// math
#define PI 3.1415926536f
#define TWOPI 6.28318530718f
#define HALFPI 1.57079632679f
#define LOG_2 0.693147181f

// many of the following are for convenience and consistency
// changing them is likely to have unpredicable consequences

// sizes
#define COMMAND_SIZE 80
#define MAX_HISTORY 25
#define MAX_PRESETS 1000
#define MAX_PRESET_DIRS 128
#define MAX_BANK_ROOT_DIRS 128
#define MAX_BANKS_IN_ROOT 128
#define MAX_AD_HARMONICS 128
#define MAX_SUB_HARMONICS 64
#define PAD_MAX_SAMPLES 96
#define NUM_MIDI_PARTS 64
#define NUM_MIDI_CHANNELS 16
#define MIDI_LEARN_BLOCK 200
#define MAX_ENVELOPE_POINTS 40
#define MIN_ENVELOPE_DB -60
#define MAX_RESONANCE_POINTS 256
#define MAX_KEY_SHIFT 36
#define MIN_KEY_SHIFT -36
#define NO_MSG 255 // these three may become different
#define UNUSED 255
#define NO_ACTION 255

// GUI colours
#define ADD_COLOUR 0xdfafbf00
#define BASE_COLOUR 0xbfbfbf00
#define SUB_COLOUR 0xafcfdf00
#define PAD_COLOUR 0xcfdfaf00
#define YOSHI_COLOUR 0x0000e100

// XML types
#define XML_INSTRUMENT 1
#define XML_PARAMETERS 2
#define XML_MICROTONAL 3
#define XML_STATE 4
#define XML_VECTOR 5
#define XML_MIDILEARN 6
#define XML_CONFIG 7
#define XML_PRESETS 8
#define XML_BANK 9
#define XML_HISTORY 10

// these were previously (pointlessly) user configurable
#define NUM_VOICES 8
#define POLIPHONY 80
#define NUM_SYS_EFX 4
#define NUM_INS_EFX 8
#define NUM_PART_EFX 3
#define NUM_KIT_ITEMS 16
#define VELOCITY_MAX_SCALE 8.0f
#define FADEIN_ADJUSTMENT_SCALE 20
#define MAX_EQ_BANDS 8  // MAX_EQ_BANDS must be less than 20
#define MAX_FILTER_STAGES 5
#define FF_MAX_VOWELS 6
#define FF_MAX_FORMANTS 12
#define FF_MAX_SEQUENCE 8
#define MAX_PHASER_STAGES 12
#define MAX_ALIENWAH_DELAY 100

/*
 * for many of the following, where they are in groups the
 * group order must not change, but the actual values can
 * and new entries can be added between the group ends
 */

namespace TOPLEVEL // usage TOPLEVEL::section::vector
{
    enum section: unsigned char {
        part1 = 0,
        part64 = 63,
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

    // this pair critcally cannot be changed as
    // they rely on 'parameter' being < 64
    enum route : unsigned char {
        lowPriority = 128,
        adjustAndLoopback = 192
    };

    // bit-wise type and source share the same byte
    // but will eventually be split up
    enum type : unsigned char {
        // bits 0, 1
        Adjust = 0,
        Read = 0,
        Minimum,
        Maximum,
        Default,
        LearnRequest = 3,
        // remaining used bit-wise
        Error = 4, // also identifes static limits
        Limits = 4, // yes we can pair these - who knew?
        Write = 64, // false = read
        Learnable = 64, // shared value
        Integer = 128 // false = float
    };

    enum source : unsigned char {
        // all used bit-wise
        MIDI = 8,
        CLI = 16,
        UpdateAfterSet = 16, // so gui can update
        GUI = 32
    };

    enum control : unsigned char {
        errorMessage = 254 // FE
    };

    enum muted : unsigned char {
        stopSound = 1,
        masterReset,
        patchsetLoad,
        vectorLoad,
        stateLoad
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
        defaultStateStart = 16,
        hideNonFatalErrors,
        showSplash,
        logInstrumentLoadTimes,
        logXMLheaders,
        saveAllXMLdata,
        enableGUI,
        enableCLI,
        enableAutoInstance,
        exposeStatus,

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

        //enableBankRootChange = 64,
        bankRootCC = 65,
        bankCC = 67,
        enableProgramChange,
        programChangeEnablesPart,
        //enableExtendedProgramChange,
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
        renameInstrument, // not yet
        saveInstrument, // not yet
        deleteInstrument, // not yet
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

// the following are actual MIDI numbers
// not to be confused with part controls!
namespace MIDI // usage MIDI::control::noteOn
{
    enum control : unsigned char {
        noteOn = 0,
        noteOff,
        controller,
        programChange = 8// also bank and root - split?
    };
    enum function : unsigned char {
        modulation = 1,
        volume = 7,
        panning = 10,
        expression = 11,
        legato = 68,
        filterQ = 71,
        filterCutoff = 74,
        bandwidth
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

        setCurrentRootBank = 73,
        loadInstrument,
        saveInstrument,
        loadNamedInstrument = 78,
        saveNamedInstrument,
        loadNamedPatchset,
        saveNamedPatchset,
        loadNamedVector = 84,
        saveNamedVector,
        loadNamedScale = 88,
        saveNamedScale,
        loadNamedState = 92,
        saveNamedState,
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
        externalModulator,

        detuneFrequency = 32,
        equalTemperVariation,
        baseFrequencyAs440Hz,
        octave,
        detuneType, // L35 cents, L10 cents, E100 cents, E1200 cents
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
        modulatorFrequencyAs440Hz = 98,
        modulatorOctave,
        modulatorDetuneType, // L35 cents, L10 cents, E100 cents, E1200 cents
        modulatorCoarseDetune,
        enableModulatorFrequencyEnvelope = 104,
        modulatorOscillatorPhase = 112,
        modulatorOscillatorSource, // -1 local, 'n' external

        delay = 128,
        enableResonance = 130, // for this voice
        voiceOscillatorPhase = 136,
        voiceOscillatorSource, // - 1 local, 'n' external
        soundType // Oscillator, White noise, Pink noise
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
        waveshapeType, //  None, Atan, Asym1, Pow, Sine Qnts, Zigzag, Lmt, LmtU, LmtL, Ilmt
        filterParameter1,
        filterParameter2,
        filterBeforeWaveshape,
        filterType, // None, LP, HP1a, HP1b, BP1, BS1, LP2, HP2, BP2, BS2, Cos, Sin, Lsh, S
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
        smoothGraph
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
        dynFilter = 136 // this actually uses the kititem byte
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

    enum sysIns : unsigned char {
        toEffect1 = 1, // system only
        toEffect2, // system only
        toEffect3, // system only
        effectNumber,
        effectType,
        effectDestination // insert only
    };
}

union CommandBlock{
    struct{
        float value;
        unsigned char type;
        unsigned char control;
        unsigned char part;
        unsigned char kit;
        unsigned char engine;
        unsigned char insert;
        unsigned char parameter;
        unsigned char par2;
    } data;
    char bytes [sizeof(data)];
};

#endif
