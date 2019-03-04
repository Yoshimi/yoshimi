#
#   CMakeLists.txt
#
#   Copyright 2009-2011, Alan Calvert
#   Copyright 2014-2019, Will Godfrey & others
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

# Modified February 2019

project (Yoshimi)
cmake_minimum_required (VERSION 3.0)
cmake_policy (VERSION 3.0)
cmake_policy (SET CMP0004 NEW)
if (POLICY CMP0046)
    cmake_policy (SET CMP0046 NEW)
endif (POLICY CMP0046) # opengl version
if (POLICY CMP0072)
    cmake_policy (SET CMP0072 OLD)
endif (POLICY CMP0072)
set (CMAKE_CXX_STANDARD 11) # we seem to need both for
add_definitions(-std=gnu++11) # various versions of cmake

add_definitions(-Wall)

# vvv
#add_definitions(-Wpedantic)
##add_definitions(-Werror=format-security)
##add_definitions(-Wextra)
# ^^^ comment these out on release pushes

set (YOSHIMI_VERSION "1.5.10.2")
add_definitions (-DMIN_CONFIG_MAJOR=1)
add_definitions (-DMIN_CONFIG_MINOR=5)

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

option (BuildFor0ld_X86 "Build for legacy x86 system" OFF)

option (BuildForAMD_X86_64 "Build for AMD x86_64 system" OFF)
option (BuildForCore2_X86_64 "Build for Intel Core2 x86_64 system" OFF)
option (BuildForNEON "Build for ARM Cortex A9 system" OFF)
option (BuildForThisCPU "Build for presently running system" OFF)
option (BuildForDebug "Debug build without optimisation. Enable gdb debugging support" OFF)

option (BuildWithFLTK "Build with FLTK GUI" ON)

#option to build lv2 plugin
option (LV2Plugin "Build yoshimi lv2 plugin interface" ON)

# developer info
option (ShowGuiReports "Enable GUI control reports" OFF)
option (ShowNoteOnTimes "Report Note-On Times" OFF)
option (ReportNotes "Report Notes On/Off" OFF)


set (BuildOptions_X86
    "-msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer"
    CACHE STRING "basic X86 compiler options"
)

set (BuildOptions_X86_64AMD
    "-march=athlon64 -m64 -Wall -msse -msse2 -mfpmath=sse -ffast-math -fno-finite-math-only -fomit-frame-pointer"
  CACHE STRING "X86_64 compiler options"
)

set (BuildOptions_X86_64Core2
    "-march=core2 -m64 -Wall -msse -msse2 -mfpmath=sse -ffast-math -fno-finite-math-only -fomit-frame-pointer"
  CACHE STRING "X86_64 compiler options"
)

set (BuildOptions_NEON
    "-march=armv7-a -mfloat-abi=hard -mfpu=neon -mcpu=cortex-a9 -mtune=cortex-a9 -pipe -mvectorize-with-neon-quad -funsafe-loop-optimizations"
  CACHE STRING "Cortex_a9 compiler options"
)

set (BuildOptionsThisCPU
    "-march=native -mtune=native -Wall"
  CACHE STRING "Current CPU compiler options"
)

set (BuildOptionsBasic
    "-ffast-math -fomit-frame-pointer"
    CACHE STRING "basic compilier options"
)

set (BuildOptionsDebug
    "-O0 -g3 -ggdb -Wpointer-arith -Wpedantic -Wextra" CACHE STRING "Compile without optimisation and with debug information")
set (BuildOptionsRelease
    "-O3" CACHE STRING "Compile with optimisation for Release builds")

if (BuildWithFLTK)
    add_definitions (-DGUI_FLTK)
endif(BuildWithFLTK)

find_package (PkgConfig REQUIRED)
if (PKG_CONFIG_FOUND)
    message(STATUS "Found pkg-config ${PKG_CONFIG_EXECUTABLE}")
