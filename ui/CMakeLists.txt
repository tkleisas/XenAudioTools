add_library(xen_ui STATIC
    src/ScoreCanvas.cpp
    src/XenEditorPanel.cpp
)
target_include_directories(xen_ui PUBLIC include)
target_compile_definitions(xen_ui PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)
target_link_libraries(xen_ui PUBLIC xen_engine juce::juce_gui_basics)
target_compile_features(xen_ui PUBLIC cxx_std_17)
target_compile_options(xen_ui PRIVATE -Wall -Wextra -Wpedantic)
