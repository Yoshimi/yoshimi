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

set(yoshimi_manifest_ttl manifest.ttl)

add_library (yoshimi_lv2 MODULE
            ${yoshimi_lv2_plugin_files}
            ${yoshimi_dsp_files}
            ${yoshimi_effects_files}
            ${yoshimi_misc_files}
            ${yoshimi_params_files}
            ${yoshimi_synth_files})
SET_TARGET_PROPERTIES(yoshimi_lv2 PROPERTIES PREFIX "")
target_include_directories (yoshimi_lv2 PUBLIC ${LV2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})

target_link_libraries(yoshimi_lv2 ${LV2_LIBRARIES} )

install (TARGETS yoshimi_lv2 DESTINATION ${CMAKE_INSTALL_PREFIX}/${LV2_INSTALL_DIR}/yoshimi.lv2)
install (FILES manifest.ttl yoshimi.ttl
  DESTINATION ${CMAKE_INSTALL_PREFIX}/${LV2_INSTALL_DIR}/yoshimi.lv2
)
