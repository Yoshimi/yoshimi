#
#   CMakeLists.txt (LV2)
#
#   Copyright 2014, Andrew Deryabin <andrewderyabin@gmail.com>
#   Copyright 2018-2021, Will Godfrey & others
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

file (GLOB yoshimi_lv2_plugin_files
        YoshimiLV2Plugin.cpp
        YoshimiLV2Plugin.h
        Config.cpp
        Config.h
        lv2extui.h
        lv2extprg.h)
file (GLOB yoshimi_dsp_files
    ../DSP/FFTwrapper.cpp  ../DSP/AnalogFilter.cpp  ../DSP/FormantFilter.cpp
    ../DSP/SVFilter.cpp  ../DSP/Filter.cpp  ../DSP/Unison.cpp
    ../DSP/FFTwrapper.h  ../DSP/AnalogFilter.h  ../DSP/FormantFilter.h
    ../DSP/SVFilter.h  ../DSP/Filter.h  ../DSP/Unison.h)
file (GLOB yoshimi_effects_files
    ../Effects/Alienwah.cpp  ../Effects/Chorus.cpp  ../Effects/Echo.cpp
    ../Effects/EffectLFO.cpp  ../Effects/EffectMgr.cpp  ../Effects/Effect.cpp
    ../Effects/Phaser.cpp  ../Effects/Reverb.cpp  ../Effects/EQ.cpp
    ../Effects/Distorsion.cpp  ../Effects/DynamicFilter.cpp
    ../Effects/Alienwah.h  ../Effects/Chorus.h  ../Effects/Echo.h
    ../Effects/EffectLFO.h  ../Effects/EffectMgr.h  ../Effects/Effect.h
    ../Effects/Phaser.h  ../Effects/Reverb.h  ../Effects/EQ.h
    ../Effects/Distorsion.h  ../Effects/DynamicFilter.h)
file (GLOB yoshimi_misc_files
    #../Misc/Config.cpp ../Misc/Config.h ../ConfBuild.h
    ../Misc/SynthEngine.cpp  ../Misc/Bank.cpp  ../Misc/Microtonal.cpp
    ../Misc/Part.cpp  ../Misc/XMLwrapper.cpp  ../Misc/WavFile.cpp
    ../Misc/SynthEngine.h  ../Misc/Bank.h  ../Misc/Microtonal.h
    ../Misc/Part.h  ../Misc/XMLwrapper.h  ../Misc/WavFile.h)
file (GLOB yoshimi_interface_files
    ../Interface/MidiDecode.cpp ../Interface/MidiDecode.h
    ../Interface/InterChange.cpp ../Interface/InterChange.h
    ../Interface/Data2Text.cpp ../Interface/Data2text.h
    ../Interface/RingBuffer.h
    ../Interface/MidiLearn.cpp ../Interface/MidiLearn.h
    ../UI/MiscGui.cpp ../UI/MiscGui.h)
file (GLOB yoshimi_params_files
    ../Params/ADnoteParameters.cpp  ../Params/EnvelopeParams.cpp
    ../Params/FilterParams.cpp  ../Params/LFOParams.cpp
    ../Params/SUBnoteParameters.cpp  ../Params/PADnoteParameters.cpp
    ../Params/Controller.cpp  ../Params/Presets.cpp ../Params/PresetsStore.cpp
    ../Params/ADnoteParameters.h  ../Params/EnvelopeParams.h
    ../Params/FilterParams.h  ../Params/LFOParams.h
    ../Params/SUBnoteParameters.h  ../Params/PADnoteParameters.h
    ../Params/OscilParameters.cpp ../Params/OscilParameters.h
    ../Params/Controller.h  ../Params/Presets.h ../Params/PresetsStore.h)
