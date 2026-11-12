# Hot corners run inside KWin, so like the window menu they are built
# against the exact KWin that loads them.
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_FIND_MODULE_DIR})
find_package(Qt6 6.8 REQUIRED COMPONENTS DBus Qml Quick Widgets)
find_package(KWin 6.3 REQUIRED)
find_package(KF6 6.10 REQUIRED COMPONENTS Config)

add_library(wildos_hotcorners MODULE main.cpp hotcorners.cpp hotcorners.h)
qt_add_resources(wildos_hotcorners hotcornersqml PREFIX /wildos/hotcorners FILES Toast.qml)
set_target_properties(wildos_hotcorners PROPERTIES AUTOMOC ON PREFIX "")
target_link_libraries(wildos_hotcorners PRIVATE KWin::kwin KF6::ConfigCore Qt6::Core Qt6::Gui Qt6::DBus Qt6::Qml Qt6::Quick)

install(TARGETS wildos_hotcorners LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kwin/plugins)
