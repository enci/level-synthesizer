add_executable(level_synthesizer_tests
    test_grid.cpp
    test_tagset.cpp
)

target_link_libraries(level_synthesizer_tests PRIVATE
    level_synthesizer_library
    Catch2::Catch2WithMain
)

catch_discover_tests(level_synthesizer_tests)
