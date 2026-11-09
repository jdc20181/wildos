# Slow keys for KWin 6.3, which has sticky and bounce keys but not this one
# (KWin added its own after 6.5). Built against the exact KWin it runs in,
# like the window menu.
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_FIND_MODULE_DIR})
find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Quick Widgets)
find_package(KWin 6.3 REQUIRED)
find_package(KF6 6.10 REQUIRED COMPONENTS Config)

add_library(wildos_slowkeys MODULE main.cpp slowkeys.cpp slowkeys.h)
set_target_properties(wildos_slowkeys PROPERTIES AUTOMOC ON PREFIX "")
target_link_libraries(wildos_slowkeys PRIVATE KWin::kwin KF6::ConfigCore Qt6::Core Qt6::Gui)

install(TARGETS wildos_slowkeys LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kwin/plugins)
