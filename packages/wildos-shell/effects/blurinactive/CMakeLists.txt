# Built the same way as roundcorners, which explains the finds.
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_FIND_MODULE_DIR})
find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Quick Widgets)
find_package(KWin 6.3 REQUIRED)
find_package(KF6 6.10 REQUIRED COMPONENTS Config)

add_library(wildos_blurinactive MODULE blurinactive.cpp blurinactive.h ../focus/focus.h blurinactive.qrc)
set_target_properties(wildos_blurinactive PROPERTIES AUTOMOC ON AUTORCC ON PREFIX "")
set_property(TARGET wildos_blurinactive APPEND PROPERTY AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY_SUPPORTED)
target_link_libraries(wildos_blurinactive PRIVATE KWin::kwin KF6::ConfigCore Qt6::Core Qt6::Gui)

install(TARGETS wildos_blurinactive LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kwin/effects/plugins)
