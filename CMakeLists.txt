cmake_minimum_required(VERSION 3.22)
project(MusicallyUnorthodox VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

option(MU_BUILD_TESTS "Build the DSP test runner" ON)

# JUCE: use a local checkout if JUCE_DIR is given, otherwise fetch it.
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(MUAssets SOURCES
    Assets/mu_logo_white.png)

juce_add_plugin(MusicallyUnorthodox
    COMPANY_NAME "Musically Unorthodox"
    COMPANY_WEBSITE "https://musicallyunorthodox.com"
    BUNDLE_ID "com.musicallyunorthodox.mixassistant"
    PLUGIN_MANUFACTURER_CODE Muno
    PLUGIN_CODE Muxa
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Musically Unorthodox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Analyzer Dynamics EQ
    HARDENED_RUNTIME_ENABLED TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE)

target_sources(MusicallyUnorthodox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/state/ParameterLayout.cpp
    Source/session/SessionBus.cpp
    Source/session/BalanceEngine.cpp
    Source/dsp/LoudnessMeter.cpp
    Source/dsp/SpectrumAnalyzer.cpp
    Source/dsp/EQEngine.cpp
    Source/dsp/Compressor.cpp
    Source/dsp/VocalSpace.cpp
    Source/dsp/RoleDetector.cpp
    Source/license/License.cpp
    Source/ui/Theme.cpp
    Source/ui/MULookAndFeel.cpp
    Source/ui/HeaderBar.cpp
    Source/ui/MixPage.cpp
    Source/ui/EQPage.cpp
    Source/ui/CompressorPage.cpp
    Source/ui/LevelsPage.cpp
    Source/ui/SettingsPage.cpp
    Source/ui/Meters.cpp)

target_include_directories(MusicallyUnorthodox PRIVATE Source)

target_compile_definitions(MusicallyUnorthodox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    MU_VERSION_STRING="${PROJECT_VERSION}")

target_link_libraries(MusicallyUnorthodox PRIVATE
    MUAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_cryptography
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(MU_BUILD_TESTS)
    add_executable(MUTests
        Source/tests/TestMain.cpp
        Source/dsp/LoudnessMeter.cpp
        Source/dsp/EQEngine.cpp
        Source/dsp/Compressor.cpp
        Source/dsp/VocalSpace.cpp
        Source/dsp/RoleDetector.cpp
        Source/session/BalanceEngine.cpp)
    target_include_directories(MUTests PRIVATE Source)
    target_compile_definitions(MUTests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODULE_AVAILABLE_juce_dsp=1)
    target_link_libraries(MUTests PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()

if(MU_BUILD_TESTS)
    add_executable(MUSession Source/tests/SessionHarness.cpp)
    target_include_directories(MUSession PRIVATE Source)
    target_link_libraries(MUSession PRIVATE MusicallyUnorthodox MUAssets juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography)
    target_compile_definitions(MUSession PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
endif()
