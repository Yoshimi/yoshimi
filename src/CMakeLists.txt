#
#   CMakeLists.txt
#
#   Copyright 2009-2010, Alan Calvert
#
#   This file is part of yoshimi, which is free software: you can
#   redistribute it and/or modify it under the terms of the GNU General
#   Public License as published by the Free Software Foundation, either
#   version 3 of the License, or (at your option) any later version.
#
#   yoshimi is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

project(Yoshimi)
cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)
cmake_policy(SET CMP0004 OLD)
set(YOSHIMI_VERSION "0.058.1")
file(WRITE version.txt "${YOSHIMI_VERSION}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_USE_RELATIVE_PATHS OFF)
    # relative paths break some scripts(?)
set(CMAKE_SKIP_RULE_DEPENDENCY OFF)
    # Rebuild objects if rules have changed, even if source hasn't.

# User options
set(DEFAULT_AUDIO jack CACHE STRING "Default audio driver - alsa or jack")
set(DEFAULT_MIDI jack CACHE STRING "Default midi driver - alsa or jack")
option(BUILD_X86_64 "Build for x86_64 system" OFF)
option(USE_ASM_F21 "Use assembler FLOAT to INT conversions" ON)
option(DEBUG_BUILD "Include gdb debug support" OFF)
set(YOSHI_FIFO_DIR /dev/shm CACHE STRING "Directory for fifos")

set(BUILD_X86_64_OPTS
    # "-march=native
    "-O3 -march=athlon64 -m64 -Wall -ffast-math -fno-finite-math-only -fomit-frame-pointer"
    CACHE STRING "X86_64 complier options"
)
set(BUILD_OPTS
    "-O3 -march=native -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer"
    CACHE STRING "X86 complier options"
)

set(BUILD_DEBUG_OPTS "-O0 -g3 -ggdb -Wall -Wpointer-arith"
    CACHE STRING "Debug build flags")

# Zyn parameters
set(ZynMaxAdHarmonics 128 CACHE STRING
    "number of harmonics of additive synth - must be less than half Oscilsize" FORCE)
add_definitions(-DMAX_AD_HARMONICS=${ZynMaxAdHarmonics})

set(ZynMaxSubHarmonics 64 CACHE STRING "number of harmonics of substractive" FORCE)
add_definitions(-DMAX_SUB_HARMONICS=${ZynMaxSubHarmonics})

set(ZynMaxPadSamples 64 CACHE STRING "maximum samples per PADsynth instrument (or item)" FORCE)
add_definitions(-DPAD_MAX_SAMPLES=${ZynMaxPadSamples})

set(ZynMidiParts 16 CACHE STRING "maximum number of midi parts" FORCE)
add_definitions(-DNUM_MIDI_PARTS=${ZynMidiParts})

set(ZynMidiChannels 16 CACHE STRING "maximum number of midi channels" FORCE)
add_definitions(-DNUM_MIDI_CHANNELS=${ZynMidiChannels})

set(ZynNumVoices 8 CACHE STRING "number of voices of additive synth for a single note" FORCE)
add_definitions(-DNUM_VOICES=${ZynNumVoices})

set(ZynPoliphony 60 CACHE STRING "The poliphony (notes)" FORCE)
add_definitions(-DPOLIPHONY=${ZynPoliphony})

set(ZynNumSysEfx 4 CACHE STRING "Number of system effects" FORCE)
add_definitions(-DNUM_SYS_EFX=${ZynNumSysEfx})

set(ZynNumInsertionEfx 8 CACHE STRING "Number of insertion effects" FORCE)
add_definitions(-DNUM_INS_EFX=${ZynNumInsertionEfx})

set(ZynNumPartEfx 3 CACHE STRING "Number of part effects" FORCE)
add_definitions(-DNUM_PART_EFX=${ZynNumPartEfx})

set(ZynNumKitItems 16 CACHE STRING "Maximum number of instruments per part" FORCE)
add_definitions(-DNUM_KIT_ITEMS=${ZynNumKitItems})

set(ZynVelocityMaxScale 8.0 CACHE STRING "How is applied the velocity sensing" FORCE)
add_definitions(-DVELOCITY_MAX_SCALE=${ZynVelocityMaxScale})

set(ZynMaxEqBands 8 CACHE STRING "maximum number of bands of the equaliser" FORCE)
if (${ZynMaxEqBands} GREATER 19)
    message(FATAL_ERROR "ZynMaxEqBands too high, must be less than 20")
endif (${ZynMaxEqBands} GREATER 19)
add_definitions(-DMAX_EQ_BANDS=${ZynMaxEqBands})

set(ZynMaxFilterStages 5 CACHE STRING "Maximum filter stages" FORCE)
add_definitions(-DMAX_FILTER_STAGES=${ZynMaxFilterStages})

set(ZynMaxVowels 6 CACHE STRING "Maximum formant filter vowels" FORCE)
add_definitions(-DFF_MAX_VOWELS=${ZynMaxVowels})

set(ZynMaxFormants 12 CACHE STRING "Maximum formant filter formants" FORCE)
add_definitions(-DFF_MAX_FORMANTS=${ZynMaxFormants})

set(ZynMaxSequence 8 CACHE STRING "Maximum formant filter sequence" FORCE)
add_definitions(-DFF_MAX_SEQUENCE=${ZynMaxSequence})
add_definitions(-DPI=3.1415926536f -DLOG_2=0.693147181f)


mark_as_advanced(EXECUTABLE_OUTPUT_PATH)
mark_as_advanced(LIBRARY_OUTPUT_PATH)
mark_as_advanced(CMAKE_BUILD_TYPE)
mark_as_advanced(CMAKE_INSTALL_PREFIX)

set(PRELUDE "Building Yoshimi version ${YOSHIMI_VERSION} for ${CMAKE_SYSTEM_NAME}")

if(DEBUG_BUILD)
    set(CMAKE_BUILD_TYPE "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG ${BUILD_DEBUG_OPTS})
    message(STATUS "${PRELUDE} with gdb debug information")
    message(STATUS "Debug build flags: ${CMAKE_CXX_FLAGS_DEBUG}")
else(DEBUG_BUILD)
    set(CMAKE_BUILD_TYPE "Release")
    if(BUILD_X86_64)
        set(CMAKE_CXX_FLAGS_RELEASE ${BUILD_X86_64_OPTS})
    else(BUILD_X86_64)
        set(CMAKE_CXX_FLAGS_RELEASE ${BUILD_OPTS})
    endif(BUILD_X86_64)
    message(STATUS "${PRELUDE} without debug information")
    message(STATUS "Build flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif(DEBUG_BUILD)

# library dependency checks
# PKG-CONFIG
find_package(PkgConfig REQUIRED)
if(PKG_CONFIG_FOUND)
    message(STATUS "Found pkg-config ${PKG_CONFIG_EXECUTABLE}")
else(PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config required but not found")
endif(PKG_CONFIG_FOUND)

# libz
include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES z)
check_c_source_compiles(
    "#include <zlib.h>
     int main(int argc, char **argv) {
         gzFile zzz  = gzopen(\"/dev/null\", \"rb\");
         if (NULL != zzz)
            gzclose(zzz);
         return 0;
     }" HAS_LIBZ
)
if(HAS_LIBZ)
    message(STATUS "Found libz")
    set(YOSHI_LIBRARIES ${YOSHI_LIBRARIES} z)
else(HAS_LIBZ)
    message(FATAL_ERROR "libz required but not found: ${HAS_LIBZ}")
endif(HAS_LIBZ)

# fontconfig
pkg_check_modules(FONTCONFIG REQUIRED fontconfig>=0.22)
mark_as_advanced(FONTCONFIG_LIBRARIES)
if(FONTCONFIG_FOUND)
    message(STATUS "Found fontconfig ${FONTCONFIG_VERSION}")
else(FONTCONFIG_FOUND)
    message(FATAL_ERROR "fontconfig>=0.22 required but not found")
endif(FONTCONFIG_FOUND)

# fltk
find_package(FLTK REQUIRED)
if(FLTK_FOUND)
    message(STATUS "Found fltk")
else(FLTK_FOUND)
    message(FATAL_ERROR "FLTK required but not found")
endif(FLTK_FOUND)
mark_as_advanced(FLTK_DIR)
mark_as_advanced(FLTK_FLUID_EXECUTABLE)
mark_as_advanced(FLTK_MATH_LIBRARY)
set (FLTK_sources
   UI/WidgetPDialUI.fl  UI/PresetsUI.fl  UI/EnvelopeUI.fl
   UI/LFOUI.fl  UI/FilterUI.fl  UI/VirKeyboardUI.fl
   UI/ConfigUI.fl  UI/SUBnoteUI.fl  UI/ResonanceUI.fl
   UI/OscilGenUI.fl  UI/ADnoteUI.fl  UI/PADnoteUI.fl
   UI/EffUI.fl  UI/BankUI.fl  UI/PartUI.fl
   UI/MicrotonalUI.fl  UI/MasterUI.fl  UI/MasterMiscUI.fl
   UI/GuiThreadUI.fl UI/ParametersUI.fl UI/ConsoleUI.fl
)
fltk_wrap_ui(yoshimi ${FLTK_sources})
set_source_files_properties(UI/MasterUI.h PROPERTIES GENERATED 1)
set(YOSHI_INCLUDES ${FLTK_INCLUDE_DIR})
set(YOSHI_SOURCES ${yoshimi_FLTK_UI_SRCS})
set(YOSHI_LIBRARIES ${YOSHI_LIBRARIES} ${FLTK_LIBRARIES})

# fftw3
pkg_check_modules(FFTW3 REQUIRED fftw3>=0.22)
if(FFTW3_FOUND)
    set(FFTW3_LIBRARIES "${FFTW3_LIBRARIES};fftw3_threads;pthread")
    message(STATUS "Found fftw3 ${FFTW3_VERSION}")
else(FFTW3_FOUND)
    message(FATAL_ERROR "fftw3 >=0.22 required but not found")
endif(FFTW3_FOUND)

# mxml
pkg_check_modules(MXML REQUIRED mxml>=2.5)
if(MXML_FOUND)
    message(STATUS "Found mxml ${MXML_VERSION}")
else(MXML_FOUND)
    message(FATAL_ERROR "mxml >=2.5 required but not found")
endif(MXML_FOUND)

# Alsa
pkg_check_modules(ALSA REQUIRED alsa>=1.0.17)
if(ALSA_FOUND)
    message(STATUS "Found Alsa ${ALSA_VERSION}")
else(ALSA_FOUND)
    message(FATAL_ERROR "Alsa >=1.0.17 required but not found")
endif(ALSA_FOUND)

# Jack
pkg_check_modules(JACK REQUIRED jack>=0.115.6)
if(JACK_FOUND)
    message(STATUS "Found jack ${JACK_VERSION}")
else(JACK_FOUND)
    message(FATAL_ERROR "Jack >=0.115.6 required but not found")
endif(JACK_FOUND)

# libsndfile
pkg_check_modules(LIBSNDFILE REQUIRED sndfile)
if(LIBSNDFILE_FOUND)
    message(STATUS "Found libsndfile ${LIBSNDFILE_VERSION}")
else(LIBSNDFILE_FOUND)
    message(FATAL_ERROR "libsndfile required but not found")
endif(LIBSNDFILE_FOUND)

set (DSP_sources
    DSP/FFTwrapper.cpp  DSP/AnalogFilter.cpp  DSP/FormantFilter.cpp
    DSP/SVFilter.cpp  DSP/Filter.cpp
)

set(Effects_sources
    Effects/Alienwah.cpp  Effects/Chorus.cpp  Effects/Echo.cpp
    Effects/Effect.cpp  Effects/EffectLFO.cpp  Effects/EffectMgr.cpp
    Effects/Phaser.cpp  Effects/Reverb.cpp  Effects/EQ.cpp
    Effects/Distorsion.cpp  Effects/DynamicFilter.cpp
)

set(Misc_sources
    Misc/Bank.cpp  Misc/Master.cpp  Misc/Microtonal.cpp  Misc/Part.cpp
    Misc/Util.cpp  Misc/Config.cpp  Misc/XMLwrapper.cpp
)

set(Params_sources
    Params/ADnoteParameters.cpp  Params/EnvelopeParams.cpp
    Params/FilterParams.cpp  Params/LFOParams.cpp
    Params/SUBnoteParameters.cpp  Params/PADnoteParameters.cpp
    Params/Controller.cpp  Params/Presets.cpp
    Params/PresetsStore.cpp
)

set(Synth_sources
    Synth/ADnote.cpp  Synth/Envelope.cpp  Synth/LFO.cpp  Synth/OscilGen.cpp
    Synth/SUBnote.cpp  Synth/Resonance.cpp  Synth/PADnote.cpp
)

set(MusicIO_sources
    MusicIO/MusicClient.cpp  MusicIO/MusicIO.cpp  MusicIO/JackEngine.cpp
    MusicIO/AlsaEngine.cpp  MusicIO/JackClient.cpp  MusicIO/AlsaClient.cpp
    MusicIO/JackAlsaClient.cpp  MusicIO/AlsaJackClient.cpp MusicIO/WavRecord.cpp
)

add_definitions(
    -D'YOSHIMI_VERSION="${YOSHIMI_VERSION}"'
    ${ALSA_LDFLAGS}
    ${JACK_LDFLAGS}
    -DYOSHI_FIFO_DIR="${YOSHI_FIFO_DIR}"
)

if(${DEFAULT_AUDIO} STREQUAL "jack")
    message(STATUS "Default audio driver is Jack")
    add_definitions(-DDEFAULT_AUDIO=jack_audio)
elseif(${DEFAULT_AUDIO} STREQUAL "alsa")
    message(STATUS "Default audio driver is Alsa")
    add_definitions(-DDEFAULT_AUDIO=alsa_audio)
else(${DEFAULT_AUDIO} STREQUAL "alsa")
    message(FATAL_ERROR "Invalid DEFAULT_AUDIO selection: " ${DEFAULT_AUDIO})
endif(${DEFAULT_AUDIO} STREQUAL "jack")

if(${DEFAULT_MIDI} STREQUAL "jack")
    message(STATUS "Default midi driver is Jack")
    add_definitions(-DDEFAULT_MIDI=jack_midi)
elseif(${DEFAULT_MIDI} STREQUAL "alsa")
    message(STATUS "Default midi driver is Alsa")
    add_definitions(-DDEFAULT_MIDI=alsa_midi)
else(${DEFAULT_MIDI} STREQUAL "alsa")
    message(FATAL_ERROR "Invalid DEFAULT_MIDI selection: " ${DEFAULT_MIDI})
endif(${DEFAULT_MIDI} STREQUAL "jack")

if(USE_ASM_F21)
    add_definitions(-DASM_F2I)
endif(USE_ASM_F21)

set(YOSHI_INCLUDES
    ${YOSHI_INCLUDES}
    ${FONTCONFIG_INCLUDE_DIRS}
    ${FFTW3_INC_DIR}
    ${MXML_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    ${JACK_INCLUDE_DIRS}
    ${LIBSNDFILE_INCLUDE_DIRS}
)

set(YOSHI_SOURCES
    ${YOSHI_SOURCES}
    ${DSP_sources}
    ${Effects_sources}
    ${Misc_sources}
    ${Params_sources}
    ${Synth_sources}
    ${MusicIO_sources}
)

set(YOSHI_LIBRARIES
    ${YOSHI_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${MXML_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${JACK_LIBRARIES}
    ${LIBSNDFILE_LIBRARIES}
)

include_directories(AFTER ${YOSHI_INCLUDES})

add_executable(yoshimi ${YOSHI_SOURCES} main.cpp)

target_link_libraries(yoshimi ${YOSHI_LIBRARIES})

install(TARGETS yoshimi RUNTIME DESTINATION bin)

install(DIRECTORY ../banks DESTINATION share/yoshimi
    FILE_PERMISSIONS
        OWNER_READ OWNER_WRITE
        GROUP_READ GROUP_WRITE
        WORLD_READ
    DIRECTORY_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)

set_directory_properties(PROPERTIES
    ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_SOURCE_DIR}/*UI.c* ${CMAKE_SOURCE_DIR}/src/*UI.h"
)

add_custom_target(showversion
    COMMAND echo -n "Version: "
    COMMAND cat version.txt
    COMMAND echo
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# clean sweep
add_custom_target(distclean
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/*.log
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/Makefile
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/install_manifest.txt
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/cmake_install.cmake
    COMMAND find ${CMAKE_SOURCE_DIR} -type f -name CMakeCache.txt | xargs -r rm -f
    COMMAND find ${CMAKE_SOURCE_DIR} -type d -name CMakeFiles | xargs -r rm -rf
    COMMAND find ${CMAKE_SOURCE_DIR} -type f -name "*.marks" | xargs -r rm -f
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_dependencies(distclean clean)
