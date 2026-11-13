cmake_minimum_required(VERSION 3.22)

# Version is <base>.<build number>. CI passes its run number as the build, so every release has a higher
# plugin version than the last one: hosts re-scan it and the in-plugin updater can tell which is newer.
set(MU_VERSION_BASE "0.4")
set(MU_BUILD_NUMBER "0" CACHE STRING "Build number (CI run number); 0 for local builds")
set(MU_UPDATE_BASE_URL "https://github.com/YbwSolo/musically-unorthodox/releases/latest/download" CACHE STRING "Where the plugin looks for latest.json")
project(MusicallyUnorthodox VERSION ${MU_VERSION_BASE}.${MU_BUILD_NUMBER} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

option(MU_BUILD_TESTS "Build the DSP test runner" ON)
option(MU_LTO "Link-time optimisation (slower builds, faster plugin)" ON)

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

file(GLOB MU_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icons/*.svg")

juce_add_binary_data(MUAssets SOURCES
    Assets/mu_logo.png
    ${MU_ICONS}
    Assets/fonts/Anton-Regular.ttf
    Assets/fonts/Inter-Regular.ttf
    Assets/fonts/Inter-SemiBold.ttf)

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
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Analyzer Dynamics EQ
    HARDENED_RUNTIME_ENABLED TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE)

set(MU_DSP_SOURCES
    Source/dsp/LoudnessMeter.cpp
    Source/dsp/EQEngine.cpp
    Source/dsp/Compressor.cpp
    Source/dsp/Gate.cpp
    Source/dsp/DeEsser.cpp
    Source/dsp/Smoother.cpp
    Source/dsp/Saturator.cpp
    Source/dsp/Rider.cpp
    Source/dsp/StereoTool.cpp
    Source/dsp/Maximizer.cpp
    Source/dsp/VocalSpace.cpp
    Source/dsp/RoleDetector.cpp
    Source/analysis/TonalAnalysis.cpp
    Source/session/BalanceEngine.cpp)

target_sources(MusicallyUnorthodox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/state/ParameterLayout.cpp
    Source/state/ChainPresets.cpp
    Source/session/SessionBus.cpp
    Source/update/Updater.cpp
    Source/ai/LocalAI.cpp
    Source/dsp/SpectrumAnalyzer.cpp
    ${MU_DSP_SOURCES}
    Source/ui/Theme.cpp
    Source/ui/Icons.cpp
    Source/ui/MULookAndFeel.cpp
    Source/ui/Widgets.cpp
    Source/ui/HelpText.cpp
    Source/ui/HeaderBar.cpp
    Source/ui/MeterRail.cpp
    Source/ui/InfoBar.cpp
    Source/ui/MixPage.cpp
    Source/ui/EQPage.cpp
    Source/ui/DynamicsPage.cpp
    Source/ui/TonePage.cpp
    Source/ui/MasterPage.cpp
    Source/ui/LearnPage.cpp
    Source/ui/SettingsPanel.cpp)

target_include_directories(MusicallyUnorthodox PRIVATE Source)

target_compile_definitions(MusicallyUnorthodox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    MU_VERSION_STRING="${PROJECT_VERSION}"
    MU_BUILD_NUMBER=${MU_BUILD_NUMBER}
    MU_UPDATE_BASE_URL="${MU_UPDATE_BASE_URL}")

target_link_libraries(MusicallyUnorthodox PRIVATE
    MUAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_cryptography
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
if(MU_LTO)
    target_link_libraries(MusicallyUnorthodox PUBLIC juce::juce_recommended_lto_flags)
endif()

if(MU_BUILD_TESTS)
    add_executable(MUTests
        Source/tests/TestMain.cpp
        Source/dsp/SpectrumAnalyzer.cpp
        ${MU_DSP_SOURCES})
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
    target_link_libraries(MUSession PRIVATE MusicallyUnorthodox MUAssets juce::juce_audio_utils juce::juce_dsp)
    target_compile_definitions(MUSession PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
endif()

if(MU_BUILD_TESTS)
    add_executable(MUPreview Source/tests/PreviewHarness.cpp)
    target_include_directories(MUPreview PRIVATE Source)
    target_link_libraries(MUPreview PRIVATE MusicallyUnorthodox MUAssets juce::juce_audio_utils juce::juce_dsp)
    target_compile_definitions(MUPreview PRIVATE JUCE_STANDALONE_APPLICATION=1)
endif()
