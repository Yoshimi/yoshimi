file (GLOB yoshimi_lv2_plugin_files
        YoshimiLV2Plugin.cpp
        YoshimiLV2Plugin.h
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
    ../Misc/Config.cpp ../Misc/Config.h
    ../Misc/SynthEngine.cpp  ../Misc/Bank.cpp  ../Misc/Microtonal.cpp
    ../Misc/Part.cpp  ../Misc/XMLwrapper.cpp  ../Misc/MiscFuncs.cpp
    ../Misc/SynthEngine.h  ../Misc/Bank.h  ../Misc/Microtonal.h
    ../Misc/Part.h  ../Misc/XMLwrapper.h  ../Misc/MiscFuncs.h)
file (GLOB yoshimi_params_files
    ../Params/ADnoteParameters.cpp  ../Params/EnvelopeParams.cpp
    ../Params/FilterParams.cpp  ../Params/LFOParams.cpp
    ../Params/SUBnoteParameters.cpp  ../Params/PADnoteParameters.cpp
    ../Params/Controller.cpp  ../Params/Presets.cpp ../Params/PresetsStore.cpp
    ../Params/ControlInterface.cpp
    ../Params/ADnoteParameters.h  ../Params/EnvelopeParams.h
    ../Params/FilterParams.h  ../Params/LFOParams.h
    ../Params/SUBnoteParameters.h  ../Params/PADnoteParameters.h
    ../Params/Controller.h  ../Params/Presets.h ../Params/PresetsStore.h
    ../Params/ControlInterface.h)
file (GLOB yoshimi_synth_files
    ../Synth/ADnote.cpp  ../Synth/Envelope.cpp  ../Synth/LFO.cpp  ../Synth/OscilGen.cpp
    ../Synth/SUBnote.cpp  ../Synth/Resonance.cpp  ../Synth/PADnote.cpp
    ../Synth/BodyDisposal.cpp
    ../Synth/ADnote.h  ../Synth/Envelope.h  ../Synth/LFO.h  ../Synth/OscilGen.h
    ../Synth/SUBnote.h  ../Synth/Resonance.h  ../Synth/PADnote.h
    ../Synth/BodyDisposal.h)
file (GLOB yoshimi_musicio_files
    ../MusicIO/MusicIO.cpp ../MusicIO/MusicIO.h ../MusicIO/Runtime.h)
file (GLOB yoshimi_manifest_ttl
    manifest.ttl)
file (GLOB yoshimi_plugin_ttl
    yoshimi.ttl)

set (GuiFluids
    ../UI/WidgetPDialUI.fl  ../UI/PresetsUI.fl  ../UI/EnvelopeUI.fl
    ../UI/LFOUI.fl  ../UI/FilterUI.fl  ../UI/VirKeyboardUI.fl
    ../UI/ConfigUI.fl  ../UI/SUBnoteUI.fl  ../UI/ResonanceUI.fl
    ../UI/OscilGenUI.fl  ../UI/ADnoteUI.fl  ../UI/PADnoteUI.fl
    ../UI/EffUI.fl  ../UI/BankUI.fl  ../UI/PartUI.fl
    ../UI/MicrotonalUI.fl  ../UI/MasterUI.fl    ../UI/MasterMiscUI.fl
    ../UI/ParametersUI.fl ../UI/ConsoleUI.fl
)

fltk_wrap_ui (yoshimi_lv2 ${GuiFluids})
set_source_files_properties (../UI/MasterUI.h PROPERTIES GENERATED 1)

add_definitions (-DYOSHIMI_LV2_PLUGIN=1)

add_library (yoshimi_lv2 MODULE
            ${yoshimi_lv2_plugin_files}
            ${yoshimi_manifest_ttl}
            ${yoshimi_plugin_ttl}
            ${yoshimi_dsp_files}
            ${yoshimi_effects_files}
            ${yoshimi_misc_files}
            ${yoshimi_params_files}
            ${yoshimi_synth_files}
            ${yoshimi_musicio_files}
            ${yoshimi_lv2_FLTK_UI_SRCS})

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
                      ${JACK_LIBRARIES}
                      z
                      dl
)

add_dependencies(yoshimi_lv2 yoshimi_lv2_ttl)

install (TARGETS yoshimi_lv2 DESTINATION ${CMAKE_INSTALL_PREFIX}/${LV2_INSTALL_DIR}/yoshimi.lv2)
install (FILES ${yoshimi_manifest_ttl} ${yoshimi_plugin_ttl}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/${LV2_INSTALL_DIR}/yoshimi.lv2
)
