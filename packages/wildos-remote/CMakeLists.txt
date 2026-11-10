cmake_minimum_required(VERSION 3.25)
project(wildos-remote VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets DBus Network Qml Quick QuickWidgets QuickControls2)
# Qt6::GuiPrivate only for QPlatformNativeInterface: the wl_surface behind a
# window, which Qt 6.8 offers no public way to reach.
find_package(Qt6Keychain REQUIRED)
find_package(qtermwidget6 REQUIRED)
find_package(KF6WindowSystem REQUIRED)

# The Remote Desktop engine Windows mode draws its apps with. The package
# build copies it in beside this file (build/build.sh); a build from the
# source tree reaches it next door.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rdp-engine/engine.cmake)
    set(WILDOS_RDP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rdp-engine)
else()
    set(WILDOS_RDP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../wildos-rdp)
endif()
include(${WILDOS_RDP_DIR}/engine.cmake)

# "Send all shortcuts there" asks KWin through this protocol.
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
set(INHIBIT_XML ${WAYLAND_PROTOCOLS}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h
           ${CMAKE_CURRENT_BINARY_DIR}/keyboard-shortcuts-inhibit-unstable-v1-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header ${INHIBIT_XML} ${CMAKE_CURRENT_BINARY_DIR}/keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${INHIBIT_XML} ${CMAKE_CURRENT_BINARY_DIR}/keyboard-shortcuts-inhibit-unstable-v1-protocol.c
    DEPENDS ${INHIBIT_XML})

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(wildos-remote src/main.cpp)
target_compile_definitions(wildos-remote PRIVATE
    WILDOS_REMOTE_VERSION="${PROJECT_VERSION}"
    WILDOS_REMOTE_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/wildos-remote"
    WILDOS_REMOTE_SOURCEDIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

file(GLOB QML_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/*.qml)
set_source_files_properties(qml/RemoteIcons.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
foreach(file ${QML_SOURCES})
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(wildos-remote
    URI WildOS.Remote
    QML_FILES ${QML_SOURCES}
    RESOURCES data/org.wildos.remote.svg
    SOURCES
        src/app.cpp src/app.h
        src/askpass.cpp src/askpass.h
        src/desktopview.cpp src/desktopview.h
        src/filebrowser.cpp src/filebrowser.h
        src/hostkeys.cpp src/hostkeys.h
        src/hosts.cpp src/hosts.h
        src/keychain.cpp src/keychain.h
        src/notify.cpp src/notify.h
        src/paths.h
        src/remotedesktop.cpp src/remotedesktop.h
        src/secrets.cpp src/secrets.h
        src/sessionwindow.cpp src/sessionwindow.h
        src/sftp.cpp src/sftp.h
        src/shortcuts.cpp src/shortcuts.h
        src/snippets.cpp src/snippets.h
        src/terminalview.cpp src/terminalview.h
        src/windowframe.cpp src/windowframe.h
        ${CMAKE_CURRENT_BINARY_DIR}/keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h
        ${CMAKE_CURRENT_BINARY_DIR}/keyboard-shortcuts-inhibit-unstable-v1-protocol.c
)

target_include_directories(wildos-remote PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(wildos-remote PRIVATE
    wildos-rdp-engine
    Qt6::GuiPrivate PkgConfig::WAYLAND
    Qt6::Widgets Qt6::DBus Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickWidgets Qt6::QuickControls2
    qt6keychain
    qtermwidget6
    KF6::WindowSystem
)

install(TARGETS wildos-remote RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/org.wildos.remote.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/org.wildos.remote.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES data/Remote.colorscheme DESTINATION ${CMAKE_INSTALL_DATADIR}/wildos-remote)
