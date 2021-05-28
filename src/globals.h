/*
    globals.h - general static definitions

    Copyright 2018-2021, Will Godfrey

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
#include <string>

/*
 * For test purposes where you want guaranteed identical results, enable the
 * #define below.
 * Be aware this does strange things to both subSynth and padSynth as they
 * actually *require* randomness to produce normal sounds.
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

// many of the following are for convenience and consistency
// changing them is likely to have unpredicable consequences

// sizes
#define COMMAND_SIZE 252
#define MAX_HISTORY 25
#define MAX_PRESETS 1000
#define MAX_PRESET_DIRS 128
#define MAX_BANK_ROOT_DIRS 128
#define MAX_BANKS_IN_ROOT 128
#define MAX_INSTRUMENTS_IN_BANK 160
#define MAX_AD_HARMONICS 128
#define MAX_SUB_HARMONICS 64
#define PAD_MAX_SAMPLES 96
#define NUM_MIDI_PARTS 64
#define PART_NORMAL 0
#define PART_MONO 1
#define PART_LEGATO 2
#define MIDI_NOT_LEGATO 3
#define MIDI_LEGATO 4
#define NUM_MIDI_CHANNELS 16
#define MIDI_LEARN_BLOCK 400
#define MAX_ENVELOPE_POINTS 40
#define MIN_ENVELOPE_DB -60
#define MAX_RESONANCE_POINTS 256
#define MAX_KEY_SHIFT 36
#define MIN_KEY_SHIFT -36
#define A_MIN 30.0f
#define A_DEF 440.0f
#define A_MAX 1100.0f

// The number of discrete steps we use for the LFO BPM frequency. Make sure to
// update LFO_BPM_LCM as well, if this is updated.
#define LFO_BPM_STEPS 33
// The Least Common Multiple of all the possible LFO fractions.
#define LFO_BPM_LCM 720720

#define MIN_OSCIL_SIZE 256 // MAX_AD_HARMONICS * 2
#define MAX_OSCIL_SIZE 16384
#define MIN_BUFFER_SIZE 16
#define MAX_BUFFER_SIZE 8192
#define NO_MSG 255 // these two may become different
#define UNUSED 255

// GUI colours
#define ADD_COLOUR 0xdfafbf00
#define BASE_COLOUR 0xbfbfbf00
#define SUB_COLOUR 0xafcfdf00
#define PAD_COLOUR 0xcfdfaf00
#define YOSHI_COLOUR 0x0000e100
#define EXTOSC_COLOUR 0x8fbfdf00
#define EXTVOICE_COLOUR 0x9fdf8f00
#define MODOFF_COLOUR 0x80808000

// these were previously (pointlessly) user configurable
#define NUM_VOICES 8
#define POLIPHONY 60
#define PART_DEFAULT_LIMIT 20
#define NUM_SYS_EFX 4
#define NUM_INS_EFX 8
#define NUM_PART_EFX 3
#define NUM_KIT_ITEMS 16
#define FADEIN_ADJUSTMENT_SCALE 20
#define MAX_EQ_BANDS 8  // MAX_EQ_BANDS must be less than 20
#define MAX_FILTER_STAGES 5
#define FF_MAX_VOWELS 6
#define FF_MAX_FORMANTS 12
#define FF_MAX_SEQUENCE 8

#define DEFAULT_NAME "Simple Sound"
#define UNTITLED "No Title"

#define DEFAULT_AUDIO alsa_audio
#define DEFAULT_MIDI alsa_midi

#define FORCED_EXIT 16

namespace _SYS_
{
    // float to bool done this way to ensure consistency
    // we are always using positive values
    inline bool F2B(float value) {return value > 0.5f;}

    enum mute {Idle, Pending, Fading, Active, Complete, Request, Immediate};

    enum type {Normal, Default, JackFirst, JackSecond, StartupFirst, StartupSecond, InProgram, RestoreConf};
}

/*
 * for many of the following, where they are in groups the
 * group order must not change, but the actual values can
 * and new entries can be added between the group ends
 *
 * *** WARNING ***
 *
 * The above is no longer completely true!
 * Controller numbers in particular must not change if
 * these might be recorded in MIDI-learn files. The same
 * applies to voice numbers in 'engine' - use existing
 * gaps for new controls instead.
 *
 * Generally any controls/features that can't be learned
 * should be safe to move.
 */

namespace TOPLEVEL // usage TOPLEVEL::section::vector
{
    enum section : unsigned char {
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
        config = 248, // F8
        message = 250 // FA
    };

    namespace type {
        enum {
            // bits 0, 1 as values
            Adjust = 0, // return value adjusted within limits
            Minimum, // return this value
            Maximum, // return this value
            Default, // return this value
            // remaining used bit-wise
            Limits, // read limits shown above
            Error = 8,
            LearnRequest = 16,
            Learnable = 32,
            Write = 64,
            Integer = 128 // false = float
        };
    }

    namespace action {
        enum {
            // bits 0 to 3
            toAll = 0, // except MIDI
            fromMIDI,
            fromCLI,
            fromGUI,
            // space for any other sources
            noAction = 15, // internal use
            // remaining used bit-wise
            forceUpdate = 32,
            loop = 64, // internal use
            lowPrio = 128,
            muteAndLoop = 192
        };
    }

    enum control : unsigned char {
        // insert any new entries here

        /*
         * the following values must never appear in any other sections
         */
        textMessage = 254, // FE
        forceExit // this is effective from *any* section!
    };

