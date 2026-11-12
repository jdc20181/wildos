# Built against the exact KWin it runs in, like the other effects here.
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_FIND_MODULE_DIR})
find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Quick Widgets)
find_package(KWin 6.3 REQUIRED)
find_package(KF6 6.10 REQUIRED COMPONENTS Config GlobalAccel)
find_package(PkgConfig REQUIRED)
# Three-finger drag arrived in libinput 1.27.
pkg_check_modules(Libinput REQUIRED IMPORTED_TARGET libinput>=1.27)

add_library(wildos_gestures MODULE gestures.cpp gestures.h)
set_target_properties(wildos_gestures PROPERTIES AUTOMOC ON PREFIX "")
# The factory macro declares the Q_OBJECT plugin class, so moc has to see it.
set_property(TARGET wildos_gestures APPEND PROPERTY AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY)
target_link_libraries(wildos_gestures PRIVATE KWin::kwin KF6::ConfigCore KF6::GlobalAccel PkgConfig::Libinput Qt6::Core Qt6::Gui Qt6::DBus)

install(TARGETS wildos_gestures LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kwin/effects/plugins)
