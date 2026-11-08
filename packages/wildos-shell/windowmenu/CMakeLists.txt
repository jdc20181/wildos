# The window menu runs inside KWin, so like the corners effect it is built
# against the exact KWin it is loaded by.
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_FIND_MODULE_DIR})
find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Qml Quick Widgets)
find_package(KWin 6.3 REQUIRED)

add_library(wildos_windowmenu MODULE main.cpp windowmenu.cpp windowmenu.h)
qt_add_resources(wildos_windowmenu windowmenuqml PREFIX /wildos/windowmenu FILES WindowMenu.qml)
set_target_properties(wildos_windowmenu PROPERTIES AUTOMOC ON PREFIX "")
target_link_libraries(wildos_windowmenu PRIVATE KWin::kwin Qt6::Core Qt6::Gui Qt6::DBus Qt6::Qml Qt6::Quick)

install(TARGETS wildos_windowmenu LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kwin/plugins)