else (PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config required but not found")
endif (PKG_CONFIG_FOUND)

include (CheckFunctionExists)

check_function_exists(exp10f EXP10F)

if(EXP10F)
    add_definitions(-DHAVE_EXP10F)
endif(EXP10F)

check_c_source_compiles (
    "#include <argp.h>
    #include <stdlib.h>
     int main () {
         int argc=1;
         char *argv[]={\"test\"};
         argp_parse(0,argc,argv,0,0,0);
         return 0;
     }" LIBC_HAS_ARGP
)

if (NOT LIBC_HAS_ARGP)
    message(STATUS "libc does not have argp")
    find_library (ARGP_LIB argp REQUIRED)
    if (ARGP_LIB)
        message(STATUS "Found libargp")
    else(ARGP_LIB)
        message(FATAL_ERROR "libargp required but not found")
    endif(ARGP_LIB)
endif(NOT LIBC_HAS_ARGP)

# libz
set (CMAKE_REQUIRED_LIBRARIES z)
check_c_source_compiles (
    "#include <zlib.h>
     #include <stdlib.h>
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
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    pkg_check_modules (ALSA REQUIRED alsa>=1.0.17)
    if (ALSA_FOUND)
	message(STATUS "Found Alsa ${ALSA_VERSION}")
    else (ALSA_FOUND)
	message(FATAL_ERROR "Alsa >=1.0.17 required but not found")
    endif (ALSA_FOUND)
    add_definitions(-DHAVE_ALSA)
    FIND_LIBRARY(LIBDL_LINUX NAMES dl)
else()

endif()

# Jack
pkg_check_modules (JACK REQUIRED jack>=0.115.6)
if (JACK_FOUND)
    message(STATUS "Found jack ${JACK_VERSION}")
else (JACK_FOUND)
    message(FATAL_ERROR "Jack >=0.115.6 required but not found")
endif (JACK_FOUND)

# fontconfig
pkg_check_modules (FONTCONFIG REQUIRED fontconfig>=0.22)
mark_as_advanced(FONTCONFIG_LIBRARIES)
if(FONTCONFIG_FOUND)
    message (STATUS "Found fontconfig ${FONTCONFIG_VERSION}")
else(FONTCONFIG_FOUND)
    message (FATAL_ERROR "fontconfig>=0.22 required but not found")
endif(FONTCONFIG_FOUND)


if (BuildWithFLTK)
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
endif(BuildWithFLTK)


# libncurses / libcursesw
set(CURSES_NEED_NCURSES TRUE)
find_package (Curses REQUIRED)
if (CURSES_FOUND)
    message(STATUS "Found libncurses or libncursesw")
else (CURSES_FOUND)
    message(FATAL_ERROR "libncurses or libncursesw required but not found")
endif (CURSES_FOUND)

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

#find_package(PkgConfig REQUIRED)
if(Readline_INCLUDE_DIR AND Readline_LIBRARY AND CURSES_LIBRARY)
  set(READLINE_FOUND TRUE)
else(Readline_INCLUDE_DIR AND Readline_LIBRARY AND CURSES_LIBRARY)
  FIND_LIBRARY(Readline_LIBRARY NAMES readline)
  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(Readline DEFAULT_MSG Readline_INCLUDE_DIR Readline_LIBRARY )
  MARK_AS_ADVANCED(Readline_INCLUDE_DIR Readline_LIBRARY)
endif(Readline_INCLUDE_DIR AND Readline_LIBRARY AND CURSES_LIBRARY)

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

# set platform specific compiler flags
if (BuildFor0ld_X86)
    set (OurCxxFlags ${BuildOptions_X86})
else (BuildFor0ld_X86)
    if (BuildForAMD_X86_64)
        set (OurCxxFlags ${BuildOptions_X86_64AMD})
    else (BuildForAMD_X86_64)
        if (BuildForCore2_X86_64)
            set (OurCxxFlags ${BuildOptions_X86_64Core2})
        else (BuildForCore2_X86_64)
            if (BuildForNEON)
                set (OurCxxFlags ${BuildOptions_NEON})
            else (BuildForNEON)
                if (BuildForThisCPU)
                    set (OurCxxFlags ${BuildOptionsThisCPU})
                else (BuildForThisCPU)
                    set (OurCxxFlags ${BuildOptionsBasic})
                endif (BuildForThisCPU)
            endif (BuildForNEON)
        endif (BuildForCore2_X86_64)
    endif (BuildForAMD_X86_64)
endif (BuildFor0ld_X86)
set (CMAKE_CXX_FLAGS "${OurCxxFlags} ${CMAKE_CXX_FLAGS}")

if (BuildForDebug)
    set (CMAKE_BUILD_TYPE "Debug")
    set (CMAKE_CXX_FLAGS_DEBUG ${BuildOptionsDebug})
    message (STATUS "DEBUG Build, flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
else (BuildForDebug)
    set (CMAKE_BUILD_TYPE "Release")
    set (CMAKE_CXX_FLAGS_RELEASE ${BuildOptionsRelease})
    message (STATUS "RELEASE Build, flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
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
    Misc/Config.cpp  Misc/SynthEngine.cpp  Misc/Bank.cpp  Misc/Splash.cpp
    Misc/Microtonal.cpp   Misc/Part.cpp  Misc/XMLwrapper.cpp  Misc/MiscFuncs.cpp   Misc/WavFile.cpp
)

set (Interface_Sources
    Interface/CmdInterface.cpp
    Interface/InterChange.cpp
    Interface/MidiLearn.cpp
    Interface/MidiDecode.cpp
    Interface/FileMgr.cpp
)

set (Params_sources
    Params/ADnoteParameters.cpp  Params/EnvelopeParams.cpp
    Params/FilterParams.cpp  Params/LFOParams.cpp
    Params/SUBnoteParameters.cpp  Params/PADnoteParameters.cpp
    Params/Controller.cpp  Params/Presets.cpp
    Params/PresetsStore.cpp  Params/UnifiedPresets.cpp
)

set (Synth_sources
    Synth/ADnote.cpp  Synth/Envelope.cpp  Synth/LFO.cpp  Synth/OscilGen.cpp
    Synth/SUBnote.cpp  Synth/Resonance.cpp  Synth/PADnote.cpp
)

set (MusicIO_sources
    MusicIO/MusicClient.cpp  MusicIO/MusicIO.cpp  MusicIO/JackEngine.cpp
    MusicIO/AlsaEngine.cpp
)

if (BuildWithFLTK)
    set (FltkUI_names
        "PresetsUI" "EnvelopeUI"
        "LFOUI" "FilterUI" "VirKeyboardUI"
        "ConfigUI" "SUBnoteUI" "ResonanceUI"
        "OscilGenUI" "ADnoteUI" "PADnoteUI"
        "EffUI" "BankUI" "PartUI"
        "MicrotonalUI" "MasterUI" "MasterMiscUI"
        "ParametersUI" "ConsoleUI" "VectorUI"
        "MidiLearnUI"
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

    set (FltkUI_sources
        ${FltkUI_sources}
        UI/DynamicTooltip.cpp
        UI/WidgetPDial.cpp
        UI/WidgetMWSlider.cpp
        UI/MiscGui.cpp
    )
    set (YOSHI_INCLUDES ${FLTK_INCLUDE_DIR})
endif(BuildWithFLTK)

add_definitions (
    -D'YOSHIMI_VERSION="${YOSHIMI_VERSION}"'
    -D'BASE_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}"'
    ${ALSA_LDFLAGS}
    ${JACK_LDFLAGS}
    -DYOSHI_FIFO_DIR="${FifoDirectory}"
)

add_definitions (-DYOSHIMI="yoshimi")

if (ShowGuiReports)
    add_definitions (-DENABLE_REPORTS)
endif (ShowGuiReports)

if (ShowNoteOnTimes)
    add_definitions (-DREPORT_NOTEON)
endif (ShowNoteOnTimes)

if (ReportNotes)
    add_definitions (-DREPORT_NOTES_ON_OFF)
endif (ReportNotes)

add_definitions(-DDEFAULT_AUDIO=jack_audio)

add_definitions(-DDEFAULT_MIDI=jack_midi)

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
    ${Interface_Sources}
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
    ${CURSES_LIBRARIES}
    ${Readline_LIBRARY}
    z
    #dl
    ${LIBDL_LINUX}
)


if("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD")
    link_directories(/usr/local/lib)
endif()

add_executable (yoshimi ${ProgSources} main.cpp)

target_link_libraries (yoshimi ${ExternLibraries})


if (ARGP_LIB)
    target_link_libraries (yoshimi ${ARGP_LIB})
endif(ARGP_LIB)

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
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../desktop/metainfo/yoshimi.appdata.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../desktop/yoshimi.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

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


if (LV2Plugin)
    if (NOT BuildWithFLTK)
        message(FATAL_ERROR "Can't build LV2 headless")
    endif()
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
endif (LV2Plugin)
