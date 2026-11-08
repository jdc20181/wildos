cmake_minimum_required(VERSION 3.25)
project(wildos-winmode-setup VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui DBus Network Qml Quick QuickControls2 Svg)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(wildos-winmode-setup src/main.cpp)

file(GLOB QML_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/*.qml)
foreach(file ${QML_SOURCES})
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(wildos-winmode-setup
    URI WildOS.WinMode
    QML_FILES ${QML_SOURCES}
    RESOURCES images/wild-os-icon.svg
    SOURCES
        src/setupapp.cpp src/setupapp.h
        src/licence.cpp src/licence.h
        src/pccheck.cpp src/pccheck.h
        src/options.cpp src/options.h
)

target_include_directories(wildos-winmode-setup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(wildos-winmode-setup PRIVATE
    Qt6::Gui Qt6::DBus Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg
)

install(TARGETS wildos-winmode-setup RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/wildos)
