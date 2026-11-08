cmake_minimum_required(VERSION 3.25)
project(wildos-shell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml DBus)
find_package(KF6Config REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_qml_module(wildosshell
    URI org.wildos.shell
    VERSION 1.0
    PLUGIN_TARGET wildosshell
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/org/wildos/shell
    NO_GENERATE_PLUGIN_SOURCE
    CLASS_NAME WildOSShellPlugin
    SOURCES src/pillmask.cpp src/pillmask.h src/plugin.cpp src/launchwatcher.cpp src/launchwatcher.h src/taskbarstyle.cpp src/taskbarstyle.h
)
target_include_directories(wildosshell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(wildosshell PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::DBus KF6::ConfigCore)

set(QML_DIR ${QT6_INSTALL_QML}/org/wildos/shell)
install(TARGETS wildosshell LIBRARY DESTINATION ${QML_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org/wildos/shell/qmldir DESTINATION ${QML_DIR})

add_subdirectory(effects/roundcorners)
add_subdirectory(windowmenu)
