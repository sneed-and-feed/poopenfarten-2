# ==============================================================================
# POOPENFARTEN II (PF-02) · Headless C++20 DSP Verification Test Suite
# ==============================================================================

cmake_minimum_required(VERSION 3.22)
project(poopenfarten2_headless_tests LANGUAGES CXX)

if(NOT TARGET poopenfarten2_headless_dsp_tests)
    add_executable(poopenfarten2_headless_dsp_tests
        pf02_headless_dsp_tests.cpp
    )

    target_include_directories(poopenfarten2_headless_dsp_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../dsp
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine
        ${CMAKE_CURRENT_SOURCE_DIR}/../parameters
    )

    if(TARGET pf02_dsp_core)
        target_link_libraries(poopenfarten2_headless_dsp_tests PRIVATE
            pf02_dsp_core
        )
    endif()

    set_target_properties(poopenfarten2_headless_dsp_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    if(MSVC)
        target_compile_options(poopenfarten2_headless_dsp_tests PRIVATE /W4 /permissive- /utf-8 /fp:fast)
    else()
        target_compile_options(poopenfarten2_headless_dsp_tests PRIVATE -Wall -Wextra -pedantic -ffast-math)
    endif()

    add_test(NAME Pf02HeadlessDspTests COMMAND poopenfarten2_headless_dsp_tests)
endif()
