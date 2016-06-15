#
#   CMakeLists.txt
#
#   Copyright 2009-2011, Alan Calvert
#   Copyright 2014-2016, Will Godfrey, and others
#
#   This file is part of yoshimi, which is free software: you can
#   redistribute it and/or modify it under the terms of the GNU General
#   Public License as published by the Free Software Foundation, either
#   version 2 of the License, or (at your option) any later version.
#
#   yoshimi is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

project (Yoshimi)
cmake_minimum_required (VERSION 2.6)
cmake_policy (VERSION 2.6)
cmake_policy (SET CMP0004 OLD)
if (POLICY CMP0046)
    cmake_policy (SET CMP0046 OLD)
endif (POLICY CMP0046)
set (CMAKE_CXX_STANDARD 11) # we seem to need both for
add_definitions(-std=gnu++11) # various versions of cmake
set (YOSHIMI_VERSION "1.4.0.1")
file (WRITE version.txt "${YOSHIMI_VERSION}")

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_USE_RELATIVE_PATHS OFF)
    # relative paths break some scripts(?)
set (CMAKE_SKIP_RULE_DEPENDENCY OFF)
    # Rebuild objects if rules have changed, even if source hasn't.

message (STATUS "Building Yoshimi version ${YOSHIMI_VERSION} for ${CMAKE_SYSTEM_NAME}")

include (CheckCSourceCompiles)
include(GNUInstallDirs)

mark_as_advanced (EXECUTABLE_OUTPUT_PATH)
mark_as_advanced (LIBRARY_OUTPUT_PATH)
mark_as_advanced (CMAKE_BUILD_TYPE)
mark_as_advanced (CMAKE_INSTALL_PREFIX)

# Check for jack session availability
check_c_source_compiles (
    "#include <jack/session.h>
    int main(int argc, char **argv)
    {
        jack_session_event_type_t x = JackSessionSave;
        return 0;
    }" HasJackSession
)

if (HasJackSession)
    option (JackSessionSupport "Include Jack Session Support" ON)
else (HasJackSession)
    set (JackSessionSupport OFF)
endif (HasJackSession)

#set (DefaultAudio jack CACHE STRING "Default audio driver - alsa or jack")
#set (DefaultMidi jack CACHE STRING "Default midi driver - alsa or jack")

option (BuildForAMD_X86_64 "Build for AMD x86_64 system" OFF)
option (BuildForCore2_X86_64 "Build for Intel Core2 x86_64 system" OFF)
option (BuildForDebug "Include gdb debugging support" OFF)

#option to build lv2 plugin
option (BuildLV2Plugin "Build yoshimi lv2 plugin interface" ON)

set (FifoDirectory /dev/shm CACHE STRING "Directory for fifos")

set (BuildOptions_x86_64AMD
    "-O3 -march=athlon64 -m64 -Wall -ffast-math -fno-finite-math-only -fomit-frame-pointer"
  CACHE STRING "X86_64 compiler options"
)

set (BuildOptions_X86_64Core2
    "-O3 -march=core2 -m64 -Wall -ffast-math -fno-finite-math-only -fomit-frame-pointer"
  CACHE STRING "X86_64 compiler options"
)

set (BuildOptionsBasic
    "-O3 -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer"
    CACHE STRING "basic X86 complier options"
)

set (BuildOptionsDebug
    "-O0 -g3 -ggdb -Wall -Wpointer-arith" CACHE STRING "Debug build flags")

find_package (PkgConfig REQUIRED)
if (PKG_CONFIG_FOUND)
    message(STATUS "Found pkg-config ${PKG_CONFIG_EXECUTABLE}")
