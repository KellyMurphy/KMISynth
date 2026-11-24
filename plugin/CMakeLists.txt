juce_add_plugin(KMISynth
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "KMI"
    BUNDLE_ID "com.kmi.kmisynth"
    PLUGIN_MANUFACTURER_CODE Kmii
    PLUGIN_CODE Kmis
    FORMATS VST3 Standalone
    PRODUCT_NAME "KMISynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
)

target_sources(KMISynth PRIVATE
    src/Parameters.cpp
    src/PluginEditor.cpp
    src/PluginProcessor.cpp
    src/ui/Controls.cpp
    src/ui/LookAndFeel.cpp
    src/ui/Pages.cpp
    src/ui/PresetBrowser.cpp
    src/ui/RandomPanel.cpp
    src/ui/Views.cpp
)

target_compile_definitions(KMISynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(KMISynth
    PRIVATE
        kmi_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
