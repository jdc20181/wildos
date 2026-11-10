# Built the same way as roundcorners, which explains the finds.
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_FIND_MODULE_DIR})
find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Quick Widgets)
find_package(KWin 6.3 REQUIRED)
find_package(KF6 6.10 REQUIRED COMPONENTS Config)

add_library(wildos_outline MODULE outline.cpp outline.h ../focus/focus.h outline.qrc)
set_target_properties(wildos_outline PROPERTIES AUTOMOC ON AUTORCC ON PREFIX "")
set_property(TARGET wildos_outline APPEND PROPERTY AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY_SUPPORTED)
target_link_libraries(wildos_outline PRIVATE KWin::kwin KF6::ConfigCore Qt6::Core Qt6::Gui)

install(TARGETS wildos_outline LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kwin/effects/plugins)
