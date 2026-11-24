cmake_minimum_required(VERSION 3.25)

project(KMISynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(KMI_BUILD_PLUGIN "Build the JUCE VST3/Standalone plugin" ON)
option(KMI_BUILD_TESTS "Build the engine test suite" ON)

# Framework-agnostic DSP engine (no JUCE dependency).
add_subdirectory(engine)

if(KMI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(KMI_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(plugin)
endif()
