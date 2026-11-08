cmake_minimum_required(VERSION 3.21)
project(wildos-installer-qml LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Qml)
qt_standard_project_setup(REQUIRES 6.5)
include(GNUInstallDirs)

# The installer pages are loaded by Calamares, one QML engine per page. This
# module is what they share (see installer.h).
qt_add_qml_module(wildosinstaller
    URI WildOS.Installer
    VERSION 1.0
    PLUGIN_TARGET wildosinstaller
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/WildOS/Installer
    SOURCES
        installer.h installer.cpp
)

set(WILDOS_QML_DIR "${CMAKE_INSTALL_LIBDIR}/qt6/qml/WildOS/Installer")
install(TARGETS wildosinstaller LIBRARY DESTINATION ${WILDOS_QML_DIR})
install(FILES ${CMAKE_BINARY_DIR}/WildOS/Installer/qmldir DESTINATION ${WILDOS_QML_DIR})
