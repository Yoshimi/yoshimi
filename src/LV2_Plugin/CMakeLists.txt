file (GLOB yoshimi_lv2_plugin_files
        YoshimiLV2Plugin.cpp
        YoshimiLV2Plugin.h)
file (GLOB yoshimi_dsp_files
    ../DSP/*.cpp ../DSP/*.h)
file (GLOB yoshimi_effects_files
    ../Effects/*.cpp ../Effects/*.h)
file (GLOB yoshimi_misc_files
    ../Misc/*.cpp ../Misc/*.h)
file (GLOB yoshimi_params_files
    ../Params/*.cpp ../Params/*.h)
file (GLOB yoshimi_synth_files
    ../Synth/*.cpp ../Synth/*.h)

add_library (Yoshimi MODULE
            ${yoshimi_lv2_plugin_files}
            ${yoshimi_dsp_files}
            ${yoshimi_effects_files}
            ${yoshimi_misc_files}
            ${yoshimi_params_files}
            ${yoshimi_synth_files})
SET_TARGET_PROPERTIES(Yoshimi PROPERTIES PREFIX "")
target_include_directories (Yoshimi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
