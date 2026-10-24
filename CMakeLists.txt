cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Poopenfarten II (`PF-02 · Darm-Akustischer & Ultrametrischer Triaden-Synthesizer`)
# JUCE 8 CMake Multi-Format & Multi-Platform Build System
# Formats: Standalone (.exe), VST3 (.vst3), CLAP (.clap), AU (.component)
# ==============================================================================

# Configure Apple deployment targets before project()
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

project(poopenfarten2 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Position-independent code for shared library plugin targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(PF02_BUILD_TESTS "Build PF-02 verification test suites" ON)
option(PF02_USE_WEBVIEW "Build PF-02 with embedded WebView GUI" ON)

# ==============================================================================
# Real-Time Compiler Optimizations & Cross-Platform Warning Flags
# ==============================================================================
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/utf-8 /W4 /O2 /fp:fast)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        add_compile_options(/arch:AVX2)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -ffast-math -fno-finite-math-only)
endif()

# Global JUCE WebView2 definitions
add_compile_definitions(
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    PF02_HAS_JUCE=1
)

include(FetchContent)

# ==============================================================================
# 1. Provision Microsoft.Web.WebView2 for Windows
# ==============================================================================
if(WIN32)
    set(WEBVIEW2_VERSION "1.0.2849.39")
    set(LOCAL_WEBVIEW2_DIR_AS42 "${CMAKE_CURRENT_SOURCE_DIR}/../braun_as-42/build/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
    set(LOCAL_WEBVIEW2_DIR_RB26 "${CMAKE_CURRENT_SOURCE_DIR}/../braun_rb-26/build/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
    set(LOCAL_WEBVIEW2_DIR_MR16 "${CMAKE_CURRENT_SOURCE_DIR}/../braun_mr-16/build/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
    set(LOCAL_WEBVIEW2_DIR_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/build/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
    set(BINARY_WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")

    if(DEFINED JUCE_WEBVIEW2_PACKAGE_LOCATION AND EXISTS "${JUCE_WEBVIEW2_PACKAGE_LOCATION}/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}/build/native/include/WebView2.h")
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${JUCE_WEBVIEW2_PACKAGE_LOCATION}" CACHE PATH "WebView2 location" FORCE)
    elseif(EXISTS "${LOCAL_WEBVIEW2_DIR_AS42}/build/native/include/WebView2.h")
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../braun_as-42/build/packages" CACHE PATH "WebView2 location" FORCE)
    elseif(EXISTS "${LOCAL_WEBVIEW2_DIR_RB26}/build/native/include/WebView2.h")
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../braun_rb-26/build/packages" CACHE PATH "WebView2 location" FORCE)
    elseif(EXISTS "${LOCAL_WEBVIEW2_DIR_MR16}/build/native/include/WebView2.h")
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../braun_mr-16/build/packages" CACHE PATH "WebView2 location" FORCE)
    elseif(EXISTS "${LOCAL_WEBVIEW2_DIR_BUILD}/build/native/include/WebView2.h")
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/build/packages" CACHE PATH "WebView2 location" FORCE)
    elseif(EXISTS "${BINARY_WEBVIEW2_DIR}/build/native/include/WebView2.h")
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_BINARY_DIR}/packages" CACHE PATH "WebView2 location" FORCE)
    else()
        if(NOT EXISTS "${BINARY_WEBVIEW2_DIR}/build/native/include/WebView2.h")
            message(STATUS "Downloading Microsoft.Web.WebView2 NuGet package (${WEBVIEW2_VERSION})...")
            set(WEBVIEW2_ZIP "${CMAKE_BINARY_DIR}/webview2_${WEBVIEW2_VERSION}.zip")
            file(DOWNLOAD
                "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
                "${WEBVIEW2_ZIP}"
                SHOW_PROGRESS
            )
            file(ARCHIVE_EXTRACT
                INPUT "${WEBVIEW2_ZIP}"
                DESTINATION "${BINARY_WEBVIEW2_DIR}"
            )
            file(REMOVE "${WEBVIEW2_ZIP}")
        endif()
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_BINARY_DIR}/packages" CACHE PATH "WebView2 location" FORCE)
    endif()
    message(STATUS "JUCE_WEBVIEW2_PACKAGE_LOCATION set to: ${JUCE_WEBVIEW2_PACKAGE_LOCATION}")
endif()

# ==============================================================================
# 2. Fetch and Configure JUCE 8 (8.0.6)
# ==============================================================================
if(NOT TARGET juce::juce_audio_processors)
    set(LOCAL_JUCE_DIR_AS42 "${CMAKE_CURRENT_SOURCE_DIR}/../braun_as-42/build/_deps/juce-src")
    set(LOCAL_JUCE_DIR_RB26 "${CMAKE_CURRENT_SOURCE_DIR}/../braun_rb-26/build/_deps/juce-src")
    set(LOCAL_JUCE_DIR_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/juce-src")

    if(EXISTS "${LOCAL_JUCE_DIR_AS42}/CMakeLists.txt")
        message(STATUS "Using local cached JUCE 8 from ${LOCAL_JUCE_DIR_AS42}")
        FetchContent_Declare(
            JUCE
            SOURCE_DIR "${LOCAL_JUCE_DIR_AS42}"
        )
    elseif(EXISTS "${LOCAL_JUCE_DIR_RB26}/CMakeLists.txt")
        message(STATUS "Using local cached JUCE 8 from ${LOCAL_JUCE_DIR_RB26}")
        FetchContent_Declare(
            JUCE
            SOURCE_DIR "${LOCAL_JUCE_DIR_RB26}"
        )
    elseif(EXISTS "${LOCAL_JUCE_DIR_BUILD}/CMakeLists.txt")
        message(STATUS "Using local cached JUCE 8 from ${LOCAL_JUCE_DIR_BUILD}")
        FetchContent_Declare(
            JUCE
            SOURCE_DIR "${LOCAL_JUCE_DIR_BUILD}"
        )
    else()
        message(STATUS "Fetching JUCE 8 (8.0.6)...")
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.6
            GIT_SHALLOW    TRUE
        )
    endif()
    FetchContent_MakeAvailable(JUCE)
endif()

# ==============================================================================
# 3. Fetch CLAP-JUCE Extensions for Native CLAP 1.0+ Support
# ==============================================================================
if(NOT TARGET clap_juce_extensions)
    set(LOCAL_CLAP_DIR_MR16 "${CMAKE_CURRENT_SOURCE_DIR}/../braun_mr-16/build/_deps/clap-juce-extensions-src")
    set(LOCAL_CLAP_DIR_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/clap-juce-extensions-src")

    if(EXISTS "${LOCAL_CLAP_DIR_MR16}/CMakeLists.txt")
        message(STATUS "Using local cached clap-juce-extensions from ${LOCAL_CLAP_DIR_MR16}")
        FetchContent_Declare(
            clap-juce-extensions
            SOURCE_DIR "${LOCAL_CLAP_DIR_MR16}"
        )
    elseif(EXISTS "${LOCAL_CLAP_DIR_BUILD}/CMakeLists.txt")
        message(STATUS "Using local cached clap-juce-extensions from ${LOCAL_CLAP_DIR_BUILD}")
        FetchContent_Declare(
            clap-juce-extensions
            SOURCE_DIR "${LOCAL_CLAP_DIR_BUILD}"
        )
    else()
        message(STATUS "Provisioning clap-juce-extensions...")
        FetchContent_Declare(
            clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG        main
            GIT_SUBMODULES_RECURSE TRUE
        )
    endif()
    FetchContent_MakeAvailable(clap-juce-extensions)
endif()

# ==============================================================================
# 4. Packaging Web Assets (Zip Archive & Binary Data)
# ==============================================================================
set(PF02_WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web")
set(PF02_WEB_ASSETS_ZIP "${CMAKE_CURRENT_BINARY_DIR}/web_assets_pf02.zip")

if(NOT EXISTS "${PF02_WEB_ASSETS_ZIP}")
    message(STATUS "Packaging initial web assets archive: ${PF02_WEB_ASSETS_ZIP}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar cf "${PF02_WEB_ASSETS_ZIP}" --format=zip .
        WORKING_DIRECTORY "${PF02_WEB_DIR}"
    )
endif()

file(GLOB_RECURSE PF02_WEB_FILES "${PF02_WEB_DIR}/*")
add_custom_command(
    OUTPUT "${PF02_WEB_ASSETS_ZIP}"
    COMMAND ${CMAKE_COMMAND} -E tar cf "${PF02_WEB_ASSETS_ZIP}" --format=zip .
    WORKING_DIRECTORY "${PF02_WEB_DIR}"
    DEPENDS ${PF02_WEB_FILES}
    COMMENT "Packaging PF-02 web assets into web_assets_pf02.zip"
)
add_custom_target(PackagePf02WebAssets DEPENDS "${PF02_WEB_ASSETS_ZIP}")

juce_add_binary_data(Pf02WebAssets SOURCES "${PF02_WEB_ASSETS_ZIP}")
add_dependencies(Pf02WebAssets PackagePf02WebAssets)

if(WIN32)
    set(WEBVIEW2_INCLUDE_DIR "${JUCE_WEBVIEW2_PACKAGE_LOCATION}/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}/build/native/include")
endif()

# ==============================================================================
# 5. pf02_dsp_core Static Library (Decoupled Physical Modeling DSP Core)
# ==============================================================================
add_library(pf02_dsp_core STATIC
    source/dsp/DspMath.h
    source/dsp/DarkSnailMath.h
    source/dsp/MucosalExciter.h
    source/dsp/MucosalExciter.cpp
    source/dsp/AeroFluidEngine.h
    source/dsp/AeroFluidEngine.cpp
    source/dsp/ModalResonatorMatrix.h
    source/dsp/ModalResonatorMatrix.cpp
    source/dsp/LorenzAttractor.h
    source/dsp/LorenzAttractor.cpp
    source/dsp/MasteringChain.h
    source/dsp/MasteringChain.cpp
    source/dsp/SpectralModalExtractor.h
    source/dsp/SpectralModalExtractor.cpp
    source/parameters/Parameters.h
    source/parameters/PresetManager.h
    source/parameters/PresetManager.cpp
    source/engine/PhysicalVoice.h
    source/engine/PhysicalVoice.cpp
    source/engine/VoiceManager.h
    source/engine/VoiceManager.cpp
    source/engine/Pf02AudioEngine.h
    source/engine/Pf02AudioEngine.cpp
)

target_include_directories(pf02_dsp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/source/engine
)

target_link_libraries(pf02_dsp_core PUBLIC
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ==============================================================================
# 6. Target Plugin Formats & poopenfarten2 Plugin Target Declaration
# ==============================================================================
set(PF02_FORMATS VST3 Standalone)
if(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(PF02_FORMATS AUv3 Standalone)
    else()
        list(APPEND PF02_FORMATS AU AUv3)
    endif()
endif()

set(PF02_EXTRA_PLUGIN_ARGS "")
if(WIN32)
    list(APPEND PF02_EXTRA_PLUGIN_ARGS NEEDS_WEBVIEW2 TRUE)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND PF02_EXTRA_PLUGIN_ARGS NEEDS_WEB_BROWSER TRUE)
endif()

juce_add_plugin(poopenfarten2
    COMPANY_NAME "Sneed's Feed & Seed Ltd."
    BUNDLE_ID "com.sneedandfeed.poopenfarten2"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE SndF
    PLUGIN_CODE Pf02
    FORMATS ${PF02_FORMATS}
    PRODUCT_NAME "PF-02"
    VERSION "0.1.0"
    ${PF02_EXTRA_PLUGIN_ARGS}
)

target_sources(poopenfarten2 PRIVATE
    source/plugin/PluginProcessor.cpp
    source/plugin/PluginProcessor.h
    source/plugin/PluginEditor.cpp
    source/plugin/PluginEditor.h
)

target_include_directories(poopenfarten2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/source/engine
)

if(WIN32 AND DEFINED WEBVIEW2_INCLUDE_DIR)
    target_include_directories(poopenfarten2 PRIVATE ${WEBVIEW2_INCLUDE_DIR})
endif()

target_compile_definitions(poopenfarten2 PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    PF02_HAS_JUCE=1
    PF02_HAS_JUCE_PROCESSORS=1
)

target_link_libraries(poopenfarten2 PRIVATE
    pf02_dsp_core
    Pf02WebAssets
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Linux embedded WebKitGTK sub-process requirement
if(UNIX AND NOT APPLE)
    get_directory_property(_sub_target JUCE_EMBEDDED_LINUX_SUBPROCESS_TARGET)
    if(_sub_target)
        target_link_libraries(poopenfarten2 PUBLIC ${_sub_target})
    endif()
endif()

# Attach CLAP format target via clap-juce-extensions
if(TARGET clap_juce_extensions)
    clap_juce_extensions_plugin(
        TARGET poopenfarten2
        CLAP_ID "com.sneedandfeed.poopenfarten2"
        CLAP_FEATURES instrument synthesizer physical-modelling
    )
endif()

# ==============================================================================
# 7. Verification Test Suites & Runners
# ==============================================================================
if(PF02_BUILD_TESTS)
    # Core DSP Unit Tests
    add_executable(pf02_dsp_core_tests
        tests/test_dsp_core.cpp
    )
    target_link_libraries(pf02_dsp_core_tests PRIVATE pf02_dsp_core)
    target_include_directories(pf02_dsp_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Challenger M1-2 Tests
    add_executable(pf02_challenger_m1_2_tests
        tests/test_challenger_m1_2.cpp
    )
    target_link_libraries(pf02_challenger_m1_2_tests PRIVATE pf02_dsp_core)
    target_include_directories(pf02_challenger_m1_2_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Challenger M1-1 Adversarial Stress Tests
    add_executable(pf02_dsp_adversarial_tests
        tests/test_adversarial_stress.cpp
    )
    target_link_libraries(pf02_dsp_adversarial_tests PRIVATE pf02_dsp_core)
    target_include_directories(pf02_dsp_adversarial_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Milestone M2 Engine & APVTS Tests
    add_executable(pf02_m2_engine_tests
        tests/test_m2_engine.cpp
    )
    target_link_libraries(pf02_m2_engine_tests PRIVATE pf02_dsp_core)
    target_include_directories(pf02_m2_engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Headless DSP Verification Test Suite (Tiers 1-4)
    enable_testing()
    add_subdirectory(source/tests)
endif()
