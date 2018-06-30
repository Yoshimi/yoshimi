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
#define XML_PRESETS 4
#define XML_STATE 5
#define XML_CONFIG 6
#define XML_BANK 7
#define XML_HISTORY 8
#define XML_VECTOR 9
#define XML_MIDILEARN 10
#define NO_MSG 255

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

namespace topLevel // usage topLevel::section::vector
{
    enum section: unsigned char {
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

    // the following critcally cannot be changed.
    enum route : unsigned char {
        adjust = 64,
        lowPriority = 128,
        adjustAndLoopback = 192
    };

    enum control : unsigned char {
        errorMessage = 254 // FE
    };

    // inserts are here as they are split between many sections
    // but must remain distinct.
    enum insert : unsigned char {
        LFOgroup = 0,
        filterGroup,
        envelopeGroup,
        envelopePoints,
        envelopePointChange,
        oscillatorGroup, // 5
        harmonicAmplitude,
        harmonicPhaseBandwidth,
        resonanceGroup,
        resonanceGraphInsert,
        systemEffectSend = 16,
        kitGroup = 32 // 20
    };
}

namespace configLevel // usage configLevel::control::oscillatorSize
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

namespace partLevel // usage partLevel::control::volume
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
        effectNum = 64,
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
        partBusy = 252}; // internally generated - read only

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
        addMod8};
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