else (PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config required but not found")
endif (PKG_CONFIG_FOUND)

# libz
set (CMAKE_REQUIRED_LIBRARIES z)
check_c_source_compiles (
    "#include <zlib.h>
     int main(int argc, char **argv) {
         gzFile zzz  = gzopen(\"/dev/null\", \"rb\");
         if (NULL != zzz)
            gzclose(zzz);
         return 0;
     }" HAS_LIBZ
)
if (HAS_LIBZ)
    message(STATUS "Found libz")
else (HAS_LIBZ)
    message(FATAL_ERROR "libz required but not found: ${HAS_LIBZ}")
endif (HAS_LIBZ)

# fftw3f
pkg_check_modules (FFTW3F REQUIRED fftw3f>=0.22)
if (FFTW3F_FOUND)
    set (FFTW3F_LIBRARIES "${FFTW3F_LIBRARIES}")
    message (STATUS "Found fftw3f ${FFTW3F_VERSION}")
else (FFTW3F_FOUND)
    message (FATAL_ERROR "fftw3f >=0.22 required but not found")
endif (FFTW3F_FOUND)

# mxml
pkg_check_modules (MXML REQUIRED mxml>=2.5)
if (MXML_FOUND)
    message(STATUS "Found mxml ${MXML_VERSION}")
else (MXML_FOUND)
    message(FATAL_ERROR "mxml >=2.5 required but not found")
endif (MXML_FOUND)

# Alsa
pkg_check_modules (ALSA REQUIRED alsa>=1.0.17)
if (ALSA_FOUND)
    message(STATUS "Found Alsa ${ALSA_VERSION}")
else (ALSA_FOUND)
    message(FATAL_ERROR "Alsa >=1.0.17 required but not found")
endif (ALSA_FOUND)

# Jack
pkg_check_modules (JACK REQUIRED jack>=0.115.6)
if (JACK_FOUND)
    message(STATUS "Found jack ${JACK_VERSION}")
else (JACK_FOUND)
    message(FATAL_ERROR "Jack >=0.115.6 required but not found")
endif (JACK_FOUND)

# Boost
find_package (Boost)
if (Boost_FOUND)
    message(STATUS "Found boost headers version " ${Boost_VERSION})
else (Boost_FOUND)
    message (FATAL_ERROR "boost headers required, but not found")
endif (Boost_FOUND)

# fontconfig
pkg_check_modules (FONTCONFIG REQUIRED fontconfig>=0.22)
mark_as_advanced(FONTCONFIG_LIBRARIES)
if(FONTCONFIG_FOUND)
    message (STATUS "Found fontconfig ${FONTCONFIG_VERSION}")
else(FONTCONFIG_FOUND)
    message (FATAL_ERROR "fontconfig>=0.22 required but not found")
endif(FONTCONFIG_FOUND)


# libcairo
pkg_check_modules (LIBCAIRO REQUIRED cairo)
if (LIBCAIRO_FOUND)
    message (STATUS "Found libcairo ${LIBCAIRO_VERSION}")
else (LIBCAIRO_FOUND)
    message (FATAL_ERROR "libcairo required but not found")
endif (LIBCAIRO_FOUND)


# fltk
find_package (FLTK REQUIRED)
if (FLTK_FOUND)
    message (STATUS "Found FLTK")
else (FLTK_FOUND)
    message (FATAL_ERROR "FLTK required but not found")
endif (FLTK_FOUND)
mark_as_advanced (FLTK_DIR)
mark_as_advanced (FLTK_FLUID_EXECUTABLE)
mark_as_advanced (FLTK_MATH_LIBRARY)


# libncurses
pkg_check_modules (LIBNCURSES ncurses)
if (LIBNCURSES_FOUND)
    message (STATUS "Found libncurses ${LIBNCURSES_VERSION}")
else ()
    pkg_check_modules (LIBNCURSES REQUIRED ncursesw)
    if (LIBNCURSES_FOUND)
        message (STATUS "Found libncursesw ${LIBNCURSES_VERSION}")
    else()
        message (FATAL_ERROR "libncurses required but not found")
    endif ()
endif ()

find_path(Readline_ROOT_DIR
    NAMES include/readline/readline.h
)

find_path(Readline_INCLUDE_DIR
    NAMES readline/readline.h
    HINTS ${Readline_ROOT_DIR}/include
)

find_library(Readline_LIBRARY
    NAMES readline
    HINTS ${Readline_ROOT_DIR}/lib
)

if(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY)
  set(READLINE_FOUND TRUE)
else(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY)
  FIND_LIBRARY(Readline_LIBRARY NAMES readline)
  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(Readline DEFAULT_MSG Readline_INCLUDE_DIR Readline_LIBRARY )
  MARK_AS_ADVANCED(Readline_INCLUDE_DIR Readline_LIBRARY)
endif(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY)

mark_as_advanced(
    Readline_ROOT_DIR
    Readline_INCLUDE_DIR
    Readline_LIBRARY
)

if(READLINE_FOUND)
    message (STATUS "Readline library found" )
else(READLINE_FOUND)
    message( FATAL_ERROR "Readline library not found! Please install development components (libreadline-dev)" )
endif(READLINE_FOUND)

set (YOSHI_INCLUDES ${FLTK_INCLUDE_DIR})

if (BuildForDebug)
    set (CMAKE_BUILD_TYPE "Debug")
    set (CMAKE_CXX_FLAGS_DEBUG ${BuildOptionsDebug})
    message (STATUS "Building for ${CMAKE_BUILD_TYPE}, flags: ${CMAKE_CXX_FLAGS_DEBUG}")
else (BuildForDebug)
    set (CMAKE_BUILD_TYPE "Release")
    if (BuildForAMD_X86_64)
        set (CMAKE_CXX_FLAGS_RELEASE ${BuildOptions_x86_64AMD})
    else (BuildForAMD_X86_64)
        if (BuildForCore2_X86_64)
            set (CMAKE_CXX_FLAGS_RELEASE ${BuildOptions_X86_64Core2})
        else (BuildForCore2_X86_64)
            set (CMAKE_CXX_FLAGS_RELEASE ${BuildOptionsBasic})
        endif (BuildForCore2_X86_64)
    endif (BuildForAMD_X86_64)
    message (STATUS "Building for ${CMAKE_BUILD_TYPE}, flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif (BuildForDebug)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../desktop/yoshimi.desktop.in"
    "${CMAKE_CURRENT_BINARY_DIR}/yoshimi.desktop"
    IMMEDIATE @ONLY)

set (DSP_sources
    DSP/FFTwrapper.cpp  DSP/AnalogFilter.cpp  DSP/FormantFilter.cpp
    DSP/SVFilter.cpp  DSP/Filter.cpp  DSP/Unison.cpp
)

set (Effects_sources
    Effects/Alienwah.cpp  Effects/Chorus.cpp  Effects/Echo.cpp
    Effects/EffectLFO.cpp  Effects/EffectMgr.cpp  Effects/Effect.cpp
    Effects/Phaser.cpp  Effects/Reverb.cpp  Effects/EQ.cpp
    Effects/Distorsion.cpp  Effects/DynamicFilter.cpp
)

set (Misc_sources
    Misc/ConfBuild.cpp  Misc/Config.cpp  Misc/SynthEngine.cpp  Misc/Bank.cpp
    Misc/Microtonal.cpp   Misc/Part.cpp  Misc/XMLwrapper.cpp  Misc/MiscFuncs.cpp   Misc/WavFile.cpp   Misc/CmdInterface.cpp
)

set (Params_sources
    Params/ADnoteParameters.cpp  Params/EnvelopeParams.cpp
    Params/FilterParams.cpp  Params/LFOParams.cpp
    Params/SUBnoteParameters.cpp  Params/PADnoteParameters.cpp
    Params/Controller.cpp  Params/Presets.cpp Params/PresetsStore.cpp
)

set (Synth_sources
    Synth/ADnote.cpp  Synth/Envelope.cpp  Synth/LFO.cpp  Synth/OscilGen.cpp
    Synth/SUBnote.cpp  Synth/Resonance.cpp  Synth/PADnote.cpp
    Synth/BodyDisposal.cpp
)

set (MusicIO_sources
    MusicIO/MusicClient.cpp  MusicIO/MusicIO.cpp  MusicIO/JackEngine.cpp
    MusicIO/AlsaEngine.cpp
)

set (FltkUI_names
    "WidgetPDialUI"  "PresetsUI"  "EnvelopeUI"
    "LFOUI"  "FilterUI"  "VirKeyboardUI"
    "ConfigUI"  "SUBnoteUI"  "ResonanceUI"
    "OscilGenUI"  "ADnoteUI"  "PADnoteUI"
    "EffUI"  "BankUI"  "PartUI"
    "MicrotonalUI"  "MasterUI" "MasterMiscUI"
    "ParametersUI" "ConsoleUI" "VectorUI"
    "WidgetMWSliderUI" "WidgetMWRollerUI"
)

# workaround fltk_wrap_ui breakage
set (FltkUI_sources)
set (FltkUI_headers)
foreach(basename ${FltkUI_names})
    set (fluidfile "${CMAKE_CURRENT_SOURCE_DIR}/UI/${basename}.fl")
    set (sourcefile "${basename}.cpp")
    set (headerfile "${basename}.h")
    add_custom_command(
        OUTPUT ${sourcefile}
        COMMAND ${FLTK_FLUID_EXECUTABLE} ARGS -c -o .cpp ${fluidfile}
        DEPENDS ${fluidfile}
    )
    set (FltkUI_sources ${FltkUI_sources} "${sourcefile}")
    set (FltkUI_headers ${FltkUI_headers} "${headerfile}")
endforeach(basename ${FltkUI_names})

add_definitions (
    -D'YOSHIMI_VERSION="${YOSHIMI_VERSION}"'
    -D'BASE_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}"'
    ${ALSA_LDFLAGS}
    ${JACK_LDFLAGS}
    -DYOSHI_FIFO_DIR="${FifoDirectory}"
)

add_definitions (-DYOSHIMI="yoshimi")
add_definitions (-DCOMMAND_SIZE=80)
add_definitions (-DMAX_HISTORY=25)
add_definitions (-DMAX_PRESETS=1000)
add_definitions (-DMAX_PRESET_DIRS=128)
add_definitions (-DMAX_BANK_ROOT_DIRS=128)
add_definitions (-DMAX_BANKS_IN_ROOT=128)
add_definitions (-DMAX_AD_HARMONICS=128)
add_definitions (-DMAX_SUB_HARMONICS=64)
add_definitions (-DPAD_MAX_SAMPLES=96)
add_definitions (-DADD_COLOUR=0xdfafbf00)
add_definitions (-DBASE_COLOUR=0xbfbfbf00)
add_definitions (-DSUB_COLOUR=0xafcfdf00)
add_definitions (-DPAD_COLOUR=0xcfdfaf00)
add_definitions (-DNUM_MIDI_PARTS=64)
add_definitions (-DNUM_MIDI_CHANNELS=16)
add_definitions (-DMAX_ENVELOPE_POINTS=40)
add_definitions (-DMIN_ENVELOPE_DB=-60)
add_definitions (-DMAX_RESONANCE_POINTS=256)
add_definitions (-DMAX_KEY_SHIFT=36)
add_definitions (-DMIN_KEY_SHIFT=-36)

add_definitions (-DPI=3.1415926536f)
add_definitions (-DTWOPI=6.28318530718f)
add_definitions (-DHALFPI=1.57079632679f)
add_definitions (-DLOG_2=0.693147181f)

#probably not the best way to do this :(
add_definitions (-DXML_INSTRUMENT=1)
add_definitions (-DXML_PARAMETERS=2)
add_definitions (-DXML_MICROTONAL=3)
add_definitions (-DXML_PRESETS=4)
add_definitions (-DXML_STATE=5)
add_definitions (-DXML_CONFIG=6)
add_definitions (-DXML_BANK=7)
add_definitions (-DXML_HISTORY=8)
add_definitions (-DXML_VECTOR=9)

# these were previously (pointlessly) user configurable
add_definitions (-DNUM_VOICES=8)
add_definitions (-DPOLIPHONY=80)
add_definitions (-DNUM_SYS_EFX=4)
add_definitions (-DNUM_INS_EFX=8)
add_definitions (-DNUM_PART_EFX=3)
add_definitions (-DNUM_KIT_ITEMS=16)
add_definitions (-DVELOCITY_MAX_SCALE=8.0f)
add_definitions (-DFADEIN_ADJUSTMENT_SCALE=20)
add_definitions (-DMAX_EQ_BANDS=8) # MAX_EQ_BANDS must be less than 20")
add_definitions (-DMAX_FILTER_STAGES=5)
add_definitions (-DFF_MAX_VOWELS=6)
add_definitions (-DFF_MAX_FORMANTS=12)
add_definitions (-DFF_MAX_SEQUENCE=8)
add_definitions (-DMAX_PHASER_STAGES=12)
add_definitions (-DMAX_ALIENWAH_DELAY=100)

#if (${DefaultAudio} STREQUAL "jack")
#    message(STATUS "Default audio driver is Jack")
    add_definitions(-DDEFAULT_AUDIO=jack_audio)
#elseif (${DefaultAudio} STREQUAL "alsa")
#    message(STATUS "Default audio driver is Alsa")
#    add_definitions(-DDEFAULT_AUDIO=alsa_audio)
#else (${DefaultAudio} STREQUAL "alsa")
#    message(FATAL_ERROR "Invalid DefaultAudio selection: " ${DefaultAudio})
#endif (${DefaultAudio} STREQUAL "jack")

#if (${DefaultMidi} STREQUAL "jack")
#    message(STATUS "Default midi driver is Jack")
    add_definitions(-DDEFAULT_MIDI=jack_midi)
#elseif (${DefaultMidi} STREQUAL "alsa")
#    message(STATUS "Default midi driver is Alsa")
#    add_definitions(-DDEFAULT_MIDI=alsa_midi)
#else (${DefaultMidi} STREQUAL "alsa")
#    message(FATAL_ERROR "Invalid DefaultMidi selection: " ${DefaultMidi})
#endif (${DefaultMidi} STREQUAL "jack")

if (JackSessionSupport)
    message(STATUS "With jack session support")
    add_definitions(-DJACK_SESSION)
else (JackSessionSupport)
    message(STATUS "Without jack session support")
endif (JackSessionSupport)

# Check for jack latency API >= 0.120.1
check_c_source_compiles (
    "#include <jack/jack.h>
    int main(int argc, char **argv)
    {
        if (jack_set_latency_callback)
            return 0;
        else
            return 1;
    }" HasJackLatency
)

if (HasJackLatency)
    add_definitions(-DJACK_LATENCY)
endif (HasJackLatency)

set (ProgSources
    ${Misc_sources}
    ${Params_sources}
    ${Synth_sources}
    ${DSP_sources}
    ${Effects_sources}
    ${MusicIO_sources}
    ${FltkUI_sources}
)

include_directories (AFTER
    ${MXML_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    ${JACK_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
    ${FLTK_INCLUDE_DIR}
    ${FFTW3F_INC_DIR}
    ${LIBCAIRO_INCLUDE_DIRS}
    ${Readline_INCLUDE_DIR}
)

set(ExternLibraries
    ${FONTCONFIG_LIBRARIES}
    ${FLTK_LIBRARIES}
    ${MXML_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${JACK_LIBRARIES}
    ${LIBSNDFILE_LIBRARIES}
    ${FFTW3F_LIBRARIES}
    ${LIBCAIRO_LIBRARIES}
    ${Readline_LIBRARY}
    ncurses
    z
    dl
)

add_executable (yoshimi ${ProgSources} main.cpp)

target_link_libraries (yoshimi ${ExternLibraries})

install (TARGETS yoshimi RUNTIME DESTINATION bin)

install (DIRECTORY ../banks DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/yoshimi
    FILE_PERMISSIONS
        OWNER_READ OWNER_WRITE
        GROUP_READ GROUP_WRITE
        WORLD_READ
    DIRECTORY_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

install (DIRECTORY ../examples DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/yoshimi
    FILE_PERMISSIONS
        OWNER_READ OWNER_WRITE
        GROUP_READ GROUP_WRITE
        WORLD_READ
    DIRECTORY_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

install (DIRECTORY ../presets DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/yoshimi
    FILE_PERMISSIONS
        OWNER_READ OWNER_WRITE
        GROUP_READ GROUP_WRITE
        WORLD_READ
    DIRECTORY_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

install (DIRECTORY ../doc/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/yoshimi
    FILE_PERMISSIONS
        OWNER_READ OWNER_WRITE
        GROUP_READ GROUP_WRITE
        WORLD_READ
    DIRECTORY_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/yoshimi.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../desktop/yoshimi.png
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pixmaps)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../desktop/yoshimi.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../desktop/yoshimi_alt.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../desktop/yoshimi.appdata.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/appdata)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../desktop/yoshimi.1
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/man/man1)

set_directory_properties (PROPERTIES
    ADDITIONAL_MAKE_CLEAN_FILES "${FltkUI_headers}"
)

add_custom_target (showversion
    COMMAND echo -n "Version: "
    COMMAND cat version.txt
    COMMAND echo
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# clean sweep
add_custom_target (distclean
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/*.log
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/Makefile
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/install_manifest.txt
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/cmake_install.cmake
    COMMAND find ${CMAKE_SOURCE_DIR} -type f -name CMakeCache.txt | xargs -r rm -f
    COMMAND find ${CMAKE_SOURCE_DIR} -type d -name CMakeFiles | xargs -r rm -rf
    COMMAND find ${CMAKE_SOURCE_DIR} -type f -name "*.marks" | xargs -r rm -f
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/LV2_Plugin/Makefile
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/LV2_Plugin/cmake_install.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)


if (BuildLV2Plugin)
    if (EXISTS $ENV{LV2_PATH})
        set(LV2_INSTALL_DIR $ENV{LV2_PATH} CACHE PATH
        "Specifies where the LV2 libraries should be installed")
    else()
        set(LV2_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/lv2 CACHE PATH
        "Specifies where the LV2 libraries should be installed")
    endif()

    pkg_check_modules(LV2 REQUIRED lv2>=1.0.0)
    if (LV2_FOUND)
        message (STATUS "Found lv2 package ${LV2_VERSION}")
    else (LV2_FOUND)
        message (FATAL_ERROR "lv2 package required but not found (version 1.0.0 needed)")
    endif (LV2_FOUND)
    add_subdirectory(LV2_Plugin)
endif (BuildLV2Plugin)
