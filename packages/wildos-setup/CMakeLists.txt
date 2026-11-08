cmake_minimum_required(VERSION 3.25)
project(wildos-setup VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(wildos-setup-helper src/main.cpp src/service.cpp src/service.h)
target_link_libraries(wildos-setup-helper PRIVATE Qt6::Core Qt6::DBus)

set(LIBEXEC ${CMAKE_INSTALL_PREFIX}/libexec/wildos)
install(TARGETS wildos-setup-helper RUNTIME DESTINATION ${LIBEXEC})
install(PROGRAMS data/wildos-setup-boot data/wildos-setup-session DESTINATION ${LIBEXEC})
install(FILES data/wildos-setup-boot.service data/wildos-setup-helper.service
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
install(FILES data/org.wildos.Setup1.service DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system-services)
install(FILES data/org.wildos.Setup1.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system.d)
install(FILES data/org.wildos.setup.policy DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES data/10-wildos-setup.rules DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/rules.d)
install(FILES data/wildos-setup.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)
install(FILES data/kwinrc DESTINATION ${CMAKE_INSTALL_DATADIR}/wildos-setup)
