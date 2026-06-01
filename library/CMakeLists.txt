add_library(level_synthesizer_library STATIC
    grid.cpp
    tagset.cpp
)

target_include_directories(level_synthesizer_library PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(level_synthesizer_library PUBLIC nlohmann_json::nlohmann_json)
