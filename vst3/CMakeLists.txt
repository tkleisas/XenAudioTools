juce_add_plugin(XenakisUPIC
    COMPANY_NAME "bokontep"
    PRODUCT_NAME "Xenakis UPIC"
    PLUGIN_MANUFACTURER_CODE Boko
    PLUGIN_CODE XnUp
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Instrument|Synth"
    BUNDLE_ID "com.bokontep.xenakisupic"
)

target_sources(XenakisUPIC PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(XenakisUPIC PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_include_directories(XenakisUPIC PRIVATE
    "${XEN_JUCE_DIR}/modules/juce_audio_processors/format_types/VST3_SDK"
)

target_link_libraries(XenakisUPIC PRIVATE
    xen_ui
    xen_engine
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
