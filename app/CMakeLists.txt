juce_add_gui_app(XenakisUPICStudio
    COMPANY_NAME "bokontep"
    PRODUCT_NAME "Xenakis UPIC Studio"
    BUNDLE_ID "com.bokontep.xenakisupicstudio"
    MICROPHONE_PERMISSION_ENABLED FALSE
)

target_sources(XenakisUPICStudio PRIVATE Source/Main.cpp)

target_compile_definitions(XenakisUPICStudio PRIVATE
    XEN_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(XenakisUPICStudio PRIVATE
    xen_ui
    xen_engine
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
