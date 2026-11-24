add_executable(kmi_engine_tests engine_tests.cpp)
target_link_libraries(kmi_engine_tests PRIVATE kmi_engine)
set_target_properties(kmi_engine_tests PROPERTIES FOLDER "KMISynth")
if(MSVC)
    target_compile_options(kmi_engine_tests PRIVATE /W4 /permissive-)
endif()
add_test(NAME kmi_engine_tests COMMAND kmi_engine_tests)

# CPU breakdown benchmark (not a test; run manually: kmi_bench [seconds])
add_executable(kmi_bench bench.cpp)
target_link_libraries(kmi_bench PRIVATE kmi_engine)
set_target_properties(kmi_bench PROPERTIES FOLDER "KMISynth")

# Renders every factory preset playing a short phrase to WAV files (kmi_render <out-dir>).
add_executable(kmi_render render_demos.cpp)
target_link_libraries(kmi_render PRIVATE kmi_engine)
set_target_properties(kmi_render PROPERTIES FOLDER "KMISynth")
