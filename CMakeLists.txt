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
option(ZEQ_BUILD_TESTS "Build the MatchFit unit-test executable" OFF)
option(ZEQ_COVERAGE "Instrument the test targets for gcov coverage (CI/SonarCloud only)" OFF)
option(ZEQ_RT_SANITIZE "Build ZandersEQRtCheck with clang RealtimeSanitizer (needs clang>=20)" OFF)
option(ZEQ_BUILD_BENCH "Build the ZandersEQBench micro-benchmark (fetches nanobench)" OFF)

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
    COMPANY_NAME            "ZandersAudio"
    BUNDLE_ID               "com.zandersaudio.zanderseq"
    PLUGIN_MANUFACTURER_CODE Zand
    PLUGIN_CODE             Zeq1
    FORMATS                 ${ZEQ_FORMATS}
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
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
    juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# --- Unit tests (pure DSP, no JUCE) -----------------------------------------
if(ZEQ_BUILD_TESTS)
    add_executable(ZandersEQTests tests/MatchFitTests.cpp)
    target_compile_features(ZandersEQTests PRIVATE cxx_std_17)

    add_executable(ZandersEQRoutingTests tests/ChannelRoutingTests.cpp)
    target_compile_features(ZandersEQRoutingTests PRIVATE cxx_std_17)

    add_executable(ZandersEQDynamicsTests tests/DynamicsTests.cpp)
    target_compile_features(ZandersEQDynamicsTests PRIVATE cxx_std_17)

    # Integration harness: drives the REAL processBlock, so it compiles the engine +
    # GUI sources and the JUCE modules straight into the test exe (it does NOT link the
    # ZandersEQ shared-code lib, which would double-consume the JUCE modules). The GUI
    # sources + ZandersEQData are needed only because PluginProcessor's compiled
    # createEditor() references ZandersEqEditor — no window is ever opened at runtime.
    add_executable(ZandersEQEngineTests
        tests/EngineTests.cpp
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/gui/NeonLookAndFeel.cpp
        src/gui/EqGraphComponent.cpp
        src/gui/BandStrip.cpp
        src/gui/PresetBar.cpp
        src/gui/BandEditorRail.cpp)
    target_include_directories(ZandersEQEngineTests PRIVATE src)
    target_compile_definitions(ZandersEQEngineTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(ZandersEQEngineTests PRIVATE
        ZandersEQData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    # Coverage instrumentation, opt-in via -DZEQ_COVERAGE=ON. Applied only to the test
    # executables (none of which link juce_recommended_lto_flags), so there is no LTO/gcov
    # conflict and the shipped plugin build is unaffected.
    if(ZEQ_COVERAGE)
        foreach(t ZandersEQTests ZandersEQRoutingTests ZandersEQDynamicsTests ZandersEQEngineTests)
            target_compile_options(${t} PRIVATE --coverage -O0 -g)
            target_link_options(${t}    PRIVATE --coverage)
        endforeach()
    endif()

    enable_testing()
    add_test(NAME MatchFitTests COMMAND ZandersEQTests)
    add_test(NAME ChannelRoutingTests COMMAND ZandersEQRoutingTests)
    add_test(NAME DynamicsTests COMMAND ZandersEQDynamicsTests)
    add_test(NAME EngineTests COMMAND ZandersEQEngineTests)
endif()

# ----------------------------------------------------------------------------
# Performance & real-time-safety harnesses (opt-in, CI only). Both mirror the
# ZandersEQEngineTests target above: they compile the engine + GUI sources and link the
# JUCE modules directly (NOT the ZandersEQ plugin target, which would double-consume the
# modules) and deliberately omit juce_recommended_lto_flags. The GUI sources + ZandersEQData
# are pulled in only because the compiled createEditor() references ZandersEqEditor — no
# window is ever opened by these headless drivers, so they run under Xvfb with no display.
# ----------------------------------------------------------------------------
set(ZEQ_ENGINE_SOURCES
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/gui/NeonLookAndFeel.cpp
    src/gui/EqGraphComponent.cpp
    src/gui/BandStrip.cpp
    src/gui/PresetBar.cpp
    src/gui/BandEditorRail.cpp)

# --- RealtimeSanitizer driver (clang >= 20 only) ----------------------------
# Annotated audio-thread DSP (processEq and below) is compiled with [[clang::nonblocking]]
# via the ZEQ_RT_NONBLOCKING define; -fsanitize=realtime then flags any malloc/lock/syscall
# inside that subtree at runtime. clang-only: MSVC/gcc have no RealtimeSanitizer.
if(ZEQ_RT_SANITIZE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ZEQ_RT_SANITIZE requires clang (>= 20) for -fsanitize=realtime")
    endif()

    add_executable(ZandersEQRtCheck tests/rt_check.cpp ${ZEQ_ENGINE_SOURCES})
    set_target_properties(ZandersEQRtCheck PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(ZandersEQRtCheck PRIVATE src)
    target_compile_definitions(ZandersEQRtCheck PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        "ZEQ_RT_NONBLOCKING=[[clang::nonblocking]]")   # CMake passes the brackets through fine inside quotes
    target_compile_options(ZandersEQRtCheck PRIVATE -fsanitize=realtime)
    target_link_options   (ZandersEQRtCheck PRIVATE -fsanitize=realtime)
    target_link_libraries(ZandersEQRtCheck PRIVATE
        ZandersEQData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()

# --- nanobench performance micro-benchmark ----------------------------------
# Header-only use: we define ANKERL_NANOBENCH_IMPLEMENT in perf_bench.cpp and add the include
# dir; we do NOT link a nanobench library target (that would duplicate the implementation symbols).
if(ZEQ_BUILD_BENCH)
    FetchContent_Declare(nanobench
        GIT_REPOSITORY https://github.com/martinus/nanobench.git
        GIT_TAG        v4.3.11
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(nanobench)

    add_executable(ZandersEQBench tests/perf_bench.cpp ${ZEQ_ENGINE_SOURCES})
    set_target_properties(ZandersEQBench PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(ZandersEQBench PRIVATE
        src
        ${nanobench_SOURCE_DIR}/src/include)
    target_compile_definitions(ZandersEQBench PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(ZandersEQBench PRIVATE
        ZandersEQData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
