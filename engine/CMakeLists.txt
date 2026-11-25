add_library(kmi_engine STATIC
    src/Algorithm.cpp
    src/Arpeggiator.cpp
    src/Sequencer.cpp
    src/Tuning.cpp
    src/Decimator.cpp
    src/Effects.cpp
    src/Engine.cpp
    src/Envelope.cpp
    src/Randomise.cpp
    src/UserWavetables.cpp
    src/Filters.cpp
    src/Lfo.cpp
    src/Modulation.cpp
    src/Patch.cpp
    src/Presets.cpp
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
    # No fused multiply-add contraction: clang fuses a*b+c by default on ARM, which rounds differently from
    # x86/MSVC and differently between the scalar and SIMD voice paths (feedback FM amplifies the difference).
    target_compile_options(kmi_engine PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off $<$<CONFIG:Release>:-O3>)
endif()
