#
#   CMakeLists.txt
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
set(YOSHIMI_VERSION "0.010")
FILE(WRITE version.txt "${YOSHIMI_VERSION}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_USE_RELATIVE_PATHS OFF)
    # relative paths break some scripts(?)
set(CMAKE_SKIP_RULE_DEPENDENCY OFF)
    # Rebuild objects if rules have changed, even if source hasn't.
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake.checks")
    # modules for 3rd party library checks reside here

# User options
set(DEFAULT_AUDIO jack CACHE STRING "Default audio driver - alsa or jack")
set(DEFAULT_MIDI jack CACHE STRING "Default midi driver - alsa or jack")
option(WANT_GUI "Build with GUI" ON)
option(X86_64_BUILD "Build for x86_64 system" OFF)
option(WANT_ASM_F21 "Assembler FLOAT to INT conversions" ON)
option(WANT_DEBUG "Include gdb debug support" OFF)

set(X86_64_OPTS
    # "-march=native
    "-O3 -march=athlon64 -m64 -Wall -ffast-math -fno-finite-math-only -fomit-frame-pointer"
    CACHE STRING "X86_64 complier options"
)
set(X86_OPTS
    "-O3 -march=native -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer"
    CACHE STRING "X86 complier options"
)

mark_as_advanced(ASM_F21)
mark_as_advanced(EXECUTABLE_OUTPUT_PATH)
mark_as_advanced(LIBRARY_OUTPUT_PATH)

set(PRELUDE "Building Yoshimi version ${YOSHIMI_VERSION} for ${CMAKE_SYSTEM_NAME}")
if(WANT_DEBUG)
    message(STATUS "${PRELUDE} with gdb debug information")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Choose the type of build, options are: Debug Release" FORCE)
else(WANT_DEBUG)
    message(STATUS "${PRELUDE} without debug information")
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: Debug Release" FORCE)
endif(WANT_DEBUG)
MARK_AS_ADVANCED(CMAKE_BUILD_TYPE)

# Set OS define
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    add_definitions(-DOS_LINUX)
elseif(CMAKE_SYSTEM_NAME strequal Darwin)
    message(FATAL_ERROR "Sorry, can't do a ${CMAKE_SYSTEM_NAME} build yet")
endif()

# library dependency checks
# PKG-CONFIG
find_package(PkgConfig REQUIRED)
if(PKG_CONFIG_FOUND)
    message(STATUS "Found pkg-config ${PKG_CONFIG_EXECUTABLE}")
else(PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config required but not found")
endif(PKG_CONFIG_FOUND)

if(WANT_GUI)
    message(STATUS "Will build gui")
    # fontconfig
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig>=0.22)
    mark_as_advanced(FONTCONFIG_LIBRARIES)
    message(STATUS "Found fontconfig ${FONTCONFIG_VERSION}")

    # fltk
    find_package(MYFLTK REQUIRED)
    mark_as_advanced(FORCE FLTK_BASE_LIBRARY)
    mark_as_advanced(FORCE FLTK_CONFIG_SCRIPT)
    mark_as_advanced(FORCE FLTK_DIR)
    mark_as_advanced(FORCE FLTK_FLUID_EXECUTABLE)
    mark_as_advanced(FORCE FLTK_FORMS_LIBRARY)
    mark_as_advanced(FORCE FLTK_GL_LIBRARY)
    mark_as_advanced(FORCE FLTK_IMAGES_LIBRARY)
    mark_as_advanced(FORCE FLTK_INCLUDE_DIR)
    mark_as_advanced(FORCE FLTK_MATH_LIBRARY)
    set (FLTK_sources
       UI/WidgetPDial.fl  UI/PresetsUI.fl  UI/EnvelopeUI.fl
       UI/LFOUI.fl  UI/FilterUI.fl  UI/VirKeyboard.fl
       UI/ConfigUI.fl  UI/SUBnoteUI.fl  UI/ResonanceUI.fl
       UI/OscilGenUI.fl  UI/ADnoteUI.fl  UI/PADnoteUI.fl
       UI/EffUI.fl  UI/BankUI.fl  UI/PartUI.fl
       UI/MicrotonalUI.fl  UI/MasterUI.fl  UI/GuiThread.fl
    )
    fltk_wrap_ui(yoshimi ${FLTK_sources})
    set_source_files_properties(UI/MasterUI.h PROPERTIES GENERATED 1)
    set(YOSHI_INCLUDES ${FLTK_INCLUDE_DIR})
    set(YOSHI_SOURCES ${yoshimi_FLTK_UI_SRCS})
    set(YOSHI_LIBRARIES ${MYFLTK_LIBRARIES})
    add_definitions(${MYFLTK_LDFLAGS})
else(WANT_GUI)
    message(STATUS "Will not build gui")
    add_definitions(-DDISABLE_GUI)
endif(WANT_GUI)

# FFTW3
pkg_check_modules(FFTW3 REQUIRED fftw3>=0.22)
message(STATUS "Found fftw3 ${FFTW3_VERSION}")

# mxml
set(MXML_LIBRARIES "" CACHE STRING "mxml libaries")
pkg_check_modules(MXML REQUIRED mxml>=2.5)
mark_as_advanced(FORCE MXML_LIBRARIES)
message(STATUS "Found mxml ${MXML_VERSION}")

# Alsa
pkg_check_modules(ALSA REQUIRED alsa)
if(ALSA_FOUND)
    message(STATUS "Found Alsa ${ALSA_VERSION}")
else(ALSA_FOUND)
    message(FATAL_ERROR "Alsa required but not found")
endif(ALSA_FOUND)

# Jack
pkg_check_modules(JACK REQUIRED jack)
if(JACK_FOUND)
    message(STATUS "Found jack ${JACK_VERSION}")
else(JACK_FOUND)
    message(FATAL_ERROR "Jack required but not found")
endif(JACK_FOUND)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -ggdb -Wall -Wpointer-arith")
if(X86_64_BUILD)
    set(CMAKE_CXX_FLAGS_RELEASE ${X86_64_OPTS})
else(X86_64_BUILD)
    set(CMAKE_CXX_FLAGS_RELEASE ${X86_OPTS})
endif(X86_64_BUILD)

set (DSP_sources
    DSP/FFTwrapper.cpp  DSP/AnalogFilter.cpp  DSP/FormantFilter.cpp 
    DSP/SVFilter.cpp  DSP/Filter.cpp
)

set(Effects_sources 
    Effects/Alienwah.cpp  Effects/Chorus.cpp  Effects/Echo.cpp
    Effects/Effect.cpp  Effects/EffectLFO.cpp  Effects/EffectMgr.cpp
    Effects/Phaser.cpp  Effects/Reverb.cpp  Effects/EQ.cpp
    Effects/Distorsion.cpp  Effects/DynamicFilter.cpp  Effects/Fader.cpp
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
    MusicIO/JackAlsaClient.cpp  MusicIO/AlsaJackClient.cpp
)

add_definitions(-D'YOSHIMI_VERSION="${YOSHIMI_VERSION}"')
add_definitions(${ALSA_LDFLAGS} ${JACK_LDFLAGS})

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
else(${DEFAULT_midi} STREQUAL "alsa")
    message(FATAL_ERROR "Invalid DEFAULT_MIDI selection: " ${DEFAULT_MIDI})
endif(${DEFAULT_MIDI} STREQUAL "jack")

if(WANT_ASM_F21)
    add_definitions(-DASM_F2I)
endif(WANT_ASM_F21)

set(YOSHI_INCLUDES 
    ${YOSHI_INCLUDES}
    ${FONTCONFIG_INCLUDE_DIRS}
    ${FFTW3F_INC_DIR}
    ${JACK_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
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
)

include_directories(AFTER ${YOSHI_INCLUDES})

add_executable(yoshimi ${YOSHI_SOURCES} main.cpp)

target_link_libraries(yoshimi ${YOSHI_LIBRARIES})

install(TARGETS yoshimi RUNTIME DESTINATION bin)

# Custom target to clean CMake generated files
add_custom_target( distclean
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/*.log ${CMAKE_SOURCE_DIR}/src/*.log
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/Makefile
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/install_manifest.txt
    COMMAND rm -f ${CMAKE_SOURCE_DIR}/cmake_install.cmake
    COMMAND find ${CMAKE_SOURCE_DIR} -type f -name CMakeCache.txt | xargs -r rm -f
    COMMAND find ${CMAKE_SOURCE_DIR} -type d -name CMakeFiles | xargs -r rm -rf
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_dependencies(distclean clean)
