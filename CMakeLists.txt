cmake_minimum_required(VERSION 3.22)
project(MixCheckAI VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MixCheckAI
  COMPANY_NAME "MixCheck"
  PLUGIN_MANUFACTURER_CODE Mchk
  PLUGIN_CODE Mcai
  FORMATS VST3 Standalone
  PRODUCT_NAME "Mix Check AI"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Analyzer Fx
  MICROPHONE_PERMISSION_ENABLED TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(MixCheckAI PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_include_directories(MixCheckAI PRIVATE Source)

target_compile_definitions(MixCheckAI PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_MP3AUDIOFORMAT=1
  JUCE_USE_OGGVORBIS=1)

if(MSVC)
  target_compile_options(MixCheckAI PRIVATE /utf-8)
endif()

target_link_libraries(MixCheckAI
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags
         juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)

# Engine unit test (no JUCE needed):  cmake --build . --target core_test
add_executable(core_test Tests/core_test.cpp)
target_include_directories(core_test PRIVATE Source/Core)