file (GLOB yoshimi_synth_files
    ../Synth/ADnote.cpp  ../Synth/Envelope.cpp  ../Synth/LFO.cpp  ../Synth/OscilGen.cpp
    ../Synth/SUBnote.cpp  ../Synth/Resonance.cpp  ../Synth/PADnote.cpp
    ../Synth/ADnote.h  ../Synth/Envelope.h  ../Synth/LFO.h  ../Synth/OscilGen.h
    ../Synth/SUBnote.h  ../Synth/Resonance.h  ../Synth/PADnote.h
    ../Synth/BodyDisposal.h)
file (GLOB yoshimi_musicio_files
    ../MusicIO/MusicIO.cpp ../MusicIO/MusicIO.h)
file (GLOB yoshimi_manifest_ttl
    manifest.ttl)
file (GLOB yoshimi_plugin_ttl
    yoshimi.ttl)

set (FltkUI_names
    "PresetsUI"  "EnvelopeUI"
    "LFOUI"  "FilterUI"  "VirKeyboardUI"
    "ConfigUI"  "SUBnoteUI"  "ResonanceUI"
    "OscilGenUI"  "ADnoteUI"  "PADnoteUI"
    "EffUI"  "BankUI"  "PartUI"
    "MicrotonalUI"  "MasterUI" "MasterMiscUI"
    "ParametersUI" "ConsoleUI" "VectorUI"
    "MidiLearnUI"
)

# workaround fltk_wrap_ui breakage
set (FltkUI_sources)
set (FltkUI_headers)
foreach(basename ${FltkUI_names})
    set (fluidfile "${CMAKE_CURRENT_SOURCE_DIR}/../UI/${basename}.fl")
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
    ../UI/DynamicTooltip.cpp
    ../UI/WidgetPDial.cpp
    ../UI/WidgetMWSlider.cpp
    ../UI/WidgetCheckButton.cpp
    ../UI/WidgetSpinner.cpp
    ../UI/YoshiWin.cpp
)

add_definitions (-DYOSHIMI_LV2_PLUGIN=1)
add_definitions (-fvisibility=hidden)

add_library (yoshimi_lv2 MODULE
            ${yoshimi_lv2_plugin_files}
            ${yoshimi_manifest_ttl}
            ${yoshimi_plugin_ttl}
            ${yoshimi_dsp_files}
            ${yoshimi_effects_files}
            ${yoshimi_misc_files}
            ${yoshimi_interface_files}
            ${yoshimi_params_files}
            ${yoshimi_synth_files}
            ${yoshimi_musicio_files}
            ${FltkUI_sources})

add_custom_target(yoshimi_lv2_ttl
                   COMMAND test -f ${yoshimi_manifest_ttl}
                   COMMAND test -f ${yoshimi_plugin_ttl})

SET_TARGET_PROPERTIES(yoshimi_lv2 PROPERTIES PREFIX "")
include_directories (AFTER
                            ${LV2_INCLUDE_DIRS}
                            ${FLTK_INCLUDE_DIR}
                            ${MXML_INCLUDE_DIRS}
                            ${FONTCONFIG_INCLUDE_DIRS}
                            ${FFTW3F_INC_DIR}
                            ${CMAKE_CURRENT_SOURCE_DIR}
                            ${CMAKE_SOURCE_DIR})

target_link_libraries(yoshimi_lv2
                      ${LV2_LIBRARIES}
                      ${FLTK_LIBRARIES}
                      ${MXML_LIBRARIES}
                      ${LIBSNDFILE_LIBRARIES}
                      ${FFTW3F_LIBRARIES}
                      ${LIBCAIRO_LIBRARIES}
                      z
                      ${LIBDL_LINUX_LIBS}
)

add_dependencies(yoshimi_lv2 yoshimi_lv2_ttl)

install (TARGETS yoshimi_lv2 DESTINATION ${LV2_INSTALL_DIR}/yoshimi.lv2)
install (FILES ${yoshimi_manifest_ttl} ${yoshimi_plugin_ttl}
  DESTINATION ${LV2_INSTALL_DIR}/yoshimi.lv2
)

set_directory_properties(PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${FltkUI_headers}")
