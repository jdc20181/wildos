# KWin's exported target pulls in Wayland and epoxy through find modules that
# ship with extra-cmake-modules.
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_FIND_MODULE_DIR})
# KWin::kwin links these but its config file does not look for them.
find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Quick Widgets)
find_package(KWin 6.3 REQUIRED)
find_package(KF6 6.10 REQUIRED COMPONENTS Config)

# KWin loads binary effects with QPluginLoader and refuses one whose IID names
# another KWin version, so this must be built against the KWin it runs under.
# debian/control pins kwin-data to 6.3 for that reason.
add_library(wildos_roundcorners MODULE roundcorners.cpp roundcorners.h roundcorners.qrc)
set_target_properties(wildos_roundcorners PROPERTIES AUTOMOC ON AUTORCC ON PREFIX "")
# The factory macro declares the Q_OBJECT plugin class, so moc has to see it.
set_property(TARGET wildos_roundcorners APPEND PROPERTY AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY_SUPPORTED)
target_link_libraries(wildos_roundcorners PRIVATE KWin::kwin KF6::ConfigCore Qt6::Core Qt6::Gui)

install(TARGETS wildos_roundcorners LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kwin/effects/plugins)
