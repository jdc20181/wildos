cmake_minimum_required(VERSION 3.25)
# C for the Wayland protocol code qtwaylandscanner generates.
project(wildos-launch VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui DBus WaylandClient)
find_package(PlasmaWaylandProtocols REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6WindowSystem REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(wildos-launchd
    src/main.cpp
    src/feedback.cpp src/feedback.h
    src/launcher.cpp src/launcher.h
    src/store.cpp src/store.h
    src/preload.cpp src/preload.h
)
target_compile_definitions(wildos-launchd PRIVATE WILDOS_LAUNCH_VERSION="${PROJECT_VERSION}")

qt6_generate_wayland_protocol_client_sources(wildos-launchd
    FILES ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-window-management.xml
)

target_include_directories(wildos-launchd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(wildos-launchd PRIVATE
    Qt6::Gui Qt6::DBus Qt6::WaylandClient
    KF6::ConfigCore KF6::WindowSystem
)

# The desktop files name this exact path, which is how KWin decides to
# grant the launch feedback interface.
install(TARGETS wildos-launchd RUNTIME DESTINATION libexec/wildos)
install(FILES data/org.wildos.launchd.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/autostart/org.wildos.launchd.desktop DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart)
install(FILES data/systemd/wildos-launch-preload.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user)
install(DIRECTORY kwin/org.wildos.launch DESTINATION ${CMAKE_INSTALL_DATADIR}/kwin/scripts)
