cmake_minimum_required(VERSION 3.25)
project(wildos-settings VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets DBus Qml Quick QuickControls2)
find_package(KF6KCMUtils REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Service REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(KF6ColorScheme REQUIRED)
find_package(packagekitqt6 REQUIRED)
find_package(Cups REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(wildos-settings src/main.cpp)
target_compile_definitions(wildos-settings PRIVATE WILDOS_SETTINGS_VERSION="${PROJECT_VERSION}")

file(GLOB_RECURSE QML_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/*.qml)
set_source_files_properties(qml/components/Metrics.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/components/Pages.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/components/Navigator.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/components/Format.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/components/Protection.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(wildos-settings
    URI WildOS.Settings
    QML_FILES ${QML_SOURCES}
    SOURCES
        src/kcmhost.cpp src/kcmhost.h
        src/system.cpp src/system.h
        src/updates.cpp src/updates.h
        src/appsmodel.cpp src/appsmodel.h
        src/windowsapps.cpp src/windowsapps.h
        src/windowsmode.cpp src/windowsmode.h
        src/inputdevices.cpp src/inputdevices.h
        src/defaultapps.cpp src/defaultapps.h
        src/clock.cpp src/clock.h
        src/startupapps.cpp src/startupapps.h
        src/keyboard.cpp src/keyboard.h
        src/printers.cpp src/printers.h
        src/users.cpp src/users.h
        src/language.cpp src/language.h
        src/appaccess.cpp src/appaccess.h
)

target_include_directories(wildos-settings PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(wildos-settings PRIVATE
    Qt6::Widgets Qt6::DBus Qt6::Qml Qt6::Quick Qt6::QuickControls2
    KF6::KCMUtils KF6::KCMUtilsQuick KF6::ConfigCore KF6::CoreAddons KF6::Service KF6::I18n KF6::ColorScheme
    PK::packagekitqt6
    Cups::Cups
    PkgConfig::GLIB
    util
)

install(TARGETS wildos-settings RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/org.wildos.settings.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
# The helpers are looked up by these exact paths (polkit pins the update
# helper to it), so they do not follow the distribution's libexec layout.
install(PROGRAMS data/settings-update-policy data/settings-users data/settings-locale data/settings-protection DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec/wildos)
install(FILES data/org.wildos.settings.policy DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)

# The automatic theme runs in every session and must not pull in the
# Settings app's QML and KCM dependencies.
qt_add_executable(wildos-auto-theme src/autotheme/main.cpp src/autotheme/sun.h)
target_link_libraries(wildos-auto-theme PRIVATE Qt6::Core Qt6::Gui Qt6::DBus KF6::ConfigCore)
install(TARGETS wildos-auto-theme RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/org.wildos.auto-theme.desktop DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart)
# Loads the firewall for a Strict or Maximum protection level at start-up.
install(FILES data/wildos-protection.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
