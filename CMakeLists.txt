cmake_minimum_required(VERSION 3.22)
project(VibeVerb VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

# JUCE framework
add_subdirectory(JUCE)

# Plugin target
juce_add_plugin(VibeVerb
    PRODUCT_NAME               "VibeVerb"
    VERSION                    ${PROJECT_VERSION}
    COMPANY_NAME               "Mr.W"
    BUNDLE_ID                  "com.mrw.vibeverb"
    PLUGIN_MANUFACTURER_CODE   MrWw
    PLUGIN_CODE                VbVb
    FORMATS                    VST3
    VST3_CATEGORIES            "Fx|Reverb"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    PLUGIN_MANUFACTURER        "Mr.W"
)

target_sources(VibeVerb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ReverbEngine.cpp
)

target_include_directories(VibeVerb PRIVATE Source)

target_link_libraries(VibeVerb PRIVATE
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
    juce::juce_data_structures
)

target_compile_definitions(VibeVerb PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
)
