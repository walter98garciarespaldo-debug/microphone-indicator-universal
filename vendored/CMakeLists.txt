cmake_minimum_required(VERSION 3.20)
project(microphone_indicator_windows VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(microphone-indicator-windows WIN32
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../resources/resources.rc
)

target_compile_definitions(microphone-indicator-windows PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(microphone-indicator-windows PRIVATE
    user32
    gdi32
    shell32
    ole32
    advapi32
)

set_target_properties(microphone-indicator-windows PROPERTIES
    OUTPUT_NAME "microphone-indicator-windows"
)
