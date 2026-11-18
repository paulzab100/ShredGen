cmake_minimum_required(VERSION 3.22)
project(ShredGen VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# VST3, который принимает MIDI и отдаёт MIDI.
# IS_SYNTH TRUE нужен, чтобы большинство DAW позволили вставить его на инструментальную дорожку
# и отправить его MIDI-выход на другой синтезатор.
juce_add_plugin(ShredGen
    COMPANY_NAME "YourBrand"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Ybrd
    PLUGIN_CODE Shgn
    FORMATS VST3
    PRODUCT_NAME "ShredGen"
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(ShredGen PRIVATE PluginProcessor.cpp PluginProcessor.h)

target_compile_definitions(ShredGen PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ShredGen
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