    enum msgResponse : unsigned char {
        refreshBankDefaults,
        cancelBankDefaults,
        cancelMidiLearn
        // any other value = no response
        // but there may still be a message
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
        MLearn, //     learned MIDI CC lists
        Presets, //    parts of instruments or effects

        // not XML but there for consistency
        PadSample,
        ScalaTune,
        ScalaMap,
        Dir, // for filer, any directory request

        // only file types from here onwards
        Config,
        MasterConfig,
        Bank,
        History
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
        enableSinglePath,
        enableHighlight, // in banks
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
        alsaMidiType,
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
        // instrument selection done in 'part'
        // actual control should probably be here
        readInstrumentName = 0, // in bank, by ID
        findInstrumentName, // next in list or '*' if at end
        renameInstrument, // in bank
        saveInstrument, // to bank
        deleteInstrument, // from bank
        selectFirstInstrumentToSwap,
        selectSecondInstrumentAndSwap,
        lastSeenInBank,

        selectBank = 16, // in root, by ID or read ID + name
        renameBank, // or read just the name
        createBank,
        deleteBank, // not yet - currently 'remove' at top level
        findBankSize,
        selectFirstBankToSwap,
        selectSecondBankAndSwap,
        importBank, // not yet (currently done in main)
        exportBank, // not yet (currently done in main)

        selectRoot = 32, // by ID - also reads the current one
        changeRootId, // change ID of current root
        addNamedRoot, // link or create if not already there
        deselectRoot, // remove reference, but don't touch contents
        installBanks,
        refreshDefaults
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
        cancelLearn = 250,
        learned
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
        soloType,
        soloCC,

        pitchWheel = 640,
        channelPressure,
        keyPressure,

        programchange = 999,

        maxNRPN = 0x7fff,
        identNRPN = 0x8000,
        null
    };

    enum SoloType : unsigned char {
        Disabled = 0,
        Row,
        Column,
        Loop,
        TwoWay,
        Channel
    };
}

namespace SCALES // usage SCALES::control::refFrequency
{
    enum control : unsigned char {
        enableMicrotonal = 0,
        refFrequency,
        refNote,
        invertScale,
        invertedScaleCenter,
        scaleShift,

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
        mono = 0,
        volume,
        partNumber = 14,
        availableParts,
        panLawType,
        detune = 32,
        keyShift = 35,
        reseed = 40,
        soloType = 48,
        soloCC,

        exportBank = 60, // some of these should be in 'bank'
        importBank,
        deleteBank,

        loadInstrumentFromBank = 76,
        loadInstrumentByName,
        saveNamedInstrument,
        loadNamedPatchset,
        saveNamedPatchset,
        loadNamedVector = 84,
        saveNamedVector,
        loadNamedScale = 88,
        saveNamedScale,
        loadNamedState = 92,
        saveNamedState,
        readLastSeen,
        loadFileFromList,
        defaultPart,
        exportPadSynthSamples,
        masterReset,
        masterResetAndMlearn,
        openManualPDF = 100,
        startInstance = 104,
        stopInstance,
        stopSound = 128,
        readPartPeak = 200, // now does L/R
        readMainLRpeak,
        readMainLRrms
    };

    enum panningType : unsigned char {
        cut = 0,
        normal,
        boost
    };

}

namespace PART // usage PART::control::volume
{
    enum control : unsigned char {
        enable = 0,
        enableAdd,
        enableSub,
        enablePad,
        enableKitLine,
        volume,
        velocitySense,
        panning,
        velocityOffset,
        midiChannel,
        keyMode,
        channelATset,
        keyATset,
        portamento,
        kitItemMute,
        minNote,
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
        midiFMamp,
        midiResonanceCenter,
        midiResonanceBandwidth,
    // end of midi controls

        instrumentCopyright = 220,
        instrumentComments,
        instrumentName,
        instrumentType,
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

    namespace aftertouchType {
        enum {  // all powers of 2 handled bit-wise
            off = 0,
            filterCutoff,
            filterCutoffDown,
            filterQ = 4,
            filterQdown = 8,
            pitchBend = 16,
            pitchBendDown = 32,
            volume = 64,
            modulation = 128 // this MUST be the highest bit
        };
    }

    namespace envelope
    {
        enum groupmode : int {
            amplitudeLin = 1,
            amplitudeLog,
            frequency,
            filter,
            bandwidth
        };
    }
}

namespace ADDSYNTH // usage ADDSYNTH::control::volume
{
    enum control : unsigned char {
        volume = 0,
        velocitySense,
        panning,
        enableRandomPan,
        randomWidth,

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
        enableVoice = 0,
        volume,
        velocitySense,
        panning,
        enableRandomPan,
        randomWidth,
        invertPhase,
        enableAmplitudeEnvelope,
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
        enableRandomPan,
        randomWidth,

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
        enableRandomPan,
        randomWidth,

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
    enum wave : unsigned char {
        sine = 0,
        triangle,
        pulse,
        saw,
        power,
        gauss,
        diode,
        absSine,
        pulseSine,
        stretchSine,
        chirp,
        absStretchSine,
        chebyshev,
        square,
        spike,
        circle,
        hyperSec,
        user
    };
}

namespace RESONANCE // usage RESONANCE::control::maxDb
{
    enum control : unsigned char {
        enableResonance = 0,
        maxDb,
        centerFrequency,
        octaves,

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
        bpm,
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
        dynFilter,
        count // this must be the last item!
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

/*
 * it is ESSENTIAL that the size is a power of 2
 */
union CommandBlock{
    struct{
        float value;
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

#endif
