cmake_minimum_required(VERSION 3.25)
project(wildos-lighting VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui DBus Network)
find_package(KF6Config REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)

# QtGui for QColor only; the program opens no windows.
qt_add_executable(wildos-lighting src/main.cpp src/lighting.cpp src/lighting.h src/plan.cpp src/plan.h)
target_link_libraries(wildos-lighting PRIVATE Qt6::Core Qt6::Gui Qt6::DBus Qt6::Network KF6::ConfigCore)

install(TARGETS wildos-lighting RUNTIME DESTINATION libexec/wildos)
install(PROGRAMS libexec/rgb-devices DESTINATION libexec/wildos)
install(FILES data/wildos-lighting.service data/wildos-openrgb.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user)
install(FILES data/60-openrgb.rules DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/udev/rules.d)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_executable(plantest tests/plantest.cpp src/plan.cpp)
    target_include_directories(plantest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(plantest PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME plan COMMAND plantest)
    # rgb-devices against a fake sysfs and rules file.
    add_test(NAME scripts COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts.sh ${CMAKE_CURRENT_SOURCE_DIR}/libexec)
endif()
