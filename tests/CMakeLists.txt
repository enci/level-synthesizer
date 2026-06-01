add_executable(synth_tests
    test_grid.cpp
)

target_link_libraries(synth_tests PRIVATE
    synth_library
    Catch2::Catch2WithMain
)

catch_discover_tests(synth_tests)
