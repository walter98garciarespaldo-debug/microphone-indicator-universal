cmake_minimum_required(VERSION 3.16)
project(microphone_indicator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
    # Configuración de compilación para Windows (si se usa CMake en lugar de build.ps1)
    add_executable(microphone-indicator-windows WIN32
        src/windows/main.cpp
        resources/resources.rc
    )
    target_link_libraries(microphone-indicator-windows PRIVATE
        user32 gdi32 shell32 ole32 advapi32
    )
elseif (UNIX AND NOT APPLE)
    # Configuración de compilación para Linux
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PULSE REQUIRED libpulse libpulse-mainloop-glib glib-2.0)

    add_executable(microphone-indicator-linux
        src/linux/main.cpp
    )

    target_include_directories(microphone-indicator-linux PRIVATE
        ${PULSE_INCLUDE_DIRS}
    )

    target_link_libraries(microphone-indicator-linux PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        ${PULSE_LIBRARIES}
    )

    # Copiar recursos a la carpeta de salida
    add_custom_command(TARGET microphone-indicator-linux POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:microphone-indicator-linux>/resources
    )
endif()
