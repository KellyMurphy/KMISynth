add_library(kmi_engine STATIC
    src/Algorithm.cpp
    src/Decimator.cpp
    src/Effects.cpp
    src/Engine.cpp
    src/Envelope.cpp
    src/Filters.cpp
    src/Lfo.cpp
    src/Modulation.cpp
    src/Patch.cpp
    src/Voice.cpp
    src/Wavetables.cpp
)

target_include_directories(kmi_engine PUBLIC include)
target_compile_features(kmi_engine PUBLIC cxx_std_20)
set_target_properties(kmi_engine PROPERTIES FOLDER "KMISynth" POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    # SSE2 baseline (x64 default); an AVX2 path is a Phase 2 item (R-RT-6).
    target_compile_options(kmi_engine PRIVATE /W4 /permissive- $<$<CONFIG:Release>:/O2 /Oi>)
else()
    target_compile_options(kmi_engine PRIVATE -Wall -Wextra -Wpedantic $<$<CONFIG:Release>:-O3>)
endif()
