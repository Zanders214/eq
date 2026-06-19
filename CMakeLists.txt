cmake_minimum_required(VERSION 3.22)

project(ZandersEQ VERSION 0.1.0 LANGUAGES C CXX)

# ----------------------------------------------------------------------------
# ZandersEQ — a FabFilter Pro-Q-style parametric EQ plugin (VST3 / AU / Standalone)
# Built on JUCE. JUCE is pulled in reproducibly via FetchContent (pinned tag).
# ----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(ZEQ_BUILD_STANDALONE "Build the Standalone target (handy for testing)" ON)

# --- JUCE -------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# --- Plugin formats ---------------------------------------------------------
set(ZEQ_FORMATS VST3)
if(APPLE)
    list(APPEND ZEQ_FORMATS AU)        # AudioUnit is macOS only
endif()
if(ZEQ_BUILD_STANDALONE)
    list(APPEND ZEQ_FORMATS Standalone)
endif()

juce_add_plugin(ZandersEQ
    PRODUCT_NAME            "ZandersEQ"
    COMPANY_NAME            "Neon Plugins"
    BUNDLE_ID               "com.neonplugins.zanderseq"
    PLUGIN_MANUFACTURER_CODE Neon
    PLUGIN_CODE             Zeq1
    FORMATS                 ${ZEQ_FORMATS}
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         "Fx" "EQ"
    AU_MAIN_TYPE            "kAudioUnitType_Effect")

# --- Brand fonts bundled as binary data (offline, self-contained) -----------
juce_add_binary_data(ZandersEQData
    SOURCES
        resources/fonts/SpaceGroteskMedium.ttf
        resources/fonts/SpaceGroteskSemiBold.ttf
        resources/fonts/SpaceGroteskBold.ttf
        resources/fonts/JetBrainsMonoRegular.ttf
        resources/fonts/JetBrainsMonoMedium.ttf)

# Static-link the binary-data lib cleanly into the plugin.
set_target_properties(ZandersEQData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# --- Sources ----------------------------------------------------------------
target_sources(ZandersEQ PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/gui/NeonLookAndFeel.cpp
    src/gui/EqGraphComponent.cpp
    src/gui/BandStrip.cpp
    src/gui/PresetBar.cpp
    src/gui/BandEditorRail.cpp)

target_include_directories(ZandersEQ PRIVATE src)

target_compile_definitions(ZandersEQ PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(ZandersEQ PRIVATE
    ZandersEQData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
