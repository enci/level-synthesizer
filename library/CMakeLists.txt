add_library(synth_library STATIC
    grid.cpp
)

target_include_directories(synth_library PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(synth_library PUBLIC nlohmann_json::nlohmann_json)
